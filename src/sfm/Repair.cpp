#include "sfm/Repair.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <thread>

#include "sfm/Pipeline.h"
#include "sfm/core/CameraSetup.h"
#include "sfm/core/FeatureCompaction.h"
#include "sfm/core/Log.h"
#include "sfm/core/Matches.h"
#include "sfm/feature/GpsPairs.h"
#include "sfm/feature/LearnedMatcher.h"
#include "sfm/feature/PairSelection.h"
#include "sfm/feature/Pairing.h"
#include "sfm/feature/Verification.h"
#include "sfm/map/Mapper.h"
#include "sfm/map/Merge.h"
#include "sfm/map/Orient.h"
#include "i18n/catalog/Sfm.h"

namespace fs = std::filesystem;

namespace sfm {

namespace L = sfm::slog;
namespace M = spirula::i18n::msg::sfm;
using sfm::slog::Tag;

namespace {

// Registered cameras nearest a hinted centre: what a hand-placed camera can
// see, whatever the image content scored.
constexpr size_t kHintNeighbours = 20;
// Below this a camera agrees with the frame fit, whatever the median is.
constexpr double kFrameGateDeg = 0.5;

std::string stem(const std::string& name) {
    const size_t slash = name.find_last_of('/');
    const size_t dot = name.find_last_of('.');
    return dot != std::string::npos && (slash == std::string::npos || dot > slash)
               ? name.substr(0, dot)
               : name;
}

bool loadFeatures(const std::string& featdir, const MatchesDatabase& db, bool descriptors,
                  int threads, std::vector<FeatureSet>& feats) {
    feats.assign(db.images.size(), FeatureSet());
    const unsigned hc = std::thread::hardware_concurrency();
    int nt = threads > 0 ? threads : (hc > 0 ? (int)hc : 1);
    nt = std::max(1, std::min<int>(nt, (int)db.images.size()));
    std::atomic<size_t> next{0};
    std::mutex mtx;
    std::string first_error;
    std::vector<std::thread> pool;
    for (int t = 0; t < nt; t++)
        pool.emplace_back([&] {
            for (size_t i = next++; i < db.images.size(); i = next++) {
                try {
                    feats[i] = readFeatures(featdir + "/" + db.images[i].name + ".bin", descriptors);
                } catch (const std::exception& e) {
                    std::lock_guard<std::mutex> lk(mtx);
                    if (first_error.empty()) first_error = e.what();
                }
            }
        });
    for (std::thread& th : pool) th.join();
    if (!first_error.empty()) {
        L::err_raw(Tag::Map, first_error);
        return false;
    }
    return true;
}

// Re-index a model's observations onto `feats`: `row_raw` takes an image's
// model row to its raw feature row (absent = the same row), `raw_new` a raw row
// to its index in `feats` (null = the same row).
void reindexModel(Reconstruction& m, const std::map<uint32_t, std::vector<uint32_t>>& row_raw,
                  const std::vector<std::vector<StoredFeatureIndex>>* raw_new,
                  const std::vector<FeatureSet>& feats) {
    std::map<uint32_t, std::vector<uint32_t>> remap;  // per image: model row -> feats row
    for (auto& kv : m.images) {
        const uint32_t id = kv.first;
        if (id >= feats.size()) continue;
        Image& im = kv.second;
        const auto rr = row_raw.find(id);
        std::vector<uint32_t>& r = remap[id];
        r.assign(im.points2D.size(), kUnusedFeature);
        for (size_t k = 0; k < r.size(); k++) {
            const uint32_t raw = rr != row_raw.end() ? rr->second[k] : (uint32_t)k;
            r[k] = !raw_new ? raw : raw < (*raw_new)[id].size() ? (*raw_new)[id][raw] : kUnusedFeature;
        }
        std::vector<uint64_t> ids(feats[id].count(), kInvalidPoint3D);
        for (size_t a = 0; a < im.point3D_ids.size() && a < r.size(); a++)
            if (r[a] != kUnusedFeature && r[a] < ids.size()) ids[r[a]] = im.point3D_ids[a];
        im.point3D_ids = std::move(ids);
        im.points2D.resize(feats[id].count());
        for (uint32_t f = 0; f < feats[id].count(); f++)
            im.points2D[f] = {feats[id].keypoints[f].x, feats[id].keypoints[f].y};
    }
    std::vector<uint64_t> dead;
    for (auto& kv : m.points3D) {
        std::vector<TrackElement> t;
        for (TrackElement e : kv.second.track) {
            auto r = remap.find(e.image_id);
            if (r == remap.end() || e.point2D_idx >= r->second.size()) continue;
            const uint32_t b = r->second[e.point2D_idx];
            if (b == kUnusedFeature) continue;
            e.point2D_idx = b;
            t.push_back(e);
        }
        if (t.size() < 2) dead.push_back(kv.first);
        kv.second.track = std::move(t);
    }
    for (uint64_t pid : dead) m.points3D.erase(pid);
    for (auto& kv : m.images)
        for (uint64_t& p : kv.second.point3D_ids)
            if (p != kInvalidPoint3D && !m.points3D.count(p)) p = kInvalidPoint3D;
}

// Candidate pairs for the targets against the registered images: content
// score, file order, GPS and, for a hint, the cameras round where it was put.
std::vector<std::pair<uint32_t, uint32_t>> targetPairs(
    const SfmConfig& cfg, const MatchesDatabase& db, const std::vector<FeatureSet>& feats,
    const std::vector<uint32_t>& targets, const std::vector<uint32_t>& registered,
    const std::map<uint32_t, Vec3>& hint_centres, const std::map<uint32_t, Vec3>& centres,
    const PriorSource* placed) {
    const uint32_t n = (uint32_t)db.images.size();
    std::vector<char> is_target(n, 0);
    for (uint32_t t : targets) is_target[t] = 1;
    auto touches = [&](const std::pair<uint32_t, uint32_t>& p) {
        return is_target[p.first] || is_target[p.second];
    };
    std::vector<std::pair<uint32_t, uint32_t>> pairs =
        prefilterPairsFor(feats, cfg.prefilter, targets, registered);
    std::vector<std::string> names(n);
    for (uint32_t i = 0; i < n; i++) names[i] = db.images[i].name;
    for (const auto& p : sequentialPairs(n, cfg.overlap, cfg.quadratic_overlap, folderRuns(names)))
        if (touches(p)) pairs.push_back(p);
    if (placed)
        for (const auto& p : gpsProximityPairs(*placed, n, cfg.sensor_pair_radius, 20))
            if (touches(p)) pairs.push_back(p);
    for (const auto& h : hint_centres) {
        std::vector<std::pair<double, uint32_t>> d;
        for (const auto& c : centres)
            if (c.first != h.first) d.push_back({(c.second - h.second).norm(), c.first});
        const size_t take = std::min(kHintNeighbours, d.size());
        std::partial_sort(d.begin(), d.begin() + take, d.end());
        for (size_t k = 0; k < take; k++)
            pairs.emplace_back(std::min(h.first, d[k].second), std::max(h.first, d[k].second));
    }
    std::set<std::pair<uint32_t, uint32_t>> have;
    for (const TwoViewMatches& p : db.pairs)
        have.insert({std::min(p.image1, p.image2), std::max(p.image1, p.image2)});
    std::sort(pairs.begin(), pairs.end());
    pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());
    pairs.erase(std::remove_if(pairs.begin(), pairs.end(),
                               [&](const std::pair<uint32_t, uint32_t>& p) {
                                   return p.first == p.second || have.count(p);
                               }),
                pairs.end());
    return pairs;
}

std::vector<TwoViewMatches> verifyTargetPairs(const SfmConfig& cfg, const CameraSetup& cs,
                                              const std::vector<FeatureSet>& feats,
                                              const std::vector<std::pair<uint32_t, uint32_t>>& pairs) {
    std::unique_ptr<IFeatureMatcher> matcher =
        createFeatureMatcher(cfg.matcher, cfg.match, cfg.lightglue, cfg.loma_match);
    VerificationOptions vopt;
    vopt.two_view = cfg.twoview;
    vopt.num_threads = cfg.threads;
    vopt.match_batch_pairs = cfg.match.batch_pairs;
    // Wide lenses verify on the sphere, as matchFeatureDir does; an equirect has no plane.
    BearingCache bc;
    if (cs.anyWide()) {
        bc = precomputeBearings(feats, perImageCameras(cs, feats.size()), !cs.mixed(), cfg.threads);
        vopt.bearings = &bc;
    }
    auto matchFn = [&](size_t b, size_t e, std::vector<std::vector<FeatureMatch>>& out) {
        matcher->matchBatch(feats, pairs, b, e, out);
    };
    return verifyPairs(feats, pairs, matchFn, vopt);
}

bool writeMatchesSafely(const fs::path& path, const MatchesDatabase& db) {
    std::error_code ec;
    const fs::path orig = path.string() + ".orig";
    if (!fs::exists(orig, ec)) fs::copy_file(path, orig, ec);
    if (ec) return false;
    const fs::path tmp = path.string() + ".tmp";
    writeMatches(tmp.string(), db);
    fs::rename(tmp, path, ec);
    return !ec;
}

const char* outcomeName(Mapper::RepairOutcome o) {
    switch (o) {
        case Mapper::RepairOutcome::Moved: return "moved";
        case Mapper::RepairOutcome::Kept: return "kept";
        case Mapper::RepairOutcome::Added: return "added";
        case Mapper::RepairOutcome::Failed: return "failed";
        case Mapper::RepairOutcome::Dropped: return "removed";
    }
    return "failed";
}

}  // namespace

int runRepair(RepairJob job, std::vector<RepairLine>* report) {
    SfmConfig& cfg = job.cfg;
    if (std::string err = cfg.finalize(CMD_MAP); !err.empty()) {
        L::err_raw(Tag::Map, err);
        return 1;
    }
    if (std::string err = cfg.resolveDevice(); !err.empty()) {
        L::err_raw(Tag::Map, err);
        return 1;
    }
    MapperOptions& opt = cfg.mapper;
    const bool verbose = opt.verbose;
    const fs::path ws(job.workspace);
    const fs::path matches_path = ws / "matches.bin";
    const std::string featdir = (ws / "features").string();
    std::error_code ec;
    if (!fs::exists(matches_path, ec) || !fs::is_directory(featdir, ec)) {
        L::fail(Tag::Map, M::repair_needs_workspace, {ws.string()});
        return 1;
    }

    MatchesDatabase db = readMatches(matches_path.string());
    const uint32_t n = (uint32_t)db.images.size();
    Reconstruction model;
    try {
        model = Reconstruction::readBinary(job.model_dir);
    } catch (const std::exception& e) {
        L::fail(Tag::Map, M::map_cannot_read, {job.model_dir, e.what()});
        return 1;
    }
    RigTable rigs = readRigs(job.model_dir, model);

    std::vector<FeatureSet> feats;
    if (!loadFeatures(featdir, db, job.match, cfg.threads, feats)) return 1;

    // A model indexes every feature row, or only those some match referenced
    // (--compact-unused-features) when it was written -- not necessarily the
    // matches.bin of now. Compaction keeps order, so rows are found by position.
    std::map<uint32_t, std::vector<uint32_t>> row_raw;
    for (const auto& kv : model.images) {
        const Image& im = kv.second;
        if (!im.registered) continue;
        const size_t raw = kv.first < n ? feats[kv.first].count() : 0;
        if (raw && im.points2D.size() == raw) continue;
        const std::vector<Keypoint>& kp = feats[kv.first].keypoints;
        auto same = [&](size_t j, size_t k) {
            return std::fabs(kp[j].x - im.points2D[k].x) <= 1e-3 &&
                   std::fabs(kp[j].y - im.points2D[k].y) <= 1e-3;
        };
        std::vector<uint32_t> rows(im.points2D.size());
        bool found = raw > 0;
        for (size_t k = 0, j = 0; found && k < rows.size(); k++, j++) {
            while (j < raw && !same(j, k)) j++;
            found = j < raw;
            if (found) rows[k] = (uint32_t)j;
        }
        if (!found) {
            L::fail(Tag::Map, M::repair_keypoints_differ,
                    {im.name, (long long)im.points2D.size(), (long long)raw});
            return 1;
        }
        row_raw[kv.first] = std::move(rows);
    }
    const bool compact = !row_raw.empty();

    std::map<std::string, uint32_t> by_name;
    for (uint32_t i = 0; i < n; i++) by_name[stem(db.images[i].name)] = i;
    for (const auto& kv : model.images) by_name[stem(kv.second.name)] = kv.first;
    // A front end may hold the absolute path; the longest name that ends it wins.
    auto resolve = [&](const std::string& name, uint32_t& id) {
        std::string key = stem(name);
        std::replace(key.begin(), key.end(), '\\', '/');
        auto it = by_name.find(key);
        if (it == by_name.end()) {
            size_t best = 0;
            for (auto c = by_name.begin(); c != by_name.end(); ++c) {
                const std::string& k = c->first;
                if (k.size() > best && k.size() < key.size() &&
                    key.compare(key.size() - k.size(), k.size(), k) == 0 &&
                    key[key.size() - k.size() - 1] == '/') {
                    best = k.size();
                    it = c;
                }
            }
        }
        if (it == by_name.end()) {
            L::err(Tag::Map, M::repair_unknown_image, {name});
            return false;
        }
        id = it->second;
        return true;
    };
    std::set<uint32_t> in_model;
    for (const auto& kv : model.images)
        if (kv.second.registered) in_model.insert(kv.first);
    Mapper::RepairRequest rq;
    rq.audit_all = job.audit_all;
    for (const std::string& s : job.replace)
        if (uint32_t id; resolve(s, id)) rq.replace.push_back(id);
    for (const std::string& s : job.add)
        if (uint32_t id; resolve(s, id)) rq.add.push_back(id);
    if (job.add_missing) {
        std::set<uint32_t> skip;
        for (const std::string& s : job.exclude)
            if (uint32_t id; resolve(s, id)) skip.insert(id);
        for (uint32_t i = 0; i < n; i++)
            if (!in_model.count(i) && !skip.count(i)) rq.add.push_back(i);
    }
    for (const RepairHint& h : job.hints)
        if (uint32_t id; resolve(h.name, id)) rq.hints.emplace_back(id, h.pose);

    CameraSetup cs;
    if (!loadCameraSetup(db, cs)) cs = buildCameras(db.images, feats, cfg.camera);
    const SensorCaptures sensors = loadSensorCaptures(cfg, verbose);
    applyMetricGpsAuto(cfg, sensors, cfg.image_dir);
    std::unique_ptr<TelemetryPriors> priors =
        cfg.sensor_map ? makeSensorPriors(cfg, sensors, db, cs.ids) : nullptr;
    std::unique_ptr<ExifGpsPriors> exif_priors =
        cfg.sensor_map && !priors ? makeExifGpsPriors(cfg, cfg.image_dir, db, cs, verbose)
                                  : nullptr;
    PriorSource* placed = priors ? static_cast<PriorSource*>(priors.get()) : exif_priors.get();

    std::vector<uint32_t> targets;
    for (uint32_t i : rq.replace) targets.push_back(i);
    for (uint32_t i : rq.add) targets.push_back(i);
    for (const auto& h : rq.hints) targets.push_back(h.first);
    std::sort(targets.begin(), targets.end());
    targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
    if (job.match && !targets.empty()) {
        std::vector<uint32_t> registered(in_model.begin(), in_model.end());
        std::map<uint32_t, Vec3> centres, hint_centres;
        for (uint32_t i : in_model) centres[i] = cameraCenter(model.images.at(i).pose);
        for (const auto& h : rq.hints) hint_centres[h.first] = cameraCenter(h.second);
        const std::vector<std::pair<uint32_t, uint32_t>> pairs =
            targetPairs(cfg, db, feats, targets, registered, hint_centres, centres, placed);
        std::vector<TwoViewMatches> fresh;
        if (!pairs.empty()) fresh = verifyTargetPairs(cfg, cs, feats, pairs);
        L::out(Tag::Map, M::repair_matched,
               {(long long)targets.size(), (long long)pairs.size(), (long long)fresh.size()});
        if (!fresh.empty()) {
            for (TwoViewMatches& t : fresh) db.pairs.push_back(std::move(t));
            if (!writeMatchesSafely(matches_path, db))
                L::err_raw(Tag::Map, "cannot update " + matches_path.string());
        }
        for (FeatureSet& f : feats) {
            f.descriptors.clear();
            f.descriptors.shrink_to_fit();
        }
    }

    if (compact) {
        FeatureCompactionPlan plan = buildFeatureCompactionPlan(db);
        for (size_t i = 0; i < feats.size(); i++)
            feats[i] = compactFeatureSet(std::move(feats[i]), plan.old_to_new[i],
                                         plan.compact_counts[i]);
        remapMatches(db, plan, feats);
        reindexModel(model, row_raw, &plan.old_to_new, feats);
    } else {
        reindexModel(model, row_raw, nullptr, feats);
    }

    opt.initial_cameras = cs.cameras;
    opt.known_focal_cameras = cs.focal_known;
    opt.given_focal_cameras = cs.focal_given;
    opt.measured_focal_cameras = cs.focal_measured;
    if (rigs.empty()) rigs = buildRigs(db, cfg, verbose);
    const SequenceTable seqs = buildSequences(db, cfg, verbose);
    if (priors)
        calibrateSensorPriorsFromDatabase(*priors, db, feats, perImageCameras(cs, feats.size()),
                                          cfg.twoview, cfg.threads, verbose);
    Mapper mapper(db, feats, opt, cs.ids, &rigs, &seqs, placed);
    Mapper::RepairStats st;
    Reconstruction out = mapper.repair(model, rq, &st);

    // Back into the input's frame through the cameras nobody asked to change.
    std::set<uint32_t> touched(targets.begin(), targets.end());
    for (const Mapper::RepairItem& it : st.items) touched.insert(it.image);
    std::vector<Pose> src, dst;
    for (const auto& kv : out.images)
        if (kv.second.registered && in_model.count(kv.first) && !touched.count(kv.first)) {
            src.push_back(kv.second.pose);
            dst.push_back(model.images.at(kv.first).pose);
        }
    // A wrong camera nobody named still moves in the final BA, and two of them
    // turned 90 deg tilted the whole model 3.4 deg: refit on the ones that agree.
    if (Sim3 T; estimateSim3FromPoses(src, dst, T)) {
        for (int pass = 0; pass < 2; pass++) {
            std::vector<double> err(src.size());
            for (size_t k = 0; k < src.size(); k++)
                err[k] = rotationAngleDeg(mul(transformPose(T, src[k]).R, transpose(dst[k].R)));
            std::vector<double> sorted = err;
            std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
            const double gate = std::max(kFrameGateDeg, 3.0 * sorted[sorted.size() / 2]);
            std::vector<Pose> s2, d2;
            for (size_t k = 0; k < src.size(); k++)
                if (err[k] <= gate) {
                    s2.push_back(src[k]);
                    d2.push_back(dst[k]);
                }
            if (s2.size() == src.size() || !estimateSim3FromPoses(s2, d2, T)) break;
            src.swap(s2);
            dst.swap(d2);
        }
        applySim3(out, T);
    }

    for (auto& kv : out.images) {
        auto it = model.images.find(kv.first);
        if (it != model.images.end()) kv.second.name = it->second.name;
    }
    std::vector<Reconstruction> models{std::move(out)};
    if (!cfg.image_dir.empty()) {
        resolveImageNames(models, cfg.image_dir);
        recolorPoints(models, cfg);
    }
    splitCamerasBySize(models, feats);
    const Reconstruction& res = models.front();

    fs::create_directories(job.output_dir, ec);
    const fs::path od(job.output_dir);
    if (fs::path(job.model_dir) != od && fs::exists(fs::path(job.model_dir) / "gauge.txt", ec))
        fs::copy_file(fs::path(job.model_dir) / "gauge.txt", od / "gauge.txt",
                      fs::copy_options::overwrite_existing, ec);
    res.writeBinary(job.output_dir);
    writeRigs(od, res, &rigs);

    std::vector<RepairLine> lines;
    int counts[5] = {0, 0, 0, 0, 0};
    for (const Mapper::RepairItem& it : st.items) {
        RepairLine l;
        auto im = res.images.find(it.image);
        l.name = im != res.images.end() ? im->second.name : db.images[it.image].name;
        l.outcome = outcomeName(it.outcome);
        l.rot_deg = it.rot_deg;
        l.shift = it.shift;
        lines.push_back(l);
        counts[(int)it.outcome]++;
    }
    {
        std::ofstream f(od / "repair.txt");
        f << "# outcome rotation_deg shift_over_spread name\n";
        for (const RepairLine& l : lines)
            f << l.outcome << ' ' << l.rot_deg << ' ' << l.shift << ' ' << l.name << '\n';
    }
    L::out(Tag::Map, M::repair_summary,
           {(long long)counts[(int)Mapper::RepairOutcome::Moved],
            (long long)counts[(int)Mapper::RepairOutcome::Added],
            (long long)counts[(int)Mapper::RepairOutcome::Kept],
            (long long)counts[(int)Mapper::RepairOutcome::Failed],
            (long long)counts[(int)Mapper::RepairOutcome::Dropped]});
    if (report) *report = std::move(lines);
    return 0;
}

bool readRepairHints(const std::string& path, std::vector<RepairHint>& out, std::string& err) {
    std::ifstream f(path);
    if (!f) {
        err = "cannot open " + path;
        return false;
    }
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        RepairHint h;
        Quat q;
        Vec3 t;
        if (!(ss >> q[0] >> q[1] >> q[2] >> q[3] >> t.x >> t.y >> t.z >> std::ws) ||
            !std::getline(ss, h.name) || h.name.empty()) {
            err = path + ": cannot read '" + line + "'";
            return false;
        }
        h.pose = {quaternionToRotation(q), t};
        out.push_back(std::move(h));
    }
    return true;
}

bool writeRepairHints(const std::string& path, const std::vector<RepairHint>& hints) {
    std::ofstream f(path);
    if (!f) return false;
    f.precision(17);
    for (const RepairHint& h : hints) {
        const Quat q = rotationToQuaternion(h.pose.R);
        f << q[0] << ' ' << q[1] << ' ' << q[2] << ' ' << q[3] << ' ' << h.pose.t.x << ' '
          << h.pose.t.y << ' ' << h.pose.t.z << ' ' << h.name << '\n';
    }
    return (bool)f;
}

bool readRepairReport(const std::string& path, std::vector<RepairLine>& out) {
    std::ifstream f(path);
    if (!f) return false;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        RepairLine l;
        if ((ss >> l.outcome >> l.rot_deg >> l.shift >> std::ws) && std::getline(ss, l.name))
            out.push_back(std::move(l));
    }
    return true;
}

}  // namespace sfm

#include "sfm/Pipeline.h"
#include "sfm/core/Resume.h"
#include "sfm/core/Progress.h"
#include "sfm/map/RegionalStore.h"
#include "sfm/map/RegionalPlan.h"
#include "sfm/map/RegionalTransform.h"
#include "sfm/map/RegionalWindows.h"
#include "sfm/map/RegionalQuality.h"
#include "sfm/map/Orient.h"
#include "i18n/TimeFormat.h"
#include <array>
#include <iomanip>
#include <sstream>
#include <thread>

namespace sfm {
namespace {
namespace fs = std::filesystem;
using regional::Region;
using regional::RegionInput;
using sfm::slog::Tag;
namespace M = spirula::i18n::msg::sfm;

struct SavedRegion { fs::path path; size_t memory = 0; };
struct MapIdsScope {
    explicit MapIdsScope(const std::vector<uint32_t>& ids) { events::set_map_image_ids(ids); }
    ~MapIdsScope() { events::set_map_image_ids({}); }
};

uint64_t signatureHash(const std::string& s) {
    uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : s) { hash ^= c; hash *= 1099511628211ull; }
    return hash;
}

void savePlan(const fs::path& root, const std::vector<Region>& plan) {
    model_store_detail::Writer out(root / "plan.part");
    out.scalar<uint32_t>(1); out.scalar<uint64_t>(plan.size());
    for (const Region& r : plan) { out.vector(r.core); out.vector(r.images); }
    out.finish(); regional::replaceFile(root / "plan.part", root / "plan.bin");
}

std::vector<Region> readPlan(const fs::path& root, size_t images) {
    model_store_detail::Reader in(root / "plan.bin");
    if (in.scalar<uint32_t>() != 1) throw std::runtime_error("unsupported regional plan");
    std::vector<Region> plan(in.count(16)); std::vector<uint8_t> owner(images);
    for (Region& r : plan) {
        r.core = in.vector<uint32_t>(); r.images = in.vector<uint32_t>();
        if (r.core.empty() || !std::is_sorted(r.images.begin(), r.images.end()) ||
            std::adjacent_find(r.images.begin(), r.images.end()) != r.images.end())
            throw std::runtime_error("invalid regional checkpoint plan");
        for (uint32_t i : r.images) if (i >= images) throw std::runtime_error("regional checkpoint image out of range");
        for (uint32_t i : r.core)
            if (i >= images || owner[i]++ || !std::binary_search(r.images.begin(), r.images.end(), i))
                throw std::runtime_error("invalid regional checkpoint ownership");
    }
    if (std::count(owner.begin(), owner.end(), 1) != images) throw std::runtime_error("incomplete regional checkpoint plan");
    in.finish(); return plan;
}

fs::path stageFile(const fs::path& root, size_t pass, size_t region) {
    return root / ("pass-" + std::to_string(pass) + "-region-" + std::to_string(region));
}

std::vector<SavedRegion> savedStage(const fs::path& file) {
    std::vector<SavedRegion> result;
    if (!fs::exists(file.string() + ".done")) return result;
    model_store_detail::Reader in(file.string() + ".done");
    size_t count = in.count(16);
    for (size_t m = 0; m < count; ++m) {
        SavedRegion item; item.path = file.string() + "-" + std::to_string(m) + ".bin";
        uint64_t bytes = in.scalar<uint64_t>(); item.memory = in.scalar<uint64_t>();
        if (!fs::exists(item.path) || fs::file_size(item.path) != bytes) throw std::runtime_error("regional checkpoint file changed");
        result.push_back(item);
    }
    in.finish(); return result;
}

std::vector<SavedRegion> commitStage(const fs::path& file, const std::vector<Reconstruction>& models) {
    std::vector<SavedRegion> result;
    for (size_t m = 0; m < models.size(); ++m) {
        SavedRegion item{file.string() + "-" + std::to_string(m) + ".bin", modelResidentBytes(models[m])};
        regional::atomicModel(item.path, models[m]); result.push_back(item);
    }
    model_store_detail::Writer out(file.string() + ".part"); out.scalar<uint64_t>(result.size());
    for (const auto& r : result) { out.scalar<uint64_t>(fs::file_size(r.path)); out.scalar<uint64_t>(r.memory); }
    out.finish(); fs::rename(file.string() + ".part", file.string() + ".done");
    return result;
}

Reconstruction readSaved(const SavedRegion& item, size_t budget) {
    if (item.memory > budget / 2) throw std::length_error("regional checkpoint exceeds current memory budget");
    return model_store_detail::readModel(item.path);
}

void addSummary(Reconstruction& summary, const Reconstruction& rec, const Region& region,
                std::vector<uint32_t>& source, size_t preview_points) {
    for (const auto& kv : rec.images) {
        if (!kv.second.registered) continue;
        bool owned = std::binary_search(region.core.begin(), region.core.end(), kv.first);
        auto found = summary.images.find(kv.first);
        uint32_t weight = kv.second.numPoint3D();
        uint32_t score = owned ? weight + (1u << 30) : weight;
        if (found != summary.images.end() && source.at(kv.first) >= score) continue;
        Image im; im.id = kv.first; im.camera_id = kv.second.camera_id; im.name = kv.second.name;
        im.pose = kv.second.pose; im.registered = true; im.exif_orientation = kv.second.exif_orientation;
        summary.images[kv.first] = std::move(im); source.at(kv.first) = score;
        events::map_placed(kv.first);
    }
    size_t step = std::max<size_t>(1, rec.points3D.size() / std::max<size_t>(1, preview_points)), seen = 0;
    for (const auto& kv : rec.points3D) {
        if (summary.points3D.size() >= progress::kMaxPoints) break;
        if (seen++ % step) continue;
        Point3D p; p.xyz = kv.second.xyz; std::copy(kv.second.rgb, kv.second.rgb + 3, p.rgb);
        summary.points3D.emplace(summary.next_point3D_id++, std::move(p));
    }
}

bool alignRegional(Reconstruction& rec, const Reconstruction& summary, PriorSource& gps,
                   double max_error, bool require_overlap = false) {
    struct View { Pose a, b; Camera cam; std::vector<Vec3> points; std::vector<Vec2> xy; };
    std::vector<View> views;
    for (const auto& kv : rec.images) {
        auto found = summary.images.find(kv.first);
        if (!kv.second.registered || found == summary.images.end()) continue;
        const Image& im = kv.second;
        View v; v.a = im.pose; v.b = found->second.pose; v.cam = rec.cameras.at(im.camera_id);
        size_t step = std::max<size_t>(1, im.numPoint3D() / 32), seen = 0;
        for (size_t f = 0; f < im.point3D_ids.size(); ++f) {
            auto point = rec.points3D.find(im.point3D_ids[f]); if (point == rec.points3D.end()) continue;
            if (seen++ % step) continue; v.points.push_back(point->second.xyz); v.xy.push_back(im.points2D[f]);
        }
        if (!v.points.empty()) views.push_back(std::move(v));
    }
    if (views.size() >= 3) {
        auto fit = [&](const std::vector<int>& indices) {
            std::vector<Pose> a, b; for (int i : indices) { a.push_back(views[i].a); b.push_back(views[i].b); }
            Sim3 t; std::vector<Sim3> out; if (estimateSim3FromPoses(a, b, t)) out.push_back(t); return out;
        };
        auto residual = [&](const Sim3& t, int i) {
            const View& v = views[i]; double sum = 0;
            for (size_t k = 0; k < v.points.size(); ++k)
                sum += std::min(max_error * 10, reprojErrorAt(v.cam, v.b, v.xy[k], transformPoint(t, v.points[k])) / v.cam.pixel_scale);
            double error = sum / v.points.size(); return error * error;
        };
        RansacOptions opt; opt.max_error = max_error; opt.max_num_trials = 500; opt.seed = 0;
        auto fit_result = loransac<Sim3>(int(views.size()), 2, fit, fit, residual, opt);
        if (fit_result.success && fit_result.num_inliers >= 3 && fit_result.num_inliers * 2 >= views.size()) {
            applySim3(rec, fit_result.model); return true;
        }
        if (require_overlap) return false;
    }
    MetricRef ref;
    for (const auto& kv : rec.images) {
        Vec3 target;
        if (kv.second.registered && gps.position(kv.first, target)) {
            ref.centres.push_back(cameraCenter(kv.second.pose)); ref.targets.push_back(target); ref.image_ids.push_back(kv.first);
        }
    }
    auto fit = fitMetricGauge(ref, 5., MetricAxes::Full, 0.01);
    if (!fit.ok) {
        slog::diag(Tag::Map, "[regional] alignment unresolved: %zu registered, %zu shared views, GPS gauge failed (%d)\n",
                   rec.numRegistered(), views.size(), int(fit.reason));
        return false;
    }
    applySim3(rec, fit.T); return true;
}


struct RegionalCosts { double initial = 0, final = 0; uint64_t observations = 0; };
struct CameraEstimate { Camera camera; std::vector<std::pair<std::array<double, 12>, uint64_t>> rows; };
using CameraEstimates = std::map<uint32_t, CameraEstimate>;
void addCameraEstimates(CameraEstimates& groups, const Reconstruction& rec) {
    std::map<uint32_t, uint64_t> counts;
    for (const auto& kv : rec.images) if (kv.second.registered) counts[kv.second.camera_id] += kv.second.numPoint3D();
    for (const auto& kv : rec.cameras) if (counts[kv.first]) {
        auto& group = groups[kv.first]; group.camera = kv.second; std::array<double, 12> params{};
        packIntrinsics(kv.second, params.data()); group.rows.emplace_back(params, counts[kv.first]);
    }
}
void applyCameraEstimates(Reconstruction& out, const CameraEstimates& groups) {
    for (const auto& kv : groups) {
        Camera camera = kv.second.camera; double values[12]{};
        for (int p = 0; p < camNumParams(camera.model); ++p) {
            std::vector<std::pair<double, uint64_t>> sorted; uint64_t total = 0;
            for (const auto& row : kv.second.rows) { sorted.emplace_back(row.first[p], row.second); total += row.second; }
            std::sort(sorted.begin(), sorted.end()); uint64_t weight = 0;
            for (const auto& row : sorted) { weight += row.second; values[p] = row.first; if (weight >= (total + 1) / 2) break; }
        }
        unpackIntrinsics(camera, values); out.cameras[kv.first] = camera;
    }
}

RegionalCosts boundedRegionalBA(Reconstruction& rec, const Reconstruction& consensus, const SfmConfig& cfg,
                               const std::set<uint64_t>& shared_fixed, size_t budget, VkContext& context) {
    RegionalCosts costs; CameraEstimates estimates;
    const double started = now(); double prepare_seconds = 0, solve_seconds = 0; size_t windows = 0;
    regional::WindowGraph graph(rec, budget / 32);
    const double graph_seconds = now() - started;
    auto remaining = graph.remaining();
    size_t window = std::min<size_t>(256, std::max<size_t>(16, budget / (16u << 20)));
    while (!remaining.empty()) {
        const double prepare_started = now();
        cancel::check(); auto group = graph.select(remaining, window, window / 4 + 8);
        std::set<uint64_t> fixed = shared_fixed;
        Reconstruction sub = regional::windowModel(rec, group.images, fixed, cfg.threads);
        for (auto& kv : sub.cameras) { auto common = consensus.cameras.find(kv.first); if (common != consensus.cameras.end()) kv.second = common->second; }
        BundleOptions opt; opt.real = realCfgFromName(cfg.mapper.ba_real); opt.max_iters = 15;
        opt.loss = cfg.mapper.ba_loss; opt.loss_param = float(cfg.mapper.ba_loss_param);
        opt.device_selector = cfg.mapper.device_selector; opt.device = cfg.mapper.device;
        opt.threads = cfg.threads; opt.solver = "cg"; opt.refine_intrinsics = true;
        opt.shared_ctx = &context;
        opt.refine_principal_point = cfg.mapper.refine_principal_point; opt.refine_extra_params = cfg.mapper.refine_extra_params;
        SolverStats stats; opt.stats = &stats; opt.fixed_points = &fixed;
        opt.fixed_images = &group.support;
        opt.host_budget_bytes = budget / 3; opt.over_budget_throws = true;
        prepare_seconds += now() - prepare_started; const double solve_started = now();
        try { runGlobalBA(sub, opt); }
        catch (const BAOverBudget&) { if (window <= 16) throw; window /= 2; continue; }
        catch (const BAIndexCapacity&) { if (window <= 16) throw; window /= 2; continue; }
        solve_seconds += now() - solve_started; ++windows;
        if (!std::isfinite(stats.final_cost) || stats.final_cost > stats.initial_cost * (1 + 1e-8) + 1e-8)
            throw std::runtime_error("regional coordination: invalid fixed-landmark BA cost");
        costs.initial += stats.initial_cost; costs.final += stats.final_cost;
        for (const auto& p : sub.points3D) costs.observations += p.second.track.size();
        addCameraEstimates(estimates, sub);
        for (const auto& kv : sub.images)
            if (group.core.count(kv.first)) rec.images.at(kv.first).pose = kv.second.pose;
        for (const auto& kv : sub.points3D) {
            auto& p = rec.points3D.at(kv.first);
            p.xyz = kv.second.xyz;
        }
        for (uint32_t id : group.core) remaining.erase(id);
    }
    applyCameraEstimates(rec, estimates);
    slog::diag(Tag::Map, "[regional] BA timing: %zu windows, graph %.3fs, prepare %.3fs, solve %.3fs, total %.3fs\n",
               windows, graph_seconds, prepare_seconds, solve_seconds, now() - started);
    return costs;
}

void cameraConsensus(Reconstruction& out, const std::vector<SavedRegion>& saved, size_t budget) {
    CameraEstimates groups;
    for (const auto& item : saved) {
        addCameraEstimates(groups, readSaved(item, budget));
    }
    applyCameraEstimates(out, groups);
}

void commitSavedStage(const fs::path& stage, const std::vector<SavedRegion>& saved, const RegionalCosts& costs) {
    model_store_detail::Writer c(stage.string() + ".costs.part");
    c.scalar(costs); c.finish(); regional::replaceFile(stage.string() + ".costs.part", stage.string() + ".costs");
    model_store_detail::Writer out(stage.string() + ".part"); out.scalar<uint64_t>(saved.size());
    for (const auto& model : saved) { out.scalar<uint64_t>(fs::file_size(model.path)); out.scalar<uint64_t>(model.memory); }
    out.finish(); regional::replaceFile(stage.string() + ".part", stage.string() + ".done");
}
RegionalCosts readStageCosts(const fs::path& stage) {
    model_store_detail::Reader in(stage.string() + ".costs"); auto costs = in.scalar<RegionalCosts>(); in.finish();
    if (!std::isfinite(costs.initial) || !std::isfinite(costs.final) || costs.initial < 0 || costs.final < 0)
        throw std::runtime_error("invalid regional coordination cost checkpoint");
    return costs;
}
struct RoundState { double baseline = 0, cost = 0; uint64_t observations = 0; uint32_t stop = 0; };
fs::path roundFile(const fs::path& root, size_t pass, const char* suffix) {
    return root / ("round-" + std::to_string(pass) + suffix);
}
void commitRound(const fs::path& root, size_t pass, const RoundState& state) {
    auto done = roundFile(root, pass, ".done"); model_store_detail::Writer out(done.string() + ".part");
    out.scalar<uint64_t>(0x32444e554f525353ull); out.scalar(state);
    for (auto suffix : {"-landmarks.bin", "-consensus.bin"}) out.scalar<uint64_t>(fs::file_size(roundFile(root, pass, suffix)));
    out.finish(); regional::replaceFile(done.string() + ".part", done);
}
RoundState readRound(const fs::path& root, size_t pass) {
    model_store_detail::Reader in(roundFile(root, pass, ".done"));
    if (in.scalar<uint64_t>() != 0x32444e554f525353ull) throw std::runtime_error("invalid regional round checkpoint");
    auto state = in.scalar<RoundState>();
    for (auto suffix : {"-landmarks.bin", "-consensus.bin"})
        if (in.scalar<uint64_t>() != fs::file_size(roundFile(root, pass, suffix))) throw std::runtime_error("regional round state changed");
    in.finish();
    if (!std::isfinite(state.baseline) || !std::isfinite(state.cost) || state.baseline < 0 || state.cost < 0 || !state.observations || state.stop > 2)
        throw std::runtime_error("invalid regional round cost state");
    return state;
}
} // namespace

std::optional<AutoResult> run_regional(SfmConfig cfg, const std::string& matches,
                                      const std::string& features, const std::string& images,
                                      const fs::path& sparse) {
    if (cfg.pairs != "spatial-blocks") return std::nullopt;
    MatchesIndex index;
    if (!indexMatches(matches, index) || !index.complete) throw std::runtime_error("regional mapping needs a complete matches.bin");
    if (!index.metadata.hasCameras()) throw std::runtime_error("regional mapping requires cached camera setup; use auto to create it");
    CameraSetup cameras; loadCameraSetup(index.metadata, cameras);
    SensorCaptures sensors = loadSensorCaptures(cfg, !cfg.quiet);
    std::unique_ptr<PriorSource> gps = makeSensorPriors(cfg, sensors, index.metadata, cameras.ids);
    if (!gps) { cfg.sensor_pairs = true; gps = makeExifGpsPriors(cfg, images, index.metadata, cameras, !cfg.quiet); }
    std::vector<Vec3> positions(index.images.size());
    for (uint32_t i = 0; i < index.images.size(); ++i) {
        cancel::check();
        if (!gps || !gps->position(i, positions[i]) || !std::isfinite(positions[i].x) ||
            !std::isfinite(positions[i].y) || !std::isfinite(positions[i].z)) {
            slog::warn(Tag::Map, M::match_blocks_need_gps, {(long long)i, (long long)index.images.size()}); return std::nullopt;
        }
        uint32_t count = 0;
        if (!peekFeatures((fs::path(features) / (index.images[i].name + ".bin")).string(), count) || count != index.images[i].num_features)
            throw std::runtime_error("regional mapping: cached matches do not index the current feature files");
    }
    std::string signature = "regional-v2\n" + regional::regionalBaseSignature(stageSignature(cfg, CMD_MAP)) +
        regional::regionalBaseSignature(stageSignature(cfg, CMD_AUTO)) +
        "matches=" + fs::absolute(matches).string() + "\nbytes=" + std::to_string(fs::file_size(matches)) +
        "\ntime=" + std::to_string(fs::last_write_time(matches).time_since_epoch().count()) +
        "\nfeatures=" + regionalInputDigest(features) + "\n";
    fs::path root = fs::path(features).parent_path() / ".regional" / std::to_string(signatureHash(signature));
    fs::create_directories(root);
    if (fs::exists(root / "signature.txt") && resume::recorded(root / "signature.txt") != signature)
        throw std::runtime_error("regional checkpoint signature collision");
    resume::store(root / "signature.txt", signature);
    configureMappingMemory(cfg, root.string());
    const size_t budget = cfg.bup.model_memory_bytes;
    std::vector<Region> plan;
    if (fs::exists(root / "plan.bin")) plan = readPlan(root, index.images.size());
    else { plan = regional::planRegions(index, positions, size_t(cfg.block_size)); savePlan(root, plan); }
    slog::out(Tag::Map, M::regional_plan, {(long long)plan.size(), cfg.block_size, root.string()});
    events::stage_begin(Stage::Map, index.images.size()); events::map_begin(index.images.size());
    Reconstruction summary; summary.cameras = cameras.cameras;
    std::vector<uint32_t> source(index.images.size());
    std::vector<std::vector<SavedRegion>> completed(plan.size());
    double started = now();
    for (size_t r = 0; r < plan.size(); ++r) {
        cancel::check(); fs::path stage = stageFile(root, 0, r); completed[r] = savedStage(stage);
        if (completed[r].empty() && !fs::exists(stage.string() + ".done")) {
            slog::out(Tag::Map, M::regional_region, {(long long)(r + 1), (long long)plan.size(), 0,
                (long long)plan[r].core.size(), (long long)(plan[r].images.size() - plan[r].core.size())});
            try {
                std::string input_key;
                for (uint32_t i : plan[r].images) input_key += std::to_string(i) + ",";
                fs::path local_stage = root / ("local-region-" + std::to_string(r) + "-" + std::to_string(signatureHash(input_key)));
                std::vector<SavedRegion> cached_local = savedStage(local_stage);
                std::vector<Reconstruction> models;
                if (!fs::exists(local_stage.string() + ".done")) {
                    RegionInput input = regional::loadRegion(index, plan[r], matches, features, budget);
                    MapIdsScope ids_scope(input.global);
                    SfmConfig local = cfg; local.mapper.initial_cameras = cameras.cameras;
                    local.mapper.report_progress = false;
                    local.mapper.known_focal_cameras = cameras.focal_known; local.mapper.given_focal_cameras = cameras.focal_given;
                    local.mapper.measured_focal_cameras = cameras.focal_measured;
                    RemappedPriorSource priors(*gps, input.global);
                    RigTable rigs = buildRigs(input.db, local, !cfg.quiet); SequenceTable seqs = buildSequences(input.db, local, !cfg.quiet);
                    Mapper mapper(input.db, input.features, local.mapper, input.db.camera_ids, &rigs, &seqs, cfg.sensor_map ? &priors : nullptr);
                    AssembleStats stats; models = runMapper(mapper, input.db, input.features, local, stats);
                    double finish = 0; models = finishModels(mapper, std::move(models), local, !cfg.quiet, finish);
                    models.erase(std::remove_if(models.begin(), models.end(), [](const auto& model) {
                        return model.numRegistered() < 3 || model.points3D.empty();
                    }), models.end());
                    resolveImageNames(models, images);
                    for (auto& model : models) regional::restoreFeatureIds(model, input, index);
                    recolorPoints(models, cfg); cached_local = commitStage(local_stage, models);
                } else {
                    for (const auto& item : cached_local) models.push_back(readSaved(item, budget));
                    slog::diag(Tag::Map, "[regional] resumed local geometry for region %zu/%zu\n", r + 1, plan.size());
                }
                std::set<uint32_t> unresolved;
                for (auto& model : models) {
                    if (alignRegional(model, summary, *gps, cfg.max_error)) continue;
                    for (const auto& kv : model.images) if (kv.second.registered) unresolved.insert(kv.first);
                }
                if (!unresolved.empty()) {
                    std::set<uint32_t> anchors;
                    for (const auto& kv : summary.images) if (kv.second.registered) anchors.insert(kv.first);
                    const size_t cap = std::max<size_t>(256, plan[r].core.size());
                    const size_t extra = plan[r].images.size() - plan[r].core.size();
                    const size_t step = std::min<size_t>(64, extra < cap ? cap - extra : 0);
                    if (step && regional::expandAlignmentContext(index, plan[r], unresolved, anchors, step)) {
                        savePlan(root, plan);
                        slog::diag(Tag::Map, "[regional] region %zu alignment needs more context; expanded to %zu images, completed regions retained\n",
                                   r + 1, plan[r].images.size());
                        --r; continue;
                    }
                    throw std::runtime_error("regional mapping: cannot align region to shared cameras or GPS; local geometry and completed regions retained");
                }
                completed[r] = commitStage(stage, models);
            } catch (const std::exception& e) {
                if (!dynamic_cast<const MergeOverBudget*>(&e) && !dynamic_cast<const BAOverBudget*>(&e) &&
                    !dynamic_cast<const BAIndexCapacity*>(&e) && !dynamic_cast<const regional::RegionInputOverBudget*>(&e)) throw;
                if (plan[r].core.size() < 32) throw;
                size_t middle = plan[r].core.size() / 2;
                Region a, b; a.core.assign(plan[r].core.begin(), plan[r].core.begin() + middle);
                b.core.assign(plan[r].core.begin() + middle, plan[r].core.end());
                auto halo = [&](Region& child) {
                    child.images = child.core; std::set<uint32_t> own(child.core.begin(), child.core.end());
                    std::map<uint32_t, uint64_t> votes;
                    for (const auto& p : index.pairs) {
                        if (own.count(p.image1) && !own.count(p.image2)) votes[p.image2] += p.count;
                        if (own.count(p.image2) && !own.count(p.image1)) votes[p.image1] += p.count;
                    }
                    std::vector<std::pair<uint64_t, uint32_t>> rank; for (const auto& kv : votes) rank.emplace_back(kv.second, kv.first);
                    std::sort(rank.rbegin(), rank.rend());
                    for (size_t k = 0; k < std::min(rank.size(), std::max<size_t>(8, child.core.size() / 4)); ++k) child.images.push_back(rank[k].second);
                    std::sort(child.images.begin(), child.images.end());
                };
                halo(a); halo(b); plan[r] = std::move(a); plan.push_back(std::move(b)); completed.resize(plan.size()); savePlan(root, plan);
                slog::diag(Tag::Map, "[regional] region exceeded working budget; split core and retry, completed regions retained\n");
                --r; continue;
            }
        } else slog::diag(Tag::Map, "[regional] resumed completed region %zu/%zu\n", r + 1, plan.size());
        for (const auto& item : completed[r]) { auto rec = readSaved(item, budget); addSummary(summary, rec, plan[r], source, progress::kMaxPoints / plan.size()); }
        events::progress(Stage::Map, r + 1, plan.size());
        progress::model(summary, true);
    }
    std::vector<SavedRegion> all;
    for (const auto& v : completed) all.insert(all.end(), v.begin(), v.end());
    if (all.empty()) throw std::runtime_error("regional mapping produced no models");
    cameraConsensus(summary, all, budget);
    std::ostringstream policy;
    policy << "shared-landmarks-v2\n" << cfg.regional_iterations << '\n' << std::setprecision(17)
           << cfg.regional_cost_tolerance << '\n' << cfg.regional_landmarks << "\ngraph-windows-fixed-support-balanced-landmarks-committed-validation-uniform-align-guard\n";
    fs::path coordination = root / ("landmarks-v2-" + std::to_string(signatureHash(policy.str())));
    fs::create_directories(coordination);
    const size_t cache = std::min<size_t>(64u << 20, budget / 32);
    const size_t shared_memory = std::min<size_t>(256u << 20, budget / 16);
    regional::SharedIndex shared;
    VkContext coordination_context;
    auto validation = [&](const std::vector<SavedRegion>& saved, const Reconstruction& poses, const regional::SharedIndex& points) {
        regional::Quality quality;
        for (size_t m = 0; m < saved.size(); ++m) {
            cancel::check(); auto rec = readSaved(saved[m], budget); points.inject(m, rec);
            auto local = regional::validationCost(coordination / ("validation-" + std::to_string(m) + ".bin"), rec, poses, m, cfg.max_error, cfg.threads);
            quality.cost += local.cost; quality.observations += local.observations;
        }
        return quality;
    };
    auto flatten = [&]() { all.clear(); for (const auto& v : completed) all.insert(all.end(), v.begin(), v.end()); };
    auto restoreRound = [&](size_t pass) {
        shared = regional::readSharedIndex(roundFile(coordination, pass, "-landmarks.bin"), all.size(), shared_memory);
        summary = model_store_detail::readModel(roundFile(coordination, pass, "-consensus.bin"));
        for (size_t r = 0; r < plan.size(); ++r) {
            auto saved = savedStage(stageFile(coordination, pass, r));
            if (saved.size() != completed[r].size()) throw std::runtime_error("regional round model count changed");
            completed[r] = std::move(saved);
        }
        flatten();
        if (shared.models != all.size()) throw std::runtime_error("regional shared index model count changed");
    };
    if (fs::exists(roundFile(coordination, 0, ".done"))) {
        readRound(coordination, 0); restoreRound(0);
    } else {
        shared = regional::buildSharedIndex(all.size(), [&](size_t m) { return readSaved(all.at(m), budget); },
            coordination / "index", cache, shared_memory, size_t(cfg.regional_landmarks));
        auto fit = regional::guardedSharedAlignment(shared, 50, shared_memory);
        slog::diag(Tag::Map, "[regional] alignment reference: %zu pairs, uniform %.9g, candidate %.9g, gate %.9g, uniform fallback %u\n",
            fit.reference_pairs, fit.uniform_reference_cost, fit.candidate_reference_cost, fit.reference_gate, unsigned(fit.used_uniform));
        slog::diag(Tag::Map, "[regional] shared alignment: %zu landmarks, %zu block pairs, cost %.9g -> %.9g, RMS %.9g\n",
            shared.landmarks.size(), fit.pair_count, fit.initial_cost, fit.final_cost, fit.rms);
        std::fill(source.begin(), source.end(), 0); summary.images.clear(); summary.points3D.clear();
        size_t model = 0;
        for (size_t r = 0; r < plan.size(); ++r) {
            fs::path stage = stageFile(coordination, 0, r); std::vector<SavedRegion> aligned;
            for (size_t m = 0; m < completed[r].size(); ++m, ++model) {
                cancel::check(); auto rec = readSaved(completed[r][m], budget); applySim3(rec, fit.transforms.at(model));
                SavedRegion saved{stage.string() + "-" + std::to_string(m) + ".bin", modelResidentBytes(rec)};
                regional::atomicModel(saved.path, rec); aligned.push_back(saved);
                regional::writeValidation(coordination / ("validation-" + std::to_string(model) + ".bin"), rec, model);
                addSummary(summary, rec, plan[r], source, progress::kMaxPoints / plan.size());
            }
            commitSavedStage(stage, aligned, {}); completed[r] = std::move(aligned);
        }
        flatten(); cameraConsensus(summary, all, budget);
        regional::triangulateShared(shared, summary, cfg.max_error);
        regional::writeSharedIndex(roundFile(coordination, 0, "-landmarks.bin"), shared);
        regional::atomicModel(roundFile(coordination, 0, "-consensus.bin"), summary);
        auto initial = validation(all, summary, shared);
        commitRound(coordination, 0, {initial.cost, initial.cost, initial.observations, 0});
    }
    RoundState accepted = readRound(coordination, 0);
    for (size_t pass = 1; pass <= size_t(cfg.regional_iterations); ++pass) {
        if (fs::exists(roundFile(coordination, pass, ".done"))) {
            auto state = readRound(coordination, pass);
            if (state.stop == 2) break;
            restoreRound(pass); accepted = state;
            slog::diag(Tag::Map, "[regional] resumed shared-landmark round %zu, cost %.9g\n", pass, state.cost);
            if (state.stop) break;
            continue;
        }
        events::stage_begin(Stage::Refine, plan.size());
        Reconstruction next = summary; next.images.clear(); next.points3D.clear(); std::fill(source.begin(), source.end(), 0);
        std::vector<std::vector<SavedRegion>> refined(plan.size());
        RegionalCosts total; size_t model = 0;
        CameraEstimates camera_estimates;
        for (size_t r = 0; r < plan.size(); ++r) {
            cancel::check(); fs::path stage = stageFile(coordination, pass, r); refined[r] = savedStage(stage);
            RegionalCosts costs;
            if (refined[r].empty() && !fs::exists(stage.string() + ".done")) {
                for (size_t m = 0; m < completed[r].size(); ++m) {
                    Reconstruction rec;
                    for (unsigned attempt = 0; attempt < 2; ++attempt) {
                        try {
                            rec = readSaved(completed[r][m], budget);
                            auto fixed = shared.inject(model + m, rec);
                            auto local = boundedRegionalBA(rec, summary, cfg, fixed, budget, coordination_context);
                            const double triangulation_started = now();
                            regional::triangulatePrivate(rec, fixed, cfg.max_error, cfg.threads);
                            slog::diag(Tag::Map, "[regional] private triangulation: %.3fs, threads %d\n", now() - triangulation_started, cfg.threads);
                            SavedRegion saved{stage.string() + "-" + std::to_string(m) + ".bin", modelResidentBytes(rec)};
                            regional::atomicModel(saved.path, rec); refined[r].push_back(saved);
                            costs.initial += local.initial; costs.final += local.final; costs.observations += local.observations;
                            break;
                        } catch (const Cancelled&) { throw; }
                        catch (const BAOverBudget&) { throw; }
                        catch (const BAIndexCapacity&) { throw; }
                        catch (const std::runtime_error& e) {
                            if (attempt) throw;
                            slog::diag(Tag::Map, "[regional] region %zu model %zu retry once: %s\n", r + 1, m, e.what());
                            std::this_thread::sleep_for(std::chrono::seconds(1)); cancel::check();
                        }
                    }
                    addSummary(next, rec, plan[r], source, progress::kMaxPoints / plan.size());
                    addCameraEstimates(camera_estimates, rec);
                }
                commitSavedStage(stage, refined[r], costs);
            } else {
                costs = readStageCosts(stage);
                for (const auto& item : refined[r]) {
                    auto rec = readSaved(item, budget);
                    addSummary(next, rec, plan[r], source, progress::kMaxPoints / plan.size());
                    addCameraEstimates(camera_estimates, rec);
                }
            }
            if (refined[r].size() != completed[r].size()) throw std::runtime_error("regional refinement model count changed");
            model += completed[r].size(); total.initial += costs.initial; total.final += costs.final; total.observations += costs.observations;
            slog::out(Tag::Map, M::regional_region, {(long long)(r + 1), (long long)plan.size(), (long long)pass,
                (long long)plan[r].core.size(), (long long)(plan[r].images.size() - plan[r].core.size())});
            events::progress(Stage::Refine, r + 1, plan.size());
        }
        std::vector<SavedRegion> flat; for (const auto& v : refined) flat.insert(flat.end(), v.begin(), v.end());
        applyCameraEstimates(next, camera_estimates);
        auto next_shared = shared;
        regional::triangulateShared(next_shared, next, cfg.max_error);
        RoundState state;
        auto committed = validation(flat, next, next_shared);
        state.baseline = accepted.baseline; state.cost = committed.cost; state.observations = committed.observations;
        state.stop = regional::qualityStop(committed, {accepted.cost, accepted.observations}, state.baseline,
                                         cfg.regional_cost_tolerance, pass == size_t(cfg.regional_iterations));
        regional::writeSharedIndex(roundFile(coordination, pass, "-landmarks.bin"), next_shared);
        regional::atomicModel(roundFile(coordination, pass, "-consensus.bin"), next);
        commitRound(coordination, pass, state);
        slog::diag(Tag::Map, "[regional] shared round %zu: committed cost %.9g -> %.9g, baseline %.9g, fixed observations %llu, window cost %.9g -> %.9g, stop %u\n",
            pass, accepted.cost, state.cost, state.baseline, (unsigned long long)state.observations, total.initial, total.final, state.stop);
        events::stage_end(Stage::Refine);
        if (state.stop == 2) break;
        completed = std::move(refined); summary = std::move(next); shared = std::move(next_shared); accepted = state;
        if (state.stop) break;
    }
    all.clear(); for (const auto& v : completed) all.insert(all.end(), v.begin(), v.end());
    regional::atomicModel(root / "consensus.bin", summary);
    events::stage_begin(Stage::Finish);
    fs::path pending = sparse / "regional-pending"; fs::create_directories(pending);
    std::vector<uint32_t> owners(index.images.size());
    for (uint32_t r = 0; r < plan.size(); ++r) for (uint32_t i : plan[r].core) owners.at(i) = r;
    auto stats = regional::exportUnified(all.size(), [&](size_t m) {
        auto rec = readSaved(all.at(m), budget); shared.inject(m, rec); return rec;
    }, summary,
        [&](uint32_t i) { return fs::path(features) / (index.images.at(i).name + ".bin"); },
        coordination / "export", pending, cache, cfg.max_error, &owners);
    ModelGauge gauge; gauge.metric = gauge.oriented = true; gauge.scale = "gps"; gauge.up = "gps"; writeGauge(pending, gauge);
    resume::store(pending / "regional-complete.txt", signature + policy.str());
    fs::create_directories(sparse);
    if (fs::exists(sparse / "0")) {
        fs::path previous = sparse / ("regional-previous-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::rename(sparse / "0", previous);
    }
    fs::rename(pending, sparse / "0");
    events::stage_end(Stage::Finish); events::stage_end(Stage::Map);
    AutoResult result; result.images = index.images.size(); result.registered = stats.images;
    result.points = stats.points; result.models = 1; result.mean_reproj = stats.error;
    result.median_reproj = stats.median;
    result.metric = true; result.partial = stats.images < index.images.size() || stats.error > 2. || stats.components > 1;
    result.sparse_dir = sparse; result.exit_code = stats.images < 2 || stats.points == 0 ? 2 : result.partial ? 3 : 0;
    Event event; event.kind = Event::Kind::Result; event.stage = Stage::Finish; event.images = result.images;
    event.registered = result.registered; event.points = result.points; event.models = 1;
    event.mean_reproj = result.mean_reproj; event.partial = result.partial; event.metric = result.metric; events::emit(event);
    slog::out(Tag::Run, M::sum_map, {spirula::i18n::format_duration(now() - started), (long long)result.registered,
        (long long)result.images, (long long)result.points, (long long)summary.cameras.size()});
    slog::out(Tag::Run, M::sum_model_error, {slog::num(stats.error, 3), slog::num(stats.median, 3), (long long)stats.observations});
    slog::diag(Tag::Map, "[regional] unified export: %llu images, %llu points, %llu observations; %llu conflicting tracks dropped\n",
               (unsigned long long)stats.images, (unsigned long long)stats.points, (unsigned long long)stats.observations,
               (unsigned long long)stats.conflicts);
    slog::diag(Tag::Map, "[regional] cross-region tracks: %llu, connected region groups: %zu\n",
               (unsigned long long)stats.cross_region_points, stats.components);
    slog::out(Tag::Run, M::sum_written, {(sparse / "0").string()});
    progress::gauge(true, true); progress::model(summary, true);
    return result;
}
} // namespace sfm

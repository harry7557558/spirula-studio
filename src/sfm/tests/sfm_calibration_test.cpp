#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <map>
#include <numeric>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "sfm/core/Features.h"
#include "sfm/core/Matches.h"
#include "sfm/core/Manifest.h"
#include "sfm/core/Model.h"
#include "sfm/tests/TestMain.h"

using namespace sfm;
namespace fs = std::filesystem;

struct TrackComponents {
    std::vector<uint32_t> offsets, parents;

    explicit TrackComponents(const MatchesDatabase& db) : offsets(db.images.size() + 1, 0) {
        uint64_t count = 0;
        for (size_t i = 0; i < db.images.size(); ++i) {
            count += db.images[i].num_features;
            if (count > UINT32_MAX) throw std::runtime_error("too many evaluation features");
            offsets[i + 1] = static_cast<uint32_t>(count);
        }
        parents.resize(static_cast<size_t>(count));
        std::iota(parents.begin(), parents.end(), 0);
        for (const auto& p : db.pairs) {
            for (const auto& m : p.matches) {
                uint32_t a = root(index(p.image1, m.idx1));
                uint32_t b = root(index(p.image2, m.idx2));
                parents[std::max(a, b)] = std::min(a, b);
            }
        }
    }

    uint32_t index(uint32_t image, uint32_t feature) const {
        if (image + 1 >= offsets.size() || feature >= offsets[image + 1] - offsets[image])
            throw std::runtime_error("match index outside feature table");
        return offsets[image] + feature;
    }

    uint32_t root(uint32_t i) {
        while (i != parents[i]) {
            parents[i] = parents[parents[i]];
            i = parents[i];
        }
        return i;
    }

    static uint32_t hash(uint32_t n) {
        n ^= n >> 16;
        n *= 0x7feb352dU;
        n ^= n >> 15;
        n *= 0x846ca68bU;
        return n ^ (n >> 16);
    }
};

struct GraphCoverage {
    size_t components = 0, matched_features = 0, conflicted_components = 0;
    size_t conflicted_features = 0, largest_component = 0;
};

static GraphCoverage graphCoverage(const MatchesDatabase& database) {
    TrackComponents graph(database);
    std::vector<uint32_t> count(graph.parents.size(), 0);
    std::vector<uint32_t> last_image(graph.parents.size(), UINT32_MAX);
    std::vector<uint8_t> conflicted(graph.parents.size(), 0);
    for (uint32_t image = 0; image < database.images.size(); ++image)
        for (uint32_t feature = 0; feature < database.images[image].num_features; ++feature) {
            const uint32_t root = graph.root(graph.index(image, feature));
            ++count[root];
            if (last_image[root] == image) conflicted[root] = 1;
            last_image[root] = image;
        }
    GraphCoverage result;
    for (size_t root = 0; root < count.size(); ++root) {
        if (count[root] < 2) continue;
        ++result.components;
        result.matched_features += count[root];
        result.largest_component = std::max(result.largest_component, size_t(count[root]));
        if (conflicted[root]) {
            ++result.conflicted_components;
            result.conflicted_features += count[root];
        }
    }
    return result;
}

static void auditGraph(const std::string& matches) {
    const auto coverage = graphCoverage(readMatches(matches));
    std::printf("{\"components\":%zu,\"matched_features\":%zu,\"conflicted_components\":%zu,"
                "\"conflicted_features\":%zu,\"largest_component\":%zu}\n",
                coverage.components, coverage.matched_features, coverage.conflicted_components,
                coverage.conflicted_features, coverage.largest_component);
}

static void splitMatches(const std::string& input, const std::string& train,
                         const std::string& heldout) {
    MatchesDatabase full = readMatches(input);
    TrackComponents components(full);
    MatchesDatabase test = full;
    for (size_t i = 0; i < full.pairs.size(); ++i) {
        std::vector<FeatureMatch> kept, hidden;
        for (const auto& m : full.pairs[i].matches) {
            const uint32_t r = components.root(components.index(full.pairs[i].image1, m.idx1));
            (TrackComponents::hash(r) % 10 == 0 ? hidden : kept).push_back(m);
        }
        full.pairs[i].matches = MatchList(std::move(kept));
        test.pairs[i].matches = MatchList(std::move(hidden));
    }
    auto drop = [](auto& pairs, size_t minimum) {
        pairs.erase(std::remove_if(pairs.begin(), pairs.end(), [&](const auto& p) {
            return p.matches.size() < minimum;
        }), pairs.end());
    };
    drop(full.pairs, 15);
    drop(test.pairs, 1);
    writeMatches(train, full);
    writeMatches(heldout, test);
    std::printf("split pairs: train %zu heldout %zu\n", full.pairs.size(), test.pairs.size());
}

static void permuteMatches(const std::string& input, const std::string& output, uint32_t seed) {
    auto database = readMatches(input);
    const auto images = database.images;
    const auto camera_ids = database.camera_ids;
    std::vector<uint32_t> order(images.size()), inverse(images.size());
    std::iota(order.begin(), order.end(), 0);
    std::mt19937 random(seed);
    std::shuffle(order.begin(), order.end(), random);
    for (size_t i = 0; i < order.size(); ++i) {
        inverse[order[i]] = static_cast<uint32_t>(i);
        database.images[i] = images[order[i]];
        if (camera_ids.size() == images.size()) database.camera_ids[i] = camera_ids[order[i]];
    }
    for (auto& pair : database.pairs) {
        pair.image1 = inverse[pair.image1];
        pair.image2 = inverse[pair.image2];
        if (pair.image1 > pair.image2) {
            std::swap(pair.image1, pair.image2);
            for (auto& match : pair.matches.mut()) std::swap(match.idx1, match.idx2);
        }
    }
    std::sort(database.pairs.begin(), database.pairs.end(), [](const auto& a, const auto& b) {
        return std::make_pair(a.image1, a.image2) < std::make_pair(b.image1, b.image2);
    });
    writeMatches(output, database);
}

static double predictionError(const Pose& a, const Vec3& ba, const Pose& b, const Vec3& bb,
                              const Pose& c, const Vec3& bc) {
    Vec3 X = triangulateDLT(poseToP(a), poseToP(b), ba, bb);
    const Vec3 xa = mul(a.R, X) + a.t, xb = mul(b.R, X) + b.t;
    const Vec3 xc = mul(c.R, X) + c.t;
    if (!(xa.dot(ba) > 0 && xb.dot(bb) > 0 && xc.dot(bc) > 0)) return 180;
    const Vec3 ray = xc.normalized();
    const double error = std::atan2(ray.cross(bc).norm(), ray.dot(bc)) * 180.0 / M_PI;
    return std::isfinite(error) ? error : 180;
}

static bool observableTriplet(const std::vector<Vec3>& rays, std::array<size_t, 3>& chosen) {
    if (rays.size() < 3) return false;
    double best = 0;
    for (size_t i = 0; i < rays.size(); ++i)
        for (size_t j = i + 1; j < rays.size(); ++j) {
            const double angle = std::atan2(rays[i].cross(rays[j]).norm(), rays[i].dot(rays[j]));
            const double parallax = std::min(angle, M_PI - angle);
            if (parallax > best) { best = parallax; chosen[0] = i; chosen[1] = j; }
        }
    if (best < 1.5 * M_PI / 180) return false;
    chosen[2] = rays.size() / 2;
    while (chosen[2] == chosen[0] || chosen[2] == chosen[1]) chosen[2] = (chosen[2] + 1) % rays.size();
    return true;
}

static Vec3 featureBearing(const Camera& camera, const FeatureSet& features, uint32_t feature) {
    const auto& keypoint = features.keypoints[feature];
    return camera.bearing({keypoint.x * double(camera.width) / features.width,
                           keypoint.y * double(camera.height) / features.height}).normalized();
}

static void evaluate(const std::string& model_dir, const std::string& matches,
                     const std::string& feature_dir, const std::string& selector_dir = {},
                     const std::string& selector_features = {}) {
    const MatchesDatabase db = readMatches(matches);
    TrackComponents components(db);
    const Reconstruction model = Reconstruction::readBinary(model_dir);
    std::map<std::string, const Image*> by_name;
    for (const auto& kv : model.images)
        by_name[fs::path(kv.second.name).replace_extension().generic_string()] = &kv.second;
    std::vector<const Image*> images(db.images.size(), nullptr);
    Reconstruction selector;
    std::vector<const Image*> selection_images(db.images.size(), nullptr);
    std::vector<FeatureSet> selection_features(db.images.size());
    std::map<std::string, const Image*> selection_names;
    if (!selector_dir.empty()) {
        selector = Reconstruction::readBinary(selector_dir);
        selector.points3D.clear();
        for (const auto& kv : selector.images)
            selection_names[fs::path(kv.second.name).replace_extension().generic_string()] = &kv.second;
    }
    std::vector<FeatureSet> features(db.images.size());
    for (size_t i = 0; i < db.images.size(); ++i) {
        fs::path name = db.images[i].name;
        const auto found = by_name.find(name.replace_extension().generic_string());
        if (found != by_name.end()) images[i] = found->second;
        fs::path path = fs::path(feature_dir) / db.images[i].name;
        path.replace_extension(".bin");
        features[i] = readFeatures(path.string(), false);
        if (features[i].count() != db.images[i].num_features)
            throw std::runtime_error("evaluation feature indexing mismatch");
        if (!selector_dir.empty()) {
            const auto selected = selection_names.find(name.generic_string());
            if (selected != selection_names.end()) selection_images[i] = selected->second;
            path = fs::path(selector_features) / db.images[i].name;
            path.replace_extension(".bin");
            selection_features[i] = readFeatures(path.string(), false);
            if (selection_features[i].count() != features[i].count())
                throw std::runtime_error("selector feature indexing mismatch");
        }
    }
    std::map<uint32_t, std::vector<TrackElement>> tracks;
    for (const auto& p : db.pairs)
        for (const auto& m : p.matches) {
            const auto r = components.root(components.index(p.image1, m.idx1));
            if (TrackComponents::hash(r) % 4 != 0) continue;
            tracks[r].push_back({p.image1, m.idx1});
            tracks[r].push_back({p.image2, m.idx2});
        }
    std::vector<double> errors;
    size_t conflicted = 0, missing = 0, unobservable = 0;
    for (auto& entry : tracks) {
        auto& t = entry.second;
        std::sort(t.begin(), t.end(), [](const auto& a, const auto& b) {
            if (a.image_id != b.image_id) return a.image_id < b.image_id;
            return a.point2D_idx < b.point2D_idx;
        });
        t.erase(std::unique(t.begin(), t.end(), [](const auto& a, const auto& b) {
            return a.image_id == b.image_id && a.point2D_idx == b.point2D_idx;
        }), t.end());
        bool clash = false;
        for (size_t i = 1; i < t.size(); ++i) clash |= t[i].image_id == t[i - 1].image_id;
        if (clash) { ++conflicted; continue; }
        if (t.size() < 3) continue;
        std::array<TrackElement, 3> chosen = {t.front(), t.back(), t[t.size() / 2]};
        if (!selector_dir.empty()) {
            std::vector<TrackElement> available;
            for (const auto& e : t)
                if (selection_images[e.image_id]) available.push_back(e);
            std::vector<TrackElement> sampled;
            const size_t count = std::min<size_t>(24, available.size());
            for (size_t i = 0; i < count; ++i)
                sampled.push_back(available[i * (available.size() - 1) / std::max<size_t>(1, count - 1)]);
            std::vector<Vec3> rays;
            for (const auto& e : sampled) {
                const Image& im = *selection_images[e.image_id];
                rays.push_back(mul(transpose(im.pose.R),
                    featureBearing(selector.cameras.at(im.camera_id),
                                   selection_features[e.image_id], e.point2D_idx)));
            }
            std::array<size_t, 3> indices{};
            if (!observableTriplet(rays, indices)) { ++unobservable; continue; }
            for (size_t i = 0; i < 3; ++i) chosen[i] = sampled[indices[i]];
        }
        Pose poses[3];
        Vec3 bearings[3];
        bool present = true;
        for (int i = 0; i < 3; ++i) {
            const auto& e = chosen[i];
            const Image* im = images[e.image_id];
            if (!im) { present = false; continue; }
            poses[i] = im->pose;
            bearings[i] = featureBearing(model.cameras.at(im->camera_id),
                                        features[e.image_id], e.point2D_idx);
        }
        if (!present) ++missing;
        errors.push_back(present ? predictionError(poses[0], bearings[0], poses[1], bearings[1],
                                                   poses[2], bearings[2]) : 180);
    }
    std::sort(errors.begin(), errors.end());
    auto percentile = [&](double q) {
        return errors.empty() ? 180 : errors[static_cast<size_t>(q * (errors.size() - 1))];
    };
    auto fraction = [&](double limit) {
        return errors.empty() ? 0.0 : double(std::lower_bound(errors.begin(), errors.end(), limit) -
                                             errors.begin()) / errors.size();
    };
    std::printf("{\"triplets\":%zu,\"conflicted_tracks\":%zu,\"missing\":%zu,\"unobservable\":%zu,"
                "\"median_deg\":%.9g,\"p90_deg\":%.9g,\"within_0_1_deg\":%.9g,"
                "\"within_0_5_deg\":%.9g,\"within_1_deg\":%.9g}\n",
                errors.size(), conflicted, missing, unobservable, percentile(0.5), percentile(0.9),
                fraction(0.1), fraction(0.5), fraction(1));
}

static void audit(const std::string& model_dir) {
    const Reconstruction model = Reconstruction::readBinary(model_dir);
    struct ResidualBin { size_t count = 0; double dx = 0, dy = 0, radial = 0, squared = 0; };
    std::map<uint32_t, std::array<ResidualBin, 6>> bins;
    std::vector<double> coverage, support;
    size_t observations = 0, inconsistent = 0, duplicate_views = 0, invalid_projection = 0;
    size_t histogram[7] = {};
    for (const auto& entry : model.points3D) {
        const auto& point = entry.second;
        const size_t length = point.track.size();
        if (length < 2) ++inconsistent;
        const int bucket = length <= 4 ? std::max(0, (int)length - 2) :
                           length <= 8 ? 3 : length <= 16 ? 4 : length <= 32 ? 5 : 6;
        ++histogram[bucket];
        std::set<uint32_t> images;
        for (const auto& e : point.track) {
            if (!images.insert(e.image_id).second) ++duplicate_views;
            const auto image = model.images.find(e.image_id);
            if (image == model.images.end() || e.point2D_idx >= image->second.point3D_ids.size() ||
                image->second.point3D_ids[e.point2D_idx] != entry.first) ++inconsistent;
        }
    }
    for (const auto& entry : model.images) {
        const Image& im = entry.second;
        const Camera& camera = model.cameras.at(im.camera_id);
        bool occupied[64] = {};
        size_t count = 0;
        for (size_t i = 0; i < im.point3D_ids.size(); ++i) {
            if (im.point3D_ids[i] == kInvalidPoint3D) continue;
            const auto point = model.points3D.find(im.point3D_ids[i]);
            if (point == model.points3D.end()) { ++inconsistent; continue; }
            if (std::none_of(point->second.track.begin(), point->second.track.end(),
                             [&](const TrackElement& e) {
                                 return e.image_id == im.id && e.point2D_idx == i;
                             })) ++inconsistent;
            ++count;
            const Vec2& p = im.points2D[i];
            const int x = (int)std::clamp(8 * p.x / camera.width, 0.0, 7.0);
            const int y = (int)std::clamp(8 * p.y / camera.height, 0.0, 7.0);
            occupied[8*y+x] = true;
            const Vec2 prediction = camera.project(mul(im.pose.R, point->second.xyz) + im.pose.t);
            double dx = prediction.x - p.x, dy = prediction.y - p.y;
            if (camera.isSpherical()) dx = std::remainder(dx, (double)camera.width);
            if (!std::isfinite(dx) || !std::isfinite(dy)) { ++invalid_projection; continue; }
            const double rx = 2 * (p.x - camera.cx) / camera.width;
            const double ry = 2 * (p.y - camera.cy) / camera.height;
            const double radius = std::hypot(rx, ry);
            auto& bin = bins[im.camera_id][std::min(5, (int)(radius * 4))];
            ++bin.count;
            bin.dx += dx;
            bin.dy += dy;
            bin.radial += radius > 1e-12 ? (dx*rx + dy*ry) / radius : 0;
            bin.squared += dx*dx + dy*dy;
        }
        observations += count;
        support.push_back((double)count);
        coverage.push_back(std::count(occupied, occupied + 64, true) / 64.0);
    }
    std::sort(support.begin(), support.end());
    std::sort(coverage.begin(), coverage.end());
    std::printf("{\"images\":%zu,\"points\":%zu,\"observations\":%zu,\"inconsistent\":%zu,"
                "\"duplicate_views\":%zu,\"invalid_projection\":%zu,"
                "\"median_view_support\":%.0f,\"median_grid_coverage\":%.6f,"
                "\"track_histogram\":[%zu,%zu,%zu,%zu,%zu,%zu,%zu],\"camera_radial_bins\":[",
                model.images.size(), model.points3D.size(), observations, inconsistent, duplicate_views,
                invalid_projection,
                support.empty() ? 0 : support[support.size()/2],
                coverage.empty() ? 0 : coverage[coverage.size()/2],
                histogram[0],histogram[1],histogram[2],histogram[3],histogram[4],histogram[5],histogram[6]);
    bool first = true;
    for (const auto& camera : bins)
        for (size_t i = 0; i < camera.second.size(); ++i) {
            const auto& bin = camera.second[i];
            if (!bin.count) continue;
            std::printf("%s{\"camera\":%u,\"bin\":%zu,\"count\":%zu,\"mean_dx_px\":%.6g,"
                        "\"mean_dy_px\":%.6g,\"mean_radial_px\":%.6g,\"rms_px\":%.6g}",
                        first ? "" : ",",camera.first,i,bin.count,bin.dx/bin.count,bin.dy/bin.count,
                        bin.radial/bin.count,std::sqrt(bin.squared/bin.count));
            first = false;
        }
    std::printf("]}\n");
}

static void auditRig(const std::string& model_dir, const std::string& matches,
                     const std::string& manifest_file) {
    const auto model = Reconstruction::readBinary(model_dir);
    const auto database = readMatches(matches);
    auto manifest = manifest_read(manifest_file);
    for (auto& rig : manifest.rigs) {
        const std::string error = applyRigKind(rig);
        if (!error.empty()) throw std::runtime_error(error);
    }
    std::vector<std::string> names;
    for (const auto& im : database.images) names.push_back(im.name);
    const auto table = buildRigTable(names, manifest.rigs);
    std::map<std::string, const Image*> by_name;
    std::vector<double> cx, cy, cz;
    for (const auto& entry : model.images) {
        by_name[fs::path(entry.second.name).replace_extension().generic_string()] = &entry.second;
        const Vec3 c = cameraCenter(entry.second.pose);
        cx.push_back(c.x); cy.push_back(c.y); cz.push_back(c.z);
    }
    const Vec3 middle{rig_detail::medianOf(cx), rig_detail::medianOf(cy), rig_detail::medianOf(cz)};
    std::vector<double> radii;
    for (const auto& entry : model.images) radii.push_back((cameraCenter(entry.second.pose) - middle).norm());
    const double radius = std::max(1e-12, rig_detail::medianOf(radii));
    auto find = [&](uint32_t id) -> const Image* {
        if (id == kNoImage) return nullptr;
        const auto found = by_name.find(fs::path(names[id]).replace_extension().generic_string());
        return found == by_name.end() ? nullptr : found->second;
    };
    auto percentile = [](std::vector<double> values, double q) {
        if (values.empty()) return 0.0;
        const size_t k = std::min(values.size() - 1, (size_t)(q * values.size()));
        std::nth_element(values.begin(), values.begin() + k, values.end());
        return values[k];
    };
    std::printf("{\"rig_members\":[");
    bool first = true;
    for (size_t r = 0; r < table.rigs.size(); ++r) {
        const auto& rig = table.rigs[r];
        for (size_t m = 1; m < rig.members.size(); ++m) {
            const bool known = rig.members[0].has_ext && rig.members[m].has_ext;
            const Mat3 nominal = known ? relativePose(rig.members[0].ext, rig.members[m].ext).R
                                       : mat3Identity();
            std::vector<Mat3> rotations;
            std::vector<double> nominal_error, separation;
            size_t possible = 0;
            for (const auto& frame : rig.frames) {
                if (frame[0] != kNoImage && frame[m] != kNoImage) ++possible;
                const Image* a = find(frame[0]);
                const Image* b = find(frame[m]);
                if (!a || !b) continue;
                const Mat3 rotation = relativePose(a->pose, b->pose).R;
                rotations.push_back(rotation);
                if (known) nominal_error.push_back(rotationAngleDeg(mul(rotation, transpose(nominal))));
                separation.push_back((cameraCenter(a->pose) - cameraCenter(b->pose)).norm() / radius);
            }
            const Mat3 mean = rotations.empty() ? mat3Identity() : rig_detail::meanRotation(rotations);
            std::vector<double> scatter;
            for (const auto& rotation : rotations)
                scatter.push_back(rotationAngleDeg(mul(rotation, transpose(mean))));
            std::printf("%s{\"rig\":%zu,\"member\":%zu,\"possible_pairs\":%zu,\"pairs\":%zu,"
                        "\"nominal_known\":%s,\"nominal_median_deg\":%.9g,"
                        "\"scatter_median_deg\":%.9g,\"scatter_p90_deg\":%.9g,"
                        "\"separation_median_radius\":%.9g,\"separation_p90_radius\":%.9g}",
                        first ? "" : ",", r, m, possible, rotations.size(), known ? "true" : "false",
                        percentile(nominal_error, 0.5), percentile(scatter, 0.5), percentile(scatter, 0.9),
                        percentile(separation, 0.5), percentile(separation, 0.9));
            first = false;
        }
    }
    std::printf("]}\n");
}

static int calibrationTest(int argc, char** argv) {
    if (argc == 3 && std::string(argv[1]) == "--graph-audit") {
        auditGraph(argv[2]);
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--audit") {
        audit(argv[2]);
        return 0;
    }
    if (argc == 5 && std::string(argv[1]) == "--rig-audit") {
        auditRig(argv[2], argv[3], argv[4]);
        return 0;
    }
    if (argc == 5 && std::string(argv[1]) == "--split") {
        splitMatches(argv[2], argv[3], argv[4]);
        return 0;
    }
    if (argc == 5 && std::string(argv[1]) == "--permute") {
        permuteMatches(argv[2], argv[3], static_cast<uint32_t>(std::stoul(argv[4])));
        return 0;
    }
    if (argc == 5 && std::string(argv[1]) == "--evaluate") {
        evaluate(argv[2], argv[3], argv[4]);
        return 0;
    }
    if ((argc == 6 || argc == 7) && std::string(argv[1]) == "--evaluate-observable") {
        evaluate(argv[2], argv[3], argv[4], argv[5], argc == 7 ? argv[6] : argv[4]);
        return 0;
    }
    if (argc != 1) throw std::runtime_error("expected --audit MODEL, --graph-audit MATCHES, --rig-audit MODEL MATCHES MANIFEST, --split INPUT TRAIN TEST, --permute INPUT OUTPUT SEED, --evaluate MODEL MATCHES FEATURES or --evaluate-observable MODEL MATCHES FEATURES SELECTOR_MODEL [SELECTOR_FEATURES]");
    const Vec3 X{0.2, -0.1, 4};
    const Pose a{mat3Identity(), {-1, 0, 0}}, b{mat3Identity(), {1, 0, 0}};
    const Pose c{mat3Identity(), {0, 0.5, 0}}, wrong{mat3Identity(), {0, 0.8, 0}};
    const Vec3 ba = (X + a.t).normalized(), bb = (X + b.t).normalized();
    const Vec3 bc = (X + c.t).normalized();
    const double good = predictionError(a, ba, b, bb, c, bc);
    const double bad = predictionError(a, ba, b, bb, wrong, bc);
    bool ok = good < 1e-8 && bad > 1;
    std::printf("%s: third-view prediction good %.9g deg wrong %.9g deg\n", ok ? "PASS" : "FAIL", good, bad);
    if (experimentInt("SFM_ACUTE_PARALLAX", 0, 1)) {
        const Vec3 origin{};
        const bool parallax_ok = triangulationAngle(origin, {-1,0,0}, {1,0,0}) < 1e-8 &&
            std::fabs(triangulationAngle(origin, {-1,0,0}, {0,-1,0}) - M_PI/2) < 1e-8 &&
            triangulationAngle(origin, {-1,0,0}, {1,0.001,0}) < 0.002;
        std::printf("%s: opposed rays have no usable triangulation parallax\n",
                    parallax_ok ? "PASS" : "FAIL");
        ok &= parallax_ok;
    }
    std::array<size_t, 3> selected{};
    const bool observed = observableTriplet({ba, ba, bb}, selected) && selected[0] == 0 &&
                          selected[1] == 2 && selected[2] == 1 &&
                          !observableTriplet({ba, ba, ba}, selected);
    std::printf("%s: fixed reference parallax avoids a co-located triangulation pair\n",
                observed ? "PASS" : "FAIL");
    ok &= observed;
    MatchesDatabase graph;
    for (int i = 0; i < 4; ++i) graph.images.push_back({"view" + std::to_string(i), 20});
    for (uint32_t i = 0; i < 3; ++i) {
        TwoViewMatches pair;
        pair.image1 = i;
        pair.image2 = i + 1;
        for (uint32_t j = 0; j < 20; ++j) pair.matches.push_back({j, j});
        graph.pairs.push_back(std::move(pair));
    }
    TrackComponents components(graph);
    bool disjoint = true;
    size_t hidden = 0;
    for (uint32_t j = 0; j < 20; ++j) {
        const uint32_t root = components.root(components.index(0, j));
        hidden += TrackComponents::hash(root) % 10 == 0;
        for (uint32_t i = 1; i < 4; ++i)
            disjoint &= components.root(components.index(i, j)) == root;
    }
    disjoint &= hidden > 0 && hidden < 20;
    std::printf("%s: held-out partition keeps complete correspondence components together\n",
                disjoint ? "PASS" : "FAIL");
    ok &= disjoint;
    const auto clean_graph = graphCoverage(graph);
    graph.pairs.front().matches.push_back({0, 1});
    const auto conflicted_graph = graphCoverage(graph);
    const bool coverage_ok = clean_graph.components == 20 && clean_graph.matched_features == 80 &&
        clean_graph.conflicted_features == 0 && conflicted_graph.components == 19 &&
        conflicted_graph.conflicted_components == 1 && conflicted_graph.conflicted_features == 8 &&
        conflicted_graph.largest_component == 8;
    std::printf("%s: graph audit counts observations hidden by a conflicting component\n",
                coverage_ok ? "PASS" : "FAIL");
    ok &= coverage_ok;
    for (double direction : {1.0, -1.0}) {
        const Vec3 truth{0.2, -0.1, direction * 4};
        const Vec3 start = truth + Vec3{0.1, -0.08, direction * 0.4};
        std::vector<PointObservation> observations;
        for (int i = 0; i < 8; ++i) {
            const Pose pose{mat3Identity(), {0.4 * i - 1.4, 0.15 * (i % 3), 0}};
            Vec3 ray = (truth + pose.t).normalized();
            if (i == 7) ray = (ray + Vec3{0.05, 0.03, 0}).normalized();
            observations.push_back({poseToP(pose), ray, 0.001});
        }
        Mat3 normal;
        Vec3 gradient;
        pointAngularCost(start, observations, &normal, &gradient);
        const double analytic[] = {gradient.x, gradient.y, gradient.z};
        double gradient_error = 0;
        for (int k = 0; k < 3; ++k) {
            Vec3 delta{k == 0 ? 1e-6 : 0, k == 1 ? 1e-6 : 0, k == 2 ? 1e-6 : 0};
            const double numerical = (pointAngularCost(start + delta, observations) -
                                      pointAngularCost(start - delta, observations)) / 2e-6;
            gradient_error = std::max(gradient_error, std::fabs(numerical - analytic[k]) /
                                                       std::max(1.0, std::fabs(numerical)));
        }
        const Vec3 fit = refineTriangulatedPoint(start, observations, 20);
        const double error = (fit - truth).norm();
        const bool pass = gradient_error < 1e-6 && error < 0.025 &&
                          pointAngularCost(fit, observations) < pointAngularCost(start, observations);
        std::printf("%s: ray refinement direction %.0f position %.6g gradient %.3g\n",
                    pass ? "PASS" : "FAIL", direction, error, gradient_error);
        ok &= pass;
    }
    return ok ? 0 : 1;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, calibrationTest); }

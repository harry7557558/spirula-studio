#include "sfm/map/RegionalTransform.h"
#include "sfm/map/Orient.h"
#include "sfm/tests/TestMain.h"
#include <chrono>
#include <regex>
#include <iomanip>

using namespace sfm;
namespace fs = std::filesystem;
static void require(bool v, const char* message) { if (!v) throw std::runtime_error(message); }

static Reconstruction fixture(uint32_t model) {
    Reconstruction rec; rec.cameras[0] = Camera::defaultFor(0, 800, 600, 500, CamModel::Pinhole);
    for (uint32_t i = 0; i < 4; ++i) {
        Image im; im.id = i; im.camera_id = 0; im.registered = true;
        im.pose.t = {double(i) * -.5, 0, 0}; im.points2D.resize(80); im.point3D_ids.resize(80);
        rec.images[i] = im;
    }
    for (uint32_t k = 0; k < 80; ++k) {
        uint64_t id = 10000 * (model + 1) + k;
        Point3D p; p.xyz = {double(k % 10) * .2 - 1, double(k / 10) * .2 - .8, 5 + .03 * (k % 7)};
        for (auto& item : rec.images) {
            auto& im = item.second; Vec3 q = p.xyz + im.pose.t;
            im.points2D[k] = {500 * q.x / q.z + 400, 500 * q.y / q.z + 300};
            im.point3D_ids[k] = id; p.track.push_back({im.id, k});
        }
        rec.points3D[id] = p;
    }
    if (model) applySim3(rec, {1 + .15 * model, so3Exp({.02 * model, -.03 * model, .01 * model}), {double(model), -2., .3}});
    return rec;
}

static int probe(const fs::path& input, const fs::path& scratch, const fs::path& reference = {}) {
    std::vector<std::pair<std::pair<unsigned, unsigned>, fs::path>> ordered;
    std::regex pattern("pass-0-region-([0-9]+)-([0-9]+)\\.bin");
    for (const auto& f : fs::directory_iterator(input)) {
        std::smatch match; std::string name = f.path().filename().string();
        if (std::regex_match(name, match, pattern)) ordered.push_back({{unsigned(std::stoul(match[1])), unsigned(std::stoul(match[2]))}, f.path()});
    }
    std::sort(ordered.begin(), ordered.end()); require(!ordered.empty(), "no initial checkpoint models found");
    auto index = regional::buildSharedIndex(ordered.size(), [&](size_t i) { return model_store_detail::readModel(ordered.at(i).second); },
        scratch, 32u << 20, 128u << 20, 100000);
    auto fit = regional::guardedSharedAlignment(index);
    regional::writeSharedIndex(scratch / "shared.bin", index);
    double reference_before = 0, reference_after = 0; uint64_t reference_pairs = 0;
    if (!reference.empty()) {
        auto baseline = regional::readSharedIndex(reference, ordered.size(), 128u << 20);
        for (size_t m = 0; m < ordered.size(); ++m) {
            auto rec = model_store_detail::readModel(ordered[m].second);
            for (const auto& item : baseline.by_model[m]) for (auto& member : baseline.landmarks[item.second].members)
                if (member.model == m) member.xyz = rec.points3D.at(member.point).xyz;
        }
        auto solved = baseline; auto old_fit = regional::alignSharedBlocks(solved);
        auto rho = [](double e) { return e <= 5 ? e * e : 5 * (2 * e - 5); };
        for (const auto& p : baseline.landmarks) if (p.valid)
            for (size_t a = 0; a < p.members.size(); ++a) for (size_t b = a + 1; b < p.members.size(); ++b) {
                const auto& x = p.members[a]; const auto& y = p.members[b];
                reference_before += rho((transformPoint(old_fit.transforms[x.model], x.xyz) - transformPoint(old_fit.transforms[y.model], y.xyz)).norm());
                reference_after += rho((transformPoint(fit.transforms[x.model], x.xyz) - transformPoint(fit.transforms[y.model], y.xyz)).norm());
                ++reference_pairs;
            }
    }
    std::ofstream report(scratch / "probe.json");
    report << std::setprecision(17) << "{\"models\":" << index.models << ",\"candidates\":" << index.candidates
        << ",\"landmarks\":" << index.landmarks.size() << ",\"conflicts\":" << index.conflicts
        << ",\"pairs\":" << fit.pair_count << ",\"initial_cost\":" << fit.initial_cost
        << ",\"final_cost\":" << fit.final_cost << ",\"rms\":" << fit.rms
        << ",\"fixed_reference_pairs\":" << reference_pairs << ",\"fixed_reference_huber5_old\":" << reference_before
        << ",\"fixed_reference_huber5_new\":" << reference_after
        << ",\"uniform_alignment_fallback\":" << (fit.used_uniform ? "true" : "false")
        << ",\"guard_reference_pairs\":" << fit.reference_pairs
        << ",\"guard_uniform_cost\":" << fit.uniform_reference_cost
        << ",\"guard_candidate_cost\":" << fit.candidate_reference_cost << "}\n";
    require(bool(report), "cannot save probe report");
    std::printf("PASS read-only checkpoints: %zu models, %zu shared landmarks, %zu pairs, RMS %.9g\n",
        index.models, index.landmarks.size(), fit.pair_count, fit.rms); return 0;
}

static int test(int argc, char** argv) {
    if ((argc == 4 || argc == 5) && std::string(argv[1]) == "--probe") return probe(argv[2], argv[3], argc == 5 ? fs::path(argv[4]) : fs::path{});
    fs::path root = fs::temp_directory_path() / ("sfm-shared-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root); std::vector<Reconstruction> models;
    for (uint32_t i = 0; i < 3; ++i) models.push_back(fixture(i));
    auto index = regional::buildSharedIndex(models.size(), [&](size_t i) { return models.at(i); }, root / "index", 1u << 20, 8u << 20);
    require(index.landmarks.size() == 80 && index.conflicts == 0, "shared identity indexing failed");
    for (const auto& p : index.landmarks) require(p.members.size() == 3 && p.observations.size() == 4, "shared observations not deduplicated");
    auto aligned = index; auto result = regional::alignSharedBlocks(aligned);
    require(result.pair_count == 3 && result.rms < 1e-8, "block transforms did not recover geometry");
    for (const auto& p : aligned.landmarks) for (const auto& member : p.members)
        require((member.xyz - p.xyz).norm() < 1e-8, "common landmarks remain inconsistent");
    require((transformPoint(result.transforms[0], {2, 3, 4}) - Vec3{2, 3, 4}).norm() < 1e-12, "reference gauge moved");
    auto noisy = index; noisy.landmarks.clear();
    for (const auto& p : index.landmarks) for (uint32_t group = 0; group < 3; ++group) {
        auto q = p; q.key = p.key * 3 + group;
        q.members.erase(std::remove_if(q.members.begin(), q.members.end(), [&](const auto& m) { return m.model == group; }), q.members.end());
        for (auto& m : q.members) if (m.model == q.members.back().model) {
            m.xyz.x += .01 * (group + 1) + .005 * std::sin(double(m.point));
            m.xyz.y += .008 * group + .003 * std::cos(double(m.point));
        }
        noisy.landmarks.push_back(std::move(q));
    }
    auto reduced = regional::alignSharedBlocks(noisy);
    std::printf("Joint loop cost %.9g -> %.9g\n", reduced.initial_cost, reduced.final_cost);
    require(reduced.final_cost < reduced.initial_cost * .9999 && std::isfinite(reduced.rms), "joint loop objective did not improve");
    auto biased = index;
    for (auto& p : biased.landmarks) for (auto& m : p.members) if (m.model == 2) m.xyz.x += .02;
    biased.alignment_reference = index.landmarks;
    auto guarded = regional::guardedSharedAlignment(biased);
    require(guarded.used_uniform && guarded.candidate_reference_cost > guarded.uniform_reference_cost,
        "seam-biased transform did not fall back to uniform reference");
    auto disconnected = index; disconnected.models = 4; bool rejected = false;
    try { regional::alignSharedBlocks(disconnected); } catch (const std::runtime_error&) { rejected = true; }
    require(rejected, "disconnected blocks silently accepted");
    for (auto& p : aligned.landmarks) p.xyz = p.xyz + Vec3{.02, -.01, .05};
    double before = regional::sharedCost(aligned, models[0], 3);
    regional::triangulateShared(aligned, models[0], 3);
    require(regional::sharedCost(aligned, models[0], 3) < before * 1e-6, "shared triangulation did not improve reprojection");
    auto fixed = aligned.inject(1, models[1]); require(fixed.size() == 80, "local point IDs were conflated with another model");
    auto private_model = fixture(0);
    for (auto& p : private_model.points3D) p.second.xyz = p.second.xyz + Vec3{.01, .02, -.04};
    Vec3 held = private_model.points3D.at(10000).xyz;
    regional::triangulatePrivate(private_model, {10000}, 3);
    require((private_model.points3D.at(10000).xyz - held).norm() == 0, "private triangulation changed a shared constraint");
    require((private_model.points3D.at(10001).xyz - fixture(0).points3D.at(10001).xyz).norm() < 1e-8, "private triangulation did not recover geometry");
    regional::writeSharedIndex(root / "shared.bin", aligned);
    auto loaded = regional::readSharedIndex(root / "shared.bin", 3, 8u << 20);
    require(loaded.landmarks.size() == 80 && loaded.inject(2, models[2]).size() == 80, "checkpoint identities changed");
    rejected = false;
    try { regional::readSharedIndex(root / "shared.bin", 3, 1); } catch (const std::runtime_error&) { rejected = true; }
    require(rejected, "checkpoint budget ignored");
    fs::resize_file(root / "shared.bin", fs::file_size(root / "shared.bin") - 1); rejected = false;
    try { regional::readSharedIndex(root / "shared.bin", 3, 8u << 20); } catch (const std::exception&) { rejected = true; }
    require(rejected, "truncated checkpoint accepted");
    require(regional::regionalBaseSignature("block-size=1659\nregional-iterations=8\nregional-cost-tolerance=0.001\nregional-landmarks=100000\nmax-error=3\n") ==
        "block-size=1659\nmax-error=3\n", "coordination options invalidate initial geometry");
    fs::remove_all(root); std::puts("PASS shared landmarks, joint transforms, triangulation, checkpoint validation"); return 0;
}
int main(int argc, char** argv) { return sfmTestMain(argc, argv, test); }

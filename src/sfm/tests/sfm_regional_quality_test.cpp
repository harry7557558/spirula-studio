#include "sfm/map/RegionalQuality.h"
#include "sfm/map/RegionalWindows.h"
#include "sfm/map/RegionalLandmarks.h"
#include "sfm/tests/TestMain.h"
#include <chrono>
#include <cstring>

using namespace sfm;
static void require(bool v, const char* message) { if (!v) throw std::runtime_error(message); }

static Reconstruction fixture(bool shuffled) {
    Reconstruction rec; rec.cameras[0] = Camera::defaultFor(0, 800, 600, 500, CamModel::Pinhole);
    auto id = [&](uint32_t i) { return shuffled ? (i * 7 + 5) % 12 : i; };
    for (uint32_t i = 0; i < 12; ++i) {
        Image im; im.id = id(i); im.camera_id = 0; im.registered = true; im.pose.t = {-double(i), 0, 0};
        rec.images[im.id] = im;
    }
    for (uint32_t i = 0; i < 11; ++i) for (uint32_t k = 0; k < 8; ++k) {
        Point3D p; p.xyz = {double(i) + .1 * k, .2 * k, 5}; uint64_t point = i * 8 + k;
        for (uint32_t image : {id(i), id(i + 1)}) {
            auto& im = rec.images.at(image); uint32_t f = uint32_t(im.points2D.size());
            im.points2D.push_back(rec.cameras[0].project(mul(im.pose.R, p.xyz) + im.pose.t));
            im.point3D_ids.push_back(point); p.track.push_back({image, f});
        }
        rec.points3D[point] = p;
    }
    return rec;
}

static regional::Quality referenceCost(const std::filesystem::path& path, const Reconstruction& local,
                                       const Reconstruction& committed, uint64_t model, double gate) {
    model_store_detail::Reader in(path);
    require(in.scalar<uint64_t>() == 0x314c415653535353ull && in.scalar<uint64_t>() == model, "reference identity");
    const uint64_t count = in.scalar<uint64_t>(); regional::Quality result;
    for (uint64_t o = 0; o < count; ++o) {
        const auto row = in.scalar<regional::ValidationObservation>();
        const auto& im = committed.images.at(row.image); const auto& cam = committed.cameras.at(im.camera_id);
        double e = reprojErrorAt(cam, im.pose, row.xy, local.points3D.at(row.point).xyz) / cam.pixel_scale;
        result.cost += e <= gate ? e * e : gate * (2 * e - gate); ++result.observations;
    }
    in.finish(); return result;
}

static void benchmark(const std::filesystem::path& path, const Reconstruction& local,
                      const Reconstruction& committed, uint64_t model) {
    auto start = std::chrono::steady_clock::now();
    const auto expected = referenceCost(path, local, committed, model, 3);
    double serial = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    double previous = -1;
    for (int threads : {1, 4, 0}) {
        start = std::chrono::steady_clock::now();
        auto actual = regional::validationCost(path, local, committed, model, 3, threads);
        double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        require(actual.observations == expected.observations && std::abs(actual.cost - expected.cost) <= std::max(1., expected.cost) * 1e-10,
                "bulk validation changed reference cost");
        require(previous < 0 || previous == actual.cost, "validation depends on thread scheduling"); previous = actual.cost;
        std::printf("BENCH observations %llu reference %.6fs threads %d bulk %.6fs speedup %.3fx cost %.17g\n",
                    (unsigned long long)actual.observations, serial, threads, elapsed, serial / elapsed, actual.cost);
    }
}

static void triangulationBenchmark(const Reconstruction& input, const std::set<uint64_t>& fixed) {
    auto serial = input; auto parallel = input;
    for (auto& p : serial.points3D) if (!fixed.count(p.first)) p.second.xyz.x += .01;
    parallel = serial; auto start = std::chrono::steady_clock::now();
    regional::triangulatePrivate(serial, fixed, 3, 1);
    double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    start = std::chrono::steady_clock::now(); regional::triangulatePrivate(parallel, fixed, 3, 0);
    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    for (const auto& p : serial.points3D)
        require(std::memcmp(&p.second.xyz, &parallel.points3D.at(p.first).xyz, sizeof(Vec3)) == 0, "parallel triangulation changed coordinates");
    std::printf("TRIANGULATION points %zu serial %.6fs parallel %.6fs speedup %.3fx coordinates bitwise identical\n",
                serial.points3D.size(), seconds, elapsed, seconds / elapsed);
}

static int test(int argc, char** argv) {
    namespace fs = std::filesystem;
    if (argc == 5 || argc == 7 || argc == 8) {
        auto local = model_store_detail::readModel(argv[2]); auto committed = model_store_detail::readModel(argv[3]);
        std::set<uint64_t> fixed;
        if (argc >= 7) {
            auto shared = regional::readSharedIndex(argv[5], std::stoull(argv[6]), 256u << 20);
            fixed = shared.inject(std::stoull(argv[4]), local);
        }
        benchmark(argv[1], local, committed, std::stoull(argv[4]));
        if (argc == 8) {
            auto end = local.points3D.begin(); size_t count = 0;
            while (end != local.points3D.end() && count++ < 100000) ++end;
            local.points3D.erase(end, local.points3D.end()); triangulationBenchmark(local, fixed);
        }
        return 0;
    }
    auto root = fs::temp_directory_path() / ("sfm-quality-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root); auto rec = fixture(false);
    regional::writeValidation(root / "validation.bin", rec, 7);
    auto q = regional::validationCost(root / "validation.bin", rec, rec, 7, 3);
    require(q.cost < 1e-10 && q.observations == 176, "validation omitted observations");
    auto poses = rec; poses.images.at(4).pose.t.x += .2;
    auto bad = regional::validationCost(root / "validation.bin", rec, poses, 7, 3);
    require(bad.cost > 1 && regional::qualityStop(bad, q, q.cost, .001, false) == 2, "writeback pose degradation accepted");
    poses = rec; poses.cameras.at(0).fx *= 1.2;
    bad = regional::validationCost(root / "validation.bin", rec, poses, 7, 3);
    require(bad.cost > 1 && regional::qualityStop(bad, q, q.cost, .001, false) == 2, "consensus intrinsic degradation accepted");
    auto fewer = q; --fewer.observations; bool rejected = false;
    try { regional::qualityStop(fewer, q, q.cost, .001, false); } catch (const std::runtime_error&) { rejected = true; }
    require(rejected, "different observation sets accepted");
    require(regional::qualityStop({99, 176}, {100, 176}, 100, .001, false) == 0, "improvement stopped too early");
    require(regional::qualityStop({99.95, 176}, {100, 176}, 100, .001, false) == 1, "convergence not recognized");
    regional::writeValidation(root / "validation.bin", rec, 7);
    regional::atomicModel(root / "model.bin", rec); regional::atomicModel(root / "model.bin", poses);
    require(model_store_detail::readModel(root / "model.bin").cameras.at(0).fx == poses.cameras.at(0).fx, "checkpoint replacement failed");
    fs::resize_file(root / "validation.bin", fs::file_size(root / "validation.bin") - 1); rejected = false;
    try { regional::validationCost(root / "validation.bin", rec, rec, 7, 3); } catch (const std::exception&) { rejected = true; }
    require(rejected, "truncated validation accepted");
    auto large = root / "large.bin";
    {
        model_store_detail::Writer out(large);
        out.scalar<uint64_t>(0x314c415653535353ull); out.scalar<uint64_t>(7); out.scalar<uint64_t>(1000003);
        for (uint64_t o = 0; o < 1000003; ++o) {
            const uint64_t point = (o / 5) % rec.points3D.size();
            auto row = rec.points3D.at(point).track.at(o % 2);
            auto xy = rec.images.at(row.image_id).points2D.at(row.point2D_idx); xy.x += double(o % 11) * .31;
            out.scalar(regional::ValidationObservation{point, row.image_id, 0, xy});
        }
        out.finish();
    }
    benchmark(large, rec, poses, 7);
    auto missing = rec; missing.points3D.erase(13); rejected = false;
    try { regional::validationCost(large, missing, poses, 7, 3); } catch (const std::exception&) { rejected = true; }
    require(rejected, "parallel missing-point exception lost");
    auto nonfinite = poses; nonfinite.cameras.at(0).fx = std::numeric_limits<double>::quiet_NaN(); rejected = false;
    try { regional::validationCost(large, rec, nonfinite, 7, 3); } catch (const std::exception&) { rejected = true; }
    require(rejected, "parallel nonfinite exception lost");
    std::atomic<bool> cancelled{true}; cancel::set_token(&cancelled); rejected = false;
    try { regional::validationCost(large, rec, poses, 7, 3); } catch (const Cancelled&) { rejected = true; }
    cancel::set_token(nullptr); require(rejected, "validation ignored cancellation");
    auto geometry = rec;
    for (uint64_t k = 88; k < 10003; ++k) geometry.points3D[k] = rec.points3D.at(k % 88);
    triangulationBenchmark(geometry, {0, 4, 8});
    geometry.points3D.at(100).track.at(0).image_id = UINT32_MAX; rejected = false;
    try { regional::triangulatePrivate(geometry, {}, 3); } catch (const std::exception&) { rejected = true; }
    require(rejected, "parallel triangulation lost worker failure");
    std::vector<std::vector<double>> groups;
    for (bool shuffle : {false, true}) {
        auto model = fixture(shuffle); regional::WindowGraph graph(model, 1u << 20); auto remaining = graph.remaining();
        std::vector<double> signature;
        while (!remaining.empty()) {
            auto window = graph.select(remaining, 4, 2); require(window.core.size() == 4, "connected core not filled");
            std::vector<double> x;
            for (uint32_t id : window.core) { x.push_back(cameraCenter(model.images.at(id).pose).x); remaining.erase(id); }
            std::sort(x.begin(), x.end()); require(x.back() - x.front() == 3, "window crossed spatially unrelated cameras");
            signature.insert(signature.end(), x.begin(), x.end());
            for (uint32_t id : window.support) require(!window.core.count(id), "support camera also active");
        }
        groups.push_back(signature);
    }
    require(groups[0] == groups[1], "windows depend on photo numbering");
    fs::remove_all(root); std::puts("PASS committed quality, observation consistency, checkpoint replacement, graph windows"); return 0;
}
int main(int argc, char** argv) { return sfmTestMain(argc, argv, test); }

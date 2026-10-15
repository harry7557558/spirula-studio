// `spirula lod` -- build, inspect, and benchmark hierarchical Level-of-Detail
// models (.slod) for large-scale Gaussian Splatting reconstructions.

#include "app/Tools.h"

#include "checkpoint/SplatMerge.h"
#include "checkpoint/SplatPly.h"
#include "lod/HierarchyBuilder.h"
#include "lod/HierarchyCache.h"
#include "backend/api/BackendRuntime.h"
#include "engine/Engine.h"
#include "engine/EngineState.h"
#include "lod/HierarchyFile.h"
#include "lod/HierarchyNode.h"
#include "lod/HierarchyRenderer.h"
#include "lod/HierarchyTraversal.h"
#include "lod/LodConfig.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

void print_usage() {
    const std::string prog = app::program_name();
    std::fprintf(stderr, "Spirula Studio -- Hierarchical LOD Tool\n\n");
    std::fprintf(stderr, "Usage:\n");
    std::fprintf(stderr, "    %s build <splat.ply | partition.json | run_dir> [options]\n", prog.c_str());
    std::fprintf(stderr, "    %s info <file.slod>\n", prog.c_str());
    std::fprintf(stderr, "    %s export <file.slod> -o <output.ply>\n", prog.c_str());
    std::fprintf(stderr, "    %s bench <file.slod> [options]\n\n", prog.c_str());
    std::fprintf(stderr, "Commands:\n");
    std::fprintf(stderr, "    build        Construct a hierarchical LOD model (.slod) from splats\n");
    std::fprintf(stderr, "    info         Inspect hierarchy topology, bounds, and chunk metadata\n");
    std::fprintf(stderr, "    export       Extract the full-detail model back to standard PLY format\n");
    std::fprintf(stderr, "    bench        Benchmark traversal and cut selection across camera paths\n\n");
    std::fprintf(stderr, "Build Options:\n");
    std::fprintf(stderr, "    -o, --output <file.slod>   Output .slod file path (default: <input>.slod)\n");
    std::fprintf(stderr, "    --leaf-size <n>            Maximum Gaussians per leaf node (default: 32)\n");
    std::fprintf(stderr, "    --parent-gaussians <n>     Summary Gaussians per internal node (default: 8)\n");
    std::fprintf(stderr, "    --sh-decay <factor>        SH band attenuation factor (default: 0.5)\n");
    std::fprintf(stderr, "    --runs <dir>               Runs folder for partition.json merge\n");
    std::fprintf(stderr, "    --verbose                  Verbose logging during construction\n\n");
    std::fprintf(stderr, "Benchmark Options:\n");
    std::fprintf(stderr, "    --threshold <pixels>       Screen-space error threshold (default: 1.0)\n");
    std::fprintf(stderr, "    --frames <n>               Number of camera positions to benchmark (default: 50)\n");
    std::fprintf(stderr, "    --budget <mb>              VRAM budget for cache simulation (default: 0 = unlimited)\n");
}

int run_build(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 2;
    }

    std::string input_path;
    std::string output_path;
    std::string runs_dir;
    spirula::LodBuildConfig cfg;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s requires an argument\n", a.c_str());
                std::exit(2);
            }
            return argv[++i];
        };

        if (a == "--help" || a == "-h") {
            print_usage();
            return 0;
        } else if (a == "-o" || a == "--output") {
            output_path = next();
        } else if (a == "--leaf-size") {
            cfg.max_leaf_size = std::atoi(next());
        } else if (a == "--parent-gaussians") {
            cfg.max_parent_gaussians = std::atoi(next());
        } else if (a == "--sh-decay") {
            cfg.sh_band_decay = (float)std::atof(next());
        } else if (a == "--runs") {
            runs_dir = next();
        } else if (a == "--verbose") {
            cfg.verbose = true;
        } else if (!a.empty() && a[0] == '-') {
            std::fprintf(stderr, "unknown option: %s\n", a.c_str());
            return 2;
        } else if (input_path.empty()) {
            input_path = a;
        } else {
            std::fprintf(stderr, "unexpected positional argument: %s\n", a.c_str());
            return 2;
        }
    }

    if (input_path.empty()) {
        std::fprintf(stderr, "error: input path required\n");
        return 2;
    }

    // Determine default output path if not specified
    if (output_path.empty()) {
        fs::path p(input_path);
        if (p.extension() == ".json" || p.extension() == ".ply") {
            output_path = p.replace_extension(".slod").string();
        } else {
            output_path = (p / "model.slod").string();
        }
    }

    spirula::SplatCloud cloud;

    // Check if input is a partition.json
    if (fs::is_regular_file(input_path) && fs::path(input_path).extension() == ".json") {
        std::printf("Detected partition manifest: %s\n", input_path.c_str());
        spirula::ScenePartition partition = spirula::read_partition(input_path);
        if (partition.empty()) {
            std::fprintf(stderr, "error: partition file is empty or invalid\n");
            return 1;
        }

        std::string search_dir = runs_dir;
        if (search_dir.empty()) {
            search_dir = (fs::path(input_path).parent_path() / "outputs").string();
            if (!fs::exists(search_dir)) {
                search_dir = fs::path(input_path).parent_path().string();
            }
        }

        std::printf("Locating partition runs in: %s\n", search_dir.c_str());
        std::vector<std::string> run_paths = spirula::find_partition_runs(
            search_dir, input_path, partition.num_parts);

        spirula::MergeStats stats;
        std::printf("Merging %d partition models...\n", partition.num_parts);
        cloud = spirula::merge_partition_splats(partition, run_paths, stats, [](const std::string& msg) {
            std::printf("  %s\n", msg.c_str());
        });
        std::printf("Successfully merged %lld total splats (SH degree %d)\n",
                    (long long)cloud.num, cloud.sh_degree);
    } else {
        // Direct PLY or run folder
        auto [ply_path, run_dir] = spirula::find_splat_ply(input_path);
        std::printf("Reading splat model from: %s\n", ply_path.c_str());
        cloud = spirula::read_splat_ply(ply_path, /*want_sh=*/true);
        std::printf("Loaded %lld splats (SH degree %d, SB lobes %d)\n",
                    (long long)cloud.num, cloud.sh_degree, cloud.sb_lobes);
    }

    if (cloud.num <= 0) {
        std::fprintf(stderr, "error: input cloud has 0 splats\n");
        return 1;
    }

    auto t_start = std::chrono::steady_clock::now();

    auto log_fn = [](const std::string& msg) {
        std::printf("[LOD Build] %s\n", msg.c_str());
    };

    spirula::GaussianHierarchy hier = spirula::build_gaussian_hierarchy(cloud, cfg, log_fn);

    std::printf("Writing hierarchy to .slod format: %s\n", output_path.c_str());
    spirula::write_slod(hier, output_path);

    auto t_end = std::chrono::steady_clock::now();
    double sec = std::chrono::duration<double>(t_end - t_start).count();

    uintmax_t file_bytes = fs::file_size(output_path);
    std::printf("Hierarchy build complete in %.2f s\n", sec);
    std::printf("  Nodes:      %zu (max depth %d)\n", hier.nodes.size(), hier.max_depth);
    std::printf("  Leaves:     %lld\n", (long long)hier.num_splats);
    std::printf("  Summaries:  %lld\n", (long long)hier.total_summary_gaussians());
    std::printf("  File size:  %.2f MB\n", (double)file_bytes / (1024.0 * 1024.0));

    return 0;
}

int run_info(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s info <file.slod>\n", app::program_name().c_str());
        return 2;
    }

    std::string path = argv[1];
    std::string err = spirula::validate_slod(path);
    if (!err.empty()) {
        std::fprintf(stderr, "Validation error for %s: %s\n", path.c_str(), err.c_str());
        return 1;
    }

    spirula::GaussianHierarchy hier = spirula::read_slod(path, /*metadata_only=*/true);
    uintmax_t file_bytes = fs::file_size(path);

    std::printf("SLOD File Info: %s\n", path.c_str());
    std::printf("  Validation:         PASSED (valid .slod v1 container)\n");
    std::printf("  Total Splats:       %lld\n", (long long)hier.num_splats);
    std::printf("  Tree Depth:         %d\n", hier.max_depth);
    std::printf("  Total Nodes:        %zu\n", hier.nodes.size());
    std::printf("  Root Node:          %d\n", hier.root);
    std::printf("  SH Degree:          %d\n", hier.sh_degree);
    std::printf("  SB Lobes:           %d\n", hier.sb_lobes);
    std::printf("  Total Summaries:    %lld\n", (long long)hier.total_summary_gaussians());

    if (hier.root >= 0 && hier.root < (int32_t)hier.nodes.size()) {
        const auto& r = hier.nodes[(size_t)hier.root];
        std::printf("  Root Bounding Box:  [%.2f, %.2f, %.2f] to [%.2f, %.2f, %.2f]\n",
                    r.bounds.lo[0], r.bounds.lo[1], r.bounds.lo[2],
                    r.bounds.hi[0], r.bounds.hi[1], r.bounds.hi[2]);
        std::printf("  Scene Extents:      dx=%.2f, dy=%.2f, dz=%.2f (diag=%.2f)\n",
                    r.bounds.extent(0), r.bounds.extent(1), r.bounds.extent(2),
                    r.bounds.diagonal());
        std::printf("  Root Error Radius:  %.3f\n", r.error_radius);
    }
    std::printf("  Total File Size:    %.2f MB (%ju bytes)\n",
                (double)file_bytes / (1024.0 * 1024.0), (uintmax_t)file_bytes);

    return 0;
}

int run_export(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s export <file.slod> -o <output.ply>\n", app::program_name().c_str());
        return 2;
    }

    std::string input_path;
    std::string output_path;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-o" || a == "--output") {
            if (i + 1 < argc) output_path = argv[++i];
        } else if (!a.empty() && a[0] != '-') {
            input_path = a;
        }
    }

    if (input_path.empty() || output_path.empty()) {
        std::fprintf(stderr, "error: input .slod and output .ply paths required\n");
        return 2;
    }

    std::printf("Reading .slod model: %s\n", input_path.c_str());
    spirula::GaussianHierarchy hier = spirula::read_slod(input_path, /*metadata_only=*/false);

    spirula::SplatCloud cloud;
    cloud.num = hier.num_splats;
    cloud.sh_degree = hier.sh_degree;
    cloud.sb_lobes = hier.sb_lobes;
    cloud.means = std::move(hier.means);
    cloud.quats = std::move(hier.quats);
    cloud.scales = std::move(hier.scales);
    cloud.opacities = std::move(hier.opacities);
    cloud.features_dc = std::move(hier.features_dc);
    cloud.features_sh = std::move(hier.features_sh);

    std::printf("Exporting %lld full-detail Gaussians to: %s\n", (long long)cloud.num, output_path.c_str());
    spirula::write_splat_ply(cloud, output_path);
    std::printf("Wrote %s\n", output_path.c_str());

    return 0;
}

static TorchTensorView bench_ttv(const void* p, std::vector<int64_t> shape) {
    return std::make_tuple((uint64_t)p, (uint32_t)4, std::move(shape));
}

int run_bench(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s bench <file.slod> [options]\n", app::program_name().c_str());
        std::fprintf(stderr, "Options:\n");
        std::fprintf(stderr, "  --threshold <px>    Screen error threshold (default: 1.0)\n");
        std::fprintf(stderr, "  --frames <N>        Benchmark frames count (default: 50)\n");
        std::fprintf(stderr, "  --budget <MB>       VRAM budget in MB (default: 0 = unconstrained)\n");
        std::fprintf(stderr, "  --gpu               Execute actual GPU rasterization & uploads\n");
        std::fprintf(stderr, "  --compare-full      Run identical orbit with unpartitioned full resolution\n");
        return 2;
    }

    std::string path;
    float quality_threshold = 1.0f;
    int num_frames = 50;
    int64_t budget_mb = 0;
    bool run_gpu = false;
    bool compare_full = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--threshold" && i + 1 < argc) {
            quality_threshold = (float)std::atof(argv[++i]);
        } else if (a == "--frames" && i + 1 < argc) {
            num_frames = std::atoi(argv[++i]);
        } else if (a == "--budget" && i + 1 < argc) {
            budget_mb = std::atoll(argv[++i]);
        } else if (a == "--gpu") {
            run_gpu = true;
        } else if (a == "--compare-full") {
            compare_full = true;
            run_gpu = true;
        } else if (!a.empty() && a[0] != '-') {
            path = a;
        }
    }

    if (path.empty()) {
        std::fprintf(stderr, "error: file path required\n");
        return 2;
    }

    std::printf("Loading %s for LOD benchmark...\n", path.c_str());
    spirula::GaussianHierarchy hier = spirula::read_slod(path, /*metadata_only=*/false);

    const auto& root_box = hier.nodes[(size_t)hier.root].bounds;
    float center[3] = {root_box.center(0), root_box.center(1), root_box.center(2)};
    float radius = root_box.diagonal() * 0.75f;

    spirula::LodRenderConfig cfg;
    cfg.enabled = true;
    cfg.quality_threshold = quality_threshold;
    cfg.vram_budget_mb = budget_mb;

    std::printf("\n============================================================\n");
    std::printf("Spirula Studio LOD Benchmark: %s\n", path.c_str());
    std::printf("Total Scene Splats:      %lld\n", (long long)hier.num_splats);
    std::printf("Quality Threshold:       %.2f pixels\n", quality_threshold);
    std::printf("Benchmark Frames:        %d orbit viewpoints\n", num_frames);
    std::printf("Backend Mode:            %s\n", run_gpu ? "Real GPU Rasterization" : "CPU Traversal Only");
    if (budget_mb > 0) std::printf("VRAM Budget Envelope:    %lld MB\n", (long long)budget_mb);
    std::printf("============================================================\n\n");

    const int64_t coeffs = hier.coeffs();
    const int64_t bytes_per_splat = (int64_t)(12 + 16 + 12 + 4 + 12 + coeffs * 12);

    constexpr int W = 1280, H = 720;
    const std::vector<float> dist(kCameraDistortionParams, 0.0f);
    const std::vector<float> intr = {1000.0f, 1000.0f, (float)W * 0.5f, (float)H * 0.5f};

    // Precompute viewpoints
    std::vector<spirula::LodViewParams> viewpoints((size_t)num_frames);
    std::vector<std::vector<float>> viewmats((size_t)num_frames, std::vector<float>(16));

    for (int f = 0; f < num_frames; f++) {
        float angle = (float)f / (float)num_frames * 2.0f * 3.14159265f;
        auto& view = viewpoints[(size_t)f];
        view.width = W; view.height = H;
        view.fx = 1000.0f; view.fy = 1000.0f;
        view.cx = (float)W * 0.5f; view.cy = (float)H * 0.5f;

        view.cam_pos[0] = center[0] + radius * std::cos(angle);
        view.cam_pos[1] = center[1] + radius * 0.3f;
        view.cam_pos[2] = center[2] + radius * std::sin(angle);

        float dir[3] = {center[0] - view.cam_pos[0], center[1] - view.cam_pos[1], center[2] - view.cam_pos[2]};
        float len = std::sqrt(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]);
        if (len > 0) {
            view.cam_dir[0] = dir[0] / len;
            view.cam_dir[1] = dir[1] / len;
            view.cam_dir[2] = dir[2] / len;
        }

        float up_ref[3] = {0.0f, 1.0f, 0.0f};
        float right[3] = {
            view.cam_dir[1] * up_ref[2] - view.cam_dir[2] * up_ref[1],
            view.cam_dir[2] * up_ref[0] - view.cam_dir[0] * up_ref[2],
            view.cam_dir[0] * up_ref[1] - view.cam_dir[1] * up_ref[0]
        };
        float rlen = std::sqrt(right[0]*right[0] + right[1]*right[1] + right[2]*right[2]);
        if (rlen > 1e-4f) {
            right[0] /= rlen; right[1] /= rlen; right[2] /= rlen;
        } else {
            right[0] = 1.0f; right[1] = 0.0f; right[2] = 0.0f;
        }
        float up[3] = {
            right[1] * view.cam_dir[2] - right[2] * view.cam_dir[1],
            right[2] * view.cam_dir[0] - right[0] * view.cam_dir[2],
            right[0] * view.cam_dir[1] - right[1] * view.cam_dir[0]
        };
        for (int i = 0; i < 3; i++) {
            view.cam_right[i] = right[i];
            view.cam_up[i] = up[i];
        }

        float* V = viewmats[(size_t)f].data();
        V[0] = right[0]; V[1] = right[1]; V[2] = right[2];
        V[3] = -(right[0]*view.cam_pos[0] + right[1]*view.cam_pos[1] + right[2]*view.cam_pos[2]);
        V[4] = up[0];    V[5] = up[1];    V[6] = up[2];
        V[7] = -(up[0]*view.cam_pos[0] + up[1]*view.cam_pos[1] + up[2]*view.cam_pos[2]);
        V[8] = view.cam_dir[0]; V[9] = view.cam_dir[1]; V[10] = view.cam_dir[2];
        V[11] = -(view.cam_dir[0]*view.cam_pos[0] + view.cam_dir[1]*view.cam_pos[1] + view.cam_dir[2]*view.cam_pos[2]);
        V[12] = 0.0f; V[13] = 0.0f; V[14] = 0.0f; V[15] = 1.0f;

        float fov_rad = 60.0f * 3.14159265f / 180.0f;
        float tan_half = std::tan(fov_rad * 0.5f);
        float a = 1.0f / (1.7778f * tan_half);
        float b = 1.0f / tan_half;
        float zn = 0.1f, zf = 1000.0f;
        float P[16] = {
            a,    0.0f, 0.0f, 0.0f,
            0.0f, b,    0.0f, 0.0f,
            0.0f, 0.0f, (zf + zn) / (zf - zn), -(2.0f * zf * zn) / (zf - zn),
            0.0f, 0.0f, 1.0f, 0.0f
        };
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                float sum = 0.0f;
                for (int k = 0; k < 4; k++) sum += P[r * 4 + k] * V[k * 4 + c];
                view.view_proj[r * 4 + c] = sum;
            }
        }
    }

    // A. Full resolution reference benchmark (if requested)
    std::vector<double> full_frame_times;
    double full_peak_vram_mb = 0.0;
    if (compare_full) {
        std::printf("Running Full-Resolution Baseline on GPU...\n");
        engine_reset();
        set_data_3dgs(hier.num_splats,
                      bench_ttv(hier.means.data(), {hier.num_splats, 3}),
                      bench_ttv(hier.quats.data(), {hier.num_splats, 4}),
                      bench_ttv(hier.scales.data(), {hier.num_splats, 3}),
                      bench_ttv(hier.opacities.data(), {hier.num_splats, 1}),
                      bench_ttv(hier.features_dc.data(), {hier.num_splats, 3}),
                      bench_ttv(hier.features_sh.data(), {hier.num_splats, coeffs, 3}),
                      hier.sb_lobes);
        full_peak_vram_mb = (double)(hier.num_splats * bytes_per_splat) / (1024.0 * 1024.0);

        for (int f = 0; f < num_frames; f++) {
            auto t0 = std::chrono::steady_clock::now();
            set_camera_params(W, H, "PINHOLE", "NONE",
                              bench_ttv(viewmats[(size_t)f].data(), {1, 4, 4}),
                              bench_ttv(intr.data(), {1, 4}),
                              bench_ttv(dist.data(), {1, kCameraDistortionParams}));
            forward_3dgs("3dgs", 0, false, false, 0);
            backend::device_synchronize();
            auto t1 = std::chrono::steady_clock::now();
            full_frame_times.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
        }
    }

    // B. Hierarchical LOD benchmark
    std::vector<double> lod_total_times;
    std::vector<double> lod_traversal_times;
    std::vector<double> lod_upload_times;
    std::vector<int64_t> lod_rendered_splats;
    int64_t lod_upload_bytes = 0;
    int lod_upload_events = 0;
    double lod_peak_vram_mb = 0.0;

    if (run_gpu) {
        engine_reset();
        auto hier_ptr = std::make_shared<spirula::GaussianHierarchy>(std::move(hier));
        spirula::HierarchyRenderer renderer(hier_ptr);
        renderer.reset_history();

        for (int f = 0; f < num_frames; f++) {
            auto t_f0 = std::chrono::steady_clock::now();
            auto stats = renderer.update_and_upload(viewpoints[(size_t)f], cfg);

            set_camera_params(W, H, "PINHOLE", "NONE",
                              bench_ttv(viewmats[(size_t)f].data(), {1, 4, 4}),
                              bench_ttv(intr.data(), {1, 4}),
                              bench_ttv(dist.data(), {1, kCameraDistortionParams}));
            forward_3dgs("3dgs", 0, false, false, 0);
            backend::device_synchronize();
            auto t_f1 = std::chrono::steady_clock::now();

            double f_ms = std::chrono::duration<double, std::milli>(t_f1 - t_f0).count();
            lod_total_times.push_back(f_ms);
            lod_traversal_times.push_back(stats.traversal_time_ms);
            lod_upload_times.push_back(stats.upload_time_ms);
            lod_rendered_splats.push_back(stats.total_rendered_splats);
            lod_upload_bytes += stats.upload_bytes;
            if (stats.gpu_buffers_updated) lod_upload_events++;

            double vram_mb = (double)(engine_get_cur_num_splats() * bytes_per_splat) / (1024.0 * 1024.0);
            lod_peak_vram_mb = std::max(lod_peak_vram_mb, vram_mb);
        }
    } else {
        spirula::HierarchyTraversal traversal;
        for (int f = 0; f < num_frames; f++) {
            auto t0 = std::chrono::steady_clock::now();
            spirula::LodCutResult cut = traversal.select_cut(hier, viewpoints[(size_t)f], cfg);
            auto t1 = std::chrono::steady_clock::now();

            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            lod_traversal_times.push_back(ms);
            lod_rendered_splats.push_back(cut.total_rendered_splats);
        }
    }

    // Compute percentiles helper
    auto calc_percentile = [](std::vector<double> v, double p) -> double {
        if (v.empty()) return 0.0;
        std::sort(v.begin(), v.end());
        size_t idx = (size_t)std::clamp<double>(p * (double)v.size(), 0.0, (double)v.size() - 1);
        return v[idx];
    };

    auto calc_avg = [](const std::vector<double>& v) -> double {
        if (v.empty()) return 0.0;
        double s = 0; for (double x : v) s += x;
        return s / (double)v.size();
    };

    double avg_trav_ms = calc_avg(lod_traversal_times);
    double p95_trav_ms = calc_percentile(lod_traversal_times, 0.95);
    double avg_splats = 0.0;
    for (int64_t s : lod_rendered_splats) avg_splats += (double)s;
    avg_splats /= (double)lod_rendered_splats.size();

    int64_t scene_splats = hier.num_splats;
    if (scene_splats == 0 && !lod_rendered_splats.empty()) {
        scene_splats = (int64_t)avg_splats;
    }
    double reduction = (scene_splats > 0) ? (1.0 - avg_splats / (double)scene_splats) * 100.0 : 0.0;

    std::printf("\nBenchmark Results:\n");
    std::printf("CPU Traversal Performance:\n");
    std::printf("  Average Traversal Time:  %.3f ms (p95: %.3f ms)\n", avg_trav_ms, p95_trav_ms);
    std::printf("  Traversal Framerate:     %.1f FPS\n", 1000.0 / std::max(0.001, avg_trav_ms));
    std::printf("  Average Rendered Splats: %.0f / %lld (%.1f%% reduction)\n",
                avg_splats, (long long)scene_splats, reduction);

    if (run_gpu) {
        double avg_frame_ms = calc_avg(lod_total_times);
        double p95_frame_ms = calc_percentile(lod_total_times, 0.95);
        double p99_frame_ms = calc_percentile(lod_total_times, 0.99);
        double avg_upload_ms = calc_avg(lod_upload_times);
        double upload_freq_pct = (double)lod_upload_events / (double)num_frames * 100.0;
        double upload_mb = (double)lod_upload_bytes / (1024.0 * 1024.0);

        std::printf("\nInteractive GPU Rendering Performance:\n");
        std::printf("  Average Frame Time:      %.3f ms (%.1f FPS)\n", avg_frame_ms, 1000.0 / avg_frame_ms);
        std::printf("  p95 Frame Time:          %.3f ms\n", p95_frame_ms);
        std::printf("  p99 Frame Time:          %.3f ms\n", p99_frame_ms);
        std::printf("  Average Upload Time:     %.3f ms\n", avg_upload_ms);
        std::printf("  GPU Peak VRAM:           %.2f MB\n", lod_peak_vram_mb);
        std::printf("  GPU Upload Events:       %d / %d frames (%.1f%%)\n",
                    lod_upload_events, num_frames, upload_freq_pct);
        std::printf("  Total Uploaded Data:     %.2f MB (%.2f MB/frame active)\n",
                    upload_mb, lod_upload_events > 0 ? upload_mb / lod_upload_events : 0.0);

        if (compare_full) {
            double full_avg_ms = calc_avg(full_frame_times);
            double full_p95_ms = calc_percentile(full_frame_times, 0.95);
            double full_p99_ms = calc_percentile(full_frame_times, 0.99);
            double speedup = full_avg_ms / std::max(0.001, avg_frame_ms);
            double vram_saving = (1.0 - lod_peak_vram_mb / std::max(0.001, full_peak_vram_mb)) * 100.0;

            std::printf("\nComparison: Full Resolution vs Hierarchical LOD:\n");
            std::printf("  %-24s %-18s %-18s %s\n", "Metric", "Full Resolution", "Hierarchical LOD", "Delta");
            std::printf("  %-24s %7.2f ms          %7.2f ms        %+.1fx speedup\n",
                        "Average Frame Time", full_avg_ms, avg_frame_ms, speedup);
            std::printf("  %-24s %7.1f FPS         %7.1f FPS       %+.1f%%\n",
                        "Average Framerate", 1000.0 / full_avg_ms, 1000.0 / avg_frame_ms, (speedup - 1.0) * 100.0);
            std::printf("  %-24s %7.2f ms          %7.2f ms\n",
                        "p95 Frame Time", full_p95_ms, p95_frame_ms);
            std::printf("  %-24s %7.2f ms          %7.2f ms\n",
                        "p99 Frame Time", full_p99_ms, p99_frame_ms);
            std::printf("  %-24s %7.2f MB          %7.2f MB        -%.1f%% saved\n",
                        "Peak VRAM Allocation", full_peak_vram_mb, lod_peak_vram_mb, vram_saving);
            std::printf("  %-24s %7lld             %7.0f           -%.1f%%\n",
                        "Rendered Splats", (long long)scene_splats, avg_splats, reduction);
        }
    }

    return 0;
}

}  // namespace

int spirula_lod_main(int argc, char** argv) {
    app::set_program_name(argc ? argv[0] : nullptr, "spirula lod");

    if (argc < 2) {
        print_usage();
        return 2;
    }

    std::string cmd = argv[1];
    if (cmd == "build") {
        return run_build(argc - 1, argv + 1);
    } else if (cmd == "info") {
        return run_info(argc - 1, argv + 1);
    } else if (cmd == "export") {
        return run_export(argc - 1, argv + 1);
    } else if (cmd == "bench") {
        return run_bench(argc - 1, argv + 1);
    } else if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        print_usage();
        return 0;
    } else {
        std::fprintf(stderr, "unknown lod subcommand: %s\n\n", cmd.c_str());
        print_usage();
        return 2;
    }
}


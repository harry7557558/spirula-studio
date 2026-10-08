#include "dense/Reconstruction.h"
#include "dense/ConfigFields.h"
#include "data/SparseEdit.h"
#include "core/CameraModel.h"
#include "core/Sha256.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>

using namespace spirula::dense;

int main() {
    try {
        const auto root = std::filesystem::temp_directory_path() /
            ("spirula-dense-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        DenseConfig config;
        config.preset = "custom"; config.stride = 1;
        config.match.low_width = config.match.low_height = 32;
        config.match.high_width = config.match.high_height = 0;
        config.geometry.min_source_images = 3;
        config.voxel_size = 0.02;
        std::vector<View> views(4);
        ViewPixels pixels;
        pixels.width = pixels.height = 32;
        pixels.rgb.resize(32 * 32 * 3, 0.5f);
        pixels.keep.resize(32 * 32, 1);
        for (int y = 0; y < 32; ++y) for (int x = 0; x < 8; ++x) pixels.keep[y * 32 + x] = 0;
        for (int i = 0; i < 4; ++i) {
            views[i].source_image = i;
            views[i].center = {i * 0.25, 0, 0};
            views[i].camera.model = (int)CameraModelType::PINHOLE;
            views[i].camera.width = views[i].camera.height = 32;
            views[i].camera.fx = views[i].camera.fy = 32;
            views[i].camera.cx = views[i].camera.cy = 16;
        }
        auto prediction = [](double disparity) {
            spirula::roma::Prediction p;
            p.width = p.height = 32;
            p.warp.resize(32 * 32 * 2); p.overlap.resize(32 * 32, 1); p.precision.resize(32 * 32 * 3);
            for (int y = 0; y < 32; ++y) for (int x = 0; x < 32; ++x) {
                const size_t i = y * 32 + x;
                p.warp[i * 2] = (float)(2 * (x + 0.5 + disparity) / 32 - 1);
                p.warp[i * 2 + 1] = (float)(2 * (y + 0.5) / 32 - 1);
                p.precision[i * 3] = p.precision[i * 3 + 2] = 1;
            }
            return p;
        };
        const auto preview_dir = (root / "progress").string();
        Reconstruction reconstruction((root / "observations").string(), views, config, nullptr, preview_dir);
        for (uint32_t a = 0; a < 4; ++a) for (uint32_t b = a + 1; b < 4; ++b) {
            const double disparity = -(double)(b - a) * 2.0;
            reconstruction.add_pair(a, b, pixels, pixels, {prediction(disparity), prediction(-disparity)});
        }
        if (reconstruction.checkpoint().points != 0) throw std::runtime_error("raw pair points reached filtered preview");
        for (uint32_t v = 0; v < views.size(); ++v) reconstruction.complete_reference(v);
        const auto provisional = reconstruction.checkpoint();
        if (provisional.provisional || !provisional.filtered || !provisional.error.empty() || !provisional.points ||
            std::filesystem::file_size(root / "progress" / provisional.file) != provisional.points * 15)
            throw std::runtime_error("provisional checkpoint count or flush mismatch");
        ParsedDataset dataset;
        dataset.center = {4000000.123456789, 5, -2};
        dataset.raw_to_file = {0,-1,0,10, 1,0,0,20, 0,0,1,30, 0,0,0,1};
        const auto path = (root / "plane.ply").string();
        const auto stats = reconstruction.finish(path, dataset);
        const auto final = reconstruction.checkpoint();
        if (final.provisional || !final.error.empty() || final.points != stats.exported || final.file == provisional.file ||
            std::filesystem::file_size(root / "progress" / final.file) != stats.exported * 15 ||
            !std::filesystem::exists(root / "progress" / provisional.file))
            throw std::runtime_error("final checkpoint or immutable provisional prefix mismatch");
        if (stats.exported < 400 || stats.masked == 0 || stats.refined < stats.exported) throw std::runtime_error("unexpected coverage or mask statistics");
        const auto cloud = read_ply_points(path);
        if ((uint64_t)cloud.num() != stats.exported) throw std::runtime_error("streaming PLY count mismatch");
        for (int64_t i = 0; i < cloud.num(); ++i) {
            if (std::fabs(cloud.xyz[i * 3 + 2] - 32) > 1e-7 || cloud.xyz[i * 3 + 1] < 4000019 ||
                cloud.rgb[i * 3] != 128) throw std::runtime_error("source transform, precision, plane or color mismatch");
        }
        DenseConfig decoded;
        read_config(decoded, json_parse(config_json(config)));
        if (config_json(decoded) != config_json(config)) throw std::runtime_error("dense settings roundtrip mismatch");
        config.remove_outliers = true; config.cpu_workers = 2; config.point_limit = 100;
        Reconstruction filtered((root / "filtered").string(), views, config);
        for (uint32_t a = 0; a < 4; ++a) for (uint32_t b = a + 1; b < 4; ++b) {
            const double disparity = -(double)(b - a) * 2.0;
            filtered.add_pair(a, b, pixels, pixels, {prediction(disparity), prediction(-disparity)});
        }
        const auto filtered_stats = filtered.finish((root / "filtered.ply").string(), dataset);
        if (filtered_stats.exported != 100 || filtered_stats.fused != stats.fused)
            throw std::runtime_error("outlier filtering or point limit failed");
        bool finished_twice = false;
        try { filtered.finish((root / "twice.ply").string(), dataset); }
        catch (const std::exception&) { finished_twice = true; }
        if (!finished_twice) throw std::runtime_error("repeated finish was accepted");
        std::atomic<bool> stop{true};
        Reconstruction cancelled((root / "cancelled").string(), views, config, &stop);
        bool refused = false;
        try { cancelled.add_pair(0, 1, pixels, pixels, {prediction(-2), prediction(2)}); }
        catch (const std::exception&) { refused = true; }
        if (!refused) throw std::runtime_error("cancellation was ignored");
        auto growing_views = std::vector<View>(84, views[0]);
        for (size_t i = 0; i < growing_views.size(); ++i) growing_views[i].source_image = (int64_t)i;
        growing_views[82].center = views[1].center; growing_views[83].center = views[2].center;
        Reconstruction growing((root / "growing").string(), growing_views, config, nullptr, preview_dir);
        uint64_t previous = 0;
        for (uint32_t v = 0; v < 82; ++v) {
            growing.add_pair(v, 82, pixels, pixels, {prediction(-2), prediction(2)});
            growing.add_pair(v, 83, pixels, pixels, {prediction(-4), prediction(4)});
            growing.complete_reference(v);
            const auto current = growing.checkpoint();
            if (current.points <= previous || !current.error.empty()) throw std::runtime_error("live checkpoint stopped growing");
            previous = current.points;
        }
        if (previous <= 50000) throw std::runtime_error("filtered checkpoint did not grow past 50k");
        if (std::filesystem::file_size(root / "progress" / growing.checkpoint().file) != previous * 15)
            throw std::runtime_error("uncapped point checkpoint size mismatch");
        auto source_config = config; source_config.matching_space = "source"; source_config.samples_per_reference = 100;
        source_config.remove_outliers = false; source_config.point_limit = 0;
        auto source_views = views; for (auto& v : source_views) v.source_camera = true;
        Reconstruction source((root / "source").string(),source_views,source_config,nullptr,preview_dir);
        source.add_pair(0,1,pixels,pixels,{prediction(-2),prediction(2)});
        source.add_pair(0,2,pixels,pixels,{prediction(-4),prediction(4)});
        source.complete_reference(0);
        const auto source_preview = source.checkpoint();
        if (source_preview.points < 50 || source_preview.points > 100 || !source_preview.filtered)
            throw std::runtime_error("source reference sampling or support failed");
        auto all_settings = source_config; all_settings.samples_per_reference = 0;
        Reconstruction all_samples((root / "all-source-samples").string(), source_views, all_settings, nullptr, preview_dir);
        all_samples.add_pair(0,1,pixels,pixels,{prediction(-2),prediction(2)});
        all_samples.add_pair(0,2,pixels,pixels,{prediction(-4),prediction(4)});
        all_samples.complete_reference(0);
        if (all_samples.checkpoint().points <= source_preview.points)
            throw std::runtime_error("zero source sampling count did not keep all eligible samples");
        auto low_overlap = prediction(-2);
        std::fill(low_overlap.overlap.begin(), low_overlap.overlap.end(), 0.49f);
        Reconstruction rejected_confidence((root / "source-confidence").string(), source_views, source_config, nullptr, preview_dir);
        rejected_confidence.add_pair(0,1,pixels,pixels,{low_overlap,prediction(2)});
        rejected_confidence.add_pair(0,2,pixels,pixels,{prediction(-4),prediction(4)});
        rejected_confidence.complete_reference(0);
        if (rejected_confidence.checkpoint().points)
            throw std::runtime_error("rejected confidence was clamped into source sample support");
        auto source_settings = source_config; source_settings.sampling_seed = 9007199254740991ull;
        DenseConfig source_roundtrip; read_config(source_roundtrip,json_parse(config_json(source_settings)));
        if (config_json(source_roundtrip) != config_json(source_settings)) throw std::runtime_error("source config roundtrip failed");
        auto reference_run = [&](const char* name, DenseConfig settings) {
            Reconstruction run((root / name).string(),source_views,settings);
            for (uint32_t a = 0; a < 4; ++a) for (uint32_t b = 0; b < 4; ++b) if (a != b)
                run.add_pair(a,b,pixels,pixels,{prediction(-((double)b-a)*2),{}});
            for (uint32_t a = 0; a < 4; ++a) run.complete_reference(a,true);
            const auto path = (root / (std::string(name) + ".ply")).string();
            const auto statistics = run.finish(path,dataset);
            return std::make_pair(statistics,spirula::sha256_file(path));
        };
        auto serial_settings = source_config; serial_settings.cpu_workers = 1; serial_settings.match.bidirectional = false;
        const auto serial_reference = reference_run("serial-reference",serial_settings);
        auto parallel_settings = serial_settings; parallel_settings.cpu_workers = 2;
        const auto parallel_reference = reference_run("parallel-reference",parallel_settings);
        if (serial_reference.first.min_reference_support < 3 ||
            serial_reference.first.max_reference_reprojection_error > serial_settings.source_reprojection_error ||
            serial_reference.first.min_reference_support != parallel_reference.first.min_reference_support ||
            serial_reference.first.max_reference_reprojection_error != parallel_reference.first.max_reference_reprojection_error ||
            serial_reference.first.reference_reprojection_histogram != parallel_reference.first.reference_reprojection_histogram)
            throw std::runtime_error("filtered preview quality statistics violate support or reprojection requirements");
        if (serial_reference.second != parallel_reference.second ||
            serial_reference.first.refined != parallel_reference.first.refined ||
            parallel_reference.first.refinement_workers > 2)
            throw std::runtime_error("parallel reference processing changed deterministic output");
        auto spill_settings = serial_settings; spill_settings.image_cache_bytes = 32768;
        const auto spill_reference = reference_run("spilled-reference",spill_settings);
        if (serial_reference.second != spill_reference.second || serial_reference.first.refined != spill_reference.first.refined)
            throw std::runtime_error("disk-spilled sampling or reference processing changed output");
        spill_settings.image_cache_bytes = 4096;
        const auto support_spill = reference_run("spilled-support",spill_settings);
        if (serial_reference.second != support_spill.second || serial_reference.first.refined != support_spill.first.refined)
            throw std::runtime_error("disk-spilled distinct support or point refinement changed output");
        auto cancelled_settings = parallel_settings;
        std::atomic<bool> reference_stop{false};
        Reconstruction cancelled_reference((root / "cancelled-reference").string(),source_views,cancelled_settings,&reference_stop);
        cancelled_reference.add_pair(0,1,pixels,pixels,{prediction(-2),{}});
        cancelled_reference.add_pair(0,2,pixels,pixels,{prediction(-4),{}});
        cancelled_reference.complete_reference(0,true); reference_stop = true;
        bool reference_cancelled = false;
        try { cancelled_reference.finish((root / "cancelled-reference.ply").string(),dataset); }
        catch (const std::exception&) { reference_cancelled = true; }
        if (!reference_cancelled) throw std::runtime_error("background reference cancellation was ignored");
        const auto blocked = root / "not-a-directory";
        { std::ofstream file(blocked); file << "blocked"; }
        Reconstruction without_preview((root / "no-preview").string(), views, config, nullptr, blocked.string());
        without_preview.add_pair(0, 1, pixels, pixels, {prediction(-2), prediction(2)});
        if (without_preview.checkpoint().error.empty()) throw std::runtime_error("checkpoint I/O failure was not reported");
        std::printf("PASS dense disk observations, cycle/mask filtering, three-image support, fusion, streaming PLY, source transform and cancellation (%llu points)\n",
                    (unsigned long long)stats.exported);
        return 0;
    } catch (const std::exception& e) { std::fprintf(stderr, "FAIL: %s\n", e.what()); return 1; }
}

#include "data/DatasetParser.h"
#include "config/TrainConfigJson.h"

#include <cmath>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace {
int failures = 0;

void check(bool ok, const char* name) {
    std::printf("%s %s\n", ok ? "ok" : "FAIL", name);
    if (!ok) ++failures;
}

template<class F> void rejects(F f, const char* name) {
    try { f(); check(false, name); }
    catch (const std::runtime_error&) { check(true, name); }
}

void write_ply(const fs::path& path, bool binary, int count = 2,
               bool finite = true, bool rgb = true) {
    std::ofstream out(path, std::ios::binary);
    out << "ply\nformat " << (binary ? "binary_little_endian" : "ascii")
        << " 1.0\nelement vertex " << count
        << "\nproperty double x\nproperty double y\nproperty double z\n";
    if (rgb) out << "property uchar red\nproperty uchar green\nproperty uchar blue\n";
    out << "end_header\n";
    for (int i = 0; i < count; ++i) {
        const double xyz[] = {1000000.25 + i, 2000000.5 + i, 3000000.75 + i};
        if (binary) {
            out.write(reinterpret_cast<const char*>(xyz), sizeof xyz);
            if (rgb) out.write("\x0a\x14\x1e", 3);
        } else {
            out.precision(17);
            if (finite) out << xyz[0]; else out << "nan";
            out << ' ' << xyz[1] << ' ' << xyz[2];
            if (rgb) out << " 10 20 30";
            out << '\n';
        }
    }
}

void write_colmap(const fs::path& root) {
    fs::create_directories(root / "sparse" / "0");
    std::ofstream(root / "sparse/0/cameras.txt") << "1 PINHOLE 640 480 500 500 320 240\n";
    std::ofstream images(root / "sparse/0/images.txt");
    for (int i = 0; i < 4; ++i)
        images << i + 1 << " 1 0 0 0 " << i << " 0 5 1 " << i << ".png\n\n";
    std::ofstream(root / "sparse/0/points3D.txt") << "1 1 2 3 255 0 0 0\n";
}

void write_nerfstudio(const fs::path& root) {
    std::ofstream(root / "transforms.json") << R"({
        "camera_model":"PINHOLE", "w":640, "h":480,
        "fl_x":500, "fl_y":500, "cx":320, "cy":240,
        "ply_file_path":"missing-native.ply",
        "applied_transform":[[1,0,0,10],[0,1,0,20],[0,0,1,30]],
        "frames":[{"file_path":"0.png",
            "transform_matrix":[[1,0,0,10],[0,1,0,20],[0,0,1,30],[0,0,0,1]]}]
    })";
}
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const fs::path root = fs::temp_directory_path() /
        ("ss_seed_pointcloud_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    try {
        write_colmap(root);
        DatasetParserConfig cfg;
        cfg.require_image_files = false;
        auto native = parse_colmap_dataset(root.string(), cfg);
        check(native.points.num() == 1 && native.points.xyz[0] == 1,
              "unset path retains native sparse points");
        for (bool binary : {false, true}) {
            write_ply(root / "lidar.ply", binary);
            cfg.seed_pointcloud = "lidar.ply";
            cfg.center_mode = "none";
            auto ds = parse_colmap_dataset(root.string(), cfg);
            check(ds.points.num() == 2 && std::abs(ds.points.xyz[0] - 1000000.25) < 1e-8,
                  "external cloud replaces native cloud with double precision");
            check(ds.points.rgb[0] == 10 && ds.points.rgb[1] == 20 && ds.points.rgb[2] == 30,
                  "RGB survives ASCII and binary parsing");
            check(ds.c2w == native.c2w, "replacement leaves uncentered camera poses intact");
            cfg.seed_pointcloud = (root / "lidar.ply").string();
            cfg.center_mode = "point-mean";
            cfg.eval_mode = "interval";
            cfg.eval_interval = 2;
            ds = parse_colmap_dataset(root.string(), cfg);
            cfg.split = "eval";
            auto eval = parse_colmap_dataset(root.string(), cfg);
            check(ds.center == eval.center && ds.points.xyz == eval.points.xyz,
                  "training and evaluation use the same absolute cloud and center");
            cfg.split = "train";
            cfg.eval_mode = "all";
            cfg.center_mode = "point-median";
            ds = parse_colmap_dataset(root.string(), cfg);
            check(std::abs(ds.points.xyz[0]) < 2 && ds.center[0] > 999999,
                  "point centering uses the replacement cloud");
            check(std::abs((double)ds.c2w[3] + ds.center[0] - native.c2w[3]) < 0.1,
                  "cloud and cameras receive the same center shift");
        }
        cfg.center_mode = "none";
        std::ofstream(root / "sparse/0/points3D.txt") << "corrupt\n";
        check(parse_colmap_dataset(root.string(), cfg).points.num() == 2,
              "overridden sparse cloud is not read");
        fs::remove(root / "sparse/0/points3D.txt");
        check(parse_colmap_dataset(root.string(), cfg).points.num() == 2,
              "poses-only COLMAP accepts external cloud");
        write_nerfstudio(root);
        auto ns = parse_nerfstudio_dataset(root.string(), cfg);
        check(ns.points.xyz[0] == 999990.25 && ns.points.xyz[1] == 1999980.5,
              "Nerfstudio applies the same applied_transform inverse");
        cfg.seed_pointcloud = "missing.ply";
        rejects([&] { parse_colmap_dataset(root.string(), cfg); }, "missing explicit cloud fails");
        cfg.seed_pointcloud = "lidar.ply";
        write_ply(root / "lidar.ply", false, 0);
        rejects([&] { parse_colmap_dataset(root.string(), cfg); }, "empty explicit cloud fails");
        write_ply(root / "lidar.ply", false, 2, false);
        rejects([&] { parse_colmap_dataset(root.string(), cfg); }, "non-finite cloud fails");
        write_ply(root / "lidar.ply", false, 2, true, false);
        rejects([&] { parse_colmap_dataset(root.string(), cfg); }, "cloud without RGB fails");
        TrainConfig config;
        config.seed_pointcloud = "lidar.ply";
        std::string json = "{";
        for (const auto& item : train_config_json_pairs(config)) {
            if (json.size() > 1) json += ',';
            json += std::string("\"") + item.first + "\":" + item.second;
        }
        json += '}';
        TrainConfig restored;
        train_config_from_json(json_parse(json), restored);
        check(restored.seed_pointcloud == config.seed_pointcloud,
              "seed path survives the shared config and preset JSON encoding");
    } catch (const std::exception& e) {
        std::printf("FAIL %s\n", e.what());
        ++failures;
    }
    fs::remove_all(root);
    return failures ? 1 : 0;
}

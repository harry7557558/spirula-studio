#include "data/MarkerDetector.h"
#include "data/MarkerScan.h"
#include "data/MarkerAlign.h"
#include "data/SceneTransform.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

using namespace spirula;
namespace fs = std::filesystem;

static int failures = 0;
static void check(bool ok, const char* text) {
    std::printf("%s %s\n", ok ? "ok" : "FAIL", text);
    if (!ok) ++failures;
}

struct Pattern {
    MarkerFamily family;
    int side;
    std::vector<uint8_t> bytes;
};

static std::vector<uint8_t> render(const Pattern& p, int rotation) {
    constexpr int w = 300, cell = 20;
    std::vector<uint8_t> img(w * w, 255);
    const int bits = p.side * p.side;
    for (int y = -1; y <= p.side; ++y) {
        for (int x = -1; x <= p.side; ++x) {
            int value = 0;
            if (x >= 0 && x < p.side && y >= 0 && y < p.side) {
                const int k = y * p.side + x;
                const int shift = k / 8 == bits / 8 ? bits % 8 - 1 - k % 8 : 7 - k % 8;
                value = ((p.bytes[(size_t)k / 8] >> shift) & 1) * 255;
            }
            for (int j = 0; j < cell; ++j) for (int i = 0; i < cell; ++i) {
                int px = 50 + (x + 1) * cell + i, py = 50 + (y + 1) * cell + j;
                for (int r = 0; r < rotation; ++r) { const int old = px; px = w - 1 - py; py = old; }
                img[(size_t)py * w + px] = (uint8_t)value;
            }
        }
    }
    return img;
}

int main(int argc, char** argv) {
    // ID 42 bytes from OpenCV's predefined dictionaries, independent of AprilTag's bit layout.
    const Pattern patterns[] = {
        {MarkerFamily::Aruco4x4, 4, {50, 140}},
        {MarkerFamily::Aruco5x5, 5, {108, 36, 251, 1}},
        {MarkerFamily::Aruco6x6, 6, {97, 89, 6, 155, 10}},
        {MarkerFamily::Aruco7x7, 7, {53, 233, 63, 158, 54, 124, 0}},
        {MarkerFamily::ArucoOriginal, 5, {132, 18, 148, 1}}
    };
    MarkerDetector automatic(MarkerFamily::Auto, 1);
    for (const auto& p : patterns) {
        MarkerDetector detector(p.family, 1);
        for (int rotation = 0; rotation < 4; ++rotation) {
            const auto img = render(p, rotation);
            const auto detected = detector.detect_grayscale(img.data(), 300, 300);
            const auto found = automatic.detect_grayscale(img.data(), 300, 300);
            check(detected.size() == 1 && detected[0].id == 42 && detected[0].family == p.family,
                  marker_family_name(p.family));
            check(std::any_of(found.begin(), found.end(), [&](const MarkerDetection& d) {
                return d.id == 42 && d.family == p.family;
            }), "auto detects correct dictionary and ID after rotation");
        }
    }
    const auto base = fs::temp_directory_path() / ("spirula-marker-test-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(base / "sparse/0");
    for (const auto* name : {"cameras.bin", "images.bin", "points3D.bin"})
        std::ofstream(base / "sparse/0" / name);
    Marker3D marker;
    marker.id = 42;
    marker.family = MarkerFamily::ArucoOriginal;
    check(save_markers_json((base / "markers.json").string(), {marker}), "save marker family");
    check(marker_dataset_dir((base / "sparse/0").string()) == base.string(), "resolve sparse/0 to dataset root");
    const auto file = find_markers_file((base / "sparse/0/points3D.bin").string());
    std::vector<Marker3D> loaded;
    check(!file.empty() && load_markers_json(file, loaded) && loaded[0].family == marker.family,
          "find root JSON when opening sparse/0/points3D.bin and preserve family");
    std::atomic<bool> cancel{true};
    ParsedDataset empty;
    empty.image_filenames = {"missing.jpg"};
    auto scan = scan_markers(empty, MarkerFamily::Tag36h10, 1, 1, &cancel);
    check(scan.cancelled && scan.images == 0, "cancel before image decode");
    scan = scan_markers(empty, MarkerFamily::Tag36h10, 1, 1);
    check(scan.unreadable == 1 && scan.markers.empty(), "report unreadable images");

    ParsedDataset calibrated;
    calibrated.num_cameras = 2;
    calibrated.widths = calibrated.heights = {300, 300};
    calibrated.camera_models = calibrated.camera_distortions = {0, 0};
    calibrated.dist_coeffs.resize(16);
    calibrated.intrins = {200, 200, 150, 150, 200, 200, 150, 150};
    calibrated.c2w = {1,0,0,0, 0,-1,0,0, 0,0,-1,0,
                      1,0,0,1, 0,-1,0,0, 0,0,-1,0};
    const auto img = render(patterns[0], 0);
    for (int camera = 0; camera < 2; ++camera) {
        const int size = camera ? 150 : 300;
        std::vector<uint8_t> pixels((size_t)size * size, 255);
        for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
            const int src_x = camera ? x * 2 + 40 : x;
            const int src_y = camera ? y * 2 : y;
            if (src_x < 300) pixels[(size_t)y * size + x] = img[(size_t)src_y * 300 + src_x];
        }
        const auto path = (base / (std::to_string(camera) + ".pgm")).string();
        std::ofstream file(path, std::ios::binary);
        file << "P5\n" << size << " " << size << "\n255\n";
        file.write((const char*)pixels.data(), (std::streamsize)pixels.size());
        calibrated.image_filenames.push_back(path);
    }
    scan = scan_markers(calibrated, MarkerFamily::Auto, 1, 1);
    check(scan.family == MarkerFamily::Aruco4x4 && scan.markers.size() == 1 &&
          scan.markers[0].id == 42 && std::abs(scan.markers[0].center[2] - 5) < 0.05,
          "auto scan triangulates resized photos in calibrated pixel coordinates");

    MarkerObservation a, b;
    a.image_id = 0; b.image_id = 1;
    a.det.id = b.det.id = 42;
    a.det.center[0] = 110; b.det.center[0] = 150;
    a.det.center[1] = b.det.center[1] = 110;
    check(triangulate_markers({a, b}, calibrated).empty(), "reject intersection behind observing cameras");
    {
        std::ofstream file(base / "invalid.json");
        file << "{\"markers\":[{\"id\":42}]}";
    }
    check(!load_markers_json((base / "invalid.json").string(), loaded) && loaded.size() == 1,
          "invalid JSON geometry does not replace previous markers");

    const auto run = base / "outputs/run";
    fs::create_directories(run / "step-000001000.ckpt");
    {
        std::ofstream file(run / "config.json");
        file << "{\"data\":\"../..\",\"image_dir\":\"photos\",\"colmap_recon_dir\":\"sparse/0\"}";
        SceneTransform transform;
        transform.scale = 2;
        transform.t[0] = 3;
        std::ofstream sidecar(run / "scene_transform.json");
        const double centre[3] = {0, 0, 0};
        sidecar << scene_transform_json(transform, "none", centre);
    }
    const auto context = resolve_marker_dataset((run / "step-000001000.ckpt/splat.ply").string());
    check(context.directory == base.string() && context.config.image_dir == "photos" &&
          context.config.recon_dir == "sparse/0", "resolve a checkpoint PLY through run config and relative data path");
    check(context.file_from_dataset.s == 2 && context.file_from_dataset.t[0] == 3,
          "preserve dataset-to-trained-splat frame from run sidecar");
    const auto loose = fs::path(base.string() + "-loose");
    fs::create_directories(loose);
    check(resolve_marker_dataset((loose / "model.ply").string()).directory.empty(),
          "loose PLY does not pick an unrelated dataset in a sibling directory");
    fs::remove_all(loose);

    std::vector<Marker3D> layout(4);
    for (int i = 0; i < 4; ++i) { layout[(size_t)i].id = 11 + i * 10; layout[(size_t)i].center[0] = i; }
    layout[3].center[1] = 2;
    const double up[3] = {0, 0, 1};
    MarkerSystemConfig suggested;
    suggested.three_markers = suggest_marker_system(layout, up);
    Sim3 orientation;
    check(solve_marker_system(layout, suggested, orientation) && orientation.R[8] > 0.99,
          "suggest non-collinear IDs with positive scene up when first three are collinear");
    layout.resize(2);
    const auto pair = suggest_marker_system(layout, up);
    check(pair.origin_id == 11 && pair.x_axis_id == 21 && pair.xy_plane_id == -1,
          "two-marker defaults contain only available IDs");
    fs::remove_all(base);

    for (int i = 1; i < argc; ++i) {
        const std::string dir = argv[i];
        DatasetParserConfig cfg;
        cfg.require_image_files = false;
        const auto ds = parse_dataset(dir, cfg, "");
        const auto result = scan_markers(ds, MarkerFamily::Auto, 1, 4);
        std::printf("DATASET %s: images=%d unreadable=%d observations=%d markers=%zu family=%s\n",
                    dir.c_str(), result.images, result.unreadable, result.observations,
                    result.markers.size(), marker_family_name(result.family));
        for (const auto& m : result.markers)
            std::printf("  ID=%d views=%d error=%.3f\n", m.id, m.num_views, m.reproj_error_px);
        check(!result.markers.empty(), "real dataset has triangulated markers");
        if (result.markers.size() >= 3) {
            MarkerSystemConfig system;
            system.three_markers.origin_id = result.markers[0].id;
            system.three_markers.x_axis_id = result.markers[1].id;
            system.three_markers.xy_plane_id = result.markers[2].id;
            Sim3 transform;
            const bool ok = solve_marker_system(result.markers, system, transform);
            double origin[3];
            transform.apply(result.markers[0].center, origin);
            check(ok && std::hypot(std::hypot(origin[0], origin[1]), origin[2]) < 1e-6,
                  "real marker centres solve alignment with origin at zero");
        }
    }
    return failures ? 1 : 0;
}

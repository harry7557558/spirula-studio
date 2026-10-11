#include "app/gui/DatasetPrep.h"
#include "app/gui/TelemetryProbe.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

int failures = 0;

void check(bool ok, const char* message) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", message);
    if (!ok) failures++;
}

struct Scratch {
    fs::path root = fs::temp_directory_path() /
        ("spirula_telemetry_probe_test_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    Scratch() { fs::create_directories(root); }
    ~Scratch() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
};

std::vector<uint8_t> gps_tiff(uint32_t lat, uint32_t lon, bool bad_den = false) {
    std::vector<uint8_t> bytes(128, 0);
    auto u16 = [&](size_t at, uint16_t value) {
        for (int i = 0; i < 2; i++) bytes[at + i] = (uint8_t)(value >> (8 * i));
    };
    auto u32 = [&](size_t at, uint32_t value) {
        for (int i = 0; i < 4; i++) bytes[at + i] = (uint8_t)(value >> (8 * i));
    };
    auto entry = [&](size_t at, uint16_t tag, uint16_t type,
                     uint32_t count, uint32_t value) {
        u16(at, tag); u16(at + 2, type); u32(at + 4, count); u32(at + 8, value);
    };
    bytes[0] = bytes[1] = 'I';
    u16(2, 42); u32(4, 8); u16(8, 1);
    entry(10, 0x8825, 4, 1, 26);
    u16(26, 4);
    entry(28, 1, 2, 2, 'N');
    entry(40, 2, 5, 3, 80);
    entry(52, 3, 2, 2, 'E');
    entry(64, 4, 5, 3, 104);
    u32(80, lat); u32(104, lon);
    for (size_t at : {80u, 88u, 96u, 104u, 112u, 120u}) u32(at + 4, 1);
    if (bad_den) u32(84, 0);
    return bytes;
}

void write_file(const fs::path& path, const std::vector<uint8_t>& data = {}) {
    fs::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream.write((const char*)data.data(), (std::streamsize)data.size());
}

void write_photo(const fs::path& path, uint32_t lat = 32, uint32_t lon = 112,
                 bool with_attitude = false, bool bad_den = false) {
    const auto tiff = gps_tiff(lat, lon, bad_den);
    if (path.extension() == ".TIFF") { write_file(path, tiff); return; }
    std::vector<uint8_t> bytes{0xff, 0xd8};
    auto app1 = [&](const std::string& payload) {
        const size_t length = payload.size() + 2;
        bytes.insert(bytes.end(), {0xff, 0xe1, (uint8_t)(length >> 8), (uint8_t)length});
        bytes.insert(bytes.end(), payload.begin(), payload.end());
    };
    app1(std::string("Exif\0\0", 6) + std::string((const char*)tiff.data(), tiff.size()));
    if (with_attitude) {
        app1(std::string("http://ns.adobe.com/xap/1.0/\0", 29) +
             "<x drone-dji:GimbalYawDegree=\"12\" drone-dji:GimbalPitchDegree=\"-90\" "
             "drone-dji:GimbalRollDegree=\"0\"/>");
    }
    bytes.insert(bytes.end(), {0xff, 0xd9});
    write_file(path, bytes);
}

void check_photo_tree(const fs::path& root) {
    const fs::path photos = root / "photos";
    write_photo(photos / "cam0" / "a.JPG", 32, 112, true);
    write_photo(photos / "cam1" / "nested" / "b.TIFF");
    write_photo(photos / "cam1" / "invalid_lat.jpg", 91, 112);
    write_photo(photos / "cam1" / "invalid_lon.jpg", 32, 181);
    write_photo(photos / "cam1" / "invalid_rational.jpg", 32, 112, false, true);
    for (const char* extension : {".JPEG", ".PNG", ".WEBP", ".BMP", ".EXR",
                                  ".INSP", ".HEIC", ".HEIF", ".HIF"})
        write_file(photos / "cam0" / (std::string("unknown") + extension));
    write_file(photos / "notes.txt");
    write_file(photos / "masks" / "a.png");
    write_file(photos / "masks" / "cam0" / "b.png");

    const auto info = gui::probe_photo_telemetry(photos.string());
    check(info.done && !info.failed, "nested photo scan completes");
    check(info.photos == 14 && info.photos == gui::DatasetPrep::count_images(
              photos.string(), (photos / "masks").string()),
          "all supported formats count, matching preparation while excluding masks");
    check(info.with_gps == 2, "only usable GPS fixes count, including TIFF photos");
    check(info.with_attitude == 1, "XMP attitude still reads");

    gui::TelemetryProbe probe;
    const auto pending = probe.get(photos.string(), false);
    check(!pending.done && !pending.failed, "first GUI query queues a pending scan");
    gui::TelemetryInfo done;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    do {
        done = probe.get(photos.string(), false);
        if (done.done || done.failed) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now() < deadline);
    check(done.done && done.photos == info.photos && done.with_gps == info.with_gps,
          "GUI worker returns the same complete coverage");
}

void check_dataset_layouts(const fs::path& root) {
    const fs::path dataset = root / "dataset";
    write_photo(dataset / "images" / "cam0" / "a.jpg");
    write_photo(dataset / "images" / "cam1" / "b.jpg");
    write_file(dataset / "masks" / "cam0" / "a.png");
    write_file(dataset / "feature_masks" / "cam0" / "a.png");
    const auto dataset_info = gui::probe_photo_telemetry(dataset.string());
    const auto images_info = gui::probe_photo_telemetry((dataset / "images").string());
    check(dataset_info.done && dataset_info.photos == 2 && dataset_info.with_gps == 2,
          "dataset root scans images rather than generated masks");
    check(images_info.done && images_info.photos == dataset_info.photos,
          "selecting images directly produces the same count");

    std::error_code ec;
    fs::create_directory_symlink(dataset / "images", root / "linked-images", ec);
    if (!ec) {
        const auto linked = gui::probe_photo_telemetry((root / "linked-images").string());
        check(linked.done && linked.photos == 2 && linked.with_gps == 2,
              "linked prepared image folders are scanned");
    }
}

void check_failures(const fs::path& root) {
    const auto missing = gui::probe_photo_telemetry((root / "missing").string());
    check(!missing.done && missing.failed, "missing input never becomes complete");
    write_file(root / "regular-file");
    const auto regular = gui::probe_photo_telemetry((root / "regular-file").string());
    check(!regular.done && regular.failed, "non-directory input never becomes complete");
    fs::create_directory(root / "empty");
    const auto empty = gui::probe_photo_telemetry((root / "empty").string());
    check(empty.done && empty.photos == 0 && empty.with_gps == 0,
          "an empty tree reports no positioned photos");
    std::atomic<bool> cancel{true};
    const auto cancelled = gui::probe_photo_telemetry(root.string(), &cancel);
    check(!cancelled.done && !cancelled.failed && cancelled.photos == 0,
          "cancelled scans remain incomplete");

    std::error_code ec;
    const fs::path cycle = root / "cycle";
    fs::create_directory(cycle);
    fs::create_directory_symlink(cycle, cycle / "loop", ec);
    if (!ec) {
        const auto loop = gui::probe_photo_telemetry(cycle.string());
        check(!loop.done && loop.failed, "directory cycles cannot certify GPS coverage");
        const fs::path child = root / "cycle-child" / "images";
        fs::create_directories(child);
        fs::create_directory_symlink(child, child / "loop", ec);
        check(!ec, "fixture: a cycle inside images is created");
        const auto child_loop = gui::probe_photo_telemetry(child.parent_path().string());
        check(!child_loop.done && child_loop.failed,
              "images folder resolution detects cycles before metadata reading");
    }
}

}  // namespace

int main() {
    Scratch scratch;
    check_photo_tree(scratch.root);
    check_dataset_layouts(scratch.root);
    check_failures(scratch.root);
    if (failures == 0) std::puts("telemetry_probe_test: OK");
    return failures == 0 ? 0 : 1;
}

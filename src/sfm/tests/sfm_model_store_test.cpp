#include "sfm/map/ModelStore.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <set>
#include <thread>

using namespace sfm;

static void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

static Reconstruction fixture() {
    Reconstruction m;
    Camera c;
    c.id = 101; c.model = CamModel::FullOpenCV; c.width = 5280; c.height = 3956;
    int k = 0;
    for (double* p : {&c.fx, &c.fy, &c.cx, &c.cy, &c.k1, &c.k2, &c.p1, &c.p2,
                      &c.k3, &c.k4, &c.k5, &c.k6, &c.sx1, &c.sy1, &c.pixel_scale})
        *p = (++k) * 0.123456789012345;
    m.cameras.emplace(101, c);
    for (uint32_t id : {20u, 92u, 701u}) {
        Image im;
        im.id = id; im.camera_id = c.id; im.name = "capture/subfolder/photo-" + std::to_string(id);
        for (size_t j = 0; j < im.pose.R.size(); ++j) im.pose.R[j] = (j + id) * 0.019876543210987;
        im.pose.t = {1.23456789012345, -3.56789012345678, 99.23456789012345};
        im.registered = id != 701; im.exif_orientation = 6;
        im.points2D = {{12.25, 29.125}, {-1.5, 399.01}};
        im.point3D_ids = {id == 701 ? kInvalidPoint3D : 987654321012345ull, kInvalidPoint3D};
        im.points2D.reserve(7); im.point3D_ids.reserve(8);
        m.images.emplace(id, std::move(im));
    }
    Point3D p;
    p.xyz = {0.123456789012345, 2.34567890123456, -44.5678901234567};
    p.rgb[0] = 7; p.rgb[1] = 201; p.rgb[2] = 253; p.error = 1.6170123456789;
    p.track = {{20, 0}, {92, 0}};
    p.track.reserve(11);
    m.points3D.emplace(987654321012345ull, p);
    m.next_point3D_id = 987654321012399ull;
    m.rigs.resize(3);
    RigCalib& rig = m.rigs[1];
    rig.ref = 1; rig.resize(2); rig.user_scale = 17.2345678901234;
    rig.cam_from_rig[0] = m.images.at(20).pose;
    rig.cam_from_rig[1] = m.images.at(92).pose;
    rig.established = {1, 1}; rig.fixed = {0, 1}; rig.support = {23, 24};
    rig.spread_deg = {0.012345678901234, 0.987654321098765}; rig.declined_at = {123, 456};
    m.rig_detached = {20, 701};
    return m;
}

static std::vector<char> bytes(const std::filesystem::path& p) {
    std::ifstream file(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

static void testRoundtrip() {
    ModelStore store("");
    const Reconstruction m = fixture();
    const auto meta = store.put(m);
    check(meta.registered_images == std::vector<uint32_t>({20, 92}), "wrong registered-image metadata");
    check(meta.resident_bytes == modelResidentBytes(m), "wrong resident-byte metadata");
    const Reconstruction loaded = store.load(meta, meta.resident_bytes);
    check(loaded.images.size() == 3 && !loaded.images.at(701).registered, "unregistered image lost");
    check(loaded.images.at(20).exif_orientation == 6, "EXIF orientation lost");
    check(loaded.cameras.at(101).pixel_scale == m.cameras.at(101).pixel_scale, "pixel scale lost");
    check(loaded.rigs.size() == 3 && loaded.rig_detached == m.rig_detached, "rig state lost");
    check(loaded.next_point3D_id == m.next_point3D_id, "next point id changed");
    check(modelResidentBytes(loaded) <= meta.resident_bytes, "reload exceeds saved memory estimate");
    const auto again = store.put(loaded);
    check(bytes(store.path(meta)) == bytes(store.path(again)), "model fields changed during disk roundtrip");
    bool rejected = false;
    try { store.load(meta, meta.resident_bytes - 1); } catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "over-budget model loaded");
    const auto empty = store.put({});
    check(store.load(empty).images.empty(), "empty model failed");
    Reconstruction reserved = m;
    const size_t before = modelResidentBytes(reserved);
    reserved.images.at(20).points2D.reserve(10000);
    check(modelResidentBytes(reserved) > before + 9000 * sizeof(Vec2), "capacity ignored in memory estimate");
}

static void testGroups() {
    std::vector<StoredModel> models = {
        {0, 70, {1, 2, 3}}, {1, 60, {3, 4, 5}}, {2, 50, {5, 6}},
        {3, 40, {100, 101}}, {4, 40, {101, 102}},
    };
    const auto groups = storedModelGroups(models, 140);
    check(groups == std::vector<std::vector<size_t>>({{0, 1}, {2}, {3, 4}}), "overlap grouping changed");
    std::set<size_t> seen;
    for (const auto& group : groups) {
        size_t sum = 0;
        for (size_t i : group) { sum += models[i].resident_bytes; check(seen.insert(i).second, "model grouped twice"); }
        check(sum <= 140, "group exceeds resident budget");
    }
    check(seen.size() == models.size(), "grouping dropped a model");
    check(storedModelBytes(models) == 260, "wrong total memory estimate");
    bool rejected = false;
    try { storedModelGroups(models, 69); } catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "oversized atom did not fail");
    check(storedModelGroups({}, 0).empty(), "empty grouping failed");
}

static void testConcurrencyAndCleanup() {
    std::filesystem::path dir;
    {
        ModelStore store("");
        dir = store.directory();
        std::vector<StoredModel> stored(12);
        std::vector<std::thread> workers;
        for (size_t i = 0; i < stored.size(); ++i)
            workers.emplace_back([&, i] { stored[i] = store.put(fixture()); });
        for (auto& worker : workers) worker.join();
        std::set<uint64_t> files;
        for (const auto& meta : stored) {
            check(files.insert(meta.file).second, "concurrent spill overwrote a model");
            check(store.load(meta).points3D.size() == 1, "concurrent spill damaged a model");
        }
        store.erase(stored[0]);
        check(!std::filesystem::exists(store.path(stored[0])), "obsolete model not removed");
    }
    check(!std::filesystem::exists(dir), "scratch directory not removed");
    bool rejected = false;
    try {
        ModelStore store("");
        dir = store.directory();
        auto meta = store.put(fixture());
        std::filesystem::resize_file(store.path(meta), 12);
        store.load(meta);
    } catch (const std::runtime_error&) { rejected = true; }
    check(rejected && !std::filesystem::exists(dir), "truncated model or failure cleanup failed");
    {
        ModelStore store("");
        const auto meta = store.put(fixture());
        std::fstream file(store.path(meta), std::ios::binary | std::ios::in | std::ios::out);
        file.seekp(20);
        const uint64_t bad = ~uint64_t{0};
        file.write(reinterpret_cast<const char*>(&bad), sizeof bad); file.close();
        rejected = false;
        try { store.load(meta); } catch (const std::runtime_error&) { rejected = true; }
        check(rejected, "corrupt count did not fail before allocation");
    }
}

int main() {
    try {
        testRoundtrip(); testGroups(); testConcurrencyAndCleanup();
        std::puts("sfm_model_store_test: PASS");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "sfm_model_store_test: %s\n", e.what());
        return 1;
    }
}

// --poses below extraction: a COLMAP model written with awkward bytes comes back
// bit for bit, with points that fit it (docs/notes/fixed-poses.md).
//
// The input is what a matchmove export can hand over: quaternions that are not
// unit length or have w < 0, ids that are not positions, names with an
// extension, images out of order, and two cameras of which one is a model
// (SIMPLE_RADIAL) that only reaches the triangulation through a mapping.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "sfm/core/FixedPoses.h"
#include "sfm/map/Mapper.h"
#include "sfm/tests/SyntheticRegister.h"
#include "sfm/tests/TestMain.h"

namespace fs = std::filesystem;
using namespace sfm;

static void check(bool condition, const char* message, int& fails) {
    std::printf("%s: %s\n", condition ? "ok" : "FAIL", message);
    if (!condition) fails++;
}

static fs::path tempDir() {
    const fs::path d = fs::temp_directory_path() / "spirula_sfm_fixed_pose_selftest";
    std::error_code ec;
    fs::remove_all(d, ec);
    fs::create_directories(d, ec);
    return d;
}

template <class T>
static void put(std::string& s, T v) {
    s.append((const char*)&v, sizeof v);
}

static std::string slurp(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

static void spit(const fs::path& p, const std::string& s) {
    std::ofstream(p, std::ios::binary | std::ios::trunc).write(s.data(), (std::streamsize)s.size());
}

// The same cloud SyntheticRegister.h's makeScene draws: feature p of every image is point p.
static std::vector<Vec3> scenePoints(int n) {
    std::mt19937 rng(5);
    std::uniform_real_distribution<double> ub(-2.5, 2.5);
    std::vector<Vec3> pts((size_t)n);
    for (Vec3& p : pts) p = {ub(rng), ub(rng), ub(rng)};
    return pts;
}

static uint32_t fileId(int k) { return 100 + 7 * (uint32_t)k; }

// cameras.bin with camera 7 (PINHOLE) and camera 9 (SIMPLE_RADIAL, k = 0), both
// the scene's 1200 px lens; images.bin with scene image k as fileId(k), last first.
static void writeInput(const fs::path& dir, const synth_reg::Scene& sc) {
    std::string cams;
    put<uint64_t>(cams, 2);
    put<uint32_t>(cams, 7);
    put<int32_t>(cams, 1);
    put<uint64_t>(cams, 1280);
    put<uint64_t>(cams, 960);
    for (double v : {1200.0, 1200.0, 640.0, 480.0}) put<double>(cams, v);
    put<uint32_t>(cams, 9);
    put<int32_t>(cams, 2);
    put<uint64_t>(cams, 1280);
    put<uint64_t>(cams, 960);
    for (double v : {1200.0, 640.0, 480.0, 0.0}) put<double>(cams, v);
    spit(dir / "cameras.bin", cams);

    std::string ims;
    const int M = (int)sc.gt.size();
    put<uint64_t>(ims, (uint64_t)M);
    for (int k = M - 1; k >= 0; k--) {
        Quat q = rotationToQuaternion(sc.gt[(size_t)k].R);
        if (k % 3 == 1) for (double& v : q) v = -v;
        if (k % 3 == 2) for (double& v : q) v *= 1.7;
        put<uint32_t>(ims, fileId(k));
        for (double v : q) put<double>(ims, v);
        const Vec3& t = sc.gt[(size_t)k].t;
        for (double v : {t.x, t.y, t.z}) put<double>(ims, v);
        put<uint32_t>(ims, k % 2 ? 9u : 7u);
        const std::string name = "cam" + std::to_string(k) + ".jpg";
        ims.append(name.c_str(), name.size() + 1);
        put<uint64_t>(ims, 1);   // a tracker point the run is to drop
        for (double v : {10.0, 20.0}) put<double>(ims, v);
        put<uint64_t>(ims, 1);
    }
    spit(dir / "images.bin", ims);
}

// Parsed here rather than with FixedPoses.h or Model.h, so the checks are independent.
struct Reader {
    std::string s;
    size_t at = 0;
    template <class T>
    T take() {
        T v{};
        if (at + sizeof v <= s.size()) std::memcpy(&v, s.data() + at, sizeof v);
        at += sizeof v;
        return v;
    }
};

struct ImageRows {
    std::string pose;                      // the 56 pose bytes
    std::vector<uint64_t> point3D_ids;
};

static std::map<uint32_t, ImageRows> readImages(const fs::path& images_bin) {
    Reader r{slurp(images_bin)};
    std::map<uint32_t, ImageRows> out;
    const uint64_t n = r.take<uint64_t>();
    for (uint64_t i = 0; i < n && r.at < r.s.size(); i++) {
        const uint32_t id = r.take<uint32_t>();
        ImageRows& row = out[id];
        row.pose = r.s.substr(r.at, 56);
        r.at += 56 + 4;
        r.at = r.s.find('\0', r.at) + 1;
        const uint64_t np = r.take<uint64_t>();
        for (uint64_t p = 0; p < np; p++) {
            r.at += 16;
            row.point3D_ids.push_back(r.take<uint64_t>());
        }
    }
    return out;
}

static std::map<uint32_t, std::string> poseBytes(const fs::path& images_bin) {
    std::map<uint32_t, std::string> out;
    for (const auto& kv : readImages(images_bin)) out[kv.first] = kv.second.pose;
    return out;
}

// Every track element names an image of images.bin whose row points back at
// the point, and the file holds `want` points.
static bool tracksAgree(const fs::path& dir, size_t want) {
    const std::map<uint32_t, ImageRows> images = readImages(dir / "images.bin");
    Reader r{slurp(dir / "points3D.bin")};
    const uint64_t n = r.take<uint64_t>();
    bool ok = n == want;
    for (uint64_t i = 0; i < n && r.at < r.s.size(); i++) {
        const uint64_t id = r.take<uint64_t>();
        r.at += 3 * 8 + 3 + 8;
        const uint64_t len = r.take<uint64_t>();
        for (uint64_t k = 0; k < len; k++) {
            const uint32_t img = r.take<uint32_t>(), idx = r.take<uint32_t>();
            auto it = images.find(img);
            ok = ok && it != images.end() && idx < it->second.point3D_ids.size() &&
                 it->second.point3D_ids[idx] == id;
        }
    }
    return ok;
}

static void testRoundTrip(const fs::path& root, int& fails) {
    const int M = 12, N = 400;
    const synth_reg::Scene sc = synth_reg::makeScene(M);
    const fs::path in = root / "in", out = root / "out" / "sparse" / "0";
    fs::create_directories(in);
    writeInput(in, sc);

    const FixedPoses fp = readFixedPoses(in.string());
    check(fp.images.size() == (size_t)M && fp.cameras.size() == 2, "the model reads", fails);
    check(fp.cameras.at(9).model == CamModel::Radial && fp.cameras.at(9).k2 == 0.0,
          "SIMPLE_RADIAL reads as RADIAL with k2 = 0", fails);
    const std::vector<int64_t> db_index = matchFixedImages(fp, sc.db);
    bool matched = true;
    for (size_t k = 0; k < fp.images.size(); k++)
        matched = matched && db_index[k] == (int64_t)(M - 1 - (int)k);
    check(matched, "names with an extension find their stems", fails);

    const FixedSetup setup = fixedSetup(fp, db_index, sc.feats);
    MapperOptions opt;
    opt.verbose = false;
    opt.initial_cameras = setup.model.cameras;
    Mapper mapper(sc.db, sc.feats, opt, setup.camera_ids);
    const Reconstruction rec = mapper.triangulateFixed(setup.model);
    check(rec.numRegistered() == (uint32_t)M, "every posed image is in the model", fails);

    bool same = true;
    for (const auto& kv : setup.model.images) {
        auto it = rec.images.find(kv.first);
        same = same && it != rec.images.end() &&
               std::memcmp(&it->second.pose, &kv.second.pose, sizeof(Pose)) == 0;
    }
    check(same, "triangulation leaves every pose untouched", fails);

    const std::vector<Vec3> truth = scenePoints(N);
    std::vector<double> err;
    for (const auto& kv : rec.points3D) {
        const uint32_t p = kv.second.track.front().point2D_idx;
        err.push_back((kv.second.xyz - truth[p]).norm());
    }
    std::sort(err.begin(), err.end());
    std::printf("points %zu of %d, median error %.4f, p95 %.4f\n", err.size(), N,
                err.empty() ? 0.0 : err[err.size() / 2],
                err.empty() ? 0.0 : err[err.size() * 95 / 100]);
    check(err.size() >= 300, "most of the cloud triangulates", fails);
    check(!err.empty() && err[err.size() / 2] < 0.02 && err[err.size() * 95 / 100] < 0.1,
          "points land where the scene put them", fails);

    writeFixedModel(out.string(), fp, db_index, rec);
    check(checkFixedModel(out.string(), readFixedPoses(in.string())).empty(),
          "the gate passes what was written", fails);
    check(slurp(out / "cameras.bin") == slurp(in / "cameras.bin"), "cameras.bin is the input's",
          fails);
    check(poseBytes(out / "images.bin") == poseBytes(in / "images.bin"),
          "every pose is the input's, byte for byte", fails);

    check(tracksAgree(out, rec.points3D.size()),
          "tracks name the file's image ids and agree with images.bin", fails);

    std::string ims = slurp(out / "images.bin");
    ims[8 + 4 + 3] ^= 0x01;   // the first image's qw, one bit
    spit(out / "images.bin", ims);
    check(!checkFixedModel(out.string(), fp).empty(), "the gate catches one flipped bit", fails);
    writeFixedModel(out.string(), fp, db_index, rec);
    std::string cams = slurp(out / "cameras.bin");
    cams.back() ^= 0x01;
    spit(out / "cameras.bin", cams);
    check(!checkFixedModel(out.string(), fp).empty(), "the gate catches a changed camera", fails);
}

static void testRefusals(const fs::path& root, int& fails) {
    const fs::path d = root / "bad";
    fs::create_directories(d);
    std::string cams;
    put<uint64_t>(cams, 1);
    put<uint32_t>(cams, 1);
    put<int32_t>(cams, 7);   // FOV: no model of ours reproduces it
    put<uint64_t>(cams, 100);
    put<uint64_t>(cams, 100);
    for (int i = 0; i < 5; i++) put<double>(cams, 1.0);
    spit(d / "cameras.bin", cams);
    bool threw = false;
    try {
        readFixedPoses(d.string());
    } catch (const std::exception&) {
        threw = true;
    }
    check(threw, "an unsupported camera model is refused", fails);

    fs::remove(d / "cameras.bin");
    spit(d / "cameras.txt", "# text\n");
    std::string why;
    try {
        readFixedPoses(d.string());
    } catch (const std::exception& e) {
        why = e.what();
    }
    check(why.find("binary") != std::string::npos, "a text model says to save it as .bin",
          fails);

    MatchesDatabase db;
    db.images = {{"a", 0}, {"b", 0}};
    FixedPoses fp;
    for (const char* n : {"a.png", "a.jpg", "c.jpg"}) {
        FixedImage im;
        im.name = n;
        fp.images.push_back(im);
    }
    const std::vector<int64_t> idx = matchFixedImages(fp, db);
    check(idx[0] == 0 && idx[1] == -1 && idx[2] == -1,
          "an unknown name and a second name on one stem match nothing", fails);
}

static int cmdFixedPoseTest(int, char**) {
    int fails = 0;
    const fs::path root = tempDir();
    testRoundTrip(root, fails);
    testRefusals(root, fails);
    std::error_code ec;
    fs::remove_all(root, ec);
    std::printf("%s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, cmdFixedPoseTest); }

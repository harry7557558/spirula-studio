// The mapper registration tests' shared scenes: a pinhole camera arc round a
// point cloud (makeScene) and a run of a rig scene that reads which frames the
// rig placed whole off SS_SFM_RIG_DUMP (runRig).
#pragma once
#include <cstdlib>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "sfm/core/Log.h"
#include "sfm/core/Model.h"
#include "sfm/core/PriorSource.h"
#include "sfm/core/Rig.h"
#include "sfm/map/Mapper.h"
#include "sfm/tests/SyntheticRig.h"

namespace synth_reg {
using namespace sfm;

inline Pose lookAt(const Vec3& C, const Vec3& target) {
    Vec3 f = (target - C).normalized();
    Vec3 up0 = {0, 1, 0};
    Vec3 r = up0.cross(f).normalized();
    Vec3 u = f.cross(r);
    Mat3 R = {r.x, r.y, r.z, u.x, u.y, u.z, f.x, f.y, f.z};
    Vec3 t = mul(R, C);
    return {R, {-t.x, -t.y, -t.z}};
}

struct Scene {
    MatchesDatabase db;
    std::vector<FeatureSet> feats;
    std::vector<Pose> gt;
};

// M cameras on a 150-degree arc round a cloud of points, so growth crosses many
// BA boundaries. `roll_deg[c]` rolls camera c about its axis; `bob` 0 keeps every
// up axis vertical, as a horizon-levelled camera's is.
inline Scene makeScene(int M, const std::vector<double>& roll_deg = {}, double bob = 1.5) {
    const int W = 1280, H = 960, N = 400;
    Camera K = Camera::defaultFor(1, W, H, 1200);
    std::mt19937 rng(5);
    std::uniform_real_distribution<double> ub(-2.5, 2.5);
    std::normal_distribution<double> noise(0.0, 0.4);
    std::vector<Vec3> pts(N);
    for (auto& p : pts) p = {ub(rng), ub(rng), ub(rng)};
    Scene s;
    std::vector<std::vector<char>> vis(M, std::vector<char>(N, 0));
    s.feats.resize(M);
    for (int c = 0; c < M; c++) {
        const double ang = -1.3 + 2.6 * c / (M - 1);
        Pose P = lookAt({9 * std::sin(ang), bob * std::sin(0.7 * c), 9 * std::cos(ang)}, {0, 0, 0});
        if ((size_t)c < roll_deg.size()) {
            const Mat3 Rz = angleAxisToRotation({0.0, 0.0, roll_deg[(size_t)c] * M_PI / 180.0});
            P.R = mul(Rz, P.R);
            P.t = mul(Rz, P.t);
        }
        s.gt.push_back(P);
        s.feats[c].width = W;
        s.feats[c].height = H;
        s.feats[c].keypoints.resize(N);
        for (int p = 0; p < N; p++) {
            const Vec3 pc = mul(P.R, pts[p]) + P.t;
            const Vec2 px = K.project(pc);
            if (pc.z > 0.1 && px.x > 0 && px.x < W && px.y > 0 && px.y < H) {
                s.feats[c].keypoints[p] = {(float)(px.x + noise(rng)), (float)(px.y + noise(rng)), 2, 0, 0};
                vis[c][p] = 1;
            } else {
                s.feats[c].keypoints[p] = {-1000, -1000, 2, 0, 0};
            }
        }
    }
    s.db.images.resize(M);
    for (int c = 0; c < M; c++) s.db.images[c] = {"cam" + std::to_string(c), (uint32_t)N};
    for (int i = 0; i < M; i++)
        for (int j = i + 1; j < M; j++) {
            TwoViewMatches tv;
            tv.image1 = i;
            tv.image2 = j;
            tv.config = (int)TwoViewConfig::Uncalibrated;
            for (int p = 0; p < N; p++)
                if (vis[i][p] && vis[j][p]) tv.matches.push_back({(uint32_t)p, (uint32_t)p, 0});
            if (tv.matches.size() >= 15) s.db.pairs.push_back(std::move(tv));
        }
    return s;
}

inline void setEnv(const char* k, const char* v) {
#ifdef _WIN32
    _putenv_s(k, v ? v : "");
#else
    if (v) setenv(k, v, 1);
    else unsetenv(k);
#endif
}

// The frames registerFrame placed whole, read off SS_SFM_RIG_DUMP: the candidate
// each was tried through, and whether the source had been asked for a fit by then.
struct RigRun {
    Mapper::PriorStats st;
    Reconstruction model;
    size_t models = 0;
    std::vector<uint32_t> placed;
    std::vector<char> fitted;
    std::vector<uint32_t> triggers;   // images a GPS trigger fired on, in order
};

inline RigRun runRig(const synth_rig::RigScene& sc, const RigTable& rigs, MapperOptions opt,
                     PriorSource& gps, const std::string& events) {
    RigRun r;
    std::map<std::string, uint32_t> id;
    for (uint32_t i = 0; i < sc.names.size(); i++) id[sc.names[i]] = i;
    setEnv("SS_SFM_RIG_DUMP", "1");
    slog::set_sink([&](slog::Tag, slog::Level lv, const std::string& line) {
        static const std::string key = "[rig] frame of ", fired = "[prior] GPS: ";
        if (lv == slog::Level::Diag && line.compare(0, fired.size(), fired) == 0 &&
            line.find("bundle adjusting now") != std::string::npos) {
            auto it = id.find(line.substr(fired.size(), line.find(" is ") - fired.size()));
            if (it != id.end()) r.triggers.push_back(it->second);
            return;
        }
        if (lv != slog::Level::Diag || line.compare(0, key.size(), key) != 0) return;
        if (line.find("-> placed together") == std::string::npos) return;
        auto it = id.find(line.substr(key.size(), line.find(':') - key.size()));
        if (it == id.end()) return;
        r.placed.push_back(it->second);
        r.fitted.push_back(events.find('F') != std::string::npos);
    });
    Mapper m(sc.db, sc.feats, opt, sc.cam_ids, &rigs, nullptr, &gps);
    std::vector<Reconstruction> models = m.run();
    slog::set_sink({});
    setEnv("SS_SFM_RIG_DUMP", nullptr);
    r.st = m.priorStats();
    r.models = models.size();
    if (!models.empty()) r.model = models.front();
    return r;
}

inline bool registered(const Reconstruction& rec, uint32_t img) {
    auto it = rec.images.find(img);
    return it != rec.images.end() && it->second.registered;
}

}  // namespace synth_reg

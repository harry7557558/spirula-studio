// Host-only check of camhost::plan_split_faces: one face per frame, every
// visible ray covered, Uniform one size, PerFace never under half a frame,
// the back frame only when asked for.
// No GPU. Exit code 0 = every check passed.

#include "core/CameraModel.h"
#include "data/CameraMath.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

int g_fail = 0;
#define CHECK(cond, ...)                                                   \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::fprintf(stderr, "  FAIL: " __VA_ARGS__);                  \
            std::fprintf(stderr, "\n");                                    \
            ++g_fail;                                                      \
        }                                                                  \
    } while (0)

constexpr double kPi = 3.14159265358979323846;

struct Case {
    const char* name;
    int model, tier, w, h;
    double fx, fy, cx, cy;
    float dist[kCameraDistortionParams];
    int faces;        // expected count, both fits
    int back_faces;   // and with the back frame admitted
};

const Case kCases[] = {
    {"fisheye 960x960, ~200 deg", 1, 0, 960, 960, 258.5, 258.5, 480, 480, {}, 5, 5},
    {"fisheye 1000x1500 crop + prism", 1, 2, 1000, 1500, 530.4, 530.4, 501.2, 750.4,
     {0.009f, 0.002f, -0.0006f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}, 5, 5},
    {"fisheye 16:9, 120 deg", 1, 0, 1920, 1080, 700, 700, 960, 540, {}, 5, 5},
    {"fisheye 800x800, 100 deg", 1, 0, 800, 800, 458, 458, 400, 400, {}, 5, 5},
    // Square sensor seen past 135 degrees in its corners: the one case whose
    // back frame is admitted, and only when asked for.
    {"equisolid circle", 2, 0, 1400, 1400, 420, 420, 700, 700, {}, 5, 6},
    {"equirect full", 3, 0, 1024, 512, 1024 / (2 * kPi), 1024 / (2 * kPi), 512, 256,
     {}, 6, 6},
    // The polar faces' corners reach down to 35 degrees, so they stay.
    {"equirect 360x90", 3, 0, 1024, 256, 1024 / (2 * kPi), 1024 / (2 * kPi), 512, 128,
     {}, 6, 6},
};

camhost::Camera camera(const Case& c) {
    camhost::Camera cam;
    cam.model = c.model; cam.tier = c.tier;
    cam.width = c.w; cam.height = c.h;
    cam.fx = c.fx; cam.fy = c.fy; cam.cx = c.cx; cam.cy = c.cy;
    std::copy(std::begin(c.dist), std::end(c.dist), std::begin(cam.dist));
    return cam;
}

// Rays the image holds and how many of them land in no face. A fisheye's
// back frame (its cell reaches 125 degrees at the corners) is opt-in.
void coverage(const Case& c, const camhost::Camera& cam,
              const std::vector<camhost::SplitFace>& faces,
              int64_t& seen, int64_t& missed) {
    const bool equi = c.model == (int)CameraModelType::EQUIRECTANGULAR;
    const double* table = equi ? camhost::equirect_face_axes()
                               : camhost::fisheye_face_axes();
    seen = missed = 0;
    for (int y = 0; y < 256; ++y)
        for (int x = 0; x < 256; ++x) {
            const double px = (x + 0.5) / 256 * c.w, py = (y + 0.5) / 256 * c.h;
            double r[3], back[2];
            if (!camhost::generate_ray((px - c.cx) / c.fx, (py - c.cy) / c.fy,
                                       c.model, c.tier, c.dist, r))
                continue;
            if (!camhost::ray_in_frame(cam, r, back)) continue;
            if (!equi && -r[2] >= std::max(std::fabs(r[0]), std::fabs(r[1]))) continue;
            ++seen;
            bool hit = false;
            for (const camhost::SplitFace& f : faces) {
                const double* a = table + 9 * f.face;
                const double z = a[6]*r[0] + a[7]*r[1] + a[8]*r[2];
                if (z <= 1e-12) continue;
                const double u = (a[0]*r[0] + a[1]*r[1] + a[2]*r[2]) / z;
                const double v = (a[3]*r[0] + a[4]*r[1] + a[5]*r[2]) / z;
                const double fpx = u * f.fx + f.cx, fpy = v * f.fy + f.cy;
                if (fpx >= 0 && fpx <= f.width && fpy >= 0 && fpy <= f.height) {
                    hit = true;
                    break;
                }
            }
            if (!hit) ++missed;
        }
}

double pixels(const std::vector<camhost::SplitFace>& faces) {
    double px = 0;
    for (const camhost::SplitFace& f : faces) px += (double)f.width * f.height;
    return px;
}

void check_plan(const Case& c, const camhost::Camera& cam, camhost::FaceFit fit,
                const std::vector<camhost::SplitFace>& faces, int expect,
                const char* fname) {
    CHECK((int)faces.size() == expect, "%s %s: %d faces, expected %d", c.name,
          fname, (int)faces.size(), expect);
    if (faces.empty()) return;
    const int half = (int)std::lround(faces[0].fx);
    bool used[6] = {};
    for (size_t i = 0; i < faces.size(); ++i) {
        const camhost::SplitFace& f = faces[i];
        CHECK(f.face >= 0 && f.face < 6 && !used[f.face],
              "%s %s: frame %d used twice", c.name, fname, f.face);
        if (f.face >= 0 && f.face < 6) used[f.face] = true;
        CHECK(f.fx == faces[0].fx && f.fy == f.fx, "%s %s: focal differs", c.name, fname);
        CHECK(f.width >= half && f.width <= 2 * half &&
              f.height >= half && f.height <= 2 * half,
              "%s %s: face %d is %dx%d, half frame %d", c.name, fname, f.face,
              f.width, f.height, half);
        CHECK(f.cx <= half && f.width - f.cx <= half &&
              f.cy <= half && f.height - f.cy <= half,
              "%s %s: face %d leaves its frame", c.name, fname, f.face);
        CHECK(f.crop_w <= f.width && f.crop_h <= f.height,
              "%s %s: face %d smaller than its crop", c.name, fname, f.face);
        if (fit == camhost::FaceFit::Uniform)
            CHECK(f.width == faces[0].width && f.height == faces[0].height,
                  "%s uniform: face %d is %dx%d, not %dx%d", c.name, f.face,
                  f.width, f.height, faces[0].width, faces[0].height);
        // Sizes descend, so equal sizes are adjacent and a pass is one run.
        if (i > 0)
            CHECK(faces[i - 1].height > f.height ||
                  (faces[i - 1].height == f.height && faces[i - 1].width >= f.width),
                  "%s %s: sizes out of order at face %d", c.name, fname, f.face);
    }
    int64_t seen = 0, missed = 0;
    coverage(c, cam, faces, seen, missed);
    CHECK(seen > 0 && missed <= seen / 500 + 1, "%s %s: %lld of %lld rays uncovered",
          c.name, fname, (long long)missed, (long long)seen);
    const int K = c.model == (int)CameraModelType::EQUIRECTANGULAR ? 6 : 5;
    const double uncropped = (double)K * (2 * half) * (2 * half);
    CHECK(pixels(faces) <= 6.0 * (2 * half) * (2 * half) + 1,
          "%s %s: more pixels than a cube", c.name, fname);
    std::printf("  %-32s %-8s %d faces", c.name, fname, (int)faces.size());
    for (const camhost::SplitFace& f : faces)
        std::printf(" %dx%d", f.width, f.height);
    std::printf("  %.0f%% of uncropped  uncovered %lld/%lld\n",
                100.0 * pixels(faces) / uncropped, (long long)missed, (long long)seen);
}

}  // namespace

int main() {
    for (const Case& c : kCases) {
        const camhost::Camera cam = camera(c);
        const auto uni = camhost::plan_split_faces(cam, camhost::FaceFit::Uniform);
        const auto per = camhost::plan_split_faces(cam, camhost::FaceFit::PerFace);
        const auto back =
            camhost::plan_split_faces(cam, camhost::FaceFit::Uniform, true);
        check_plan(c, cam, camhost::FaceFit::Uniform, uni, c.faces, "uniform");
        check_plan(c, cam, camhost::FaceFit::PerFace, per, c.faces, "per-face");
        check_plan(c, cam, camhost::FaceFit::Uniform, back, c.back_faces, "back");
        CHECK(pixels(per) <= pixels(uni) + 1, "%s: per-face draws more than uniform", c.name);
    }
    std::printf("%s\n", g_fail ? "FAILURES" : "split planner ok");
    return g_fail ? 1 : 0;
}

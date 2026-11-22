// Baking a placement into a model must not change what it looks like: render
// a scene, move the splats AND the camera by one similarity, render again.
// Every attribute checkpoint/SplatTransform.h touches is on that path -- the
// means, the orientation, the log scales, the SH bands, whose rotation is the
// part a wrong sign survives everywhere except here, and the spherical-beta
// lobes. The control renders the moved model with its colour
// rows left alone and must NOT match.
//
//   ./splat_transform_render        (either backend, no reference file)

#include <checkpoint/SplatPly.h>
#include <checkpoint/SplatTransform.h>
#include <engine/Engine.h>
#include <engine/EngineState.h>

#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

static constexpr int64_t N = 3000;
static constexpr int W = 200, H = 150;

static TorchTensorView ttv(const void* p, std::vector<int64_t> shape) {
    return std::make_tuple((uint64_t)p, (uint32_t)4, std::move(shape));
}
static TorchTensorView ttv_null() {
    return std::make_tuple((uint64_t)0, 4u, std::vector<int64_t>{0});
}

static int g_failures = 0;
static void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

static std::vector<float> render(const spirula::SplatCloud& c,
                                 const std::vector<float>& viewmat,
                                 const char* prim, const char* cam, int degree) {
    const int64_t K = c.coeffs();
    // set_data_3dgs is a no-op on an initialized world.
    engine_reset();
    set_data_3dgs(c.num, ttv(c.means.data(), {c.num, 3}),
                  ttv(c.quats.data(), {c.num, 4}),
                  ttv(c.scales.data(), {c.num, 3}),
                  ttv(c.opacities.data(), {c.num, 1}),
                  ttv(c.features_dc.data(), {c.num, 3}),
                  ttv(c.features_sh.data(), {c.num, K, 3}), c.sb_lobes);
    const std::vector<float> intr = {150, 150, W * 0.5f, H * 0.5f};
    const std::vector<float> dist(kCameraDistortionParams, 0.0f);
    set_camera_params(W, H, cam, "NONE", ttv(viewmat.data(), {1, 4, 4}),
                      ttv(intr.data(), {1, 4}),
                      ttv(dist.data(), {1, kCameraDistortionParams}));
    forward_3dgs(prim, degree, false, false, 0);
    backend::device_synchronize();
    std::vector<float> rgb((size_t)H * W * 3);
    engine_copy_render_to_host(ttv(rgb.data(), {1, H, W, 3}), ttv_null(),
                               ttv_null(), ttv_null(), ttv_null());
    return rgb;
}

int main() {
    std::mt19937 rng(20260921u);
    auto uf = [&](float lo, float hi) {
        return lo + (hi - lo) * (float)(rng() & 0xffffff) / 16777215.0f;
    };

    struct Basis { int degree, lobes; };
    std::vector<Basis> bases = {{3, 0}, {4, 0}, {0, 2}, {3, 4}};
    for (const Basis& basis : bases) {
        const int degree = basis.degree;
        spirula::SplatCloud c;
        c.num = N;
        c.sh_degree = degree;
        c.sb_lobes = basis.lobes;
        const int64_t K = c.coeffs();
        c.means.resize(N * 3); c.quats.resize(N * 4); c.scales.resize(N * 3);
        c.opacities.resize(N); c.features_dc.resize(N * 3);
        c.features_sh.resize(N * K * 3);
        for (int64_t i = 0; i < N; i++) {
            c.means[3*i+0] = uf(-2.f, 2.f);
            c.means[3*i+1] = uf(-1.5f, 1.5f);
            c.means[3*i+2] = uf(-1.f, 1.f);
            for (int k = 0; k < 4; k++) c.quats[4*i+k] = uf(-1.f, 1.f);
            // Anisotropic on purpose: an isotropic Gaussian cannot tell a
            // right orientation from a wrong one.
            c.scales[3*i+0] = uf(-4.5f, -2.0f);
            c.scales[3*i+1] = uf(-4.5f, -2.0f);
            c.scales[3*i+2] = uf(-6.0f, -4.0f);
            c.opacities[i] = uf(-1.f, 3.f);
            for (int k = 0; k < 3; k++) c.features_dc[3*i+k] = uf(0.f, 1.5f);
        }
        // Strong view dependence, so a wrong band matrix is a wrong image;
        // lobes bright and sharp enough that a wrong axis is one too.
        for (float& v : c.features_sh) v = uf(-0.6f, 0.6f);
        for (int64_t i = 0; i < N; i++) {
            float* lobe =
                c.features_sh.data() + (i * K + c.basis().sh_coeffs()) * 3;
            for (int l = 0; l < c.sb_lobes; l++, lobe += 6) {
                for (int k = 0; k < 3; k++) lobe[k] = uf(-0.3f, 0.6f);
                lobe[3] = uf(0.f, 3.1416f);
                lobe[4] = uf(0.f, 6.2832f);
                lobe[5] = uf(-1.f, 2.f);
            }
        }
        // Unit, as FusedGeometryOptim.cu leaves them after every step: the
        // 3dgut rasterizer builds its rotation from the stored value as is.
        for (int64_t i = 0; i < N; i++) {
            float n = 0;
            for (int k = 0; k < 4; k++) n += c.quats[4*i+k] * c.quats[4*i+k];
            n = std::sqrt(n);
            for (int k = 0; k < 4; k++) c.quats[4*i+k] /= n;
        }

        // Camera 5 units back along -z of the world, looking at the origin.
        const std::vector<float> V = {1, 0, 0, 0.1f,  0, 1, 0, -0.05f,
                                      0, 0, 1, 5.0f,  0, 0, 0, 1};

        const double axis[3] = {0.48, -0.6, 0.64};
        const double pivot[3] = {0.3, -0.2, 0.1};
        spirula::Sim3 T = spirula::Sim3::rotation_about(axis, 1.1, pivot);
        T.s = 1.7;
        T.t[0] += 0.4; T.t[1] -= 0.7; T.t[2] += 0.25;

        // x_cam = V T^-1 x', times s so the camera stays rigid: a pinhole
        // does not see a uniform scale of camera space.
        const spirula::Sim3 Ti = T.inverse();
        std::vector<float> V2(16, 0.0f);
        V2[15] = 1.0f;
        for (int r = 0; r < 3; r++) {
            for (int col = 0; col < 3; col++) {
                double v = 0;
                for (int k = 0; k < 3; k++) v += V[r*4+k] * Ti.R[k*3+col];
                V2[r*4+col] = (float)v;
            }
            double t = 0;
            for (int k = 0; k < 3; k++) t += V[r*4+k] * Ti.t[k];
            V2[r*4+3] = (float)(T.s * (t + V[r*4+3]));
        }

        spirula::SplatCloud moved = c;
        spirula::transform_splats(moved, T);
        spirula::SplatCloud stale = moved;
        stale.features_sh = c.features_sh;

        const struct { const char* prim; const char* cam; } cfgs[] = {
            {"3dgs", "PINHOLE"}, {"mip", "PINHOLE"}, {"3dgut", "PINHOLE"},
            {"3dgs", "FISHEYE"}};
        for (const auto& cfg : cfgs) {
            const std::vector<float> a = render(c, V, cfg.prim, cfg.cam, degree);
            const std::vector<float> b = render(moved, V2, cfg.prim, cfg.cam, degree);
            const std::vector<float> s = render(stale, V2, cfg.prim, cfg.cam, degree);
            if (const char* err = backend::last_error()) {
                std::fprintf(stderr, "backend error: %s\n", err);
                return 1;
            }
            double worst = 0, mean = 0, lit = 0, stale_mean = 0;
            for (size_t i = 0; i < a.size(); i++) {
                const double d = std::fabs((double)a[i] - b[i]);
                worst = std::max(worst, d);
                mean += d;
                lit += std::fabs(a[i]);
                stale_mean += std::fabs((double)a[i] - s[i]);
            }
            mean /= (double)a.size();
            lit /= (double)a.size();
            stale_mean /= (double)a.size();
            std::printf("     SH %d SB %d %-5s %-8s  mean |d| %.2e  max %.2e  "
                        "(image mean %.3f; colour left alone: %.2e)\n",
                        degree, c.sb_lobes, cfg.prim, cfg.cam, mean, worst, lit,
                        stale_mean);
            const std::string tag = std::string("SH ") + std::to_string(degree) +
                                    " SB " + std::to_string(c.sb_lobes) + " " +
                                    cfg.prim + " " + cfg.cam;
            check(lit > 0.05, tag + ": the render is not empty");
            check(mean < 2e-4 && worst < 2e-2, tag + ": moved model = same image");
            check(stale_mean > 20.0 * std::max(mean, 1e-6),
                  tag + ": un-rotated colour is visibly wrong");
        }
    }
    std::printf("%s\n", g_failures ? "FAILED" : "all passed");
    return g_failures ? 1 : 0;
}

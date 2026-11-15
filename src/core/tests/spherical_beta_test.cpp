// Spherical-beta lobes on the host (core/SphericalBeta.h, checkpoint/SplatPly.h):
// a turned lobe seen from the turned direction is the lobe it was, and a PLY
// carries lobes in the layout Deformable Beta Splatting's code, gsplat and
// PlayCanvas read -- sb_params_{f * L + l}, field f of lobe l, after f_rest --
// which a hand-written file pins independently of our own writer.

#include "checkpoint/SplatPly.h"
#include "checkpoint/SplatTransform.h"
#include "core/SphericalBeta.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace sb = spirula::sb;

static int g_failures = 0;
static void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

static void random_rotation(std::mt19937& rng, double R[9]) {
    std::normal_distribution<double> g(0.0, 1.0);
    double q[4] = {g(rng), g(rng), g(rng), g(rng)};
    const double n = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    for (double& v : q) v /= n;
    const double w = q[0], x = q[1], y = q[2], z = q[3];
    const double M[9] = {1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y),
                         2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x),
                         2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y)};
    for (int i = 0; i < 9; i++) R[i] = M[i];
}

int main() {
    std::mt19937 rng(7u);
    std::uniform_real_distribution<float> u(0.f, 1.f);

    // ---- a turned lobe from a turned direction is the same colour ----
    double worst = 0.0, lit = 0.0;
    for (int trial = 0; trial < 2000; trial++) {
        float lobe[sb::kLobeFloats] = {u(rng) - 0.3f, u(rng) - 0.3f, u(rng) - 0.3f,
                                       3.1416f * u(rng), 6.2832f * u(rng) - 3.0f,
                                       2.0f * u(rng) - 0.5f};
        double R[9], d[3], Rd[3];
        random_rotation(rng, R);
        float turned[sb::kLobeFloats];
        std::copy(lobe, lobe + sb::kLobeFloats, turned);
        sb::rotate_lobe(R, turned);
        // A direction near the lobe's axis, so the colour is not just zero.
        double a[3];
        sb::axis(lobe[3], lobe[4], a);
        double n = 0;
        for (int k = 0; k < 3; k++) { d[k] = a[k] + 0.4 * (u(rng) - 0.5); n += d[k] * d[k]; }
        for (int k = 0; k < 3; k++) d[k] /= std::sqrt(n);
        for (int r = 0; r < 3; r++) Rd[r] = R[3 * r] * d[0] + R[3 * r + 1] * d[1] + R[3 * r + 2] * d[2];
        double c0[3], c1[3];
        sb::lobe_color(lobe, d, c0);
        sb::lobe_color(turned, Rd, c1);
        for (int k = 0; k < 3; k++) worst = std::max(worst, std::fabs(c0[k] - c1[k]));
        lit += c0[0];
    }
    std::printf("     rotated lobe, worst colour difference %.2e\n", worst);
    check(lit / 2000.0 > 1e-2, "the probe directions light the lobes");
    check(worst < 1e-5, "colour'(R d) = colour(d) for a turned lobe");

    // A fresh lobe's axis is uniform on the sphere: its mean is the origin.
    double mean_axis[3] = {0, 0, 0};
    bool fresh_ok = true;
    for (int i = 0; i < 20000; i++) {
        float lobe[sb::kLobeFloats];
        sb::init_lobe(u(rng), u(rng), lobe);
        double a[3];
        sb::axis(lobe[3], lobe[4], a);
        for (int k = 0; k < 3; k++) mean_axis[k] += a[k] / 20000.0;
        fresh_ok = fresh_ok && lobe[0] == 0.f && lobe[5] == 0.f;
    }
    check(fresh_ok, "a fresh lobe has amplitude 0, sharpness 0");
    check(std::fabs(mean_axis[0]) < 0.02 && std::fabs(mean_axis[1]) < 0.02 &&
              std::fabs(mean_axis[2]) < 0.02,
          "fresh lobe axes cover the sphere evenly");
    check(std::fabs(sb::softplus(0.0) - 0.1) < 1e-12, "softplus(0) = 0.1, the trainers' sharpness");
    float dark[sb::kLobeFloats];
    sb::dark_lobe(dark);
    check(sb::softplus(dark[0]) < 1e-20, "a dark lobe adds nothing");

    // ---- the published layout, written by hand, read by us ----
    const fs::path dir = fs::temp_directory_path() /
                         ("spirula_sb_test_" + std::to_string(rng()));
    fs::create_directories(dir);
    const int L = 2, Ksh = 3;  // SH degree 1
    const int n = 4;
    {
        std::ofstream f(dir / "hand.ply", std::ios::binary);
        f << "ply\nformat binary_little_endian 1.0\nelement vertex " << n << "\n";
        for (const char* p : {"x", "y", "z", "f_dc_0", "f_dc_1", "f_dc_2"})
            f << "property float " << p << "\n";
        for (int j = 0; j < 3 * Ksh; j++) f << "property float f_rest_" << j << "\n";
        for (int j = 0; j < 6 * L; j++) f << "property float sb_params_" << j << "\n";
        for (const char* p : {"opacity", "scale_0", "scale_1", "scale_2",
                              "rot_0", "rot_1", "rot_2", "rot_3"})
            f << "property float " << p << "\n";
        f << "end_header\n";
        for (int i = 0; i < n; i++) {
            std::vector<float> row;
            for (int k = 0; k < 6; k++) row.push_back((float)(i * 100 + k));
            for (int ch = 0; ch < 3; ch++)          // f_rest: channel-major
                for (int j = 0; j < Ksh; j++) row.push_back((float)(i * 100 + 10 + 3 * j + ch));
            for (int fld = 0; fld < 6; fld++)       // sb_params: field-major
                for (int l = 0; l < L; l++) row.push_back((float)(i * 100 + 50 + 6 * l + fld));
            for (int k = 0; k < 8; k++) row.push_back(1.0f);
            f.write((const char*)row.data(), (std::streamsize)(row.size() * sizeof(float)));
        }
    }
    spirula::SplatCloud c = spirula::read_splat_ply((dir / "hand.ply").string());
    check(c.sh_degree == 1 && c.sb_lobes == L, "SH degree 1 and two lobes found");
    bool layout_ok = c.coeffs() == Ksh + 2 * L;
    for (int i = 0; i < n && layout_ok; i++) {
        const float* row = &c.features_sh[(size_t)(i * c.coeffs() * 3)];
        for (int j = 0; j < Ksh; j++)
            for (int ch = 0; ch < 3; ch++)
                layout_ok = layout_ok && row[3 * j + ch] == (float)(i * 100 + 10 + 3 * j + ch);
        for (int l = 0; l < L; l++)
            for (int fld = 0; fld < 6; fld++)
                layout_ok = layout_ok &&
                            row[3 * Ksh + 6 * l + fld] == (float)(i * 100 + 50 + 6 * l + fld);
    }
    check(layout_ok, "bands and lobes land coefficient-major in the row");
    bool short_ok = false;
    try {
        spirula::SplatCloud m = spirula::read_splat_ply((dir / "hand.ply").string(), false);
        short_ok = m.sb_lobes == 0 && m.features_sh.empty();
    } catch (...) {}
    check(short_ok, "want_sh = false skips the lobes too");

    // ---- our writer is the inverse of our reader ----
    spirula::write_splat_ply(c, (dir / "out.ply").string());
    const spirula::SplatCloud back = spirula::read_splat_ply((dir / "out.ply").string());
    check(back.basis() == c.basis() && back.features_sh == c.features_sh &&
              back.means == c.means && back.opacities == c.opacities,
          "write then read gives the same cloud");
    {
        int64_t seen = 0;
        bool runs_ok = true;
        auto same = [&](const std::vector<float>& run, const std::vector<float>& all,
                        int64_t first, int64_t width) {
            return run.size() % (size_t)std::max<int64_t>(width, 1) == 0 &&
                   std::equal(run.begin(), run.end(), all.begin() + first * width);
        };
        spirula::read_splat_ply_chunks((dir / "out.ply").string(), 2,
            [&](const spirula::SplatCloud& r, int64_t first, int64_t total) {
                const int64_t K = back.coeffs();
                runs_ok = runs_ok && first == seen && total == back.num &&
                          r.basis() == back.basis() && r.num <= 2 &&
                          same(r.means, back.means, first, 3) &&
                          same(r.quats, back.quats, first, 4) &&
                          same(r.scales, back.scales, first, 3) &&
                          same(r.opacities, back.opacities, first, 1) &&
                          same(r.features_dc, back.features_dc, first, 3) &&
                          same(r.features_sh, back.features_sh, first, K * 3);
                seen += r.num;
            });
        check(runs_ok && seen == back.num, "a chunked read gives the same rows");
    }

    // ---- SplatRenderMode tag for mip ----
    const std::string hdr_mip = spirula::splat_ply_header(10, c.basis(), "mip");
    const std::string hdr_3dgs = spirula::splat_ply_header(10, c.basis(), "3dgs");
    check(hdr_mip.find("comment SplatRenderMode: mip\n") != std::string::npos &&
              hdr_3dgs.find("comment") == std::string::npos,
          "splat_ply_header tags mip and leaves 3dgs untagged");
    spirula::SplatCloud c_mip = c;
    c_mip.primitive = "mip";
    spirula::write_splat_ply(c_mip, (dir / "mip.ply").string());
    const spirula::SplatCloud back_mip = spirula::read_splat_ply((dir / "mip.ply").string());
    check(back_mip.primitive == "mip", "mip tag roundtrips through PLY header");

    // ---- a placement baked on save turns the lobes ----
    spirula::Sim3 T;
    random_rotation(rng, T.R);
    const spirula::SplatTransform xf(T, c.basis());
    spirula::write_splat_ply(c, (dir / "moved.ply").string(), nullptr, &xf);
    const spirula::SplatCloud moved = spirula::read_splat_ply((dir / "moved.ply").string());
    bool moved_ok = true;
    for (int i = 0; i < n; i++)
        for (int l = 0; l < L; l++) {
            float want[sb::kLobeFloats];
            const float* src = &c.features_sh[(size_t)((i * c.coeffs() + Ksh) * 3 + 6 * l)];
            std::copy(src, src + sb::kLobeFloats, want);
            sb::rotate_lobe(T.R, want);
            const float* got = &moved.features_sh[(size_t)((i * c.coeffs() + Ksh) * 3 + 6 * l)];
            for (int k = 0; k < sb::kLobeFloats; k++)
                moved_ok = moved_ok && std::fabs(got[k] - want[k]) < 1e-5f;
        }
    check(moved_ok, "saving with a placement rotates every lobe");

    fs::remove_all(dir);
    std::printf("%s\n", g_failures ? "FAILED" : "all passed");
    return g_failures ? 1 : 0;
}

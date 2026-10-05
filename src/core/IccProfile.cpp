// ICC profile reading -- see core/IccProfile.h. Tag layouts are ICC.1:2010
// section 10; every multi-byte field is big-endian.

#include "core/IccProfile.h"

#include "core/ColorSpace.h"

#include <array>
#include <cmath>
#include <vector>

namespace icc {
namespace {

constexpr uint32_t sig(const char (&s)[5]) {
    return (uint32_t)(uint8_t)s[0] << 24 | (uint32_t)(uint8_t)s[1] << 16 |
           (uint32_t)(uint8_t)s[2] << 8 | (uint32_t)(uint8_t)s[3];
}

// Out of range reads 0 and clears `ok`, so a truncated profile fails.
struct Bytes {
    const uint8_t* p = nullptr;
    size_t n = 0;
    bool ok = true;

    uint32_t u32(size_t o) {
        if (o > n || n - o < 4) { ok = false; return 0; }
        return (uint32_t)p[o] << 24 | (uint32_t)p[o + 1] << 16 | (uint32_t)p[o + 2] << 8 |
               (uint32_t)p[o + 3];
    }
    uint16_t u16(size_t o) {
        if (o > n || n - o < 2) { ok = false; return 0; }
        return (uint16_t)(p[o] << 8 | p[o + 1]);
    }
    double s15f16(size_t o) { return (int32_t)u32(o) / 65536.0; }
};

struct Tag {
    size_t offset = 0, size = 0;
    bool found = false;
};

struct Curve {
    std::vector<double> table;  // curv with two or more entries
    int fn = 0;                 // parametric function type 0..4
    double g = 1, a = 1, b = 0, c = 0, d = 0, e = 0, f = 0;

    double operator()(double x) const {
        if (!table.empty()) {
            const double t = std::min(std::max(x, 0.0), 1.0) * (double)(table.size() - 1);
            const size_t i = std::min((size_t)t, table.size() - 2);
            return table[i] + (table[i + 1] - table[i]) * (t - (double)i);
        }
        switch (fn) {
            case 1: return x >= -b / a ? std::pow(a * x + b, g) : 0.0;
            case 2: return x >= -b / a ? std::pow(a * x + b, g) + c : c;
            case 3: return x >= d ? std::pow(a * x + b, g) : c * x;
            case 4: return x >= d ? std::pow(a * x + b, g) + e : c * x + f;
            default: return std::pow(x, g);
        }
    }

    bool identity() const {
        for (int k = 1; k < 20; k++)
            if (std::fabs((*this)(k / 20.0) - k / 20.0) > 0.01) return false;
        return true;
    }
};

bool read_curve(Bytes& r, const Tag& t, Curve& c) {
    if (!t.found) return false;
    const size_t o = t.offset;
    const uint32_t type = r.u32(o);
    if (type == sig("curv")) {
        const uint32_t count = r.u32(o + 8);
        if (count > (r.n - o) / 2) return false;
        if (count == 1) c.g = r.u16(o + 12) / 256.0;
        if (count >= 2) {
            c.table.resize(count);
            for (uint32_t i = 0; i < count; i++) c.table[i] = r.u16(o + 12 + 2 * i) / 65535.0;
        }
        return r.ok;
    }
    if (type != sig("para")) return false;
    c.fn = r.u16(o + 8);
    static const int kParams[] = {1, 3, 4, 5, 7};
    if (c.fn > 4) return false;
    double v[7] = {1, 1, 0, 0, 0, 0, 0};
    for (int i = 0; i < kParams[c.fn]; i++) v[i] = r.s15f16(o + 12 + 4 * (size_t)i);
    c.g = v[0]; c.a = v[1]; c.b = v[2]; c.c = v[3]; c.d = v[4]; c.e = v[5]; c.f = v[6];
    if (c.fn != 0 && c.a == 0.0) return false;
    return r.ok;
}

using M3 = std::array<double, 9>;

M3 mul(const M3& x, const M3& y) {
    M3 o{};
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            for (int k = 0; k < 3; k++) o[i * 3 + j] += x[i * 3 + k] * y[k * 3 + j];
    return o;
}

bool inv(const M3& m, M3& o) {
    const double det = m[0] * (m[4] * m[8] - m[5] * m[7]) - m[1] * (m[3] * m[8] - m[5] * m[6]) +
                       m[2] * (m[3] * m[7] - m[4] * m[6]);
    if (std::fabs(det) < 1e-12) return false;
    o = {(m[4] * m[8] - m[5] * m[7]) / det, (m[2] * m[7] - m[1] * m[8]) / det,
         (m[1] * m[5] - m[2] * m[4]) / det, (m[5] * m[6] - m[3] * m[8]) / det,
         (m[0] * m[8] - m[2] * m[6]) / det, (m[2] * m[3] - m[0] * m[5]) / det,
         (m[3] * m[7] - m[4] * m[6]) / det, (m[1] * m[6] - m[0] * m[7]) / det,
         (m[0] * m[4] - m[1] * m[3]) / det};
    return true;
}

// RGB -> XYZ with the white at Y = 1, from CIE xy primaries and white.
bool rgb_to_xyz(const float* xy, M3& out) {
    M3 p{};
    for (int i = 0; i < 3; i++) {
        const double x = xy[2 * i], y = xy[2 * i + 1];
        if (std::fabs(y) < 1e-9) return false;
        p[0 * 3 + i] = x / y;
        p[1 * 3 + i] = 1.0;
        p[2 * 3 + i] = (1.0 - x - y) / y;
    }
    const double w[3] = {xy[6] / xy[7], 1.0, (1.0 - xy[6] - xy[7]) / xy[7]};
    M3 pi;
    if (!inv(p, pi)) return false;
    for (int i = 0; i < 3; i++) {
        const double s = pi[i * 3] * w[0] + pi[i * 3 + 1] * w[1] + pi[i * 3 + 2] * w[2];
        for (int r = 0; r < 3; r++) p[r * 3 + i] *= s;
    }
    out = p;
    return true;
}

// Bradford adaptation of an RGB -> XYZ matrix to the ICC PCS white, D50.
M3 adapt_to_d50(const M3& m) {
    static const M3 kBradford = {0.8951, 0.2664, -0.1614, -0.7502, 1.7135,
                                 0.0367, 0.0389, -0.0685, 1.0296};
    const double src[3] = {m[0] + m[1] + m[2], m[3] + m[4] + m[5], m[6] + m[7] + m[8]};
    const double dst[3] = {0.9642, 1.0, 0.8249};
    M3 ki;
    inv(kBradford, ki);
    M3 scale{};
    for (int i = 0; i < 3; i++) {
        const double s = kBradford[i * 3] * src[0] + kBradford[i * 3 + 1] * src[1] +
                         kBradford[i * 3 + 2] * src[2];
        const double d = kBradford[i * 3] * dst[0] + kBradford[i * 3 + 1] * dst[1] +
                         kBradford[i * 3 + 2] * dst[2];
        scale[i * 4] = d / s;
    }
    return mul(mul(ki, scale), mul(kBradford, m));
}

bool is_near(const M3& a, const M3& b) {
    for (int i = 0; i < 9; i++)
        if (std::fabs(a[i] - b[i]) > 0.003) return false;
    return true;
}

// Colorants are normally Bradford-adapted to D50; some older profiles store
// them at the source white instead, which is the second comparison.
void match_gamut(const M3& colorants, ColorSpace& out) {
    for (const colorspace::GamutPrimaries& g : colorspace::kGamutPrimaries) {
        M3 m;
        if (!rgb_to_xyz(g.xy, m)) continue;
        if (is_near(colorants, adapt_to_d50(m)) || is_near(colorants, m)) {
            out.gamut = std::string(g.name) == "Rec.709" ? "" : g.name;
            return;
        }
    }
    out.gamut_known = false;
}

}  // namespace

bool read(const uint8_t* p, size_t n, ColorSpace& out) {
    out = ColorSpace();
    Bytes r{p, n};
    if (n < 132 || r.u32(36) != sig("acsp")) return false;
    const uint32_t space = r.u32(16);
    if (space != sig("RGB ") && space != sig("GRAY")) return false;

    const uint32_t count = r.u32(128);
    if (count > (n - 132) / 12) return false;
    Tag rx, gx, bx, rt, gt, bt, kt;
    for (uint32_t i = 0; i < count; i++) {
        const size_t e = 132 + 12 * (size_t)i;
        Tag t;
        t.offset = r.u32(e + 4);
        t.size = r.u32(e + 8);
        t.found = t.offset <= n && t.size <= n - t.offset && t.size >= 12;
        switch (r.u32(e)) {
            case sig("rXYZ"): rx = t; break;
            case sig("gXYZ"): gx = t; break;
            case sig("bXYZ"): bx = t; break;
            case sig("rTRC"): rt = t; break;
            case sig("gTRC"): gt = t; break;
            case sig("bTRC"): bt = t; break;
            case sig("kTRC"): kt = t; break;
            default: break;
        }
    }
    if (!r.ok) return false;

    if (space == sig("GRAY")) {
        Curve k;
        if (!read_curve(r, kt, k)) return false;
        out.grey = true;
        out.is_linear = k.identity();
        return true;
    }
    if (r.u32(20) != sig("XYZ ")) return false;
    Curve c[3];
    if (!read_curve(r, rt, c[0]) || !read_curve(r, gt, c[1]) || !read_curve(r, bt, c[2]))
        return false;
    M3 colorants;
    const Tag* xyz[3] = {&rx, &gx, &bx};
    for (int i = 0; i < 3; i++) {
        if (!xyz[i]->found || r.u32(xyz[i]->offset) != sig("XYZ ")) return false;
        for (int k = 0; k < 3; k++)
            colorants[k * 3 + i] = r.s15f16(xyz[i]->offset + 8 + 4 * (size_t)k);
    }
    if (!r.ok) return false;
    out.is_linear = c[0].identity() && c[1].identity() && c[2].identity();
    match_gamut(colorants, out);
    return true;
}

}  // namespace icc

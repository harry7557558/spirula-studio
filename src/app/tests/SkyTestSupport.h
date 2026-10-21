#pragma once

#include "app/SkyModel.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ostream>
#include <string>
#include <vector>

namespace spirula::sky::test {

inline int g_failures = 0;

inline void expect(bool ok, const std::string& what) {
    std::printf("%s  %s\n", ok ? "ok  " : "BAD ", what.c_str());
    if (!ok) g_failures++;
}

inline uint32_t bits(float v) {
    uint32_t u = 0;
    std::memcpy(&u, &v, sizeof u);
    return u;
}

inline float from_bits(uint32_t u) {
    float v = 0.0f;
    std::memcpy(&v, &u, sizeof v);
    return v;
}

// A camera at `t` looking down CV +Z; c2w columns are the OpenGL camera axes.
inline spirula::sky::View view_at(const double t[3], double fx, double fy, int w, int h) {
    spirula::sky::View v;
    v.width = w;
    v.height = h;
    v.model = 0;
    v.tier = 0;
    v.fx = fx;
    v.fy = fy;
    v.cx = w / 2.0;
    v.cy = h / 2.0;
    v.c2w[0] = 1.0; v.c2w[1] = 0.0; v.c2w[2] = 0.0; v.c2w[3] = t[0];
    v.c2w[4] = 0.0; v.c2w[5] = -1.0; v.c2w[6] = 0.0; v.c2w[7] = t[1];
    v.c2w[8] = 0.0; v.c2w[9] = 0.0; v.c2w[10] = -1.0; v.c2w[11] = t[2];
    return v;
}

inline spirula::sky::Rows rows_of(const std::vector<float>& xyz) {
    spirula::sky::Rows r;
    r.radius = 1.0;
    r.means = xyz;
    r.features_dc.assign(xyz.size(), 0.0f);
    return r;
}

template<class T>
bool same_bytes(const std::vector<T>& a, const std::vector<T>& b) {
    return a.size() == b.size() &&
        (a.empty() || std::memcmp(a.data(), b.data(), a.size() * sizeof(T)) == 0);
}

template<class T>
void snapshot(std::ostream* out, const std::vector<T>& values) {
    if (!out) return;
    const uint64_t size = values.size();
    out->write((const char*)&size, sizeof size);
    if (size) out->write((const char*)values.data(), size * sizeof(T));
}

}  // namespace spirula::sky::test

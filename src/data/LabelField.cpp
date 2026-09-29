// LabelField.cpp -- see LabelField.h; the metric mirrors shaders/region.slang.

#include "data/LabelField.h"

#include "data/RegionProgram.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace spirula {

// ===========================================================================
// Packing
// ===========================================================================

uint32_t LabelField::oct_encode(const float dir[3]) {
    const float l1 = std::fabs(dir[0]) + std::fabs(dir[1]) + std::fabs(dir[2]);
    if (!(l1 > 1e-12f)) return kOmni;
    float u = dir[0] / l1, v = dir[1] / l1;
    if (dir[2] < 0) {
        const float ou = u, ov = v;
        u = (1.0f - std::fabs(ov)) * (ou >= 0 ? 1.0f : -1.0f);
        v = (1.0f - std::fabs(ou)) * (ov >= 0 ? 1.0f : -1.0f);
    }
    const uint32_t a = (uint32_t)std::lround((u * 0.5f + 0.5f) * 4095.0f);
    const uint32_t b = (uint32_t)std::lround((v * 0.5f + 0.5f) * 4095.0f);
    const uint32_t bits = (std::min(a, 4095u)) | (std::min(b, 4095u) << 12);
    return bits == kOmni ? bits - 1 : bits;
}

void LabelField::oct_decode(uint32_t bits, float out[3]) {
    const float u = (float)(bits & 0xFFFu) / 4095.0f * 2.0f - 1.0f;
    const float v = (float)((bits >> 12) & 0xFFFu) / 4095.0f * 2.0f - 1.0f;
    float n[3] = {u, v, 1.0f - std::fabs(u) - std::fabs(v)};
    const float t = std::max(-n[2], 0.0f);
    n[0] += n[0] >= 0.0f ? -t : t;
    n[1] += n[1] >= 0.0f ? -t : t;
    const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    for (int k = 0; k < 3; k++) out[k] = n[k] / (len > 0 ? len : 1.0f);
}

uint32_t LabelField::pack(int label, const float dir[3]) {
    const uint32_t dir_bits = dir ? oct_encode(dir) : kOmni;
    return ((uint32_t)std::clamp(label, 0, 255)) | (dir_bits << 8);
}

namespace {

uint32_t bits_of(float f) {
    uint32_t u;
    std::memcpy(&u, &f, 4);
    return u;
}
float float_of(uint32_t u) {
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}

}  // namespace

// ===========================================================================
// Build
// ===========================================================================

LabelField LabelField::build(const float* xyz, const int32_t* labels, const float* dirs,
                             int64_t n, int leaf_size) {
    LabelField f;
    std::vector<int64_t> keep;
    for (int64_t i = 0; i < n; i++)
        if (labels[i] >= 0 && labels[i] < kNone && std::isfinite(xyz[i * 3]) &&
            std::isfinite(xyz[i * 3 + 1]) && std::isfinite(xyz[i * 3 + 2]))
            keep.push_back(i);
    if (keep.empty()) return f;
    leaf_size = std::max(1, leaf_size);

    // Median split on the widest axis, seeds reordered leaf-contiguous.
    struct Todo { size_t node, lo, hi; };
    std::vector<int64_t> order = keep;
    std::vector<float> nodes;
    auto box_of = [&](size_t lo, size_t hi, float out[6]) {
        out[0] = out[1] = out[2] = 3e38f;
        out[3] = out[4] = out[5] = -3e38f;
        for (size_t i = lo; i < hi; i++)
            for (int k = 0; k < 3; k++) {
                const float v = xyz[order[i] * 3 + k];
                out[k] = std::min(out[k], v);
                out[3 + k] = std::max(out[3 + k], v);
            }
    };
    nodes.resize(8, 0.0f);
    std::vector<Todo> todo = {{0, 0, order.size()}};
    while (!todo.empty()) {
        const Todo t = todo.back();
        todo.pop_back();
        float box[6];
        box_of(t.lo, t.hi, box);
        float* nd = &nodes[t.node * 8];
        for (int k = 0; k < 3; k++) { nd[k] = box[k]; nd[4 + k] = box[3 + k]; }
        if (t.hi - t.lo <= (size_t)leaf_size) {
            nd[3] = float_of((uint32_t)t.lo);
            nd[7] = float_of((uint32_t)(t.hi - t.lo));
            continue;
        }
        int axis = 0;
        for (int k = 1; k < 3; k++)
            if (box[3 + k] - box[k] > box[3 + axis] - box[axis]) axis = k;
        const size_t mid = (t.lo + t.hi) / 2;
        std::nth_element(order.begin() + (std::ptrdiff_t)t.lo, order.begin() + (std::ptrdiff_t)mid,
                         order.begin() + (std::ptrdiff_t)t.hi,
                         [&](int64_t a, int64_t b) { return xyz[a * 3 + axis] < xyz[b * 3 + axis]; });
        const size_t left = nodes.size() / 8;
        nodes.resize(nodes.size() + 16, 0.0f);
        nd = &nodes[t.node * 8];
        nd[3] = float_of((uint32_t)left);
        nd[7] = float_of(0u);
        todo.push_back({left + 1, mid, t.hi});
        todo.push_back({left, t.lo, mid});
    }
    f.nodes.swap(nodes);
    f.seeds.resize(order.size() * 4);
    for (size_t i = 0; i < order.size(); i++) {
        const int64_t src = order[i];
        for (int k = 0; k < 3; k++) f.seeds[i * 4 + k] = xyz[src * 3 + k];
        const float* d = dirs ? &dirs[src * 3] : nullptr;
        const bool omni = !d || (d[0] == 0 && d[1] == 0 && d[2] == 0) || !std::isfinite(d[0]);
        f.seeds[i * 4 + 3] = float_of(pack(labels[src], omni ? nullptr : d));
    }
    return f;
}

// ===========================================================================
// Query
// ===========================================================================

namespace {

inline float aabb_d2(const float p[3], const float* lo, const float* hi) {
    float d2 = 0;
    for (int k = 0; k < 3; k++) {
        const float d = std::max(std::max(lo[k] - p[k], p[k] - hi[k]), 0.0f);
        d2 += d * d;
    }
    return d2;
}

}  // namespace

int LabelField::label(const double pd[3], const double* nd) const {
    if (nodes.empty()) return kNone;
    const float p[3] = {(float)pd[0], (float)pd[1], (float)pd[2]};
    float nq[3] = {0, 0, 0};
    const bool has_n = nd != nullptr;
    if (has_n)
        for (int k = 0; k < 3; k++) nq[k] = (float)nd[k];
    uint32_t stack[48];
    uint32_t sp = 0;
    stack[sp++] = 0;
    float best = 3.0e38f;
    int label = kNone;
    while (sp > 0) {
        const uint32_t node = stack[--sp];
        const float* n0 = &nodes[node * 8];
        if (aabb_d2(p, n0, n0 + 4) >= best) continue;
        const uint32_t first = bits_of(n0[3]);
        const uint32_t count = bits_of(n0[7]);
        if (count > 0) {
            for (uint32_t i = first; i < first + count; i++) {
                const float* s = &seeds[(size_t)i * 4];
                const float d[3] = {p[0] - s[0], p[1] - s[1], p[2] - s[2]};
                float e2 = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
                const uint32_t packed = bits_of(s[3]);
                const uint32_t dir_bits = packed >> 8;
                float dir[3];
                if (dir_bits != kOmni) {
                    oct_decode(dir_bits, dir);
                    const float behind = std::max(0.0f, -(d[0] * dir[0] + d[1] * dir[1] + d[2] * dir[2]));
                    e2 += (kBehindWeight * behind) * (kBehindWeight * behind);
                } else {
                    const float inv = 1.0f / std::sqrt(std::max(e2, 1e-30f));
                    for (int k = 0; k < 3; k++) dir[k] = -d[k] * inv;
                }
                if (has_n) {
                    const float a = nq[0] * dir[0] + nq[1] * dir[1] + nq[2] * dir[2];
                    if (a < kOrientCutoff) continue;
                    e2 *= 1.0f + kOrientPenalty * std::max(0.0f, -a);
                }
                if (e2 < best) {
                    best = e2;
                    label = (int)(packed & 0xFFu);
                }
            }
        } else {
            const float dl = aabb_d2(p, &nodes[first * 8], &nodes[first * 8 + 4]);
            const float dr = aabb_d2(p, &nodes[(first + 1) * 8], &nodes[(first + 1) * 8 + 4]);
            const uint32_t near_ = dl <= dr ? first : first + 1;
            const uint32_t far_ = dl <= dr ? first + 1 : first;
            if (sp + 2 <= 48) {
                stack[sp++] = far_;
                stack[sp++] = near_;
            }
        }
    }
    return label;
}

void LabelField::labels_of(const float* xyz, int64_t n, int32_t* out) const {
#pragma omp parallel for schedule(static)
    for (int64_t i = 0; i < n; i++) {
        const double p[3] = {xyz[i * 3], xyz[i * 3 + 1], xyz[i * 3 + 2]};
        out[i] = label(p);
    }
}

int LabelField::num_labels() const {
    int m = -1;
    for (int64_t i = 0; i < num_seeds(); i++)
        m = std::max(m, (int)(bits_of(seeds[(size_t)i * 4 + 3]) & 0xFFu));
    return m + 1;
}

std::vector<int64_t> LabelField::histogram() const {
    std::vector<int64_t> h((size_t)num_labels(), 0);
    for (int64_t i = 0; i < num_seeds(); i++) h[bits_of(seeds[(size_t)i * 4 + 3]) & 0xFFu]++;
    return h;
}

Aabb LabelField::bounds() const {
    Aabb b;
    if (nodes.empty()) return b;
    for (int k = 0; k < 3; k++) { b.lo[k] = nodes[k]; b.hi[k] = nodes[4 + k]; }
    return b;
}

void LabelField::lattice(int per_axis, std::vector<float>& xyz_out,
                         std::vector<int32_t>& label_out) const {
    xyz_out.clear();
    label_out.clear();
    if (nodes.empty()) return;
    const Aabb b = bounds();
    double lo[3], step[3];
    per_axis = std::max(2, per_axis);
    for (int k = 0; k < 3; k++) {
        const double ext = std::max(b.hi[k] - b.lo[k], 1e-9);
        lo[k] = b.lo[k] - 0.1 * ext;
        step[k] = 1.2 * ext / per_axis;
    }
    for (int z = 0; z < per_axis; z++)
        for (int y = 0; y < per_axis; y++)
            for (int x = 0; x < per_axis; x++) {
                const double p[3] = {lo[0] + (x + 0.5) * step[0], lo[1] + (y + 0.5) * step[1],
                                     lo[2] + (z + 0.5) * step[2]};
                for (int k = 0; k < 3; k++) xyz_out.push_back((float)p[k]);
                label_out.push_back(label(p));
            }
}

// ===========================================================================
// Interchange
// ===========================================================================

namespace {

template <typename T>
void put(std::string& out, const T& v) {
    out.append(reinterpret_cast<const char*>(&v), sizeof v);
}
template <typename T>
bool get(const char* data, size_t size, size_t& at, T& v) {
    if (at + sizeof v > size) return false;
    std::memcpy(&v, data + at, sizeof v);
    at += sizeof v;
    return true;
}

}  // namespace

void LabelField::write(std::string& out) const {
    out.append("SSLF", 4);
    put(out, (uint32_t)1);
    put(out, (uint64_t)num_seeds());
    put(out, (uint64_t)num_nodes());
    out.append(reinterpret_cast<const char*>(seeds.data()), seeds.size() * sizeof(float));
    out.append(reinterpret_cast<const char*>(nodes.data()), nodes.size() * sizeof(float));
}

bool LabelField::read(const char* data, size_t size, size_t& consumed, LabelField& out) {
    if (size < 4 || std::memcmp(data, "SSLF", 4) != 0) return false;
    size_t at = 4;
    uint32_t version = 0;
    uint64_t ns = 0, nn = 0;
    if (!get(data, size, at, version) || version != 1) return false;
    if (!get(data, size, at, ns) || !get(data, size, at, nn)) return false;
    const size_t bytes = (size_t)(ns * 4 + nn * 8) * sizeof(float);
    if (at + bytes > size) return false;
    out.seeds.resize((size_t)ns * 4);
    out.nodes.resize((size_t)nn * 8);
    std::memcpy(out.seeds.data(), data + at, out.seeds.size() * sizeof(float));
    at += out.seeds.size() * sizeof(float);
    std::memcpy(out.nodes.data(), data + at, out.nodes.size() * sizeof(float));
    at += out.nodes.size() * sizeof(float);
    consumed = at;
    return true;
}

void LabelField::write_file(const std::string& path) const {
    std::string bytes;
    write(bytes);
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) throw std::runtime_error("cannot write " + path);
    std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
}

LabelField LabelField::read_file(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) throw std::runtime_error("cannot read " + path);
    std::string bytes;
    char buf[1 << 16];
    size_t got;
    while ((got = std::fread(buf, 1, sizeof buf, f)) > 0) bytes.append(buf, got);
    std::fclose(f);
    LabelField v;
    size_t used = 0;
    if (!read(bytes.data(), bytes.size(), used, v))
        throw std::runtime_error(path + " is not a label field");
    return v;
}

void LabelRegion::write_json(JsonWriter& w) const {
    w.field("label", _label);
    if (!_file.empty()) {
        w.field("file", _file);
    } else {
        std::string bytes;
        _field->write(bytes);
        w.field("data", base64_encode(bytes));
    }
}

bool LabelRegion::emit(RegionProgram& out, std::string& error) const {
    if (out.field && out.field != _field) {
        error = "a program refers to one label field";
        return false;
    }
    out.field = _field;
    out.push(3, nullptr, 0, nullptr, nullptr, _label);
    return true;
}

// ---- base64 ---------------------------------------------------------------

namespace {
constexpr char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
}

std::string base64_encode(const std::string& in) {
    std::string out;
    out.reserve((in.size() + 2) / 3 * 4);
    size_t i = 0;
    for (; i + 2 < in.size(); i += 3) {
        const uint32_t v = ((uint8_t)in[i] << 16) | ((uint8_t)in[i + 1] << 8) | (uint8_t)in[i + 2];
        out += kB64[(v >> 18) & 63];
        out += kB64[(v >> 12) & 63];
        out += kB64[(v >> 6) & 63];
        out += kB64[v & 63];
    }
    if (i < in.size()) {
        uint32_t v = (uint8_t)in[i] << 16;
        if (i + 1 < in.size()) v |= (uint8_t)in[i + 1] << 8;
        out += kB64[(v >> 18) & 63];
        out += kB64[(v >> 12) & 63];
        out += i + 1 < in.size() ? kB64[(v >> 6) & 63] : '=';
        out += '=';
    }
    return out;
}

std::string base64_decode(const std::string& in) {
    std::string out;
    out.reserve(in.size() / 4 * 3);
    uint32_t acc = 0;
    int bits = 0;
    for (char ch : in) {
        const char* p = std::strchr(kB64, ch);
        if (ch == '=' || !p || !ch) continue;
        acc = (acc << 6) | (uint32_t)(p - kB64);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out += (char)((acc >> bits) & 0xff);
        }
    }
    return out;
}

}  // namespace spirula

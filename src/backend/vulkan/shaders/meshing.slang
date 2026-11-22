// Surface-meshing kernels (Vulkan backend), mirroring mesh/Meshing.cu:
// Gaussian activation, the 7-points-per-Gaussian cloud, the LBVH build over
// the Gaussians' support, the occupancy field (static density aggregation and
// the camera-segment visual hull), per-edge bisection and vertex coloring.
//
// Conventions this file follows, all of them established by the other entries
// here (see backend/vulkan/README.md):
//
//   * float3 arrays are declared `float*` and indexed 3*i+k. Slang would give
//     a `float3*` array a 16-byte stride; the host buffers are packed 12-byte
//     float3, which is what every other shader here assumes too.
//   * The scene is FLAT in the params struct (MESH_SCENE_PTRS +
//     MESH_SCENE_SCALARS) and lifted into a local `MeshScene` on entry, so the
//     params layout is a plain C struct the launcher can static_assert.
//   * Node AABBs are built in fmap_ordered u32 space so the bottom-up merge
//     is a plain InterlockedMin/Max (exactly the visualizer's LBVH port); the
//     launcher runs mesh_lbvh_unmap_aabb afterwards, so every consumer below
//     reads ordinary floats.
//   * The Karras split search uses the bounded ceil-halving form -- the CUDA
//     `tf <<= 1` loop exits only through signed overflow (README's UB-loop
//     rule), which hangs Intel.

#include "int64_compat.slang"

static const float ALPHA_THRESHOLD = 1.0f / 255.0f;
static const int kStackSize = 64;

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------
[ForceInline]
float3 ld3(float* p, int i) {
    return float3(p[3 * i], p[3 * i + 1], p[3 * i + 2]);
}
[ForceInline]
void st3(float* p, int i, float3 v) {
    p[3 * i] = v.x;
    p[3 * i + 1] = v.y;
    p[3 * i + 2] = v.z;
}

// Order-preserving float <-> u32 map, so a signed float min/max is an
// unsigned integer min/max (visualizer.slang's fmap_ordered).
[ForceInline]
uint fmap_ordered(float f) {
    uint b = asuint(f);
    return (b & 0x80000000u) != 0 ? ~b : (b | 0x80000000u);
}
[ForceInline]
float funmap_ordered(uint u) {
    uint b = (u & 0x80000000u) != 0 ? (u ^ 0x80000000u) : ~u;
    return asfloat(b);
}

[ForceInline]
int clz32(uint x) {
    if (x == 0) return 32;
    return 31 - int(firstbithigh(x));
}

// Coordinate remap: identity near the origin, ~ x^(1/k) for large |x|.
// Mirrors Meshing.cu's remap_coord_dev and MeshingDevice.h's remap_coord.
[ForceInline]
float remap_coord(float x, float rel_scale) {
    const float k = 2.5f;
    return k * sinh((1.0f / k) * asinh(x / rel_scale)) * rel_scale;
}

// 21-bit-per-axis Morton interleave, built as two 32-bit words so it works
// with and without shaderInt64. Bits 0..10 of an axis land in word 0 (at
// 3i <= 30) and bits 11..20 in word 1 (at 3i-32, i.e. 1..28), so the split is
// exact; the per-axis shift s in {0,1,2} then carries across the pair.
[ForceInline]
uint spread3_11(uint v) {
    uint r = 0;
    [ForceUnroll]
    for (int i = 0; i < 11; ++i)
        r |= ((v >> i) & 1u) << (3 * i);
    return r;
}

[ForceInline]
u64key_t morton3D(float3 c /* in [0,1] */) {
    const float s = float((1u << 21) - 1);
    uint xi = uint(min(max(c.x * s, 0.0f), s));
    uint yi = uint(min(max(c.y * s, 0.0f), s));
    uint zi = uint(min(max(c.z * s, 0.0f), s));

    uint lo = 0, hi = 0;
    [ForceUnroll]
    for (int a = 0; a < 3; ++a) {
        uint v = a == 0 ? xi : (a == 1 ? yi : zi);
        uint sft = a == 0 ? 2u : (a == 1 ? 1u : 0u);  // x<<2 | y<<1 | z
        uint b_lo = spread3_11(v & 0x7ffu);
        uint b_hi = spread3_11(v >> 11) << 1;  // 3*(i+11) - 32 == 3*i + 1
        if (sft == 0u) {
            lo |= b_lo;
            hi |= b_hi;
        } else {
            lo |= b_lo << sft;
            hi |= (b_hi << sft) | (b_lo >> (32u - sft));
        }
    }
    return u64key_make(gindex_t(hi), lo);
}

// ---------------------------------------------------------------------------
// The scene, flat in params and lifted into a local struct.
// ---------------------------------------------------------------------------
// Split into pointers and scalars so every params struct below can list ALL
// its pointers first and ALL its 4-byte scalars after: that layout has no
// internal padding, so the C++ mirror in kernels/Meshing.cpp cannot drift
// from it by an alignment rule the two languages disagree about.
#define MESH_SCENE_PTRS    \
    float* sc_mean;        \
    float* sc_ax0;         \
    float* sc_ax1;         \
    float* sc_ax2;         \
    float* sc_invs2;       \
    float* sc_opac;        \
    float* sc_k2;          \
    float* sc_gcol;        \
    int* sc_kept;          \
    float* sc_leafMin;     \
    float* sc_leafMax;     \
    int2* sc_internal;     \
    float* sc_nodeAABB;    \
    float* sc_campos;

#define MESH_SCENE_SCALARS \
    int sc_num_kept;       \
    int sc_num_cameras;    \
    float sc_iso;

#define MESH_SCENE_LOAD(s, p)      \
    s.mean = (p).sc_mean;          \
    s.ax0 = (p).sc_ax0;            \
    s.ax1 = (p).sc_ax1;            \
    s.ax2 = (p).sc_ax2;            \
    s.invs2 = (p).sc_invs2;        \
    s.opac = (p).sc_opac;          \
    s.k2 = (p).sc_k2;              \
    s.gcol = (p).sc_gcol;          \
    s.kept = (p).sc_kept;          \
    s.leafMin = (p).sc_leafMin;    \
    s.leafMax = (p).sc_leafMax;    \
    s.internal = (p).sc_internal;  \
    s.nodeAABB = (p).sc_nodeAABB;  \
    s.campos = (p).sc_campos;      \
    s.num_kept = (p).sc_num_kept;  \
    s.num_cameras = (p).sc_num_cameras;  \
    s.iso = (p).sc_iso;

struct MeshScene {
    float* mean;
    float* ax0;
    float* ax1;
    float* ax2;
    float* invs2;
    float* opac;
    float* k2;
    float* gcol;
    int* kept;
    float* leafMin;
    float* leafMax;
    int2* internal;
    float* nodeAABB;
    float* campos;
    int num_kept;
    int num_cameras;
    float iso;
};

// ---- Gaussian density math -------------------------------------------------
[ForceInline]
float quad_form(MeshScene s, int g, float3 d) {
    float u0 = dot(d, ld3(s.ax0, g));
    float u1 = dot(d, ld3(s.ax1, g));
    float u2 = dot(d, ld3(s.ax2, g));
    float3 is = ld3(s.invs2, g);
    return u0 * u0 * is.x + u1 * u1 * is.y + u2 * u2 * is.z;
}

[ForceInline]
float density_at(MeshScene s, int g, float3 p) {
    float q = quad_form(s, g, p - ld3(s.mean, g));
    if (q >= s.k2[g]) return 0.0f;
    return s.opac[g] * exp(-0.5f * q);
}

// Closed-form max density of Gaussian g along the segment o + t*dir, t in [0,1].
[ForceInline]
float seg_max_density(MeshScene s, int g, float3 o, float3 dir) {
    float3 e = o - ld3(s.mean, g);
    float3 a0 = ld3(s.ax0, g), a1 = ld3(s.ax1, g), a2 = ld3(s.ax2, g);
    float e0 = dot(e, a0), e1 = dot(e, a1), e2 = dot(e, a2);
    float d0 = dot(dir, a0), d1 = dot(dir, a1), d2 = dot(dir, a2);
    float3 is = ld3(s.invs2, g);
    float A = d0 * d0 * is.x + d1 * d1 * is.y + d2 * d2 * is.z;
    float B = e0 * d0 * is.x + e1 * d1 * is.y + e2 * d2 * is.z;
    float C = e0 * e0 * is.x + e1 * e1 * is.y + e2 * e2 * is.z;
    float t = (A > 1e-20f) ? (-B / A) : 0.0f;
    t = min(max(t, 0.0f), 1.0f);
    float q = A * t * t + 2.0f * B * t + C;
    if (q >= s.k2[g]) return 0.0f;
    return s.opac[g] * exp(-0.5f * q);
}

// ---- AABB tests ------------------------------------------------------------
[ForceInline]
bool point_in_aabb(float3 p, float3 bmin, float3 bmax) {
    return all(p >= bmin) && all(p <= bmax);
}

[ForceInline]
bool seg_aabb(float3 o, float3 dir, float3 bmin, float3 bmax) {
    float t0 = 0.0f, t1 = 1.0f;
    [ForceUnroll]
    for (int a = 0; a < 3; ++a) {
        float oa = a == 0 ? o.x : (a == 1 ? o.y : o.z);
        float da = a == 0 ? dir.x : (a == 1 ? dir.y : dir.z);
        float lo = a == 0 ? bmin.x : (a == 1 ? bmin.y : bmin.z);
        float hi = a == 0 ? bmax.x : (a == 1 ? bmax.y : bmax.z);
        if (abs(da) < 1e-20f) {
            if (oa < lo || oa > hi) return false;
        } else {
            float inv = 1.0f / da;
            float ta = (lo - oa) * inv, tb = (hi - oa) * inv;
            if (ta > tb) { float tmp = ta; ta = tb; tb = tmp; }
            t0 = max(t0, ta);
            t1 = min(t1, tb);
            if (t0 > t1) return false;
        }
    }
    return true;
}

// ---- static occupancy: density aggregation of Gaussians containing p -------
float occ_static(MeshScene s, float3 p) {
    float S = 0.0f;  // sum log(1 - density)
    if (s.num_kept == 1) {
        float d = density_at(s, s.kept[0], p);
        return d > ALPHA_THRESHOLD ? d : 0.0f;
    }
    int stack[kStackSize];
    int sp = 0;
    if (!point_in_aabb(p, ld3(s.nodeAABB, 0), ld3(s.nodeAABB, 1))) return 0.0f;
    stack[sp++] = 0;
    while (sp > 0) {
        int2 ch = s.internal[stack[--sp]];
        [ForceUnroll]
        for (int c = 0; c < 2; ++c) {
            int child = c == 0 ? ch.x : ch.y;
            if (child < 0) {
                int kp = ~child;
                if (point_in_aabb(p, ld3(s.leafMin, kp), ld3(s.leafMax, kp))) {
                    float d = density_at(s, s.kept[kp], p);
                    if (d > ALPHA_THRESHOLD) S += log(1.0f - min(d, 0.999f));
                }
            } else if (point_in_aabb(p, ld3(s.nodeAABB, 2 * child),
                                     ld3(s.nodeAABB, 2 * child + 1))) {
                if (sp < kStackSize) stack[sp++] = child;
            }
        }
    }
    return 1.0f - exp(S);
}

// ---- one camera: sum log(1-d) over Gaussians the segment p->cam crosses -----
float seg_log_transmittance(MeshScene s, float3 p, float3 cam) {
    float3 dir = cam - p;
    float S = 0.0f;
    if (s.num_kept == 1) {
        float d = seg_max_density(s, s.kept[0], p, dir);
        return d > ALPHA_THRESHOLD ? log(1.0f - min(d, 0.999f)) : 0.0f;
    }
    int stack[kStackSize];
    int sp = 0;
    if (!seg_aabb(p, dir, ld3(s.nodeAABB, 0), ld3(s.nodeAABB, 1))) return 0.0f;
    stack[sp++] = 0;
    while (sp > 0) {
        int2 ch = s.internal[stack[--sp]];
        [ForceUnroll]
        for (int c = 0; c < 2; ++c) {
            int child = c == 0 ? ch.x : ch.y;
            if (child < 0) {
                int kp = ~child;
                if (seg_aabb(p, dir, ld3(s.leafMin, kp), ld3(s.leafMax, kp))) {
                    float d = seg_max_density(s, s.kept[kp], p, dir);
                    if (d > ALPHA_THRESHOLD) S += log(1.0f - min(d, 0.999f));
                }
            } else if (seg_aabb(p, dir, ld3(s.nodeAABB, 2 * child),
                                ld3(s.nodeAABB, 2 * child + 1))) {
                if (sp < kStackSize) stack[sp++] = child;
            }
        }
    }
    return S;
}

float occ_dynamic(MeshScene s, float3 p) {
    float occ_min = 1.0f;
    for (int ci = 0; ci < s.num_cameras; ++ci) {
        float S = seg_log_transmittance(s, p, ld3(s.campos, ci));
        occ_min = min(occ_min, 1.0f - exp(S));
    }
    return occ_min;
}

[ForceInline]
float occ_eval(MeshScene s, float3 p, bool dynamic) {
    return dynamic ? occ_dynamic(s, p) : occ_static(s, p);
}

// ---------------------------------------------------------------------------
// Vertex coloring from the splats' DC color.
//
// NOTE: Meshing.cu also carries color_camera (front-to-back compositing along
// each camera ray). Its call site is `if (dynamic && false)`, so it is dead on
// CUDA too, and it needs three 64-entry per-thread arrays -- which would spill
// catastrophically here. It is deliberately not ported; port it on both sides
// together if that branch is ever switched on.
// ---------------------------------------------------------------------------

// Density-weighted average color of the splats that contain p. Returns false
// if p is inside no splat's support.
bool color_static(MeshScene s, float3 p, out float3 col) {
    col = float3(0.0f);
    float wsum = 0.0f;
    float3 csum = float3(0.0f);
    if (s.num_kept == 1) {
        float d = density_at(s, s.kept[0], p);
        if (d > 0.0f) { col = ld3(s.gcol, s.kept[0]); return true; }
        return false;
    }
    int stack[kStackSize];
    int sp = 0;
    if (!point_in_aabb(p, ld3(s.nodeAABB, 0), ld3(s.nodeAABB, 1))) return false;
    stack[sp++] = 0;
    while (sp > 0) {
        int2 ch = s.internal[stack[--sp]];
        [ForceUnroll]
        for (int c = 0; c < 2; ++c) {
            int child = c == 0 ? ch.x : ch.y;
            if (child < 0) {
                int kp = ~child;
                if (point_in_aabb(p, ld3(s.leafMin, kp), ld3(s.leafMax, kp))) {
                    int g = s.kept[kp];
                    float d = density_at(s, g, p);
                    if (d > 0.0f) { wsum += d; csum += ld3(s.gcol, g) * d; }
                }
            } else if (point_in_aabb(p, ld3(s.nodeAABB, 2 * child),
                                     ld3(s.nodeAABB, 2 * child + 1))) {
                if (sp < kStackSize) stack[sp++] = child;
            }
        }
    }
    if (wsum > 0.0f) { col = csum * (1.0f / wsum); return true; }
    return false;
}

// Color of the splat whose center is nearest p (last-resort fallback).
float3 color_nearest(MeshScene s, float3 p) {
    if (s.num_kept <= 1) return ld3(s.gcol, s.kept[0]);
    float best = 1e30f;
    int bg = s.kept[0];
    int stack[kStackSize];
    int sp = 0;
    stack[sp++] = 0;
    while (sp > 0) {
        int2 ch = s.internal[stack[--sp]];
        [ForceUnroll]
        for (int c = 0; c < 2; ++c) {
            int child = c == 0 ? ch.x : ch.y;
            if (child < 0) {
                int g = s.kept[~child];
                float3 d = p - ld3(s.mean, g);
                float d2 = dot(d, d);
                if (d2 < best) { best = d2; bg = g; }
            } else {
                float3 lo = ld3(s.nodeAABB, 2 * child);
                float3 hi = ld3(s.nodeAABB, 2 * child + 1);
                float3 dd = max(max(lo - p, p - hi), float3(0.0f));
                if (dot(dd, dd) < best && sp < kStackSize) stack[sp++] = child;
            }
        }
    }
    return ld3(s.gcol, bg);
}

// ===========================================================================
// Entries
// ===========================================================================

// ---- activation ------------------------------------------------------------
struct MeshActivateParams {
    float* means;
    float* quats;
    float* logsc;
    float* logit;
    float* fdc;
    float* mean;
    float* ax0;
    float* ax1;
    float* ax2;
    float* invs2;
    float* opac;
    float* radius;
    float* k2;
    int* valid;
    float* gcol;
    uint32_t N;
    uint32_t wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void mesh_activate(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                   uniform MeshActivateParams* pp) {
    MeshActivateParams p = *pp;
    uint gid = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (gid >= p.N) return;
    int i = int(gid);

    st3(p.mean, i, ld3(p.means, i));

    float w = p.quats[4 * i], x = p.quats[4 * i + 1];
    float y = p.quats[4 * i + 2], z = p.quats[4 * i + 3];
    float nrm = rsqrt(max(w * w + x * x + y * y + z * z, 1e-20f));
    w *= nrm; x *= nrm; y *= nrm; z *= nrm;
    st3(p.ax0, i, float3(1 - 2 * (y * y + z * z), 2 * (x * y + w * z),
                         2 * (x * z - w * y)));
    st3(p.ax1, i, float3(2 * (x * y - w * z), 1 - 2 * (x * x + z * z),
                         2 * (y * z + w * x)));
    st3(p.ax2, i, float3(2 * (x * z + w * y), 2 * (y * z - w * x),
                         1 - 2 * (x * x + y * y)));

    float sx = exp(p.logsc[3 * i]);
    float sy = exp(p.logsc[3 * i + 1]);
    float sz = exp(p.logsc[3 * i + 2]);
    st3(p.invs2, i, float3(1.0f / (sx * sx), 1.0f / (sy * sy), 1.0f / (sz * sz)));

    // base color from the SH band-0 (DC) coefficient
    const float C0 = 0.28209479177387814f;
    st3(p.gcol, i,
        clamp(float3(0.5f) + C0 * ld3(p.fdc, i), float3(0.0f), float3(1.0f)));

    float op = 1.0f / (1.0f + exp(-p.logit[i]));
    p.opac[i] = op;
    if (op > ALPHA_THRESHOLD) {
        float kk2 = 2.0f * log(op / ALPHA_THRESHOLD);
        p.k2[i] = kk2;
        p.radius[i] = sqrt(kk2) * max(sx, max(sy, sz));
        p.valid[i] = 1;
    } else {
        p.k2[i] = 0.0f;
        p.radius[i] = 0.0f;
        p.valid[i] = 0;
    }
}

// ---- point cloud -----------------------------------------------------------
struct MeshPointCloudParams {
    int* kept;
    float* mean;
    float* ax0;
    float* ax1;
    float* ax2;
    float* invs2;
    float* k2;
    float* out_xyz;
    float min_axis_ratio;
    uint32_t num_kept;
    uint32_t wgs_per_row;
    uint32_t samples_per_gaussian;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void mesh_pointcloud(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                     uniform MeshPointCloudParams* pp) {
    MeshPointCloudParams p = *pp;
    uint gid = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (gid >= p.num_kept) return;
    int i = int(gid);
    int g = p.kept[i];
    float3 m = ld3(p.mean, g);
    if (p.samples_per_gaussian <= 1u) {
        int base = i * 3;
        p.out_xyz[base + 0] = m.x;
        p.out_xyz[base + 1] = m.y;
        p.out_xyz[base + 2] = m.z;
        return;
    }
    float k = sqrt(p.k2[g]) * 2.5f;  // 2.5x the cutoff, for overlapping splats
    float3 s = rsqrt(ld3(p.invs2, g));
    float lo = p.min_axis_ratio * max(s.x, max(s.y, s.z));
    float3 o0 = ld3(p.ax0, g) * (k * max(s.x, lo));
    float3 o1 = ld3(p.ax1, g) * (k * max(s.y, lo));
    float3 o2 = ld3(p.ax2, g) * (k * max(s.z, lo));
    float3 pts[7] = { m, m + o0, m - o0, m + o1, m - o1, m + o2, m - o2 };
    gindex_t base = gindex_t(gid) * 21;
    [ForceUnroll]
    for (int j = 0; j < 7; ++j) {
        elem_at(p.out_xyz, base + gindex_t(3 * j + 0))[0] = pts[j].x;
        elem_at(p.out_xyz, base + gindex_t(3 * j + 1))[0] = pts[j].y;
        elem_at(p.out_xyz, base + gindex_t(3 * j + 2))[0] = pts[j].z;
    }
}

// ---- LBVH: per-leaf AABB + morton -----------------------------------------
struct MeshBvhPrepParams {
    int* kept;
    float* mean;
    float* ax0;
    float* ax1;
    float* ax2;
    float* invs2;
    float* k2;
    float* leafMin;
    float* leafMax;
    u64key_t* morton;
    int* iota;
    // Scalars, not float3: a float3 field's alignment is a rule Slang and C++
    // could disagree on, and this struct's whole point is a layout the
    // launcher can static_assert.
    float remap_min_x, remap_min_y, remap_min_z;
    float remap_ext_x, remap_ext_y, remap_ext_z;  // 1 / extent
    float rel_scale;
    uint32_t num_kept;
    uint32_t wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void mesh_bvh_prep(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                   uniform MeshBvhPrepParams* pp) {
    MeshBvhPrepParams p = *pp;
    uint gid = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (gid >= p.num_kept) return;
    int kp = int(gid);
    int g = p.kept[kp];
    float3 m = ld3(p.mean, g);
    float3 is = ld3(p.invs2, g);
    float s0 = 1.0f / is.x, s1 = 1.0f / is.y, s2 = 1.0f / is.z;  // sigma^2
    float3 a0 = ld3(p.ax0, g), a1 = ld3(p.ax1, g), a2 = ld3(p.ax2, g);
    // diagonal of Sigma = R diag(sigma^2) R^T
    float cxx = a0.x * a0.x * s0 + a1.x * a1.x * s1 + a2.x * a2.x * s2;
    float cyy = a0.y * a0.y * s0 + a1.y * a1.y * s1 + a2.y * a2.y * s2;
    float czz = a0.z * a0.z * s0 + a1.z * a1.z * s1 + a2.z * a2.z * s2;
    float k = sqrt(p.k2[g]);
    float3 bound = float3(k * sqrt(cxx), k * sqrt(cyy), k * sqrt(czz));
    st3(p.leafMin, kp, m - bound);
    st3(p.leafMax, kp, m + bound);

    float3 rc = float3(
        (remap_coord(m.x, p.rel_scale) - p.remap_min_x) * p.remap_ext_x,
        (remap_coord(m.y, p.rel_scale) - p.remap_min_y) * p.remap_ext_y,
        (remap_coord(m.z, p.rel_scale) - p.remap_min_z) * p.remap_ext_z);
    p.morton[kp] = morton3D(rc);
    p.iota[kp] = kp;
}

// ---- LBVH: Karras radix tree ----------------------------------------------
// Shared by the Gaussian LBVH and the mesh-triangle LBVH.
struct MeshLbvhNodesParams {  // 32 bytes: pushed directly
    u64key_t* morton;  // [n] sorted
    int* argsort;      // [n]
    int2* internal;    // [n-1]
    int* parent;       // [n-1], -1 initialized
    uint32_t n;
    uint32_t wgs_per_row;
};

[ForceInline]
int lbvh_delta(u64key_t* morton, uint n, int i, int j) {
    if (j < 0 || j >= int(n)) return -1;
    if (u64key_eq(morton[i], morton[j]))
        return 64 + clz32(uint(i) ^ uint(j));
    return u64key_clz(u64key_xor(morton[i], morton[j]));
}

[shader("compute")]
[numthreads(256, 1, 1)]
void mesh_lbvh_internal(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                        uniform MeshLbvhNodesParams p) {
    uint gid = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (gid >= p.n - 1) return;
    int i = int(gid);

    #define delta(a, b) lbvh_delta(p.morton, p.n, a, b)

    int d = delta(i, i + 1) - delta(i, i - 1);
    d = d > 0 ? 1 : d < 0 ? -1 : 0;

    int delta_min = delta(i, i - d);
    int lmax = 2;
    while (delta(i, i + lmax * d) > delta_min)
        lmax <<= 1;

    int l = 0;
    for (int t = lmax >> 1; t >= 1; t >>= 1)
        if (delta(i, i + (l + t) * d) > delta_min)
            l += t;
    int j = i + l * d;

    int delta_node = delta(i, j);
    int sp_ = 0;
    // CUDA probes t = ceil(l/2), ceil(l/4), ... and exits only through signed
    // overflow (README's UB-loop rule). Same probe sequence, bounded.
    if (l > 0) {
        int t = (l + 1) / 2;
        while (true) {
            if (delta(i, i + (sp_ + t) * d) > delta_node)
                sp_ += t;
            if (t == 1) break;
            t = (t + 1) / 2;
        }
    }
    int gamma = i + sp_ * d + min(d, 0);

    int left = min(i, j) == gamma ? ~p.argsort[gamma] : gamma;
    int right = max(i, j) == gamma + 1 ? ~p.argsort[gamma + 1] : gamma + 1;
    p.internal[i] = int2(left, right);

    if (left >= 0) InterlockedMax(p.parent[left], i);
    if (right >= 0) InterlockedMax(p.parent[right], i);

    #undef delta
}

// ---- LBVH: node AABBs (built in mapped u32 space) --------------------------
struct MeshLbvhInitAabbParams {  // 16 bytes: pushed directly
    uint* nodeAABB;  // [2*n_internal * 3] mapped words
    uint32_t n_internal;
    uint32_t wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void mesh_lbvh_init_aabb(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                         uniform MeshLbvhInitAabbParams p) {
    uint gid = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (gid >= p.n_internal) return;
    uint hi = fmap_ordered(1e30f), lo = fmap_ordered(-1e30f);
    [ForceUnroll]
    for (int k = 0; k < 3; k++) {
        p.nodeAABB[(2 * gid) * 3 + k] = hi;      // min slot starts high
        p.nodeAABB[(2 * gid + 1) * 3 + k] = lo;  // max slot starts low
    }
}

struct MeshLbvhAabbParams {  // 48 bytes: pushed directly
    int2* internal;
    int* parent;
    float* leafMin;
    float* leafMax;
    uint* nodeAABB;  // mapped words
    uint32_t n;
    uint32_t wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void mesh_lbvh_aabb(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                    uniform MeshLbvhAabbParams p) {
    uint gid = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (gid >= p.n - 1) return;
    int i = int(gid);

    int2 ch = p.internal[i];
    if (ch.x >= 0 && ch.y >= 0) return;  // no leaf child -> filled from below

    float3 bmin = float3(1e30f), bmax = float3(-1e30f);
    if (ch.x < 0) {
        bmin = min(bmin, ld3(p.leafMin, ~ch.x));
        bmax = max(bmax, ld3(p.leafMax, ~ch.x));
    }
    if (ch.y < 0) {
        bmin = min(bmin, ld3(p.leafMin, ~ch.y));
        bmax = max(bmax, ld3(p.leafMax, ~ch.y));
    }

    uint mmin[3] = {fmap_ordered(bmin.x), fmap_ordered(bmin.y),
                    fmap_ordered(bmin.z)};
    uint mmax[3] = {fmap_ordered(bmax.x), fmap_ordered(bmax.y),
                    fmap_ordered(bmax.z)};

    // All six atomics always run (CUDA uses `&`, not `&&`), and the climb
    // stops when this thread's box was already covered.
    while (true) {
        uint o0, o1, o2, o3, o4, o5;
        InterlockedMin(p.nodeAABB[(2 * i) * 3 + 0], mmin[0], o0);
        InterlockedMin(p.nodeAABB[(2 * i) * 3 + 1], mmin[1], o1);
        InterlockedMin(p.nodeAABB[(2 * i) * 3 + 2], mmin[2], o2);
        InterlockedMax(p.nodeAABB[(2 * i + 1) * 3 + 0], mmax[0], o3);
        InterlockedMax(p.nodeAABB[(2 * i + 1) * 3 + 1], mmax[1], o4);
        InterlockedMax(p.nodeAABB[(2 * i + 1) * 3 + 2], mmax[2], o5);
        bool covered = (o0 <= mmin[0]) & (o1 <= mmin[1]) & (o2 <= mmin[2]) &
                       (o3 >= mmax[0]) & (o4 >= mmax[1]) & (o5 >= mmax[2]);
        if (covered) break;
        i = p.parent[i];
        if (i < 0) break;
    }
}

struct MeshUnmapParams {  // 16 bytes: pushed directly
    uint* words;  // mapped -> float bits, in place
    uint32_t n;
    uint32_t wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void mesh_lbvh_unmap_aabb(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                          uniform MeshUnmapParams p) {
    uint gid = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (gid >= p.n) return;
    p.words[gid] = asuint(funmap_ordered(p.words[gid]));
}

// ---- occupancy / bisection / color ----------------------------------------
struct MeshOccParams {
    MESH_SCENE_PTRS
    float* pts;
    float* occ;
    MESH_SCENE_SCALARS
    uint32_t n;
    uint32_t dynamic;
    uint32_t wgs_per_row;
};

[shader("compute")]
[numthreads(128, 1, 1)]
void mesh_occ(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
              uniform MeshOccParams* pp) {
    MeshOccParams p = *pp;
    uint gid = (wg.y * p.wgs_per_row + wg.x) * 128 + tid;
    if (gid >= p.n) return;
    int i = int(gid);
    MeshScene s;
    MESH_SCENE_LOAD(s, p)
    p.occ[i] = occ_eval(s, ld3(p.pts, i), p.dynamic != 0);
}

struct MeshBisectParams {
    MESH_SCENE_PTRS
    float* cloud;
    int* ea;
    int* eb;
    float* oa;
    float* ob;
    float* out_xyz;
    MESH_SCENE_SCALARS
    uint32_t n;
    uint32_t iters;
    uint32_t dynamic;
    uint32_t wgs_per_row;
};

[shader("compute")]
[numthreads(128, 1, 1)]
void mesh_bisect(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                 uniform MeshBisectParams* pp) {
    MeshBisectParams p = *pp;
    uint gid = (wg.y * p.wgs_per_row + wg.x) * 128 + tid;
    if (gid >= p.n) return;
    int i = int(gid);
    MeshScene s;
    MESH_SCENE_LOAD(s, p)

    float3 pa = ld3(p.cloud, p.ea[i]);
    float3 pb = ld3(p.cloud, p.eb[i]);
    // bisection narrows the bracket [pa,pb] straddling iso; the occupancy at
    // each end is carried so the final step is a linear interpolation.
    float occa = p.oa[i], occb = p.ob[i];
    bool a_in = occa >= s.iso;
    for (uint it = 0; it < p.iters; ++it) {
        float3 mid = (pa + pb) * 0.5f;
        float om = occ_eval(s, mid, p.dynamic != 0);
        if ((om >= s.iso) == a_in) { pa = mid; occa = om; }
        else                       { pb = mid; occb = om; }
    }
    float denom = occb - occa;
    float t = (abs(denom) > 1e-12f) ? (s.iso - occa) / denom : 0.5f;
    t = min(max(t, 0.0f), 1.0f);
    st3(p.out_xyz, i, pa + (pb - pa) * t);
}

struct MeshColorizeParams {
    MESH_SCENE_PTRS
    float* verts;
    float* rgb;
    MESH_SCENE_SCALARS
    uint32_t n;
    uint32_t fallback_only;  // 1: only fill verts the render path left < 0
    uint32_t wgs_per_row;
};

[shader("compute")]
[numthreads(128, 1, 1)]
void mesh_colorize(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                   uniform MeshColorizeParams* pp) {
    MeshColorizeParams p = *pp;
    uint gid = (wg.y * p.wgs_per_row + wg.x) * 128 + tid;
    if (gid >= p.n) return;
    int i = int(gid);
    if (p.fallback_only != 0 && p.rgb[3 * i] >= 0.0f) return;
    MeshScene s;
    MESH_SCENE_LOAD(s, p)
    float3 pt = ld3(p.verts, i);
    float3 col;
    if (!color_static(s, pt, col)) col = color_nearest(s, pt);
    st3(p.rgb, i, col);
}

struct MeshOccCombineParams {  // 24 bytes: pushed directly
    float* occ;
    float* occ_static;
    uint32_t n;
    uint32_t wgs_per_row;
};

[shader("compute")]
[numthreads(128, 1, 1)]
void mesh_occ_combine(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                      uniform MeshOccCombineParams p) {
    uint gid = (wg.y * p.wgs_per_row + wg.x) * 128 + tid;
    if (gid >= p.n) return;
    float a = p.occ_static[gid], b = p.occ[gid];
    p.occ[gid] = 1.0f - (1.0f - a) * (1.0f - b);
}

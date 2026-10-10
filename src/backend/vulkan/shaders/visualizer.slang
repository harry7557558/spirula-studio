// Visualizer kernels (Vulkan backend), mirroring kernels/visualize/Visualizer.cu:
// camera-frustum geometry fill, LBVH build (morton keys + Karras internal
// nodes + bottom-up AABBs), depth min/max, the annotate/colormap/overlay
// blit, and the training-thumbnail capture.
//
// Portability substitutions vs the CUDA source (all documented in the README
// rules):
//  - float atomicMin/Max (root/tree AABBs, min_max) use InterlockedMin/Max
//    on an ORDER-PRESERVING u32 mapping of the float bits (exact, works on
//    devices without float atomics). Mapped buffers are decoded either by a
//    small unmap pass (tree AABBs, consumed by the traversal hot loop) or at
//    the consumer (min_max in the blit; root AABB host-side).
//  - warp ballot/reduce in the blit's gray-plane fit becomes a groupshared
//    serial reduction over the 32-thread workgroup (deterministic; the CUDA
//    kernel assumes WARP_SIZE == 32 which Vulkan must not).
//  - uint8 buffers (thumbnails RGBA, done mask, final RGB bytes) are
//    accessed as RGBA words or through int8_compat.slang byte helpers; the
//    [H,W,3] byte output is produced by a separate pack kernel from a
//    per-pixel RGBA word scratch.
//  - 64-bit morton keys / LBVH deltas go through int64_compat.slang
//    (bit-identical (lo, hi) pair emulation in the ".noint64" variant).
//  - 2D thread blocks (16x16 min/max, 8x4 blit) become flat wide-X layouts
//    (VUID-02757: workgroup X must be a multiple of the subgroup size).

#define CudaDeviceExport ForceInline
#include "shaders/projection_utils.slang"
#undef CudaDeviceExport

#include "int64_compat.slang"
#include "int8_compat.slang"
#include "rgb_format.slang"
#include "dist_spec.slang"

// generate_ray taking the full 8-float storage row: the callers keep their
// groupshared barriers at top level.
[ForceInline]
bool _vis_ray<D : ICameraDistortion>(float2 uv, int camera_model,
                                     float row[kDistCoeffRow],
                                     out float3 raydir) {
    float dc[D.kNumParams];
    [ForceUnroll]
    for (int k = 0; k < D.kNumParams; k++) dc[k] = row[k];
    return generate_ray<D>(uv, camera_model, dc, raydir);
}

#define _vis_ray_call(D) ok = _vis_ray<D>(uv, camera_model, row, raydir)

// Tier folded from kDistortion: the blit's view camera, one per dispatch.
[ForceInline]
bool vis_ray(float2 uv, int camera_model, float row[kDistCoeffRow],
             out float3 raydir) {
    bool ok = false;
    raydir = float3(0.0f);
    SS_DISPATCH_DIST(_vis_ray_call)
    return ok;
}

// Tier as a RUNTIME value: the frustum fill draws a whole camera table in one
// dispatch and a dataset may mix tiers, so the spec constant (which carries
// the view camera's tier) cannot stand in for the per-camera one. Uniform per
// workgroup -- one camera per workgroup.
[ForceInline]
bool vis_ray_dyn(float2 uv, int camera_model, int tier,
                 float row[kDistCoeffRow], out float3 raydir) {
    bool ok = false;
    raydir = float3(0.0f);
    switch (tier) {
    case 0:  _vis_ray_call(DistNone); break;
    case 1:  _vis_ray_call(DistOpenCV); break;
    default: _vis_ray_call(DistThinPrism); break;
    }
    return ok;
}
#undef _vis_ray_call

static const int kNumFrustumSegments = 16;
static const int kNumFrustumFaces = 8;

// Triangle-AABB inflation, relative to the box's coordinate magnitude. See the
// CUDA source (triangle_aabb) for why a frustum face needs it.
static const float kTriAabbPad = 1e-5f;

// ---------------------------------------------------------------------------
// Order-preserving float <-> u32 mapping (monotonic for ALL floats).
// ---------------------------------------------------------------------------

[ForceInline]
uint fmap_ordered(float f) {
    uint b = asuint(f);
    return (b & 0x80000000u) != 0 ? ~b : (b | 0x80000000u);
}

// inverse of fmap_ordered: high bit set -> positive (clear it),
// high bit clear -> negative (bitwise NOT).
[ForceInline]
float funmap_ordered(uint u) {
    uint b = (u & 0x80000000u) != 0 ? (u ^ 0x80000000u) : ~u;
    return asfloat(b);
}

// ---------------------------------------------------------------------------
// Frustum geometry fill: one workgroup (64 threads = 4*kNumFrustumSegments)
// per camera. Mirrors fill_frustum_segments_kernel.
// ---------------------------------------------------------------------------

struct VisFrustumParams {  // 84 -> 88 bytes: pushed directly
    float4* intrins;         // [N]
    int* widths;             // [N]
    int* heights;            // [N]
    int* camera_models;      // [N]
    float* dist_coeffs;      // [N,8] or null
    int* distortions;        // [N] CameraDistortionType, or null
    float* camera_to_worlds; // [N,3,4]
    float4* lss_buffer;      // [N, 8*kNumFrustumSegments, 2]
    float4* tri_buffer;      // [N, 4*kNumFrustumFaces^2, 4]
    float size;
    uint32_t N;
    uint32_t _pad0;
};

groupshared float3 _vis_polylines[4 * kNumFrustumSegments];

[ForceInline]
float3 frustum_ray(float2 uv, int camera_model, int tier,
                   float dist[kDistCoeffRow]) {
    float3 raydir = float3(asfloat(0x7fc00000u));  // NaN
    if (!vis_ray_dyn(uv, camera_model, tier, dist, raydir)) {
        // binary search for the last valid uv toward the center
        float t0 = 0.0f, t1 = 1.0f;
        for (int iter = 0; iter < 12; ++iter) {
            float t = 0.5f * (t0 + t1);
            float3 temp;
            if (vis_ray_dyn(uv * t, camera_model, tier, dist, temp)) {
                t0 = t;
                raydir = temp;
            } else {
                t1 = t;
            }
        }
    }
    return raydir;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void vis_fill_frustum(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                      uniform VisFrustumParams p) {
    uint bid = wg.x;
    if (bid >= p.N) return;

    float4 intrin = p.intrins[bid];
    float fx = intrin.x, fy = intrin.y, cx = intrin.z, cy = intrin.w;
    float width = float(p.widths[bid]);
    float height = float(p.heights[bid]);
    int camera_model = p.camera_models[bid];
    int tier = is_null(p.distortions) ? 0 : p.distortions[bid];
    float dist[kDistCoeffRow];
    [ForceUnroll]
    for (int k = 0; k < kDistCoeffRow; k++)
        dist[k] = is_null(p.dist_coeffs)
                      ? 0.0f
                      : p.dist_coeffs[kDistCoeffRow * bid + k];
    float2 corners[4] = {
        float2(-cx / fx, -cy / fy),
        float2((width - cx) / fx, -cy / fy),
        float2((width - cx) / fx, (height - cy) / fy),
        float2(-cx / fx, (height - cy) / fy),
    };
    float sxy = sqrt(fx * fy / (width * height));
    float sz = rsqrt(sxy);
    sxy *= sz * p.size;
    const float r0 = 0.015f * p.size * sz, r1 = 0.03f * p.size * sz;
    sz = p.size / sz;

    float* c2w = p.camera_to_worlds + 12 * bid;
    float3x3 R;
    [ForceUnroll]
    for (int r = 0; r < 3; r++)
        R[r] = float3(c2w[4 * r + 0], c2w[4 * r + 1], c2w[4 * r + 2]);
    float3 t = float3(c2w[3], c2w[7], c2w[11]);

    bool wide = camera_model == 1 /*FISHEYE*/ || camera_model == 2
                /*EQUISOLID*/ || camera_model == 3 /*EQUIRECTANGULAR*/;

    // polylines
    for (uint i = tid; i < uint(4 * kNumFrustumSegments); i += 64) {
        uint corner_idx = i / kNumFrustumSegments;
        float2 uv = corners[corner_idx] +
                    (corners[(corner_idx + 1) % 4] - corners[corner_idx]) *
                        (float(i % kNumFrustumSegments) / kNumFrustumSegments);
        float3 pt = frustum_ray(uv, camera_model, tier, dist);
        if (wide)
            pt = normalize(pt) * sqrt((2.0f * sxy * sxy + sz * sz) / 3.0f);
        else
            pt = pt * float3(sxy, sxy, sz) / pt.z;
        pt = float3(dot(R[0], pt), dot(R[1], pt), dot(R[2], pt)) + t;
        _vis_polylines[i] = pt;
    }
    GroupMemoryBarrierWithGroupSync();

    // frame
    float4* lss = p.lss_buffer + bid * (8 * kNumFrustumSegments * 2);
    for (uint i = tid; i < uint(4 * kNumFrustumSegments); i += 64) {
        float3 p0 = _vis_polylines[i];
        float3 p1 = _vis_polylines[(i + 1) % (4 * kNumFrustumSegments)];
        if (!isfinite(dot(p0, p1))) {
            p0 = t;
            p1 = t;
        }
        lss[2 * i] = float4(p0.x, p0.y, p0.z, r1);
        lss[2 * i + 1] = float4(p1.x, p1.y, p1.z, r1);
    }

    // anchor
    lss = lss + 4 * kNumFrustumSegments * 2;
    for (uint i = tid; i < uint(4 * kNumFrustumSegments); i += 64) {
        float3 pc =
            _vis_polylines[(i / kNumFrustumSegments) * kNumFrustumSegments];
        float s0 = float(i % kNumFrustumSegments) /
                   float(kNumFrustumSegments);
        float s1 = s0 + 1.0f / float(kNumFrustumSegments);
        float3 p0 = t + (pc - t) * s0;
        float3 p1 = t + (pc - t) * s1;
        if (!isfinite(dot(p0, p1))) {
            p0 = t;
            p1 = t;
        }
        lss[2 * i] = float4(p0.x, p0.y, p0.z, r0 + (r1 - r0) * s0);
        lss[2 * i + 1] = float4(p1.x, p1.y, p1.z, r0 + (r1 - r0) * s1);
    }

    // faces
    float4* tri = p.tri_buffer +
                  bid * (4 * kNumFrustumFaces * kNumFrustumFaces * 4);
    for (uint i = tid; i < uint(kNumFrustumFaces * kNumFrustumFaces);
         i += 64) {
        float u0 = float(i / kNumFrustumFaces) / kNumFrustumFaces;
        float v0 = float(i % kNumFrustumFaces) / kNumFrustumFaces;
        float u1 = u0 + 1.0f / kNumFrustumFaces;
        float v1 = v0 + 1.0f / kNumFrustumFaces;
        float uc = 0.5f * (u0 + u1);
        float vc = 0.5f * (v0 + v1);
        float2 uvs[5] = {
            float2(uc, vc), float2(u0, v0), float2(u0, v1),
            float2(u1, v1), float2(u1, v0),
        };
        float3 verts[5];
        [ForceUnroll]
        for (int pi = 0; pi < 5; ++pi) {
            float u = uvs[pi].x, v = uvs[pi].y;
            float2 uv = corners[0] * (1.0f - u) * (1.0f - v) +
                        corners[1] * u * (1.0f - v) + corners[2] * u * v +
                        corners[3] * (1.0f - u) * v;
            float3 pt = frustum_ray(uv, camera_model, tier, dist);
            if (wide)
                pt = normalize(pt) *
                     sqrt((2.0f * sxy * sxy + sz * sz) / 3.0f);
            else
                pt = pt * float3(sxy, sxy, sz) / pt.z;
            pt = float3(dot(R[0], pt), dot(R[1], pt), dot(R[2], pt)) + t;
            verts[pi] = pt;
        }
        [ForceUnroll]
        for (uint pi = 0; pi < 4; ++pi) {
            float3 p0 = verts[0];
            float3 p1 = verts[pi + 1];
            float3 p2 = verts[(pi + 1) % 4 + 1];
            float2 uv0 = uvs[0];
            float2 uv1 = uvs[pi + 1];
            float2 uv2 = uvs[(pi + 1) % 4 + 1];
            tri[4 * (4 * i + pi) + 0] = float4(p0.x, p0.y, p0.z, p1.x);
            tri[4 * (4 * i + pi) + 1] = float4(p1.y, p1.z, p2.x, p2.y);
            tri[4 * (4 * i + pi) + 2] = float4(p2.z, uv0.x, uv0.y, uv1.x);
            tri[4 * (4 * i + pi) + 3] =
                float4(uv1.y, uv2.x, uv2.y, asfloat(bid));
        }
    }
}

// ---------------------------------------------------------------------------
// Element AABB loaders (kVisPrim: 0 = linear swept sphere, 1 = triangle).
// ---------------------------------------------------------------------------

[SpecializationConstant]
const int kVisPrim = 0;

[ForceInline]
void lss_aabb_of(float4* buffer, uint idx, out float3 bmin, out float3 bmax) {
    float4 e0 = buffer[2 * idx];
    float4 e1 = buffer[2 * idx + 1];
    bmin = float3(min(e0.x - e0.w, e1.x - e1.w),
                  min(e0.y - e0.w, e1.y - e1.w),
                  min(e0.z - e0.w, e1.z - e1.w));
    bmax = float3(max(e0.x + e0.w, e1.x + e1.w),
                  max(e0.y + e0.w, e1.y + e1.w),
                  max(e0.z + e0.w, e1.z + e1.w));
}

[ForceInline]
void unpack_triangle(float4* buffer, uint idx, out float3 v0, out float3 v1,
                     out float3 v2, out float2 uv0, out float2 uv1,
                     out float2 uv2, out uint cam_idx) {
    float4 e0 = buffer[4 * idx + 0];
    float4 e1 = buffer[4 * idx + 1];
    float4 e2 = buffer[4 * idx + 2];
    float4 e3 = buffer[4 * idx + 3];
    v0 = float3(e0.x, e0.y, e0.z);
    v1 = float3(e0.w, e1.x, e1.y);
    v2 = float3(e1.z, e1.w, e2.x);
    uv0 = float2(e2.y, e2.z);
    uv1 = float2(e2.w, e3.x);
    uv2 = float2(e3.y, e3.z);
    cam_idx = asuint(e3.w);
}

[ForceInline]
void elem_aabb_of(float4* buffer, uint idx, out float3 bmin,
                  out float3 bmax) {
    if (kVisPrim == 0) {
        lss_aabb_of(buffer, idx, bmin, bmax);
    } else {
        float3 v0, v1, v2;
        float2 uv0, uv1, uv2;
        uint ci;
        unpack_triangle(buffer, idx, v0, v1, v2, uv0, uv1, uv2, ci);
        bmin = min(min(v0, v1), v2);
        bmax = max(max(v0, v1), v2);
        // A frustum face is planar, so an axis-aligned one gives a
        // zero-thickness box that the slab test can never report a hit for --
        // see the CUDA source. Pad it out of degeneracy.
        float m = max(max(max(abs(bmin.x), abs(bmin.y)), abs(bmin.z)),
                      max(max(abs(bmax.x), abs(bmax.y)), abs(bmax.z)));
        float pad = kTriAabbPad * max(m, 1e-20f);
        bmin -= pad;
        bmax += pad;
    }
}

// ---------------------------------------------------------------------------
// Root AABB reduction (mapped-u32 atomics; subgroup pre-reduce).
// aabb_reduced: u32[6] = {min xyz (mapped), max xyz (mapped)}; host
// initializes min words to 0xffffffff and max words to 0, decodes after
// readback with funmap_ordered.
// ---------------------------------------------------------------------------

struct VisAabbParams {  // 28 -> 32 bytes: pushed directly
    float4* buffer;
    uint* aabb_reduced;  // [6] mapped
    uint32_t num_elem;
    uint32_t wgs_per_row;
    uint32_t _pad0;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void vis_compute_aabb(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                      uniform VisAabbParams p) {
    uint gid = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (gid >= p.num_elem) return;

    float3 bmin, bmax;
    elem_aabb_of(p.buffer, gid, bmin, bmax);

    // subgroup pre-reduction (size-agnostic), then one atomic per subgroup
    float3 smin = WaveActiveMin(bmin);
    float3 smax = WaveActiveMax(bmax);
    if (WaveIsFirstLane()) {
        InterlockedMin(p.aabb_reduced[0], fmap_ordered(smin.x));
        InterlockedMin(p.aabb_reduced[1], fmap_ordered(smin.y));
        InterlockedMin(p.aabb_reduced[2], fmap_ordered(smin.z));
        InterlockedMax(p.aabb_reduced[3], fmap_ordered(smax.x));
        InterlockedMax(p.aabb_reduced[4], fmap_ordered(smax.y));
        InterlockedMax(p.aabb_reduced[5], fmap_ordered(smax.z));
    }
}

// ---------------------------------------------------------------------------
// Morton sorting keys. Mirrors fill_sorting_keys_kernel + get_morton_key.
// ---------------------------------------------------------------------------

static const uint kMortonBitsPerDim = 21;

#ifndef SS_EMULATE_INT64

[ForceInline]
uint64_t insert_2_zeros_between_bits(uint64_t x) {
    x = (x | (x << 32)) & 0xFFFF00000000FFFFULL;
    x = (x | (x << 16)) & 0x00FF0000FF0000FFULL;
    x = (x | (x << 8)) & 0x100F00F00F00F00FULL;
    x = (x | (x << 4)) & 0x10C30C30C30C30C3ULL;
    x = (x | (x << 2)) & 0x1249249249249249ULL;
    return x;
}

[ForceInline]
u64key_t get_morton_key(float3 pos) {
    const uint64_t mask_comp = (uint64_t(1) << uint64_t(kMortonBitsPerDim)) - 1;
    float scale = exp2(float(kMortonBitsPerDim));
    uint64_t x = uint64_t(pos.x * scale + 0.5f) & mask_comp;
    uint64_t y = uint64_t(pos.y * scale + 0.5f) & mask_comp;
    uint64_t z = uint64_t(pos.z * scale + 0.5f) & mask_comp;
    const uint64_t mask_full =
        ((uint64_t(1) << uint64_t(3 * kMortonBitsPerDim)) - 1);
    x = insert_2_zeros_between_bits(x) & mask_full;
    y = insert_2_zeros_between_bits(y) & mask_full;
    z = insert_2_zeros_between_bits(z) & mask_full;
    return (x * 2 + y) * 2 + z;
}

#else  // SS_EMULATE_INT64: same 63-bit codes in a (lo, hi) word pair

// Classic 32-bit 3D spread: bits 0..9 -> bits 0, 3, ..., 27.
[ForceInline]
uint _vis_spread10(uint x) {
    x = (x | (x << 16)) & 0x030000FFu;
    x = (x | (x << 8)) & 0x0300F00Fu;
    x = (x | (x << 4)) & 0x030C30C3u;
    x = (x | (x << 2)) & 0x09249249u;
    return x;
}

// 21-bit component -> bit 3*i of the 63-bit code. Bits 0..10 land in the lo
// word (3*10 = 30), bits 11..20 in the hi word at 3*i - 32 = 1, 4, ..., 28.
[ForceInline]
uint2 _vis_spread21(uint x) {
    uint lo = _vis_spread10(x & 0x3ffu) | (((x >> 10) & 1u) << 30);
    uint hi = _vis_spread10((x >> 11) & 0x3ffu) << 1;
    return uint2(lo, hi);
}

[ForceInline]
uint2 _vis_shl(uint2 v, uint k) {  // k in 1..31
    return uint2(v.x << k, (v.y << k) | (v.x >> (32u - k)));
}

// Bit-identical to the native path: x at bit 3i+2, y at 3i+1, z at 3i.
[ForceInline]
u64key_t get_morton_key(float3 pos) {
    const uint mask_comp = (1u << kMortonBitsPerDim) - 1u;
    float scale = exp2(float(kMortonBitsPerDim));
    uint x = uint(pos.x * scale + 0.5f) & mask_comp;
    uint y = uint(pos.y * scale + 0.5f) & mask_comp;
    uint z = uint(pos.z * scale + 0.5f) & mask_comp;
    uint2 mx = _vis_shl(_vis_spread21(x), 2u);
    uint2 my = _vis_shl(_vis_spread21(y), 1u);
    uint2 mz = _vis_spread21(z);
    return uint2(mx.x | my.x | mz.x, mx.y | my.y | mz.y);
}

#endif  // SS_EMULATE_INT64

struct VisMortonParams {  // 48 bytes: pushed directly
    float4* buffer;
    u64key_t* splat_keys;
    // root AABB as scalars (vector members would change the push layout)
    float rmin_x, rmin_y, rmin_z;
    float rmax_x, rmax_y, rmax_z;
    uint32_t num_elem;
    uint32_t wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void vis_fill_morton(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                     uniform VisMortonParams p) {
    uint gid = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (gid >= p.num_elem) return;
    float3 bmin, bmax;
    elem_aabb_of(p.buffer, gid, bmin, bmax);
    float3 center = (bmin + bmax) * 0.5f;
    float3 rmin = float3(p.rmin_x, p.rmin_y, p.rmin_z);
    float3 rmax = float3(p.rmax_x, p.rmax_y, p.rmax_z);
    p.splat_keys[gid] = get_morton_key((center - rmin) / (rmax - rmin));
}

struct VisIdentityParams {  // 20 -> 24 bytes: pushed directly
    int* data;
    uint32_t n;
    uint32_t wgs_per_row;
    uint32_t _pad0;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void vis_fill_identity(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                       uniform VisIdentityParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx < p.n) p.data[idx] = int(idx);
}

// ---------------------------------------------------------------------------
// Karras LBVH internal nodes. Mirrors fill_lbvh_internal_nodes_kernel.
// ---------------------------------------------------------------------------

[ForceInline]
int clz32(uint x) {
    if (x == 0) return 32;
    return 31 - int(firstbithigh(x));
}

struct VisLbvhNodesParams {  // 40 bytes: pushed directly
    u64key_t* morton;      // [n] sorted
    int* element_indices;  // [n] argsort
    int2* internal_nodes;  // [n-1]
    int* parent_nodes;     // [n-1], -1 initialized
    uint32_t num_elements;
    uint32_t wgs_per_row;
};

[ForceInline]
int lbvh_delta(u64key_t* morton, uint num_elements, int i, int j) {
    if (j < 0 || j >= int(num_elements)) return -1;
    if (u64key_eq(morton[i], morton[j]))
        return 64 + clz32(uint(i) ^ uint(j));
    return u64key_clz(u64key_xor(morton[i], morton[j]));
}

[shader("compute")]
[numthreads(256, 1, 1)]
void vis_lbvh_nodes(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                    uniform VisLbvhNodesParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.num_elements - 1) return;
    int i = int(idx);

    #define delta(a, b) lbvh_delta(p.morton, p.num_elements, a, b)

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
    int s = 0;
    // CUDA probes t = ceil(l/2), ceil(l/4), ... and terminates only through
    // signed tf overflow / division by zero (UB that happens to exit on
    // NVIDIA and llvmpipe but spins forever on Intel — device lost). The
    // ceil-halving form probes the same t sequence down to 1, where the
    // split search has provably converged, so the result is identical.
    if (l > 0) {
        int t = (l + 1) / 2;
        while (true) {
            if (delta(i, i + (s + t) * d) > delta_node)
                s += t;
            if (t == 1) break;
            t = (t + 1) / 2;
        }
    }
    int gamma = i + s * d + min(d, 0);

    int left = min(i, j) == gamma ? ~p.element_indices[gamma] : gamma;
    int right =
        max(i, j) == gamma + 1 ? ~p.element_indices[gamma + 1] : gamma + 1;
    p.internal_nodes[i] = int2(left, right);

    if (left >= 0) InterlockedMax(p.parent_nodes[left], i);
    if (right >= 0) InterlockedMax(p.parent_nodes[right], i);

    #undef delta
}

// ---------------------------------------------------------------------------
// Tree AABB init + bottom-up fill, in MAPPED u32 space, then an unmap pass.
// treeAABB layout: [2*num_cells] float3 == u32 words in mapped space.
// ---------------------------------------------------------------------------

struct VisTreeInitParams {  // 20 -> 24 bytes: pushed directly
    uint* treeAABB;  // [num_cells * 2 * 3] mapped words
    uint32_t num_cells;
    uint32_t wgs_per_row;
    uint32_t _pad0;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void vis_tree_init_aabb(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                        uniform VisTreeInitParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.num_cells) return;
    uint hi = fmap_ordered(1e10f), lo = fmap_ordered(-1e10f);
    [ForceUnroll]
    for (int k = 0; k < 3; k++) {
        p.treeAABB[(2 * idx) * 3 + k] = hi;      // min slot starts high
        p.treeAABB[(2 * idx + 1) * 3 + k] = lo;  // max slot starts low
    }
}

struct VisLbvhAabbParams {  // 44 -> 48 bytes: pushed directly
    float4* buffer;
    int2* internal_nodes;
    int* parent_nodes;
    uint* treeAABB;  // mapped words
    uint32_t num_elements;
    uint32_t wgs_per_row;
    uint32_t _pad0;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void vis_lbvh_aabb(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                   uniform VisLbvhAabbParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.num_elements - 1) return;
    int i = int(idx);

    int2 children = p.internal_nodes[i];
    if (children.x >= 0 && children.y >= 0) return;

    float3 aabb_min = float3(0.0f), aabb_max = float3(0.0f);
    if (children.x < 0)
        elem_aabb_of(p.buffer, uint(~children.x), aabb_min, aabb_max);
    if (children.y < 0) {
        float3 bmin1, bmax1;
        elem_aabb_of(p.buffer, uint(~children.y), bmin1, bmax1);
        if (children.x < 0) {
            aabb_min = min(aabb_min, bmin1);
            aabb_max = max(aabb_max, bmax1);
        } else {
            aabb_min = bmin1;
            aabb_max = bmax1;
        }
    }

    uint mmin[3] = {fmap_ordered(aabb_min.x), fmap_ordered(aabb_min.y),
                    fmap_ordered(aabb_min.z)};
    uint mmax[3] = {fmap_ordered(aabb_max.x), fmap_ordered(aabb_max.y),
                    fmap_ordered(aabb_max.z)};

    // Climb while this thread still improves any bound (same all-six
    // non-short-circuit atomics as the CUDA kernel; mapped comparisons
    // preserve float ordering).
    while (true) {
        uint o0, o1, o2, o3, o4, o5;
        InterlockedMin(p.treeAABB[(2 * i) * 3 + 0], mmin[0], o0);
        InterlockedMin(p.treeAABB[(2 * i) * 3 + 1], mmin[1], o1);
        InterlockedMin(p.treeAABB[(2 * i) * 3 + 2], mmin[2], o2);
        InterlockedMax(p.treeAABB[(2 * i + 1) * 3 + 0], mmax[0], o3);
        InterlockedMax(p.treeAABB[(2 * i + 1) * 3 + 1], mmax[1], o4);
        InterlockedMax(p.treeAABB[(2 * i + 1) * 3 + 2], mmax[2], o5);
        bool improved_none = (o0 < mmin[0]) & (o1 < mmin[1]) &
                             (o2 < mmin[2]) & (o3 > mmax[0]) &
                             (o4 > mmax[1]) & (o5 > mmax[2]);
        if (improved_none) break;
        i = p.parent_nodes[i];
        if (i < 0) break;
    }
}

struct VisUnmapParams {  // 20 -> 24 bytes: pushed directly
    uint* words;  // mapped -> float bits, in place
    uint32_t n;
    uint32_t wgs_per_row;
    uint32_t _pad0;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void vis_unmap_aabb(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                    uniform VisUnmapParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.n) return;
    p.words[idx] = asuint(funmap_ordered(p.words[idx]));
}

// ---------------------------------------------------------------------------
// Depth image min/max (for the c==1 colormap path). Output is MAPPED u32;
// the blit decodes it. Host initializes min word to 0xffffffff, max to 0.
// ---------------------------------------------------------------------------

struct VisMinMaxParams {  // 28 -> 32 bytes: pushed directly
    float* depths;  // [H,W,1]
    uint* min_max;  // [2] mapped
    uint32_t total;
    uint32_t wgs_per_row;
    uint32_t _pad0;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void vis_min_max(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                 uniform VisMinMaxParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    float amin = 1e38f, amax = -1e38f;
    if (idx < p.total) {
        float z = p.depths[idx];
        if (isfinite(z)) {
            amin = min(amin, z);
            amax = max(amax, z);
        }
    }
    float smin = WaveActiveMin(amin);
    float smax = WaveActiveMax(amax);
    if (WaveIsFirstLane()) {
        InterlockedMin(p.min_max[0], fmap_ordered(smin));
        InterlockedMax(p.min_max[1], fmap_ordered(smax));
    }
}

// ---------------------------------------------------------------------------
// Ray-primitive intersections (verbatim ports).
// ---------------------------------------------------------------------------

[ForceInline]
float ray_aabb_intersection(float3 bc, float3 br, float3 ro, float3 rd,
                            float t0, float t1) {
    float3 n = (bc - ro) / rd;
    float3 k = br / float3(abs(rd.x), abs(rd.y), abs(rd.z));
    float tmin = max(n.x - k.x, max(n.y - k.y, n.z - k.z));
    float tmax = min(n.x + k.x, min(n.y + k.y, n.z + k.z));
    bool intersect = tmin < tmax && tmax > t0 && tmin < t1;
    return intersect ? (tmin > t0 ? tmin : tmax) : -1.0f;
}

[ForceInline]
float ray_linear_swept_sphere_intersection(float3 pa, float3 pb, float ra,
                                           float rb, float3 ro, float3 rd,
                                           float t0, float t1) {
    // https://www.shadertoy.com/view/MlKfzm by Inigo Quilez, MIT license
    float3 ba = pb - pa, oa = ro - pa, ob = ro - pb;
    float rr = ra - rb;
    float m0 = dot(ba, ba), m1 = dot(ba, oa), m2 = dot(ba, rd),
          m3 = dot(rd, oa), m5 = dot(oa, oa), m6 = dot(ob, rd),
          m7 = dot(ob, ob);
    float d2 = m0 - rr * rr;
    float k2 = d2 - m2 * m2;
    float k1 = d2 * m3 - m1 * m2 + m2 * rr * ra;
    float k0 = d2 * m5 - m1 * m1 + m1 * rr * ra * 2.0f - m0 * ra * ra;
    float h = k1 * k1 - k0 * k2;
    if (h < 0.0f) return -1.0f;
    float t = (-sqrt(h) - k1) / k2;
    float y = m1 - ra * rr + t * m2;
    if (y > 0.0f && y < d2)
        return (t - t0) * (t - t1) < 0.0f ? t : -1.0f;
    float h1 = m3 * m3 - m5 + ra * ra;
    float h2 = m6 * m6 - m7 + rb * rb;
    if (max(h1, h2) < 0.0f) return -1.0f;
    float r = 1e20f;
    if (h1 > 0.0f) r = -m3 - sqrt(h1);
    if (h2 > 0.0f) {
        float t = -m6 - sqrt(h2);
        if ((t - t0) * (t - t1) < 0.0f && t < r) r = t;
    }
    return (r - t0) * (r - t1) < 0.0f ? r : -1.0f;
}

[ForceInline]
bool ray_triangle_intersection(float4* tri_buffer, uint idx, float3 ro,
                               float3 rd, float t0, float t1, out float out_t,
                               out uint out_cam_idx, out float2 out_uv) {
    float3 v0, v1, v2;
    float2 uv0, uv1, uv2;
    unpack_triangle(tri_buffer, idx, v0, v1, v2, uv0, uv1, uv2, out_cam_idx);
    float3 po = v0 - ro;
    float3 a = v1 - v0;
    float3 b = v2 - v0;
    float invdet = 1.0f / dot(rd, cross(a, b));
    float t = dot(po, cross(a, b)) * invdet;
    float u = dot(rd, cross(b, po)) * invdet;
    float v = dot(rd, cross(po, a)) * invdet;
    float w = 1.0f - u - v;
    bool intersect = (t > t0 && t < t1 && u > 0.0f && v > 0.0f && w > 0.0f);
    out_t = t;
    out_uv = uv0 * w + uv1 * u + uv2 * v;
    return intersect;
}

// ---------------------------------------------------------------------------
// Turbo colormap (the only one the blit kernel evaluates).
// ---------------------------------------------------------------------------

static const float COEFFS_turbo[3][8] = {
    {0.14637796, 2.94711014, -10.15061040, -90.46877071, 550.44382083,
     -1061.31232675, 874.85901369, -266.03287948},
    {0.08594198, 2.06520532, 9.64581379, -55.09180383, 149.40813531,
     -245.26823636, 202.98596195, -63.82163439},
    {0.23431523, 6.33792818, 9.26380342, -203.76964931, 604.07329733,
     -766.82678386, 447.99287080, -97.24397473},
};

[ForceInline]
float3 colormap_turbo(float t) {
    float3 c = float3(0.0f);
    [ForceUnroll]
    for (int k = 7; k >= 0; --k)
        c = c * t +
            float3(COEFFS_turbo[0][k], COEFFS_turbo[1][k], COEFFS_turbo[2][k]);
    return c;
}

// ---------------------------------------------------------------------------
// Thumbnail bilinear tap (RGBA8 words). Mirrors get_thumbnail_bilinear,
// including the integer 256-step weights.
// ---------------------------------------------------------------------------

[ForceInline]
float3 get_thumbnail_bilinear(uint* thumb_words, uint TW, uint TH, uint bid,
                              float2 uv) {
    const int W = int(TW), H = int(TH);
    float x = clamp(uv.x * float(W) + 0.5f, 0.0f, float(W - 1));
    float y = clamp(uv.y * float(H) + 0.5f, 0.0f, float(H - 1));
    int x0 = int(floor(x));
    int x1 = x0 + 1;
    int y0 = int(floor(y));
    int y1 = y0 + 1;
    int wx1 = int(256.0f * (x - float(x0)));
    int wx0 = 256 - wx1;
    int wy1 = int(256.0f * (y - float(y0)));
    int wy0 = 256 - wy1;

    // x1/y1 can reach W/H only when the fractional weight is exactly 0 (the
    // CUDA kernel reads the out-of-row texel and multiplies by 0); clamp the
    // index instead — weight 0 keeps the result identical.
    uint base = bid * TW * TH;
    uint xc1 = uint(min(x1, W - 1)), yc1 = uint(min(y1, H - 1));
    uint c00 = thumb_words[base + uint(y0) * TW + uint(x0)];
    uint c10 = thumb_words[base + uint(y0) * TW + xc1];
    uint c01 = thumb_words[base + yc1 * TW + uint(x0)];
    uint c11 = thumb_words[base + yc1 * TW + xc1];

    int w00 = wx0 * wy0, w10 = wx1 * wy0, w01 = wx0 * wy1, w11 = wx1 * wy1;
    #define _CH(word, sh) int((word >> sh) & 0xffu)
    float4 rgba = float4(
        float(_CH(c00, 0) * w00 + _CH(c10, 0) * w10 + _CH(c01, 0) * w01 +
              _CH(c11, 0) * w11),
        float(_CH(c00, 8) * w00 + _CH(c10, 8) * w10 + _CH(c01, 8) * w01 +
              _CH(c11, 8) * w11),
        float(_CH(c00, 16) * w00 + _CH(c10, 16) * w10 + _CH(c01, 16) * w01 +
              _CH(c11, 16) * w11),
        float(_CH(c00, 24) * w00 + _CH(c10, 24) * w10 + _CH(c01, 24) * w01 +
              _CH(c11, 24) * w11)) *
        (1.0f / (255.0f * 256.0f * 256.0f));
    #undef _CH
    // Uncaptured thumbnail (alpha 0) -> magenta
    if (rgba.w < 0.5f) {
        int c = int(floor(uv.x * 4.0f)) + int(floor(uv.y * 4.0f));
        return (c & 1) != 0 ? float3(0.85f, 0.05f, 0.85f)
                            : float3(0.15f, 0.0f, 0.15f);
    }
    return float3(rgba.x * rgba.w, rgba.y * rgba.w, rgba.z * rgba.w);
}

// ---------------------------------------------------------------------------
// The blit. One 32-thread workgroup per 8x4 pixel tile, grid
// (ceil(W/8), ceil(H/4), 1). Writes per-pixel RGBA words to out_rgba
// scratch; vis_pack_rgb8 packs the [H,W,3] byte image afterwards.
// ---------------------------------------------------------------------------

struct VisBlitParams {  // 16 ptrs + 11 scalars: via params ring
    float* render_rgbs;    // [H,W,C] C in {1,3}
    float* render_depths;  // [H,W,1]
    float* render_alphas;  // [H,W,1]
    float4* view_intrins;  // [4] (one camera)
    float* view_viewmat;   // [4,4]
    float* view_dist;      // [8] or null
    float4* lss_buffer;    // or null
    int2* lss_nodes;
    float3* lss_aabb;
    float4* tri_buffer;    // or null
    int2* tri_nodes;
    float3* tri_aabb;
    float* overlay_colors;  // [M,3] (overlay part only) or null
    uint* thumbnails;       // [N,S,S] RGBA words
    uint* min_max;          // [2] mapped (see vis_min_max) or null
    uint* out_rgba;         // [H,W] RGBA words (A ignored)
    int32_t view_camera_model;
    int32_t width;
    int32_t height;
    int32_t rgb_channels;  // 1 -> turbo colormap, 3 -> passthrough
    int32_t num_cam_lss;
    int32_t show_cams;
    int32_t show_overlay;
    int32_t thumb_w;
    int32_t thumb_h;
    int32_t has_lss;   // lss_buffer/nodes/aabb valid (flag, not ptr-null:
    int32_t has_tri;   // see backend README on speculated loads)
    int32_t alpha_stride;  // per-pixel element stride of render_alphas (=
                           // its channel count); channel 0 read for opacity
};

static const float kGrayThreshold = 0.1657f;

groupshared float _blit_gray[32];
groupshared float _blit_fit[4];  // gray_a, gray_b, gray_c(fit), num_gray
groupshared uint _blit_stack[32][33];

[shader("compute")]
[numthreads(32, 1, 1)]
void vis_blit(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
              uniform VisBlitParams* pp) {
    VisBlitParams p = *pp;
    const uint tx = tid % 8, ty = tid / 8;
    const int pix_x = int(wg.x * 8 + tx);
    const int pix_y = int(wg.y * 4 + ty);

    // clamped render color (all 32 threads participate in the gray fit)
    int cx_ = min(pix_x, p.width - 1), cy2 = min(pix_y, p.height - 1);
    float3 render_rgb;
    if (p.rgb_channels == 1) {
        float amin = funmap_ordered(p.min_max[0]);
        float amax = funmap_ordered(p.min_max[1]);
        float z = p.render_rgbs[cy2 * p.width + cx_];
        z = (z - amin) / (amax - amin);
        render_rgb = colormap_turbo(z);
    } else {
        uint o = uint(cy2 * p.width + cx_) * 3;
        render_rgb = float3(p.render_rgbs[o], p.render_rgbs[o + 1],
                            p.render_rgbs[o + 2]);
    }

    // Camera-border black/white decision: least-squares plane fit of the
    // tile's grayscale (CUDA warp reduce -> serial groupshared reduction).
    const float3 rgb2gray = float3(0.299f, 0.587f, 0.114f);
    float gray_a = 0.0f, gray_b = 0.0f, gray_c = dot(render_rgb, rgb2gray);
    _blit_gray[tid] = gray_c;
    GroupMemoryBarrierWithGroupSync();
    if (tid == 0) {
        float sxg = 0.0f, syg = 0.0f, sg = 0.0f;
        int num_gray = 0;
        for (int k = 0; k < 32; k++) {
            float g = _blit_gray[k];
            float xx = float(k % 8) * (1.0f / 8.0f);
            float yy = float(k / 8) * (1.0f / 4.0f);
            sxg += xx * g;
            syg += yy * g;
            sg += g;
            if (g >= kGrayThreshold) num_gray++;
        }
        _blit_fit[0] = 3.809523809523809e-01f * sxg -
                       1.666666666666667e-01f * sg;
        _blit_fit[1] = 3.999999999999999e-01f * syg -
                       1.500000000000000e-01f * sg;
        _blit_fit[2] = -1.666666666666667e-01f * sxg -
                       1.500000000000000e-01f * syg +
                       1.604166666666667e-01f * sg;
        _blit_fit[3] = float(num_gray);
    }
    GroupMemoryBarrierWithGroupSync();
    {
        int num_gray = int(_blit_fit[3]);
        if (num_gray > 0 && num_gray < 32) {
            gray_a = _blit_fit[0];
            gray_b = _blit_fit[1];
            gray_c = _blit_fit[2];
        }
    }

    if (pix_x >= p.width || pix_y >= p.height) return;

    float depth = p.render_depths[pix_y * p.width + pix_x];
    float opac = p.render_alphas[(pix_y * p.width + pix_x) * p.alpha_stride];
    depth /= min(max(opac, 1e-6f) / 0.5f, 1.0f);
    if (depth == 0.0f) depth = 1e10f;

    const int MSAA = 2;
    float4 intrin = p.view_intrins[0];
    float fx = intrin.x, fy = intrin.y, cx = intrin.z, cy = intrin.w;
    float3x3 R;
    [ForceUnroll]
    for (int r = 0; r < 3; r++)
        R[r] = float3(p.view_viewmat[4 * r + 0], p.view_viewmat[4 * r + 1],
                      p.view_viewmat[4 * r + 2]);
    float3 t = float3(p.view_viewmat[3], p.view_viewmat[7],
                      p.view_viewmat[11]);
    float dist[kDistCoeffRow];
    [ForceUnroll]
    for (int k = 0; k < kDistCoeffRow; k++)
        dist[k] = is_null(p.view_dist) ? 0.0f : p.view_dist[k];

    float alpha_final = 0.0f;
    float3 rgb_final = float3(0.0f);

    for (int i = 0; i < MSAA * MSAA; ++i) {
        const float px = float(pix_x) + (float(i / MSAA) + 0.5f) / MSAA;
        const float py = float(pix_y) + (float(i % MSAA) + 0.5f) / MSAA;
        float gray = gray_a * (px * (1.0f / 8.0f) - float(wg.x)) +
                     gray_b * (py * (1.0f / 4.0f) - float(wg.y)) + gray_c;
        float3 ray_o = transform_ray_o(R, t);
        float3 ray_d;
        float2 uv = float2((px - cx) / fx, (py - cy) / fy);
        bool inside = vis_ray(uv, p.view_camera_model, dist, ray_d);
        if (p.view_camera_model == 1 /*FISHEYE*/)
            inside = inside && length(uv) < 3.14159265358979323846f;
        if (p.view_camera_model == 2 /*EQUISOLID*/)
            inside = inside && length(uv) < 2.0f;
        ray_d = transform_ray_d(R, ray_d);
        if (!inside) continue;

        float3 rgb = float3(0.0f);
        float alpha = 0.0f;
        float tmax = depth;

        // BVH traversal: linear swept spheres
        if (p.has_lss != 0) {
            uint stackSize = 0;
            float3 bmin = p.lss_aabb[0], bmax = p.lss_aabb[1];
            if (ray_aabb_intersection(0.5f * (bmin + bmax),
                                      0.5f * (bmax - bmin), ray_o, ray_d,
                                      0.0f, tmax) > 0.0f) {
                _blit_stack[tid][stackSize] = 0u;
                stackSize++;
            }
            for (uint _steps = 0; _steps < 65536; _steps++) {
                if (stackSize == 0) break;
                --stackSize;
                uint elem = _blit_stack[tid][stackSize];
                int2 node = p.lss_nodes[elem];
                [ForceUnroll]
                for (uint ci = 0; ci < 2; ci++) {
                    int childIdx = ci == 0 ? node.x : node.y;
                    if (childIdx < 0) {
                        int idx = ~childIdx;
                        bool overlay = idx >= p.num_cam_lss;
                        if (overlay ? (p.show_overlay == 0)
                                    : (p.show_cams == 0))
                            continue;
                        float4 e0 = p.lss_buffer[2 * idx];
                        float4 e1 = p.lss_buffer[2 * idx + 1];
                        float t_temp = ray_linear_swept_sphere_intersection(
                            float3(e0.x, e0.y, e0.z),
                            float3(e1.x, e1.y, e1.z), e0.w, e1.w, ray_o,
                            ray_d, 0.0f, tmax);
                        if (t_temp > 0.0f) {
                            tmax = t_temp;
                            if (overlay) {
                                float* c = p.overlay_colors +
                                           3 * (idx - p.num_cam_lss);
                                rgb = float3(c[0], c[1], c[2]);
                            } else {
                                rgb = float3(
                                    gray >= kGrayThreshold ? 0.1f : 0.85f);
                            }
                            alpha = 1.0f;
                        }
                    } else {
                        float3 nmin = p.lss_aabb[2 * childIdx];
                        float3 nmax = p.lss_aabb[2 * childIdx + 1];
                        if (ray_aabb_intersection(
                                0.5f * (nmin + nmax), 0.5f * (nmax - nmin),
                                ray_o, ray_d, 0.0f, tmax) > 0.0f &&
                            stackSize < 33) {
                            _blit_stack[tid][stackSize] = uint(childIdx);
                            stackSize += 1;
                        }
                    }
                }
            }
        }

        // BVH traversal: thumbnail triangles
        if (p.has_tri != 0) {
            uint stackSize = 0;
            float3 bmin = p.tri_aabb[0], bmax = p.tri_aabb[1];
            if (ray_aabb_intersection(0.5f * (bmin + bmax),
                                      0.5f * (bmax - bmin), ray_o, ray_d,
                                      0.0f, tmax) > 0.0f) {
                _blit_stack[tid][stackSize] = 0u;
                stackSize++;
            }
            for (uint _steps = 0; _steps < 65536; _steps++) {
                if (stackSize == 0) break;
                --stackSize;
                uint elem = _blit_stack[tid][stackSize];
                int2 node = p.tri_nodes[elem];
                [ForceUnroll]
                for (uint ci = 0; ci < 2; ci++) {
                    int childIdx = ci == 0 ? node.x : node.y;
                    if (childIdx < 0) {
                        float t_temp;
                        uint cam_idx;
                        float2 uv2;
                        if (ray_triangle_intersection(
                                p.tri_buffer, uint(~childIdx), ray_o, ray_d,
                                0.0f, tmax, t_temp, cam_idx, uv2)) {
                            tmax = t_temp;
                            rgb = get_thumbnail_bilinear(
                                p.thumbnails, uint(p.thumb_w),
                                uint(p.thumb_h), cam_idx, uv2);
                            alpha = 1.0f;
                        }
                    } else {
                        float3 nmin = p.tri_aabb[2 * childIdx];
                        float3 nmax = p.tri_aabb[2 * childIdx + 1];
                        if (ray_aabb_intersection(
                                0.5f * (nmin + nmax), 0.5f * (nmax - nmin),
                                ray_o, ray_d, 0.0f, tmax) > 0.0f &&
                            stackSize < 33) {
                            _blit_stack[tid][stackSize] = uint(childIdx);
                            stackSize += 1;
                        }
                    }
                }
            }
        }

        float a = alpha / float(MSAA * MSAA);
        alpha_final += a;
        rgb_final += rgb * a;
    }

    if (alpha_final > 0.0f)
        render_rgb = render_rgb * (1.0f - alpha_final) + rgb_final;
    render_rgb =
        255.0f * min(max(render_rgb, float3(0.0f)), float3(1.0f)) + 0.5f;
    uint r8 = uint(render_rgb.x), g8 = uint(render_rgb.y),
         b8 = uint(render_rgb.z);
    p.out_rgba[pix_y * p.width + pix_x] = r8 | (g8 << 8) | (b8 << 16);
}

// ---------------------------------------------------------------------------
// Pack the RGBA-word scratch into the caller's [H,W,3] uint8 buffer: one
// thread per OUTPUT WORD (4 bytes; the byte tail spills into the 4-byte
// rounding of device allocations).
// ---------------------------------------------------------------------------

struct VisPackParams {  // 24 bytes: pushed directly
    uint* rgba;      // [H*W] words
    uint* out_rgb8;  // ceil(3*H*W/4) words
    uint32_t total_bytes;  // 3*H*W
    uint32_t wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void vis_pack_rgb8(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                   uniform VisPackParams p) {
    uint w = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (4 * w >= p.total_bytes) return;
    uint num_pix = p.total_bytes / 3;
    uint word = 0;
    [ForceUnroll]
    for (uint k = 0; k < 4; k++) {
        uint byte_idx = 4 * w + k;
        // tail bytes past 3*H*W read pixel 0 (they land in the output
        // buffer's 4-byte rounding and are never consumed)
        uint pix = byte_idx < p.total_bytes ? byte_idx / 3 : 0;
        uint ch = byte_idx % 3;
        uint v = (p.rgba[pix] >> (8 * ch)) & 0xffu;
        word |= v << (8 * k);
    }
    p.out_rgb8[w] = word;
}

// ---------------------------------------------------------------------------
// Thumbnail capture: one workgroup (128 threads) per post batch entry.
// Mirrors update_thumbnails_kernel; RGBA stores are words, the done mask is
// a byte mask written via InterlockedOr on its containing word.
// ---------------------------------------------------------------------------

struct VisThumbParams {  // 5 ptrs + 10 scalars = 80 bytes: pushed directly
    uint* rgb;           // [B_post, H_rgb, W_rgb, 3], read as rgb_fmt
    int* cam_indices;    // [B_post]
    uint* thumbnails;    // [N,S,S] RGBA words
    uint* done_mask;     // [ceil(N/4)] byte-per-camera words
    uint* alpha_mask;    // [B_post, H_a, W_a] bool bytes as words, or null
    int32_t H_rgb;
    int32_t W_rgb;
    int32_t B_post;
    int32_t N;
    int32_t S;
    int32_t H_alpha;
    int32_t W_alpha;
    int32_t has_alpha;  // alpha_mask valid (flag, not ptr-null)
    uint rgb_fmt;       // core/PixelFormat.h
    int32_t _pad0;
};

[ForceInline]
float3 thumb_bilinear_sample_f3(uint* src, uint fmt, int b, int x_dst,
                                int y_dst, int W_dst, int H_dst, int W_src,
                                int H_src) {
    // Mirrors Interpolation.cuh resolve_taps + bilinear_sample_f3.
    float u = (float(x_dst) + 0.5f) * float(W_src) / float(W_dst) - 0.5f;
    float v = (float(y_dst) + 0.5f) * float(H_src) / float(H_dst) - 0.5f;
    int x0r = int(floor(u)), y0r = int(floor(v));
    float fx = u - float(x0r), fy = v - float(y0r);
    int x0 = max(0, min(W_src - 1, x0r));
    int y0 = max(0, min(H_src - 1, y0r));
    int x1 = max(0, min(W_src - 1, x0r + 1));
    int y1 = max(0, min(H_src - 1, y0r + 1));
    float w00 = (1.0f - fx) * (1.0f - fy);
    float w10 = fx * (1.0f - fy);
    float w01 = (1.0f - fx) * fy;
    float w11 = fx * fy;
    gindex_t plane = gindex_t(b) * gindex_t(H_src) * gindex_t(W_src);
    #define _LD(yy, xx) rgb_load3(src, plane + gindex_t(yy) * W_src + (xx), fmt)
    float3 v00 = _LD(y0, x0), v10 = _LD(y0, x1), v01 = _LD(y1, x0),
           v11 = _LD(y1, x1);
    #undef _LD
    return float3(w00 * v00.x + w10 * v10.x + w01 * v01.x + w11 * v11.x,
                  w00 * v00.y + w10 * v10.y + w01 * v01.y + w11 * v11.y,
                  w00 * v00.z + w10 * v10.z + w01 * v01.z + w11 * v11.z);
}

[ForceInline]
bool thumb_nearest_sample_b(uint* src_words, int b, int x_dst, int y_dst,
                            int W_dst, int H_dst, int W_src, int H_src) {
    float u = (float(x_dst) + 0.5f) * float(W_src) / float(W_dst) - 0.5f;
    float v = (float(y_dst) + 0.5f) * float(H_src) / float(H_dst) - 0.5f;
    int x = max(0, min(W_src - 1, int(floor(u + 0.5f))));
    int y = max(0, min(H_src - 1, int(floor(v + 0.5f))));
    gindex_t byte_idx = gindex_t(b) * gindex_t(H_src) * gindex_t(W_src) +
                        gindex_t(y) * W_src + x;
    return u8_load(src_words, byte_idx) == 1u;
}

[shader("compute")]
[numthreads(128, 1, 1)]
void vis_update_thumbnails(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                           uniform VisThumbParams p) {
    int b = int(wg.x);
    if (b >= p.B_post) return;
    int post_idx = p.cam_indices[b];
    if (post_idx < 0 || post_idx >= p.N) return;
    if (u8_load(p.done_mask, post_idx) != 0) return;

    int S = p.S;
    int s_total = S * S;
    uint dst = uint(post_idx) * uint(S * S);

    for (int px = int(tid); px < s_total; px += 128) {
        int sy = px / S;
        int sx = px % S;
        float3 c = thumb_bilinear_sample_f3(p.rgb, p.rgb_fmt, b, sx, sy, S,
                                            S, p.W_rgb, p.H_rgb);
        if (p.has_alpha != 0) {
            bool inside = thumb_nearest_sample_b(p.alpha_mask, b, sx, sy, S,
                                                 S, p.W_alpha, p.H_alpha);
            if (!inside) c = float3(0.5f, 0.5f, 0.5f);
        }
        uint r8 = uint(clamp(255.0f * c.x + 0.5f, 0.0f, 255.0f));
        uint g8 = uint(clamp(255.0f * c.y + 0.5f, 0.0f, 255.0f));
        uint b8 = uint(clamp(255.0f * c.z + 0.5f, 0.0f, 255.0f));
        p.thumbnails[dst + uint(sy * S + sx)] =
            r8 | (g8 << 8) | (b8 << 16) | (255u << 24);
    }
    GroupMemoryBarrierWithGroupSync();
    if (tid == 0)
        u8_store(p.done_mask, post_idx, 0xffu);
}

// Declared last: spec IDs follow declaration order (see dist_spec.slang).
[SpecializationConstant]
const int kDistortion = 0;

// Rasterize-and-sample meshing kernels (Vulkan backend), mirroring
// mesh/MeshingRaster.cu: project each query point into a rendered camera and
// sample its occupancy moments / color / observed resolution, plus the
// mesh-triangle occlusion test the visibility cull rides on.
//
// The per-camera render itself is the engine's own 3DGUT projection +
// tile intersection + rasterize_moments (rasterize_moments.slang), driven from
// the portable mesh/MeshingRasterHost.cpp -- nothing here renders.
//
// Same conventions as meshing.slang: float3 arrays are `float*` indexed 3*i+k,
// pointers first then 4-byte scalars, and the triangle LBVH's Karras build and
// node AABBs are the shared entries in meshing.slang.

#define CudaDeviceExport ForceInline
#include "int64_compat.slang"
#include "shaders/projection_utils.slang"
#undef CudaDeviceExport
#include "dist_spec.slang"

// Camera model of the images being sampled. Value parameter on the CUDA side;
// a specialization constant here, like every other camera-model axis
// (kDistortion follows at the end of the file).
[SpecializationConstant]
const int kCameraModel = 0;

static const int kCullStack = 64;

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

// ---------------------------------------------------------------------------
// Shared device helpers (mesh/MeshingRaster.cu's anonymous namespace)
// ---------------------------------------------------------------------------

// Bilinear-sample the [H,W] float3 image at continuous pixel (u,v), where pixel
// (j,i) center is at (j+0.5, i+0.5) (matching the rasterizer).
[ForceInline]
float3 bilinear3(float* img, int W, int H, float u, float v) {
    float sx = u - 0.5f, sy = v - 0.5f;
    int j0 = int(floor(sx)), i0 = int(floor(sy));
    float fx = sx - float(j0), fy = sy - float(i0);
    int j1 = j0 + 1, i1 = i0 + 1;
    j0 = min(max(j0, 0), W - 1); j1 = min(max(j1, 0), W - 1);
    i0 = min(max(i0, 0), H - 1); i1 = min(max(i1, 0), H - 1);
    float3 a = ld3(img, i0 * W + j0), b = ld3(img, i0 * W + j1);
    float3 c = ld3(img, i1 * W + j0), d = ld3(img, i1 * W + j1);
    float w00 = (1 - fx) * (1 - fy), w10 = fx * (1 - fy);
    float w01 = (1 - fx) * fy,       w11 = fx * fy;
    return w00 * a + w10 * b + w01 * c + w11 * d;
}

// Project p (world) into the camera. Returns false if behind the camera or out
// of frame; else fills pixel (u,v) and the along-ray depth z = |p_cam|.
[ForceInline]
bool project_point<D : ICameraDistortion>(
    float* viewmat, float* intrin, float* dist, int W, int H, float3 pw,
    out float u, out float v, out float z) {
    u = 0.0f; v = 0.0f; z = 0.0f;
    float3 p_cam;
    [ForceUnroll]
    for (int r = 0; r < 3; ++r)
        p_cam[r] = viewmat[4 * r + 0] * pw.x + viewmat[4 * r + 1] * pw.y +
                   viewmat[4 * r + 2] * pw.z + viewmat[4 * r + 3];

    float4 intr = float4(intrin[0], intrin[1], intrin[2], intrin[3]);
    float dc[D.kNumParams];
    [ForceUnroll]
    for (int t = 0; t < D.kNumParams; ++t) dc[t] = dist[t];

    // The projections handle the behind-camera / invalid-distortion cases and
    // return pixel-space uv (already scaled by fx,fy and offset by cx,cy).
    float2 uv;
    bool valid =
        (kCameraModel == 1) ? fisheye_proj<D, false>(p_cam, intr, dc, uv) :
        (kCameraModel == 2) ? equisolid_proj<D, false>(p_cam, intr, dc, uv) :
        (kCameraModel == 3) ? equirect_proj<false>(p_cam, intr, uv) :
                              persp_proj<D, false>(p_cam, intr, dc, uv);
    if (!valid) return false;
    u = uv.x; v = uv.y;
    if (u < 0.0f || u >= float(W) || v < 0.0f || v >= float(H)) return false;
    z = length(p_cam);
    return true;
}

// Reconstruct occupancy 1-T(z) from a sampled moment. Returns false (ABSTAIN)
// when the pixel's ray never accumulated alpha -- no confident surface (sky /
// grazing / too thin) -- so it does not carve.
[ForceInline]
bool occ_from_moment(float3 m, float z, out float occ) {
    occ = 0.0f;
    float m0 = m.x, c0 = m.y, c1 = m.z;
    if (!(m0 > 0.0f)) return false;
    float e = max(c0 + c1 * z, 0.0f);
    occ = min(m0, 1.0f - exp(-e * e));
    return true;
}

// Bilinear-sample occupancy at continuous pixel (u,v): evaluate the moment ->
// occupancy at each of the 4 corner pixels first, then bilinearly blend the
// resulting occupancies. Corners that abstain are dropped and the weights
// renormalized; abstains only if every corner abstains. This avoids blending
// moments across a surface edge before the nonlinear evaluation.
[ForceInline]
bool occ_bilinear(float* img, int W, int H, float u, float v, float z,
                  out float occ) {
    occ = 0.0f;
    float sx = u - 0.5f, sy = v - 0.5f;
    int j0 = int(floor(sx)), i0 = int(floor(sy));
    float fx = sx - float(j0), fy = sy - float(i0);
    int j1 = j0 + 1, i1 = i0 + 1;
    j0 = min(max(j0, 0), W - 1); j1 = min(max(j1, 0), W - 1);
    i0 = min(max(i0, 0), H - 1); i1 = min(max(i1, 0), H - 1);
    float3 m[4] = { ld3(img, i0 * W + j0), ld3(img, i0 * W + j1),
                    ld3(img, i1 * W + j0), ld3(img, i1 * W + j1) };
    float w[4] = { (1 - fx) * (1 - fy), fx * (1 - fy),
                   (1 - fx) * fy,       fx * fy };
    float acc = 0.0f, wsum = 0.0f;
    [ForceUnroll]
    for (int t = 0; t < 4; ++t) {
        float o;
        if (occ_from_moment(m[t], z, o)) { acc += w[t] * o; wsum += w[t]; }
    }
    if (wsum <= 0.0f) return false;
    occ = clamp((acc + 1e-6f) / (wsum + 1e-6f), 0.0f, 1.0f);
    return true;
}

// ===========================================================================
// Occupancy fusion (docs/notes/mesh-field.md)
// ===========================================================================

// A camera carves alone only if at least 1/kCarveResolution as sharp at the
// point as the sharpest camera that sees matter there.
static const float kCarveResolution = 2.0f;

[ForceInline]
float4 ld4(float* p, int i) {
    return float4(p[4 * i], p[4 * i + 1], p[4 * i + 2], p[4 * i + 3]);
}
[ForceInline]
void st4(float* p, int i, float4 v) {
    p[4 * i] = v.x;
    p[4 * i + 1] = v.y;
    p[4 * i + 2] = v.z;
    p[4 * i + 3] = v.w;
}

// One camera's view of a point at ray depth z, over the pixel corners that saw
// anything: its occupancy, the largest corner's, and the depth behind the
// camera's surface (where its fit reaches `iso`). False when no corner did.
[ForceInline]
bool view_of_point(float* img, int W, int H, float u, float v, float z, float iso,
                   out float occ, out float occ_max, out float behind) {
    occ = 0.0f; occ_max = 0.0f; behind = 0.0f;
    float level = sqrt(-log(1.0f - iso));
    float sx = u - 0.5f, sy = v - 0.5f;
    int j0 = int(floor(sx)), i0 = int(floor(sy));
    float fx = sx - float(j0), fy = sy - float(i0);
    int j1 = j0 + 1, i1 = i0 + 1;
    j0 = min(max(j0, 0), W - 1); j1 = min(max(j1, 0), W - 1);
    i0 = min(max(i0, 0), H - 1); i1 = min(max(i1, 0), H - 1);
    float3 m[4] = { ld3(img, i0 * W + j0), ld3(img, i0 * W + j1),
                    ld3(img, i1 * W + j0), ld3(img, i1 * W + j1) };
    float w[4] = { (1 - fx) * (1 - fy), fx * (1 - fy),
                   (1 - fx) * fy,       fx * fy };
    float acc = 0.0f, bacc = 0.0f, wsum = 0.0f;
    [ForceUnroll]
    for (int t = 0; t < 4; ++t) {
        float m0 = m[t].x, c0 = m[t].y, c1 = m[t].z;
        if (!(m0 > 0.0f) || !(w[t] > 0.0f)) continue;
        float e = max(c0 + c1 * z, 0.0f);
        float o = min(m0, 1.0f - exp(-e * e));
        // a ray that never reaches the level has its surface at infinity
        float b = -z;
        if (m0 >= iso && c1 > 0.0f) b = min(max((c0 + c1 * z - level) / c1, -z), z);
        acc += w[t] * o; bacc += w[t] * b; wsum += w[t];
        occ_max = max(occ_max, o);
    }
    if (wsum <= 0.0f) return false;
    occ = clamp(acc / wsum, 0.0f, 1.0f);
    behind = bacc / wsum;
    return true;
}

struct MeshSampleOccParams {  // 88 bytes: pushed directly
    float* xyz;
    float* viewmat;
    float* intrin;
    float* dist;
    float* moments;
    float* acc;
    uint32_t n;
    uint32_t W;
    uint32_t H;
    uint32_t tsdf;
    float iso;
    float band_px;
    float deep_weight;
    float clean;
    uint32_t wgs_per_row;
    uint32_t _pad0;
};

// acc[i] = (sharpest clear sight, weight, weighted occupancy, sharpest camera
// voting matter); under p.tsdf (seen, weight, weighted distance in bands, -).
[ForceInline]
void _ms_occ<D : ICameraDistortion>(MeshSampleOccParams p, int i) {
    float u, v, z;
    if (!project_point<D>(p.viewmat, p.intrin, p.dist, int(p.W), int(p.H),
                          ld3(p.xyz, i), u, v, z))
        return;
    // the nearest pixel: a blend would invent depths across an edge
    if (p.tsdf != 0) { u = floor(u) + 0.5f; v = floor(v) + 0.5f; }
    float vote, occ_max, behind;
    if (!view_of_point(p.moments, int(p.W), int(p.H), u, v, z, p.iso, vote,
                       occ_max, behind))
        return;  // abstain
    // pixels per world unit at the point: how sharply this camera sees it
    float nu = sqrt(max(p.intrin[0] * p.intrin[1], 1e-12f)) / max(z, 1e-9f);
    float band = p.band_px / nu;
    float4 a = ld4(p.acc, i);
    if (p.tsdf != 0) {
        float d = -behind / band;
        a.x = 1.0f;
        if (d >= -1.0f) { a.y += nu; a.z += nu * min(d, 1.0f); }
    } else {
        float deep = clamp(behind / band - 1.0f, 0.0f, 1.0f);
        float w = nu * (1.0f + (p.deep_weight - 1.0f) * deep);
        a.x = max(a.x, nu * max(1.0f - occ_max / p.clean, 0.0f));
        a.y += w;
        a.z += w * vote;
        if (vote >= p.iso) a.w = max(a.w, nu * (1.0f - deep));
    }
    st4(p.acc, i, a);
}

#define _ms_occ_call(D) _ms_occ<D>(p, int(gid))

[shader("compute")]
[numthreads(256, 1, 1)]
void mesh_sample_occ(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                     uniform MeshSampleOccParams p) {
    uint gid = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (gid >= p.n) return;
    SS_DISPATCH_DIST(_ms_occ_call)
}
#undef _ms_occ_call

struct MeshFinalizeOccParams {  // 40 bytes: pushed directly
    float* acc;
    float* occ;
    uint32_t n;
    uint32_t tsdf;
    float iso;
    float clean;
    uint32_t wgs_per_row;
    uint32_t _pad0;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void mesh_finalize_occ(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                       uniform MeshFinalizeOccParams p) {
    uint gid = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (gid >= p.n) return;
    int i = int(gid);
    float4 a = ld4(p.acc, i);
    if (p.tsdf != 0) {
        // in no camera's band: inside when some camera saw it deeper, else free
        float o = a.x > 0.0f ? 1.0f : 0.0f;
        if (a.y > 0.0f) {
            float d = a.z / a.y;
            o = d >= 0.0f ? p.iso * (1.0f - d) : p.iso - (1.0f - p.iso) * d;
        }
        p.occ[i] = o;
        return;
    }
    if (!(a.y > 0.0f)) { p.occ[i] = 0.0f; return; }   // unseen -> free space
    float carved = a.x > 0.0f ? min(p.iso * a.w / (kCarveResolution * a.x), 1.0f) : 1.0f;
    p.occ[i] = min(carved, a.z / a.y);
}

// ===========================================================================
// Color sampling (rendered DC color weighted by transmittance until the point)
// ===========================================================================
struct MeshSampleColorParams {  // 72 bytes: pushed directly
    float* xyz;
    float* viewmat;
    float* intrin;
    float* dist;
    float* moments;
    float* rgb_img;
    float* num;
    float* den;
    uint32_t n;
    uint32_t W;
    uint32_t H;
    uint32_t wgs_per_row;
};

[ForceInline]
void _ms_color<D : ICameraDistortion>(MeshSampleColorParams p, int i) {
    float u, v, z;
    if (!project_point<D>(p.viewmat, p.intrin, p.dist, int(p.W), int(p.H),
                          ld3(p.xyz, i), u, v, z))
        return;
    float occ;
    // Sample the occlusion slightly IN FRONT of the point (2% of depth):
    // extracted surfaces often sit under a skin of low-opacity splat fuzz
    // whose own alpha would otherwise read as "occluded" and reject every
    // view of a perfectly visible surface.
    if (!occ_bilinear(p.moments, int(p.W), int(p.H), u, v, 0.98f * z, occ))
        return;  // abstain (no confident surface from this view)
    float w = 1.0f - occ;   // transmittance until just before the point
    // Views that barely see the point are dropped and the rest sharpened
    // (w^4), so a half-occluded view cannot paint occluder color onto the
    // surface behind it.
    if (w <= 0.15f) return;
    float w4 = w * w; w4 *= w4;
    float3 c = bilinear3(p.rgb_img, int(p.W), int(p.H), u, v);
    p.num[3 * i + 0] += w4 * c.x;
    p.num[3 * i + 1] += w4 * c.y;
    p.num[3 * i + 2] += w4 * c.z;
    p.den[i] += w4;
}

#define _ms_color_call(D) _ms_color<D>(p, int(gid))

[shader("compute")]
[numthreads(256, 1, 1)]
void mesh_sample_color(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                       uniform MeshSampleColorParams p) {
    uint gid = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (gid >= p.n) return;
    SS_DISPATCH_DIST(_ms_color_call)
}
#undef _ms_color_call

struct MeshFinalizeColorParams {  // 32 bytes: pushed directly
    float* num;
    float* den;
    float* rgb;
    uint32_t n;
    uint32_t wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void mesh_finalize_color(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                         uniform MeshFinalizeColorParams p) {
    uint gid = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (gid >= p.n) return;
    int i = int(gid);
    float d = p.den[i];
    // No view sees the point with meaningful transmittance (fully occluded /
    // interior): mark for the static-color fallback.
    if (d > 1e-4f)
        st3(p.rgb, i, ld3(p.num, i) * (1.0f / d));
    else
        st3(p.rgb, i, float3(-1.0f));
}

// ===========================================================================
// View texel density
// ===========================================================================
struct MeshViewDensityParams {  // 56 bytes: pushed directly
    float* xyz;
    float* viewmat;
    float* intrin;
    float* dist;
    float* moments;
    float* dens;
    uint32_t n;
    uint32_t W;
    uint32_t H;
    uint32_t wgs_per_row;
};

[ForceInline]
void _ms_density<D : ICameraDistortion>(MeshViewDensityParams p, int i) {
    float u, v, z;
    if (!project_point<D>(p.viewmat, p.intrin, p.dist, int(p.W), int(p.H),
                          ld3(p.xyz, i), u, v, z))
        return;
    float occ;
    // same front-shifted sample as the color path: tolerate surface fuzz
    if (!occ_bilinear(p.moments, int(p.W), int(p.H), u, v, 0.98f * z, occ))
        return;
    if (1.0f - occ < 0.25f) return;  // (mostly) occluded from this view
    float f = sqrt(max(p.intrin[0] * p.intrin[1], 0.0f));
    float nu = f / max(z, 1e-9f);
    if (nu > p.dens[i]) p.dens[i] = nu;
}

#define _ms_density_call(D) _ms_density<D>(p, int(gid))

[shader("compute")]
[numthreads(256, 1, 1)]
void mesh_sample_view_density(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                              uniform MeshViewDensityParams p) {
    uint gid = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (gid >= p.n) return;
    SS_DISPATCH_DIST(_ms_density_call)
}
#undef _ms_density_call

// ===========================================================================
// Visibility cull
// ===========================================================================

// Per-triangle leaf AABB + centroid Morton code (and an iota id). The tree
// itself is built by meshing.slang's shared mesh_lbvh_* entries.
struct MeshTriPrepParams {  // 72 bytes: pushed directly
    int* faces;
    float* verts;
    float* leafMin;
    float* leafMax;
    u64key_t* morton;
    int* iota;
    float bmin_x, bmin_y, bmin_z;
    float ext_x, ext_y, ext_z;  // 1 / extent
    uint32_t nf;
    uint32_t wgs_per_row;
};

// 21-bit-per-axis Morton interleave -- identical to meshing.slang's (kept
// local so each entry's include closure stays minimal).
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
        uint sft = a == 0 ? 2u : (a == 1 ? 1u : 0u);
        uint b_lo = spread3_11(v & 0x7ffu);
        uint b_hi = spread3_11(v >> 11) << 1;
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

[shader("compute")]
[numthreads(256, 1, 1)]
void mesh_tri_prep(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                   uniform MeshTriPrepParams p) {
    uint gid = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (gid >= p.nf) return;
    int t = int(gid);
    float3 a = ld3(p.verts, p.faces[3 * t]);
    float3 b = ld3(p.verts, p.faces[3 * t + 1]);
    float3 c = ld3(p.verts, p.faces[3 * t + 2]);
    st3(p.leafMin, t, min(min(a, b), c));
    st3(p.leafMax, t, max(max(a, b), c));
    float3 ctr = (a + b + c) * (1.0f / 3.0f);
    float3 rc = float3((ctr.x - p.bmin_x) * p.ext_x,
                       (ctr.y - p.bmin_y) * p.ext_y,
                       (ctr.z - p.bmin_z) * p.ext_z);
    p.morton[t] = morton3D(rc);
    p.iota[t] = t;
}

// Slab test: does segment o + t*dir, t in [0,1], touch the AABB?
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

// Moeller-Trumbore: segment o + t*dir intersects triangle (a,b,c)? Reports t.
[ForceInline]
bool ray_tri(float3 o, float3 dir, float3 a, float3 b, float3 c, out float tout) {
    tout = 0.0f;
    float3 e1 = b - a, e2 = c - a;
    float3 pv = cross(dir, e2);
    float det = dot(e1, pv);
    if (abs(det) < 1e-20f) return false;   // ray parallel to triangle
    float inv = 1.0f / det;
    float3 tv = o - a;
    float u = dot(tv, pv) * inv;
    if (u < -1e-6f || u > 1.0f + 1e-6f) return false;
    float3 qv = cross(tv, e1);
    float v = dot(dir, qv) * inv;
    if (v < -1e-6f || u + v > 1.0f + 1e-6f) return false;
    tout = dot(e2, qv) * inv;
    return true;
}

// Does triangle `tid` (skipped if it contains vertex `vid`) block the open
// segment o->cam? The parametric margins exclude intersections hugging the
// endpoints, biasing the borderline toward "visible".
[ForceInline]
bool tri_hits(float3 o, float3 dir, int tid, int vid, float* verts, int* faces) {
    int ia = faces[3 * tid], ib = faces[3 * tid + 1], ic = faces[3 * tid + 2];
    if (ia == vid || ib == vid || ic == vid) return false;  // connected -> skip
    float t;
    return ray_tri(o, dir, ld3(verts, ia), ld3(verts, ib), ld3(verts, ic), t) &&
           t > 1e-3f && t < 1.0f - 1e-4f;
}

bool seg_blocked(float3 o, float3 cam, int vid, float* verts, int* faces,
                 int nf, float* leafMin, float* leafMax, int2* internal,
                 float* nodeAABB) {
    float3 dir = cam - o;
    if (nf < 2) {   // no internal nodes: scan the (<=1) tris
        for (int t = 0; t < nf; ++t)
            if (tri_hits(o, dir, t, vid, verts, faces)) return true;
        return false;
    }
    int stack[kCullStack];
    int sp = 0;
    if (!seg_aabb(o, dir, ld3(nodeAABB, 0), ld3(nodeAABB, 1))) return false;
    stack[sp++] = 0;
    while (sp > 0) {
        int2 ch = internal[stack[--sp]];
        [ForceUnroll]
        for (int cc = 0; cc < 2; ++cc) {
            int child = cc == 0 ? ch.x : ch.y;
            if (child < 0) {
                int tid = ~child;
                if (seg_aabb(o, dir, ld3(leafMin, tid), ld3(leafMax, tid)) &&
                    tri_hits(o, dir, tid, vid, verts, faces))
                    return true;
            } else if (seg_aabb(o, dir, ld3(nodeAABB, 2 * child),
                                ld3(nodeAABB, 2 * child + 1))) {
                if (sp < kCullStack) stack[sp++] = child;
            }
        }
    }
    return false;
}

struct MeshCullParams {
    float* verts;
    int* faces;
    float* viewmats;
    float* intrins;
    float* dist;
    int* Ws;
    int* Hs;
    float* leafMin;
    float* leafMax;
    int2* internal;
    float* nodeAABB;
    uint* visible;
    uint32_t first;
    uint32_t count;
    uint32_t nf;
    uint32_t C;
    uint32_t wgs_per_row;
    uint32_t _pad0;
};

[ForceInline]
void _ms_cull<D : ICameraDistortion>(MeshCullParams p, int i) {
    if (p.visible[i] != 0u) return;   // an earlier launch saw it
    float3 pt = ld3(p.verts, i);
    for (uint c = 0; c < p.C; ++c) {
        float* vm = p.viewmats + c * 16;
        float u, v, z;
        if (!project_point<D>(vm, p.intrins + c * 4, p.dist + c * kDistCoeffRow,
                              p.Ws[c], p.Hs[c], pt, u, v, z))
            continue;   // out of frame / behind camera
        // camera center in world: C = -R^T t (viewmat row-major world->cam)
        float3 cam = float3(
            -(vm[0] * vm[3] + vm[4] * vm[7] + vm[8] * vm[11]),
            -(vm[1] * vm[3] + vm[5] * vm[7] + vm[9] * vm[11]),
            -(vm[2] * vm[3] + vm[6] * vm[7] + vm[10] * vm[11]));
        if (!seg_blocked(pt, cam, i, p.verts, p.faces, int(p.nf),
                         p.leafMin, p.leafMax, p.internal, p.nodeAABB)) {
            p.visible[i] = 1u;
            return;
        }
    }
}

#define _ms_cull_call(D) _ms_cull<D>(p, int(p.first + gid))

[shader("compute")]
[numthreads(256, 1, 1)]
void mesh_cull(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
               uniform MeshCullParams* pp) {
    MeshCullParams p = *pp;
    uint gid = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (gid >= p.count) return;
    SS_DISPATCH_DIST(_ms_cull_call)
}
#undef _ms_cull_call

// Declared last: spec IDs follow declaration order (see dist_spec.slang).
[SpecializationConstant]
const int kDistortion = 0;

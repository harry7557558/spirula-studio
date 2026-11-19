/*
 * MeshingRaster.cu
 *
 * CUDA device side of the rasterize-and-sample meshing path: the kernels
 * behind mesh/MeshingDevice.h's third section -- projecting query points into
 * a rendered camera and sampling its occupancy moments / color, plus the
 * mesh-triangle occlusion test the visibility cull rides on.
 *
 * The driver that renders each camera and calls these lives in
 * mesh/MeshingRasterHost.cpp (portable); the Vulkan half of the kernels is
 * backend/vulkan/kernels/Meshing.cpp + shaders/meshing_raster.slang.
 */

#include "mesh/MeshingDevice.h"

#include <cuda_runtime.h>
#include <cstdint>

#include <core/Common.cuh>

#ifdef __CUDACC__
// {persp,fisheye,equisolid}_proj_nav -- match the (possibly distorted) camera
// model the occupancy/color images were rendered with. slang.cuh + FixedArray /
// Matrix come in via Common.cuh above.
namespace SlangProjectionUtils {
#include "generated/set_namespace.cuh"
#include "generated/projection_utils.cuh"
}
#include "core/CameraDistortion.cuh"
#endif

// Run BODY(tier) for the tier `value` names. The camera model stays runtime;
// the tier is one compile-time axis shared by every camera in the launch.
#define _SS_DISPATCH_DISTORTION(value, BODY)                                       \
    do { switch ((CameraDistortionType)(value)) {                                  \
        case CameraDistortionType::None:      BODY(CameraDistortionType::None);      break; \
        case CameraDistortionType::OpenCV:    BODY(CameraDistortionType::OpenCV);    break; \
        case CameraDistortionType::ThinPrism: BODY(CameraDistortionType::ThinPrism); break; \
        default: throw std::runtime_error("Unknown camera distortion tier");        \
    } } while (0)

namespace meshing {

// ---------------------------------------------------------------------------
// Shared device helpers
// ---------------------------------------------------------------------------
namespace {

// Bilinear-sample the [H,W] float3 image at continuous pixel (u,v), where pixel
// (j,i) center is at (j+0.5, i+0.5) (matching the rasterizer).
__device__ __forceinline__ float3 bilinear3(
    const float3* img, int W, int H, float u, float v
) {
    float sx = u - 0.5f, sy = v - 0.5f;
    int j0 = (int)floorf(sx), i0 = (int)floorf(sy);
    float fx = sx - j0, fy = sy - i0;
    int j1 = j0 + 1, i1 = i0 + 1;
    j0 = min(max(j0, 0), W - 1); j1 = min(max(j1, 0), W - 1);
    i0 = min(max(i0, 0), H - 1); i1 = min(max(i1, 0), H - 1);
    float3 a = img[i0 * W + j0], b = img[i0 * W + j1];
    float3 c = img[i1 * W + j0], d = img[i1 * W + j1];
    float w00 = (1 - fx) * (1 - fy), w10 = fx * (1 - fy);
    float w01 = (1 - fx) * fy,       w11 = fx * fy;
    return make_float3(
        w00*a.x + w10*b.x + w01*c.x + w11*d.x,
        w00*a.y + w10*b.y + w01*c.y + w11*d.y,
        w00*a.z + w10*b.z + w01*c.z + w11*d.z);
}

// Project p (world) into the camera. Returns false if behind the camera or out
// of frame; else fills pixel (u,v) and the along-ray depth z = |p_cam| (matches
// the rasterizer's evaluate_color depth metric). Uses the same projection +
// distortion model the occupancy/color images were rendered with, so the sample
// lands on the pixel the splats were actually rasterized to.
template<CameraDistortionType distortion>
__device__ __forceinline__ bool project_point(
    const float* viewmat, const float* intrin,
    const CameraDistortionCoeffsBuffer& dist, int cam,
    int model, int W, int H,
    float px, float py, float pz, float& u, float& v, float& z
) {
    float cx_ = viewmat[0]*px + viewmat[1]*py + viewmat[2]*pz + viewmat[3];
    float cy_ = viewmat[4]*px + viewmat[5]*py + viewmat[6]*pz + viewmat[7];
    float cz_ = viewmat[8]*px + viewmat[9]*py + viewmat[10]*pz + viewmat[11];

    float3 p_cam = make_float3(cx_, cy_, cz_);
    float4 intr = make_float4(intrin[0], intrin[1], intrin[2], intrin[3]);
    auto dist_coeffs = dist.load<distortion>(cam);

    // proj_nav handles the behind-camera / invalid-distortion cases and returns
    // pixel-space uv (already scaled by fx,fy and offset by cx,cy).
    float2 uv;
    bool valid = camera_proj_nav<distortion>(
        (CameraModelType)model, p_cam, intr, dist_coeffs, &uv);
    if (!valid) return false;
    u = uv.x; v = uv.y;
    if (u < 0.0f || u >= (float)W || v < 0.0f || v >= (float)H) return false;
    z = sqrtf(cx_*cx_ + cy_*cy_ + cz_*cz_);
    return true;
}

// Reconstruct occupancy 1-T(z) from a sampled moment
// Returns false (ABSTAIN) when the pixel's ray never reached tau_hi -- i.e. no
// confident surface (sky / grazing / too thin) -- so it does not carve.
__device__ __forceinline__ bool occ_from_moment(
    float3 m, float z, float& occ
) {
    float m0 = m.x, c0 = m.y, c1 = m.z;
    if (!(m0 > 0.0f))
        return false;
    occ = fminf(m0, 1.0f - __expf(-powf(fmaxf(c0 + c1 * z, 0.0f), 2.0f)));
    return true;
}

// Bilinear-sample occupancy at continuous pixel (u,v): evaluate the moment ->
// occupancy at each of the 4 corner pixels first, then bilinearly blend the
// resulting occupancies. Corners that abstain (occ_from_moment false) are
// dropped and the weights renormalized over the valid corners; abstains only if
// every corner abstains. This avoids blending moments across a surface edge
// (sky m0=0 mixed into an object) before the nonlinear evaluation.
__device__ __forceinline__ bool occ_bilinear(
    const float3* img, int W, int H, float u, float v, float z, float& occ
) {
    float sx = u - 0.5f, sy = v - 0.5f;
    int j0 = (int)floorf(sx), i0 = (int)floorf(sy);
    float fx = sx - j0, fy = sy - i0;
    int j1 = j0 + 1, i1 = i0 + 1;
    j0 = min(max(j0, 0), W - 1); j1 = min(max(j1, 0), W - 1);
    i0 = min(max(i0, 0), H - 1); i1 = min(max(i1, 0), H - 1);
    const float3 m[4] = { img[i0*W+j0], img[i0*W+j1], img[i1*W+j0], img[i1*W+j1] };
    const float w[4] = { (1-fx)*(1-fy), fx*(1-fy), (1-fx)*fy, fx*fy };
    float acc = 0.0f, wsum = 0.0f;
    for (int t = 0; t < 4; ++t) {
        float o;
        if (occ_from_moment(m[t], z, o)) { acc += w[t] * o; wsum += w[t]; }
    }
    if (wsum <= 0.0f) return false;
    occ = (acc + 1e-6f) / (wsum + 1e-6f);
    occ = fminf(fmaxf(occ, 0.0f), 1.0f);
    return true;
}


// ---------------------------------------------------------------------------
// Occupancy fusion (docs/notes/mesh-field.md)
// ---------------------------------------------------------------------------

// A camera carves alone only if at least 1/kCarveResolution as sharp at the
// point as the sharpest camera that sees matter there.
constexpr float kCarveResolution = 2.0f;

// One camera's view of a point at ray depth z, over the pixel corners that saw
// anything: its occupancy, the largest corner's, and the depth behind the
// camera's surface (where its fit reaches `iso`). False when no corner did.
__device__ __forceinline__ bool view_of_point(
    const float3* img, int W, int H, float u, float v, float z, float iso,
    float& occ, float& occ_max, float& behind
) {
    const float level = sqrtf(-__logf(1.0f - iso));
    float sx = u - 0.5f, sy = v - 0.5f;
    int j0 = (int)floorf(sx), i0 = (int)floorf(sy);
    float fx = sx - j0, fy = sy - i0;
    int j1 = j0 + 1, i1 = i0 + 1;
    j0 = min(max(j0, 0), W - 1); j1 = min(max(j1, 0), W - 1);
    i0 = min(max(i0, 0), H - 1); i1 = min(max(i1, 0), H - 1);
    const float3 m[4] = { img[i0*W+j0], img[i0*W+j1], img[i1*W+j0], img[i1*W+j1] };
    const float w[4] = { (1-fx)*(1-fy), fx*(1-fy), (1-fx)*fy, fx*fy };
    float acc = 0.0f, bacc = 0.0f, wsum = 0.0f;
    occ_max = 0.0f;
    for (int t = 0; t < 4; ++t) {
        const float m0 = m[t].x, c0 = m[t].y, c1 = m[t].z;
        if (!(m0 > 0.0f) || !(w[t] > 0.0f)) continue;
        const float e = fmaxf(c0 + c1 * z, 0.0f);
        const float o = fminf(m0, 1.0f - __expf(-e * e));
        // a ray that never reaches the level has its surface at infinity
        float b = -z;
        if (m0 >= iso && c1 > 0.0f) b = fminf(fmaxf((c0 + c1 * z - level) / c1, -z), z);
        acc += w[t] * o; bacc += w[t] * b; wsum += w[t];
        occ_max = fmaxf(occ_max, o);
    }
    if (wsum <= 0.0f) return false;
    occ = fminf(fmaxf(acc / wsum, 0.0f), 1.0f);
    behind = bacc / wsum;
    return true;
}

// acc[i] = (sharpest clear sight, weight, weighted occupancy, sharpest camera
// voting matter); under f.tsdf (seen, weight, weighted distance in bands, -).
template<CameraDistortionType distortion>
__global__ void sample_occ_kernel(
    const float* __restrict__ xyz, int n,
    const float* __restrict__ viewmat, const float* __restrict__ intrin,
    const CameraDistortionCoeffsBuffer dist, int model,
    const float3* __restrict__ moments, int W, int H,
    OccFusion f, float4* __restrict__ acc
) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    float u, v, z;
    if (!project_point<distortion>(viewmat, intrin, dist, 0, model, W, H,
                       xyz[3*i], xyz[3*i+1], xyz[3*i+2], u, v, z))
        return;
    // the nearest pixel: a blend would invent depths across an edge
    if (f.tsdf) { u = floorf(u) + 0.5f; v = floorf(v) + 0.5f; }
    float vote, occ_max, behind;
    if (!view_of_point(moments, W, H, u, v, z, f.iso, vote, occ_max, behind))
        return;  // abstain
    // pixels per world unit at the point: how sharply this camera sees it
    const float nu = sqrtf(fmaxf(intrin[0] * intrin[1], 1e-12f)) / fmaxf(z, 1e-9f);
    const float band = f.band_px / nu;
    float4 a = acc[i];
    if (f.tsdf) {
        const float d = -behind / band;
        a.x = 1.0f;
        if (d >= -1.0f) { a.y += nu; a.z += nu * fminf(d, 1.0f); }
    } else {
        const float deep = fminf(fmaxf(behind / band - 1.0f, 0.0f), 1.0f);
        const float w = nu * (1.0f + (f.deep_weight - 1.0f) * deep);
        a.x = fmaxf(a.x, nu * fmaxf(1.0f - occ_max / f.clean, 0.0f));
        a.y += w;
        a.z += w * vote;
        if (vote >= f.iso) a.w = fmaxf(a.w, nu * (1.0f - deep));
    }
    acc[i] = a;
}

__global__ void finalize_occ_kernel(
    int n, const float4* __restrict__ acc, OccFusion f, float* __restrict__ occ
) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    const float4 a = acc[i];
    if (f.tsdf) {
        // in no camera's band: inside when some camera saw it deeper, else free
        float o = a.x > 0.0f ? 1.0f : 0.0f;
        if (a.y > 0.0f) {
            const float d = a.z / a.y;
            o = d >= 0.0f ? f.iso * (1.0f - d) : f.iso - (1.0f - f.iso) * d;
        }
        occ[i] = o;
        return;
    }
    if (!(a.y > 0.0f)) { occ[i] = 0.0f; return; }   // unseen -> free space
    const float carved = a.x > 0.0f ? fminf(f.iso * a.w / (kCarveResolution * a.x), 1.0f) : 1.0f;
    occ[i] = fminf(carved, a.z / a.y);
}


// ---------------------------------------------------------------------------
// Color sampling (rendered DC color weighted by transmittance until the point)
// ---------------------------------------------------------------------------
template<CameraDistortionType distortion>
__global__ void sample_color_kernel(
    const float* __restrict__ xyz, int n,
    const float* __restrict__ viewmat, const float* __restrict__ intrin,
    const CameraDistortionCoeffsBuffer dist, int model,
    const float3* __restrict__ moments, const float3* __restrict__ rgb,
    int W, int H,
    float3* __restrict__ num, float* __restrict__ den
) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    float u, v, z;
    if (!project_point<distortion>(viewmat, intrin, dist, 0, model, W, H,
                       xyz[3*i], xyz[3*i+1], xyz[3*i+2], u, v, z))
        return;
    float occ;
    // Sample the occlusion slightly IN FRONT of the point (2% of depth):
    // extracted surfaces often sit under a skin of low-opacity splat fuzz
    // whose own alpha would otherwise read as "occluded" and reject every
    // view of a perfectly visible surface (the pixel color DOES show this
    // surface, through the fuzz). True foreground occluders (a fender in
    // front of a wheel) sit much farther than 2% in front and still reject.
    if (!occ_bilinear(moments, W, H, u, v, 0.98f * z, occ))
        return;  // abstain (no confident surface from this view)
    float w = 1.0f - occ;            // transmittance until just before the point
    // The rendered pixel color is the FULL composite along the ray: a fraction
    // (1-w) of it comes from splats in FRONT of the point. A linear weight let
    // half-occluded views paint occluder colors onto the surface behind them
    // (wheels picked up fender/body colors), so views that barely see the
    // point are dropped and the rest sharpened (w^4) to favor clear views.
    if (w <= 0.15f) return;
    float w4 = w * w; w4 *= w4;
    float3 c = bilinear3(rgb, W, H, u, v);
    num[i].x += w4 * c.x; num[i].y += w4 * c.y; num[i].z += w4 * c.z;
    den[i] += w4;
}

__global__ void finalize_color_kernel(
    int n, const float3* __restrict__ num, const float* __restrict__ den,
    float* __restrict__ rgb
) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    float d = den[i];
    // No view sees the point with meaningful transmittance (fully occluded /
    // interior): mark for the static-color fallback instead of averaging a
    // handful of contaminated samples.
    if (d > 1e-4f) {
        rgb[3*i+0] = num[i].x / d;
        rgb[3*i+1] = num[i].y / d;
        rgb[3*i+2] = num[i].z / d;
    } else {
        rgb[3*i+0] = -1.0f; rgb[3*i+1] = -1.0f; rgb[3*i+2] = -1.0f;
    }
}


// ---------------------------------------------------------------------------
// View texel density
// ---------------------------------------------------------------------------
template<CameraDistortionType distortion>
__global__ void sample_view_density_kernel(
    const float* __restrict__ xyz, int n,
    const float* __restrict__ viewmat, const float* __restrict__ intrin,
    const CameraDistortionCoeffsBuffer dist, int model,
    const float3* __restrict__ moments, int W, int H,
    float* __restrict__ dens
) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    float u, v, z;
    if (!project_point<distortion>(viewmat, intrin, dist, 0, model, W, H,
                       xyz[3*i], xyz[3*i+1], xyz[3*i+2], u, v, z))
        return;
    float occ;
    // same front-shifted sample as the color path: tolerate surface fuzz
    if (!occ_bilinear(moments, W, H, u, v, 0.98f * z, occ))
        return;
    if (1.0f - occ < 0.25f) return;  // (mostly) occluded from this view
    float f = sqrtf(fmaxf(intrin[0] * intrin[1], 0.0f));
    float nu = f / fmaxf(z, 1e-9f);
    if (nu > dens[i]) dens[i] = nu;
}


// ---------------------------------------------------------------------------
// Visibility cull: drop mesh vertices seen by no camera.
// ---------------------------------------------------------------------------
__device__ __forceinline__ float3 fmin3(float3 a, float3 b) {
    return make_float3(fminf(a.x,b.x), fminf(a.y,b.y), fminf(a.z,b.z));
}
__device__ __forceinline__ float3 fmax3(float3 a, float3 b) {
    return make_float3(fmaxf(a.x,b.x), fmaxf(a.y,b.y), fmaxf(a.z,b.z));
}

// 21-bit-per-axis Morton interleave (same as Meshing.cu's LBVH build).
__device__ __forceinline__ uint64_t expand_bits_21(uint64_t x) {
    x &= 0x1fffffULL;
    x = (x | (x << 32)) & 0x1f00000000ffffULL;
    x = (x | (x << 16)) & 0x1f0000ff0000ffULL;
    x = (x | (x << 8))  & 0x100f00f00f00f00fULL;
    x = (x | (x << 4))  & 0x10c30c30c30c30c3ULL;
    x = (x | (x << 2))  & 0x1249249249249249ULL;
    return x;
}
__device__ __forceinline__ uint64_t morton3D(float3 c /* in [0,1] */) {
    const float s = (float)((1u << 21) - 1);
    uint64_t xi = (uint64_t)fminf(fmaxf(c.x * s, 0.0f), s);
    uint64_t yi = (uint64_t)fminf(fmaxf(c.y * s, 0.0f), s);
    uint64_t zi = (uint64_t)fminf(fmaxf(c.z * s, 0.0f), s);
    return (expand_bits_21(xi) << 2) | (expand_bits_21(yi) << 1) | expand_bits_21(zi);
}

// Slab test: does segment o + t*dir, t in [0,1], touch the AABB?
__device__ __forceinline__ bool seg_aabb(float3 o, float3 dir, float3 bmin, float3 bmax) {
    float t0 = 0.0f, t1 = 1.0f;
    #pragma unroll
    for (int a = 0; a < 3; ++a) {
        float oa = a==0?o.x:(a==1?o.y:o.z);
        float da = a==0?dir.x:(a==1?dir.y:dir.z);
        float lo = a==0?bmin.x:(a==1?bmin.y:bmin.z);
        float hi = a==0?bmax.x:(a==1?bmax.y:bmax.z);
        if (fabsf(da) < 1e-20f) { if (oa < lo || oa > hi) return false; }
        else {
            float inv = 1.0f / da;
            float ta = (lo - oa) * inv, tb = (hi - oa) * inv;
            if (ta > tb) { float tmp = ta; ta = tb; tb = tmp; }
            t0 = fmaxf(t0, ta); t1 = fminf(t1, tb);
            if (t0 > t1) return false;
        }
    }
    return true;
}

// Moeller-Trumbore: segment o + t*dir intersects triangle (a,b,c)? Reports t.
__device__ __forceinline__ bool ray_tri(
    float3 o, float3 dir, float3 a, float3 b, float3 c, float& tout
) {
    float3 e1 = b - a, e2 = c - a;
    float3 pv = cross(dir, e2);
    float det = dot(e1, pv);
    if (fabsf(det) < 1e-20f) return false;          // ray parallel to triangle
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
// segment o->cam? Parametric margins exclude intersections hugging the
// endpoints: the near margin suppresses self-occlusion acne from triangles
// adjacent to (but not containing) the vertex, biasing the borderline toward
// "visible" so the surface is not over-culled.
__device__ __forceinline__ bool tri_hits(
    float3 o, float3 dir, int tid, int vid,
    const float* __restrict__ verts, const int* __restrict__ faces
) {
    int ia = faces[3*tid], ib = faces[3*tid+1], ic = faces[3*tid+2];
    if (ia == vid || ib == vid || ic == vid) return false;   // connected -> skip
    float3 a = make_float3(verts[3*ia], verts[3*ia+1], verts[3*ia+2]);
    float3 b = make_float3(verts[3*ib], verts[3*ib+1], verts[3*ib+2]);
    float3 c = make_float3(verts[3*ic], verts[3*ic+1], verts[3*ic+2]);
    float t;
    return ray_tri(o, dir, a, b, c, t) && t > 1e-3f && t < 1.0f - 1e-4f;
}

inline constexpr int kCullStack = 64;

__device__ bool seg_blocked(
    float3 o, float3 cam, int vid,
    const float* __restrict__ verts, const int* __restrict__ faces, int nf,
    const float3* __restrict__ leafMin, const float3* __restrict__ leafMax,
    const int2* __restrict__ internal, const float3* __restrict__ nodeAABB
) {
    float3 dir = cam - o;
    if (nf < 2) {                       // no internal nodes: scan the (<=1) tris
        for (int t = 0; t < nf; ++t)
            if (tri_hits(o, dir, t, vid, verts, faces)) return true;
        return false;
    }
    int stack[kCullStack]; int sp = 0;
    if (!seg_aabb(o, dir, nodeAABB[0], nodeAABB[1])) return false;
    stack[sp++] = 0;
    while (sp > 0) {
        int ni = stack[--sp];
        int2 ch = internal[ni];
        #pragma unroll
        for (int cc = 0; cc < 2; ++cc) {
            int child = cc == 0 ? ch.x : ch.y;
            if (child < 0) {
                int tid = ~child;
                if (seg_aabb(o, dir, leafMin[tid], leafMax[tid]) &&
                    tri_hits(o, dir, tid, vid, verts, faces))
                    return true;
            } else if (seg_aabb(o, dir, nodeAABB[2*child], nodeAABB[2*child+1])) {
                if (sp < kCullStack) stack[sp++] = child;
            }
        }
    }
    return false;
}

// Per-triangle leaf AABB + centroid Morton code (and an iota id).
__global__ void tri_prep_kernel(
    int nf, const int* __restrict__ faces, const float* __restrict__ verts,
    float3 bmin, float3 inv_ext,
    float3* leafMin, float3* leafMax, uint64_t* morton, int* iota
) {
    int t = blockIdx.x * blockDim.x + threadIdx.x;
    if (t >= nf) return;
    int ia = faces[3*t], ib = faces[3*t+1], ic = faces[3*t+2];
    float3 a = make_float3(verts[3*ia], verts[3*ia+1], verts[3*ia+2]);
    float3 b = make_float3(verts[3*ib], verts[3*ib+1], verts[3*ib+2]);
    float3 c = make_float3(verts[3*ic], verts[3*ic+1], verts[3*ic+2]);
    leafMin[t] = fmin3(fmin3(a, b), c);
    leafMax[t] = fmax3(fmax3(a, b), c);
    float3 ctr = (a + b + c) * (1.0f / 3.0f);
    float3 rc = make_float3((ctr.x - bmin.x) * inv_ext.x,
                            (ctr.y - bmin.y) * inv_ext.y,
                            (ctr.z - bmin.z) * inv_ext.z);
    morton[t] = morton3D(rc);
    iota[t] = t;
}

template<CameraDistortionType distortion>
__global__ void cull_kernel(
    const float* __restrict__ verts, int first, int count,
    const int* __restrict__ faces, int nf,
    const float* __restrict__ viewmats, const float* __restrict__ intrins,
    const CameraDistortionCoeffsBuffer dist,
    const int* __restrict__ Ws, const int* __restrict__ Hs,
    int model, int C,
    const float3* __restrict__ leafMin, const float3* __restrict__ leafMax,
    const int2* __restrict__ internal, const float3* __restrict__ nodeAABB,
    uint32_t* __restrict__ visible
) {
    int t = blockIdx.x * blockDim.x + threadIdx.x;
    if (t >= count) return;
    const int i = first + t;
    if (visible[i]) return;                        // an earlier launch saw it
    float3 p = make_float3(verts[3*i], verts[3*i+1], verts[3*i+2]);
    for (int c = 0; c < C; ++c) {
        const float* vm = viewmats + (size_t)c * 16;
        float u, v, z;
        if (!project_point<distortion>(
                vm, intrins + (size_t)c*4, dist, c,
                model, Ws[c], Hs[c], p.x, p.y, p.z, u, v, z))
            continue;                              // out of frame / behind camera
        // camera center in world: C = -R^T t (viewmat row-major world->cam 4x4)
        float3 cam = make_float3(
            -(vm[0]*vm[3] + vm[4]*vm[7] + vm[8]*vm[11]),
            -(vm[1]*vm[3] + vm[5]*vm[7] + vm[9]*vm[11]),
            -(vm[2]*vm[3] + vm[6]*vm[7] + vm[10]*vm[11]));
        if (!seg_blocked(p, cam, i, verts, faces, nf,
                         leafMin, leafMax, internal, nodeAABB)) {
            visible[i] = 1u;
            return;
        }
    }
}

} // namespace


// ---------------------------------------------------------------------------
// Launchers (mesh/MeshingDevice.h)
// ---------------------------------------------------------------------------
void launch_sample_occ(
    const float* xyz, int n,
    const float* viewmat, const float* intrin, const float* dist,
    int camera_model, int distortion,
    const float3* moments, int W, int H, const OccFusion& f, float4* acc
) {
    if (n <= 0) return;
    const CameraDistortionCoeffsBuffer dcb(const_cast<float*>(dist));
    #define LAUNCH(D) \
        sample_occ_kernel<D><<<_LAUNCH_ARGS_1D(n, 256)>>>( \
            xyz, n, viewmat, intrin, dcb, camera_model, moments, W, H, f, acc)
    _SS_DISPATCH_DISTORTION(distortion, LAUNCH);
    #undef LAUNCH
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

void launch_finalize_occ(int n, const float4* acc, const OccFusion& f, float* occ) {
    if (n <= 0) return;
    finalize_occ_kernel<<<_LAUNCH_ARGS_1D(n, 256)>>>(n, acc, f, occ);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

void launch_sample_color(
    const float* xyz, int n,
    const float* viewmat, const float* intrin, const float* dist,
    int camera_model, int distortion,
    const float3* moments, const float3* rgb_img,
    int W, int H, float3* num, float* den
) {
    if (n <= 0) return;
    const CameraDistortionCoeffsBuffer dcb(const_cast<float*>(dist));
    #define LAUNCH(D) \
        sample_color_kernel<D><<<_LAUNCH_ARGS_1D(n, 256)>>>( \
            xyz, n, viewmat, intrin, dcb, camera_model, moments, rgb_img, W, H, \
            num, den)
    _SS_DISPATCH_DISTORTION(distortion, LAUNCH);
    #undef LAUNCH
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

void launch_finalize_color(int n, const float3* num, const float* den,
                           float* rgb) {
    if (n <= 0) return;
    finalize_color_kernel<<<_LAUNCH_ARGS_1D(n, 256)>>>(n, num, den, rgb);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

void launch_sample_view_density(
    const float* xyz, int n,
    const float* viewmat, const float* intrin, const float* dist,
    int camera_model, int distortion,
    const float3* moments, int W, int H, float* dens
) {
    if (n <= 0) return;
    const CameraDistortionCoeffsBuffer dcb(const_cast<float*>(dist));
    #define LAUNCH(D) \
        sample_view_density_kernel<D><<<_LAUNCH_ARGS_1D(n, 256)>>>( \
            xyz, n, viewmat, intrin, dcb, camera_model, moments, W, H, dens)
    _SS_DISPATCH_DISTORTION(distortion, LAUNCH);
    #undef LAUNCH
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

void launch_tri_prep(
    int nf, const int* faces, const float* verts,
    float3 bmin, float3 inv_ext,
    float3* leafMin, float3* leafMax, uint64_t* morton, int* iota
) {
    if (nf <= 0) return;
    tri_prep_kernel<<<_LAUNCH_ARGS_1D(nf, 256)>>>(
        nf, faces, verts, bmin, inv_ext, leafMin, leafMax, morton, iota);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

void launch_cull(
    const float* verts, int first, int count, const int* faces, int nf,
    const float* viewmats, const float* intrins, const float* dist,
    const int* Ws, const int* Hs, int camera_model, int distortion, int C,
    const float3* leafMin, const float3* leafMax,
    const int2* internal, const float3* nodeAABB,
    uint32_t* visible
) {
    if (count <= 0) return;
    const CameraDistortionCoeffsBuffer dcb(const_cast<float*>(dist));
    #define LAUNCH(D) \
        cull_kernel<D><<<_LAUNCH_ARGS_1D(count, 256)>>>( \
            verts, first, count, faces, nf, viewmats, intrins, dcb, Ws, Hs, camera_model, C, \
            leafMin, leafMax, internal, nodeAABB, visible)
    _SS_DISPATCH_DISTORTION(distortion, LAUNCH);
    #undef LAUNCH
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

} // namespace meshing

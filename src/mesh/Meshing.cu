/*
 * Meshing.cu
 *
 * CUDA device side of the 3DGS surface meshing pipeline: the kernels behind
 * mesh/MeshingDevice.h's first two sections. The host orchestration that
 * drives them lives in mesh/OccupancyEvaluator.cpp (portable, both backends);
 * the Vulkan half is backend/vulkan/kernels/Meshing.cpp + shaders/meshing.slang.
 *
 * What is here (see Meshing.h for the pipeline):
 *   - Gaussian activation (quat/scale/opacity) and principal-axis extraction.
 *   - 7-points-per-Gaussian point cloud sampling.
 *   - An LBVH over the Gaussians' support (Karras radix tree + coordinate
 *     remap, mirroring SplatTileIntersector.cu), so unbounded scenes with
 *     distant splats stay fast -- unlike a uniform grid, whose cell size is
 *     dictated by the farthest splat.
 *   - The occupancy field: a static (density aggregation) variant and a dataset
 *     (per-camera closed-form max density along the point->camera segment,
 *     minimized over cameras) variant. Each Gaussian is a single BVH leaf, so
 *     it is visited exactly once per query -- no multi-cell double counting.
 *   - Per-edge crossing-point bisection.
 */

#include "mesh/MeshingDevice.h"

#include "core/Common.cuh"

#include <cmath>
#include <cstdint>

namespace meshing {

// ---------------------------------------------------------------------------
// Small helpers (Common.cuh already provides +,-,*,/ on floatN).
// ---------------------------------------------------------------------------
__host__ __device__ __forceinline__ float dot3(float3 a, float3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
__device__ __forceinline__ float3 fmin3(float3 a, float3 b) {
    return make_float3(fminf(a.x,b.x), fminf(a.y,b.y), fminf(a.z,b.z));
}
__device__ __forceinline__ float3 fmax3(float3 a, float3 b) {
    return make_float3(fmaxf(a.x,b.x), fmaxf(a.y,b.y), fmaxf(a.z,b.z));
}

// Coordinate remap: identity near the origin, ~ x^(1/k) for large |x|. Applied
// only to Morton ordering so a few distant splats don't starve the near-origin
// region of spatial resolution. (Copied from SplatTileIntersector.cu.) The host
// mirror is MeshingDevice.h's remap_coord, the Slang one meshing.slang's --
// keep the three identical.
__device__ __forceinline__ float remap_coord_dev(float x, float rel_scale) {
    constexpr float k = 2.5f;
    return k * sinhf((1.0f / k) * asinhf(x / rel_scale)) * rel_scale;
}

// 21-bit-per-axis Morton interleave.
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

__device__ __forceinline__ float atomicMinF(float* addr, float val) {
    int* a = (int*)addr; int old = *a, assumed;
    do { assumed = old;
         if (__int_as_float(assumed) <= val) break;
         old = atomicCAS(a, assumed, __float_as_int(val));
    } while (assumed != old);
    return __int_as_float(old);
}
__device__ __forceinline__ float atomicMaxF(float* addr, float val) {
    int* a = (int*)addr; int old = *a, assumed;
    do { assumed = old;
         if (__int_as_float(assumed) >= val) break;
         old = atomicCAS(a, assumed, __float_as_int(val));
    } while (assumed != old);
    return __int_as_float(old);
}

// ---- Gaussian density math -------------------------------------------------
__device__ __forceinline__ float quad_form(const GpuScene& s, int g, float3 d) {
    float u0 = dot3(d, s.ax0[g]);
    float u1 = dot3(d, s.ax1[g]);
    float u2 = dot3(d, s.ax2[g]);
    float3 is = s.invs2[g];
    return u0 * u0 * is.x + u1 * u1 * is.y + u2 * u2 * is.z;
}
__device__ __forceinline__ float density_at(const GpuScene& s, int g, float3 p) {
    float q = quad_form(s, g, p - s.mean[g]);
    if (q >= s.k2[g]) return 0.0f;
    return s.opac[g] * __expf(-0.5f * q);
}
// Closed-form max density of Gaussian g along the segment o + t*dir, t in [0,1].
__device__ __forceinline__ float seg_max_density(const GpuScene& s, int g, float3 o, float3 dir) {
    float3 e = o - s.mean[g];
    float e0 = dot3(e, s.ax0[g]), e1 = dot3(e, s.ax1[g]), e2 = dot3(e, s.ax2[g]);
    float d0 = dot3(dir, s.ax0[g]), d1 = dot3(dir, s.ax1[g]), d2 = dot3(dir, s.ax2[g]);
    float3 is = s.invs2[g];
    float A = d0*d0*is.x + d1*d1*is.y + d2*d2*is.z;
    float B = e0*d0*is.x + e1*d1*is.y + e2*d2*is.z;
    float C = e0*e0*is.x + e1*e1*is.y + e2*e2*is.z;
    float t = (A > 1e-20f) ? (-B / A) : 0.0f;
    t = fminf(fmaxf(t, 0.0f), 1.0f);
    float q = A*t*t + 2.0f*B*t + C;
    if (q >= s.k2[g]) return 0.0f;
    return s.opac[g] * __expf(-0.5f * q);
}

// ---- AABB tests ------------------------------------------------------------
__device__ __forceinline__ bool point_in_aabb(float3 p, float3 bmin, float3 bmax) {
    return p.x >= bmin.x && p.x <= bmax.x &&
           p.y >= bmin.y && p.y <= bmax.y &&
           p.z >= bmin.z && p.z <= bmax.z;
}
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

inline constexpr int kStackSize = 64;

// ---- static occupancy: density aggregation of Gaussians containing p -------
__device__ float occ_static(const GpuScene& s, float3 p) {
    float S = 0.0f;  // sum log(1 - density)
    if (s.num_kept == 1) {
        float d = density_at(s, s.kept[0], p);
        return d > ALPHA_THRESHOLD ? d : 0.0f;
    }
    int stack[kStackSize]; int sp = 0;
    if (!point_in_aabb(p, s.nodeAABB[0], s.nodeAABB[1])) return 0.0f;
    stack[sp++] = 0;
    while (sp > 0) {
        int ni = stack[--sp];
        int2 ch = s.internal[ni];
        #pragma unroll
        for (int c = 0; c < 2; ++c) {
            int child = c == 0 ? ch.x : ch.y;
            if (child < 0) {
                int kp = ~child;
                if (point_in_aabb(p, s.leafMin[kp], s.leafMax[kp])) {
                    float d = density_at(s, s.kept[kp], p);
                    if (d > ALPHA_THRESHOLD) S += __logf(1.0f - fminf(d, 0.999f));
                }
            } else if (point_in_aabb(p, s.nodeAABB[2*child], s.nodeAABB[2*child+1])) {
                if (sp < kStackSize) stack[sp++] = child;
            }
        }
    }
    return 1.0f - __expf(S);
}

// ---- one camera: sum log(1-d) over Gaussians the segment p->cam crosses -----
__device__ float seg_log_transmittance(const GpuScene& s, float3 p, float3 cam) {
    float3 dir = cam - p;
    float S = 0.0f;
    if (s.num_kept == 1) {
        float d = seg_max_density(s, s.kept[0], p, dir);
        return d > ALPHA_THRESHOLD ? __logf(1.0f - fminf(d, 0.999f)) : 0.0f;
    }
    int stack[kStackSize]; int sp = 0;
    if (!seg_aabb(p, dir, s.nodeAABB[0], s.nodeAABB[1])) return 0.0f;
    stack[sp++] = 0;
    while (sp > 0) {
        int ni = stack[--sp];
        int2 ch = s.internal[ni];
        #pragma unroll
        for (int c = 0; c < 2; ++c) {
            int child = c == 0 ? ch.x : ch.y;
            if (child < 0) {
                int kp = ~child;
                if (seg_aabb(p, dir, s.leafMin[kp], s.leafMax[kp])) {
                    float d = seg_max_density(s, s.kept[kp], p, dir);
                    if (d > ALPHA_THRESHOLD) S += __logf(1.0f - fminf(d, 0.999f));
                }
            } else if (seg_aabb(p, dir, s.nodeAABB[2*child], s.nodeAABB[2*child+1])) {
                if (sp < kStackSize) stack[sp++] = child;
            }
        }
    }
    return S;
}

__device__ float occ_dynamic(const GpuScene& s, float3 p) {
    float occ_min = 1.0f;
    for (int ci = 0; ci < s.num_cameras; ++ci) {
        float S = seg_log_transmittance(s, p, s.campos[ci]);
        occ_min = fminf(occ_min, 1.0f - __expf(S));
    }
    return occ_min;
}

__device__ __forceinline__ float occ_eval(const GpuScene& s, float3 p, bool dynamic) {
    return dynamic ? occ_dynamic(s, p) : occ_static(s, p);
}

// ---------------------------------------------------------------------------
// Vertex coloring from the splats' DC color (gcol).
// ---------------------------------------------------------------------------
// Same as seg_max_density but also reports the (clamped) closest-approach
// parameter, needed to order splats front-to-back along a camera ray.
__device__ __forceinline__ float seg_color_sample(
    const GpuScene& s, int g, float3 o, float3 dir, float& tstar
) {
    float3 e = o - s.mean[g];
    float e0 = dot3(e, s.ax0[g]), e1 = dot3(e, s.ax1[g]), e2 = dot3(e, s.ax2[g]);
    float d0 = dot3(dir, s.ax0[g]), d1 = dot3(dir, s.ax1[g]), d2 = dot3(dir, s.ax2[g]);
    float3 is = s.invs2[g];
    float A = d0*d0*is.x + d1*d1*is.y + d2*d2*is.z;
    float B = e0*d0*is.x + e1*d1*is.y + e2*d2*is.z;
    float C = e0*e0*is.x + e1*e1*is.y + e2*e2*is.z;
    float t = (A > 1e-20f) ? (-B / A) : 0.0f;
    t = fminf(fmaxf(t, 0.0f), 1.0f);
    tstar = t;
    float q = A*t*t + 2.0f*B*t + C;
    if (q >= s.k2[g]) return 0.0f;
    return s.opac[g] * __expf(-0.5f * q);
}

// Density-weighted average color of the splats that contain p. Returns false
// if p is inside no splat's support (e.g. a vertex nudged into a gap by merge).
__device__ bool color_static(const GpuScene& s, float3 p, float3& out) {
    float wsum = 0.0f; float3 csum = make_float3(0.f, 0.f, 0.f);
    if (s.num_kept == 1) {
        float d = density_at(s, s.kept[0], p);
        if (d > 0.0f) { out = s.gcol[s.kept[0]]; return true; }
        return false;
    }
    int stack[kStackSize]; int sp = 0;
    if (!point_in_aabb(p, s.nodeAABB[0], s.nodeAABB[1])) return false;
    stack[sp++] = 0;
    while (sp > 0) {
        int2 ch = s.internal[stack[--sp]];
        #pragma unroll
        for (int c = 0; c < 2; ++c) {
            int child = c == 0 ? ch.x : ch.y;
            if (child < 0) {
                int kp = ~child;
                if (point_in_aabb(p, s.leafMin[kp], s.leafMax[kp])) {
                    int g = s.kept[kp];
                    float d = density_at(s, g, p);
                    if (d > 0.0f) { wsum += d; csum += s.gcol[g] * d; }
                }
            } else if (point_in_aabb(p, s.nodeAABB[2*child], s.nodeAABB[2*child+1])) {
                if (sp < kStackSize) stack[sp++] = child;
            }
        }
    }
    if (wsum > 0.0f) { out = csum * (1.0f / wsum); return true; }
    return false;
}

// Color of the splat whose center is nearest p (last-resort fallback).
__device__ float3 color_nearest(const GpuScene& s, float3 p) {
    if (s.num_kept <= 1) return s.gcol[s.kept[0]];
    float best = 1e30f; int bg = s.kept[0];
    int stack[kStackSize]; int sp = 0; stack[sp++] = 0;
    while (sp > 0) {
        int2 ch = s.internal[stack[--sp]];
        #pragma unroll
        for (int c = 0; c < 2; ++c) {
            int child = c == 0 ? ch.x : ch.y;
            if (child < 0) {
                int g = s.kept[~child];
                float3 d = p - s.mean[g];
                float d2 = dot3(d, d);
                if (d2 < best) { best = d2; bg = g; }
            } else {
                float3 lo = s.nodeAABB[2*child], hi = s.nodeAABB[2*child+1];
                float dx = fmaxf(fmaxf(lo.x - p.x, p.x - hi.x), 0.0f);
                float dy = fmaxf(fmaxf(lo.y - p.y, p.y - hi.y), 0.0f);
                float dz = fmaxf(fmaxf(lo.z - p.z, p.z - hi.z), 0.0f);
                if (dx*dx + dy*dy + dz*dz < best && sp < kStackSize) stack[sp++] = child;
            }
        }
    }
    return s.gcol[bg];
}

inline constexpr int kColorCap = 64;  // max splats composited per camera ray

// Camera-aware color: for each camera, alpha-composite the splats front-to-back
// along the segment camera->p (up to p), then average the per-camera colors
// weighted by accumulated opacity. Falls back to the static color, then to the
// nearest splat, when no camera ray deposits weight.
__device__ float3 color_camera(const GpuScene& s, float3 p) {
    float3 num = make_float3(0.f, 0.f, 0.f);
    float den = 0.0f;

    for (int ci = 0; ci < s.num_cameras; ++ci) {
        float3 cam = s.campos[ci];
        float3 dir = p - cam;             // segment cam->p, t in [0,1]

        // gather hits (keep the CAP closest-to-camera ones)
        float ts[kColorCap], dd[kColorCap]; float3 cc[kColorCap];
        int cnt = 0; float tmax = -1.0f; int tmax_i = 0;
        int stack[kStackSize]; int sp = 0;
        if (s.num_kept >= 2 && seg_aabb(cam, dir, s.nodeAABB[0], s.nodeAABB[1]))
            stack[sp++] = 0;
        else if (s.num_kept == 1) { /* handled below via single leaf */ }
        while (sp > 0) {
            int2 ch = s.internal[stack[--sp]];
            #pragma unroll
            for (int c = 0; c < 2; ++c) {
                int child = c == 0 ? ch.x : ch.y;
                if (child < 0) {
                    int kp = ~child;
                    if (!seg_aabb(cam, dir, s.leafMin[kp], s.leafMax[kp])) continue;
                    int g = s.kept[kp]; float tstar;
                    float d = seg_color_sample(s, g, cam, dir, tstar);
                    if (d <= ALPHA_THRESHOLD) continue;
                    if (cnt < kColorCap) {
                        ts[cnt] = tstar; dd[cnt] = d; cc[cnt] = s.gcol[g];
                        if (tstar > tmax) { tmax = tstar; tmax_i = cnt; }
                        ++cnt;
                    } else if (tstar < tmax) {
                        ts[tmax_i] = tstar; dd[tmax_i] = d; cc[tmax_i] = s.gcol[g];
                        tmax = -1.0f;
                        for (int j = 0; j < kColorCap; ++j)
                            if (ts[j] > tmax) { tmax = ts[j]; tmax_i = j; }
                    }
                } else if (seg_aabb(cam, dir, s.nodeAABB[2*child], s.nodeAABB[2*child+1])) {
                    if (sp < kStackSize) stack[sp++] = child;
                }
            }
        }
        if (cnt == 0) continue;

        // insertion sort by t (front to back)
        for (int a = 1; a < cnt; ++a) {
            float kt = ts[a], kd = dd[a]; float3 kc = cc[a]; int b = a - 1;
            while (b >= 0 && ts[b] > kt) { ts[b+1]=ts[b]; dd[b+1]=dd[b]; cc[b+1]=cc[b]; --b; }
            ts[b+1]=kt; dd[b+1]=kd; cc[b+1]=kc;
        }
        // composite front-to-back (alpha-blend, transmittance-weighted)
        float T = 1.0f; float3 Cc = make_float3(0.f, 0.f, 0.f);
        for (int a = 0; a < cnt; ++a) {
            float al = fminf(dd[a], 0.999f);
            Cc += cc[a] * (T * al);
            T *= (1.0f - al);
            if (T < 1e-3f) break;
        }
        num += Cc; den += (1.0f - T);
    }

    if (den > 1e-6f) return num * (1.0f / den);
    float3 cs;
    if (color_static(s, p, cs)) return cs;
    return color_nearest(s, p);
}

__global__ void colorize_kernel(
    GpuScene s, const float* __restrict__ verts, int n, float* rgb, int dynamic
) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    float3 p = make_float3(verts[3*i], verts[3*i+1], verts[3*i+2]);
    float3 col;
    if (dynamic && false) col = color_camera(s, p);
    else if (!color_static(s, p, col)) col = color_nearest(s, p);
    rgb[3*i+0] = col.x; rgb[3*i+1] = col.y; rgb[3*i+2] = col.z;
}

// Combine the camera occlusion term with the view-independent static density,
// mirroring the BVH field occ = 1 - (1-occ_static)(1-occ_front): occ_static
// protects real surfaces (high density on any surface, regardless of view) from
// being carved by an underestimating camera, while occ_front (the render term)
// fills occluded interior and carves visible free space.
__global__ void occ_combine_kernel(
    int n, float* __restrict__ occ, const float* __restrict__ occ_static
) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    float a = occ_static[i], b = occ[i];
    occ[i] = 1.0f - (1.0f - a) * (1.0f - b);
}

// Fallback for the render color path: verts the cameras couldn't color (rgb<0,
// e.g. seen by no camera or fully occluded) get the static density-weighted color.
__global__ void colorize_fallback_kernel(
    GpuScene s, const float* __restrict__ verts, int n, float* rgb
) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    if (rgb[3*i+0] >= 0.0f) return;
    float3 p = make_float3(verts[3*i], verts[3*i+1], verts[3*i+2]);
    float3 col;
    if (!color_static(s, p, col)) col = color_nearest(s, p);
    rgb[3*i+0] = col.x; rgb[3*i+1] = col.y; rgb[3*i+2] = col.z;
}

// ---------------------------------------------------------------------------
// Kernels: activation / point cloud
// ---------------------------------------------------------------------------
__global__ void activate_kernel(
    int N,
    const float* __restrict__ means,  const float* __restrict__ quats,
    const float* __restrict__ logsc,  const float* __restrict__ logit,
    const float* __restrict__ fdc,
    float3* mean, float3* ax0, float3* ax1, float3* ax2,
    float3* invs2, float* opac, float* radius, float* k2, int* valid, float3* gcol
) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= N) return;

    mean[i] = make_float3(means[3*i], means[3*i+1], means[3*i+2]);

    float w = quats[4*i], x = quats[4*i+1], y = quats[4*i+2], z = quats[4*i+3];
    float n = rsqrtf(fmaxf(w*w + x*x + y*y + z*z, 1e-20f));
    w *= n; x *= n; y *= n; z *= n;
    ax0[i] = make_float3(1 - 2*(y*y + z*z), 2*(x*y + w*z),     2*(x*z - w*y));
    ax1[i] = make_float3(2*(x*y - w*z),     1 - 2*(x*x + z*z), 2*(y*z + w*x));
    ax2[i] = make_float3(2*(x*z + w*y),     2*(y*z - w*x),     1 - 2*(x*x + y*y));

    float sx = __expf(logsc[3*i]), sy = __expf(logsc[3*i+1]), sz = __expf(logsc[3*i+2]);
    invs2[i] = make_float3(1.0f/(sx*sx), 1.0f/(sy*sy), 1.0f/(sz*sz));

    // base color from the SH band-0 (DC) coefficient
    const float C0 = 0.28209479177387814f;
    gcol[i] = make_float3(
        fminf(fmaxf(0.5f + C0 * fdc[3*i+0], 0.0f), 1.0f),
        fminf(fmaxf(0.5f + C0 * fdc[3*i+1], 0.0f), 1.0f),
        fminf(fmaxf(0.5f + C0 * fdc[3*i+2], 0.0f), 1.0f));

    float op = 1.0f / (1.0f + __expf(-logit[i]));
    opac[i] = op;
    if (op > ALPHA_THRESHOLD) {
        float kk2 = 2.0f * logf(op / ALPHA_THRESHOLD);
        k2[i] = kk2;
        radius[i] = sqrtf(kk2) * fmaxf(sx, fmaxf(sy, sz));
        valid[i] = 1;
    } else {
        k2[i] = 0.0f; radius[i] = 0.0f; valid[i] = 0;
    }
}

__global__ void pointcloud_kernel(
    int num_kept, const int* __restrict__ kept,
    const float3* mean, const float3* ax0, const float3* ax1, const float3* ax2,
    const float3* invs2, const float* k2, float min_axis_ratio, float* out
) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= num_kept) return;
    int g = kept[i];
    float k = sqrtf(k2[g]) * 2.5f;  // 2.5x the cutoff, for overlapping splats
    float3 m = mean[g];
    float3 is = invs2[g];
    float s0 = rsqrtf(is.x), s1 = rsqrtf(is.y), s2 = rsqrtf(is.z);
    float lo = min_axis_ratio * fmaxf(s0, fmaxf(s1, s2));
    float3 o0 = ax0[g] * (k * fmaxf(s0, lo));
    float3 o1 = ax1[g] * (k * fmaxf(s1, lo));
    float3 o2 = ax2[g] * (k * fmaxf(s2, lo));
    float3 pts[7] = { m, m+o0, m-o0, m+o1, m-o1, m+o2, m-o2 };
    long base = (long)i * 21;
    for (int j = 0; j < 7; ++j) {
        out[base + 3*j + 0] = pts[j].x;
        out[base + 3*j + 1] = pts[j].y;
        out[base + 3*j + 2] = pts[j].z;
    }
}

// ---------------------------------------------------------------------------
// Kernels: LBVH build
// ---------------------------------------------------------------------------
// Per kept Gaussian: its (real-space) leaf AABB, its Morton code (remapped),
// and an iota value.
__global__ void bvh_prep_kernel(
    int num_kept, const int* __restrict__ kept,
    const float3* mean, const float3* ax0, const float3* ax1, const float3* ax2,
    const float3* invs2, const float* k2,
    float3 remap_min, float3 remap_inv_ext, float rel_scale,
    float3* leafMin, float3* leafMax, uint64_t* morton, int* iota
) {
    int kp = blockIdx.x * blockDim.x + threadIdx.x;
    if (kp >= num_kept) return;
    int g = kept[kp];
    float3 m = mean[g];
    float3 is = invs2[g];
    float s0 = 1.0f/is.x, s1 = 1.0f/is.y, s2 = 1.0f/is.z;  // sigma^2 per axis
    float3 a0 = ax0[g], a1 = ax1[g], a2 = ax2[g];
    // diagonal of Sigma = R diag(sigma^2) R^T
    float cxx = a0.x*a0.x*s0 + a1.x*a1.x*s1 + a2.x*a2.x*s2;
    float cyy = a0.y*a0.y*s0 + a1.y*a1.y*s1 + a2.y*a2.y*s2;
    float czz = a0.z*a0.z*s0 + a1.z*a1.z*s1 + a2.z*a2.z*s2;
    float k = sqrtf(k2[g]);
    float3 bound = make_float3(k*sqrtf(cxx), k*sqrtf(cyy), k*sqrtf(czz));
    leafMin[kp] = m - bound;
    leafMax[kp] = m + bound;

    float3 rc = make_float3(
        (remap_coord_dev(m.x, rel_scale) - remap_min.x) * remap_inv_ext.x,
        (remap_coord_dev(m.y, rel_scale) - remap_min.y) * remap_inv_ext.y,
        (remap_coord_dev(m.z, rel_scale) - remap_min.z) * remap_inv_ext.z);
    morton[kp] = morton3D(rc);
    iota[kp] = kp;
}

// Karras 2012 single-level radix tree over the sorted Morton array.
__global__ void lbvh_internal_kernel(
    int n, const uint64_t* __restrict__ morton, const int* __restrict__ argsort,
    int2* __restrict__ internal, int* __restrict__ parent
) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n - 1) return;

    #define delta(a, b) \
        (((b) < 0 || (b) >= n) ? -1 : \
         (morton[a] == morton[b] ? 64 + __clz((a) ^ (b)) \
                                 : __clzll(morton[a] ^ morton[b])))

    int d = delta(i, i+1) - delta(i, i-1);
    d = d > 0 ? 1 : (d < 0 ? -1 : 0);
    int delta_min = delta(i, i-d);
    int lmax = 2;
    while (delta(i, i + lmax*d) > delta_min) lmax <<= 1;
    int l = 0;
    for (int t = lmax >> 1; t >= 1; t >>= 1)
        if (delta(i, i + (l+t)*d) > delta_min) l += t;
    int j = i + l*d;
    int delta_node = delta(i, j);
    int sp_ = 0;
    for (int tf = 2, t; (t = (l + tf - 1) / tf) >= 1; tf <<= 1)
        if (delta(i, i + (sp_ + t)*d) > delta_node) sp_ += t;
    int gamma = i + sp_*d + min(d, 0);

    int left  = (min(i,j) == gamma)     ? ~argsort[gamma]     : gamma;
    int right = (max(i,j) == gamma + 1) ? ~argsort[gamma+1]   : gamma + 1;
    internal[i] = make_int2(left, right);
    if (left  >= 0) atomicMax(&parent[left],  i);
    if (right >= 0) atomicMax(&parent[right], i);
    #undef delta
}

__global__ void lbvh_initaabb_kernel(int n_internal, float3* nodeAABB) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n_internal) return;
    nodeAABB[2*i]   = make_float3(1e30f, 1e30f, 1e30f);
    nodeAABB[2*i+1] = make_float3(-1e30f, -1e30f, -1e30f);
}

// Bottom-up node AABBs: seed from internal nodes that have a leaf child, then
// walk to the root via parent pointers (same merge pattern as the codebase).
__global__ void lbvh_aabb_kernel(
    int n,
    const int2* __restrict__ internal, const int* __restrict__ parent,
    const float3* __restrict__ leafMin, const float3* __restrict__ leafMax,
    float3* __restrict__ nodeAABB
) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n - 1) return;
    int2 ch = internal[i];
    if (ch.x >= 0 && ch.y >= 0) return;  // no leaf child -> filled from below

    float3 bmin = make_float3(1e30f, 1e30f, 1e30f);
    float3 bmax = make_float3(-1e30f, -1e30f, -1e30f);
    if (ch.x < 0) { bmin = fmin3(bmin, leafMin[~ch.x]); bmax = fmax3(bmax, leafMax[~ch.x]); }
    if (ch.y < 0) { bmin = fmin3(bmin, leafMin[~ch.y]); bmax = fmax3(bmax, leafMax[~ch.y]); }

    int node = i;
    do {
        bool covered =
            atomicMinF(&nodeAABB[2*node].x,   bmin.x) <= bmin.x &
            atomicMinF(&nodeAABB[2*node].y,   bmin.y) <= bmin.y &
            atomicMinF(&nodeAABB[2*node].z,   bmin.z) <= bmin.z &
            atomicMaxF(&nodeAABB[2*node+1].x, bmax.x) >= bmax.x &
            atomicMaxF(&nodeAABB[2*node+1].y, bmax.y) >= bmax.y &
            atomicMaxF(&nodeAABB[2*node+1].z, bmax.z) >= bmax.z;
        if (covered) break;
        node = parent[node];
    } while (node >= 0);
}

// ---------------------------------------------------------------------------
// Kernels: occupancy / bisection
// ---------------------------------------------------------------------------
__global__ void occ_kernel(
    GpuScene s, const float* __restrict__ pts, int n, float* occ, int dynamic
) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    float3 p = make_float3(pts[3*i], pts[3*i+1], pts[3*i+2]);
    occ[i] = occ_eval(s, p, dynamic != 0);
}

__global__ void bisect_kernel(
    GpuScene s, const float* __restrict__ cloud,
    const int* __restrict__ ea, const int* __restrict__ eb,
    const float* __restrict__ oa, const float* __restrict__ ob,
    int n, int iters, int dynamic, float* out
) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    int a = ea[i], b = eb[i];
    float3 pa = make_float3(cloud[3*a], cloud[3*a+1], cloud[3*a+2]);
    float3 pb = make_float3(cloud[3*b], cloud[3*b+1], cloud[3*b+2]);
    // bisection narrows the bracket [pa,pb] straddling iso; we also carry the
    // occupancy at each end so the final step is a linear interpolation rather
    // than a midpoint -- much more accurate for the same iteration count.
    float occa = oa[i], occb = ob[i];
    bool a_in = occa >= s.iso;
    for (int it = 0; it < iters; ++it) {
        float3 mid = (pa + pb) * 0.5f;
        float om = occ_eval(s, mid, dynamic != 0);
        if ((om >= s.iso) == a_in) { pa = mid; occa = om; }
        else                       { pb = mid; occb = om; }
    }
    float denom = occb - occa;
    float t = (fabsf(denom) > 1e-12f) ? (s.iso - occa) / denom : 0.5f;
    t = fminf(fmaxf(t, 0.0f), 1.0f);
    float3 cross = pa + (pb - pa) * t;
    out[3*i+0] = cross.x; out[3*i+1] = cross.y; out[3*i+2] = cross.z;
}


// ---------------------------------------------------------------------------
// Launchers (mesh/MeshingDevice.h)
// ---------------------------------------------------------------------------
void launch_activate(
    int N,
    const float* means, const float* quats, const float* logsc,
    const float* logit, const float* fdc,
    float3* mean, float3* ax0, float3* ax1, float3* ax2, float3* invs2,
    float* opac, float* radius, float* k2, int* valid, float3* gcol
) {
    if (N <= 0) return;
    activate_kernel<<<_LAUNCH_ARGS_1D(N, 256)>>>(
        N, means, quats, logsc, logit, fdc,
        mean, ax0, ax1, ax2, invs2, opac, radius, k2, valid, gcol);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

void launch_pointcloud(
    int num_kept, const int* kept,
    const float3* mean, const float3* ax0, const float3* ax1,
    const float3* ax2, const float3* invs2, const float* k2,
    float min_axis_ratio, float* out
) {
    if (num_kept <= 0) return;
    pointcloud_kernel<<<_LAUNCH_ARGS_1D(num_kept, 256)>>>(
        num_kept, kept, mean, ax0, ax1, ax2, invs2, k2, min_axis_ratio, out);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

void launch_bvh_prep(
    int num_kept, const int* kept,
    const float3* mean, const float3* ax0, const float3* ax1,
    const float3* ax2, const float3* invs2, const float* k2,
    float3 remap_min, float3 remap_inv_ext, float rel_scale,
    float3* leafMin, float3* leafMax, uint64_t* morton, int* iota
) {
    if (num_kept <= 0) return;
    bvh_prep_kernel<<<_LAUNCH_ARGS_1D(num_kept, 256)>>>(
        num_kept, kept, mean, ax0, ax1, ax2, invs2, k2,
        remap_min, remap_inv_ext, rel_scale, leafMin, leafMax, morton, iota);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

void launch_lbvh_internal(
    int n, const uint64_t* morton_sorted, const int* argsort,
    int2* internal, int* parent
) {
    if (n < 2) return;
    lbvh_internal_kernel<<<_LAUNCH_ARGS_1D(n - 1, 256)>>>(
        n, morton_sorted, argsort, internal, parent);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

void launch_lbvh_init_aabb(int n_internal, float3* nodeAABB) {
    if (n_internal <= 0) return;
    lbvh_initaabb_kernel<<<_LAUNCH_ARGS_1D(n_internal, 256)>>>(n_internal, nodeAABB);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

void launch_lbvh_aabb(
    int n, const int2* internal, const int* parent,
    const float3* leafMin, const float3* leafMax, float3* nodeAABB
) {
    if (n < 2) return;
    lbvh_aabb_kernel<<<_LAUNCH_ARGS_1D(n - 1, 256)>>>(
        n, internal, parent, leafMin, leafMax, nodeAABB);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

void launch_occ(const GpuScene& s, const float* pts, int n, float* occ,
                int dynamic) {
    if (n <= 0) return;
    occ_kernel<<<_LAUNCH_ARGS_1D(n, 128)>>>(s, pts, n, occ, dynamic);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

void launch_bisect(
    const GpuScene& s, const float* cloud,
    const int* ea, const int* eb, const float* oa, const float* ob,
    int n, int iters, int dynamic, float* out
) {
    if (n <= 0) return;
    bisect_kernel<<<_LAUNCH_ARGS_1D(n, 128)>>>(
        s, cloud, ea, eb, oa, ob, n, iters, dynamic, out);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

void launch_colorize(const GpuScene& s, const float* verts, int n, float* rgb,
                     int dynamic) {
    if (n <= 0) return;
    colorize_kernel<<<_LAUNCH_ARGS_1D(n, 128)>>>(s, verts, n, rgb, dynamic);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

void launch_occ_combine(int n, float* occ, const float* occ_static) {
    if (n <= 0) return;
    occ_combine_kernel<<<_LAUNCH_ARGS_1D(n, 128)>>>(n, occ, occ_static);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

void launch_colorize_fallback(const GpuScene& s, const float* verts, int n,
                              float* rgb) {
    if (n <= 0) return;
    colorize_fallback_kernel<<<_LAUNCH_ARGS_1D(n, 128)>>>(s, verts, n, rgb);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

} // namespace meshing

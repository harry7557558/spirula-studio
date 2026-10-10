#include <cuda_runtime.h>
#include <cstdint>
#include <cub/cub.cuh>

#include "generated/slang.cuh"
namespace SlangProjectionUtils {
#include "generated/set_namespace.cuh"
#include "generated/projection_utils.cuh"
}


#include <core/Tensor.h>
#include <core/Common.cuh>
#include "core/PixelFormat.h"
#include "core/CameraDistortion.cuh"
#include "engine/EngineState.h"
#include "engine/EngineCommon.h"
#include "core/Interpolation.cuh"

#include <mutex>


// ---------------------------------------------------------------------------
// Viewer concurrency primitives (file-scope rather than EngineState members so
// engine_reset()'s move-assignment stays valid; these objects don't need to be
// reset between scenes).
//
// What's protected:
//   * Every field of engine().viewer (init flags, N_post, dataset arrays,
//     thumbnail cache, BVH cache, host_seen_mask, pending_thumb).
//   * Every DevicePool slot whose key starts with "viewer." -- they are owned
//     exclusively by the three entrypoints below.
//
// Why a mutex is necessary:
//   The training thread enters engine_viewer_capture_thumbnails from inside
//   the training step. The viewer/HTTP thread enters engine_viewer_init and
//   engine_blit_view from outside any Python lock (post_processor closure is
//   invoked after Trainer.lock is released). Without serialization a
//   first-time viewer connect could re-allocate v.thumbnails mid-capture and
//   trigger an IMA from the kernel launched at the bottom of capture.
// ---------------------------------------------------------------------------
namespace {

std::mutex& viewer_mutex() {
    static std::mutex m;
    return m;
}

// High-priority CUDA stream used by viewer kernels so the device scheduler
// can pick them ahead of training-stream kernels when both are queued. Lazy
// init: created once on first use, destroyed at process exit (we don't
// destroy across engine_reset because the stream is not part of engine state).
cudaStream_t viewer_stream() {
    static cudaStream_t s = []() {
        int lo = 0, hi = 0;
        cudaDeviceGetStreamPriorityRange(&lo, &hi);  // hi is the higher-priority end
        cudaStream_t out = nullptr;
        cudaStreamCreateWithPriority(&out, cudaStreamNonBlocking, hi);
        return out;
    }();
    return s;
}

// Make viewer_stream() wait until all work currently queued on the default
// stream has completed. Use before launching a viewer kernel that reads data
// written by the training (default) stream.
inline void viewer_stream_wait_default() {
    cudaEvent_t evt;
    cudaEventCreateWithFlags(&evt, cudaEventDisableTiming);
    cudaEventRecord(evt, /*default stream=*/0);
    cudaStreamWaitEvent(viewer_stream(), evt, 0);
    cudaEventDestroy(evt);
}

// Make the default stream wait until all work currently queued on
// viewer_stream() has completed. Use before returning to Python from a viewer
// entry point whose output tensor will be consumed on the default stream
// (e.g. by tensor.cpu()).
inline void default_stream_wait_viewer() {
    cudaEvent_t evt;
    cudaEventCreateWithFlags(&evt, cudaEventDisableTiming);
    cudaEventRecord(evt, viewer_stream());
    cudaStreamWaitEvent(/*default stream=*/0, evt, 0);
    cudaEventDestroy(evt);
}

} // anonymous namespace

// Launch-arg macros that target the viewer stream (mirror _LAUNCH_ARGS_*D in
// Common.cuh which hard-code stream 0).
#define _LAUNCH_ARGS_1D_VS(n,b) ((n)==0?1:_CEIL_DIV(n,b)),b,0,viewer_stream()
#define _LAUNCH_ARGS_2D_VS(nx,ny,bx,by) dim3(_CEIL_DIV(nx,bx),_CEIL_DIV(ny,by),1),dim3(bx,by),0,viewer_stream()


// Run BODY(tier) for the tier `name` spells. The camera model stays runtime --
// per-camera for the dataset frusta, a plain argument for the view camera.
#define _SS_DISPATCH_DISTORTION(name, BODY)                                        \
    do { switch (cdt(name)) {                                                      \
        case CameraDistortionType::None:      BODY(CameraDistortionType::None);      break; \
        case CameraDistortionType::OpenCV:    BODY(CameraDistortionType::OpenCV);    break; \
        case CameraDistortionType::ThinPrism: BODY(CameraDistortionType::ThinPrism); break; \
        default: throw std::runtime_error(                                         \
            "Unknown camera distortion: " + std::string(name));                    \
    } } while (0)


inline constexpr int kNumFrustumSegments = 16;
inline constexpr int kNumFrustumFaces = 8;

// Ray through a frustum-boundary point, falling back to the last valid uv
// toward the principal point when the model's domain rejects it (NaN if none
// is). The tier is a RUNTIME value here -- the fill draws a whole camera
// table in one launch and a dataset may mix tiers, so it cannot be the
// launch-wide template argument the projection kernels use. Uniform per
// block (one camera per block).
template<CameraDistortionType distortion>
__device__ __forceinline__ float3 _frustum_ray(
    float2 uv, int camera_model,
    const CameraDistortionCoeffsBuffer& dist_coeffs_buffer, uint32_t bid
) {
    auto dist_coeffs = dist_coeffs_buffer.load<distortion>(bid);
    float3 raydir = float3{NAN, NAN, NAN};
    if (!SlangDistortion<distortion>::generate_ray(uv, camera_model, dist_coeffs, &raydir)) {
        float t0 = 0.0f, t1 = 1.0f;
        for (int iter = 0; iter < 12; ++iter) {
            float t = 0.5f*(t0+t1);
            float3 temp;
            if (SlangDistortion<distortion>::generate_ray(uv*t, camera_model, dist_coeffs, &temp))
                t0 = t, raydir = temp;
            else
                t1 = t;
        }
    }
    return raydir;
}

__device__ __forceinline__ float3 frustum_ray(
    float2 uv, int camera_model, int distortion,
    const CameraDistortionCoeffsBuffer& dist_coeffs_buffer, uint32_t bid
) {
    switch ((CameraDistortionType)distortion) {
        case CameraDistortionType::OpenCV:
            return _frustum_ray<CameraDistortionType::OpenCV>(uv, camera_model, dist_coeffs_buffer, bid);
        case CameraDistortionType::ThinPrism:
            return _frustum_ray<CameraDistortionType::ThinPrism>(uv, camera_model, dist_coeffs_buffer, bid);
        default:
            return _frustum_ray<CameraDistortionType::None>(uv, camera_model, dist_coeffs_buffer, bid);
    }
}

__global__ void fill_frustum_segments_kernel(
    const float4* __restrict__ intrins, // [N, 4]
    const int32_t* __restrict__ widths, // [N]
    const int32_t* __restrict__ heights, // [N]
    const int* __restrict__ camera_models, // [N]
    CameraDistortionCoeffsBuffer dist_coeffs_buffer, // [N, ...]
    const int32_t* __restrict__ distortions, // [N] CameraDistortionType, or null
    const float* __restrict__ camera_to_worlds,  // [N, 3, 4]
    float size,
    float4* __restrict__ lss_buffer,  // [N, 8*kNumFrustumSegments, 2*4]
    float4* __restrict__ tri_buffer  // [N, 4*kNumFrustumFaces*kNumFrustumFaces, 4*4]
) {
    uint32_t bid = blockIdx.x;
    uint32_t tid = threadIdx.x;

    // intrins
    float4 intrin = intrins[bid];
    float fx = intrin.x, fy = intrin.y, cx = intrin.z, cy = intrin.w;
    float width = (float)widths[bid];
    float height = (float)heights[bid];
    CameraModelType camera_model = (CameraModelType)camera_models[bid];
    int distortion = distortions ? distortions[bid] : 0;
    float2 corners[4] = {
        {-cx / fx, -cy / fy},
        {(width-cx) / fx, -cy / fy},
        {(width-cx) / fx, (height-cy) / fy},
        {-cx / fx, (height-cy) / fy},
    };
    float sxy = sqrtf(fx * fy / (width * height));
    float sz = rsqrtf(sxy);
    sxy *= sz * size;
    const float r0 = 0.015f*size*sz, r1 = 0.03f*size*sz;
    sz = size / sz;

    // extrins
    camera_to_worlds += 12 * bid;
    float3x3 R = {  // row major
        camera_to_worlds[0], camera_to_worlds[1], camera_to_worlds[2],  // 1st row
        camera_to_worlds[4], camera_to_worlds[5], camera_to_worlds[6],  // 2nd row
        camera_to_worlds[8], camera_to_worlds[9], camera_to_worlds[10],  // 3rd row
    };
    float3 t = { camera_to_worlds[3], camera_to_worlds[7], camera_to_worlds[11] };

    // polylines
    __shared__ float3 polylines[4*kNumFrustumSegments];
    for (int i = tid; i < 4*kNumFrustumSegments; i += blockDim.x) {
        int corner_idx = i / kNumFrustumSegments;
        float2 uv = corners[corner_idx] + (corners[(corner_idx+1)%4] - corners[corner_idx])
             * ((float)(i % kNumFrustumSegments) / kNumFrustumSegments);
        float3 p = frustum_ray(uv, (int)camera_model, distortion,
                               dist_coeffs_buffer, bid);
        if (camera_model == CameraModelType::FISHEYE || camera_model == CameraModelType::EQUISOLID ||
            camera_model == CameraModelType::EQUIRECTANGULAR)
            // Wide / full-sphere models: place frustum verts on a sphere shell
            // (the pinhole p/p.z below explodes on rays with p.z <= 0, e.g. the
            // equirectangular back hemisphere).
            p = normalize(p) * sqrtf((2.0f*sxy*sxy + sz*sz) / 3.0f);
        else
            p = p * float3{sxy, sxy, sz} / p.z;
        p = float3{dot(R[0], p), dot(R[1], p), dot(R[2], p)} + t;
        polylines[i] = p;
    }
    __syncthreads();

    // frame
    lss_buffer += bid * (8*kNumFrustumSegments*2);
    for (int i = tid; i < 4*kNumFrustumSegments; i += blockDim.x) {
        float3 p0 = polylines[i];
        float3 p1 = polylines[(i+1)%(4*kNumFrustumSegments)];
        if (!isfinite(dot(p0, p1)))
            p0 = p1 = t;
        lss_buffer[2*i] = {p0.x, p0.y, p0.z, r1};
        lss_buffer[2*i+1] = {p1.x, p1.y, p1.z, r1};
    }

    // anchor
    lss_buffer += 4*kNumFrustumSegments*2;
    for (int i = tid; i < 4*kNumFrustumSegments; i += blockDim.x) {
        float3 p = polylines[(i/kNumFrustumSegments)*kNumFrustumSegments];
        float s0 = (float)(i%kNumFrustumSegments) / (float)kNumFrustumSegments;
        float s1 = s0 + 1.0f / (float)kNumFrustumSegments;
        float3 p0 = t + (p - t) * s0;
        float3 p1 = t + (p - t) * s1;
        if (!isfinite(dot(p0, p1)))
            p0 = p1 = t;
        lss_buffer[2*i] = {p0.x, p0.y, p0.z, r0 + (r1 - r0) * s0};
        lss_buffer[2*i+1] = {p1.x, p1.y, p1.z, r0 + (r1 - r0) * s1};
    }

    // faces
    tri_buffer += bid * (4*kNumFrustumFaces*kNumFrustumFaces * 4);
    for (int i = tid; i < kNumFrustumFaces*kNumFrustumFaces; i += blockDim.x) {
        float u0 = (float)(i / kNumFrustumFaces) / kNumFrustumFaces;
        float v0 = (float)(i % kNumFrustumFaces) / kNumFrustumFaces;
        float u1 = u0 + 1.0f / kNumFrustumFaces;
        float v1 = v0 + 1.0f / kNumFrustumFaces;
        float uc = 0.5f * (u0 + u1);
        float vc = 0.5f * (v0 + v1);
        float2 uvs[5] = {
            {uc, vc}, {u0, v0}, {u0, v1}, {u1, v1}, {u1, v0},
        };
        float3 verts[5];
        #pragma unroll
        for (int pi = 0; pi < 5; ++pi) {
            float u = uvs[pi].x, v = uvs[pi].y;
            float2 uv =
                corners[0] * (1.0f-u)*(1.0f-v) +
                corners[1] * u*(1.0f-v) +
                corners[2] * u*v +
                corners[3] * (1.0f-u)*v;
            float3 p = frustum_ray(uv, (int)camera_model, distortion,
                                   dist_coeffs_buffer, bid);
        if (camera_model == CameraModelType::FISHEYE || camera_model == CameraModelType::EQUISOLID ||
                camera_model == CameraModelType::EQUIRECTANGULAR)
                p = normalize(p) * sqrtf((2.0f*sxy*sxy + sz*sz) / 3.0f);
            else
                p = p * float3{sxy, sxy, sz} / p.z;
            p = float3{dot(R[0], p), dot(R[1], p), dot(R[2], p)} + t;
            verts[pi] = p;
        }
        #pragma unroll
        for (int pi = 0; pi < 4; ++pi) {
            float3 p0 = verts[0];
            float3 p1 = verts[pi+1];
            float3 p2 = verts[(pi+1)%4+1];
            float2 uv0 = uvs[0];
            float2 uv1 = uvs[pi+1];
            float2 uv2 = uvs[(pi+1)%4+1];
            tri_buffer[4*(4*i+pi)+0] = {p0.x, p0.y, p0.z, p1.x};
            tri_buffer[4*(4*i+pi)+1] = {p1.y, p1.z, p2.x, p2.y};
            tri_buffer[4*(4*i+pi)+2] = {p2.z, uv0.x, uv0.y, uv1.x};
            tri_buffer[4*(4*i+pi)+3] = {uv1.y, uv2.x, uv2.y, __uint_as_float(bid)};
        }
    }
}



inline __device__ void linear_swept_sphere_aabb(
    const float4* __restrict__ lss_buffer,
    uint32_t idx,
    float3 &bmin, float3 &bmax
) {
    float4 e0 = lss_buffer[2*idx];  // xyzr
    float4 e1 = lss_buffer[2*idx+1];  // xyzr
    bmin = {
        fminf(e0.x-e0.w, e1.x-e1.w),
        fminf(e0.y-e0.w, e1.y-e1.w),
        fminf(e0.z-e0.w, e1.z-e1.w)
    };
    bmax = {
        fmaxf(e0.x+e0.w, e1.x+e1.w),
        fmaxf(e0.y+e0.w, e1.y+e1.w),
        fmaxf(e0.z+e0.w, e1.z+e1.w)
    };
}

inline __device__ void unpack_triangle(
    const float4* __restrict__ buffer,
    const uint32_t idx,
    float3 verts[3],
    float2 uvs[3],
    uint32_t &cam_idx
) {
    float4 e0 = buffer[4*idx+0];
    float4 e1 = buffer[4*idx+1];
    float4 e2 = buffer[4*idx+2];
    float4 e3 = buffer[4*idx+3];
    verts[0] = {e0.x, e0.y, e0.z};
    verts[1] = {e0.w, e1.x, e1.y};
    verts[2] = {e1.z, e1.w, e2.x};
    uvs[0] = {e2.y, e2.z};
    uvs[1] = {e2.w, e3.x};
    uvs[2] = {e3.y, e3.z};
    cam_idx = __float_as_uint(e3.w);
}

// Inflation applied to every triangle box, relative to its coordinate
// magnitude. A frustum face is a plane, so a camera whose image plane is
// perpendicular to a world axis produces triangles -- and BVH nodes over them
// -- with zero extent along that axis. ray_aabb_intersection is a slab test,
// and a slab of half-extent 0 contributes the same value to both ends, so the
// node's tmin and tmax come out equal and `tmin < tmax` rejects it: the node is
// never entered and that camera's whole face silently disappears. Its wireframe
// stays, because a swept sphere's box is inflated by the capsule radius, which
// is why this reads as "one camera frustum with no image in it".
//
// Not a corner case: a COLMAP model has exactly one image at identity -- the
// mapper's seed, which is what defines the world frame -- so every COLMAP
// dataset has exactly one such camera. Relative to the coordinates rather than
// to the box, so the pad stays above the float resolution of the ray
// arithmetic wherever the box sits.
inline constexpr float kTriAabbPad = 1e-5f;

inline __device__ void triangle_aabb(
    const float4* __restrict__ tri_buffer,
    uint32_t idx,
    float3 &bmin, float3 &bmax
) {
    float3 verts[3];
    float2 uvs[3];
    uint32_t cam_idx;
    unpack_triangle(tri_buffer, idx, verts, uvs, cam_idx);
    bmin = fmin(fmin(verts[0], verts[1]), verts[2]);
    bmax = fmax(fmax(verts[0], verts[1]), verts[2]);
    const float m = fmaxf(
        fmaxf(fmaxf(fabsf(bmin.x), fabsf(bmin.y)), fabsf(bmin.z)),
        fmaxf(fmaxf(fabsf(bmax.x), fabsf(bmax.y)), fabsf(bmax.z)));
    const float pad = kTriAabbPad * fmaxf(m, 1e-20f);
    bmin = bmin - make_float3(pad);
    bmax = bmax + make_float3(pad);
}

inline constexpr uint kMortonBitsPerDim = 21;

__device__ __forceinline__ uint64_t insert_2_zeros_between_bits(uint64_t x) {
    x = (x | (x << 32)) & (uint64_t)0xFFFF00000000FFFFULL;
    x = (x | (x << 16)) & (uint64_t)0x00FF0000FF0000FFULL;
    x = (x | (x << 8))  & (uint64_t)0x100F00F00F00F00FULL;
    x = (x | (x << 4))  & (uint64_t)0x10C30C30C30C30C3ULL;
    x = (x | (x << 2))  & (uint64_t)0x1249249249249249ULL;
    return x;
}

__device__ __forceinline__ uint64_t get_morton_key(float3 pos) {
    constexpr uint64_t mask_comp = ((uint64_t)1 << (uint64_t)kMortonBitsPerDim) - 1;
    uint64_t x = uint64_t(pos.x * exp2f(kMortonBitsPerDim) + 0.5f) & mask_comp;
    uint64_t y = uint64_t(pos.y * exp2f(kMortonBitsPerDim) + 0.5f) & mask_comp;
    uint64_t z = uint64_t(pos.z * exp2f(kMortonBitsPerDim) + 0.5f) & mask_comp;
    constexpr uint64_t mask_full = (((uint64_t)1 << (uint64_t)(3*kMortonBitsPerDim)) - 1);
    x = insert_2_zeros_between_bits(x) & mask_full;
    y = insert_2_zeros_between_bits(y) & mask_full;
    z = insert_2_zeros_between_bits(z) & mask_full;
    return (x * 2 + y) * 2 + z;
}

enum class VisPrimitive {
    LinearSweptSphere,
    Triangle,
};

template<VisPrimitive primitive>
__global__ void compute_aabb_kernel(
    long num_elem,
    const float4* __restrict__ buffer,
    float3* __restrict__ aabb_reduced
) {
    unsigned gid = blockIdx.x * blockDim.x + threadIdx.x;
    if (gid >= num_elem)
        return;

    float3 aabb_min, aabb_max;
    if (primitive == VisPrimitive::LinearSweptSphere)
        linear_swept_sphere_aabb(buffer, gid, aabb_min, aabb_max);
    else if (primitive == VisPrimitive::Triangle)
        triangle_aabb(buffer, gid, aabb_min, aabb_max);

    auto block = cg::this_thread_block();
    cg::thread_block_tile<WARP_SIZE> warp = cg::tiled_partition<WARP_SIZE>(block);

    aabb_min.x = cg::reduce(warp, aabb_min.x, cg::less<float>());
    if (warp.thread_rank() == 0) atomicMin((float*)aabb_reduced+0, aabb_min.x);
    aabb_min.y = cg::reduce(warp, aabb_min.y, cg::less<float>());
    if (warp.thread_rank() == 0) atomicMin((float*)aabb_reduced+1, aabb_min.y);
    aabb_min.z = cg::reduce(warp, aabb_min.z, cg::less<float>());
    if (warp.thread_rank() == 0) atomicMin((float*)aabb_reduced+2, aabb_min.z);

    aabb_max.x = cg::reduce(warp, aabb_max.x, cg::greater<float>());
    if (warp.thread_rank() == 0) atomicMax((float*)aabb_reduced+3, aabb_max.x);
    aabb_max.y = cg::reduce(warp, aabb_max.y, cg::greater<float>());
    if (warp.thread_rank() == 0) atomicMax((float*)aabb_reduced+4, aabb_max.y);
    aabb_max.z = cg::reduce(warp, aabb_max.z, cg::greater<float>());
    if (warp.thread_rank() == 0) atomicMax((float*)aabb_reduced+5, aabb_max.z);
}

template<VisPrimitive primitive>
__global__ void fill_sorting_keys_kernel(
    const uint32_t num_elem,
    const float4* __restrict__ buffer,
    float3 root_min, float3 root_max,
    uint64_t* __restrict__ splat_keys
) {
    unsigned tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid >= num_elem) return;

    float3 aabb_min, aabb_max;
    if (primitive == VisPrimitive::LinearSweptSphere)
        linear_swept_sphere_aabb(buffer, tid, aabb_min, aabb_max);
    else if (primitive == VisPrimitive::Triangle)
        triangle_aabb(buffer, tid, aabb_min, aabb_max);
    float3 aabb_center = (aabb_min + aabb_max) * 0.5f;

    uint64_t key = get_morton_key((aabb_center - root_min) / (root_max - root_min));
    splat_keys[tid] = key;
}

__global__ void fill_lbvh_internal_nodes_kernel(
    const uint32_t num_elements,
    const uint64_t* __restrict__ morton,
    const int32_t* __restrict__ element_indices,
    int2* __restrict__ internal_nodes,
    int32_t* __restrict__ parent_nodes
) {
    // https://developer.nvidia.com/blog/parallelforall/wp-content/uploads/2012/11/karras2012hpg_paper.pdf
    int i = blockIdx.x * blockDim.x + threadIdx.x;

    if (i >= num_elements-1)
        return;

    #define delta(i, j) \
        (((j)<0 || (j)>=num_elements) ? -1 : morton[i] == morton[j] ? 64 + __clz(i ^ j) : __clzll(morton[i] ^ morton[j]))

    // Determine direction of the range (+1 or -1)
    int d = delta(i,i+1) - delta(i,i-1);
    d = d > 0 ? 1 : d < 0 ? -1 : 0;

    // Compute upper bound for the length of the range
    int delta_min = delta(i, i-d);
    int lmax = 2;
    while (delta(i, i+lmax*d) > delta_min)
        lmax <<= 1;

    // Find the other end using binary search
    int l = 0;
    for (int t = lmax>>1; t >= 1; t >>= 1)
        if (delta(i, i+(l+t)*d) > delta_min)
            l += t;
    int j = i + l * d;

    // Find the split position using binary search
    int delta_node = delta(i, j);
    int s = 0;
    for (int tf = 2, t; (t = (l+tf-1)/tf) >= 1; tf <<= 1)
        if (delta(i, i+(s+t)*d) > delta_node)
            s += t;
    int gamma = i + s*d + min(d, 0);

    // Output child pointers
    // regular for internal node, bit flip for leaf
    int left = min(i,j) == gamma ? ~element_indices[gamma] : gamma;
    int right = max(i,j) == gamma+1 ? ~element_indices[gamma+1] : gamma+1;
    internal_nodes[i] = make_int2(left, right);

    // Output parent pointers
    if (left >= 0)
        atomicMax(&parent_nodes[left], i);
    if (right >= 0)
        atomicMax(&parent_nodes[right], i);

    #undef delta
}

__global__ void fill_tree_init_aabb_kernel(
    unsigned num_cells,
    float3* __restrict__ treeAABB
) {
    unsigned tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid >= num_cells) return;
    treeAABB[2*tid+0] = make_float3(1e10f);
    treeAABB[2*tid+1] = -make_float3(1e10f);
}

template<VisPrimitive primitive>
__global__ void compute_lbvh_aabb_kernel(
    const uint32_t num_elements,
    const float4* __restrict__ buffer,
    const int2* __restrict__ internal_nodes,
    const int32_t* __restrict__ parent_nodes,
    float3* __restrict__ treeAABB
) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= num_elements-1) return;

    int2 children = internal_nodes[i];
    if (children.x >= 0 && children.y >= 0)
        return;

    // find splat AABB
    float3 aabb_min, aabb_max;
    if (children.x < 0) {
        if (primitive == VisPrimitive::LinearSweptSphere)
            linear_swept_sphere_aabb(buffer, ~children.x, aabb_min, aabb_max);
        else if (primitive == VisPrimitive::Triangle)
            triangle_aabb(buffer, ~children.x, aabb_min, aabb_max);
    }
    if (children.y < 0) {
        float3 aabb_min1, aabb_max1;
        if (primitive == VisPrimitive::LinearSweptSphere)
            linear_swept_sphere_aabb(buffer, ~children.y, aabb_min1, aabb_max1);
        else if (primitive == VisPrimitive::Triangle)
            triangle_aabb(buffer, ~children.y, aabb_min1, aabb_max1);
        if (children.x < 0)
            aabb_min = fmin(aabb_min, aabb_min1),
            aabb_max = fmax(aabb_max, aabb_max1);
        else
            aabb_min = aabb_min1, aabb_max = aabb_max1;
    }

    // fill parent AABB
    do {
        if (atomicMin(&treeAABB[2*i].x, aabb_min.x) < aabb_min.x &
            atomicMin(&treeAABB[2*i].y, aabb_min.y) < aabb_min.y &
            atomicMin(&treeAABB[2*i].z, aabb_min.z) < aabb_min.z &
            atomicMax(&treeAABB[2*i+1].x, aabb_max.x) > aabb_max.x &
            atomicMax(&treeAABB[2*i+1].y, aabb_max.y) > aabb_max.y &
            atomicMax(&treeAABB[2*i+1].z, aabb_max.z) > aabb_max.z
        ) break;
    } while ((i = parent_nodes[i]) >= 0);

}



inline __device__ float ray_aabb_intersection(
    float3 bc, float3 br,
    float3 ro, float3 rd,
    float t0, float t1
) {
    float3 n = (bc - ro) / rd;
    float3 k = br / float3{fabsf(rd.x), fabsf(rd.y), fabsf(rd.z)};
    float tmin = fmaxf(n.x-k.x, fmaxf(n.y-k.y, n.z-k.z));
    float tmax = fminf(n.x+k.x, fminf(n.y+k.y, n.z+k.z));
    bool intersect = tmin < tmax && tmax > t0 && tmin < t1;
    // if (intersect) t0 = fmaxf(t0, tmin), t1 = fminf(t1, tmax);
    return intersect ? (tmin > t0 ? tmin : tmax) : -1.0f;
}

inline __device__ float ray_sphere_intersection(
    float3 c, float r,
    float3 ro, float3 rd,
    float t0, float t1
) {
    float3 q = ro - c;
    float b = dot(q, rd);
    float h = b*b + r*r - dot(q, q);
    float d = sqrtf(h);
    float tmin = -b - d, tmax = -b + d;
    return h > 0.0f ? (tmin > t0 ? tmin : tmax) : -1.0f;
}

inline __device__ float ray_linear_swept_sphere_intersection(
    float3 pa, float3 pb,
    float ra, float rb,
    float3 ro, float3 rd,
    float t0, float t1
) {
    // https://www.shadertoy.com/view/MlKfzm by Inigo Quilez, MIT license
    float3 ba = pb - pa, oa = ro - pa, ob = ro - pb;
    float rr = ra - rb;
    float m0 = dot(ba,ba), m1 = dot(ba,oa), m2 = dot(ba,rd), m3 = dot(rd,oa),
        m5 = dot(oa,oa), m6 = dot(ob,rd), m7 = dot(ob,ob);
    float d2 = m0 - rr*rr;
	float k2 = d2 - m2*m2;
    float k1 = d2*m3 - m1*m2 + m2*rr*ra;
    float k0 = d2*m5 - m1*m1 + m1*rr*ra*2.0f - m0*ra*ra;
	float h = k1*k1 - k0*k2;
	if (h < 0.0f)
        return -1.0f;
    float t = (-sqrtf(h)-k1)/k2;
    float y = m1 - ra*rr + t*m2;
    if (y > 0.0f && y < d2)
        return (t-t0)*(t-t1) < 0.0f ? t : -1.0f;
    float h1 = m3*m3 - m5 + ra*ra;
    float h2 = m6*m6 - m7 + rb*rb;
    if (fmaxf(h1, h2) < 0.0f)
        return -1.0f;
    float r = 1e20f;
    if (h1 > 0.0f)
    	r = -m3 - sqrtf(h1);
	if (h2 > 0.0f) {
    	float t = -m6 - sqrtf(h2);
        if ((t-t0)*(t-t1) < 0.0f && t < r) r = t;
    }
    return (r-t0)*(r-t1) < 0.0f ? r : -1.0f;
}

inline __device__ bool ray_triangle_intersection(
    const float4* __restrict__ tri_buffer,
    uint32_t idx,
    float3 ro, float3 rd,
    float t0, float t1,
    float &out_t, uint32_t &out_cam_idx, float2 &out_uv
) {
    float3 verts[3];
    float2 uvs[3];
    unpack_triangle(tri_buffer, idx, verts, uvs, out_cam_idx);
    float3 po = verts[0] - ro;
    float3 a = verts[1] - verts[0];
    float3 b = verts[2] - verts[0];
    float invdet = 1.0f / dot(rd, cross(a, b));
    float t = dot(po, cross(a, b)) * invdet;
    float u = dot(rd, cross(b, po)) * invdet;
    float v = dot(rd, cross(po, a)) * invdet;
    float w = 1.0f - u - v;
    bool intersect = (t > t0 && t < t1 && u > 0.0f && v > 0.0f && w > 0.0f);
    if (intersect) {
        out_t = t;
        out_uv = uvs[0] * w + uvs[1] * u + uvs[2] * v;
    }
    return intersect;
}


template<CameraDistortionType distortion>
__global__ void blit_aabb_bvh_kernel(
    TensorView<float, 3> render_rgbs,  // [H, W, 3]
    const TensorView<float, 3> render_depths,  // [H, W, 1]
    const TensorView<float, 3> render_alphas,  // [H, W, 1]
    const int view_camera_model,
    const int image_width,
    const int image_height,
    const float4* __restrict__ view_intrins,  // [4]
    const float* __restrict__ view_viewmat,  // [4, 4]
    const CameraDistortionCoeffsBuffer view_dist_coeffs,
    const int num_aabb,
    const float3* __restrict__ aabb_buffer  // [N, 2, 3]
) {
    const int pix_x = blockIdx.x * blockDim.x + threadIdx.x;
    const int pix_y = blockIdx.y * blockDim.y + threadIdx.y;
    if (pix_x >= image_width || pix_y >= image_height)
        return;

    constexpr int MSAA = 2;
    float3 ray_o[MSAA*MSAA], ray_d[MSAA*MSAA];
    bool inside = false;
    {
        float4 intrin = view_intrins[0];
        float fx = intrin.x, fy = intrin.y, cx = intrin.z, cy = intrin.w;
        float3x3 R = {  // row major
            view_viewmat[0], view_viewmat[1], view_viewmat[2],  // 1st row
            view_viewmat[4], view_viewmat[5], view_viewmat[6],  // 2nd row
            view_viewmat[8], view_viewmat[9], view_viewmat[10],  // 3rd row
        };
        float3 t = { view_viewmat[3], view_viewmat[7], view_viewmat[11] };
        auto dist_coeffs = view_dist_coeffs.load<distortion>(0);

        #pragma unroll
        for (int i = 0; i < MSAA*MSAA; ++i) {
            const float px = (float)pix_x + (i/MSAA + 0.5f) / (float)MSAA;
            const float py = (float)pix_y + (i%MSAA + 0.5f) / (float)MSAA;
            ray_o[i] = SlangProjectionUtils::transform_ray_o(R, t);
            inside |= SlangDistortion<distortion>::generate_ray(
                {(px-cx)/fx, (py-cy)/fy},
                view_camera_model, dist_coeffs,
                &ray_d[i]
            );
            ray_d[i] = SlangProjectionUtils::transform_ray_d(R, ray_d[i]);
        }
    }

    if (!inside)
        return;

    float depth = render_depths.load1(pix_y, pix_x);
    float opac = render_alphas.load1(pix_y, pix_x);
    depth /= fminf(fmaxf(opac, 1e-6f) / 0.5f, 1.0f);
    if (depth == 0.0f)
        depth = 1e10f;

    float counts[MSAA*MSAA] = {0.0f};

    for (int li = 0; li < num_aabb; ++li) {
        float3 p0 = aabb_buffer[2*li];
        float3 p1 = aabb_buffer[2*li+1];

        for (int i = 0; i < MSAA*MSAA; ++i) {
            // if (counts[i] == 1.0f)
            //     continue;
            float ti = ray_aabb_intersection(
                0.5f*(p0+p1), 0.5f*(p1-p0),
                ray_o[i], ray_d[i], 0.0f, depth
            );
            if (ti > 0.0f)
                counts[i] += 1.0f;
        }
    }

    float alpha_final = 0.0f;
    #pragma unroll
    for (int i = 0; i < MSAA*MSAA; ++i) {
        float a = counts[i] / (float)(MSAA*MSAA);
        alpha_final += a;
    }
    alpha_final /= (log2f(num_aabb) + 1);
    alpha_final = alpha_final / (1.0f + alpha_final);

    if (alpha_final > 0.0f) {
        float3 rgb = render_rgbs.load3(pix_y, pix_x);
        rgb = rgb * (1.0f - alpha_final) + float3{1, 0, 0} * alpha_final;
        render_rgbs.store3(pix_y, pix_x, rgb);
    }
}


__constant__ float COEFFS_turbo[3][8] = {
    { 0.14637796, 2.94711014, -10.15061040, -90.46877071, 550.44382083, -1061.31232675, 874.85901369, -266.03287948 },
    { 0.08594198, 2.06520532, 9.64581379, -55.09180383, 149.40813531, -245.26823636, 202.98596195, -63.82163439 },
    { 0.23431523, 6.33792818, 9.26380342, -203.76964931, 604.07329733, -766.82678386, 447.99287080, -97.24397473 },
};

inline __device__ float3 colormap_turbo(float t) {
    float3 c{0.0f, 0.0f, 0.0f};
    #pragma unroll
    for (int k = 7; k >= 0; --k)
        c = c * t + float3{COEFFS_turbo[0][k], COEFFS_turbo[1][k], COEFFS_turbo[2][k]};
    return c;
}

__constant__ float COEFFS_viridis[3][4] = {
    { 0.29387218, 0.04824199, -2.14339482, 2.89151588 },
    { 0.01752027, 1.23905227, -0.16754949, -0.17213704 },
    { 0.34360395, 1.23133794, -1.71784815, 0.17750210 },
};

inline __device__ float3 colormap_viridis(float t) {
    float3 c{0.0f, 0.0f, 0.0f};
    #pragma unroll
    for (int k = 3; k >= 0; --k)
        c = c * t + float3{COEFFS_viridis[0][k], COEFFS_viridis[1][k], COEFFS_viridis[2][k]};
    return c;
}

__constant__ float COEFFS_magma[3][5] = {
    { -0.00157294, 0.45897387, 4.30604404, -5.49016037, 1.69082060 },
    { -0.01042786, 0.82685306, -2.93736985, 5.48987916, -2.36381252 },
    { -0.05490989, 3.73227094, -7.52578597, 3.89534203, 0.75796302 },
};

inline __device__ float3 colormap_magma(float t) {
    float3 c{0.0f, 0.0f, 0.0f};
    #pragma unroll
    for (int k = 4; k >= 0; --k)
        c = c * t + float3{COEFFS_magma[0][k], COEFFS_magma[1][k], COEFFS_magma[2][k]};
    return c;
}

__constant__ float COEFFS_inferno[3][5] = {
    { -0.01087222, 0.74909880, 3.64171238, -5.11462265, 1.66959821 },
    { -0.00208239, 0.51010061, -1.86975118, 4.46548484, -2.07978236 },
    { 0.00011545, 2.59796038, -2.30104808, -6.74156149, 7.14515755 },
};

inline __device__ float3 colormap_inferno(float t) {
    float3 c{0.0f, 0.0f, 0.0f};
    #pragma unroll
    for (int k = 4; k >= 0; --k)
        c = c * t + float3{COEFFS_inferno[0][k], COEFFS_inferno[1][k], COEFFS_inferno[2][k]};
    return c;
}

__constant__ float COEFFS_cividis[3][4] = {
    { -0.05784352, 1.43486697, -0.95976947, 0.59905315 },
    { 0.13417334, 0.69925032, -0.07321042, 0.14530728 },
    { 0.38674306, 0.05514949, 0.66155846, -0.89703048 },
};

inline __device__ float3 colormap_cividis(float t) {
    float3 c{0.0f, 0.0f, 0.0f};
    #pragma unroll
    for (int k = 3; k >= 0; --k)
        c = c * t + float3{COEFFS_cividis[0][k], COEFFS_cividis[1][k], COEFFS_cividis[2][k]};
    return c;
}


inline __device__ float3 get_thumbnail_bilinear(
    const TensorView<uint8_t, 4> image,  // [B, H, W, 4]
    uint32_t bid,
    float2 uv
) {
    const int W = image.shape[2],
        H = image.shape[1];
    float x = fminf(fmaxf(uv.x * (float)W + 0.5f, 0.0f), (float)(W-1));
    float y = fminf(fmaxf(uv.y * (float)H + 0.5f, 0.0f), (float)(H-1));
    int x0 = (long)floorf(x);
    int x1 = x0 + 1;
    int y0 = (long)floorf(y);
    int y1 = y0 + 1;
    int wx1 = (int)(256 * (x - x0));
    int wx0 = 256 - wx1;
    int wy1 = (int)(256 * (y - y0));
    int wy0 = 256 - wy1;

    auto c00 = image.load4(bid, y0, x0);
    auto c10 = image.load4(bid, y0, x1);
    auto c01 = image.load4(bid, y1, x0);
    auto c11 = image.load4(bid, y1, x1);

    float4 rgba = float4{
        (float)(c00.x * (wx0 * wy0) + c10.x * (wx1 * wy0) + c01.x * (wx0 * wy1) + c11.x * (wx1 * wy1)),
        (float)(c00.y * (wx0 * wy0) + c10.y * (wx1 * wy0) + c01.y * (wx0 * wy1) + c11.y * (wx1 * wy1)),
        (float)(c00.z * (wx0 * wy0) + c10.z * (wx1 * wy0) + c01.z * (wx0 * wy1) + c11.z * (wx1 * wy1)),
        (float)(c00.w * (wx0 * wy0) + c10.w * (wx1 * wy0) + c01.w * (wx0 * wy1) + c11.w * (wx1 * wy1)),
    } * (1.0f / (255.0f*256.0f*256.0f));
    // Uncaptured thumbnail (alpha 0) -> magenta
    if (rgba.w < 0.5f) {
        int c = (int)floorf(uv.x * 4.0f) + (int)floorf(uv.y * 4.0f);
        return (c & 1) ? float3{0.85f, 0.05f, 0.85f} : float3{0.15f, 0.0f, 0.15f};
    }
    return float3{
        rgba.x * rgba.w,
        rgba.y * rgba.w,
        rgba.z * rgba.w
    };
}

template<CameraDistortionType distortion>
__global__ void blit_with_bvh_kernel(
    const TensorView<float, 3> render_rgbs,  // [H, W, 3]
    const TensorView<float, 3> render_depths,  // [H, W, 1]
    const TensorView<float, 3> render_alphas,  // [H, W, 1]
    const int view_camera_model,
    const int image_width,
    const int image_height,
    const float4* __restrict__ view_intrins,  // [4]
    const float* __restrict__ view_viewmat,  // [4, 4]
    const CameraDistortionCoeffsBuffer view_dist_coeffs,
    const float4* __restrict__ lss_buffer,
    const int2* __restrict__ lss_nodes,
    const float3* __restrict__ lss_aabb,
    const float4* __restrict__ tri_buffer,
    const int2* __restrict__ tri_nodes,
    const float3* __restrict__ tri_aabb,
    // The LSS buffer is [camera frusta | overlay segments]: indices >=
    // num_cam_lss are axes/grid capsules with per-segment colors; the two
    // classes are gated independently by show_cams / show_overlay.
    const float* __restrict__ overlay_colors,  // [M, 3], overlay part only
    const int num_cam_lss,
    const bool show_cams,
    const bool show_overlay,
    const TensorView<uint8_t, 4> thumbnails,
    const float* __restrict__ min_max,
    TensorView<uint8_t, 3> out_rgbs
) {
    const int pix_x = blockIdx.x * blockDim.x + threadIdx.x;
    const int pix_y = blockIdx.y * blockDim.y + threadIdx.y;

    float3 render_rgb;
    if (render_rgbs.shape[2] == 1) {
        float amin = min_max[0], amax = min_max[1];
        float z = render_rgbs.load1(min(pix_y, image_height-1), min(pix_x, image_width-1));
        z = (z - amin) / (amax - amin);
        render_rgb = colormap_turbo(z);
    }
    else render_rgb = render_rgbs.load3(min(pix_y, image_height-1), min(pix_x, image_width-1));

    // decide whether to render camera border black or white based on background color, with noise reduction
    auto warp = cg::tiled_partition<WARP_SIZE>(cg::this_thread_block());
    static constexpr float BLOCK_DIM_X = 8;
    static constexpr float BLOCK_DIM_Y = 4;
    static constexpr float3 rgb2gray = float3{0.299f, 0.587f, 0.114f};
    // static constexpr float gray_threshold = 0.5f - 0.5f*dot(rgb2gray, normalize(rgb2gray));
    static constexpr float gray_threshold = 0.1657f;
    float gray_a = 0.0f, gray_b = 0.0f, gray_c = dot(render_rgb, rgb2gray);
    int num_gray = __popc(__ballot_sync(~0u, gray_c >= gray_threshold));
    // static constexpr int pad_gray = 2;
    static constexpr int pad_gray = 0;
    if (num_gray > pad_gray && num_gray < WARP_SIZE-pad_gray) {
        float x = (float)threadIdx.x * (1.0f / (float)BLOCK_DIM_X);
        float y = (float)threadIdx.y * (1.0f / (float)BLOCK_DIM_Y);
        #if 0
        // re-calibration needed when block size changes
        // inv([sxx sxy sx; sxy syy sy; sx sy s])
        float sxx = cg::reduce(warp, x*x, cg::plus<float>());  // TODO: closed form?
        float sxy = cg::reduce(warp, x*y, cg::plus<float>());  // TODO: closed form?
        float syy = cg::reduce(warp, y*y, cg::plus<float>());  // TODO: closed form?
        float sx = cg::reduce(warp, x, cg::plus<float>());  // TODO: closed form?
        float sy = cg::reduce(warp, y, cg::plus<float>());  // TODO: closed form?
        float s = WARP_SIZE;
        if (pix_x == 0 && pix_y == 0) printf("%f %f %f %f %f %f\n", sxx, sxy, syy, sx, sy, s);
        #endif
        float sxg = cg::reduce(warp, x*gray_c, cg::plus<float>());
        float syg = cg::reduce(warp, y*gray_c, cg::plus<float>());
        float sg = cg::reduce(warp, gray_c, cg::plus<float>());
        gray_a = 3.809523809523809e-01f * sxg - 1.666666666666667e-01f * sg;
        gray_b = 3.999999999999999e-01f * syg - 1.500000000000000e-01f * sg;
        gray_c = -1.666666666666667e-01f * sxg - 1.500000000000000e-01f * syg + 1.604166666666667e-01f * sg;
    }

    if (pix_x >= image_width || pix_y >= image_height)
        return;

    float depth = render_depths.load1(pix_y, pix_x);
    float opac = render_alphas.load1(pix_y, pix_x);
    depth /= fminf(fmaxf(opac, 1e-6f) / 0.5f, 1.0f);
    if (depth == 0.0f)
        depth = 1e10f;

    constexpr int MSAA = 2;
    float4 intrin = view_intrins[0];
    float fx = intrin.x, fy = intrin.y, cx = intrin.z, cy = intrin.w;
    float3x3 R = {  // row major
        view_viewmat[0], view_viewmat[1], view_viewmat[2],  // 1st row
        view_viewmat[4], view_viewmat[5], view_viewmat[6],  // 2nd row
        view_viewmat[8], view_viewmat[9], view_viewmat[10],  // 3rd row
    };
    float3 t = { view_viewmat[3], view_viewmat[7], view_viewmat[11] };
    auto dist_coeffs = view_dist_coeffs.load<distortion>(0);

    float alpha_final = 0.0f;
    float3 rgb_final = {0.0f, 0.0f, 0.0f};

    for (int i = 0; i < MSAA*MSAA; ++i) {
        const float px = (float)pix_x + (i/MSAA + 0.5f) / (float)MSAA;
        const float py = (float)pix_y + (i%MSAA + 0.5f) / (float)MSAA;
        float gray = gray_a * (px * (1.0f / (float)BLOCK_DIM_X) - blockIdx.x) +
            gray_b * (py * (1.0f / (float)BLOCK_DIM_Y) - blockIdx.y) + gray_c;
        float3 ray_o = SlangProjectionUtils::transform_ray_o(R, t);
        float3 ray_d;
        float2 uv = {(px-cx)/fx, (py-cy)/fy};
        bool inside = SlangDistortion<distortion>::generate_ray(
            uv,
            view_camera_model, dist_coeffs,
            &ray_d
        );
        if (view_camera_model == (int)CameraModelType::FISHEYE)
            inside &= length(uv) < (float)M_PI;
        if (view_camera_model == (int)CameraModelType::EQUISOLID)
            inside &= length(uv) < 2.0f;
        ray_d = SlangProjectionUtils::transform_ray_d(R, ray_d);
        if (!inside)
            continue;

        float3 rgb = {0.0f, 0.0f, 0.0f};
        float alpha = 0.0f;
        float tmax = depth;

        constexpr uint MAX_STACK_SIZE = 8*sizeof(int32_t)+1;
        __shared__ uint32_t stack[WARP_SIZE][MAX_STACK_SIZE];
        const int thread_rank = threadIdx.y * blockDim.x + threadIdx.x;

        // BVH traversal for linear swept spheres
        if (lss_buffer != nullptr) {
            uint stackSize = 0;
            float3 bmin = lss_aabb[0], bmax = lss_aabb[1];
            if (ray_aabb_intersection(
                0.5f*(bmin+bmax), 0.5f*(bmax-bmin), ray_o, ray_d, 0.0f, tmax
            ) > 0.0f) {
                stack[thread_rank][stackSize] = (uint32_t)0;
                stackSize++;
            }
            for (uint _num_steps = 0; _num_steps < 65536; _num_steps++) {
                if (stackSize == 0)
                    break;

                --stackSize;
                uint32_t elem = stack[thread_rank][stackSize];
                int2 node = lss_nodes[elem];

                for (uint ci = 0; ci < 2; ci++) {
                    int childIdx = ci == 0 ? node.x : node.y;
                    // leaf
                    if (childIdx < 0) {
                        int idx = ~childIdx;
                        bool overlay = idx >= num_cam_lss;
                        if (overlay ? !show_overlay : !show_cams)
                            continue;
                        float4 e0 = lss_buffer[2*idx], e1 = lss_buffer[2*idx+1];
                        float t_temp = ray_linear_swept_sphere_intersection(
                            {e0.x, e0.y, e0.z}, {e1.x, e1.y, e1.z}, e0.w, e1.w,
                            ray_o, ray_d, 0.0f, tmax
                        );
                        if (t_temp > 0.0f) {
                            tmax = t_temp;
                            if (overlay) {
                                const float* c = overlay_colors
                                                 + 3*(idx - num_cam_lss);
                                rgb = make_float3(c[0], c[1], c[2]);
                            } else {
                                rgb = make_float3(
                                    gray >= gray_threshold ? 0.1f : 0.85f);
                            }
                            alpha = 1.0f;
                        }
                    }
                    // node
                    else {
                        float3 bmin = lss_aabb[2*childIdx], bmax = lss_aabb[2*childIdx+1];
                        if (ray_aabb_intersection(
                            0.5f*(bmin+bmax), 0.5f*(bmax-bmin), ray_o, ray_d, 0.0f, tmax
                        ) > 0.0f && stackSize < MAX_STACK_SIZE) {
                            stack[thread_rank][stackSize] = (uint32_t)childIdx;
                            stackSize += 1;
                        }
                    }
                }
            }
        }

        // BVH traversal for triangle
        if (tri_buffer != nullptr) {
            uint stackSize = 0;
            float3 bmin = tri_aabb[0], bmax = tri_aabb[1];
            if (ray_aabb_intersection(
                0.5f*(bmin+bmax), 0.5f*(bmax-bmin), ray_o, ray_d, 0.0f, tmax
            ) > 0.0f) {
                stack[thread_rank][stackSize] = (uint32_t)0;
                stackSize++;
            }
            for (uint _num_steps = 0; _num_steps < 65536; _num_steps++) {
                if (stackSize == 0)
                    break;

                --stackSize;
                uint32_t elem = stack[thread_rank][stackSize];
                int2 node = tri_nodes[elem];

                for (uint ci = 0; ci < 2; ci++) {
                    int childIdx = ci == 0 ? node.x : node.y;
                    // leaf
                    if (childIdx < 0) {
                        float t_temp;
                        uint32_t cam_idx;
                        float2 uv;
                        if (ray_triangle_intersection(
                            tri_buffer, ~childIdx,
                            ray_o, ray_d, 0.0f, tmax,
                            t_temp, cam_idx, uv
                        )) {
                            tmax = t_temp;
                            rgb = get_thumbnail_bilinear(thumbnails, cam_idx, uv);
                            alpha = 1.0f;
                        }
                    }
                    // node
                    else {
                        float3 bmin = tri_aabb[2*childIdx], bmax = tri_aabb[2*childIdx+1];
                        if (ray_aabb_intersection(
                            0.5f*(bmin+bmax), 0.5f*(bmax-bmin), ray_o, ray_d, 0.0f, tmax
                        ) > 0.0f && stackSize < MAX_STACK_SIZE) {
                            stack[thread_rank][stackSize] = (uint32_t)childIdx;
                            stackSize += 1;
                        }
                    }
                }
            }
        }

        float a = alpha / (float)(MSAA*MSAA);
        alpha_final += a;
        rgb_final += rgb * a;
    }

    if (alpha_final > 0.0f)
        render_rgb = render_rgb * (1.0f - alpha_final) + rgb_final;
    render_rgb = 255.0f * fmin(fmax(render_rgb, make_float3(0.0f)), make_float3(1.0f)) + 0.5f;
    out_rgbs.store3(pix_y, pix_x, {(uint8_t)render_rgb.x, (uint8_t)render_rgb.y, (uint8_t)render_rgb.z});
}

// TorchTensorView -> TensorView helper (contiguous layout)
template<typename T, int ndim>
static TensorView<T, ndim> tv_to_view(const TorchTensorView& tv) {
    TensorView<T, ndim> view;
    view.data = (T*)std::get<0>(tv);
    auto& shape = std::get<2>(tv);
    long stride = 1;
    for (int i = ndim - 1; i >= 0; i--) {
        view.shape[i] = (i < (int)shape.size()) ? (long)shape[i] : 1L;
        view.strides[i] = stride;
        stride *= view.shape[i];
    }
    return view;
}

__global__ void fill_identity_kernel(int32_t* __restrict__ data, uint32_t n) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) data[idx] = (int32_t)idx;
}

struct BvhResult {
    int2* nodes;
    float3* aabb;
};

template<VisPrimitive primitive>
inline BvhResult build_bvh(
    uint32_t num_elem,
    const float4* buffer,
    float3 rootAABBMin,
    float3 rootAABBMax,
    const std::string& key_prefix
) {
    auto& pool = DevicePool::global();
    // BVH-build scratch keys are per-primitive prefixes ("viewer.lss_bvh" /
    // "viewer.tri_bvh" / "vis.*"), assembled at runtime -> dynamic Viewer slots.
    const VramCategory kBvhCat = VramCategory::Viewer;

    uint64_t* morton = (uint64_t*)pool.acquire_dynamic(
        kBvhCat, key_prefix + ".morton", (size_t)num_elem * sizeof(uint64_t));
    fill_sorting_keys_kernel<primitive><<<_LAUNCH_ARGS_1D(num_elem, 256)>>>(
        num_elem, buffer, rootAABBMin, rootAABBMax, morton);
    CHECK_DEVICE_ERROR(cudaGetLastError());

    int32_t* argsort_in = (int32_t*)pool.acquire_dynamic(
        kBvhCat, key_prefix + ".argsort_in", (size_t)num_elem * sizeof(int32_t));
    fill_identity_kernel<<<_LAUNCH_ARGS_1D(num_elem, 256)>>>(argsort_in, num_elem);
    CHECK_DEVICE_ERROR(cudaGetLastError());

    uint64_t* sorted_morton = (uint64_t*)pool.acquire_dynamic(
        kBvhCat, key_prefix + ".sorted_morton", (size_t)num_elem * sizeof(uint64_t));
    int32_t* argsort = (int32_t*)pool.acquire_dynamic(
        kBvhCat, key_prefix + ".argsort", (size_t)num_elem * sizeof(int32_t));
    CUB_WRAPPER(cub::DeviceRadixSort::SortPairs,
        morton, sorted_morton, argsort_in, argsort, (int)num_elem);
    CHECK_DEVICE_ERROR(cudaGetLastError());

    int2* internal_nodes = (int2*)pool.acquire_dynamic(
        kBvhCat, key_prefix + ".nodes", (size_t)(num_elem - 1) * 2 * sizeof(int32_t));
    int32_t* parent_nodes = (int32_t*)pool.acquire_dynamic(
        kBvhCat, key_prefix + ".parents", (size_t)(num_elem - 1) * sizeof(int32_t));
    cudaMemset(parent_nodes, 0xff, (num_elem - 1) * sizeof(int32_t));
    fill_lbvh_internal_nodes_kernel<<<_LAUNCH_ARGS_1D(num_elem - 1, 256)>>>(
        num_elem, sorted_morton, argsort, internal_nodes, parent_nodes);
    CHECK_DEVICE_ERROR(cudaGetLastError());

    float3* treeAABB = (float3*)pool.acquire_dynamic(
        kBvhCat, key_prefix + ".aabb", (size_t)num_elem * 2 * sizeof(float3));
    fill_tree_init_aabb_kernel<<<_LAUNCH_ARGS_1D(num_elem - 1, 256)>>>(num_elem - 1, treeAABB);
    CHECK_DEVICE_ERROR(cudaGetLastError());
    compute_lbvh_aabb_kernel<primitive><<<_LAUNCH_ARGS_1D(num_elem - 1, 256)>>>(
        num_elem, buffer, internal_nodes, parent_nodes, treeAABB);
    CHECK_DEVICE_ERROR(cudaGetLastError());

    return {internal_nodes, treeAABB};
}

__global__ void compute_min_max_kernel(
    TensorView<float, 3> depths,
    float* __restrict__ min_max  // [2]
) {
    uint32_t xid = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t yid = blockIdx.y * blockDim.y + threadIdx.y;

    float amin = 1e38f, amax = -1e38f;
    if (xid < depths.shape[1] && yid < depths.shape[0]) {
        float z = depths.load1(yid, xid);
        if (isfinite(z))
            amin = fminf(amin, z), amax = fmaxf(amax, z);
    }

    auto block = cg::this_thread_block();
    cg::thread_block_tile<WARP_SIZE> warp = cg::tiled_partition<WARP_SIZE>(block);

    amin = cg::reduce(warp, amin, cg::less<float>());
    if (warp.thread_rank() == 0) atomicMin(&min_max[0], amin);

    amax = cg::reduce(warp, amax, cg::greater<float>());
    if (warp.thread_rank() == 0) atomicMax(&min_max[1], amax);
}


// ---------------------------------------------------------------------------
// engine_viewer_init: lazy one-shot upload of static dataset camera arrays +
// allocation of the device-side thumbnail cache. Resets BVH cache flag so
// the next show_training_cameras=true call rebuilds.
// ---------------------------------------------------------------------------
void engine_viewer_init(
    TorchTensorView camera_models,
    TorchTensorView intrins,
    TorchTensorView dist_coeffs,
    TorchTensorView distortions,
    TorchTensorView camera_to_worlds,
    TorchTensorView widths,
    TorchTensorView heights,
    float camera_size)
{
    std::lock_guard<std::mutex> _vlock(viewer_mutex());
    auto& v = engine().viewer;
    int64_t N = std::get<2>(intrins)[0];
    if (N <= 0)
        throw std::runtime_error("engine_viewer_init: N_post must be > 0");

    v.N_post      = (int)N;
    v.camera_size = camera_size;

    // Upload dataset-wide arrays (host or device input). _hv_to_dv copies
    // when host, zero-copies when device.
    v.d_intrins          = _hv_to_dv<float>(PoolSlot::ViewerIntrins,
                              TorchTensorView(std::get<0>(intrins), 4, {N * 4LL}));
    v.d_widths           = _hv_to_dv<int32_t>(PoolSlot::ViewerWidths,  widths);
    v.d_heights          = _hv_to_dv<int32_t>(PoolSlot::ViewerHeights, heights);
    v.d_camera_models    = _hv_to_dv<int32_t>(PoolSlot::ViewerCmodels, camera_models);
    v.d_dist_coeffs      = _hv_to_dv<float>(PoolSlot::ViewerDist,
                              TorchTensorView(std::get<0>(dist_coeffs), 4, {N * (int64_t)kCameraDistortionParams}));
    v.d_distortions      = _hv_to_dv<int32_t>(PoolSlot::ViewerDistTier, distortions);
    v.d_camera_to_worlds = _hv_to_dv<float>(PoolSlot::ViewerC2w,
                              TorchTensorView(std::get<0>(camera_to_worlds), 4, {N * 12LL}));

    // Allocate thumbnail cache (N * S * S * 4 bytes; zero-initialized so
    // unfilled cells render as transparent black).
    constexpr int S = VIEWER_THUMBNAIL_SIZE;
    v.thumbnails.resize(PoolSlot::ViewerThumb, (int64_t)N * S * S * 4);
    cudaMemset(v.thumbnails.data_ptr(), 0,
               (size_t)N * S * S * 4 * sizeof(uint8_t));
    v.thumbnail_done_mask.resize(PoolSlot::ViewerThumbDone, N);
    cudaMemset(v.thumbnail_done_mask.data_ptr(), 0, (size_t)N * sizeof(uint8_t));
    v.host_seen_mask.assign((size_t)N, 0);
    v.pending_thumb = (int)N;

    v.bvh_built  = false;
    v.initialized = true;
}

void engine_viewer_set_camera_size(float camera_size) {
    std::lock_guard<std::mutex> _vlock(viewer_mutex());
    auto& v = engine().viewer;
    if (!v.initialized) return;
    v.camera_size = camera_size;
    // engine_blit_view rebuilds the cached BVH when this differs from the
    // size it was built at (bvh_camera_size), so live slider changes apply.
}

// ---------------------------------------------------------------------------
// Axes/grid overlay. The grid lives in the ENGINE's own frame -- the frame
// splats are trained and SAVED in (usually the raw dataset frame; the
// standalone web viewer draws its grid in the same model-native frame) --
// so grid lines mark round coordinates of the exported model. Ground plane
// at z=0, minor cells at the power of 10 chosen from the current view
// distance (zoom-adaptive; re-derived per blit through engine_blit_view's
// grid_dist), brighter lines every 10th, positive X/Y/Z half-axes in the
// web viewer's colors. The finite line patch follows the nav target
// (snapped to the lattice, with hysteresis) so the grid stays under the
// viewer at any zoom; lines always sit at global multiples of the cell
// size and the axes stay anchored at the frame origin. Each cell edge is
// one capsule so lines curve correctly under fisheye/equirectangular view
// cameras.
// ---------------------------------------------------------------------------
static void _viewer_build_grid(float view_dist,      // viewer_mutex held
                               const float target[3])
{
    auto& v = engine().viewer;
    const float radius = v.grid_radius;
    const float s = powf(10.0f, floorf(log10f(fmaxf(view_dist, 1e-6f) / 2.0f)));
    // Cover the scene, but keep the segment count bounded when zoomed far
    // in; never fewer cells than the legacy fixed extent.
    int half = (int)ceilf(radius / s);
    half = half < 20 ? 20 : (half > 100 ? 100 : half);
    // Recenter only when the target drifts toward the patch edge, so slow
    // pans don't rebuild (and re-BVH) every frame.
    bool recenter = fabsf(target[0] - v.grid_center[0]) > 0.25f * half * s ||
                    fabsf(target[1] - v.grid_center[1]) > 0.25f * half * s;
    if (v.num_overlay > 0 && s == v.grid_spacing && half == v.grid_half &&
        !recenter)
        return;
    const long gx0 = lroundf(target[0] / s), gy0 = lroundf(target[1] / s);
    const float cx = gx0 * s, cy = gy0 * s;

    const float rr = 0.004f * s;            // capsule radius (axes 2x)
    const float kMinor[3] = {0.28f, 0.30f, 0.33f};
    const float kMajor[3] = {0.45f, 0.47f, 0.50f};
    const float kAxis[3][3] = {{0.98f, 0.20f, 0.31f},       // +X
                               {0.55f, 0.86f, 0.00f},       // +Y
                               {0.16f, 0.55f, 0.98f}};      // +Z

    std::vector<float> segs, colors;
    segs.reserve(((size_t)(4*half + 2) * 2*half + 3*half) * 8);
    colors.reserve(segs.capacity() * 3 / 8);
    auto seg = [&](float ax, float ay, float az, float bx, float by, float bz,
                   float r, const float c[3]) {
        segs.insert(segs.end(), {ax, ay, az, r});
        segs.insert(segs.end(), {bx, by, bz, r});
        colors.insert(colors.end(), {c[0], c[1], c[2]});
    };
    for (int i = -half; i <= half; i++) {
        // Major/minor from the GLOBAL line index, not the patch-local one.
        const float* cX = ((gx0 + i) % 10 == 0) ? kMajor : kMinor;
        const float* cY = ((gy0 + i) % 10 == 0) ? kMajor : kMinor;
        for (int j = -half; j < half; j++) {
            seg(cx + i*s, cy + j*s, 0, cx + i*s, cy + (j+1)*s, 0, rr, cX);
            seg(cx + j*s, cy + i*s, 0, cx + (j+1)*s, cy + i*s, 0, rr, cY);
        }
    }
    for (int a = 0; a < 3; a++)                              // half-axes
        for (int j = 0; j < half; j++) {
            float p0[3] = {0, 0, 0}, p1[3] = {0, 0, 0};
            p0[a] = j*s;
            p1[a] = (j+1)*s;
            seg(p0[0], p0[1], p0[2], p1[0], p1[1], p1[2], 2.0f*rr, kAxis[a]);
        }

    int64_t M = (int64_t)colors.size() / 3;
    v.d_overlay_segs = _hv_to_dv<float>(PoolSlot::ViewerOverlaySegs,
        TorchTensorView((uint64_t)segs.data(), 4, {M * 8}));
    v.d_overlay_colors = _hv_to_dv<float>(PoolSlot::ViewerOverlayColors,
        TorchTensorView((uint64_t)colors.data(), 4, {M * 3}));
    v.num_overlay = (int)M;
    v.grid_spacing = s;
    v.grid_half = half;
    v.grid_center[0] = cx;
    v.grid_center[1] = cy;
    v.bvh_built = false;   // rebuilt (with the overlay appended) on next use
}

// radius = scene radius in the engine frame (grid extent); view_distance =
// initial nav distance in the engine frame for the spacing decade (<= 0
// falls back to radius). Later zoom / recenter updates come in via
// engine_blit_view's grid_dist / grid_target.
void engine_viewer_set_grid(float radius, float view_distance)
{
    std::lock_guard<std::mutex> _vlock(viewer_mutex());
    auto& v = engine().viewer;
    if (!v.initialized) return;
    v.grid_radius = fmaxf(radius, 1e-6f);
    v.grid_spacing = 0.0f;   // force a rebuild
    const float origin[3] = {0, 0, 0};
    _viewer_build_grid(view_distance > 0.0f ? view_distance : v.grid_radius,
                       origin);
}


// ---------------------------------------------------------------------------
// Thumbnail update: gather a downscaled RGBA per post-camera from the
// already-byte->float-warped GT buffer at engine().gt.rgb. Called by
// _engine_train_step_after_setup right after _set_cur_cam_indices uploads
// the per-batch post-camera ids and engine().gt.rgb is populated. Cheap when
// all slots are full (host counter short-circuits before any kernel launch).
// ---------------------------------------------------------------------------

__global__ void update_thumbnails_kernel(
    const void* __restrict__ rgb,            // [B_post, H_rgb, W_rgb, 3]
    PixelFormat rgb_fmt,
    int H_rgb, int W_rgb,
    const int32_t* __restrict__ cam_indices, // [B_post] device-side
    int B_post, int N, int S,
    uint8_t* __restrict__ thumbnails,        // [N, S, S, 4]
    uint8_t* __restrict__ done_mask,         // [N]
    const uint8_t* __restrict__ alpha_mask,  // [B_post, H_alpha, W_alpha] bool or nullptr
    int H_alpha, int W_alpha
) {
    int b = blockIdx.x;
    if (b >= B_post) return;
    int post_idx = cam_indices[b];
    if (post_idx < 0 || post_idx >= N) return;
    if (done_mask[post_idx] != 0) return;  // already filled; skip

    int tid = threadIdx.x;
    int s_total = S * S;
    uint8_t* dst = thumbnails + (size_t)post_idx * S * S * 4;

    for (int p = tid; p < s_total; p += blockDim.x) {
        int sy = p / S;
        int sx = p % S;

        // Bilinear RGB tap at the source resolution (matches per-pixel loss's
        // bilinear_sample_f3 convention; works when the GT RGB shape differs
        // from S x S).
        int x0, y0, x1, y1; float w00, w01, w10, w11;
        _bilinear_detail::resolve_taps(sx, sy, S, S, W_rgb, H_rgb,
                                       x0, y0, x1, y1, w00, w01, w10, w11);
        const size_t plane = (size_t)b * H_rgb * W_rgb;
        const float3 v00 = pixel_load3(rgb, rgb_fmt, plane + (size_t)y0 * W_rgb + x0);
        const float3 v10 = pixel_load3(rgb, rgb_fmt, plane + (size_t)y0 * W_rgb + x1);
        const float3 v01 = pixel_load3(rgb, rgb_fmt, plane + (size_t)y1 * W_rgb + x0);
        const float3 v11 = pixel_load3(rgb, rgb_fmt, plane + (size_t)y1 * W_rgb + x1);
        float3 c;
        c.x = w00*v00.x + w10*v10.x + w01*v01.x + w11*v11.x;
        c.y = w00*v00.y + w10*v10.y + w01*v01.y + w11*v11.y;
        c.z = w00*v00.z + w10*v10.z + w01*v01.z + w11*v11.z;

        // Mask: nearest-neighbor sample at the mask's own resolution. Out-of-
        // mask pixels render as mid-gray so masked borders are visually
        // distinct from black image content.
        if (alpha_mask != nullptr) {
            bool inside = nearest_sample_mask(alpha_mask, b, sx, sy, S, S, W_alpha, H_alpha) == 1;
            if (!inside) c = make_float3(0.5f, 0.5f, 0.5f);
        }

        uint8_t* o = dst + ((size_t)sy * S + sx) * 4;
        o[0] = (uint8_t)fminf(fmaxf(255.0f * c.x + 0.5f, 0.0f), 255.0f);
        o[1] = (uint8_t)fminf(fmaxf(255.0f * c.y + 0.5f, 0.0f), 255.0f);
        o[2] = (uint8_t)fminf(fmaxf(255.0f * c.z + 0.5f, 0.0f), 255.0f);
        o[3] = 255;
    }
    __syncthreads();
    if (tid == 0) done_mask[post_idx] = 1;
}


// Called from EngineTrainStep.cpp after _set_cur_cam_indices but before the
// forward pass. cam_indices_tv is the same host or device int32 tensor used
// by bilagrid (post-split layout).
void engine_viewer_capture_thumbnails(TorchTensorView cam_indices_tv) {
    // Fast path: skip the lock when nothing to do. v.initialized and
    // v.pending_thumb are written under the lock by engine_viewer_init /
    // this function. Reading them lockless is OK as a hint -- a stale
    // "not initialized" miss just delays the first capture by one step, and
    // pending_thumb only decreases monotonically once init has happened.
    if (!engine().viewer.initialized || engine().viewer.pending_thumb <= 0) return;
    if (!engine().gt.has_gt) return;
    const TorchTensorView& gt_rgb = engine().gt.rgb;
    if (std::get<0>(gt_rgb) == 0) return;

    std::lock_guard<std::mutex> _vlock(viewer_mutex());
    auto& v = engine().viewer;
    // Re-check after acquiring the lock (init may have run; pending may have
    // hit zero on another iter even before init).
    if (!v.initialized || v.pending_thumb <= 0) return;

    const auto& gt_shape = std::get<2>(gt_rgb);
    int64_t B_post = gt_shape[0];
    int64_t H = gt_shape[1];
    int64_t W = gt_shape[2];
    if (B_post <= 0 || H <= 0 || W <= 0) return;

    uint64_t ci_ptr = std::get<0>(cam_indices_tv);
    if (ci_ptr == 0) return;

    // Read cam_indices into host so we can update host_seen_mask + counter.
    std::vector<int32_t> host_ci((size_t)B_post);
    bool ci_on_device = _is_device_ptr((void*)ci_ptr);
    if (ci_on_device) {
        cudaMemcpy(host_ci.data(), (const void*)ci_ptr,
                   (size_t)B_post * sizeof(int32_t),
                   cudaMemcpyDeviceToHost);
        CHECK_DEVICE_ERROR(cudaGetLastError());
    } else {
        std::memcpy(host_ci.data(), (const void*)ci_ptr,
                    (size_t)B_post * sizeof(int32_t));
    }

    int newly = 0;
    for (int j = 0; j < B_post; ++j) {
        int idx = host_ci[j];
        if (idx >= 0 && idx < v.N_post && !v.host_seen_mask[(size_t)idx]) {
            v.host_seen_mask[(size_t)idx] = 1;
            ++newly;
        }
    }
    if (newly == 0) return;  // every id this batch is already cached
    v.pending_thumb -= newly;

    // Kernel reads cam_indices from device; use the engine's pre-uploaded
    // bilagrid_cur_cam_indices buffer when available (already populated by
    // _set_cur_cam_indices). Fall back to a one-shot H2D copy otherwise.
    const int32_t* d_ci = engine().bilagrid_cur_cam_indices.data_ptr();
    int32_t* tmp_ci = nullptr;
    if (d_ci == nullptr) {
        tmp_ci = DevicePool::global().acquire<int32_t>(
            PoolSlot::ViewerCapIdx, (size_t)B_post);
        cudaMemcpy(tmp_ci, host_ci.data(),
                   (size_t)B_post * sizeof(int32_t),
                   cudaMemcpyHostToDevice);
        d_ci = tmp_ci;
        CHECK_DEVICE_ERROR(cudaGetLastError());
    }

    // Optional GT mask -> mask-aware thumbnail (masked regions become gray).
    // The mask resolution can differ from gt.rgb (e.g. pit_renovation: RGB
    // 1920x1920, mask 1600x1600) so we pass its own H/W and use a separate
    // nearest sampler in the kernel.
    const uint8_t* d_alpha_mask = nullptr;
    int H_alpha = 0, W_alpha = 0;
    if (engine().gt.has_mask && engine().gt.alpha.data_ptr() != nullptr
        && engine().gt.alpha.size<0>() == B_post) {
        d_alpha_mask = engine().gt.alpha.data_ptr();
        H_alpha = (int)engine().gt.alpha.size<1>();
        W_alpha = (int)engine().gt.alpha.size<2>();
    }

    CHECK_DEVICE_ERROR(cudaGetLastError());
    // Launch on the high-priority viewer stream so this read of gt.rgb does
    // not serialize behind subsequent training kernels queued on the default
    // stream. gt.rgb was written by the default stream earlier in this same
    // training step, so the viewer stream must wait on it first.
    viewer_stream_wait_default();
    update_thumbnails_kernel<<<(uint32_t)B_post, 128, 0, viewer_stream()>>>(
        (const void*)std::get<0>(gt_rgb), pixel_format(gt_rgb),
        (int)H, (int)W,
        d_ci, (int)B_post, v.N_post, VIEWER_THUMBNAIL_SIZE,
        v.thumbnails.data_ptr(),
        v.thumbnail_done_mask.data_ptr(),
        d_alpha_mask, H_alpha, W_alpha
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}


// ---------------------------------------------------------------------------
// engine_blit_view: stateful replacement for blit_train_cameras_tensor.
// Caches the BVH (built from engine().viewer's cached dataset arrays) on
// the first show_training_cameras=true call, reads thumbnails from the
// engine's device-side cache, and writes annotated [H,W,3] uint8 output.
// ---------------------------------------------------------------------------

namespace {
// Build the BVH into engine().viewer.bvh_* pool slots. Runs the same kernel
// dance as blit_train_cameras_tensor's show-cams branch but emits to
// dedicated "viewer.*" keys so the buffers survive across calls.
void _viewer_build_bvh()
{
    auto& v = engine().viewer;
    int64_t n = v.N_post;
    constexpr int kFloatPInfByte = 0x7f;
    constexpr int kFloatNInfByte = 0xfe;

    uint32_t num_cam_lss = (uint32_t)(n * 8 * kNumFrustumSegments);
    uint32_t num_lss = num_cam_lss + (uint32_t)v.num_overlay;
    uint32_t num_tri = (uint32_t)(n * 4 * kNumFrustumFaces * kNumFrustumFaces);

    float4* lss_buffer = DevicePool::global().acquire<float4>(PoolSlot::ViewerLss, (size_t)num_lss * 2);
    float4* tri_buffer = DevicePool::global().acquire<float4>(PoolSlot::ViewerTri, (size_t)num_tri * 4);

    TorchTensorView dist_tv((uint64_t)v.d_dist_coeffs.data_ptr(), 4,
                            {(int64_t)n, (int64_t)kCameraDistortionParams});
    CameraDistortionCoeffsBuffer dist_buf(dist_tv);

    fill_frustum_segments_kernel
    <<<_LAUNCH_ARGS_1D(n * 4 * kNumFrustumSegments, 4 * kNumFrustumSegments)>>>(
        (const float4*)v.d_intrins.data_ptr(),
        v.d_widths.data_ptr(),
        v.d_heights.data_ptr(),
        v.d_camera_models.data_ptr(),
        dist_buf,
        v.d_distortions.data_ptr(),
        v.d_camera_to_worlds.data_ptr(),
        v.camera_size,
        lss_buffer,
        tri_buffer
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());

    // Overlay (axes/grid) capsules appended after the frusta; the blit
    // kernel tells the two classes apart by segment index (bvh_num_cam_lss).
    if (v.num_overlay > 0)
        cudaMemcpy(lss_buffer + (size_t)num_cam_lss * 2,
                   v.d_overlay_segs.data_ptr(),
                   (size_t)v.num_overlay * 2 * sizeof(float4),
                   cudaMemcpyDeviceToDevice);

    float3* root_aabb = DevicePool::global().acquire<float3>(PoolSlot::ViewerRootAabb, 2);
    cudaMemset(root_aabb + 0, kFloatPInfByte, sizeof(float3));
    cudaMemset(root_aabb + 1, kFloatNInfByte, sizeof(float3));
    compute_aabb_kernel<VisPrimitive::LinearSweptSphere>
    <<<_LAUNCH_ARGS_1D(num_lss, 256)>>>(num_lss, lss_buffer, root_aabb);
    CHECK_DEVICE_ERROR(cudaGetLastError());
    compute_aabb_kernel<VisPrimitive::Triangle>
    <<<_LAUNCH_ARGS_1D(num_tri, 256)>>>(num_tri, tri_buffer, root_aabb);
    CHECK_DEVICE_ERROR(cudaGetLastError());

    float3 rootAABBMin, rootAABBMax;
    {
        float h_aabb[6];
        cudaMemcpy(h_aabb, root_aabb, 6 * sizeof(float), cudaMemcpyDeviceToHost);
        float cx = 0.5f * (h_aabb[3] + h_aabb[0]);
        float cy = 0.5f * (h_aabb[4] + h_aabb[1]);
        float cz = 0.5f * (h_aabb[5] + h_aabb[2]);
        float ex = 0.5f * (h_aabb[3] - h_aabb[0]);
        float ey = 0.5f * (h_aabb[4] - h_aabb[1]);
        float ez = 0.5f * (h_aabb[5] - h_aabb[2]);
        float ms = 1.01f * fmaxf(fmaxf(ex, ey), ez);
        rootAABBMin = {cx - ms, cy - ms, cz - ms};
        rootAABBMax = {cx + ms, cy + ms, cz + ms};
    }

    // Reuse build_bvh<>; output (nodes / aabb) lands in pool slots keyed by
    // the prefix, so subsequent calls with the same key reuse the memory.
    auto lss_bvh = build_bvh<VisPrimitive::LinearSweptSphere>(
        num_lss, lss_buffer, rootAABBMin, rootAABBMax, "viewer.lss_bvh");
    auto tri_bvh = build_bvh<VisPrimitive::Triangle>(
        num_tri, tri_buffer, rootAABBMin, rootAABBMax, "viewer.tri_bvh");
    // Keep the exact pointers build_bvh() allocated (string-keyed dynamic pool);
    // engine_blit_view() reads these directly instead of re-acquiring by a
    // different static PoolSlot enum (which would be a separate empty buffer).
    v.bvh_lss_nodes_ptr = lss_bvh.nodes;
    v.bvh_lss_aabb_ptr  = lss_bvh.aabb;
    v.bvh_tri_nodes_ptr = tri_bvh.nodes;
    v.bvh_tri_aabb_ptr  = tri_bvh.aabb;

    v.bvh_num_lss = (int)num_lss;
    v.bvh_num_tri = (int)num_tri;
    v.bvh_num_cam_lss = (int)num_cam_lss;
    v.bvh_camera_size = v.camera_size;
    v.bvh_built = true;
}
} // anonymous namespace


/*[AutoHeaderGeneratorExport]*/
void engine_blit_view(
    std::string     buffer_key,
    TorchTensorView render_buffer,
    TorchTensorView render_depth,
    TorchTensorView render_alpha,
    int             view_camera_model,
    std::string     distortion,
    TorchTensorView view_intrins,
    TorchTensorView view_viewmat,
    TorchTensorView view_dist_coeffs,
    bool            show_training_cameras,
    bool            show_overlay,
    float           grid_dist,
    float           grid_target_x,
    float           grid_target_y,
    float           grid_target_z,
    TorchTensorView out_rgb)
{
    std::lock_guard<std::mutex> _vlock(viewer_mutex());
    auto& v = engine().viewer;
    if (!v.initialized) {
        throw std::runtime_error(
            "engine_blit_view: viewer not initialized; call engine_viewer_init() first.");
    }
    // Zoom-adaptive grid: re-derive the cell decade from the client's nav
    // distance and recenter the patch on its orbit target (engine-frame
    // units). No-op unless the decade changed or the target drifted.
    if (show_overlay && grid_dist > 0.0f && v.grid_radius > 0.0f) {
        const float target[3] = {grid_target_x, grid_target_y, grid_target_z};
        _viewer_build_grid(grid_dist, target);
    }
    show_overlay = show_overlay && v.num_overlay > 0;

    auto& rgb_shape = std::get<2>(render_buffer);
    int64_t h = rgb_shape[0], w = rgb_shape[1], c = rgb_shape[2];
    (void)buffer_key;  // shape (c==1 vs 3) already distinguishes colormap vs passthrough

    constexpr int kFloatPInfByte = 0x7f;
    constexpr int kFloatNInfByte = 0xfe;

    // Viewer kernels run on a high-priority stream so they can preempt
    // training kernels queued on the default stream. render_buffer /
    // render_depth / render_alpha were written by the default stream (inside
    // model.get_outputs), so the viewer stream must wait on them first.
    viewer_stream_wait_default();

    float* min_max = nullptr;
    if (c == 1) {
        min_max = DevicePool::global().acquire<float>(PoolSlot::ViewerMinMax, 2);
        cudaMemsetAsync(min_max + 0, kFloatPInfByte, sizeof(float), viewer_stream());
        cudaMemsetAsync(min_max + 1, kFloatNInfByte, sizeof(float), viewer_stream());
        compute_min_max_kernel<<<_LAUNCH_ARGS_2D_VS(w, h, 16, 16)>>>(
            tv_to_view<float, 3>(render_buffer), min_max);
        CHECK_DEVICE_ERROR(cudaGetLastError());
    }

    // Thumbnails view from engine-cached buffer.
    TensorView<uint8_t, 4> thumb_view;
    thumb_view.data = v.thumbnails.data_ptr();
    thumb_view.shape[0] = (long)v.N_post;
    thumb_view.shape[1] = (long)VIEWER_THUMBNAIL_SIZE;
    thumb_view.shape[2] = (long)VIEWER_THUMBNAIL_SIZE;
    thumb_view.shape[3] = 4L;
    thumb_view.strides[3] = 1L;
    thumb_view.strides[2] = 4L;
    thumb_view.strides[1] = (long)(VIEWER_THUMBNAIL_SIZE * 4);
    thumb_view.strides[0] = (long)(VIEWER_THUMBNAIL_SIZE * VIEWER_THUMBNAIL_SIZE * 4);

    const float4* lss_buffer = nullptr;
    const int2*   lss_nodes  = nullptr;
    const float3* lss_aabb   = nullptr;
    const float4* tri_buffer = nullptr;
    const int2*   tri_nodes  = nullptr;
    const float3* tri_aabb   = nullptr;

    if (show_training_cameras || show_overlay) {
        // Rebuild when never built, or when the live camera size changed
        // since the cached build (the frustum-size slider), or when a new
        // overlay was uploaded (engine_viewer_set_grid clears bvh_built).
        if (!v.bvh_built || v.bvh_camera_size != v.camera_size) {
            // _viewer_build_bvh launches its kernels on the default stream
            // (see build_bvh<>'s _LAUNCH_ARGS_1D usage and the synchronous
            // cudaMemcpy of root_aabb inside). After it returns, the BVH
            // buffers are populated relative to the default stream, so the
            // viewer stream must wait on the default stream once more before
            // the blit kernel reads them.
            _viewer_build_bvh();
            viewer_stream_wait_default();
        }
        lss_buffer = (const float4*)DevicePool::global().acquire<float4>(
            PoolSlot::ViewerLss, (size_t)v.bvh_num_lss * 2);
        // Use the exact BVH allocations build_bvh() produced (see
        // _viewer_build_bvh); re-acquiring by PoolSlot enum here would read a
        // different, empty static buffer and draw no frustums (the 7632e51 bug).
        lss_nodes  = (const int2*)  v.bvh_lss_nodes_ptr;
        lss_aabb   = (const float3*)v.bvh_lss_aabb_ptr;
        if (show_training_cameras) {   // thumbnails only with the frusta
            tri_buffer = (const float4*)DevicePool::global().acquire<float4>(
                PoolSlot::ViewerTri, (size_t)v.bvh_num_tri * 4);
            tri_nodes  = (const int2*)  v.bvh_tri_nodes_ptr;
            tri_aabb   = (const float3*)v.bvh_tri_aabb_ptr;
        }
    }

    #define LAUNCH(D) \
        blit_with_bvh_kernel<D><<<_LAUNCH_ARGS_2D_VS(w, h, 8, 4)>>>( \
            tv_to_view<float, 3>(render_buffer), \
            tv_to_view<float, 3>(render_depth), \
            tv_to_view<float, 3>(render_alpha), \
            view_camera_model, (int)w, (int)h, \
            (float4*)std::get<0>(view_intrins), \
            (float*)std::get<0>(view_viewmat), \
            view_dist_coeffs, \
            lss_buffer, lss_nodes, lss_aabb, \
            tri_buffer, tri_nodes, tri_aabb, \
            (const float*)v.d_overlay_colors.data_ptr(), \
            v.bvh_num_cam_lss, \
            show_training_cameras, \
            show_overlay, \
            thumb_view, \
            min_max, \
            tv_to_view<uint8_t, 3>(out_rgb))
    _SS_DISPATCH_DISTORTION(distortion, LAUNCH);
    #undef LAUNCH
    CHECK_DEVICE_ERROR(cudaGetLastError());

    // Ensure the default stream (which the Python caller will use to read
    // out_rgb via .cpu()) waits for the viewer stream's blit kernel to finish.
    default_stream_wait_viewer();
}


/*[AutoHeaderGeneratorExport]*/
void blit_train_cameras_tensor(
    TorchTensorView render_rgbs,      // [H, W, C] float32
    TorchTensorView render_depths,    // [H, W, 1] float32
    TorchTensorView render_alphas,    // [H, W, 1] float32
    const int view_camera_model,
    std::string distortion,
    TorchTensorView view_intrins,     // [1, 4] or [4] float32
    TorchTensorView view_viewmat,     // [4, 4] float32
    TorchTensorView view_dist_coeffs,
    TorchTensorView intrins,          // [N, 4] float32
    TorchTensorView widths,           // [N] int32
    TorchTensorView heights,          // [N] int32
    TorchTensorView camera_models,    // [N] int32
    TorchTensorView dist_coeffs,
    TorchTensorView distortions,      // [N] int32 CameraDistortionType
    TorchTensorView camera_to_worlds, // [N, 3, 4] float32
    TorchTensorView thumbnails,       // [B, H, W, 4] uint8
    float camera_size,
    bool show_training_cameras,
    TorchTensorView out_rgb           // [H, W, 3] uint8, pre-allocated
) {
    auto& rgb_shape = std::get<2>(render_rgbs);
    int64_t h = rgb_shape[0], w = rgb_shape[1], c = rgb_shape[2];
    int64_t n = std::get<2>(intrins)[0];

    constexpr int kFloatPInfByte = 0x7f;
    constexpr int kFloatNInfByte = 0xfe;

    float* min_max = nullptr;
    if (c == 1) {
        min_max = DevicePool::global().acquire<float>(PoolSlot::VisMinMax, 2);
        cudaMemset(min_max + 0, kFloatPInfByte, sizeof(float));
        cudaMemset(min_max + 1, kFloatNInfByte, sizeof(float));
        compute_min_max_kernel<<<_LAUNCH_ARGS_2D(w, h, 16, 16)>>>(
            tv_to_view<float, 3>(render_rgbs), min_max);
        CHECK_DEVICE_ERROR(cudaGetLastError());
    }

    if (!show_training_cameras) {
        #define LAUNCH(D) \
            blit_with_bvh_kernel<D><<<_LAUNCH_ARGS_2D(w, h, 8, 4)>>>( \
                tv_to_view<float, 3>(render_rgbs), \
                tv_to_view<float, 3>(render_depths), \
                tv_to_view<float, 3>(render_alphas), \
                view_camera_model, w, h, \
                (float4*)std::get<0>(view_intrins), \
                (float*)std::get<0>(view_viewmat), \
                view_dist_coeffs, \
                nullptr, nullptr, nullptr, \
                nullptr, nullptr, nullptr, \
                nullptr, 0, false, false, \
                tv_to_view<uint8_t, 4>(thumbnails), \
                min_max, \
                tv_to_view<uint8_t, 3>(out_rgb))
        _SS_DISPATCH_DISTORTION(distortion, LAUNCH);
        #undef LAUNCH
        CHECK_DEVICE_ERROR(cudaGetLastError());
        return;
    }

    uint32_t num_lss = (uint32_t)(n * 8 * kNumFrustumSegments);
    uint32_t num_tri = (uint32_t)(n * 4 * kNumFrustumFaces * kNumFrustumFaces);
    float4* lss_buffer = DevicePool::global().acquire<float4>(PoolSlot::VisLss, (size_t)num_lss * 2);
    float4* tri_buffer = DevicePool::global().acquire<float4>(PoolSlot::VisTri, (size_t)num_tri * 4);
    fill_frustum_segments_kernel
    <<<_LAUNCH_ARGS_1D(n * 4 * kNumFrustumSegments, 4 * kNumFrustumSegments)>>>(
        (float4*)std::get<0>(intrins),
        (int32_t*)std::get<0>(widths),
        (int32_t*)std::get<0>(heights),
        (int32_t*)std::get<0>(camera_models),
        dist_coeffs,
        (int32_t*)std::get<0>(distortions),
        (float*)std::get<0>(camera_to_worlds),
        camera_size,
        lss_buffer,
        tri_buffer);
    CHECK_DEVICE_ERROR(cudaGetLastError());

    float3* root_aabb = DevicePool::global().acquire<float3>(PoolSlot::VisRootAabb, 2);
    cudaMemset(root_aabb + 0, kFloatPInfByte, sizeof(float3));
    cudaMemset(root_aabb + 1, kFloatNInfByte, sizeof(float3));
    compute_aabb_kernel<VisPrimitive::LinearSweptSphere>
    <<<_LAUNCH_ARGS_1D(num_lss, 256)>>>(num_lss, lss_buffer, root_aabb);
    CHECK_DEVICE_ERROR(cudaGetLastError());
    compute_aabb_kernel<VisPrimitive::Triangle>
    <<<_LAUNCH_ARGS_1D(num_tri, 256)>>>(num_tri, tri_buffer, root_aabb);
    CHECK_DEVICE_ERROR(cudaGetLastError());

    float3 rootAABBMin, rootAABBMax;
    {
        float h_aabb[6];
        cudaMemcpy(h_aabb, root_aabb, 6 * sizeof(float), cudaMemcpyDeviceToHost);
        float cx = 0.5f * (h_aabb[3] + h_aabb[0]);
        float cy = 0.5f * (h_aabb[4] + h_aabb[1]);
        float cz = 0.5f * (h_aabb[5] + h_aabb[2]);
        float ex = 0.5f * (h_aabb[3] - h_aabb[0]);
        float ey = 0.5f * (h_aabb[4] - h_aabb[1]);
        float ez = 0.5f * (h_aabb[5] - h_aabb[2]);
        float ms = 1.01f * fmaxf(fmaxf(ex, ey), ez);
        rootAABBMin = {cx - ms, cy - ms, cz - ms};
        rootAABBMax = {cx + ms, cy + ms, cz + ms};
    }

    auto lss_bvh = build_bvh<VisPrimitive::LinearSweptSphere>(
        num_lss, lss_buffer, rootAABBMin, rootAABBMax, "vis.lss_bvh");
    auto tri_bvh = build_bvh<VisPrimitive::Triangle>(
        num_tri, tri_buffer, rootAABBMin, rootAABBMax, "vis.tri_bvh");

    #define LAUNCH(D) \
        blit_with_bvh_kernel<D><<<_LAUNCH_ARGS_2D(w, h, 8, 4)>>>( \
            tv_to_view<float, 3>(render_rgbs), \
            tv_to_view<float, 3>(render_depths), \
            tv_to_view<float, 3>(render_alphas), \
            view_camera_model, w, h, \
            (float4*)std::get<0>(view_intrins), \
            (float*)std::get<0>(view_viewmat), \
            view_dist_coeffs, \
            lss_buffer, \
            lss_bvh.nodes, \
            lss_bvh.aabb, \
            tri_buffer, \
            tri_bvh.nodes, \
            tri_bvh.aabb, \
            nullptr, (int)num_lss, true, false,   /* no overlay in the legacy path */ \
            tv_to_view<uint8_t, 4>(thumbnails), \
            min_max, \
            tv_to_view<uint8_t, 3>(out_rgb))
    _SS_DISPATCH_DISTORTION(distortion, LAUNCH);
    #undef LAUNCH
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

#include "kernels/loss/PerPixelLoss.cuh"
#include "kernels/loss/FusedSSIM.cuh"
#include "core/Interpolation.cuh"
#include "kernels/densify/Densify.cuh"  // canny_edge_filter_tensor

#include <cooperative_groups.h>
#include <cooperative_groups/reduce.h>
namespace cg = cooperative_groups;

#include "generated/slang.cuh"
namespace SlangPerPixelLosses {
#include "generated/set_namespace.cuh"
#include "generated/per_pixel_losses.cuh"
}

#include "core/Common.cuh"
#include <core/Tensor.h>

template<typename T>
static inline TensorView<T, 4> _make_view4(T* data, long B, long H, long W, long C) {
    TensorView<T, 4> v;
    v.data = data;
    v.shape[0] = B; v.shape[1] = H; v.shape[2] = W; v.shape[3] = C;
    v.strides[0] = H*W*C; v.strides[1] = W*C; v.strides[2] = C; v.strides[3] = 1;
    return v;
}

static inline float* _fptr(const TorchTensorView& tv) { return (float*)std::get<0>(tv); }
static inline float3* _f3ptr(const TorchTensorView& tv) { return (float3*)std::get<0>(tv); }
static inline bool* _bptr(const TorchTensorView& tv) { return (bool*)std::get<0>(tv); }
static inline int64_t* _i64ptr(const TorchTensorView& tv) { return (int64_t*)std::get<0>(tv); }
static inline bool _has(const TorchTensorView& tv) { return std::get<0>(tv) != 0; }

// Per-scale/per-buffer loss scratch. Keys are built at runtime from a pyramid
// scale index and buffer name ("ppl.s{n}.{name}", "ppl.g.{name}.s{n}") so they
// have no compile-time PoolSlot -> dynamic Image scratch (never checkpointed).
static inline TorchTensorView _pool_alloc_f(const std::string& key, long B, long H, long W, long C) {
    float* p = (float*)DevicePool::global().acquire_dynamic(
        VramCategory::Image, key, (size_t)(B * H * W * C) * sizeof(float));
    return TorchTensorView((uint64_t)p, 4, {B, H, W, C});
}
static inline TorchTensorView _pool_alloc_f_zero(PoolSlot key, long B, long H, long W, long C) {
    float* p = DevicePool::global().acquire<float>(key, (size_t)(B * H * W * C));
    cudaMemset(p, 0, B * H * W * C * sizeof(float));
    return TorchTensorView((uint64_t)p, 4, {B, H, W, C});
}
static inline TorchTensorView _pool_alloc_b(const std::string& key, long B, long H, long W) {
    bool* p = (bool*)DevicePool::global().acquire_dynamic(
        VramCategory::Image, key, (size_t)(B * H * W) * sizeof(bool));
    return TorchTensorView((uint64_t)p, 1, {B, H, W, 1});
}

static inline TensorView<float, 4> _tv_view4(const TorchTensorView& tv) {
    const auto& s = std::get<2>(tv);
    return _make_view4(_fptr(tv), s[0], s[1], s[2], s[3]);
}
static inline TensorView<uint8_t, 4> _tv_view4_u8(const TorchTensorView& tv) {
    const auto& s = std::get<2>(tv);
    return _make_view4((uint8_t*)std::get<0>(tv), s[0], s[1], s[2], s[3]);
}

template<typename T, uint size>
inline __device__ FixedArray<T, size> loadFixedArray(const T* p, long idx) {
    FixedArray<T, size> arr;
    #pragma unroll
    for (int i = 0; i < size; i++)
        arr[i] = p[idx * size + i];
    return arr;
}

template<typename T, uint size>
inline __device__ void saveFixedArray(T* __restrict__ p, long idx, const FixedArray<T, size> &arr) {
    #pragma unroll
    for (int i = 0; i < size; i++)
        p[idx * size + i] = arr[i];
}


inline __device__ int atomicAddWhich(RawLossIndex idx) {
    if (
        idx == RawLossIndex::DepthSupX ||
        idx == RawLossIndex::DepthSupY ||
        idx == RawLossIndex::DepthSupXX ||
        idx == RawLossIndex::DepthSupYY ||
        idx == RawLossIndex::DepthSupXY
    ) return 1;
    if (
        idx == RawLossIndex::DepthMaskTotal
    ) return 2;
    return 0;
}

inline __device__ bool isTotalReduce(LossIndex idx) {
    return idx != LossIndex::DepthSup;
}


__global__ void per_pixel_losses_forward_kernel(
    const size_t batch_size,
    const size_t pixels_per_image,
    const int W_render, const int H_render,
    // Per-modality GT (H, W). When the GT buffer is at the same resolution as
    // the render the bilinear sampler reduces to a single-tap read (weight 1
    // on the diagonal), so equal-shape datasets get bit-identical results.
    const int W_ref_depth,  const int H_ref_depth,
    const int W_ref_normal, const int H_ref_normal,
    const int W_ref_alpha,  const int H_ref_alpha,
    const int64_t* __restrict__ camera_indices,
    const float3* __restrict__ render_rgb,
    const float3* __restrict__ ref_rgb,
    const float* __restrict__ render_depth,
    const float* __restrict__ ref_depth,
    const float3* __restrict__ render_normal,
    const float3* __restrict__ depth_normal,
    const float3* __restrict__ ref_normal,
    const float* __restrict__ render_Ts,
    const float3* __restrict__ rgb_dist,
    const float* __restrict__ depth_dist,
    const float3* __restrict__ normal_dist,
    const float* __restrict__ median_depth,
    const float3* __restrict__ median_normal,
    const bool* __restrict__ ref_alpha,
    bool has_mask,                          // gt_alpha buffer present (per-pixel mask)
    FixedArray<float, (uint)LossWeightIndex::length> loss_weights,
    float* __restrict__ out_loss_map,  // non differentiable
    float* __restrict__ out_losses
) {
    size_t pixel_idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t batch_idx = blockIdx.y * blockDim.y + threadIdx.y;
    if (batch_idx >= batch_size)
        return;
    size_t idx = batch_idx * pixels_per_image + pixel_idx;

    // (x, y) of this thread in the rendered-resolution grid -- used only as
    // destination coords for the bilinear sampler when the GT modality is at
    // a different shape.
    int x_dst = (int)(pixel_idx % (size_t)W_render);
    int y_dst = (int)(pixel_idx / (size_t)W_render);

    FixedArray<float, (uint)RawLossIndex::length> losses;

    bool inside = pixel_idx < pixels_per_image;
    if (inside) {
        // GT modalities -- bilinear-sample at (x_dst, y_dst) into their own
        // shape. When the GT shape equals render shape, this collapses to
        // a single-tap read (weight 1 on the diagonal).
        float ref_depth_v = 1.f;
        if (ref_depth) {
            ref_depth_v = bilinear_sample_gt_depth(
                ref_depth, (int)batch_idx, x_dst, y_dst,
                W_render, H_render, W_ref_depth, H_ref_depth);
        }
        float3 ref_normal_v = make_float3(0);
        if (ref_normal) {
            ref_normal_v = bilinear_sample_gt_normal(
                ref_normal, (int)batch_idx, x_dst, y_dst,
                W_render, H_render, W_ref_normal, H_ref_normal);
        }
        bool ref_alpha_v = false;
        if (ref_alpha) {
            ref_alpha_v = nearest_sample_b(
                ref_alpha, (int)batch_idx, x_dst, y_dst,
                W_render, H_render, W_ref_alpha, H_ref_alpha);
        }

        SlangPerPixelLosses::per_pixel_losses(
            render_rgb ? render_rgb[idx] : make_float3(0),
            ref_rgb ? ref_rgb[idx] : make_float3(0),
            render_depth ? render_depth[idx] : 1.f,
            ref_depth_v,
            render_normal ? render_normal[idx] : make_float3(0),
            depth_normal ? depth_normal[idx] : make_float3(0),
            ref_normal_v,
            render_Ts ? render_Ts[idx] : 1.f,
            rgb_dist ? rgb_dist[idx] : make_float3(0),
            depth_dist ? depth_dist[idx] : 0.f,
            normal_dist ? normal_dist[idx] : make_float3(0),
            median_depth ? median_depth[idx] : 0.f,
            median_normal ? median_normal[idx] : make_float3(0),
            ref_alpha_v,
            has_mask,
            loss_weights,
            &losses
        );

        if (out_loss_map != nullptr) {
            out_loss_map[idx] = fabsf(
                losses[(int)RawLossIndex::RgbLoss] +
                losses[(int)RawLossIndex::RenderNormalSup] +
                losses[(int)RawLossIndex::DepthNormalSup] +
                losses[(int)RawLossIndex::AlphaSup] +
                losses[(int)RawLossIndex::AlphaSupUnder] +
                losses[(int)RawLossIndex::NormalReg] +
                losses[(int)RawLossIndex::AlphaReg] +
                losses[(int)RawLossIndex::RgbDistReg] +
                losses[(int)RawLossIndex::DepthDistReg] +
                losses[(int)RawLossIndex::NormalDistReg] +
                losses[(int)RawLossIndex::MeanMedianDepthSup] +
                losses[(int)RawLossIndex::MedianDepthNormalReg] +
                losses[(int)RawLossIndex::MedianNormalSup] +
                losses[(int)RawLossIndex::MedianRenderNormalReg]
            );
            // TODO: more accurate version
        }
    }

    auto block = cg::this_thread_block();
    cg::thread_block_tile<WARP_SIZE> warp = cg::tiled_partition<WARP_SIZE>(block);
    uint warp_idx = block.thread_rank() / WARP_SIZE;

    __shared__ float atomic_reduce[WARP_SIZE];

    if (camera_indices != nullptr)
        batch_idx = camera_indices[batch_idx];
    for (uint i = 0; i < (uint)RawLossIndex::length; i++) {
        float loss = inside ? losses[i] : 0.0f;
        loss = isfinite(loss) ? loss : 0.0f;
        float loss_reduced = cg::reduce(warp, loss, cg::plus<float>());
        if (warp.thread_rank() == 0)
            atomic_reduce[warp_idx] = loss_reduced;
        __syncthreads();
        loss = (warp_idx == 0) ? atomic_reduce[warp.thread_rank()] : 0.0f;
        if (__ballot_sync(~0u, loss != 0.0f) == 0)
            continue;
        loss_reduced = cg::reduce(warp, loss, cg::plus<float>());
        if (block.thread_rank() == 0 && loss_reduced != 0.0f && isfinite(loss_reduced)) {
            int which = atomicAddWhich((RawLossIndex)i);
            if (which == 0 || which == 2)
                atomicAdd(out_losses + i, loss_reduced);
            if (which == 1 || which == 2)
                atomicAdd(out_losses + (batch_idx+1)*(size_t)RawLossIndex::length + i, loss_reduced);
        }
    }
}

__global__ void per_pixel_losses_backward_kernel(
    const size_t batch_size,
    const size_t pixels_per_image,
    const int W_render, const int H_render,
    const int W_ref_depth,  const int H_ref_depth,
    const int W_ref_normal, const int H_ref_normal,
    const int W_ref_alpha,  const int H_ref_alpha,
    const int64_t* __restrict__ camera_indices,
    const float3* __restrict__ render_rgb,
    const float3* __restrict__ ref_rgb,
    const float* __restrict__ render_depth,
    const float* __restrict__ ref_depth,
    const float3* __restrict__ render_normal,
    const float3* __restrict__ depth_normal,
    const float3* __restrict__ ref_normal,
    const float* __restrict__ render_Ts,
    const float3* __restrict__ rgb_dist,
    const float* __restrict__ depth_dist,
    const float3* __restrict__ normal_dist,
    const float* __restrict__ median_depth,
    const float3* __restrict__ median_normal,
    const bool* __restrict__ ref_alpha,
    bool has_mask,                          // gt_alpha buffer present
    FixedArray<float, (uint)LossWeightIndex::length> loss_weights,
    const float* __restrict__ v_out_losses,
    float3* __restrict__ v_render_rgb,
    float3* __restrict__ v_ref_rgb,
    float* __restrict__ v_render_depth,
    float* __restrict__ v_ref_depth,
    float3* __restrict__ v_render_normal,
    float3* __restrict__ v_depth_normal,
    float3* __restrict__ v_ref_normal,
    float* __restrict__ v_render_Ts,
    float3* __restrict__ v_rgb_dist,
    float* __restrict__ v_depth_dist,
    float3* __restrict__ v_normal_dist,
    float* __restrict__ v_median_depth,
    float3* __restrict__ v_median_normal
) {
    size_t pixel_idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t batch_idx = blockIdx.y * blockDim.y + threadIdx.y;
    if (batch_idx >= batch_size)
        return;
    size_t idx = batch_idx * pixels_per_image + pixel_idx;

    bool inside = pixel_idx < pixels_per_image;
    if (!inside) return;

    int x_dst = (int)(pixel_idx % (size_t)W_render);
    int y_dst = (int)(pixel_idx / (size_t)W_render);

    if (camera_indices != nullptr)
        batch_idx = camera_indices[batch_idx];
    FixedArray<float, (uint)RawLossIndex::length> v_losses;
    for (uint i = 0; i < (uint)RawLossIndex::length; i++) {
        float temp_loss = 0.0f;
        int which = atomicAddWhich((RawLossIndex)i);
        if (which == 0 || which == 2)
            temp_loss += v_out_losses[i];
        if (which == 1 || which == 2)
            temp_loss += v_out_losses[(batch_idx+1)*(size_t)RawLossIndex::length + i];
        v_losses[i] = temp_loss;
    }

    float3 temp_v_render_rgb;
    float3 temp_v_ref_rgb;
    float temp_v_render_depth;
    float temp_v_ref_depth;
    float3 temp_v_render_normal;
    float3 temp_v_depth_normal;
    float3 temp_v_ref_normal;
    float temp_v_render_Ts;
    float3 temp_v_rgb_dist;
    float temp_v_depth_dist;
    float3 temp_v_normal_dist;
    float temp_v_median_depth;
    float3 temp_v_median_normal;

    // Re-sample the GT modalities the same way the forward did, so the bwd
    // sees identical inputs.
    float  ref_depth_v  = 1.f;
    float3 ref_normal_v = make_float3(0);
    bool   ref_alpha_v  = false;
    if (ref_depth) {
        ref_depth_v = bilinear_sample_gt_depth(
            ref_depth, (int)batch_idx, x_dst, y_dst,
            W_render, H_render, W_ref_depth, H_ref_depth);
    }
    if (ref_normal) {
        ref_normal_v = bilinear_sample_gt_normal(
            ref_normal, (int)batch_idx, x_dst, y_dst,
            W_render, H_render, W_ref_normal, H_ref_normal);
    }
    if (ref_alpha) {
        ref_alpha_v = nearest_sample_b(
            ref_alpha, (int)batch_idx, x_dst, y_dst,
            W_render, H_render, W_ref_alpha, H_ref_alpha);
    }

    SlangPerPixelLosses::per_pixel_losses_bwd(
        render_rgb ? render_rgb[idx] : make_float3(0),
        ref_rgb ? ref_rgb[idx] : make_float3(0),
        render_depth ? render_depth[idx] : 1.f,
        ref_depth_v,
        render_normal ? render_normal[idx] : make_float3(0),
        depth_normal ? depth_normal[idx] : make_float3(0),
        ref_normal_v,
        render_Ts ? render_Ts[idx] : 1.f,
        rgb_dist ? rgb_dist[idx] : make_float3(0),
        depth_dist ? depth_dist[idx] : 0.f,
        normal_dist ? normal_dist[idx] : make_float3(0),
        median_depth ? median_depth[idx] : 0.f,
        median_normal ? median_normal[idx] : make_float3(0),
        ref_alpha_v,
        has_mask,
        loss_weights,
        v_losses,
        &temp_v_render_rgb,
        &temp_v_ref_rgb,
        &temp_v_render_depth,
        &temp_v_ref_depth,
        &temp_v_render_normal,
        &temp_v_depth_normal,
        &temp_v_ref_normal,
        &temp_v_render_Ts,
        &temp_v_rgb_dist,
        &temp_v_depth_dist,
        &temp_v_normal_dist,
        &temp_v_median_depth,
        &temp_v_median_normal
    );

    // Render-resolution outputs: direct write (one writer per pixel).
    if (v_render_rgb)    v_render_rgb[idx]    = temp_v_render_rgb;
    if (v_ref_rgb)       v_ref_rgb[idx]       = temp_v_ref_rgb;
    if (v_render_depth)  v_render_depth[idx]  = temp_v_render_depth;
    if (v_render_normal) v_render_normal[idx] = temp_v_render_normal;
    if (v_depth_normal)  v_depth_normal[idx]  = temp_v_depth_normal;
    if (v_render_Ts)     v_render_Ts[idx]     = temp_v_render_Ts;
    if (v_rgb_dist)      v_rgb_dist[idx]      = temp_v_rgb_dist;
    if (v_depth_dist)    v_depth_dist[idx]    = temp_v_depth_dist;
    if (v_normal_dist)   v_normal_dist[idx]   = temp_v_normal_dist;
    if (v_median_depth)  v_median_depth[idx]  = temp_v_median_depth;
    if (v_median_normal) v_median_normal[idx] = temp_v_median_normal;

    // GT-resolution outputs: bilinear scatter (multiple render pixels may
    // share a GT tap). Uses atomicAdd, so the v_ref_depth / v_ref_normal
    // buffers MUST be zeroed before the kernel launch -- EngineLoss does
    // that via _pool_alloc_f_zero / cudaMemsetAsync.
    if (v_ref_depth) {
        bilinear_scatter_add_gt_depth(
            ref_depth, v_ref_depth, (int)batch_idx, x_dst, y_dst,
            W_render, H_render, W_ref_depth, H_ref_depth, temp_v_ref_depth);
    }
    if (v_ref_normal) {
        bilinear_scatter_add_gt_normal(
            ref_normal, v_ref_normal, (int)batch_idx, x_dst, y_dst,
            W_render, H_render, W_ref_normal, H_ref_normal, temp_v_ref_normal);
    }
}

__global__ void per_pixel_losses_reduce_forward_kernel(
    const size_t batch_size,
    const float* __restrict__ raw_losses,  // [B, RawLossIndex::length]
    FixedArray<float, (uint)LossWeightIndex::length> loss_weights,
    float* __restrict__ losses  // [LossIndex::length]
) {
    size_t batch_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (batch_idx > batch_size)
        return;

    FixedArray<float, (uint)RawLossIndex::length> local_raw_losses =
        loadFixedArray<float, (uint)RawLossIndex::length>(raw_losses, batch_idx);

    FixedArray<float, (uint)LossIndex::length> local_losses;
    SlangPerPixelLosses::per_pixel_losses_reduce(
        local_raw_losses, loss_weights,
        &local_losses
    );
    #pragma unroll
    for (int i = 0; i < (uint)LossIndex::length; i++)
        if (local_losses[i] != 0.0f && isfinite(local_losses[i])) {
            if (isTotalReduce((LossIndex)i)) {
                if (batch_idx == 0)
                    losses[i] = local_losses[i];
            }
            else if (batch_idx != 0)
                atomicAdd(&losses[i], local_losses[i] / (float)batch_size);
        }
}

__global__ void per_pixel_losses_reduce_backward_kernel(
    const size_t batch_size,
    const float* __restrict__ raw_losses,  // [B, RawLossIndex::length]
    FixedArray<float, (uint)LossWeightIndex::length> loss_weights,
    const float* __restrict__ v_losses,  // [LossIndex::length]
    float* __restrict__ v_raw_losses  // [B, RawLossIndex::length]
) {
    size_t batch_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (batch_idx > batch_size)
        return;

    FixedArray<float, (uint)LossIndex::length> local_v_losses;
    #pragma unroll
    for (int i = 0; i < (uint)LossIndex::length; i++) {
        float temp_v_loss = 0.0f;
        if (isTotalReduce((LossIndex)i)) {
            if (batch_idx == 0)
                temp_v_loss = v_losses[i];
        }
        else if (batch_idx != 0)
            temp_v_loss = v_losses[i] / (float)batch_size;
        local_v_losses[i] = temp_v_loss;
    }

    FixedArray<float, (uint)RawLossIndex::length> local_raw_losses =
        loadFixedArray<float, (uint)RawLossIndex::length>(raw_losses, batch_idx);
    FixedArray<float, (uint)RawLossIndex::length> local_v_raw_losses;

    SlangPerPixelLosses::per_pixel_losses_reduce_bwd(
        local_raw_losses, loss_weights,
        local_v_losses, &local_v_raw_losses
    );

    saveFixedArray<float, (uint)RawLossIndex::length>(v_raw_losses, batch_idx, local_v_raw_losses);
}


// Read source (H, W) from a TorchTensorView shape [B, H, W, C], or (0, 0) if
// the view is null. Used to pass per-modality dims to the kernel.
static inline void _hw_or_zero(const TorchTensorView& tv, int& H, int& W) {
    if (!_has(tv)) { H = 0; W = 0; return; }
    const auto& s = std::get<2>(tv);
    H = (int)s[1]; W = (int)s[2];
}

static void _compute_per_pixel_losses_forward(
    long B, long pixels_per_image,
    int W_render, int H_render,
    TorchTensorView render_rgb,
    TorchTensorView ref_rgb,
    TorchTensorView render_depth,
    TorchTensorView ref_depth,
    TorchTensorView render_normal,
    TorchTensorView depth_normal,
    TorchTensorView ref_normal,
    TorchTensorView render_Ts,
    TorchTensorView rgb_dist,
    TorchTensorView depth_dist,
    TorchTensorView normal_dist,
    TorchTensorView median_depth,
    TorchTensorView median_normal,
    TorchTensorView ref_alpha,
    bool has_mask,
    FixedArray<float, (uint)LossWeightIndex::length> loss_weights,
    long num_train_images,
    TorchTensorView camera_indices,
    float* loss_map_ptr,
    float* raw_losses_ptr,
    float* losses_ptr
) {
    int Hd, Wd; _hw_or_zero(ref_depth,  Hd, Wd);
    int Hn, Wn; _hw_or_zero(ref_normal, Hn, Wn);
    int Ha, Wa; _hw_or_zero(ref_alpha,  Ha, Wa);
    per_pixel_losses_forward_kernel<<<_LAUNCH_ARGS_2D(pixels_per_image, B, WARP_SIZE*WARP_SIZE, 1)>>>(
        B, pixels_per_image,
        W_render, H_render,
        Wd, Hd, Wn, Hn, Wa, Ha,
        _i64ptr(camera_indices),
        _f3ptr(render_rgb),
        _f3ptr(ref_rgb),
        _fptr(render_depth),
        _fptr(ref_depth),
        _f3ptr(render_normal),
        _f3ptr(depth_normal),
        _f3ptr(ref_normal),
        _fptr(render_Ts),
        _f3ptr(rgb_dist),
        _fptr(depth_dist),
        _f3ptr(normal_dist),
        _fptr(median_depth),
        _f3ptr(median_normal),
        _bptr(ref_alpha),
        has_mask,
        loss_weights,
        loss_map_ptr,
        raw_losses_ptr
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());

    per_pixel_losses_reduce_forward_kernel
    <<<_LAUNCH_ARGS_1D(num_train_images+1, WARP_SIZE)>>>(
        num_train_images,
        raw_losses_ptr,
        loss_weights,
        losses_ptr
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}


static void _compute_per_pixel_losses_backward(
    long B, long pixels_per_image,
    int W_render, int H_render,
    TorchTensorView render_rgb,
    TorchTensorView ref_rgb,
    TorchTensorView render_depth,
    TorchTensorView ref_depth,
    TorchTensorView render_normal,
    TorchTensorView depth_normal,
    TorchTensorView ref_normal,
    TorchTensorView render_Ts,
    TorchTensorView rgb_dist,
    TorchTensorView depth_dist,
    TorchTensorView normal_dist,
    TorchTensorView median_depth,
    TorchTensorView median_normal,
    TorchTensorView ref_alpha,
    bool has_mask,
    float* raw_losses_ptr,
    FixedArray<float, (uint)LossWeightIndex::length> loss_weights,
    float* v_losses_ptr,
    long num_train_images,
    TorchTensorView camera_indices,
    PerPixelGrads& grads
) {
    float* v_raw_losses = DevicePool::global().acquire<float>(
        PoolSlot::PplVRawLosses, (size_t)(num_train_images+1) * (uint)RawLossIndex::length);

    per_pixel_losses_reduce_backward_kernel<<<_LAUNCH_ARGS_1D(num_train_images+1, WARP_SIZE)>>>(
        num_train_images,
        raw_losses_ptr,
        loss_weights,
        v_losses_ptr,
        v_raw_losses
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());

    int Hd, Wd; _hw_or_zero(ref_depth,  Hd, Wd);
    int Hn, Wn; _hw_or_zero(ref_normal, Hn, Wn);
    int Ha, Wa; _hw_or_zero(ref_alpha,  Ha, Wa);
    per_pixel_losses_backward_kernel<<<_LAUNCH_ARGS_2D(pixels_per_image, B, 256, 1)>>>(
        B, pixels_per_image,
        W_render, H_render,
        Wd, Hd, Wn, Hn, Wa, Ha,
        _i64ptr(camera_indices),
        _f3ptr(render_rgb),
        _f3ptr(ref_rgb),
        _fptr(render_depth),
        _fptr(ref_depth),
        _f3ptr(render_normal),
        _f3ptr(depth_normal),
        _f3ptr(ref_normal),
        _fptr(render_Ts),
        _f3ptr(rgb_dist),
        _fptr(depth_dist),
        _f3ptr(normal_dist),
        _fptr(median_depth),
        _f3ptr(median_normal),
        _bptr(ref_alpha),
        has_mask,
        loss_weights,
        v_raw_losses,
        _f3ptr(grads.v_render_rgb),
        _f3ptr(grads.v_ref_rgb),
        _fptr(grads.v_render_depth),
        _fptr(grads.v_ref_depth),
        _f3ptr(grads.v_render_normal),
        _f3ptr(grads.v_depth_normal),
        _f3ptr(grads.v_ref_normal),
        _fptr(grads.v_render_Ts),
        _f3ptr(grads.v_rgb_dist),
        _fptr(grads.v_depth_dist),
        _f3ptr(grads.v_normal_dist),
        _fptr(grads.v_median_depth),
        _f3ptr(grads.v_median_normal)
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}


__global__ void avg_pool_downsample_float_kernel(
    const TensorView<float, 4> image_hs,
    TensorView<float, 4> image_ls
) {
    uint32_t xid = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t yid = blockIdx.y * blockDim.y + threadIdx.y;
    uint32_t bid = blockIdx.z * blockDim.z + threadIdx.z;
    if (yid >= image_ls.shape[1] || xid >= image_ls.shape[2])
        return;
    for (int c = 0; c < image_ls.shape[3]; ++c) {
        float v =
            image_hs.at(bid, 2*yid+0, 2*xid+0, c) +
            image_hs.at(bid, 2*yid+0, 2*xid+1, c) +
            image_hs.at(bid, 2*yid+1, 2*xid+0, c) +
            image_hs.at(bid, 2*yid+1, 2*xid+1, c);
        image_ls.at(bid, yid, xid, c) = 0.25f*v;
    }
}

// GT geometry does not pool like an image: averaging a sentinel (0 depth,
// black normal) into a 2x2 block grows a ring of plausible-looking wrong
// supervision around every hole, one pixel per level. All-sentinel stays one.
template<int channels>
__global__ void avg_pool_downsample_gt_geometry_kernel(
    const TensorView<float, 4> image_hs,
    TensorView<float, 4> image_ls
) {
    uint32_t xid = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t yid = blockIdx.y * blockDim.y + threadIdx.y;
    uint32_t bid = blockIdx.z * blockDim.z + threadIdx.z;
    if (yid >= image_ls.shape[1] || xid >= image_ls.shape[2])
        return;
    float acc[channels] = {};
    int n = 0;
    for (int dy = 0; dy < 2; ++dy)
        for (int dx = 0; dx < 2; ++dx) {
            float v[channels];
            for (int c = 0; c < channels; ++c)
                v[c] = image_hs.at(bid, 2*yid+dy, 2*xid+dx, c);
            if (channels == 1) {
                if (!(v[0] > 0.0f)) continue;
            } else {
                // core/Interpolation.cuh's gt_normal_valid.
                float s = v[0] + v[1] + v[2];
                float l2 = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
                if (!(s > -2.366f && l2 > 0.25f)) continue;
            }
            for (int c = 0; c < channels; ++c) acc[c] += v[c];
            ++n;
        }
    for (int c = 0; c < channels; ++c)
        image_ls.at(bid, yid, xid, c) =
            n ? acc[c] / (float)n : (channels == 1 ? 0.0f : -1.0f);
}

// RGB does not pool across a mask edge either: a box mean of two background
// pixels and two distractor pixels is a colour the render is then asked to
// match. Averaging only the unmasked children keeps every scale comparable.
template<int channels>
__global__ void avg_pool_downsample_masked_float_kernel(
    const TensorView<float, 4> image_hs,
    const TensorView<uint8_t, 4> mask_hs,
    TensorView<float, 4> image_ls
) {
    uint32_t xid = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t yid = blockIdx.y * blockDim.y + threadIdx.y;
    uint32_t bid = blockIdx.z * blockDim.z + threadIdx.z;
    if (yid >= image_ls.shape[1] || xid >= image_ls.shape[2])
        return;
    float acc[channels] = {}, box[channels] = {};
    int n = 0;
    for (int dy = 0; dy < 2; ++dy)
        for (int dx = 0; dx < 2; ++dx) {
            const bool valid = mask_hs.at(bid, 2*yid+dy, 2*xid+dx, 0) != 0;
            for (int c = 0; c < channels; ++c) {
                float v = image_hs.at(bid, 2*yid+dy, 2*xid+dx, c);
                box[c] += 0.25f * v;
                if (valid) acc[c] += v;
            }
            n += (int)valid;
        }
    // All four masked: the coarse pixel is masked too, so the value is unused.
    for (int c = 0; c < channels; ++c)
        image_ls.at(bid, yid, xid, c) = n ? acc[c] / (float)n : box[c];
}

template<typename uintx_t>
__global__ void avg_pool_downsample_integral_kernel(
    const TensorView<uintx_t, 4> image_hs,
    TensorView<uintx_t, 4> image_ls
) {
    uint32_t xid = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t yid = blockIdx.y * blockDim.y + threadIdx.y;
    uint32_t bid = blockIdx.z * blockDim.z + threadIdx.z;
    if (yid >= image_ls.shape[1] || xid >= image_ls.shape[2])
        return;
    for (int c = 0; c < image_ls.shape[3]; ++c) {
        float v =
            (float)image_hs.at(bid, 2*yid+0, 2*xid+0, c) +
            (float)image_hs.at(bid, 2*yid+0, 2*xid+1, c) +
            (float)image_hs.at(bid, 2*yid+1, 2*xid+0, c) +
            (float)image_hs.at(bid, 2*yid+1, 2*xid+1, c);
        image_ls.at(bid, yid, xid, c) = (uintx_t)(0.25f*v + 0.5f);
    }
}

__global__ void avg_pool_downsample_bool_kernel(
    const TensorView<uint8_t, 4> image_hs,
    TensorView<uint8_t, 4> image_ls
) {
    uint32_t xid = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t yid = blockIdx.y * blockDim.y + threadIdx.y;
    uint32_t bid = blockIdx.z * blockDim.z + threadIdx.z;
    if (yid >= image_ls.shape[1] || xid >= image_ls.shape[2])
        return;
    for (int c = 0; c < image_ls.shape[3]; ++c) {
        uint8_t v =
            image_hs.at(bid, 2*yid+0, 2*xid+0, c) +
            image_hs.at(bid, 2*yid+0, 2*xid+1, c) +
            image_hs.at(bid, 2*yid+1, 2*xid+0, c) +
            image_hs.at(bid, 2*yid+1, 2*xid+1, c);
        image_ls.at(bid, yid, xid, c) = (uint8_t)(v >= 2);
    }
}


static void _avg_pool_downsample_float(const TorchTensorView& src, const TorchTensorView& dst) {
    const auto& s = std::get<2>(dst);
    long b = s[0], h = s[1], w = s[2], c = s[3];
    avg_pool_downsample_float_kernel<<<_LAUNCH_ARGS_3D(w, h, b, 16, 16, 1)>>>(
        _tv_view4(src), _tv_view4(dst));
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

static void _avg_pool_downsample_gt_geometry(const TorchTensorView& src,
                                             const TorchTensorView& dst) {
    const auto& s = std::get<2>(dst);
    long b = s[0], h = s[1], w = s[2], c = s[3];
    if (c == 1)
        avg_pool_downsample_gt_geometry_kernel<1>
            <<<_LAUNCH_ARGS_3D(w, h, b, 16, 16, 1)>>>(_tv_view4(src), _tv_view4(dst));
    else
        avg_pool_downsample_gt_geometry_kernel<3>
            <<<_LAUNCH_ARGS_3D(w, h, b, 16, 16, 1)>>>(_tv_view4(src), _tv_view4(dst));
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

// The coarse-scale grad is scattered back uniformly over all four children,
// masked ones included -- that is the one path that lets a masked pixel be
// trained toward the distractor the mask exists to ignore.
__global__ void zero_masked_grad_kernel(
    TensorView<float, 4> grad,
    const TensorView<uint8_t, 4> mask
) {
    uint32_t xid = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t yid = blockIdx.y * blockDim.y + threadIdx.y;
    uint32_t bid = blockIdx.z * blockDim.z + threadIdx.z;
    if (yid >= grad.shape[1] || xid >= grad.shape[2])
        return;
    if (mask.at(bid, yid, xid, 0) != 0)
        return;
    for (int c = 0; c < grad.shape[3]; ++c)
        grad.at(bid, yid, xid, c) = 0.0f;
}

static void _zero_masked_grad(const TorchTensorView& grad, const TorchTensorView& mask) {
    const auto& s = std::get<2>(grad);
    zero_masked_grad_kernel<<<_LAUNCH_ARGS_3D(s[2], s[1], s[0], 16, 16, 1)>>>(
        _tv_view4(grad), _tv_view4_u8(mask));
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

static void _avg_pool_downsample_masked_float(const TorchTensorView& src,
                                             const TorchTensorView& mask,
                                             const TorchTensorView& dst) {
    const auto& s = std::get<2>(dst);
    long b = s[0], h = s[1], w = s[2], c = s[3];
    if (c == 1)
        avg_pool_downsample_masked_float_kernel<1>
            <<<_LAUNCH_ARGS_3D(w, h, b, 16, 16, 1)>>>(
                _tv_view4(src), _tv_view4_u8(mask), _tv_view4(dst));
    else
        avg_pool_downsample_masked_float_kernel<3>
            <<<_LAUNCH_ARGS_3D(w, h, b, 16, 16, 1)>>>(
                _tv_view4(src), _tv_view4_u8(mask), _tv_view4(dst));
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

static void _avg_pool_downsample_bool(const TorchTensorView& src, const TorchTensorView& dst) {
    const auto& s = std::get<2>(dst);
    long b = s[0], h = s[1], w = s[2];
    avg_pool_downsample_bool_kernel<<<_LAUNCH_ARGS_3D(w, h, b, 16, 16, 1)>>>(
        _tv_view4_u8(src), _tv_view4_u8(dst));
    CHECK_DEVICE_ERROR(cudaGetLastError());
}


__global__ void vector_add_scaled_kernel(
    float* __restrict__ dst,
    const float* __restrict__ src,
    float scale,
    int n
) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) dst[i] += src[i] * scale;
}

// Sobel x; transposed for y. Must match canny_filter_3x3 in
// kernels/densify/DensifySplitFilter.cu, which is __constant__ in that TU.
__constant__ float _nms_sobel_3x3[3][3] = {
    { -1.0f, 0.0f, 1.0f },
    { -2.0f, 0.0f, 2.0f },
    { -1.0f, 0.0f, 1.0f },
};

// Canny's NMS step run on the loss map itself, not on a gradient magnitude:
// its Sobel gives a ridge normal, and each of the two neighbours along it that
// outranks the pixel costs it one falloff factor. Stores that count, 0..2.
__global__ void loss_map_nms_mask_kernel(
    TensorView<float, 4> img,      // [B, H, W, 1]
    uint8_t* __restrict__ fails    // [B, H, W], 0..2 sides outranking
) {
    constexpr int BLOCK = 32;
    constexpr int HALO = 1;  // Sobel at the centre pixel + one step along it

    const int32_t xid = blockIdx.x * BLOCK + threadIdx.x;
    const int32_t yid = blockIdx.y * BLOCK + threadIdx.y;
    const int32_t bid = blockIdx.z;
    const int H = (int)img.shape[1], W = (int)img.shape[2];

    __shared__ float tile[BLOCK + 2 * HALO][BLOCK + 2 * HALO];
    #pragma unroll
    for (int batch = 0; batch < (BLOCK+2*HALO)*(BLOCK+2*HALO); batch += BLOCK*BLOCK) {
        int tid = batch + threadIdx.y * BLOCK + threadIdx.x;
        int y = tid / (BLOCK + 2 * HALO);
        int x = tid % (BLOCK + 2 * HALO);
        if (y < BLOCK + 2 * HALO) {
            int yi = min(max((int)(blockIdx.y * BLOCK) + y - HALO, 0), H - 1);
            int xi = min(max((int)(blockIdx.x * BLOCK) + x - HALO, 0), W - 1);
            tile[y][x] = img.load1(bid, yi, xi);
        }
    }
    __syncthreads();

    if (yid >= H || xid >= W)
        return;

    const int ty = (int)threadIdx.y + HALO, tx = (int)threadIdx.x + HALO;
    float gx = 0.0f, gy = 0.0f;
    #pragma unroll
    for (int cy = -1; cy <= 1; ++cy)
        #pragma unroll
        for (int cx = -1; cx <= 1; ++cx) {
            float v = tile[ty + cy][tx + cx];
            gx += _nms_sobel_3x3[cy + 1][cx + 1] * v;
            gy += _nms_sobel_3x3[cx + 1][cy + 1] * v;
        }

    uint8_t n_fail = 0;
    float g = sqrtf(gx * gx + gy * gy);
    if (g > 0.0f) {
        int dx = min(max((int)roundf(gx / g), -HALO), HALO);
        int dy = min(max((int)roundf(gy / g), -HALO), HALO);
        float v = tile[ty][tx];
        // Masked-out neighbours sit at 0, so they can only protect a pixel.
        n_fail = (uint8_t)((v < tile[ty + dy][tx + dx] ? 1 : 0) +
                           (v < tile[ty - dy][tx - dx] ? 1 : 0));
    }
    fails[(size_t)(bid * H + yid) * W + xid] = n_fail;
}

// Pass 2 of the in-place suppression. Overwriting inside pass 1 would race
// (a block's neighbours read the pixels it writes) and a float scratch would
// cost 4x this 1-byte count.
__global__ void loss_map_nms_apply_kernel(
    TensorView<float, 4> img,
    const uint8_t* __restrict__ fails,
    float falloff
) {
    const int32_t xid = blockIdx.x * blockDim.x + threadIdx.x;
    const int32_t yid = blockIdx.y * blockDim.y + threadIdx.y;
    const int32_t bid = blockIdx.z;
    const int H = (int)img.shape[1], W = (int)img.shape[2];
    if (yid >= H || xid >= W)
        return;
    const float lut[3] = {1.0f, falloff, falloff * falloff};
    img.at(bid, yid, xid, 0) *=
        lut[fails[(size_t)(bid * H + yid) * W + xid]];
}

// falloff 0 is the hard suppression canny does; 1 leaves the map untouched.
static void _loss_map_nms_inplace(const TorchTensorView& img,
                                  const std::string& key, float falloff) {
    const auto& sh = std::get<2>(img);
    long B = sh[0], H = sh[1], W = sh[2];
    if (B <= 0 || H <= 0 || W <= 0 || falloff >= 1.0f)
        return;
    falloff = fmaxf(falloff, 0.0f);
    uint8_t* fails = (uint8_t*)DevicePool::global().acquire_dynamic(
        VramCategory::Image, key, (size_t)(B * H * W));
    auto view = _tv_view4(img);
    loss_map_nms_mask_kernel<<<_LAUNCH_ARGS_3D(W, H, B, 32, 32, 1)>>>(view, fails);
    CHECK_DEVICE_ERROR(cudaGetLastError());
    loss_map_nms_apply_kernel<<<_LAUNCH_ARGS_3D(W, H, B, 32, 32, 1)>>>(
        view, fails, falloff);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}


__global__ void avg_pool_upsample_float_kernel(
    TensorView<float, 4> image_hs,
    const TensorView<float, 4> image_ls,
    int scale,
    float a, float b
) {
    uint32_t xid = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t yid = blockIdx.y * blockDim.y + threadIdx.y;
    uint32_t bid = blockIdx.z * blockDim.z + threadIdx.z;
    if (yid >= image_hs.shape[1] || xid >= image_hs.shape[2])
        return;
    for (int c = 0; c < image_hs.shape[3]; ++c) {
        float v = (a == 0.0f) ? 0.0f :
            a * image_hs.at(bid, yid, xid, c);
        if (yid/scale < image_ls.shape[1] && xid/scale < image_ls.shape[2])
            v += b * image_ls.at(bid, yid/scale, xid/scale, c);
        image_hs.at(bid, yid, xid, c) = v;
    }
}


/*[AutoHeaderGeneratorExport]*/
LossValues compute_multi_scale_per_pixel_losses(
    int num_loss_scales,
    TorchTensorView render_rgb,
    TorchTensorView ref_rgb,
    TorchTensorView render_depth,
    TorchTensorView ref_depth,
    TorchTensorView render_normal,
    TorchTensorView depth_normal,
    TorchTensorView ref_normal,
    TorchTensorView render_Ts,
    TorchTensorView rgb_dist,
    TorchTensorView depth_dist,
    TorchTensorView normal_dist,
    TorchTensorView median_depth,
    TorchTensorView median_normal,
    TorchTensorView ref_alpha,
    bool has_mask,
    const std::array<float, (int)LossWeightIndex::length> loss_weights_0,
    const float w_ssim,
    TorchTensorView v_losses,
    std::vector<bool> needs_input_grad,
    long num_train_images,
    TorchTensorView camera_indices,
    TorchTensorView loss_map_out,
    int loss_map_mode,
    float robust_edge_aware_quantile,
    float nms_falloff,
    PerPixelGrads& grads_out
) {
    const auto _mode = densify_loss_map_base((DensifyLossMapMode)loss_map_mode);
    const bool _nms = densify_loss_map_has_nms((DensifyLossMapMode)loss_map_mode);
    // Per-pixel L1/L2/aux terms contribute to the loss map only for LossFull.
    const bool _per_pixel_write = (_mode == DensifyLossMapMode::LossFull);
    // SSIM kernel mode: skip its loss_map write entirely for None,
    // EdgeAware and RobustEdgeAware modes; otherwise dispatch to LCS / CS /
    // structure.
    const int _ssim_mode = [&]() -> int {
        switch (_mode) {
            case DensifyLossMapMode::LossFull:
            case DensifyLossMapMode::SsimFull:
                return (int)SsimLossMapMode::SsimFull;
            case DensifyLossMapMode::SsimContrastStruct:
                return (int)SsimLossMapMode::SsimCs;
            case DensifyLossMapMode::SsimStructure:
                return (int)SsimLossMapMode::SsimStr;
            default:
                return (int)SsimLossMapMode::SsimNone;
        }
    }();
    const auto& s = std::get<2>(render_rgb);
    long B = s[0], H = s[1], W = s[2];

    FixedArray<float, (uint)LossWeightIndex::length> loss_weights =
        *reinterpret_cast<const FixedArray<float, (uint)LossWeightIndex::length>*>(loss_weights_0.data());

    if (!_has(camera_indices))
        num_train_images = B;

    // Scale arrays: [scale] -> TorchTensorView (null if not used)
    constexpr int MAX_SCALES = 4;
    if (num_loss_scales > MAX_SCALES)
        throw std::runtime_error("num_loss_scales > MAX_SCALES");

    TorchTensorView render_rgb_s[MAX_SCALES], ref_rgb_s[MAX_SCALES];
    TorchTensorView render_depth_s[MAX_SCALES], ref_depth_s[MAX_SCALES];
    TorchTensorView render_normal_s[MAX_SCALES], depth_normal_s[MAX_SCALES], ref_normal_s[MAX_SCALES];
    TorchTensorView render_Ts_s[MAX_SCALES];
    TorchTensorView rgb_dist_s[MAX_SCALES], depth_dist_s[MAX_SCALES], normal_dist_s[MAX_SCALES];
    TorchTensorView median_depth_s[MAX_SCALES], median_normal_s[MAX_SCALES];
    TorchTensorView ref_alpha_s[MAX_SCALES];

    render_rgb_s[0] = render_rgb; ref_rgb_s[0] = ref_rgb;
    render_depth_s[0] = render_depth; ref_depth_s[0] = ref_depth;
    render_normal_s[0] = render_normal; depth_normal_s[0] = depth_normal; ref_normal_s[0] = ref_normal;
    render_Ts_s[0] = render_Ts;
    rgb_dist_s[0] = rgb_dist; depth_dist_s[0] = depth_dist; normal_dist_s[0] = normal_dist;
    median_depth_s[0] = median_depth; median_normal_s[0] = median_normal;
    ref_alpha_s[0] = ref_alpha;

    // Downsample to create scales. Each modality halves its OWN previous-scale
    // shape (rather than the render shape) so GT modalities at a different
    // resolution from the rendered output still produce a coherent per-scale
    // pyramid. The per-pixel loss kernel then bilinearly samples between the
    // render-scale grid and each modality's scale grid at each scale.
    for (int sc = 1; sc < num_loss_scales; ++sc) {
        std::string pfx = "ppl.s" + std::to_string(sc) + ".";

        auto ds_f = [&](TorchTensorView& prev, TorchTensorView& curr, const std::string& name, int C) {
            if (_has(prev)) {
                const auto& pps = std::get<2>(prev);
                long nH = std::max((long)1, (long)pps[1] / 2);
                long nW = std::max((long)1, (long)pps[2] / 2);
                curr = _pool_alloc_f(pfx + name, B, nH, nW, C);
                _avg_pool_downsample_float(prev, curr);
            }
        };
        auto ds_geo = [&](TorchTensorView& prev, TorchTensorView& curr, const std::string& name, int C) {
            if (_has(prev)) {
                const auto& pps = std::get<2>(prev);
                long nH = std::max((long)1, (long)pps[1] / 2);
                long nW = std::max((long)1, (long)pps[2] / 2);
                curr = _pool_alloc_f(pfx + name, B, nH, nW, C);
                _avg_pool_downsample_gt_geometry(prev, curr);
            }
        };
        auto ds_b = [&](TorchTensorView& prev, TorchTensorView& curr, const std::string& name) {
            if (_has(prev)) {
                const auto& pps = std::get<2>(prev);
                long nH = std::max((long)1, (long)pps[1] / 2);
                long nW = std::max((long)1, (long)pps[2] / 2);
                curr = _pool_alloc_b(pfx + name, B, nH, nW);
                _avg_pool_downsample_bool(prev, curr);
            }
        };

        // Everything the loss compares pools over the same unmasked children,
        // so a masked pixel reaches no coarse value -- which is what lets a
        // fully-masked tile go unrendered (docs/datasets.md, "Skipping tiles").
        auto ds_m = [&](TorchTensorView& prev, TorchTensorView& curr,
                        const std::string& name, int C) {
            if (!_has(prev)) return;
            const TorchTensorView& mk = ref_alpha_s[sc-1];
            const auto& pps = std::get<2>(prev);
            const auto& mks = std::get<2>(mk);
            // A mask at its own resolution cannot index this image's children.
            const bool same = _has(mk) && mks[0] == pps[0] &&
                              mks[1] == pps[1] && mks[2] == pps[2];
            if (!has_mask || !same) {
                ds_f(prev, curr, name, C);
                return;
            }
            curr = _pool_alloc_f(pfx + name, B, std::max((long)1, (long)pps[1] / 2),
                                 std::max((long)1, (long)pps[2] / 2), C);
            _avg_pool_downsample_masked_float(prev, mk, curr);
        };
        ds_m(render_rgb_s[sc-1], render_rgb_s[sc], "rrgb", 3);
        ds_m(ref_rgb_s[sc-1], ref_rgb_s[sc], "frgb", 3);
        ds_m(render_depth_s[sc-1], render_depth_s[sc], "rd", 1);
        ds_geo(ref_depth_s[sc-1], ref_depth_s[sc], "fd", 1);
        ds_m(render_normal_s[sc-1], render_normal_s[sc], "rn", 3);
        ds_m(depth_normal_s[sc-1], depth_normal_s[sc], "dn", 3);
        ds_geo(ref_normal_s[sc-1], ref_normal_s[sc], "fn", 3);
        ds_m(render_Ts_s[sc-1], render_Ts_s[sc], "rT", 1);
        ds_m(rgb_dist_s[sc-1], rgb_dist_s[sc], "rgbd", 3);
        ds_m(depth_dist_s[sc-1], depth_dist_s[sc], "dd", 1);
        ds_m(normal_dist_s[sc-1], normal_dist_s[sc], "nd", 3);
        ds_m(median_depth_s[sc-1], median_depth_s[sc], "md", 1);
        ds_m(median_normal_s[sc-1], median_normal_s[sc], "mn", 3);
        ds_b(ref_alpha_s[sc-1], ref_alpha_s[sc], "ra");
    }

    // Total losses accumulator -- pool-backed device buffer (D->H read at end)
    float* total_losses_ptr = DevicePool::global().acquire<float>(
        PoolSlot::PplTotalLosses, (uint)LossIndex::length);
    cudaMemset(total_losses_ptr, 0, (uint)LossIndex::length * sizeof(float));

    float ssim_val = 0.0f;

    for (int scale = 0; scale < num_loss_scales; ++scale) {
        const auto& ss = std::get<2>(render_rgb_s[scale]);
        long Hs = ss[1], Ws = ss[2];
        size_t ppi = (size_t)(Hs * Ws);

        // Forward losses
        float* raw_losses_ptr = DevicePool::global().acquire<float>(
            PoolSlot::PplRawLosses, (size_t)(num_train_images+1) * (uint)RawLossIndex::length);
        cudaMemset(raw_losses_ptr, 0, (size_t)(num_train_images+1) * (uint)RawLossIndex::length * sizeof(float));

        float* losses_ptr = DevicePool::global().acquire<float>(
            PoolSlot::PplLosses, (uint)LossIndex::length);
        cudaMemset(losses_ptr, 0, (uint)LossIndex::length * sizeof(float));

        float* loss_map_ptr = nullptr;
        TorchTensorView loss_map_scale = {};
        if (_has(loss_map_out)) {
            loss_map_scale = _pool_alloc_f_zero(PoolSlot::PplLossMapScale, B, Hs, Ws, 1);
            loss_map_ptr = _fptr(loss_map_scale);
        }

        // Per-pixel L1/L2/aux terms are folded into the loss map only when
        // mode is LossFull. For all other modes (ssim_*, edge_aware, none),
        // the per-pixel kernel still runs to compute raw_losses (used for
        // grad / display) but doesn't write into the loss map.
        float* per_pixel_loss_map_ptr = _per_pixel_write ? loss_map_ptr : nullptr;
        _compute_per_pixel_losses_forward(
            B, ppi, (int)Ws, (int)Hs,
            render_rgb_s[scale], ref_rgb_s[scale], render_depth_s[scale], ref_depth_s[scale],
            render_normal_s[scale], depth_normal_s[scale], ref_normal_s[scale], render_Ts_s[scale],
            rgb_dist_s[scale], depth_dist_s[scale], normal_dist_s[scale],
            median_depth_s[scale], median_normal_s[scale],
            ref_alpha_s[scale], has_mask,
            loss_weights, num_train_images, camera_indices,
            per_pixel_loss_map_ptr, raw_losses_ptr, losses_ptr
        );

        // Backward losses.
        //
        // Per-scale grad scratches: at scale 0 we ALIAS to grads_out so the loss
        // kernel writes directly into the caller's gradient buffer and we skip
        // a full-resolution scratch allocation per output field. At scale > 0
        // we allocate a smaller per-scale-keyed scratch (ppl.g.*.s<scale>) for
        // the kernel to write into; upsample_grad then accumulates it into
        // grads_out. The old single key "ppl.g.*" was forced to scale-0 size
        // by the pool's high-water-mark even when only the smaller scales
        // actually needed it.
        PerPixelGrads scale_grads = {};
        // Per-scale grad scratches use the INPUT's own shape (so GT modalities
        // at a different resolution from the render allocate at their own size,
        // not the render-scale's). At scale 0 we still alias to the caller's
        // grads_out buffer when provided (single contiguous write target).
        auto alloc_grad_f = [&](TorchTensorView& out, TorchTensorView grads_out_field,
                                bool need, TorchTensorView& input,
                                const std::string& name, int C) {
            if (!(need && _has(input))) return;
            if (scale == 0 && _has(grads_out_field)) {
                out = grads_out_field;  // alias: kernel writes straight to grads_out
            } else {
                const auto& is = std::get<2>(input);
                out = _pool_alloc_f("ppl.g." + name + ".s" + std::to_string(scale),
                                    is[0], is[1], is[2], C);
            }
        };
        alloc_grad_f(scale_grads.v_render_rgb, grads_out.v_render_rgb,     needs_input_grad[0],  render_rgb_s[scale],   "vrgb",  3);
        alloc_grad_f(scale_grads.v_ref_rgb,    grads_out.v_ref_rgb,        needs_input_grad[1],  ref_rgb_s[scale],      "vfrgb", 3);
        alloc_grad_f(scale_grads.v_render_depth, grads_out.v_render_depth, needs_input_grad[2],  render_depth_s[scale], "vrd",   1);
        alloc_grad_f(scale_grads.v_ref_depth,  grads_out.v_ref_depth,      needs_input_grad[3],  ref_depth_s[scale],    "vfd",   1);
        alloc_grad_f(scale_grads.v_render_normal, grads_out.v_render_normal, needs_input_grad[4],  render_normal_s[scale], "vrn",   3);
        alloc_grad_f(scale_grads.v_depth_normal, grads_out.v_depth_normal, needs_input_grad[5],  depth_normal_s[scale], "vdn",   3);
        alloc_grad_f(scale_grads.v_ref_normal, grads_out.v_ref_normal,     needs_input_grad[6],  ref_normal_s[scale],   "vfn",   3);
        alloc_grad_f(scale_grads.v_render_Ts,  grads_out.v_render_Ts,      needs_input_grad[7],  render_Ts_s[scale],    "vrT",   1);
        alloc_grad_f(scale_grads.v_rgb_dist,   grads_out.v_rgb_dist,       needs_input_grad[8],  rgb_dist_s[scale],     "vrgbd", 3);
        alloc_grad_f(scale_grads.v_depth_dist, grads_out.v_depth_dist,     needs_input_grad[9],  depth_dist_s[scale],   "vdd",   1);
        alloc_grad_f(scale_grads.v_normal_dist, grads_out.v_normal_dist,   needs_input_grad[10], normal_dist_s[scale],  "vnd",   3);
        bool need_md = needs_input_grad.size() > 11 && needs_input_grad[11];
        bool need_mn = needs_input_grad.size() > 12 && needs_input_grad[12];
        alloc_grad_f(scale_grads.v_median_depth, grads_out.v_median_depth, need_md, median_depth_s[scale], "vmd", 1);
        alloc_grad_f(scale_grads.v_median_normal, grads_out.v_median_normal, need_mn, median_normal_s[scale], "vmn", 3);

        // GT-resolution grads use atomicAdd inside the bwd kernel (one render
        // pixel may scatter into 4 GT taps), so the destination buffer must
        // start zeroed. Render-resolution grads (v_render_*) keep the direct
        // single-writer pattern and don't need pre-zeroing.
        auto zero_view = [](TorchTensorView tv) {
            if (_has(tv)) {
                const auto& sh = std::get<2>(tv);
                size_t n = (size_t)sh[0] * (size_t)sh[1] * (size_t)sh[2] * (size_t)sh[3];
                cudaMemsetAsync((void*)std::get<0>(tv), 0, n * sizeof(float));
            }
        };
        zero_view(scale_grads.v_ref_depth);
        zero_view(scale_grads.v_ref_normal);

        _compute_per_pixel_losses_backward(
            B, ppi, (int)Ws, (int)Hs,
            render_rgb_s[scale], ref_rgb_s[scale], render_depth_s[scale], ref_depth_s[scale],
            render_normal_s[scale], depth_normal_s[scale], ref_normal_s[scale], render_Ts_s[scale],
            rgb_dist_s[scale], depth_dist_s[scale], normal_dist_s[scale],
            median_depth_s[scale], median_normal_s[scale],
            ref_alpha_s[scale], has_mask,
            raw_losses_ptr, loss_weights, _fptr(v_losses),
            num_train_images, camera_indices, scale_grads
        );

        // SSIM backward (fused forward+backward). For scale==0 we want the
        // scalar SSIM for verbose display; route it through an async readout
        // so the C++ engine call doesn't block on cudaMemcpy.
        static AsyncReadout<float> ssim_readout(1);
        float ssim;
        if (scale == 0) {
            ssim = fused_ssim_inplace_async(
                render_rgb_s[scale], ref_rgb_s[scale], ref_alpha_s[scale],
                -w_ssim,
                scale_grads.v_render_rgb,
                loss_map_scale,
                w_ssim,
                _ssim_mode,
                ssim_readout
            );
        } else {
            ssim = fused_ssim_inplace(
                render_rgb_s[scale], ref_rgb_s[scale], ref_alpha_s[scale],
                -w_ssim,
                scale_grads.v_render_rgb,
                /*return_ssim_val=*/false,
                loss_map_scale,
                w_ssim,
                _ssim_mode
            );
        }

        // Edge-aware loss maps. The SSIM kernel above skipped its loss-map
        // write via _ssim_mode = SsimNone for both EdgeAware modes, and the
        // per-pixel kernel skipped its write via _per_pixel_write = false,
        // so loss_map_scale is still zero and the kernels below overwrite
        // it cleanly. Multi-scale handled by the outer accumulator
        // (avg_pool_upsample_float_kernel) the same way as the SSIM /
        // per-pixel loss maps.
        if (_has(loss_map_scale)) {
            if (_mode == DensifyLossMapMode::EdgeAware) {
                // Plenoxels-style: canny edge magnitude of GT rgb directly.
                // No residual-awareness -- biases densification toward GT
                // structure regardless of how well the splats already
                // reconstruct it.
                canny_edge_filter_tensor(
                    DeviceTensor3D<float3>(ref_rgb_s[scale]),
                    /*mask_in_ptr=*/_bptr(ref_alpha_s[scale]),
                    DeviceTensor3D<float>(loss_map_scale)
                );
                CHECK_DEVICE_ERROR(cudaGetLastError());
            } else if (_mode == DensifyLossMapMode::RobustEdgeAware) {
                // RobustNeRF-style: Tukey biweight on |render - GT| (luma)
                // capped at the per-image q-quantile, then canny. Near-zero
                // for well-reconstructed regions, zeroed past the cutoff
                // so distractor pixels don't pull splats toward them.
                robust_canny_residual_tensor(
                    DeviceTensor3D<float3>(render_rgb_s[scale]),
                    DeviceTensor3D<float3>(ref_rgb_s[scale]),
                    /*mask_in_ptr=*/_bptr(ref_alpha_s[scale]),
                    robust_edge_aware_quantile,
                    DeviceTensor3D<float>(loss_map_scale)
                );
                CHECK_DEVICE_ERROR(cudaGetLastError());
            }
        }

        // Thin the finished map before the pyramid folds it in, so each
        // scale is suppressed against its own neighbourhood.
        if (_nms && _has(loss_map_scale))
            _loss_map_nms_inplace(loss_map_scale,
                                  "ppl.nms.s" + std::to_string(scale),
                                  nms_falloff);

        if (scale == 0)
            ssim_val = ssim;

        // Accumulate losses on device: total_losses += losses
        constexpr int N_LOSSES = (int)LossIndex::length;
        vector_add_scaled_kernel<<<1, N_LOSSES>>>(
            total_losses_ptr, losses_ptr, 1.0f, N_LOSSES);
        CHECK_DEVICE_ERROR(cudaGetLastError());

        // Upsample loss map
        if (_has(loss_map_out) && loss_map_ptr) {
            if (scale == 0) {
                cudaMemcpy(_fptr(loss_map_out), loss_map_ptr, B * H * W * sizeof(float), cudaMemcpyDeviceToDevice);
            } else {
                avg_pool_upsample_float_kernel<<<_LAUNCH_ARGS_3D(W, H, B, 16, 16, 1)>>>(
                    _make_view4(_fptr(loss_map_out), B, H, W, 1L),
                    _make_view4(loss_map_ptr, B, Hs, Ws, 1L),
                    1 << scale,
                    scale == 1 ? 1.0f / num_loss_scales : 1.0f,
                    1.0f / num_loss_scales
                );
                CHECK_DEVICE_ERROR(cudaGetLastError());
            }
        }

        // Upsample gradients and accumulate into grads_out.
        // At scale 0 the per-scale buffer aliases grads_out (see alloc_grad_f
        // above), so the copy becomes a self-copy -- skip it via pointer check.
        //
        // Each modality's grad shape comes from its own view (NOT from outer
        // render H/W), because GT modalities (v_ref_depth, v_ref_normal) live
        // at the GT's resolution, which may differ from render. The src view
        // is the per-scale scratch (downsampled by 2^scale relative to its
        // own modality), the dst view is the full-resolution accumulator.
        auto upsample_grad = [&](TorchTensorView& grad_scale, TorchTensorView& grad_acc, int C) {
            if (_has(grad_acc) && scale == 0) {
                if (_fptr(grad_scale) == _fptr(grad_acc)) return;  // aliased
                const auto& ds = std::get<2>(grad_acc);
                long dB = ds[0], dH = ds[1], dW = ds[2];
                cudaMemcpy(_fptr(grad_acc), _fptr(grad_scale),
                           (size_t)dB * dH * dW * C * sizeof(float),
                           cudaMemcpyDeviceToDevice);
                return;
            }
            if (_has(grad_scale) && _has(grad_acc)) {
                const auto& ds = std::get<2>(grad_acc);    // full-res dst
                const auto& ss = std::get<2>(grad_scale);  // scale-s src
                long dB = ds[0], dH = ds[1], dW = ds[2];
                long sH = ss[1], sW = ss[2];
                float a = (scale == 1 ? 1.0f / num_loss_scales : 1.0f);
                float b = powf(0.25f, (float)scale) / num_loss_scales;
                avg_pool_upsample_float_kernel<<<_LAUNCH_ARGS_3D(dW, dH, dB, 16, 16, 1)>>>(
                    _make_view4(_fptr(grad_acc),   dB, dH, dW, (long)C),
                    _make_view4(_fptr(grad_scale), dB, sH, sW, (long)C),
                    1 << scale, a, b
                );
                CHECK_DEVICE_ERROR(cudaGetLastError());
            }
        };

        upsample_grad(scale_grads.v_render_rgb, grads_out.v_render_rgb, 3);
        upsample_grad(scale_grads.v_ref_rgb, grads_out.v_ref_rgb, 3);
        upsample_grad(scale_grads.v_render_depth, grads_out.v_render_depth, 1);
        upsample_grad(scale_grads.v_ref_depth, grads_out.v_ref_depth, 1);
        upsample_grad(scale_grads.v_render_normal, grads_out.v_render_normal, 3);
        upsample_grad(scale_grads.v_depth_normal, grads_out.v_depth_normal, 3);
        upsample_grad(scale_grads.v_ref_normal, grads_out.v_ref_normal, 3);
        upsample_grad(scale_grads.v_render_Ts, grads_out.v_render_Ts, 1);
        upsample_grad(scale_grads.v_rgb_dist, grads_out.v_rgb_dist, 3);
        upsample_grad(scale_grads.v_depth_dist, grads_out.v_depth_dist, 1);
        upsample_grad(scale_grads.v_normal_dist, grads_out.v_normal_dist, 3);
        upsample_grad(scale_grads.v_median_depth, grads_out.v_median_depth, 1);
        upsample_grad(scale_grads.v_median_normal, grads_out.v_median_normal, 3);
    }

    // Scale 0 already leaves masked pixels at zero gradient; only the
    // upsampled coarse scales put anything there.
    if (num_loss_scales > 1 && has_mask && _has(grads_out.v_render_rgb) &&
            _has(ref_alpha)) {
        const auto& gs = std::get<2>(grads_out.v_render_rgb);
        const auto& ms = std::get<2>(ref_alpha);
        if (gs[0] == ms[0] && gs[1] == ms[1] && gs[2] == ms[2])
            _zero_masked_grad(grads_out.v_render_rgb, ref_alpha);
    }

    // Scale total losses by 1/num_scales (device-side)
    if (num_loss_scales > 1) {
        constexpr int N_LOSSES = (int)LossIndex::length;
        vector_add_scaled_kernel<<<1, N_LOSSES>>>(
            total_losses_ptr, total_losses_ptr,
            1.0f / (float)num_loss_scales - 1.0f, N_LOSSES);
        CHECK_DEVICE_ERROR(cudaGetLastError());
    }

    // Async D->H of accumulated losses: read the PREVIOUS iter's slot now
    // (event sync is ~free because the prior async copy completed long ago),
    // then queue this iter's async copy. The verbose display is one iter
    // behind, which is invisible at ms-scale ticks; in exchange, the C++
    // engine call no longer blocks the host on a per-step `cudaMemcpy`.
    static AsyncReadout<float> losses_readout((int)LossIndex::length);
    const float* h_prev = losses_readout.read_previous();

    LossValues out = {};
    if (h_prev) {
        out.rgb_loss        = h_prev[(int)LossIndex::RgbLoss];
        out.rgb_psnr        = h_prev[(int)LossIndex::RgbPSNR];
        out.depth_sup       = h_prev[(int)LossIndex::DepthSup];
        out.normal_sup      = h_prev[(int)LossIndex::NormalSup];
        out.alpha_sup       = h_prev[(int)LossIndex::AlphaSup];
        out.normal_reg      = h_prev[(int)LossIndex::NormalReg];
        out.alpha_reg       = h_prev[(int)LossIndex::AlphaReg];
        out.rgb_dist_reg    = h_prev[(int)LossIndex::RgbDistReg];
        out.depth_dist_reg  = h_prev[(int)LossIndex::DepthDistReg];
        out.normal_dist_reg = h_prev[(int)LossIndex::NormalDistReg];
        out.mean_median_depth_sup    = h_prev[(int)LossIndex::MeanMedianDepthSup];
        out.median_depth_normal_reg  = h_prev[(int)LossIndex::MedianDepthNormalReg];
        out.median_normal_sup        = h_prev[(int)LossIndex::MedianNormalSup];
        out.median_render_normal_reg = h_prev[(int)LossIndex::MedianRenderNormalReg];
    }
    out.ssim = ssim_val;
    losses_readout.issue(total_losses_ptr);
    return out;
}

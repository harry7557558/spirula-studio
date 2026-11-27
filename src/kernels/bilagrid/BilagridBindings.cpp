// Tensor-view wrappers for the bilagrid CUDA launchers, ported from
// fused-bilagrid/fused_bilagrid/bindings.h. Replaces the original torch::Tensor
// wrappers with TorchTensorView; outputs are caller-allocated.
//
// Quantile-based depth-scalar computation reuses the engine's existing
// batch_quantile_masked_radix_select from Quantile.cu (declared inline below).

#include "kernels/bilagrid/BilagridBindings.h"

#include <cstdint>
#include <stdexcept>


// Quantile selector (defined in Quantile.cu)
template<bool invert_quantile>
int batch_quantile_masked_radix_select(
    const float* d_x,
    int B,
    int N,
    float q,
    float* d_out,
    uint32_t* temp,
    backend::Stream stream
);


static inline float* _tv_f(const TorchTensorView& tv) {
    return (float*)std::get<0>(tv);
}
static inline const float* _tv_cf(const TorchTensorView& tv) {
    return (const float*)std::get<0>(tv);
}
static inline int* _tv_i(const TorchTensorView& tv) {
    return (int*)std::get<0>(tv);
}
static inline bool _tv_valid(const TorchTensorView& tv) {
    return std::get<0>(tv) != 0;
}
static inline int64_t _tv_dim(const TorchTensorView& tv, int i) {
    return std::get<2>(tv)[i];
}
static inline int64_t _tv_numel(const TorchTensorView& tv) {
    int64_t n = 1;
    for (auto d : std::get<2>(tv)) n *= d;
    return n;
}

constexpr backend::Stream kStream = backend::kDefaultStream;


// ============================================================================
// Sample (with coords)
// ============================================================================

void bilagrid_sample_forward_tensor(
    TorchTensorView bilagrid, TorchTensorView coords, TorchTensorView rgb,
    TorchTensorView output
) {
    int N = (int)_tv_dim(bilagrid, 0), L = (int)_tv_dim(bilagrid, 2),
        H = (int)_tv_dim(bilagrid, 3), W = (int)_tv_dim(bilagrid, 4);
    int m = (int)_tv_dim(coords, 1), h = (int)_tv_dim(coords, 2), w = (int)_tv_dim(coords, 3);
    bilagrid_sample_forward(
        _tv_cf(bilagrid), _tv_cf(coords), _tv_cf(rgb), _tv_f(output),
        N, L, H, W, m, h, w, kStream);
}

void bilagrid_sample_backward_tensor(
    TorchTensorView bilagrid, TorchTensorView coords, TorchTensorView rgb,
    TorchTensorView v_output,
    TorchTensorView v_bilagrid, TorchTensorView v_coords, TorchTensorView v_rgb
) {
    int N = (int)_tv_dim(bilagrid, 0), L = (int)_tv_dim(bilagrid, 2),
        H = (int)_tv_dim(bilagrid, 3), W = (int)_tv_dim(bilagrid, 4);
    int m = (int)_tv_dim(coords, 1), h = (int)_tv_dim(coords, 2), w = (int)_tv_dim(coords, 3);
    bilagrid_sample_backward(
        _tv_cf(bilagrid), _tv_cf(coords), _tv_cf(rgb), _tv_cf(v_output),
        _tv_f(v_bilagrid),
        _tv_valid(v_coords) ? _tv_f(v_coords) : nullptr,
        _tv_f(v_rgb),
        N, L, H, W, m, h, w, kStream);
}


// ============================================================================
// PPISP sample
// ============================================================================

void bilagrid_ppisp_sample_forward_tensor(
    TorchTensorView bilagrid, TorchTensorView coords, TorchTensorView rgb,
    TorchTensorView output
) {
    int N = (int)_tv_dim(bilagrid, 0), L = (int)_tv_dim(bilagrid, 2),
        H = (int)_tv_dim(bilagrid, 3), W = (int)_tv_dim(bilagrid, 4);
    int m = (int)_tv_dim(coords, 1), h = (int)_tv_dim(coords, 2), w = (int)_tv_dim(coords, 3);
    bilagrid_ppisp_sample_forward(
        _tv_cf(bilagrid), _tv_cf(coords), _tv_cf(rgb), _tv_f(output),
        N, L, H, W, m, h, w, kStream);
}

void bilagrid_ppisp_sample_backward_tensor(
    TorchTensorView bilagrid, TorchTensorView coords, TorchTensorView rgb,
    TorchTensorView v_output,
    TorchTensorView v_bilagrid, TorchTensorView v_coords, TorchTensorView v_rgb
) {
    int N = (int)_tv_dim(bilagrid, 0), L = (int)_tv_dim(bilagrid, 2),
        H = (int)_tv_dim(bilagrid, 3), W = (int)_tv_dim(bilagrid, 4);
    int m = (int)_tv_dim(coords, 1), h = (int)_tv_dim(coords, 2), w = (int)_tv_dim(coords, 3);
    bilagrid_ppisp_sample_backward(
        _tv_cf(bilagrid), _tv_cf(coords), _tv_cf(rgb), _tv_cf(v_output),
        _tv_f(v_bilagrid),
        _tv_valid(v_coords) ? _tv_f(v_coords) : nullptr,
        _tv_f(v_rgb),
        N, L, H, W, m, h, w, kStream);
}


// ============================================================================
// PPISP packed sample
// ============================================================================

void bilagrid_ppisp_packed_sample_forward_tensor(
    TorchTensorView bilagrid, TorchTensorView image_indices,
    TorchTensorView coords, TorchTensorView rgb, TorchTensorView output
) {
    int N = (int)_tv_dim(bilagrid, 0), L = (int)_tv_dim(bilagrid, 2),
        H = (int)_tv_dim(bilagrid, 3), W = (int)_tv_dim(bilagrid, 4);
    int nnz = (int)_tv_numel(image_indices);
    bilagrid_ppisp_packed_sample_forward(
        _tv_cf(bilagrid), (const int64_t*)std::get<0>(image_indices),
        _tv_cf(coords), _tv_cf(rgb), _tv_f(output),
        N, L, H, W, nnz, kStream);
}

void bilagrid_ppisp_packed_sample_backward_tensor(
    TorchTensorView bilagrid, TorchTensorView image_indices,
    TorchTensorView coords, TorchTensorView rgb, TorchTensorView v_output,
    TorchTensorView v_bilagrid, TorchTensorView v_coords, TorchTensorView v_rgb
) {
    int N = (int)_tv_dim(bilagrid, 0), L = (int)_tv_dim(bilagrid, 2),
        H = (int)_tv_dim(bilagrid, 3), W = (int)_tv_dim(bilagrid, 4);
    int nnz = (int)_tv_numel(image_indices);
    bilagrid_ppisp_packed_sample_backward(
        _tv_cf(bilagrid), (const int64_t*)std::get<0>(image_indices),
        _tv_cf(coords), _tv_cf(rgb), _tv_cf(v_output),
        _tv_f(v_bilagrid),
        _tv_valid(v_coords) ? _tv_f(v_coords) : nullptr,
        _tv_f(v_rgb),
        N, L, H, W, nnz, kStream);
}


// ============================================================================
// Uniform sample (no coords)
// ============================================================================

void bilagrid_uniform_sample_forward_tensor(
    TorchTensorView bilagrid, TorchTensorView rgb, TorchTensorView output
) {
    int N = (int)_tv_dim(bilagrid, 0), L = (int)_tv_dim(bilagrid, 2),
        H = (int)_tv_dim(bilagrid, 3), W = (int)_tv_dim(bilagrid, 4);
    int h = (int)_tv_dim(rgb, 2), w = (int)_tv_dim(rgb, 3);
    bilagrid_uniform_sample_forward(
        _tv_cf(bilagrid), _tv_cf(rgb), _tv_f(output),
        N, L, H, W, h, w, kStream);
}

void bilagrid_uniform_sample_backward_tensor(
    TorchTensorView bilagrid, TorchTensorView rgb, TorchTensorView v_output,
    TorchTensorView v_bilagrid, TorchTensorView v_rgb,
    int version, int target_tile_size
) {
    int N = (int)_tv_dim(bilagrid, 0), L = (int)_tv_dim(bilagrid, 2),
        H = (int)_tv_dim(bilagrid, 3), W = (int)_tv_dim(bilagrid, 4);
    int h = (int)_tv_dim(rgb, 2), w = (int)_tv_dim(rgb, 3);
    if (version == 1) {
        bilagrid_uniform_sample_backward_v1(
            _tv_cf(bilagrid), _tv_cf(rgb), _tv_cf(v_output),
            _tv_f(v_bilagrid), _tv_f(v_rgb),
            N, L, H, W, h, w,
            target_tile_size, kStream);
    } else if (version == 2) {
        bilagrid_uniform_sample_backward_v2(
            _tv_cf(bilagrid), _tv_cf(rgb), _tv_cf(v_output),
            _tv_f(v_bilagrid), _tv_f(v_rgb),
            N, L, H, W, h, w, kStream);
    }
}

void bilagrid_ppisp_uniform_sample_forward_tensor(
    TorchTensorView bilagrid, TorchTensorView rgb, TorchTensorView output
) {
    int N = (int)_tv_dim(bilagrid, 0), L = (int)_tv_dim(bilagrid, 2),
        H = (int)_tv_dim(bilagrid, 3), W = (int)_tv_dim(bilagrid, 4);
    int h = (int)_tv_dim(rgb, 2), w = (int)_tv_dim(rgb, 3);
    bilagrid_ppisp_uniform_sample_forward(
        _tv_cf(bilagrid), _tv_cf(rgb), _tv_f(output),
        N, L, H, W, h, w, kStream);
}

void bilagrid_ppisp_uniform_sample_backward_tensor(
    TorchTensorView bilagrid, TorchTensorView rgb, TorchTensorView v_output,
    TorchTensorView v_bilagrid, TorchTensorView v_rgb,
    int version, int target_tile_size
) {
    int N = (int)_tv_dim(bilagrid, 0), L = (int)_tv_dim(bilagrid, 2),
        H = (int)_tv_dim(bilagrid, 3), W = (int)_tv_dim(bilagrid, 4);
    int h = (int)_tv_dim(rgb, 2), w = (int)_tv_dim(rgb, 3);
    if (version == 1 || version == 2) {
        bilagrid_ppisp_uniform_sample_backward_v1(
            _tv_cf(bilagrid), _tv_cf(rgb), _tv_cf(v_output),
            _tv_f(v_bilagrid), _tv_f(v_rgb),
            N, L, H, W, h, w,
            target_tile_size, kStream);
    }
}


void bilagrid_loglinear_uniform_sample_forward_tensor(
    TorchTensorView bilagrid, TorchTensorView rgb, TorchTensorView output
) {
    int N = (int)_tv_dim(bilagrid, 0), L = (int)_tv_dim(bilagrid, 2),
        H = (int)_tv_dim(bilagrid, 3), W = (int)_tv_dim(bilagrid, 4);
    int h = (int)_tv_dim(rgb, 2), w = (int)_tv_dim(rgb, 3);
    bilagrid_loglinear_uniform_sample_forward(
        _tv_cf(bilagrid), _tv_cf(rgb), _tv_f(output),
        N, L, H, W, h, w, kStream);
}

void bilagrid_loglinear_uniform_sample_backward_tensor(
    TorchTensorView bilagrid, TorchTensorView rgb, TorchTensorView v_output,
    TorchTensorView v_bilagrid, TorchTensorView v_rgb,
    int version, int target_tile_size
) {
    int N = (int)_tv_dim(bilagrid, 0), L = (int)_tv_dim(bilagrid, 2),
        H = (int)_tv_dim(bilagrid, 3), W = (int)_tv_dim(bilagrid, 4);
    int h = (int)_tv_dim(rgb, 2), w = (int)_tv_dim(rgb, 3);
    if (version == 1 || version == 2) {
        bilagrid_loglinear_uniform_sample_backward_v1(
            _tv_cf(bilagrid), _tv_cf(rgb), _tv_cf(v_output),
            _tv_f(v_bilagrid), _tv_f(v_rgb),
            N, L, H, W, h, w,
            target_tile_size, kStream);
    }
}

void compute_depth_scalars_tensor(
    TorchTensorView depth, bool patched, TorchTensorView output
) {
    int B = patched ? 1 : (int)_tv_dim(depth, 0);
    int N = (int)(_tv_numel(depth) / B);
    uint32_t* temp = DevicePool::global().acquire<uint32_t>(
        PoolSlot::BilagridQuantileTemp, (size_t)((256+5)*B));
    batch_quantile_masked_radix_select<true>(
        _tv_cf(depth), B, N, 0.5f, _tv_f(output), temp, kStream);
}

void bilagrid_depth_uniform_sample_forward_tensor(
    TorchTensorView bilagrid, TorchTensorView depth, TorchTensorView scalars,
    TorchTensorView output
) {
    int N = (int)_tv_dim(bilagrid, 0), L = (int)_tv_dim(bilagrid, 2),
        H = (int)_tv_dim(bilagrid, 3), W = (int)_tv_dim(bilagrid, 4);
    int h = (int)_tv_dim(depth, 2), w = (int)_tv_dim(depth, 3);
    bilagrid_depth_uniform_sample_forward(
        _tv_cf(bilagrid), _tv_cf(depth), _tv_cf(scalars), _tv_f(output),
        N, L, H, W, h, w, kStream);
}

void bilagrid_depth_uniform_sample_backward_tensor(
    TorchTensorView bilagrid, TorchTensorView depth, TorchTensorView scalars,
    TorchTensorView v_output,
    TorchTensorView v_bilagrid, TorchTensorView v_depth,
    int version, int target_tile_size
) {
    int N = (int)_tv_dim(bilagrid, 0), L = (int)_tv_dim(bilagrid, 2),
        H = (int)_tv_dim(bilagrid, 3), W = (int)_tv_dim(bilagrid, 4);
    int h = (int)_tv_dim(depth, 2), w = (int)_tv_dim(depth, 3);
    if (version == 1 || version == 2) {
        bilagrid_depth_uniform_sample_backward_v1(
            _tv_cf(bilagrid), _tv_cf(depth), _tv_cf(scalars), _tv_cf(v_output),
            _tv_f(v_bilagrid), _tv_f(v_depth),
            N, L, H, W, h, w,
            target_tile_size, kStream);
    }
}

void bilagrid_normal_uniform_sample_forward_tensor(
    TorchTensorView bilagrid, TorchTensorView rgb, TorchTensorView output
) {
    int N = (int)_tv_dim(bilagrid, 0), L = (int)_tv_dim(bilagrid, 2),
        H = (int)_tv_dim(bilagrid, 3), W = (int)_tv_dim(bilagrid, 4);
    int h = (int)_tv_dim(rgb, 2), w = (int)_tv_dim(rgb, 3);
    bilagrid_normal_uniform_sample_forward(
        _tv_cf(bilagrid), _tv_cf(rgb), _tv_f(output),
        N, L, H, W, h, w, kStream);
}

void bilagrid_normal_uniform_sample_backward_tensor(
    TorchTensorView bilagrid, TorchTensorView rgb, TorchTensorView v_output,
    TorchTensorView v_bilagrid, TorchTensorView v_rgb,
    int version, int target_tile_size
) {
    int N = (int)_tv_dim(bilagrid, 0), L = (int)_tv_dim(bilagrid, 2),
        H = (int)_tv_dim(bilagrid, 3), W = (int)_tv_dim(bilagrid, 4);
    int h = (int)_tv_dim(rgb, 2), w = (int)_tv_dim(rgb, 3);
    if (version == 1 || version == 2) {
        bilagrid_normal_uniform_sample_backward_v1(
            _tv_cf(bilagrid), _tv_cf(rgb), _tv_cf(v_output),
            _tv_f(v_bilagrid), _tv_f(v_rgb),
            N, L, H, W, h, w,
            target_tile_size, kStream);
    }
}


// ============================================================================
// Patched sample (no coords, uses per-image offsets)
// ============================================================================

void bilagrid_patched_sample_forward_tensor(
    TorchTensorView bilagrid, TorchTensorView rgb,
    int h0, int w0, TorchTensorView offsets, TorchTensorView output
) {
    int N = (int)_tv_dim(bilagrid, 0), L = (int)_tv_dim(bilagrid, 2),
        H = (int)_tv_dim(bilagrid, 3), W = (int)_tv_dim(bilagrid, 4);
    int m = (int)_tv_dim(rgb, 1), h = (int)_tv_dim(rgb, 2), w = (int)_tv_dim(rgb, 3);
    bilagrid_patched_sample_forward(
        _tv_cf(bilagrid), _tv_cf(rgb), _tv_i(offsets), _tv_f(output),
        N, L, H, W, m, h, w, h0, w0, kStream);
}

void bilagrid_patched_sample_backward_tensor(
    TorchTensorView bilagrid, TorchTensorView rgb,
    int h0, int w0, TorchTensorView offsets, TorchTensorView v_output,
    TorchTensorView v_bilagrid, TorchTensorView v_rgb
) {
    int N = (int)_tv_dim(bilagrid, 0), L = (int)_tv_dim(bilagrid, 2),
        H = (int)_tv_dim(bilagrid, 3), W = (int)_tv_dim(bilagrid, 4);
    int m = (int)_tv_dim(rgb, 1), h = (int)_tv_dim(rgb, 2), w = (int)_tv_dim(rgb, 3);
    // Match bindings.h: defaults to v2 (v1 is slow for patched sample)
    bilagrid_patched_sample_backward_v2(
        _tv_cf(bilagrid), _tv_cf(rgb), _tv_i(offsets), _tv_cf(v_output),
        _tv_f(v_bilagrid), _tv_f(v_rgb),
        N, L, H, W, m, h, w, h0, w0, kStream);
}

void bilagrid_ppisp_patched_sample_forward_tensor(
    TorchTensorView bilagrid, TorchTensorView rgb,
    int h0, int w0, TorchTensorView offsets, TorchTensorView output
) {
    int N = (int)_tv_dim(bilagrid, 0), L = (int)_tv_dim(bilagrid, 2),
        H = (int)_tv_dim(bilagrid, 3), W = (int)_tv_dim(bilagrid, 4);
    int m = (int)_tv_dim(rgb, 1), h = (int)_tv_dim(rgb, 2), w = (int)_tv_dim(rgb, 3);
    bilagrid_ppisp_patched_sample_forward(
        _tv_cf(bilagrid), _tv_cf(rgb), _tv_i(offsets), _tv_f(output),
        N, L, H, W, m, h, w, h0, w0, kStream);
}

void bilagrid_ppisp_patched_sample_backward_tensor(
    TorchTensorView bilagrid, TorchTensorView rgb,
    int h0, int w0, TorchTensorView offsets, TorchTensorView v_output,
    TorchTensorView v_bilagrid, TorchTensorView v_rgb
) {
    int N = (int)_tv_dim(bilagrid, 0), L = (int)_tv_dim(bilagrid, 2),
        H = (int)_tv_dim(bilagrid, 3), W = (int)_tv_dim(bilagrid, 4);
    int m = (int)_tv_dim(rgb, 1), h = (int)_tv_dim(rgb, 2), w = (int)_tv_dim(rgb, 3);
    // Match bindings.h's choice for ppisp patched
    bilagrid_ppisp_patched_sample_backward_v1(
        _tv_cf(bilagrid), _tv_cf(rgb), _tv_i(offsets), _tv_cf(v_output),
        _tv_f(v_bilagrid), _tv_f(v_rgb),
        N, L, H, W, m, h, w, h0, w0,
        4, 1, kStream);
}

void bilagrid_loglinear_patched_sample_forward_tensor(
    TorchTensorView bilagrid, TorchTensorView rgb,
    int h0, int w0, TorchTensorView offsets, TorchTensorView output
) {
    int N = (int)_tv_dim(bilagrid, 0), L = (int)_tv_dim(bilagrid, 2),
        H = (int)_tv_dim(bilagrid, 3), W = (int)_tv_dim(bilagrid, 4);
    int m = (int)_tv_dim(rgb, 1), h = (int)_tv_dim(rgb, 2), w = (int)_tv_dim(rgb, 3);
    bilagrid_loglinear_patched_sample_forward(
        _tv_cf(bilagrid), _tv_cf(rgb), _tv_i(offsets), _tv_f(output),
        N, L, H, W, m, h, w, h0, w0, kStream);
}

void bilagrid_loglinear_patched_sample_backward_tensor(
    TorchTensorView bilagrid, TorchTensorView rgb,
    int h0, int w0, TorchTensorView offsets, TorchTensorView v_output,
    TorchTensorView v_bilagrid, TorchTensorView v_rgb
) {
    int N = (int)_tv_dim(bilagrid, 0), L = (int)_tv_dim(bilagrid, 2),
        H = (int)_tv_dim(bilagrid, 3), W = (int)_tv_dim(bilagrid, 4);
    int m = (int)_tv_dim(rgb, 1), h = (int)_tv_dim(rgb, 2), w = (int)_tv_dim(rgb, 3);
    bilagrid_loglinear_patched_sample_backward_v1(
        _tv_cf(bilagrid), _tv_cf(rgb), _tv_i(offsets), _tv_cf(v_output),
        _tv_f(v_bilagrid), _tv_f(v_rgb),
        N, L, H, W, m, h, w, h0, w0,
        4, 1, kStream);
}

void bilagrid_depth_patched_sample_forward_tensor(
    TorchTensorView bilagrid, TorchTensorView depth, TorchTensorView scalars,
    int h0, int w0, TorchTensorView offsets, TorchTensorView output
) {
    int N = (int)_tv_dim(bilagrid, 0), L = (int)_tv_dim(bilagrid, 2),
        H = (int)_tv_dim(bilagrid, 3), W = (int)_tv_dim(bilagrid, 4);
    int m = (int)_tv_dim(depth, 1), h = (int)_tv_dim(depth, 2), w = (int)_tv_dim(depth, 3);
    bilagrid_depth_patched_sample_forward(
        _tv_cf(bilagrid), _tv_cf(depth), _tv_cf(scalars), _tv_i(offsets), _tv_f(output),
        N, L, H, W, m, h, w, h0, w0, kStream);
}

void bilagrid_depth_patched_sample_backward_tensor(
    TorchTensorView bilagrid, TorchTensorView depth, TorchTensorView scalars,
    int h0, int w0, TorchTensorView offsets, TorchTensorView v_output,
    TorchTensorView v_bilagrid, TorchTensorView v_depth
) {
    int N = (int)_tv_dim(bilagrid, 0), L = (int)_tv_dim(bilagrid, 2),
        H = (int)_tv_dim(bilagrid, 3), W = (int)_tv_dim(bilagrid, 4);
    int m = (int)_tv_dim(depth, 1), h = (int)_tv_dim(depth, 2), w = (int)_tv_dim(depth, 3);
    bilagrid_depth_patched_sample_backward_v1(
        _tv_cf(bilagrid), _tv_cf(depth), _tv_cf(scalars), _tv_i(offsets), _tv_cf(v_output),
        _tv_f(v_bilagrid), _tv_f(v_depth),
        N, L, H, W, m, h, w, h0, w0,
        4, 1, kStream);
}

void bilagrid_normal_patched_sample_forward_tensor(
    TorchTensorView bilagrid, TorchTensorView rgb,
    int h0, int w0, TorchTensorView offsets, TorchTensorView output
) {
    int N = (int)_tv_dim(bilagrid, 0), L = (int)_tv_dim(bilagrid, 2),
        H = (int)_tv_dim(bilagrid, 3), W = (int)_tv_dim(bilagrid, 4);
    int m = (int)_tv_dim(rgb, 1), h = (int)_tv_dim(rgb, 2), w = (int)_tv_dim(rgb, 3);
    bilagrid_normal_patched_sample_forward(
        _tv_cf(bilagrid), _tv_cf(rgb), _tv_i(offsets), _tv_f(output),
        N, L, H, W, m, h, w, h0, w0, kStream);
}

void bilagrid_normal_patched_sample_backward_tensor(
    TorchTensorView bilagrid, TorchTensorView rgb,
    int h0, int w0, TorchTensorView offsets, TorchTensorView v_output,
    TorchTensorView v_bilagrid, TorchTensorView v_rgb
) {
    int N = (int)_tv_dim(bilagrid, 0), L = (int)_tv_dim(bilagrid, 2),
        H = (int)_tv_dim(bilagrid, 3), W = (int)_tv_dim(bilagrid, 4);
    int m = (int)_tv_dim(rgb, 1), h = (int)_tv_dim(rgb, 2), w = (int)_tv_dim(rgb, 3);
    bilagrid_normal_patched_sample_backward_v1(
        _tv_cf(bilagrid), _tv_cf(rgb), _tv_i(offsets), _tv_cf(v_output),
        _tv_f(v_bilagrid), _tv_f(v_rgb),
        N, L, H, W, m, h, w, h0, w0,
        4, 1, kStream);
}


// ============================================================================
// TV loss and channel mean
// ============================================================================

void tv_loss_forward_tensor(TorchTensorView bilagrid, TorchTensorView tv_loss) {
    int N = (int)_tv_dim(bilagrid, 0), C = (int)_tv_dim(bilagrid, 1),
        L = (int)_tv_dim(bilagrid, 2), H = (int)_tv_dim(bilagrid, 3),
        W = (int)_tv_dim(bilagrid, 4);
    tv_loss_forward(_tv_cf(bilagrid), _tv_f(tv_loss), N, C, L, H, W, kStream);
}

void tv_loss_backward_tensor(
    TorchTensorView bilagrid, float v_tv_loss,
    TorchTensorView v_bilagrid, bool inplace
) {
    int N = (int)_tv_dim(bilagrid, 0), C = (int)_tv_dim(bilagrid, 1),
        L = (int)_tv_dim(bilagrid, 2), H = (int)_tv_dim(bilagrid, 3),
        W = (int)_tv_dim(bilagrid, 4);
    tv_loss_backward(
        _tv_cf(bilagrid), v_tv_loss, _tv_f(v_bilagrid),
        N, C, L, H, W, inplace, kStream);
}

void channel_mean_forward_tensor(TorchTensorView bilagrid, TorchTensorView channel_mean) {
    int N = (int)_tv_dim(bilagrid, 0), C = (int)_tv_dim(bilagrid, 1),
        L = (int)_tv_dim(bilagrid, 2), H = (int)_tv_dim(bilagrid, 3),
        W = (int)_tv_dim(bilagrid, 4);
    channel_mean_forward(_tv_cf(bilagrid), _tv_f(channel_mean), N, C, L, H, W, kStream);
}

void channel_mean_backward_tensor(
    TorchTensorView bilagrid, TorchTensorView v_channel_mean,
    TorchTensorView v_bilagrid, bool inplace
) {
    int N = (int)_tv_dim(bilagrid, 0), C = (int)_tv_dim(bilagrid, 1),
        L = (int)_tv_dim(bilagrid, 2), H = (int)_tv_dim(bilagrid, 3),
        W = (int)_tv_dim(bilagrid, 4);
    channel_mean_backward(
        _tv_cf(bilagrid), _tv_cf(v_channel_mean), _tv_f(v_bilagrid),
        N, C, L, H, W, inplace, kStream);
}

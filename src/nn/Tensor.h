#pragma once
// A device tensor view: dtype + up to 4 row-major dimensions + a device
// address. Non-owning; the memory belongs to a VramPool slot or an Arena.
//
// **Shapes are PyTorch order** -- slowest-varying dimension first. ggml's
// reversed `ne[]` convention is undone once, in the weight loader, so nothing
// downstream has to think in two layouts.
//
// Two layout conventions run through the whole model, both chosen so that no
// op ever needs a materializing permute:
//
//   feature maps    [H, W, C]        channel-last. Makes LayerNorm2d the same
//                                    kernel as LayerNorm, and a 1x1 conv the
//                                    same kernel as a Linear.
//   token sequences [N, C] / [B,N,C] channel-last likewise; a q/k/v projection
//                                    lands as [N, n_heads * head_dim], which is
//                                    exactly what the attention kernel indexes.

#include "nn/vk/Memory.h"

#include <cstdint>
#include <initializer_list>

namespace nn {

using vk::DevicePtr;

enum class DType : uint8_t {
    F32 = 0,
    F16 = 1,   // stored packed; shaders read u32 words and f16tof32 them
    I32 = 2,
    U8  = 3,
};

inline uint32_t dtype_size(DType t) {
    switch (t) {
        case DType::F32: return 4;
        case DType::F16: return 2;
        case DType::I32: return 4;
        case DType::U8:  return 1;
    }
    return 4;
}
inline const char* dtype_name(DType t) {
    switch (t) {
        case DType::F32: return "f32";
        case DType::F16: return "f16";
        case DType::I32: return "i32";
        case DType::U8:  return "u8";
    }
    return "?";
}

struct Tensor {
    DevicePtr ptr = 0;
    DType     dtype = DType::F32;
    int32_t   ndim = 0;
    int64_t   shape[4] = {1, 1, 1, 1};

    Tensor() = default;
    Tensor(DevicePtr p, DType t, int64_t d0)
        : ptr(p), dtype(t), ndim(1) { shape[0] = d0; }
    Tensor(DevicePtr p, DType t, int64_t d0, int64_t d1)
        : ptr(p), dtype(t), ndim(2) { shape[0] = d0; shape[1] = d1; }
    Tensor(DevicePtr p, DType t, int64_t d0, int64_t d1, int64_t d2)
        : ptr(p), dtype(t), ndim(3) { shape[0] = d0; shape[1] = d1; shape[2] = d2; }
    Tensor(DevicePtr p, DType t, int64_t d0, int64_t d1, int64_t d2, int64_t d3)
        : ptr(p), dtype(t), ndim(4) {
        shape[0] = d0; shape[1] = d1; shape[2] = d2; shape[3] = d3;
    }

    bool    valid() const { return ptr != 0; }
    int64_t dim(int i) const { return (i < ndim) ? shape[i] : 1; }
    int64_t numel() const {
        int64_t n = 1;
        for (int i = 0; i < ndim; ++i) n *= shape[i];
        return n;
    }
    int64_t bytes() const { return numel() * dtype_size(dtype); }
    // Elements in all dims but the last: the "row count" every op works with.
    int64_t rows() const { return ndim ? numel() / shape[ndim - 1] : 0; }
    int64_t cols() const { return ndim ? shape[ndim - 1] : 0; }

    // Reinterpret without moving data. The caller guarantees the element count
    // matches; this is checked in debug builds by the ops that consume it.
    Tensor view(int64_t d0) const { return {ptr, dtype, d0}; }
    Tensor view(int64_t d0, int64_t d1) const { return {ptr, dtype, d0, d1}; }
    Tensor view(int64_t d0, int64_t d1, int64_t d2) const { return {ptr, dtype, d0, d1, d2}; }
    Tensor view(int64_t d0, int64_t d1, int64_t d2, int64_t d3) const {
        return {ptr, dtype, d0, d1, d2, d3};
    }

    // Collapse to [shape[0], numel/shape[0]]. Checkpoint weights arrive with
    // their PyTorch rank -- a conv kernel is [Cout, Cin, kh, kw] -- but every
    // matmul-shaped op wants the [out_features, in_features] matrix that is the
    // same bytes. Ops normalize through this rather than making callers spell
    // the reshape out.
    Tensor asMatrix() const {
        if (ndim <= 2) return *this;
        return {ptr, dtype, shape[0], numel() / shape[0]};
    }

    // Contiguous slice along dim 0: rows [begin, begin+count).
    Tensor slice0(int64_t begin, int64_t count) const {
        Tensor t = *this;
        int64_t stride = numel() / shape[0];
        t.ptr = ptr + (uint64_t)(begin * stride) * dtype_size(dtype);
        t.shape[0] = count;
        return t;
    }
    // Byte offset into the same buffer, keeping the dtype.
    Tensor offsetElems(int64_t elems) const {
        Tensor t = *this;
        t.ptr = ptr + (uint64_t)elems * dtype_size(dtype);
        return t;
    }
};

// A kernel indexes from its params pointers with a 32-bit element index and the
// driver folds the byte offset to 32 bits, so an access 4 GiB past a pointer
// wraps back to it. Launchers split, widen or refuse; README rule 7 has which.
int64_t max_span_bytes();
void    set_max_span_bytes(int64_t bytes);   // tests only; 0 restores the default

// Rows one dispatch may cover at this row pitch. Throws if one row is over.
int64_t span_rows(const char* op, int64_t row_bytes);

// Same, rounded even: an f16 operand is read as u32 word pairs, so a chunk has
// to start on an even element of it.
int64_t span_rows_even(const char* op, int64_t row_bytes);

// Throws unless a kernel can reach every element of each tensor from its base.
void check_span(const char* op, std::initializer_list<Tensor> ts);

// A kernel entry name, by value: two of them are live at once in group_norm.
struct KernelName {
    char text[64] = {};
    operator const char*() const { return text; }
};

// `entry`, or its "<module>_wide.<name>" twin -- 64-bit addressing, needs
// shaderInt64 -- when a tensor is past the narrow reach. Throws when the wide
// kernel is needed and the device cannot run it. Only for entries that have one.
KernelName span_entry(const char* entry, std::initializer_list<Tensor> ts);

// Bytes of a `dtype` tensor a kernel can reach from one pointer here: the
// narrow limit, or 2^32 elements where the wide kernels can run.
int64_t span_reach(DType dtype);

// Auto takes the wide kernel only past the narrow reach. Force takes it always,
// so one process can check both paths against the same references; Off refuses
// instead, which is what a device without shaderInt64 does.
enum class WideIndex { Auto, Force, Off };
void set_wide_index(WideIndex mode);

// Allocate a tensor from an arena (activations) or the pool (persistent).
Tensor arena_tensor(vk::Arena& a, DType t, int64_t d0, int64_t d1 = 1, int64_t d2 = 1,
                    int64_t d3 = 1, int ndim = -1);
Tensor pool_tensor(vk::PoolSlot slot, uint32_t sub, DType t, int64_t d0, int64_t d1 = 1,
                   int64_t d2 = 1, int64_t d3 = 1, int ndim = -1);

// Debug helpers: download a tensor to host floats (converting from f16), and
// print a few statistics. Only used by tests and SS_NN_LOG=3 paths -- a forward
// pass must never round-trip through the host.
void tensor_to_host(const Tensor& t, float* dst, int64_t count);
void tensor_from_host(const Tensor& t, const float* src, int64_t count);
void tensor_debug_dump(const char* label, const Tensor& t, int max_values = 8);

}  // namespace nn

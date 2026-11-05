#pragma once
// Host arithmetic the installer runs: bitsandbytes' NF4 quantizer, and the bf16
// roundings of the layers it evaluates once and stores (the timestep embedding,
// the modulation vectors, the text stream's input). Those constants were
// computed under bf16 autocast in training, so they are reproduced that way.

#include <cstdint>
#include <cstring>
#include <vector>

namespace marigold {

inline float bf16_to_f32(uint16_t h) {
    const uint32_t b = (uint32_t)h << 16;
    float f;
    std::memcpy(&f, &b, 4);
    return f;
}

// Round to nearest even, as torch's float -> bfloat16 cast.
inline uint16_t f32_to_bf16(float f) {
    uint32_t b;
    std::memcpy(&b, &f, 4);
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu)) return (uint16_t)((b >> 16) | 0x40);
    b += 0x7fffu + ((b >> 16) & 1u);
    return (uint16_t)(b >> 16);
}

inline float bf16r(float f) { return bf16_to_f32(f32_to_bf16(f)); }

// quantize_4bit(blocksize=64, quant_type="nf4") on a bf16 tensor, byte for
// byte; `absmax` comes out as the bf16 it already is. `n` is a multiple of 64.
void nf4_quantize(const uint16_t* bf16, int64_t n, uint8_t* codes, uint16_t* absmax);

// The bf16 weight bitsandbytes computes with: dequantize_4bit's, or with
// `gemv` the one-row kernel's, whose code book is itself bf16.
void nf4_dequantize(const uint8_t* codes, const uint16_t* absmax, int64_t n, float* out,
                    bool gemv = false);

// A Linear under bf16 autocast, y = x W^T + b, on bf16 values. Gemv4bit is
// bitsandbytes' one-row kernel: bf16 products, rounded before the bias goes on.
// Gemm leaves cuBLAS's reduced-precision reductions out; they vary by GPU.
enum class Rounding { Gemm, Gemv4bit };
std::vector<float> linear_bf16(const std::vector<float>& x, int64_t rows,
                               const std::vector<float>& w, int64_t N, int64_t K,
                               const float* bias, Rounding r);

}  // namespace marigold

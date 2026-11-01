#pragma once
// Reading tensors of the installed files whatever their dtype -- the NF4
// codes and bf16 scales nn::SafetensorsFile::read does not decode.

#include "nn/io/Safetensors.h"

#include <cstdint>
#include <string>
#include <vector>

namespace marigold {

std::vector<uint8_t> read_raw(const nn::SafetensorsFile& f, const std::string& name);
std::vector<float> read_f32(const nn::SafetensorsFile& f, const std::string& name);
// `prefix`.q4 and `prefix`.absmax, as the weight bitsandbytes computes with
// (nf4_dequantize's `gemv`).
std::vector<float> read_nf4(const nn::SafetensorsFile& f, const std::string& prefix,
                            bool gemv = false);

}  // namespace marigold

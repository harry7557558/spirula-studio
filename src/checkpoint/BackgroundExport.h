#pragma once

#include "backend/api/BackendTypes.h"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace ckpt {

float3 eval_sh_direction(int degree, float3 dir, float3 dc, const float3* coeffs);

bool render_background_sh_equirectangular(const float3* coeffs, int degree,
                                          int width, int height,
                                          std::vector<uint8_t>& out_rgb8,
                                          int transfer = 0,
                                          bool is_linear = false);

bool export_background_sh_webp(const std::filesystem::path& out_path,
                               const float3* coeffs, int degree,
                               int width = 2048, int height = 1024,
                               int transfer = 0, bool is_linear = false);

bool load_background_sh(const std::filesystem::path& ckpt_dir,
                        int& out_degree,
                        std::vector<float3>& out_coeffs,
                        int& out_transfer,
                        bool& out_is_linear);

bool export_background_from_checkpoint(const std::filesystem::path& ckpt_dir,
                                       const std::filesystem::path& out_path = "",
                                       int width = 2048, int height = 1024);

}  // namespace ckpt

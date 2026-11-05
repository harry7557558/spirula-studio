#pragma once

#include "backend/api/BackendTypes.h"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace ckpt {

float3 eval_sh_direction(int degree, float3 dir, float3 dc, const float3* coeffs);

// color_matrix is an optional row-major working-gamut to Rec.709 transform.
bool render_background_sh_equirectangular(const float3* coeffs, int degree,
                                          int width, int height,
                                          std::vector<uint8_t>& out_rgb8,
                                          int transfer = 0,
                                          bool is_linear = false,
                                          const float* color_matrix = nullptr);

bool export_background_sh_webp(const std::filesystem::path& out_path,
                               const float3* coeffs, int degree,
                               int width = 1024, int height = 512,
                               int transfer = 0, bool is_linear = false,
                               const float* color_matrix = nullptr);

// Missing color metadata leaves the caller's transfer and linear defaults intact.
bool load_background_sh(const std::filesystem::path& ckpt_dir,
                        int& out_degree,
                        std::vector<float3>& out_coeffs,
                        int& out_transfer,
                        bool& out_is_linear,
                        std::vector<float>* out_color_matrix = nullptr);

bool export_background_from_checkpoint(const std::filesystem::path& ckpt_dir,
                                       const std::filesystem::path& out_path = "",
                                       int width = 1024, int height = 512);

}  // namespace ckpt

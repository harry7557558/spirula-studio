#pragma once

#include "nn/io/Image.h"

#include <filesystem>
#include <vector>

namespace app {

struct FocusOverlayEntry {
    std::string image_relative;
    double mean_weight = 0;
};

void write_focus_overlay(const nn::Image& image, const std::vector<uint8_t>& weight,
                         const std::filesystem::path& output);
std::vector<uint8_t> read_focus_overlay_mask(const std::string& path, int width, int height);
void write_focus_overlay_gallery(const std::vector<FocusOverlayEntry>& entries,
                                 const std::filesystem::path& directory);

}  // namespace app

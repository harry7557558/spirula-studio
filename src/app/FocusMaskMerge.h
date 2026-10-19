#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace app {

struct FocusMaskTarget {
    std::filesystem::path root;
    std::filesystem::path output, original, record;
    std::string current_hash;
    std::string pending_hash;
};

FocusMaskTarget prepare_focus_mask(const std::filesystem::path& root,
                                   const std::filesystem::path& masks,
                                   const std::string& image_relative);
void write_focus_mask(FocusMaskTarget& target, std::vector<uint8_t>& weight, int width, int height);

}  // namespace app

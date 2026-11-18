#pragma once

#include <filesystem>

namespace image_discovery {

inline constexpr const char* kProgressiveCache = ".spirula-progressive";

inline bool generated_cache(const std::filesystem::path& path) {
    for (const auto& part : path)
        if (part == kProgressiveCache) return true;
    return false;
}

inline bool skip_cache(std::filesystem::recursive_directory_iterator& it) {
    if (!generated_cache(it->path())) return false;
    it.disable_recursion_pending();
    return true;
}

template <class IsImage>
bool holds_images_outside(const std::filesystem::path& root,
                          const std::filesystem::path& nested, IsImage is_image) {
    namespace fs = std::filesystem;
    std::error_code walk, ec;
    for (fs::recursive_directory_iterator it(root, fs::directory_options::follow_directory_symlink, walk), end;
         !walk && it != end; it.increment(walk)) {
        if (skip_cache(it)) continue;
        if (it->is_directory(ec)) {
            if (fs::equivalent(it->path(), nested, ec)) it.disable_recursion_pending();
            continue;
        }
        if (it->is_regular_file(ec) && is_image(it->path())) return true;
    }
    return false;
}

}  // namespace image_discovery

#include "data/DownscaleCache.h"
#include "data/ImageDiscovery.h"
#include "external/stb_image_write.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <set>
#include <string>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    failures += !ok;
}
bool image(const fs::path& path) { return path.extension() == ".png"; }
void write_image(const fs::path& path) {
    fs::create_directories(path.parent_path());
    const std::vector<uint8_t> pixels(16 * 12 * 3, 127);
    if (!stbi_write_png(path.string().c_str(), 16, 12, 3, pixels.data(), 16 * 3))
        throw std::runtime_error("could not write fixture");
}
std::set<std::string> discover(const fs::path& root) {
    std::set<std::string> found;
    for (fs::recursive_directory_iterator it(root), end; it != end; ++it) {
        if (image_discovery::skip_cache(it)) continue;
        if (it->is_regular_file() && image(it->path()))
            found.insert(it->path().lexically_relative(root).generic_string());
    }
    return found;
}
}  // namespace

int main() {
    const fs::path root = fs::temp_directory_path() /
        ("spirula-image-discovery-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const fs::path images = root / "images";
    const fs::path source = images / "cam0" / "frame.png";
    write_image(source);
    const fs::path level = dscache::path_for_size(source.string(), 4, 3, images.string());
    write_image(level);
    check(!image_discovery::holds_images_outside(root, images, image),
          "generated levels do not stop SfM auto from choosing dataset/images");
    check(discover(root) == std::set<std::string>{"images/cam0/frame.png"},
          "dataset-root discovery returns the photograph without its generated level");
    check(discover(root / image_discovery::kProgressiveCache).empty(),
          "direct discovery of the owned cache remains empty");

    const fs::path external = root / "photos" / "rig0" / "frame.png";
    write_image(external);
    const fs::path external_level = dscache::path_for_size(external.string(), 4, 3, images.string());
    write_image(external_level);
    check(discover(root / "photos") == std::set<std::string>{"rig0/frame.png"},
          "nested external-source fallback levels never become source photographs");
    check(image_discovery::holds_images_outside(root, images, image),
          "a real external photograph still prevents narrowing the dataset root");

    write_image(root / ".photos" / "hidden.png");
    write_image(root / ".spirula-progressive-user" / "ordinary.png");
    write_image(root / ".spirula-progressive.png");
    check(discover(root) == std::set<std::string>{"images/cam0/frame.png", "photos/rig0/frame.png",
              ".photos/hidden.png", ".spirula-progressive-user/ordinary.png", ".spirula-progressive.png"},
          "unrelated hidden folders and similar filenames remain discoverable");
    fs::remove_all(root);
    return failures != 0;
}

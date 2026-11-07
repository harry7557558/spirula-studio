// downscale_cache_test -- the <image_dir>_<d>/ cache an integer
// train_resolution_divisor writes: where it goes, that a cached load trains on
// the pixels a direct one would, and that a file already there is reused when
// it fits and left alone when it does not. Host only, in a temp directory.

#include "core/PngWrite.h"
#include "data/DataManager.h"
#include "data/DownscaleCache.h"

#include "external/stb_image.h"
#include "external/stb_image_write.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

constexpr int W = 17, H = 13, D = 2;   // odd, so floor and ceil differ

template <typename T>
std::vector<T> pattern(int c, int top) {
    std::vector<T> px((size_t)W * H * c);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            for (int k = 0; k < c; k++)
                px[((size_t)y * W + x) * c + k] =
                    (T)(k == 3 ? (x * 37 + y * 11) % (top + 1)
                               : (x * 97 + y * 53 + k * 71) * 131 % (top + 1));
    return px;
}

void write_bytes(const fs::path& p, const std::vector<uint8_t>& b) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary).write((const char*)b.data(), (std::streamsize)b.size());
}

std::vector<uint8_t> read_bytes(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

CacheMode g_mode = CacheMode::CPU;

std::vector<uint8_t> fetch(const std::string& image, const fs::path& image_dir, bool cache,
                           const float* over = nullptr, int turns = 0) {
    DataManagerConfig cfg;
    cfg.cache_mode = g_mode;
    cfg.load_masks = cfg.load_depths = cfg.load_normals = false;
    if (cache) {
        cfg.downscale_cache_divisor = D;
        cfg.image_dir = image_dir.string();
    }
    if (over) {
        cfg.composite_alpha = {1};
        for (int c = 0; c < 3; c++) cfg.composite_color[c] = over[c];
    }
    if (turns) cfg.exif_quarter_turns = {(uint8_t)turns};
    std::vector<float> viewmat(16, 0.0f);
    viewmat[0] = viewmat[5] = viewmat[10] = viewmat[15] = 1.0f;
    const int w = (turns & 1 ? H : W) / D, h = (turns & 1 ? W : H) / D;
    DataManager dm(cfg, {0}, {0}, {image}, {}, {}, {}, {w}, {h}, {}, {}, viewmat,
                   {10.0f, 10.0f, 4.0f, 3.0f}, std::vector<float>(8, 0.0f), {}, {}, {}, {},
                   {}, {}, {}, {0}, {});
    DecodedBatch b;
    dm.fetch_one(0, b);
    return b.rgb_buffer;
}

// Largest per-byte difference; 16-bit buffers are compared by their samples.
int max_diff(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, bool sixteen) {
    if (a.size() != b.size()) return 1 << 20;
    int worst = 0;
    for (size_t i = 0; i < a.size(); i += sixteen ? 2 : 1) {
        const int va = sixteen ? a[i] | a[i + 1] << 8 : a[i];
        const int vb = sixteen ? b[i] | b[i + 1] << 8 : b[i];
        worst = std::max(worst, std::abs(va - vb));
    }
    return worst;
}

void test_paths(const fs::path& root) {
    const fs::path images = root / "paths" / "images";
    write_bytes(images / "a.png", png::encode(pattern<uint8_t>(3, 255).data(), W, H, 3, 8));
    write_bytes(images / "cam0" / "b.png", read_bytes(images / "a.png"));
    write_bytes(images / "c.bmp", {'B', 'M', 0, 0});
    check(dscache::path_for((images / "a.png").string(), images.string(), 2) ==
              (root / "paths" / "images_2" / "a.png").string(),
          "flat image -> images_2/a.png");
    check(dscache::path_for((images / "cam0" / "b.png").string(), images.string() + "/", 4) ==
              (root / "paths" / "images_4" / "cam0" / "b.png").string(),
          "nested image keeps its subfolder under images_4/");
    check(dscache::path_for((images / "cam0" / "b.png").string(), "", 3) ==
              (images / "cam0_3" / "b.png").string(),
          "outside the image dir: <parent>_3/");
    check(dscache::path_for((images / "c.bmp").string(), images.string(), 2).empty(),
          "BMP is not cached");
    check(dscache::path_for((images / "a.png").string(), images.string(), 1).empty(),
          "divisor 1 is not cached");
}

// Cache off and on agree, and the second load comes from the written file.
void test_format(const fs::path& root, const std::string& name, const std::vector<uint8_t>& file,
                 bool sixteen, int tolerance, const float* over = nullptr, int turns = 0) {
    const fs::path images = root / name / "images";
    const fs::path src = images / ("x" + std::string(file[0] == 0xFF ? ".jpg" : ".png"));
    const fs::path cached = root / name / "images_2" / src.filename();
    write_bytes(src, file);
    const std::vector<uint8_t> direct = fetch(src.string(), images, false, over, turns);
    const std::vector<uint8_t> first = fetch(src.string(), images, true, over, turns);
    check(fs::exists(cached), name + ": cache written");
    int cw = 0, ch = 0, cn = 0;
    stbi_info(cached.string().c_str(), &cw, &ch, &cn);
    check(cw == W / D && ch == H / D, name + ": cache is floor-sized");
    check(sixteen == (stbi_is_16_bit(cached.string().c_str()) != 0), name + ": bit depth kept");
    check(max_diff(direct, first, sixteen) <= tolerance,
          name + ": cached load within " + std::to_string(tolerance) + " of a direct one (" +
              std::to_string(max_diff(direct, first, sixteen)) + ")");
    const std::vector<uint8_t> again = fetch(src.string(), images, true, over, turns);
    check(again == first, name + ": second load reads the cache");
}

void test_existing(const fs::path& root) {
    const fs::path images = root / "existing" / "images";
    const fs::path src = images / "x.png";
    const fs::path cached = root / "existing" / "images_2" / "x.png";
    write_bytes(src, png::encode(pattern<uint8_t>(3, 255).data(), W, H, 3, 8));
    const std::vector<uint8_t> direct = fetch(src.string(), images, false);

    // A ceil-sized file of one flat colour is accepted and is what loads.
    const int cw = (W + 1) / 2, chh = (H + 1) / 2;
    std::vector<uint8_t> flat((size_t)cw * chh * 3, 200);
    write_bytes(cached, png::encode(flat.data(), cw, chh, 3, 8));
    const std::vector<uint8_t> got = fetch(src.string(), images, true);
    bool all200 = !got.empty();
    for (uint8_t v : got) all200 = all200 && v == 200;
    check(all200, "existing ceil-sized file is used");

    // Wrong size: the original loads and the file is untouched.
    const std::vector<uint8_t> wrong = png::encode(flat.data(), cw + 3, chh, 3, 8);
    fs::remove(cached);
    write_bytes(cached, wrong);
    check(fetch(src.string(), images, true) == direct, "wrong-sized file: original used");
    check(read_bytes(cached) == wrong, "wrong-sized file left alone");

    // Truncated: likewise.
    std::vector<uint8_t> cut = png::encode(flat.data(), cw, chh, 3, 8);
    cut.resize(cut.size() / 2);
    fs::remove(cached);
    write_bytes(cached, cut);
    check(fetch(src.string(), images, true) == direct, "truncated file: original used");
    check(read_bytes(cached) == cut, "truncated file left alone");
}

std::vector<uint8_t> jpeg_of(const std::vector<uint8_t>& px) {
    return dscache::encode(dscache::Format::Jpeg, px.data(), W, H, 3, 8);
}

}  // namespace

int main() {
    const fs::path base = fs::temp_directory_path() / ("ss_downscale_cache_" + std::to_string(::rand()));
    for (CacheMode mode : {CacheMode::CPU, CacheMode::DISK}) {
        g_mode = mode;
        const fs::path root = base / (mode == CacheMode::CPU ? "cpu" : "disk");
        std::printf("-- %s\n", mode == CacheMode::CPU ? "cpu" : "disk");
        test_paths(root);
        test_format(root, "png8", png::encode(pattern<uint8_t>(3, 255).data(), W, H, 3, 8),
                    false, 0);
        test_format(root, "png16", png::encode(pattern<uint16_t>(3, 65535).data(), W, H, 3, 16),
                    true, 0);
        const float grey[3] = {0.5f, 0.25f, 1.0f};
        test_format(root, "rgba8", png::encode(pattern<uint8_t>(4, 255).data(), W, H, 4, 8),
                    false, 1, grey);
        test_format(root, "rgba16",
                    png::encode(pattern<uint16_t>(4, 65535).data(), W, H, 4, 16), true, 1, grey);
        test_format(root, "gray8", png::encode(pattern<uint8_t>(1, 255).data(), W, H, 1, 8),
                    false, 0);
        test_format(root, "jpeg", jpeg_of(pattern<uint8_t>(3, 255)), false, 12);
        // The cache holds the file's own pixels; the turn comes from the original's EXIF.
        test_format(root, "png8_turned", png::encode(pattern<uint8_t>(3, 255).data(), W, H, 3, 8),
                    false, 0, nullptr, 1);
        test_existing(root);
    }
    std::error_code ec;
    fs::remove_all(base, ec);
    std::printf("%s\n", g_failures ? "FAILED" : "PASSED");
    return g_failures ? 1 : 0;
}

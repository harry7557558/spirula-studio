// alpha_mask_test -- the mask DataManager trains an RGBA image with: its alpha
// alone, or ANDed with a mask file of another size, with flip_mask reaching
// only the file. PNG and EXR. Host only; the images are written to a temp
// directory.

#include "data/DataManager.h"
#include "data/ImageProbe.h"
#include "config/TrainConfigJson.h"

#include "external/stb_image_write.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void check(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) g_failures++;
}

constexpr int W = 8, H = 6;

// Opaque from column 4, a 127 / 128 pair either side of the gate at 2 and 3.
std::vector<uint8_t> rgba_image() {
    std::vector<uint8_t> px((size_t)W * H * 4);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            uint8_t* p = &px[((size_t)y * W + x) * 4];
            p[0] = (uint8_t)(x * 30); p[1] = (uint8_t)(y * 40); p[2] = 90;
            p[3] = x < 2 ? 0 : x == 2 ? 127 : x == 3 ? 128 : 255;
        }
    return px;
}

bool want_alpha(int x) { return x >= 3; }

std::string write_png(const fs::path& p, int w, int h, int c,
                      const std::vector<uint8_t>& px) {
    stbi_write_png(p.string().c_str(), w, h, c, px.data(), w * c);
    return p.string();
}

// rgba_image() as an uncompressed FLOAT EXR, colour premultiplied as the
// format has it. Written by hand: nothing here links an EXR writer.
std::string write_exr(const fs::path& p) {
    const std::vector<uint8_t> src = rgba_image();
    std::string b = std::string("\x76\x2f\x31\x01\x02\x00\x00\x00", 8);
    auto put = [&](const void* v, size_t n) { b.append((const char*)v, n); };
    auto i32 = [&](int32_t v) { put(&v, 4); };
    auto attr = [&](const char* name, const char* type, int32_t size) {
        b += name; b += '\0'; b += type; b += '\0'; i32(size);
    };
    attr("channels", "chlist", 4 * 18 + 1);
    for (const char* c : {"A", "B", "G", "R"}) {
        b += c; b += '\0';
        i32(2);   // FLOAT
        i32(0);   // pLinear and reserved
        i32(1); i32(1);
    }
    b += '\0';
    attr("compression", "compression", 1); b += '\0';
    for (const char* win : {"dataWindow", "displayWindow"}) {
        attr(win, "box2i", 16);
        i32(0); i32(0); i32(W - 1); i32(H - 1);
    }
    attr("lineOrder", "lineOrder", 1); b += '\0';
    const float one = 1.0f, zero = 0.0f;
    attr("pixelAspectRatio", "float", 4); put(&one, 4);
    attr("screenWindowCenter", "v2f", 8); put(&zero, 4); put(&zero, 4);
    attr("screenWindowWidth", "float", 4); put(&one, 4);
    b += '\0';
    const int32_t line = 8 + W * 4 * 4;
    for (int y = 0; y < H; y++) {
        const uint64_t at = b.size() + (uint64_t)(H - y) * 8 + (uint64_t)y * line;
        put(&at, 8);
    }
    for (int y = 0; y < H; y++) {
        i32(y); i32(W * 4 * 4);
        for (int c : {3, 2, 1, 0})
            for (int x = 0; x < W; x++) {
                const uint8_t* px = &src[((size_t)y * W + x) * 4];
                const float a = px[3] / 255.0f;
                const float v = c == 3 ? a : px[c] / 255.0f * a;
                put(&v, 4);
            }
    }
    std::ofstream(p, std::ios::binary) << b;
    return p.string();
}

// The top `rows` of an h-row gray mask white.
std::vector<uint8_t> top_mask(int w, int h, int rows) {
    std::vector<uint8_t> m((size_t)w * h, 0);
    for (int y = 0; y < rows; y++)
        for (int x = 0; x < w; x++) m[(size_t)y * w + x] = 255;
    return m;
}

struct Fetched {
    int w = 0, h = 0;
    std::vector<uint8_t> mask;
    std::vector<uint8_t> rgb;   // float32 for an EXR
    uint8_t at(int x, int y) const {   // in the image's own pixels
        return mask[(size_t)(y * h / H) * w + (size_t)(x * w / W)];
    }
};

// Both cache modes: the prefetch pool and the preload decode on their own paths.
CacheMode g_mode = CacheMode::CPU;

Fetched fetch(const std::string& image, const std::string& mask, bool alpha,
              bool flip, const float* over = nullptr, bool load_masks = true,
              bool mixed = false, float offset = 0.0f) {
    DataManagerConfig cfg;
    cfg.cache_mode = g_mode;
    cfg.load_masks = load_masks;
    cfg.load_depths = cfg.load_normals = false;
    cfg.flip_mask = flip;
    cfg.segment_and_ignore = mixed;
    cfg.mask_boundary_offset = offset;
    if (alpha) cfg.alpha_masks = {1};
    if (over) {
        cfg.composite_alpha = {1};
        for (int c = 0; c < 3; c++) cfg.composite_color[c] = over[c];
    }
    std::vector<float> viewmat(16, 0.0f);
    viewmat[0] = viewmat[5] = viewmat[10] = viewmat[15] = 1.0f;
    DataManager dm(cfg, {0}, {0}, {image},
                   mask.empty() ? std::vector<std::string>{}
                                : std::vector<std::string>{mask},
                   {}, {}, {W}, {H}, {}, {}, viewmat, {10.0f, 10.0f, 4.0f, 3.0f},
                   std::vector<float>(8, 0.0f), {}, {}, {}, {}, {}, {}, {}, {0}, {});
    DecodedBatch b;
    dm.fetch_one(0, b);
    return {b.mask_width, b.mask_height, b.mask_buffer, b.rgb_buffer};
}

void run_cases(const std::string& rgba, const std::string& small_image,
               const std::string& large_image) {
    {
        const auto m = fetch(rgba, small_image, true, false, nullptr, false);
        check(m.mask.empty(), "masks disabled: neither alpha nor sidecar creates a training mask");
    }
    {
        Fetched m = fetch(rgba, "", true, false);
        bool ok = m.w == W && m.h == H;
        for (int y = 0; y < H && ok; y++)
            for (int x = 0; x < W; x++) ok &= m.at(x, y) == (uint8_t)want_alpha(x);
        check(ok, "alpha alone, gated at 128");
    }
    for (bool flip : {false, true}) {
        Fetched m = fetch(rgba, small_image, true, flip);
        bool ok = m.w == W && m.h == H;
        for (int y = 0; y < H && ok; y++)
            for (int x = 0; x < W; x++) {
                const bool file = (y < 2) != flip;
                ok &= m.at(x, y) == (uint8_t)(want_alpha(x) && file);
            }
        check(ok, flip ? "smaller mask file, flipped, AND alpha"
                       : "smaller mask file AND alpha");
    }
    {
        Fetched m = fetch(rgba, large_image, true, false);
        bool ok = m.w == W * 2 && m.h == H * 2;
        for (int y = 0; y < m.h && ok; y++)
            for (int x = 0; x < m.w; x++)
                ok &= m.mask[(size_t)y * m.w + x] == (uint8_t)(want_alpha(x / 2) && y < 4);
        check(ok, "larger mask file keeps its size, AND alpha");
    }
    {
        Fetched m = fetch(rgba, small_image, false, true);
        bool ok = m.w == W / 2 && m.h == H / 2;
        for (int y = 0; y < m.h && ok; y++)
            for (int x = 0; x < m.w; x++)
                ok &= m.mask[(size_t)y * m.w + x] == (uint8_t)(y >= 1);
        check(ok, "no alpha flag: the flipped file alone, at its own size");
    }
    if (rgba.size() > 4 && rgba.compare(rgba.size() - 4, 4, ".exr") == 0) {
        // Premultiplied: the grey fills only what the alpha leaves.
        const float grey[3] = {0.8f, 0.8f, 0.8f};
        Fetched m = fetch(rgba, "", true, false, grey);
        const std::vector<uint8_t> src = rgba_image();
        bool ok = m.rgb.size() == (size_t)W * H * 3 * sizeof(float);
        for (int i = 0; i < W * H && ok; i++)
            for (int c = 0; c < 3; c++) {
                const float a = src[(size_t)i * 4 + 3] / 255.0f;
                const float want = src[(size_t)i * 4 + c] / 255.0f * a + 0.8f * (1.0f - a);
                float got;
                std::memcpy(&got, &m.rgb[((size_t)i * 3 + c) * sizeof(float)], sizeof got);
                ok &= std::fabs(got - want) <= 1e-5f;
            }
        check(ok, "colour composited onto the background by its alpha");
    } else {
        // Over a light grey: transparent reads as the grey, opaque as itself,
        // and 127 / 128 as the straight-alpha blend of the two.
        const float grey[3] = {0.8f, 0.8f, 0.8f};
        Fetched m = fetch(rgba, "", true, false, grey);
        const std::vector<uint8_t> src = rgba_image();
        bool ok = m.rgb.size() == (size_t)W * H * 3;
        for (int i = 0; i < W * H && ok; i++)
            for (int c = 0; c < 3; c++) {
                const double a = src[(size_t)i * 4 + 3] / 255.0;
                const double want = src[(size_t)i * 4 + c] * a + 0.8 * 255.0 * (1.0 - a);
                ok &= std::abs((double)m.rgb[(size_t)i * 3 + c] - want) <= 0.5;
            }
        check(ok, "colour composited onto the background by its alpha");
    }
}

}  // namespace

int main() {
    const fs::path dir = fs::temp_directory_path() / "ss_alpha_mask_test";
    fs::create_directories(dir);
    const std::string rgba = write_png(dir / "rgba.png", W, H, 4, rgba_image());
    const std::string rgba_exr = write_exr(dir / "rgba.exr");
    std::vector<uint8_t> opaque = rgba_image();
    for (size_t i = 3; i < opaque.size(); i += 4) opaque[i] = 255;
    const std::string rgba_opaque = write_png(dir / "opaque.png", W, H, 4, opaque);
    const std::string rgb = write_png(dir / "rgb.png", W, H, 3,
                                      std::vector<uint8_t>((size_t)W * H * 3, 128));
    // Half the image's size, and twice it: the AND happens on whichever grid
    // is finer, so neither loses detail.
    const std::string small_image = write_png(dir / "small.png", W / 2, H / 2, 1,
                                        top_mask(W / 2, H / 2, 1));
    const std::string large_image = write_png(dir / "large.png", W * 2, H * 2, 1,
                                        top_mask(W * 2, H * 2, 4));
    const uint8_t values[W] = {0, 127, 128, 250, 251, 255, 128, 0};
    const uint8_t labels[W] = {2, 2, 0, 0, 1, 1, 0, 2};
    std::vector<uint8_t> mixed_pixels((size_t)W * H * 4);
    for (int y = 0; y < H * 2; ++y) for (int x = 0; x < W * 2; ++x)
        mixed_pixels[(size_t)y * W * 2 + x] = values[x / 2];
    const std::string mixed_image = write_png(dir / "mixed.png", W * 2, H * 2, 1, mixed_pixels);

    {
        TrainConfig legacy;
        train_config_from_json(json_parse("{\"load_masks\":true,\"apply_loss_for_mask\":false}"), legacy);
        check(train_mask_mode(legacy) == "ignore", "old configs retain ignore mode");
        legacy.apply_loss_for_mask = true;
        check(train_mask_mode(legacy) == "segment", "old configs retain cut-out mode");
        legacy.load_masks = false;
        check(train_mask_mode(legacy) == "none", "old configs retain disabled masks");
        legacy.mask_mode = "segment_and_ignore";
        std::string serialized = "{";
        for (const auto& kv : train_config_json_pairs(legacy)) {
            if (serialized.size() > 1) serialized += ',';
            serialized += '"' + std::string(kv.first) + "\":" + kv.second;
        }
        serialized += '}';
        TrainConfig restored;
        train_config_from_json(json_parse(serialized), restored);
        check(train_mask_mode(restored) == "segment_and_ignore",
              "mixed mode round-trips and overrides legacy options");
    }

    {
        std::vector<uint8_t> f = probe_alpha_masks({rgb, rgba, rgba_opaque});
        check(f == std::vector<uint8_t>{0, 1, 1}, "probe flags the files with alpha");
        check(probe_alpha_masks({rgb, rgba_exr}) == std::vector<uint8_t>{0, 1},
              "probe flags an EXR with alpha");
        check(probe_alpha_masks({rgb, rgba_opaque}).empty(),
              "probe: opaque alpha is no mask");
    }

    for (CacheMode mode : {CacheMode::CPU, CacheMode::DISK}) {
        g_mode = mode;
        std::printf("-- %s cache\n", mode == CacheMode::CPU ? "cpu" : "disk");
        run_cases(rgba, small_image, large_image);
        std::printf("-- %s cache, EXR\n", mode == CacheMode::CPU ? "cpu" : "disk");
        run_cases(rgba_exr, small_image, large_image);
        for (bool alpha : {false, true}) for (bool flip : {false, true}) {
            Fetched m = fetch(rgba, mixed_image, alpha, flip, nullptr, true, true);
            bool ok = m.w == W * 2 && m.h == H * 2;
            for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
                uint8_t want = labels[x];
                if (flip && want != 0) want = want == 1 ? 2 : 1;
                if (alpha && !want_alpha(x) && want != 2) want = 0;
                ok &= m.at(x, y) == want;
            }
            check(ok, "mixed mask preserves bands, resolution, flip and ignore priority over alpha");
        }
        Fetched disabled = fetch(rgba, mixed_image, true, false, nullptr, false, true);
        check(disabled.mask.empty(), "mixed mask loading can be disabled");
        Fetched offset = fetch(rgba, mixed_image, false, false, nullptr, true, true, -0.2f);
        check(offset.at(2, 2) == 2 && offset.at(3, 2) == 0,
              "mixed boundary expansion keeps ignore priority and segment labels");
    }

    fs::remove_all(dir);
    std::printf("\n%s\n", g_failures ? "FAILURES" : "all passed");
    return g_failures ? 1 : 0;
}

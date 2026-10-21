#include "app/tests/SkyTestSupport.h"
#include "app/TrainerCore.h"
#include "checkpoint/SplatPly.h"
#include "external/stb_image_write.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace {

using namespace spirula::sky::test;
using spirula::sky::kSplatCount;
using spirula::TrainerSession;
using spirula::resolve_color;
using spirula::seed_splats;

struct BakeImages {
    std::filesystem::path root = std::filesystem::temp_directory_path() /
        ("spirula-sky-images-" + std::to_string(std::chrono::steady_clock::now()
                                                  .time_since_epoch().count()));
    std::vector<uint8_t> pixels;

    BakeImages() {
        std::filesystem::create_directory(root);
        for (int i = 0; i < 8; i++)
            pixels.insert(pixels.end(), {(uint8_t)(i * 24 + 5),
                                        (uint8_t)(i * 15 + 3),
                                        (uint8_t)(240 - i * 20)});
        expect(stbi_write_png((root / "rgb.png").string().c_str(), 4, 2, 3,
                              pixels.data(), 12) != 0, "write bake PNG fixture");
        const uint8_t grey = 73;
        expect(stbi_write_png((root / "grey.png").string().c_str(), 1, 1, 1,
                              &grey, 1) != 0, "write single-pixel bake fixture");
        std::vector<uint8_t> tiff = {'I', 'I', 42, 0};
        auto le16 = [&](uint16_t x) {
            tiff.push_back((uint8_t)x);
            tiff.push_back((uint8_t)(x >> 8));
        };
        auto le32 = [&](uint32_t x) {
            for (int i = 0; i < 4; i++) tiff.push_back((uint8_t)(x >> (8 * i)));
        };
        le32(38);
        tiff.insert(tiff.end(), pixels.begin(), pixels.end());
        for (int i = 0; i < 3; i++) le16(8);
        le16(9);
        const uint32_t entries[][4] = {
            {256, 4, 1, 4}, {257, 4, 1, 2}, {258, 3, 3, 32},
            {259, 3, 1, 1}, {262, 3, 1, 2}, {273, 4, 1, 8},
            {277, 3, 1, 3}, {278, 4, 1, 2}, {279, 4, 1, 24},
        };
        for (const auto& entry : entries) {
            le16((uint16_t)entry[0]);
            le16((uint16_t)entry[1]);
            le32(entry[2]);
            le32(entry[3]);
        }
        le32(0);
        std::ofstream file(root / "rgb.tiff", std::ios::binary);
        file.write((const char*)tiff.data(), (std::streamsize)tiff.size());
        expect((bool)file, "write bake TIFF fixture");
    }

    ~BakeImages() {
        for (const char* name : {"rgb.png", "grey.png", "rgb.tiff"})
            std::filesystem::remove(root / name);
        std::filesystem::remove(root);
    }
};

void check_bake_images(std::ostream* out) {
    BakeImages images;
    TrainerSession session;
    auto actual = rows_of({});
    for (int y = 0; y < 6; y++)
        for (int x = 0; x < 8; x++)
            actual.means.insert(actual.means.end(), {x + 0.5f, y + 0.5f, 1.0f});
    actual.features_dc.resize(actual.means.size());
    auto expected = actual;
    spirula::sky::Bake bake;
    const double t[3] = {};
    auto add = [&](const char* name, int turns, int w, int h,
                   const std::vector<uint8_t>& pixels) {
        auto view = view_at(t, 1.0, 1.0, w, h);
        view.cx = view.cy = 0.0;
        view.rgb = pixels.data();
        spirula::sky::bake_accumulate(expected, view, bake);
        session.ds.train_indices.push_back((int32_t)session.ds.num_cameras++);
        session.ds.image_filenames.push_back((images.root / name).string());
        session.ds.exif_quarter_turns.push_back((uint8_t)turns);
        session.ds.widths.push_back(w);
        session.ds.heights.push_back(h);
        session.ds.intrins.insert(session.ds.intrins.end(), {1.0f, 1.0f, 0.0f, 0.0f});
        session.ds.c2w.insert(session.ds.c2w.end(), view.c2w, view.c2w + 12);
        auto prefix = actual, expected_prefix = expected;
        spirula::sky::bake_finalize(expected_prefix, bake);
        session.bake_sky(prefix);
        expect(same_bytes(prefix.features_dc, expected_prefix.features_dc),
               std::string("bake pixels: ") + name + ", turns " + std::to_string(turns) +
               ", size " + std::to_string(w) + "x" + std::to_string(h));
    };
    const int order[4][8] = {
        {0, 1, 2, 3, 4, 5, 6, 7}, {4, 0, 5, 1, 6, 2, 7, 3},
        {7, 6, 5, 4, 3, 2, 1, 0}, {3, 7, 2, 6, 1, 5, 0, 4},
    };
    for (int turns = 0; turns < 4; turns++) {
        const int w = turns & 1 ? 2 : 4, h = turns & 1 ? 4 : 2;
        std::vector<uint8_t> rotated;
        for (int index : order[turns])
            rotated.insert(rotated.end(), images.pixels.begin() + index * 3,
                           images.pixels.begin() + index * 3 + 3);
        std::vector<uint8_t> reduced;
        for (int y = 0; y < h; y += 2)
            for (int x = 0; x < w; x += 2)
                for (int c = 0; c < 3; c++) {
                    const int a = (y * w + x) * 3 + c;
                    const int sum = rotated[a] + rotated[a + 3] +
                                    rotated[a + w * 3] + rotated[a + w * 3 + 3];
                    reduced.push_back((uint8_t)((sum + 2) / 4));
                }
        for (const char* name : {"rgb.png", "rgb.tiff"}) {
            add(name, turns, w, h, rotated);
            add("grey.png", turns, 7, 5, std::vector<uint8_t>(7 * 5 * 3, 73));
            add(name, turns, w / 2, h / 2, reduced);
        }
    }
    for (const char* name : {"rgb.png", "rgb.tiff"}) {
        add(name, 0, 3, 3, {9, 6, 237, 41, 26, 210, 73, 46, 183,
                            57, 36, 197, 89, 56, 170, 121, 76, 143,
                            105, 66, 157, 137, 86, 130, 169, 106, 103});
        add(name, 0, 7, 5, {5, 3, 240, 14, 8, 233, 27, 17, 221, 41, 26, 210,
                            55, 34, 199, 68, 43, 187, 77, 48, 180,
                            15, 9, 232, 23, 14, 225, 37, 23, 213, 51, 32, 202,
                            64, 40, 191, 78, 49, 179, 87, 54, 172,
                            53, 33, 200, 62, 38, 193, 75, 47, 181, 89, 56, 170,
                            103, 64, 159, 116, 73, 147, 125, 78, 140,
                            91, 57, 168, 100, 62, 161, 114, 71, 149, 127, 80, 138,
                            141, 88, 127, 155, 97, 115, 163, 102, 108,
                            101, 63, 160, 110, 68, 153, 123, 77, 141, 137, 86, 130,
                            151, 94, 119, 164, 103, 107, 173, 108, 100});
    }
    spirula::sky::bake_finalize(expected, bake);
    session.bake_sky(actual);
    expect(same_bytes(actual.features_dc, expected.features_dc),
           "mixed PNG/TIFF sizes, quarter turns, downsampling and upsampling match pixels");
    snapshot(out, actual.features_dc);
    for (const char* missing : {"missing.png", "missing.tiff"}) {
        session.ds.image_filenames[1] = (images.root / missing).string();
        bool failed = false;
        try { session.bake_sky(actual); }
        catch (const std::runtime_error&) { failed = true; }
        expect(failed, "an unreadable image still fails the sky bake");
    }
}

void check_bake_color_space() {
    BakeImages images;
    TrainerSession session;
    session.log_fn = [](const std::string&) {};
    const double t[3] = {};
    const auto view = view_at(t, 1.0, 1.0, 4, 2);
    session.ds.num_cameras = 1;
    session.ds.train_indices = {0};
    session.ds.image_filenames = {(images.root / "rgb.png").string()};
    session.ds.widths = {4};
    session.ds.heights = {2};
    session.ds.intrins = {1.0f, 1.0f, 0.0f, 0.0f};
    session.ds.c2w.assign(view.c2w, view.c2w + 12);
    const auto geometry = rows_of({0.5f, 0.5f, 1.0f, 3.5f, 1.5f, 1.0f,
                                   20.0f, 0.0f, 1.0f});
    auto reference = geometry;
    session.bake_sky(reference);
    struct Space { bool linear; const char* gamut; const char* transfer; };
    const Space spaces[] = {
        {true, "", "srgb"}, {false, "ACEScg", "srgb"},
        {true, "Rec.2020", "srgb"}, {true, "ACEScg", "uncharted2"},
        {false, "", "filmic"}, {true, "", "aces"},
    };
    constexpr float basis = 0.28209479177387814f;
    for (const auto& space : spaces) {
        session.cfg.splat_color_is_linear = space.linear;
        session.cfg.splat_color_gamut = space.gamut;
        session.cfg.splat_color_transfer = space.transfer;
        auto actual = geometry;
        session.bake_sky(actual);
        const auto color = resolve_color(session.cfg);
        const auto matrix = spirula::gamut_to_rec709(color.splat_gamut);
        bool matches = true;
        for (size_t row = 0; row < actual.features_dc.size(); row += 3) {
            float displayed[3];
            for (int channel = 0; channel < 3; ++channel) {
                displayed[channel] = actual.features_dc[row + channel] * basis + 0.5f;
                if (!space.linear)
                    displayed[channel] = colorspace::srgb_to_linear(displayed[channel]);
            }
            colorspace::apply3x3(matrix, displayed);
            for (int channel = 0; channel < 3; ++channel) {
                const float expected = reference.features_dc[row + channel] * basis + 0.5f;
                const float rendered = colorspace::tone_encode(displayed[channel], color.splat_transfer);
                matches &= std::isfinite(rendered) && std::fabs(rendered - expected) < 3e-5f;
            }
        }
        expect(matches, std::string("sky colours retain their display value: ") +
                           (space.linear ? "linear " : "display ") + space.gamut +
                           " / " + space.transfer);
    }
}

ColmapPoints3D seed_points() {
    ColmapPoints3D points;
    for (int i = 0; i < 16; i++) {
        points.xyz.insert(points.xyz.end(), {i * 0.3, (i % 3) * 0.2, (i % 5) * 0.4});
        for (int c = 0; c < 3; c++) points.rgb.push_back((uint8_t)(i * 13 + c * 5));
    }
    return points;
}

struct TestPly {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("spirula-sky-" + std::to_string(std::chrono::steady_clock::now()
                                           .time_since_epoch().count()) + ".ply");
    TestPly() {
        TrainConfig cfg;
        cfg.preallocate_splat_tensors = false;
        auto seed = seed_splats(seed_points(), cfg, resolve_color(cfg));
        spirula::SplatCloud cloud;
        cloud.num = seed.num;
        cloud.sh_degree = cfg.sh_degree;
        cloud.means = std::move(seed.means);
        cloud.quats = std::move(seed.quats);
        cloud.scales = std::move(seed.scales);
        cloud.opacities = std::move(seed.opacities);
        cloud.features_dc = std::move(seed.features_dc);
        cloud.features_sh = std::move(seed.features_sh);
        for (size_t i = 0; i < cloud.features_sh.size(); i++)
            cloud.features_sh[i] = (float)((int)(i % 17) - 8) * 0.01f;
        spirula::write_splat_ply(cloud, path.string());
    }
    ~TestPly() { std::filesystem::remove(path); }
};

void check_resumed_sky_optimizer() {
    TrainerSession session;
    session.log_fn = [](const std::string&) {};
    session.cfg.resume = "synthetic-sky-checkpoint";
    session.cfg.create_sky_splats = false;
    session.cfg.use_fused_proj_bwd_optim = false;
    session.cfg.cap_max = 32;
    session.ds.points = seed_points();
    auto seed = session.seed_world(resolve_color(session.cfg), kSplatCount);
    const auto before = seed;
    spirula::sky::Rows rows;
    spirula::sky::generate_at_radius(1.0, rows);
    bool rejected = false;
    try { session.add_sky_model(seed, rows); }
    catch (const std::runtime_error&) { rejected = true; }
    expect(rejected, "a restored sky requires the fused optimizer even when creation is disabled");
    expect(seed.num == before.num && same_bytes(seed.means, before.means) &&
               same_bytes(seed.quats, before.quats) && same_bytes(seed.scales, before.scales) &&
               same_bytes(seed.opacities, before.opacities) &&
               same_bytes(seed.features_dc, before.features_dc) &&
               same_bytes(seed.features_sh, before.features_sh),
           "an unsupported restored sky is rejected before modifying seeds");
    spirula::sky::Rows none;
    expect(session.add_sky_model(seed, none) == 0,
           "an unfused run without sky rows remains supported");
}

void seed_case(std::ostream* out, const std::string& ply, int mode,
               bool preallocate, int degree, int cap = 64, bool verify = true,
               int lobes = 0, int64_t sky_count = kSplatCount) {
    TrainerSession session;
    session.log_fn = [](const std::string&) {};
    session.cfg.cap_max = cap;
    session.cfg.min_init_fraction = 0.5f;
    session.cfg.preallocate_splat_tensors = preallocate;
    session.cfg.sh_degree = degree;
    session.cfg.sb_lobes = lobes;
    session.ds.points = seed_points();
    if (mode != 0) session.cfg.init_ply = ply;
    session.cfg.init_ply_add_points = mode == 2;
    const auto color = resolve_color(session.cfg);
    spirula::SeedSplats plain;
    if (verify) plain = session.seed_world(color);
    auto seed = session.seed_world(color, sky_count);
    const int64_t live = seed.num;
    const int64_t capacity = preallocate ? std::max<int64_t>(live, cap) : live;
    const float* pointers[] = {seed.means.data(), seed.quats.data(), seed.scales.data(),
                              seed.opacities.data(), seed.features_dc.data(),
                              seed.features_sh.data()};
    spirula::sky::Rows rows;
    spirula::sky::generate_at_radius(1.0, rows, sky_count);
    session.cfg.resume = "synthetic-sky-checkpoint";
    expect(session.add_sky_model(seed, rows) == sky_count,
           "sky prefix is added to point/PLY seeds");
    expect(seed.num == live + sky_count &&
               seed.opacities.size() == (size_t)(capacity + sky_count),
           "live count and capacity include exactly one prefix");
    expect(pointers[0] == seed.means.data() && pointers[1] == seed.quats.data() &&
               pointers[2] == seed.scales.data() && pointers[3] == seed.opacities.data() &&
               pointers[4] == seed.features_dc.data() && pointers[5] == seed.features_sh.data(),
           "adding the sky does not reallocate any seed buffer");
    if (verify) {
        const int64_t K = ColorBasis{degree, session.cfg.sb_lobes}.coeffs();
        const auto unchanged = [&](const std::vector<float>& combined,
                                    const std::vector<float>& before, int64_t width) {
            return combined.size() == before.size() + (size_t)(sky_count * width) &&
                (before.empty() || std::memcmp(combined.data() + sky_count * width,
                                               before.data(), before.size() * sizeof(float)) == 0);
        };
        expect(plain.num == live && unchanged(seed.means, plain.means, 3) &&
                   unchanged(seed.quats, plain.quats, 4) &&
                   unchanged(seed.scales, plain.scales, 3) &&
                   unchanged(seed.opacities, plain.opacities, 1) &&
                   unchanged(seed.features_dc, plain.features_dc, 3) &&
                   unchanged(seed.features_sh, plain.features_sh, K * 3),
               "ordinary seeds and unused padding remain byte-identical");
        const auto prefix_matches = [](const std::vector<float>& combined,
                                        const std::vector<float>& prefix) {
            return std::memcmp(combined.data(), prefix.data(), prefix.size() * sizeof(float)) == 0;
        };
        expect(prefix_matches(seed.means, rows.means) &&
                   prefix_matches(seed.quats, rows.quats) &&
                   prefix_matches(seed.scales, rows.scales) &&
                   prefix_matches(seed.opacities, rows.opacities) &&
                   prefix_matches(seed.features_dc, rows.features_dc) &&
                   std::all_of(seed.features_sh.begin(),
                               seed.features_sh.begin() + sky_count * K * 3,
                               [](float value) { return bits(value) == 0u; }),
               "sky rows match the generated prefix, including zero higher coefficients");
    }
    snapshot(out, seed.means);
    snapshot(out, seed.quats);
    snapshot(out, seed.scales);
    snapshot(out, seed.opacities);
    snapshot(out, seed.features_dc);
    snapshot(out, seed.features_sh);
}

void check_seed_layout(std::ostream* out) {
    TestPly ply;
    for (bool preallocate : {false, true})
        for (int degree : {0, 3})
            for (int mode : {0, 1, 2})
                seed_case(out, ply.path.string(), mode, preallocate, degree);
    for (bool preallocate : {false, true}) {
        for (int mode : {0, 1, 2}) {
            seed_case(nullptr, ply.path.string(), mode, preallocate, 3, 8);
            seed_case(nullptr, ply.path.string(), mode, preallocate, 0, 64, true, 2);
        }
    }
    for (int64_t sky_count : {spirula::sky::kSplatCountLow,
                              spirula::sky::kSplatCountHigh,
                              spirula::sky::kSplatCountUltra})
        seed_case(nullptr, "", 0, true, 3, 64, true, 0, sky_count);
    TrainerSession session;
    session.ds.points = seed_points();
    session.cfg.cap_max = 32;
    auto seed = session.seed_world(resolve_color(session.cfg));
    const auto before = seed;
    spirula::sky::Rows none;
    expect(session.add_sky_model(seed, none) == 0 && seed.num == before.num &&
               same_bytes(seed.means, before.means) && same_bytes(seed.quats, before.quats) &&
               same_bytes(seed.scales, before.scales) &&
               same_bytes(seed.opacities, before.opacities) &&
               same_bytes(seed.features_dc, before.features_dc) &&
               same_bytes(seed.features_sh, before.features_sh),
           "a disabled sky leaves every seed buffer unchanged");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--benchmark-seeds") {
        const auto start = std::chrono::steady_clock::now();
        seed_case(nullptr, "", 0, true, 3, 1000000, false);
        std::printf("seed setup: %.3f ms\n", std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count());
        return g_failures ? 1 : 0;
    }
    std::ofstream out;
    if (argc == 3 && std::string(argv[1]) == "--snapshot") {
        out.open(argv[2], std::ios::binary);
        if (!out) return 2;
    } else if (argc != 1) {
        return 2;
    }
    std::ostream* sink = out.is_open() ? &out : nullptr;
    check_bake_images(sink);
    check_bake_color_space();
    check_seed_layout(sink);
    check_resumed_sky_optimizer();
    std::printf("sky_setup: %s (%d failed)\n", g_failures ? "FAILED" : "ok", g_failures);
    if (sink && !out) return 2;
    return g_failures ? 1 : 0;
}

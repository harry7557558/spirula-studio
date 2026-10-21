#include "app/tests/SkyTestSupport.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <limits>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace {

using namespace spirula::sky::test;
using spirula::sky::kSplatCount;

// IEEE-754 float32 reference coordinates.
struct GoldenRow { uint32_t i, x, y, z; };
constexpr GoldenRow kGolden[] = {
    {0u, 1130735751u, 3334606907u, 0u},
    {16u, 1148978181u, 3334589527u, 1146381867u},
    {32u, 1134336462u, 3334572164u, 1155780038u},
    {48u, 3297685534u, 3334554817u, 1156806849u},
    {64u, 3306769318u, 3334537485u, 1146694958u},
    {80u, 3307860300u, 3334520170u, 3296709503u},
    {96u, 3301061122u, 3334502872u, 3308083436u},
    {112u, 1143060901u, 3334485590u, 3310522129u},
    {128u, 1160902150u, 3334468324u, 3306169428u},
    {144u, 1165142690u, 3334451075u, 1117227945u},
    {160u, 1161837315u, 3334433842u, 1160284447u},
    {176u, 1143005729u, 3334416625u, 1166308126u},
    {192u, 3306296482u, 3334399424u, 1164867585u},
    {208u, 3314156518u, 3334382240u, 1152875663u},
    {224u, 3314293270u, 3334365072u, 3302978808u},
    {240u, 3306366673u, 3334347920u, 3314128224u},
    {256u, 1149618587u, 3334330785u, 3315434970u},
    {272u, 1166207789u, 3334313666u, 3310075776u},
    {288u, 1168799039u, 3334296563u, 1129642954u},
    {304u, 1166109890u, 3334279476u, 1164707974u},
    {320u, 1144777456u, 3334262406u, 1169296575u},
    {336u, 3309713697u, 3334245351u, 1167781036u},
    {352u, 3316885066u, 3334228313u, 1155489152u},
    {368u, 3316720869u, 3334211292u, 3306510432u},
    {384u, 3308408669u, 3334194285u, 3316569616u},
    {400u, 1152749570u, 3334177296u, 3317888974u},
    {416u, 1168338589u, 3334160323u, 3312730780u},
    {432u, 1171222057u, 3334143366u, 1136820768u},
    {448u, 1167684550u, 3334126425u, 1167164067u},
    {464u, 1145027304u, 3334109499u, 1171636151u},
    {480u, 3312634238u, 3334092591u, 1169544839u},
    {496u, 3319093883u, 3334075698u, 1157045720u},
};
constexpr uint32_t kGoldenRadiusBits = 0x46C2125Bu;
constexpr uint32_t kGoldenScaleBits = 1086720023u;
constexpr float kGoldenSpacing = 291.1075439453125f;

std::vector<uint8_t> stripe_image(int w, int h) {
    std::vector<uint8_t> px((size_t)w * h * 3, 0);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            for (int c = 0; c < 3; c++)
                px[((size_t)y * w + x) * 3 + c] = (uint8_t)(x * 255 / (w - 1));
    return px;
}

void check_radius() {
    const double origin[3] = {0.0, 0.0, 0.0};
    expect(spirula::sky::scene_radius(nullptr, 0) == 0.0, "empty cloud has no radius");
    expect(spirula::sky::scene_radius(origin, 1) == 0.0, "all-origin cloud has no radius");

    const double one[3] = {3.0, 4.0, 0.0};
    expect(std::fabs(spirula::sky::scene_radius(one, 1) - 7.5) < 1e-6,
           "radius is 1.5 * max|p|");

    const double three[9] = {3.0, 4.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 2.0};
    expect(std::fabs(spirula::sky::scene_radius(three, 3) - 7.5) < 1e-6,
           "radius is the max over the cloud, not the last point");
}

void check_generate() {
    const double cloud[3] = {1.0, 0.0, 0.0};
    spirula::sky::Rows rows;
    expect(spirula::sky::generate(cloud, 1, rows), "a cloud with extent builds a sky");
    expect((int64_t)rows.opacities.size() == kSplatCount &&
               (int64_t)rows.means.size() == kSplatCount * 3,
           "the prefix is exactly 65536 rows");
    expect(rows.opacities[0] == std::numeric_limits<float>::infinity() &&
               bits(rows.opacities[12345]) == 0x7F800000u,
           "opacity is the +inf sentinel");
    expect(rows.quats[0] == 1.0f && rows.quats[1] == 0.0f && rows.quats[2] == 0.0f &&
               rows.quats[3] == 0.0f,
           "rotation is the identity quaternion");
    expect(rows.scales[0] == rows.scales[2] && rows.scales[3] == rows.scales[5],
           "each row has one log spacing on all three axes");
    expect(rows.features_dc[0] == 0.0f && rows.features_dc[3 * 3] == 0.0f,
           "DC starts at zero until the bake");
    expect(rows.scales[0] != rows.scales[3],
           "splat spacing shrinks toward the horizon");
    expect(rows.spacing > 0.0f, "the base spacing is positive");
    expect(std::fabs(rows.radius - 1.5) < 1e-9, "radius is kept for the bake");

    float xyz[3];
    spirula::sky::spiral_point(0, rows.radius, xyz);
    expect(xyz[0] == rows.means[0] && xyz[1] == rows.means[1] && xyz[2] == rows.means[2],
           "the whole prefix uses spiral_point");

    spirula::sky::Rows none;
    expect(!spirula::sky::generate(nullptr, 0, none) && none.means.empty(),
           "no sky is built when the cloud has no extent");
}

void check_counts() {
    for (int64_t count : {spirula::sky::kSplatCountLow,
                          spirula::sky::kSplatCountMedium,
                          spirula::sky::kSplatCountHigh,
                          spirula::sky::kSplatCountUltra}) {
        const double radius = 10.0;
        spirula::sky::Rows rows;
        spirula::sky::generate_at_radius(radius, rows, count);
        expect((int64_t)rows.opacities.size() == count &&
                   (int64_t)rows.means.size() == count * 3,
               "sky count selects the prefix size");
        const float expected = 1.5f * std::sqrt(4.0f * (float)radius * (float)radius /
                                                (float)count);
        expect(std::fabs(rows.spacing - expected) < 1e-4f,
               "spacing follows the sky count");
        const float last_y = rows.means[(size_t)(count - 1) * 3 + 1];
        const double u = 1.0 - 1.0 / (double)count;
        const double band = u * (spirula::sky::kHorizonBias +
                                 (1.0 - spirula::sky::kHorizonBias) * u * u);
        expect(std::fabs(last_y - (float)(radius * band)) < 1e-4f,
               "the last row closes the sphere");
        float xyz[3];
        spirula::sky::spiral_point(0, radius, xyz, count);
        expect(xyz[0] == rows.means[0] && xyz[1] == rows.means[1] &&
                   xyz[2] == rows.means[2],
               "each count uses its own spiral");
    }
}

void check_azimuth_precision() {
    const int64_t count = spirula::sky::kSplatCountUltra;
    spirula::sky::Rows rows;
    spirula::sky::generate_at_radius(10.0, rows, count);
    std::vector<float> angles;
    angles.reserve((size_t)(count / 4));
    for (int64_t i = count - count / 4; i < count; ++i)
        angles.push_back(std::atan2(rows.means[(size_t)i * 3 + 2],
                                    rows.means[(size_t)i * 3]));
    std::sort(angles.begin(), angles.end());
    float max_gap = 0.0f;
    for (size_t i = 1; i < angles.size(); ++i)
        max_gap = std::max(max_gap, angles[i] - angles[i - 1]);
    expect(max_gap < 0.01f, "ultra azimuths are not quantized");
}

void check_horizon_median() {
    spirula::sky::Rows rows;
    spirula::sky::generate_at_radius(10.0, rows, spirula::sky::kSplatCountLow);
    std::fill(rows.features_dc.begin(), rows.features_dc.end(), 0.0f);
    const int64_t spike = spirula::sky::kSplatCountLow / 2;
    rows.features_dc[(size_t)spike * 3] = 1.0f;
    rows.features_dc[0] = 1.0f;
    spirula::sky::median_filter_horizon(rows);
    expect(rows.features_dc[(size_t)spike * 3] == 0.0f,
           "a horizon outlier is replaced by its neighbours' median");
    expect(rows.features_dc[0] == 1.0f,
           "rows outside the horizon band are untouched");
}

void check_golden_spiral() {
    const double radius = from_bits(kGoldenRadiusBits);
    int bad = 0;
    for (const GoldenRow& g : kGolden) {
        float xyz[3];
        spirula::sky::spiral_point((int64_t)g.i, radius, xyz);
        if (bits(xyz[0]) != g.x || bits(xyz[1]) != g.y || bits(xyz[2]) != g.z) bad++;
    }
    expect(bad == 0, "spiral rows match the pinned bit patterns (" +
                         std::to_string(sizeof(kGolden) / sizeof(kGolden[0])) +
                         " rows, " + std::to_string(bad) + " off)");

    spirula::sky::Rows rows;
    spirula::sky::generate_at_radius(radius, rows);
    expect(rows.spacing == kGoldenSpacing, "spacing is bit-exact");
    expect(bits(rows.scales[0]) == kGoldenScaleBits, "scale is bit-exact");
    expect(bits(rows.means[1]) == kGolden[0].y, "row 0's y matches the pinned bits");
    expect(rows.quats[0] == 1.0f && rows.quats[4] == 1.0f, "every rotation is the identity");
}

void check_bake_sampler() {
    const double t[3] = {0.0, 0.0, 0.0};
    spirula::sky::View view = view_at(t, 1.0, 1.0, 2, 2);
    std::vector<uint8_t> img = stripe_image(2, 2);
    view.rgb = img.data();

    // px = 0.5 is pixel 0, 1.5 is pixel 1, 1.0 straddles both, 0.25 clamps.
    spirula::sky::Rows rows = rows_of({-0.5f, 0.0f, 1.0f,
                                       0.5f, 0.0f, 1.0f,
                                       0.0f, 0.0f, 1.0f,
                                       -0.75f, 0.0f, 1.0f});
    spirula::sky::Bake bake;
    spirula::sky::bake_accumulate(rows, view, bake);
    expect(bake.hits[0] == 1u && bake.hits[1] == 1u && bake.hits[2] == 1u &&
               bake.hits[3] == 1u,
           "rows in frame are covered");
    expect(std::fabs(bake.sum[0] - 0.0f) < 1e-6f,
           "px = 0.5 is pixel 0, not the halfway blend a missing shift gives");
    expect(std::fabs(bake.sum[3] - 1.0f) < 1e-6f, "px = 1.5 is pixel 1");
    expect(std::fabs(bake.sum[6] - 0.5f) < 1e-6f, "px = 1.0 blends pixels 0 and 1");
    expect(std::fabs(bake.sum[9] - 0.0f) < 1e-6f, "a sub-pixel edge clamps to pixel 0");

    spirula::sky::Rows behind = rows_of({0.0f, 0.0f, -1.0f});
    spirula::sky::Bake b2;
    spirula::sky::bake_accumulate(behind, view, b2);
    expect(b2.hits[0] == 0u, "a row behind the camera is not sampled");

    spirula::sky::Rows outside = rows_of({5.0f, 0.0f, 1.0f});
    spirula::sky::Bake b3;
    spirula::sky::bake_accumulate(outside, view, b3);
    expect(b3.hits[0] == 0u, "a row outside the frame is not sampled");
}

void check_view_c2w_is_the_dataset_record() {
    expect(sizeof(spirula::sky::View::c2w) == 12 * sizeof(float),
           "c2w is the dataset's own float record, not a wider type");

    // A camera at (0, 0, -5) looking down CV +Z: three rows of four floats.
    const float table[12] = {1, 0, 0, 0,
                             0, -1, 0, 0,
                             0, 0, -1, -5};
    spirula::sky::View view;
    std::memcpy(view.c2w, table, sizeof table);
    view.width = 4;
    view.height = 4;
    view.fx = view.fy = 1.0;
    view.cx = view.cy = 2.0;
    std::vector<uint8_t> img(4 * 4 * 3, 255);
    view.rgb = img.data();

    spirula::sky::Rows rows = rows_of({0.0f, 0.0f, 0.0f});
    spirula::sky::Bake bake;
    spirula::sky::bake_accumulate(rows, view, bake);
    expect(bake.hits[0] == 1u && std::fabs(bake.sum[0] - 1.0f) < 1e-6f,
           "a float camera record projects its own row into the frame");
}

void check_bake_finalize() {
    const double t[3] = {0.0, 0.0, 0.0};

    std::vector<uint8_t> flat(2 * 2 * 3, 64);
    spirula::sky::View view = view_at(t, 1.0, 1.0, 2, 2);
    view.rgb = flat.data();
    spirula::sky::Rows rows = rows_of({0.25f, 0.25f, 1.0f, -0.25f, -0.25f, 1.0f});
    spirula::sky::Bake bake;
    spirula::sky::bake_accumulate(rows, view, bake);
    spirula::sky::bake_finalize(rows, bake);
    const float want = ((float)64 / 255.0f - 0.5f) / 0.28209479177387814f;
    expect(std::fabs(rows.features_dc[0] - want) < 1e-6f &&
               std::fabs(rows.features_dc[3] - want) < 1e-6f,
           "a uniform image becomes that colour's SH DC");

    spirula::sky::Rows split = rows_of({0.25f, 0.25f, 1.0f,
                                        5.0f, 5.0f, 1.0f});
    spirula::sky::Bake b3;
    spirula::sky::bake_accumulate(split, view, b3);
    expect(b3.hits[0] == 1u && b3.hits[1] == 0u, "the far row is not covered");
    spirula::sky::bake_finalize(split, b3);
    expect(split.features_dc[3] == split.features_dc[0],
           "an uncovered row copies the nearest covered row");

    spirula::sky::View none = view_at(t, 1.0, 1.0, 2, 2);
    spirula::sky::Rows fallback = rows_of({0.25f, 0.25f, 1.0f});
    spirula::sky::Bake b4;
    spirula::sky::bake_accumulate(fallback, none, b4);
    spirula::sky::bake_finalize(fallback, b4);
    expect(fallback.features_dc[0] == 1.0f, "no coverage at all falls back to DC = 1");
}

void check_fill_threads(std::ostream* out) {
#ifdef _OPENMP
    const int saved_threads = omp_get_max_threads();
#endif
    for (int count : {1024, 16385, 65536}) {
        for (int stride : {1, 2, 257, 65536}) {
            spirula::sky::Rows serial;
            spirula::sky::generate_at_radius(10.0, serial);
            serial.means.resize((size_t)count * 3);
            serial.features_dc.resize((size_t)count * 3);
            auto parallel = serial;
            spirula::sky::Bake bake;
            bake.sum.resize((size_t)count * 3);
            bake.hits.resize(count);
            for (int i = 0; i < count; i++) {
                bake.hits[i] = i % stride == 0 ? (uint32_t)(i % 5 + 1) : 0u;
                for (int c = 0; c < 3; c++)
                    bake.sum[(size_t)i * 3 + c] = (float)((i * 13 + c * 7) % 256) / 256;
            }
#ifdef _OPENMP
            omp_set_num_threads(1);
#endif
            spirula::sky::bake_finalize(serial, bake);
#ifdef _OPENMP
            omp_set_num_threads(4);
#endif
            spirula::sky::bake_finalize(parallel, bake);
            expect(same_bytes(serial.features_dc, parallel.features_dc),
                   "serial/parallel fill: rows " + std::to_string(count) +
                   ", coverage stride " + std::to_string(stride));
            snapshot(out, serial.features_dc);
#ifdef _OPENMP
            if (count == 16385 && stride == 2) {
                auto nested = parallel;
                #pragma omp parallel num_threads(2)
                {
                    #pragma omp single
                    {
                        spirula::sky::bake_finalize(nested, bake);
                    }
                }
                expect(same_bytes(serial.features_dc, nested.features_dc),
                       "nested OpenMP fill matches serial colours");
            }
#endif
        }
    }
#ifdef _OPENMP
    omp_set_num_threads(saved_threads);
#endif
}

spirula::sky::View bake_view(int frame, int model, int tier) {
    const double t[3] = {0.2 * (frame % 3), -0.1, 0.3};
    auto view = view_at(t, 22.0, 22.0, 64, 48);
    view.model = model;
    view.tier = tier;
    view.dist[0] = 0.01f;
    view.dist[1] = -0.001f;
    const double yaw = frame * 0.53, pitch = (frame % 3 - 1) * 0.5;
    const double cy = std::cos(yaw), sy = std::sin(yaw);
    const double cp = std::cos(pitch), sp = std::sin(pitch);
    const double right[3] = {cy, 0, -sy};
    const double down[3] = {-sy * sp, cp, -cy * sp};
    const double forward[3] = {sy * cp, sp, cy * cp};
    for (int k = 0; k < 3; k++) {
        view.c2w[k * 4] = (float)right[k];
        view.c2w[k * 4 + 1] = (float)-down[k];
        view.c2w[k * 4 + 2] = (float)-forward[k];
    }
    return view;
}

void accumulate_views(const spirula::sky::Rows& rows, spirula::sky::Bake& bake,
                      int model, int tier, int frames) {
    std::vector<uint8_t> image(64 * 48 * 3);
    for (int frame = 0; frame < frames; frame++) {
        for (size_t i = 0; i < image.size(); i++)
            image[i] = (uint8_t)((i * 37 + frame * 11) % 256);
        auto view = bake_view(frame, model, tier);
        view.rgb = image.data();
        spirula::sky::bake_accumulate(rows, view, bake);
    }
}

void check_bake_threads(std::ostream* out) {
#ifdef _OPENMP
    const int saved_threads = omp_get_max_threads();
#endif
    for (int model = 0; model < 4; model++) {
        for (int tier : {0, 1, 2}) {
            spirula::sky::Rows serial;
            spirula::sky::generate_at_radius(10.0, serial);
            auto parallel = serial;
            spirula::sky::Bake a, b;
#ifdef _OPENMP
            omp_set_num_threads(1);
#endif
            accumulate_views(serial, a, model, tier, 12);
#ifdef _OPENMP
            omp_set_num_threads(4);
#endif
            accumulate_views(parallel, b, model, tier, 12);
            expect(same_bytes(a.sum, b.sum) && same_bytes(a.hits, b.hits),
                   "serial/parallel sums and hits: model " + std::to_string(model) +
                   ", tier " + std::to_string(tier));
#ifdef _OPENMP
            if (model == 0 && tier == 0) {
                spirula::sky::Bake nested;
                #pragma omp parallel num_threads(2)
                {
                    #pragma omp single
                    {
                        accumulate_views(parallel, nested, model, tier, 12);
                    }
                }
                expect(same_bytes(a.sum, nested.sum) && same_bytes(a.hits, nested.hits),
                       "nested OpenMP uses the same serial accumulation");
            }
#endif
            spirula::sky::bake_finalize(serial, a);
            spirula::sky::bake_finalize(parallel, b);
            expect(same_bytes(serial.features_dc, parallel.features_dc),
                   "serial/parallel final colours match byte for byte");
            snapshot(out, a.sum);
            snapshot(out, a.hits);
            snapshot(out, serial.features_dc);
        }
    }
#ifdef _OPENMP
    spirula::sky::Rows tail;
    spirula::sky::generate_at_radius(10.0, tail);
    tail.means.resize(16385 * 3);
    spirula::sky::Bake serial_tail, parallel_tail;
    omp_set_num_threads(1);
    accumulate_views(tail, serial_tail, 0, 0, 12);
    omp_set_num_threads(4);
    accumulate_views(tail, parallel_tail, 0, 0, 12);
    expect(same_bytes(serial_tail.sum, parallel_tail.sum) &&
               same_bytes(serial_tail.hits, parallel_tail.hits) &&
               serial_tail.hits.back() > 0,
           "a partial final chunk accumulates its last row exactly once");
    omp_set_num_threads(saved_threads);
#endif
}

void benchmark_bake() {
#ifdef _OPENMP
    const int saved_threads = omp_get_max_threads();
#endif
    for (int count : {1024, 4096, 16384, 65536}) {
        spirula::sky::Rows rows;
        spirula::sky::generate_at_radius(10.0, rows);
        for (int i = 0; i < count; i++)
            for (int c = 0; c < 3; c++)
                rows.means[(size_t)i * 3 + c] =
                    rows.means[(size_t)(i * (kSplatCount / count)) * 3 + c];
        rows.means.resize((size_t)count * 3);
        for (int model : {0, 1}) {
            for (int threads : {1, 2, 4, 8, 12}) {
#ifdef _OPENMP
                omp_set_num_threads(threads);
#endif
                spirula::sky::Bake bake;
                accumulate_views(rows, bake, model, 0, 4);
                const auto start = std::chrono::steady_clock::now();
                accumulate_views(rows, bake, model, 0, 128);
                const double ms = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - start).count();
                std::printf("bake rows=%d model=%d threads=%d: %.3f ms/image\n",
                            count, model, threads, ms / 128);
            }
        }
    }
    spirula::sky::Rows rows;
    spirula::sky::generate_at_radius(10.0, rows);
    spirula::sky::Bake bake;
    bake.sum.assign(rows.means.size(), 0.25f);
    bake.hits.assign(rows.opacities.size(), 1u);
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 32; i++) spirula::sky::bake_finalize(rows, bake);
    std::printf("fully covered finalize: %.3f ms\n",
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - start).count() / 32);
    for (int stride : {2, 257, 65536}) {
        for (size_t i = 0; i < bake.hits.size(); i++)
            bake.hits[i] = i % stride == 0 ? 1u : 0u;
        for (int threads : {1, 4, 8, 12}) {
#ifdef _OPENMP
            omp_set_num_threads(threads);
#endif
            const auto begin = std::chrono::steady_clock::now();
            for (int i = 0; i < 16; i++) spirula::sky::bake_finalize(rows, bake);
            std::printf("partial finalize stride=%d threads=%d: %.3f ms\n",
                        stride, threads, std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - begin).count() / 16);
        }
    }
#ifdef _OPENMP
    omp_set_num_threads(saved_threads);
#endif
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--golden") {
        const double radius = from_bits(kGoldenRadiusBits);
        for (const GoldenRow& g : kGolden) {
            float xyz[3];
            spirula::sky::spiral_point((int64_t)g.i, radius, xyz);
            std::printf("    {%uu, %uu, %uu, %uu},\n", g.i, bits(xyz[0]),
                        bits(xyz[1]), bits(xyz[2]));
        }
        spirula::sky::Rows rows;
        spirula::sky::generate_at_radius(radius, rows);
        std::printf("    // scale %uu\n", bits(rows.scales[0]));
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--benchmark") {
        benchmark_bake();
        return 0;
    }
    std::ofstream out;
    if (argc == 3 && std::string(argv[1]) == "--snapshot") {
        out.open(argv[2], std::ios::binary);
        if (!out) return 2;
    } else if (argc != 1) {
        return 2;
    }
    std::ostream* sink = out.is_open() ? &out : nullptr;
    check_radius();
    check_generate();
    check_counts();
    check_azimuth_precision();
    check_golden_spiral();
    check_bake_sampler();
    check_horizon_median();
    check_view_c2w_is_the_dataset_record();
    check_bake_finalize();
    check_fill_threads(sink);
    check_bake_threads(sink);
    std::printf("sky_model: %s (%d failed)\n", g_failures ? "FAILED" : "ok", g_failures);
    if (sink && !out) return 2;
    return g_failures ? 1 : 0;
}

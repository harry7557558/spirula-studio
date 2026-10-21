#pragma once

// Sky geometry and CPU color accumulation, independent of image I/O and the engine.

#include <cstdint>
#include <vector>

namespace spirula {
namespace sky {

// The frozen prefix is skipped in 256-row blocks, so every count is a multiple of 256.
inline constexpr int64_t kSplatCountLow    = int64_t(1) << 15;
inline constexpr int64_t kSplatCountMedium = int64_t(1) << 16;
inline constexpr int64_t kSplatCountHigh   = int64_t(1) << 17;
inline constexpr int64_t kSplatCountUltra  = int64_t(1) << 18;
inline constexpr int64_t kSplatCount = kSplatCountMedium;
// Latitude warp: 10x rows at the horizon; splats 0.45x there, 1.7x at the poles.
inline constexpr double kHorizonBias = 0.1;

// 1.5 * max|p|; 0 when the cloud is empty or all at the origin.
double scene_radius(const double* xyz, int64_t count);

struct Rows {
    double radius = 0.0;
    float  spacing = 0.0f;           // base spacing; scales store its log per row
    std::vector<float> means;        // [count, 3]
    std::vector<float> quats;        // [count, 4], (w, x, y, z)
    std::vector<float> scales;       // [count, 3]  log spacing on all three axes
    std::vector<float> opacities;    // [count]     +inf: sigmoid(+inf) = 1
    std::vector<float> features_dc;  // [count, 3], SH DC
};

void spiral_point(int64_t index, double radius, float xyz[3],
                  int64_t splat_count = kSplatCount);

// False, and `rows` untouched, when the cloud yields no radius.
bool generate(const double* xyz, int64_t point_count, Rows& rows,
              int64_t splat_count = kSplatCount);

void generate_at_radius(double radius, Rows& rows,
                        int64_t splat_count = kSplatCount);

struct View {
    const uint8_t* rgb = nullptr;    // [height * width * 3], 8-bit sRGB
    int width = 0, height = 0;
    int model = 0, tier = 0;
    double fx = 0.0, fy = 0.0, cx = 0.0, cy = 0.0;
    float dist[8] = {};
    float c2w[12] = {};              // row-major OpenGL camera-to-world, 3x4
};

struct Bake {
    std::vector<float> sum;          // [rows, 3]
    std::vector<uint32_t> hits;      // [rows]
};

void bake_accumulate(const Rows& rows, const View& view, Bake& bake);

void bake_finalize(Rows& rows, const Bake& bake);

// Median-filter the horizon band's DC colours; a wrong-side splat would show as a pit/spike.
void median_filter_horizon(Rows& rows);

}  // namespace sky
}  // namespace spirula

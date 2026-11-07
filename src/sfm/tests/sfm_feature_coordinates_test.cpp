#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

#include "sfm/Pipeline.h"
#include "sfm/core/FeatureCoordinates.h"
#include "sfm/core/Mask.h"
#include "sfm/feature/Sift.h"
#include "sfm/tests/TestMain.h"

using namespace sfm;
namespace fs = std::filesystem;

static int coordinateTest(int argc, char** argv) {
    if (argc == 5 && std::string(argv[1]) == "--shift-features") {
        const bool sift = std::string(argv[4]) == "sift";
        if (!sift && std::string(argv[4]) != "learned")
            throw std::runtime_error("expected sift or learned");
        for (const auto& entry : fs::recursive_directory_iterator(argv[2])) {
            if (!entry.is_regular_file() || entry.path().extension() != ".bin") continue;
            auto features = readFeatures(entry.path().string());
            correctStoredKeypoints(features, sift);
            const auto output = fs::path(argv[3]) / fs::relative(entry.path(), argv[2]);
            fs::create_directories(output.parent_path());
            writeFeatures(output.string(), features);
        }
        return 0;
    }
    if (argc != 1) throw std::runtime_error("expected --shift-features INPUT_DIR OUTPUT_DIR sift|learned");
    bool ok = true;
    SiftOptions options;
    options.verbose = false;
    options.max_num_features = 0;
    SiftExtractor extractor(options);
    double mean_dx = 0, mean_dy = 0;
    for (int trial = 0; trial < 32; ++trial) {
        GrayImage image;
        image.width = image.height = 256;
        image.orig_width = image.orig_height = 256;
        image.data.resize(256 * 256);
        const int phase = trial % 8;
        const float cx = 112.125f + 0.5f * phase;
        const float cy = 128.375f + 0.5f * ((3 * phase) % 8);
        const float radius = 5.0f + 2.0f * (trial / 8);
        for (int y = 0; y < 256; ++y)
            for (int x = 0; x < 256; ++x)
                image.data[256*y+x] = 0.9f - 0.8f * std::exp(-((x-cx)*(x-cx)+(y-cy)*(y-cy)) /
                                                            (2*radius*radius));
        const FeatureSet raw = extractor.extract(image);
        auto corrected = raw, stored = raw;
        centerExtractedKeypoints(corrected, true);
        finishFeatures(corrected, image, true);
        scaleKeypoints(stored, image.orig_width, image.orig_height);
        correctStoredKeypoints(stored, true);
        double best = 1e9, equivalent = 0, dx = 0, dy = 0;
        for (size_t i = 0; i < corrected.keypoints.size(); ++i) {
            const auto& p = corrected.keypoints[i];
            const double error = std::hypot(p.x - (cx+0.5f), p.y - (cy+0.5f));
            if (error < best) { best = error; dx = p.x - (cx+0.5f); dy = p.y - (cy+0.5f); }
            equivalent = std::max(equivalent, (double)std::hypot(p.x-stored.keypoints[i].x,
                                                                 p.y-stored.keypoints[i].y));
        }
        const bool pass = best < 0.25 && equivalent < 1e-4;
        mean_dx += dx / 32;
        mean_dy += dy / 32;
        ok &= pass;
        std::printf("%s: SIFT physical blob %d, corner error %.6f px, stored correction %.6g px\n",
                    pass ? "PASS" : "FAIL", trial, best, equivalent);
    }
    const bool unbiased = std::hypot(mean_dx, mean_dy) < 0.04;
    ok &= unbiased;
    std::printf("%s: SIFT mean corner bias %.6f, %.6f px across subpixel phases\n",
                unbiased ? "PASS" : "FAIL", mean_dx, mean_dy);
    GrayImage image;
    image.width = 40; image.height = 30;
    image.orig_width = 92; image.orig_height = 51;
    image.rgb.resize(40 * 30 * 3);
    for (int y = 0; y < 30; ++y)
        for (int x = 0; x < 40; ++x) {
            image.rgb[3*(40*y+x)] = (uint8_t)x;
            image.rgb[3*(40*y+x)+1] = (uint8_t)y;
            image.rgb[3*(40*y+x)+2] = 0;
        }
    FeatureSet features;
    features.width = 40; features.height = 30;
    features.keypoints.push_back({17.5f, 23.5f, 0, 0, 1});
    centerExtractedKeypoints(features, false);
    sampleFeatureColors(features, image);
    Mask mask;
    mask.width = 40; mask.height = 30;
    mask.bits.assign(40 * 30, 0);
    mask.bits[40 * 23 + 17] = 1;
    const bool sampled = features.colors[0] == 17 && features.colors[1] == 23 &&
                         applyMask(features, mask) == 0;
    finishFeatures(features, image, true);
    const bool scaled = features.count() == 1 &&
                        std::fabs(features.keypoints[0].x - 17.5f * 2.3f) < 1e-5 &&
                        std::fabs(features.keypoints[0].y - 23.5f * 1.7f) < 1e-5;
    std::printf("%s: learned corner coordinates sample the correct color and mask pixel\n",
                sampled ? "PASS" : "FAIL");
    std::printf("%s: learned corners rescale without a half-pixel bias\n", scaled ? "PASS" : "FAIL");
    return ok && sampled && scaled ? 0 : 1;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, coordinateTest); }

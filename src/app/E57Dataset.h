#pragma once

// A laser scan written out as a dataset the trainer opens: the images an E57
// file carries, their poses, and its point cloud thinned into seed points, in
// the Nerfstudio layout. No reconstruction runs -- the scanner placed every
// image. docs/datasets.md "E57 laser scans".

#include "data/E57Reader.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace app {

struct E57DatasetOptions {
    std::string input;
    std::string output;
    int64_t seed_points = 500000;    // 0 writes no cloud
    bool all_points = false;         // every point a seed, nothing thinned
    bool depth_maps = true;          // depths/ and normals/ from the scan
    bool pinhole = true;
    bool spherical = true;
    bool overwrite = false;          // write into a folder that is not empty
};

// What comparing the photos with the coloured points found.
enum class E57Check { NotRun, NoColor, Agree, Fixed, Unsure };

struct E57DatasetResult {
    int64_t images = 0;
    int64_t seed_points = 0;
    int64_t depth_maps = 0;
    double voxel = 0;                // metres; 0 when every point was kept
    E57Check check = E57Check::NotRun;
    bool cancelled = false;
};

// Every line it prints goes to `log`, already translated. Throws on a file it
// cannot read or a scan with no usable image.
E57DatasetResult write_e57_dataset(const E57DatasetOptions& opt,
                                   const std::function<void(const std::string&)>& log,
                                   const std::atomic<bool>* cancel = nullptr);

// Beside the scan, named after it.
std::string default_e57_dataset_dir(const std::string& input);

// Why an image does not become a camera.
enum class E57Skip { None, NoPose, NoPixels, Cylindrical, Uncalibrated,
                     PartialPanorama, BadPinhole };

// One image as transforms.json spells it: OpenGL camera-to-world, row-major
// 3x4, in the file frame; pixel centres at +0.5.
struct E57Camera {
    const char* model = "";          // "PINHOLE" or "EQUIRECTANGULAR"
    double c2w[12] = {};
    double fx = 0, fy = 0, cx = 0, cy = 0;
    int64_t width = 0, height = 0;
};
E57Skip e57_camera(const spirula::e57::Image& im, E57Camera& out);

// The images a dataset from `input` holds, under the same names, into
// `images_dir` (emptied first): the frames masking is tried on before the
// dataset exists. False when `cancel` stopped it.
bool extract_e57_images(const std::string& input, const std::string& images_dir,
                        bool pinhole, bool spherical,
                        const std::atomic<bool>* cancel = nullptr);

// What the E57 screen previews: every image that would become a camera, and a
// uniform draw of about `max_points` points, in the file frame. Empty when
// `cancel` stopped it.
struct E57Preview {
    std::vector<E57Camera> cameras;
    std::vector<double> xyz;
    std::vector<uint8_t> rgb;
};
E57Preview read_e57_preview(const std::string& path, int64_t max_points,
                            const std::atomic<bool>* cancel = nullptr);

// Thins the cloud in place to at most `target` points, one per occupied voxel
// (the mean of its points). Returns the voxel edge, 0 when nothing was merged.
double thin_to_voxels(std::vector<double>& xyz, std::vector<uint8_t>& rgb,
                      int64_t target);

}  // namespace app

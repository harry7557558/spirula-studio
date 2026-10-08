#include "app/GeometryWarp.h"
#include "data/CameraMath.h"
#include "data/DatasetParser.h"
#include "sfm/core/Camera.h"
#include "sfm/core/Pose.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <map>
#include <set>
#include <stdexcept>

namespace {
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }

sfm::CamModel model(const std::string& name) {
    if (name == "OPENCV") return sfm::CamModel::OpenCV;
    if (name == "OPENCV_FISHEYE") return sfm::CamModel::OpenCVFisheye;
    if (name == "THIN_PRISM_FISHEYE") return sfm::CamModel::ThinPrismFisheye;
    if (name == "EQUIRECTANGULAR") return sfm::CamModel::Equirect;
    if (name == "PINHOLE") return sfm::CamModel::Pinhole;
    throw std::runtime_error("unsupported calibration in camera diagnostic");
}

void verify(const ColmapCamera& raw, const app::GeometryCamera& camera, const sfm::Mat3& rotation) {
    sfm::Camera expected;
    expected.width = (int)raw.width; expected.height = (int)raw.height; expected.model = model(raw.model);
    sfm::unpackColmap(expected, raw.params.data());
    const double scale = std::min(1.0, 1280.0 / std::max(camera.width, camera.height));
    app::GeometryWarp warp;
    warp.plan(camera, std::max(16, (int)(camera.width * scale) / 16 * 16),
              std::max(16, (int)(camera.height * scale) / 16 * 16),
              camhost::splits_to_pinhole_faces(camera.model, camera.width, camera.height, camera.fx, camera.fy),
              16, 1280, app::FaceRes::Output, 0, true, app::FaceLayout::Cube);
    size_t checked = 0;
    double max_pixel_error = 0, max_ray_error = 0;
    camhost::Camera source_camera;
    source_camera.model = camera.model; source_camera.tier = camera.distortion;
    source_camera.width = camera.width; source_camera.height = camera.height;
    source_camera.fx = camera.fx; source_camera.fy = camera.fy;
    source_camera.cx = camera.cx; source_camera.cy = camera.cy;
    std::copy_n(camera.dist,8,source_camera.dist);
    source_camera.source_model = camera.source_model;
    std::copy_n(camera.source_params,16,source_camera.source_params);
    for (int face = 0; face < warp.faces(); ++face) {
        const int w = warp.faceWidth(face), h = warp.faceHeight(face);
        for (int y = 0; y < h; y += std::max(1, h / 17)) for (int x = 0; x < w; x += std::max(1, w / 19)) {
            double source[2];
            if (!warp.faceToSource(face, x + 0.5, y + 0.5, source)) continue;
            const auto ray = warp.faceRay(face, x + 0.5, y + 0.5);
            const sfm::Vec3 bearing{ray[0], ray[1], ray[2]};
            const auto source_pixel = expected.project(bearing);
            double dx = source_pixel.x - source[0];
            if (expected.isSpherical()) dx = std::remainder(dx, (double)camera.width);
            max_pixel_error = std::max(max_pixel_error, std::hypot(dx, source_pixel.y - source[1]));
            const auto inverse = expected.bearing({source[0], source[1]});
            double source_ray[3];
            require(camhost::pixel_ray(source_camera,source,source_ray),"source calibration inverse rejected a visible ray");
            max_ray_error = std::max(max_ray_error,(sfm::Vec3{source_ray[0],source_ray[1],source_ray[2]} - bearing.normalized()).norm());
            const auto world = sfm::mul(sfm::transpose(rotation), inverse);
            const auto original = sfm::mul(rotation, world).normalized();
            max_ray_error = std::max(max_ray_error, (original - bearing.normalized()).norm());
            const size_t i = (size_t)y * w + x;
            const auto& sampled = warp.faceSourcePixels(face);
            max_pixel_error = std::max(max_pixel_error, std::hypot(
                sampled[2 * i] * camera.width / warp.sampleWidth() - source[0],
                sampled[2 * i + 1] * camera.height / warp.sampleHeight() - source[1]));
            ++checked;
        }
    }
    std::printf("camera %d %s: %d faces, %zu samples, source error %.6f px, bearing error %.9f\n",
                raw.camera_id, raw.model.c_str(), warp.faces(), checked, max_pixel_error, max_ray_error);
    require(checked > 0 && max_pixel_error < 0.005 && max_ray_error < 2e-6,
            "rectified dense pixels disagree with SfM source projection or bearings");
}
}

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("usage: dense_camera_test COLMAP_DATASET");
        const std::string root = argv[1], recon = find_colmap_model(root, "");
        const auto cameras = read_cameras_binary(recon);
        const auto images = read_images_binary(recon);
        DatasetParserConfig config; config.require_image_files = false; config.center_mode = "camera-mean";
        const auto dataset = parse_colmap_dataset(root, config);
        std::map<std::string, const ColmapImage*> by_name;
        for (const auto& [id, image] : images) by_name.emplace(image.name, &image);
        std::set<int32_t> checked;
        for (size_t i = 0; i < (size_t)dataset.num_cameras; ++i) {
            const std::string name = dsparse::relative_under(dataset.image_filenames[i],
                (std::filesystem::path(root) / "images").string());
            const auto& image = *by_name.at(name);
            if (!checked.insert(image.camera_id).second) continue;
            const auto& raw = cameras.at(image.camera_id);
            app::GeometryCamera camera;
            camera.model = dataset.camera_models[i]; camera.distortion = dataset.camera_distortions[i];
            camera.width = dataset.widths[i]; camera.height = dataset.heights[i];
            camera.fx = dataset.intrins[4 * i]; camera.fy = dataset.intrins[4 * i + 1];
            camera.cx = dataset.intrins[4 * i + 2]; camera.cy = dataset.intrins[4 * i + 3];
            std::copy_n(dataset.dist_coeffs.data() + 8 * i, 8, camera.dist);
            if (!dataset.redistort.empty()) {
                camera.source_model = dataset.redistort[i].source_model;
                std::copy_n(dataset.redistort[i].params,16,camera.source_params);
            }
            sfm::Quat quaternion{image.qvec[0], image.qvec[1], image.qvec[2], image.qvec[3]};
            const auto rotation = sfm::quaternionToRotation(quaternion);
            const float* pose = dataset.c2w.data() + 12 * i;
            for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c)
                require(std::fabs(pose[c * 4 + r] * (r == 0 ? 1 : -1) - rotation[r * 3 + c]) < 1e-6,
                        "parsed camera pose disagrees with source reconstruction");
            verify(raw, camera, rotation);
        }
        std::printf("PASS %zu calibrated camera projections and parsed pose conventions\n", checked.size());
        return 0;
    } catch (const std::exception& e) { std::fprintf(stderr, "FAIL: %s\n", e.what()); return 1; }
}

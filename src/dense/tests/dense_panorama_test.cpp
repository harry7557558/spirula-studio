#include "dense/Reconstruction.h"
#include "dense/ExternalSort.h"
#include "core/CameraModel.h"

#include <chrono>
#include <cstdio>

namespace fs = std::filesystem;
using namespace spirula::dense;

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct Fixture {
    fs::path root = fs::temp_directory_path() /
        ("spirula-panorama-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Fixture() { fs::create_directories(root); }
    ~Fixture() { std::error_code error; fs::remove_all(root,error); }
};
spirula::roma::Prediction prediction(const View& from, const View& to) {
    spirula::roma::Prediction p; p.width = 64; p.height = 32;
    p.warp.resize(64 * 32 * 2); p.overlap.resize(64 * 32,1); p.precision.resize(64 * 32 * 3);
    for (int y = 0; y < 32; ++y) for (int x = 0; x < 64; ++x) {
        sfm::Vec3 ray; require(bearing(from,{x + 0.5,y + 0.5},ray),"panorama fixture has an invalid ray");
        const double dot = from.center.dot(ray);
        const double t = -dot + std::sqrt(dot * dot + 16 - from.center.dot(from.center));
        sfm::Vec2 pixel; require(project(to,from.center + ray * t,pixel),"panorama sphere is not visible");
        const size_t i = (size_t)y * 64 + x;
        p.warp[i * 2] = (float)(2 * pixel.x / 64 - 1); p.warp[i * 2 + 1] = (float)(2 * pixel.y / 32 - 1);
        p.precision[i * 3] = p.precision[i * 3 + 2] = 1;
    }
    return p;
}
}

int main() {
    try {
        Fixture fixture;
        std::vector<View> views(3);
        for (int i = 0; i < 3; ++i) {
            auto& v = views[i]; v.source_image = i; v.source_camera = true; v.center = {i * 0.3,0,0};
            v.camera.model = (int)CameraModelType::EQUIRECTANGULAR; v.camera.width = 64; v.camera.height = 32;
            v.camera.fx = v.camera.fy = 32 / 3.141592653589793; v.camera.cx = 32; v.camera.cy = 16; v.validate();
        }
        DenseConfig config; config.preset = "custom"; config.stride = 1; config.matching_space = "source";
        config.match.low_width = 64; config.match.low_height = 32; config.match.high_width = config.match.high_height = 0;
        config.match.bidirectional = true; config.samples_per_reference = 0; config.source_reprojection_error = 0.08;
        config.voxel_size = 0.01; config.cpu_workers = 1; config.image_cache_bytes = 32 * 1024;
        ViewPixels pixels; pixels.width = 64; pixels.height = 32;
        pixels.rgb.resize(64 * 32 * 3,0.5f); pixels.keep.resize(64 * 32,1);
        const auto ab = prediction(views[0],views[1]), ac = prediction(views[0],views[2]);
        const auto ba = prediction(views[1],views[0]), ca = prediction(views[2],views[0]);
        auto run = [&](const char* name,bool masked,bool shifted) {
            const auto progress = fixture.root / (std::string(name) + "-progress");
            Reconstruction reconstruction((fixture.root / name).string(),views,config,nullptr,progress.string());
            auto target_pixels = pixels; auto reverse = ba;
            if (masked) for (int y = 0; y < 32; ++y) target_pixels.keep[y * 64 + 63] = 0;
            if (shifted) for (size_t i = 0; i < reverse.warp.size(); i += 2) reverse.warp[i] += 2;
            reconstruction.add_pair(0,1,pixels,target_pixels,{ab,reverse});
            reconstruction.add_pair(0,2,pixels,pixels,{ac,ca});
            reconstruction.complete_reference(0);
            const auto preview = reconstruction.checkpoint();
            require(preview.filtered && !preview.provisional && preview.points > 1400,"filtered panorama coverage is incomplete");
            std::ifstream input(progress / preview.file,std::ios::binary); uint64_t seam_points = 0;
            for (uint64_t i = 0; i < preview.points; ++i) {
                float xyz[3]; uint8_t rgb[3]; input.read(reinterpret_cast<char*>(xyz),sizeof xyz); input.read(reinterpret_cast<char*>(rgb),sizeof rgb);
                require(input.good() && std::fabs(std::hypot(std::hypot(xyz[0],xyz[1]),xyz[2]) - 4) < 1e-4,
                    "filtered preview published a point off the calibrated sphere");
                if (xyz[2] < -2 && std::fabs(xyz[0]) < 0.3) ++seam_points;
            }
            require(seam_points > 15,"panorama seam was discarded by color or cycle sampling");
            return preview.points;
        };
        const auto baseline = run("baseline",false,false);
        require(run("wrapped-cycle",false,true) == baseline,"cycle checking rejected an equivalent wrapped warp");
        require(run("masked-seam",true,false) < baseline,"mask sampling ignored the opposite side of the panorama seam");
        std::printf("PASS calibrated panorama reconstruction, filtered sphere, periodic color/masks and wrapped cycle interpolation (%llu points)\n",
                    (unsigned long long)baseline);
        return 0;
    } catch (const std::exception& error) { std::fprintf(stderr,"FAIL: %s\n",error.what()); return 1; }
}

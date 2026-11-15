#include "app/gui/SfmProgress.h"
#include "app/gui/PreviewRenderer.h"
#include "app/gui/GlLoader.h"
#include "core/AtomicFile.h"
#include "core/CameraModel.h"
#include "dense/Artifact.h"
#include "mesh/MeshExport.h"
#include "external/stb_image_write.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace fs = std::filesystem;

namespace {
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class T> void put(std::ostream& out, const T& value) { out.write(reinterpret_cast<const char*>(&value), sizeof value); }
void point(std::ostream& out, uint64_t i) {
    const float xyz[3] = {(float)(i % 1000) / 1000, (float)((i / 1000) % 1000) / 1000, 3};
    const uint8_t rgb[3] = {(uint8_t)(i % 256), 128, 255};
    out.write(reinterpret_cast<const char*>(xyz), sizeof xyz);
    out.write(reinterpret_cast<const char*>(rgb), sizeof rgb);
}
void coordinates(const gui::LiveModel& model, uint64_t i) {
    const float expected[3] = {(float)(i % 1000) / 1000, (float)((i / 1000) % 1000) / 1000, 3};
    for (int k = 0; k < 3; ++k)
        require(model.ds.points.xyz[i * 3 + k] == (double)expected[k], "checkpoint coordinate conversion corrupted the cloud");
}
void orthographic_preview(gui::PreviewRenderer& renderer) {
    const float view[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    const float target[3] = {0,0,-2};
    for (bool mesh : {false, true}) {
        if (mesh) {
            meshing::MeshData data;
            for (int i = 0; i < 3; i++) {
                const float x = i == 0 ? -0.5f : i == 1 ? 0.5f : 0.0f;
                const float z = i == 0 ? -1.0f : i == 1 ? -10.0f : 1.0f;
                data.V.insert(data.V.end(), {{x-0.2f,-0.2f,z}, {x+0.2f,-0.2f,z},
                                             {x+0.2f,0.2f,z}, {x-0.2f,0.2f,z}});
                data.F.push_back({4*i,4*i+1,4*i+2});
                data.F.push_back({4*i,4*i+2,4*i+3});
            }
            require(renderer.build(data, nullptr), "orthographic mesh build failed");
            renderer.set_mesh_display(false, false, false);
        } else {
            gui::LiveModel model;
            model.ds.points.xyz = {-0.5,0,-1, 0.5,0,-10, 0,0,1};
            model.ds.points.rgb.assign(9, 255);
            require(renderer.build(model.ds, model.post), "orthographic cloud build failed");
        }
        gui::PreviewStyle style;
        style.point_shape = 3;
        style.point_radius = 0.1f;
        const auto tex = renderer.render(128,128,view,gui::PreviewProjection::Orthographic,
                                         1,1,1,2,target,false,1,false,&style);
        require(tex != 0, "orthographic preview did not render");
        std::vector<uint8_t> pixels(128*128*4);
        glBindTexture(GL_TEXTURE_2D, tex);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        int counts[2] = {};
        for (int y = 0; y < 128; y++) for (int x = 0; x < 128; x++) {
            if (pixels[(y*128+x)*4] < 100) continue;
            require(x < 50 || x > 78, "orthographic preview drew behind-camera geometry");
            counts[x >= 64]++;
        }
        if (counts[0] <= 10 || counts[0] != counts[1])
            std::fprintf(stderr, "orthographic %s pixels: %d, %d\n",
                         mesh ? "mesh" : "points", counts[0], counts[1]);
        require(counts[0] > 10 && counts[0] == counts[1],
                "orthographic preview footprint changed with depth");
        renderer.destroy_gl();
    }
}

void marker(const fs::path& dir, uint64_t count, const std::string& file, uint32_t version = 6) {
    std::ofstream out(dir / "model.bin.part", std::ios::binary);
    out.write("VKPM", 4); put(out, version);
    put(out, uint32_t{version >= 7 ? 32u : version >= 6 && file == "growing.points" ? 16u : 0u});
    put(out, uint32_t{0}); put(out, uint32_t{0}); put(out, count);
    if (version >= 5) {
        put(out, (uint32_t)file.size()); out.write(file.data(), (std::streamsize)file.size()); put(out, uint32_t{0});
    } else {
        put(out, (uint32_t)count);
        for (uint64_t i = 0; i < count; ++i) point(out, i);
    }
    out.close();
    spirula::replace_file(dir / "model.bin.part", dir / "model.bin");
}
void lens_marker(const fs::path& dir) {
    std::ofstream out(dir / "model.bin.part", std::ios::binary);
    out.write("VKPM", 4); put(out, uint32_t{6}); put(out, uint32_t{0});
    put(out, uint32_t{4}); put(out, uint32_t{4}); put(out, uint64_t{2});
    for (uint32_t model = 0; model < 4; ++model) {
        put(out, model);
        const float pose[12] = {1,0,0,(float)model - 1.5f, 0,1,0,0, 0,0,1,0};
        out.write(reinterpret_cast<const char*>(pose), sizeof pose);
        put(out, uint32_t{640}); put(out, uint32_t{480}); put(out, model);
        put(out, uint32_t{model < 2 ? model + 1 : 0});
        for (int k = 0; k < 4; ++k) put(out, float{123.5f + k});
        for (int k = 0; k < kCameraDistortionParams; ++k)
            put(out, model < 2 ? (k + 1) * 0.00001f : 0.0f);
    }
    const std::string file = "final.points";
    put(out, (uint32_t)file.size()); out.write(file.data(), (std::streamsize)file.size()); put(out, uint32_t{0});
    out.close();
    spirula::replace_file(dir / "model.bin.part", dir / "model.bin");
}
}

int main(int argc, char** argv) {
    GLFWwindow* window = nullptr;
    try {
        const auto dir = fs::temp_directory_path() /
            ("spirula-live-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(dir);
        std::ofstream points(dir / "growing.points", std::ios::binary);
        constexpr uint64_t first = 50017, full = 8000001;
        for (uint64_t i = 0; i < first; ++i) point(points, i);
        points.flush(); marker(dir, first, "growing.points");
        gui::LiveModel model; int64_t stamp = 0;
        require(gui::read_live_model(dir.string(), stamp, model) && model.ds.points.num() == first,
                "checkpoint was capped at 50k");
        require(model.provisional, "provisional checkpoint phase was lost");
        marker(dir,first,"growing.points",7); stamp = 0;
        require(gui::read_live_model(dir.string(),stamp,model) && model.filtered && !model.provisional,
                "filtered checkpoint phase was lost");
        for (uint64_t i : {uint64_t{0}, uint64_t{1}, uint64_t{1023}, first - 1}) coordinates(model, i);
        require(!gui::read_live_model(dir.string(), stamp, model), "unchanged checkpoint reloaded");
        int64_t retry_stamp = 0;
        marker(dir, first + 1, "growing.points");
        require(!gui::read_live_model(dir.string(), retry_stamp, model) && retry_stamp == 0,
                "unfinished checkpoint consumed publication timestamp");
        point(points, first); points.flush();
        require(gui::read_live_model(dir.string(), retry_stamp, model) && model.ds.points.num() == first + 1,
                "completed prefix was not retried");
        for (uint64_t i = first + 1; i < full; ++i) point(points, i);
        points.flush(); marker(dir, full, "growing.points"); stamp = 0;
        bool memory_rejected = false;
        try { gui::read_live_model(dir.string(), stamp, model, 256); }
        catch (const std::exception&) { memory_rejected = true; }
        require(memory_rejected && stamp == 0, "hardware memory budget ignored");
        require(gui::read_live_model(dir.string(), stamp, model) && model.ds.points.num() == full &&
                model.ds.points.rgb[(full - 1) * 3] == (uint8_t)((full - 1) % 256), "full checkpoint omitted points");
        require(glfwInit() != 0, "GLFW initialization failed");
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        window = glfwCreateWindow(256, 256, "live preview test", nullptr, nullptr);
        require(window != nullptr, "OpenGL test context unavailable");
        glfwMakeContextCurrent(window);
        gui::PreviewRenderer renderer;
        orthographic_preview(renderer);
        require(renderer.build(model.ds, model.post) && renderer.num_points() == full,
                "renderer capped the full checkpoint");
        const float view[16] = {1,0,0,0, 0,1,0,0, 0,0,1,-5, 0,0,0,1};
        const float target[3] = {0,0,0};
        const auto texture = renderer.render(256, 256, view, gui::PreviewProjection::Pinhole, 1, 1, 1, 5, target, false, 1, false);
        require(texture != 0,
                "full checkpoint did not render");
        std::vector<uint8_t> pixels(256 * 256 * 4);
        glBindTexture(GL_TEXTURE_2D, texture);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        require(glGetError() == GL_NO_ERROR, "full checkpoint rendering produced an OpenGL error");
        int min_x = 256, min_y = 256, max_x = -1, max_y = -1;
        size_t colored = 0;
        for (int y = 0; y < 256; ++y) for (int x = 0; x < 256; ++x)
            if (pixels[((size_t)y * 256 + x) * 4 + 2] > 192) {
                ++colored;
                min_x = std::min(min_x, x); max_x = std::max(max_x, x);
                min_y = std::min(min_y, y); max_y = std::max(max_y, y);
            }
        require(colored > 1000 && max_x - min_x > 30 && max_y - min_y > 30,
                "full checkpoint collapsed instead of rendering a two-dimensional cloud");
        { std::ofstream final(dir / "final.points", std::ios::binary); point(final, 0); point(final, 1); }
        marker(dir, 2, "final.points"); stamp = 0;
        require(gui::read_live_model(dir.string(), stamp, model) && model.ds.points.num() == 2 &&
                !model.provisional && renderer.build(model.ds, model.post) && renderer.num_points() == 2,
                "final checkpoint did not replace the growing cloud");
        renderer.destroy_gl();
        marker(dir, 2, "", 4); stamp = 0;
        require(gui::read_live_model(dir.string(), stamp, model) && model.ds.points.num() == 2,
                "legacy snapshot compatibility failed");
        marker(dir, 2, "final.points", 5); stamp = 0;
        require(gui::read_live_model(dir.string(), stamp, model) && model.ds.points.num() == 2 && !model.provisional,
                "version 5 snapshot compatibility failed");
        lens_marker(dir); stamp = 0;
        require(gui::read_live_model(dir.string(), stamp, model) && model.n_registered == 4,
                "native lens snapshot unavailable");
        for (int i = 0; i < 4; ++i) {
            require(model.ds.camera_models[i] == i && model.ds.camera_distortions[i] == (i < 2 ? i + 1 : 0),
                    "native camera model or distortion tier was lost");
            for (int k = 0; k < 4; ++k)
                require(model.ds.intrins[i * 4 + k] == 123.5f + k, "native camera intrinsics changed");
            for (int k = 0; k < kCameraDistortionParams; ++k)
                require(model.ds.dist_coeffs[i * kCameraDistortionParams + k] == (i < 2 ? (k + 1) * 0.00001f : 0.0f),
                        "native camera distortion changed");
        }
        require(renderer.build(model.ds, model.post), "native lens wireframes did not build");
        renderer.destroy_gl();
        marker(dir, 1, "../outside.points"); stamp = 0;
        require(!gui::read_live_model(dir.string(), stamp, model) && stamp == 0, "unsafe checkpoint path accepted");
        if (argc == 4 || argc == 5) {
            stamp = 0;
            require(gui::read_live_model(argv[1], stamp, model) && model.n_points > 0, "dataset checkpoint unavailable");
            std::ifstream records(argv[2], std::ios::binary);
            for (uint64_t i : {uint64_t{0}, model.n_points / 2, model.n_points - 1}) {
                float expected[3];
                records.seekg((std::streamoff)(i * 15));
                records.read(reinterpret_cast<char*>(expected), sizeof expected);
                require((bool)records, "dataset checkpoint record unavailable");
                for (int k = 0; k < 3; ++k)
                    require(model.ds.points.xyz[i * 3 + k] == (double)expected[k], "dataset coordinates differ from checkpoint");
            }
            require(renderer.build(model.ds, model.post) && renderer.num_points() == (int64_t)model.n_points,
                    "dataset renderer omitted points");
            gui::PreviewStyle style; style.transparent = true;
            const float actual_view[16] = {1,0,0,0, 0,1,0,0, 0,0,1,-2, 0,0,0,1};
            const auto actual = renderer.render(768, 768, actual_view, gui::PreviewProjection::Pinhole, 1, 1, 1, 2,
                                                target, false, 1, false, &style);
            require(actual != 0, "dataset checkpoint did not render");
            pixels.resize(768 * 768 * 4);
            glBindTexture(GL_TEXTURE_2D, actual);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            require(glGetError() == GL_NO_ERROR, "dataset rendering produced an OpenGL error");
            min_x = min_y = 768; max_x = max_y = -1; colored = 0;
            for (int y = 0; y < 768; ++y) for (int x = 0; x < 768; ++x)
                if (pixels[((size_t)y * 768 + x) * 4 + 3] > 0) {
                    ++colored;
                    min_x = std::min(min_x, x); max_x = std::max(max_x, x);
                    min_y = std::min(min_y, y); max_y = std::max(max_y, y);
                }
            stbi_flip_vertically_on_write(1);
            require(stbi_write_png(argv[3], 768, 768, 4, pixels.data(), 768 * 4) != 0, "cannot save dataset render");
            require(colored > 1000 && max_x - min_x > 30 && max_y - min_y > 30,
                    "dataset reconstruction collapsed to a line");
            std::printf("PASS dataset checkpoint: %u cameras, %llu points, %zu pixels, extent %dx%d\n",
                        model.n_registered, (unsigned long long)model.n_points, colored, max_x - min_x, max_y - min_y);
            if (argc == 5) {
                const auto artifact = spirula::dense::artifact_files(argv[4]);
                require(spirula::dense::artifact_checksum_valid(artifact), "dataset cloud checksum mismatch");
                DatasetParserConfig parser;
                parser.require_image_files = false; parser.center_mode = "camera-mean";
                parser.seed_pointcloud = artifact.cloud.u8string();
                const auto seeded = parse_dataset(argv[4],parser,"");
                require(seeded.points.num() == model.ds.points.num(), "preview and PLY point counts differ");
                require(seeded.points.rgb == model.ds.points.rgb, "preview and PLY colors differ");
                double maximum = 0;
                for (size_t i = 0; i < seeded.points.xyz.size(); ++i) {
                    const double error = std::fabs(seeded.points.xyz[i]-model.ds.points.xyz[i]);
                    maximum = std::max(maximum,error);
                    require(error <= 4*std::numeric_limits<float>::epsilon()*std::max(1.0,std::fabs(seeded.points.xyz[i])),
                            "preview and parsed PLY coordinate frames differ");
                }
                auto exported = model.ds;
                exported.points = seeded.points;
                require(renderer.build(exported,model.post), "parsed PLY renderer unavailable");
                const auto ply_texture = renderer.render(768,768,actual_view,gui::PreviewProjection::Pinhole,1,1,1,2,
                                                        target,false,1,false,&style);
                require(ply_texture != 0, "parsed PLY did not render");
                std::vector<uint8_t> ply_pixels(pixels.size());
                glBindTexture(GL_TEXTURE_2D,ply_texture);
                glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,ply_pixels.data());
                require(glGetError() == GL_NO_ERROR, "parsed PLY rendering produced an OpenGL error");
                size_t different = 0;
                for (size_t i = 0; i < pixels.size(); i+=4)
                    if (!std::equal(pixels.begin()+i,pixels.begin()+i+4,ply_pixels.begin()+i)) ++different;
                const std::string ply_image = std::string(argv[3])+".ply.png";
                require(stbi_write_png(ply_image.c_str(),768,768,4,ply_pixels.data(),768*4) != 0, "cannot save PLY render");
                require(different <= colored/100, "preview and PLY differ at the identical viewpoint");
                std::printf("PASS matched PLY view: maximum coordinate difference %.9g, %zu differing pixels\n",maximum,different);
            }
            renderer.destroy_gl();
        }
        glfwDestroyWindow(window); window = nullptr; glfwTerminate();
        std::printf("PASS live checkpoints: growth past 50k, immutable prefix, retry, hardware memory budget, legacy format, full %llu-point GL render\n",
                    (unsigned long long)full);
        return 0;
    } catch (const std::exception& e) {
        if (window) glfwDestroyWindow(window);
        glfwTerminate(); std::fprintf(stderr, "FAIL: %s\n", e.what()); return 1;
    }
}

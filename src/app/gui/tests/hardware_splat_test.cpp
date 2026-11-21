#include "app/gui/HardwareSplatRenderer.h"
#include "app/gui/HardwareSplatSort.h"
#include "app/gui/GlLoader.h"
#include "app/gui/SceneFrame.h"
#include "external/stb_image_write.h"
#include "checkpoint/SplatPly.h"
#include "core/BufferedFile.h"
#include "core/HostMemory.h"
#include "nn/core/Half.h"
#include "data/Json.h"
#include <chrono>
#include "engine/Engine.h"
#include "backend/api/BackendRuntime.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <numeric>
#include <random>
#include <stdexcept>

#ifdef _WIN32
#define SS_SETENV(k, v) _putenv_s(k, v)
#else
#define SS_SETENV(k, v) setenv(k, v, 1)
#endif

static TorchTensorView tv(const std::vector<float>& v, std::vector<int64_t> shape) {
    return {(uint64_t)(uintptr_t)v.data(), 4u, std::move(shape)};
}

using Buffer = gui::HardwareSplatRenderer::Buffer;
// Every buffer but colour; render_normal only for the eval3d primitives.
static const std::pair<Buffer, const char*> kBuffers[] = {
    {Buffer::Depth, "depth"}, {Buffer::Alpha, "alpha"}, {Buffer::DepthNormal, "depth_normal"},
    {Buffer::RenderNormal, "render_normal"}, {Buffer::Sh, "sh"}};

// The compute view's display of `buffer`, by RenderWorker's transforms, in its
// rows; the last forward_3dgs rendered the normal where there is one.
static std::vector<float> compute_display(Buffer buffer, int W, int H, const char* camera,
                                          const std::vector<float>& intr, int sh_degree) {
    const size_t n = size_t(W)*H;
    std::vector<float> rgb(n*3), depth(n), Ts(n), out(n*3);
    TorchTensorView nil{0,4,{0}};
    engine_copy_render_to_host(tv(rgb,{1,H,W,3}),tv(depth,{1,H,W,1}),tv(Ts,{1,H,W,1}),nil,nil);
    if (buffer == Buffer::Depth) {
        static const float k[3][8] = {
            {0.14637796f, 2.94711014f, -10.15061040f, -90.46877071f, 550.44382083f, -1061.31232675f, 874.85901369f, -266.03287948f},
            {0.08594198f, 2.06520532f, 9.64581379f, -55.09180383f, 149.40813531f, -245.26823636f, 202.98596195f, -63.82163439f},
            {0.23431523f, 6.33792818f, 9.26380342f, -203.76964931f, 604.07329733f, -766.82678386f, 447.99287080f, -97.24397473f}};
        float lo = INFINITY, hi = -INFINITY;
        for (float d : depth) if (std::isfinite(d)) { lo = std::min(lo,d); hi = std::max(hi,d); }
        for (size_t i = 0; i < n; ++i) for (int c = 0; c < 3; ++c) {
            const float t = hi > lo ? (depth[i]-lo)/(hi-lo) : 0;
            float v = 0;
            for (int j = 7; j >= 0; --j) v = v*t + k[c][j];
            out[i*3+c] = v;
        }
    } else if (buffer == Buffer::Alpha) {
        for (size_t i = 0; i < n*3; ++i) out[i] = 1-Ts[i/3];
    } else if (buffer == Buffer::RenderNormal) {
        engine_copy_render_normal_to_host(tv(out,{1,H,W,3}));
        for (auto& v : out) v = .5f+.5f*v;
    } else if (buffer == Buffer::DepthNormal) {
        std::vector<float> dist(kCameraDistortionParams,0);
        float* d_depth = (float*)backend::device_malloc(n*4);
        float* d_normal = (float*)backend::device_malloc(n*12);
        float* d_intr = (float*)backend::device_malloc(16);
        float* d_dist = (float*)backend::device_malloc(dist.size()*4);
        backend::memcpy_sync(d_depth,depth.data(),n*4,backend::MemcpyKind::HostToDevice);
        backend::memcpy_sync(d_intr,intr.data(),16,backend::MemcpyKind::HostToDevice);
        backend::memcpy_sync(d_dist,dist.data(),dist.size()*4,backend::MemcpyKind::HostToDevice);
        auto dv = [](float* p, std::vector<int64_t> shape) { return TorchTensorView{(uint64_t)(uintptr_t)p,4u,std::move(shape)}; };
        depth_to_normal_forward_tv(camera,"NONE",dv(d_intr,{1,4}),dv(d_dist,{1,kCameraDistortionParams}),true,
                                   dv(d_depth,{1,H,W,1}),dv(d_normal,{1,H,W,3}));
        backend::memcpy_sync(out.data(),d_normal,n*12,backend::MemcpyKind::DeviceToHost);
        for (float* p : {d_depth,d_normal,d_intr,d_dist}) backend::device_free(p);
        for (auto& v : out) v = .5f+.5f*v;
    } else if (buffer == Buffer::Sh) {
        std::vector<float> dc(size_t(engine_get_max_num_splats())*3,0);
        engine_debug_forward(tv(dc,{engine_get_max_num_splats(),3}),sh_degree,tv(out,{1,H,W,3}));
    }
    for (auto& v : out) v = std::clamp(v,0.f,1.f);
    return out;
}

// A GL display texture in compute's rows, and its mean / max distance from
// `ref`; over `limit`, both go to <temp>/gl-parity-<name>-{compute,gl}.png.
static std::pair<double,double> gl_difference(unsigned texture, int W, int H, const std::vector<float>& ref,
                                              double limit, const std::string& name) {
    std::vector<float> pixels(size_t(W)*H*4);
    glBindTexture(GL_TEXTURE_2D,texture); glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_FLOAT,pixels.data());
    double sum = 0, worst = 0;
    std::vector<uint8_t> a(size_t(W)*H*3), b(a.size());
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) for (int j = 0; j < 3; ++j) {
        // The blit's quantization (Visualizer.cu), which GL's RGBA8 shares.
        const float gl = pixels[((H-1-y)*W+x)*4+j], r = std::floor(ref[(y*W+x)*3+j]*255+.5f)/255;
        const double d = std::abs(r-gl);
        sum += d; worst = std::max(worst,d);
        a[(y*W+x)*3+j] = uint8_t(std::round(ref[(y*W+x)*3+j]*255)); b[(y*W+x)*3+j] = uint8_t(std::round(gl*255));
    }
    const double mean = sum/(double(W)*H*3);
    if (mean > limit) {
        const auto prefix = (std::filesystem::temp_directory_path()/("gl-parity-"+name)).string();
        stbi_write_png((prefix+"-compute.png").c_str(),W,H,3,a.data(),W*3);
        stbi_write_png((prefix+"-gl.png").c_str(),W,H,3,b.data(),W*3);
    }
    return {mean, worst};
}

static void check_depth_sort() {
    const float view[16] = {.8f,0,.6f,.2f, 0,1,0,-.1f, -.6f,0,.8f,-.3f, 0,0,0,1};
    for (uint32_t count : {0u,1u,255u,32767u,32768u,32769u,65539u}) for (bool tied : {false,true}) {
        std::vector<float> positions(size_t(count)*3);
        std::mt19937 rng(731);
        for (auto& p : positions) p = tied ? 1.f : (int(rng()%127)-63)*.125f;
        if (count > 10 && !tied) {
            positions[0] = std::numeric_limits<float>::quiet_NaN();
            positions[3] = std::numeric_limits<float>::infinity();
            positions[6] = std::numeric_limits<float>::max();
        }
        std::vector<uint32_t> keys(count),order(count),scratch(count),reference(count);
        for (int camera = 0; camera < 5; ++camera) {
            std::iota(reference.begin(),reference.end(),0);
            std::stable_sort(reference.begin(),reference.end(),[&](uint32_t a,uint32_t b) {
                return gui::splat_depth_key(positions.data()+size_t(a)*3,view,camera) <
                       gui::splat_depth_key(positions.data()+size_t(b)*3,view,camera);
            });
            for (int workers : {1,2,3,4,5,8}) {
                gui::sort_splat_depths(positions.data(),count,view,camera,keys.data(),order.data(),scratch.data(),workers);
                if (order != reference) throw std::runtime_error("Parallel splat sort differs from stable serial order");
            }
        }
    }
    std::printf("depth sort: serial/stable oracle passed for 1-8 threads, ties, uneven chunks and all cameras\n");
}

static void benchmark_depth_sort(uint32_t count, int repeats) {
    const auto available = spirula::availableCommitBytes();
    if (!available || uint64_t(count)*24 > available-available/4)
        throw std::runtime_error("Insufficient host memory for sort benchmark");
    std::vector<float> positions(size_t(count)*3);
    std::vector<uint32_t> keys(count),order(count),scratch(count);
    uint32_t seed = 731;
    auto uniform = [&] {
        seed ^= seed<<13; seed ^= seed>>17; seed ^= seed<<5;
        return float(seed & 0xffffff)/16777216;
    };
    for (uint32_t i = 0; i < count; ++i) {
        positions[size_t(i)*3] = (uniform()-.5f)*80;
        positions[size_t(i)*3+1] = (uniform()-.5f)*60;
        positions[size_t(i)*3+2] = -1-uniform()*100;
    }
    const float view[16] = {.9998000067f,0,.0199986667f,0, 0,1,0,0,
                           -.0199986667f,0,.9998000067f,0, 0,0,0,1};
    uint64_t expected = 0;
    for (int repeat = 0; repeat < repeats; ++repeat) for (int run = 0; run < 2; ++run) {
        const int workers = (run+repeat)%2 ? 4 : 1;
        const auto start = std::chrono::steady_clock::now();
        const int used = gui::sort_splat_depths(positions.data(),count,view,0,keys.data(),order.data(),scratch.data(),workers);
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        uint64_t hash = 1469598103934665603ull;
        for (uint32_t i = 0; i < count; ++i) {
            const auto index = order[i];
            if (index >= count || (i && (keys[order[i-1]] > keys[index] ||
                (keys[order[i-1]] == keys[index] && order[i-1] >= index))))
                throw std::runtime_error("Benchmark splat sort is not stable");
            hash = (hash ^ index)*1099511628211ull;
        }
        if (!repeat && !run) expected = hash;
        else if (hash != expected) throw std::runtime_error("Serial/parallel benchmark order hash mismatch");
        std::printf("sort count %u requested_threads %d actual_threads %d repeat %d seconds %.6f order_hash %llu\n",
            count,workers,used,repeat,seconds,static_cast<unsigned long long>(hash));
        std::fflush(stdout);
    }
}

static void coherent_frames() {
    constexpr int64_t count = 1000001;
    std::random_device random;
    std::filesystem::path directory;
    do {
        directory = std::filesystem::temp_directory_path()/
            ("spirula-hardware-coherent-"+std::to_string(random())+"-"+std::to_string(random()));
    } while (!std::filesystem::create_directory(directory));
    const auto path = directory/"fixture.ply";
    {
        spirula::BufferedOutputFile file(path, std::ios::binary);
        file << spirula::splat_ply_header(count, {0,0});
        const int stride = spirula::splat_ply_row_floats({0,0});
        std::vector<float> rows(size_t(65536)*stride);
        for (int64_t first = 0; first < count; first += 65536) {
            const auto n = std::min<int64_t>(65536,count-first);
            std::fill(rows.begin(),rows.end(),0);
            for (int64_t i = 0; i < n; ++i) {
                const auto index = first+i;
                float* r = rows.data()+i*stride;
                r[2] = 1000; r[9] = -20; r[10] = r[11] = r[12] = -18; r[13] = 1;
                if (index < 64) {
                    r[0] = (float(index%8)-3.5f)*.2f;
                    r[1] = (float(index/8)-3.5f)*.2f;
                    r[2] = -2-float(index%3)*.2f;
                    r[6+index%3] = 1;
                    r[9] = 2; r[10] = r[11] = r[12] = -3;
                }
            }
            file.write(reinterpret_cast<const char*>(rows.data()),n*stride*sizeof(float));
        }
        file.flush();
        if (!file) throw std::runtime_error("Coherent-frame fixture write failed");
    }
    gui::HardwareSplatRenderer renderer;
    const float center[3]{}, gamut[9] = {1,0,0,0,1,0,0,0,1};
    float view[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    renderer.load(path.string(),center,1);
    auto render = [&](int w, int h, int camera) {
        return renderer.render(w,h,view,camera,60,60,0,0,gamut,0,false);
    };
    auto read = [](unsigned texture, int w, int h) {
        glBindTexture(GL_TEXTURE_2D,texture);
        int actual_w = 0, actual_h = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&actual_w);
        glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&actual_h);
        if (actual_w != w || actual_h != h)
            throw std::runtime_error("Coherent-frame texture has the wrong resolution");
        std::vector<uint8_t> pixels(size_t(w)*h*4);
        glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        if (glGetError()) throw std::runtime_error("Coherent-frame readback failed");
        return pixels;
    };
    auto previous = read(render(96,80,0),96,80);
    view[3] = .25f; view[7] = .1f;
    auto panned = read(render(96,80,0),96,80);
    if (panned == previous)
        throw std::runtime_error("Planar pan with unchanged depth keys did not update immediately");
    previous = std::move(panned);
    view[0] = view[10] = std::cos(.25f); view[2] = std::sin(.25f); view[8] = -view[2];
    if (read(render(96,80,0),96,80) != previous)
        throw std::runtime_error("A new camera was rendered with the previous sort order");
    const auto deadline = std::chrono::steady_clock::now()+std::chrono::seconds(10);
    auto updated = previous;
    while (updated == previous && std::chrono::steady_clock::now() < deadline) {
        updated = read(render(96,80,0),96,80);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (updated == previous) throw std::runtime_error("Pending camera sort was not polled to completion");
    previous = std::move(updated);
    view[11] = -.1f;
    if (read(render(96,80,0),96,80) != previous)
        throw std::runtime_error("Planar depth translation reused different float depth keys");
    renderer.finish_sort();
    previous = read(render(96,80,0),96,80);
    view[3] = .3f;
    if (read(render(96,80,1),96,80) != previous)
        throw std::runtime_error("Camera-model change used an unmatched sort order");
    renderer.finish_sort();
    view[7] = -.2f;
    if (read(render(128,104,0),96,80) != previous)
        throw std::runtime_error("Superseded camera sort replaced the coherent image");
    view[3] = -.4f; view[11] = -.3f;
    unsigned captured = render(128,104,0);
    // The capture path drains an older request before the requested camera's work.
    for (int drain = 0; drain < 2; ++drain) {
        renderer.finish_sort();
        captured = render(128,104,0);
    }
    const auto capture = read(captured,128,104);
    renderer.destroy();
    renderer.load(path.string(),center,1);
    if (capture != read(render(128,104,0),128,104))
        throw std::runtime_error("Capture did not drain to the requested camera and resolution");
    renderer.destroy();
    std::filesystem::remove(path);
    std::filesystem::remove(directory);
    std::printf("coherent frames: immediate planar pan, held pending views, polled completion, drained requested capture\n");
}

// SS_VIEWER_STOCHASTIC=1 (set before anything reads it): the GL view averages
// unsorted frames, and once converged must show what sorted compute does.
static bool stochastic_frames() {
    SS_SETENV("SS_VIEWER_STOCHASTIC", "1");
    SS_SETENV("SS_VIEWER_STOCHASTIC_SPP", "8");
    SS_SETENV("SS_VIEWER_STOCHASTIC_FRAMES", "128");
    if (!glfwInit()) throw std::runtime_error("GLFW initialization failed");
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    auto* window = glfwCreateWindow(64, 64, "stochastic", nullptr, nullptr);
    if (!window) throw std::runtime_error("GL context creation failed");
    glfwMakeContextCurrent(window);
    constexpr int W = 160, H = 120;
    spirula::SplatCloud c; c.num = 600; c.sh_degree = 1;
    c.means.resize(c.num*3); c.quats.resize(c.num*4); c.scales.resize(c.num*3);
    c.opacities.resize(c.num); c.features_dc.resize(c.num*3); c.features_sh.resize(c.num*9);
    std::mt19937 rng(20261121);
    auto rand = [&] { return float(rng() & 65535)/65535; };
    for (int i = 0; i < c.num; ++i) {
        c.means[3*i] = (rand()-.5f)*3; c.means[3*i+1] = (rand()-.5f)*2; c.means[3*i+2] = -2-rand()*3;
        for (int j = 0; j < 4; ++j) c.quats[4*i+j] = j ? 0.f : 1.f;
        for (int j = 0; j < 3; ++j) { c.scales[3*i+j] = -3-rand()*1.5f; c.features_dc[3*i+j] = (rand()-.5f)*2; }
        c.opacities[i] = rand()*6-2;
    }
    for (auto& v : c.features_sh) v = (rand()-.5f)*.1f;
    // Exactly representable in the GL textures, so only the sampling differs.
    auto half = [](float x) { return nn::half_to_float(nn::float_to_half(x)); };
    for (auto& x : c.scales) x = half(x);
    for (auto& x : c.features_dc) x = half(x);
    for (auto& x : c.features_sh) x = 0;
    const auto path = std::filesystem::temp_directory_path()/"spirula-hardware-stochastic.ply";
    spirula::write_splat_ply(c, path.string());
    gui::HardwareSplatRenderer renderer;
    float center[3]{};
    renderer.load(path.string(), center, 1);
    engine_reset();
    set_data_3dgs(c.num, tv(c.means,{c.num,3}),tv(c.quats,{c.num,4}),tv(c.scales,{c.num,3}),
                  tv(c.opacities,{c.num,1}),tv(c.features_dc,{c.num,3}),tv(c.features_sh,{c.num,3,3}),0);
    const char* cameras[] = {"PINHOLE","FISHEYE","EQUISOLID","EQUIRECTANGULAR","ORTHOGRAPHIC"};
    const char* primitives[] = {"3dgs","mip","3dgut","2dgs"};
    const float identity[9] = {1,0,0,0,1,0,0,0,1};
    float view[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    std::vector<float> cv = {1,0,0,0, 0,-1,0,0, 0,0,-1,0, 0,0,0,1};
    std::vector<float> intr = {100,100,W*.5f,H*.5f}, dist(kCameraDistortionParams,0);
    std::vector<float> compute(W*H*3), pixels(W*H*4);
    TorchTensorView nil{0,4,{0}};
    bool failed = false;
    for (int camera = 0; camera < 5; ++camera) for (int primitive = 0; primitive < 4; ++primitive) {
        float fx = 100, fy = 100;
        if (camera == 1 || camera == 3) { fx = W/(2*3.14159265f); fy = H/3.14159265f; }
        if (camera == 2) fx = fy = W*.25f;
        intr[0] = fx; intr[1] = fy;
        set_camera_params(W,H,cameras[camera],"NONE",tv(cv,{1,4,4}),tv(intr,{1,4}),tv(dist,{1,kCameraDistortionParams}));
        forward_3dgs(primitives[primitive],1,false,false,0);
        backend::device_synchronize();
        engine_copy_render_to_host(tv(compute,{1,H,W,3}),nil,nil,nil,nil);
        unsigned texture = 0;
        int frames = 0;
        do { texture = renderer.render(W,H,view,camera,fx,fy,primitive,1,identity,0,false); ++frames; }
        while (renderer.refining() && frames < 1000);
        glBindTexture(GL_TEXTURE_2D,texture); glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_FLOAT,pixels.data());
        double sum = 0, signal = 0;
        for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) for (int j = 0; j < 3; ++j) {
            const double a = std::clamp(compute[(y*W+x)*3+j],0.f,1.f), b = pixels[((H-1-y)*W+x)*4+j];
            sum += std::abs(a-b); signal += a;
        }
        const double mean = sum/(W*H*3);
        std::printf("stochastic %s %s frames %d mean %.5f signal %.4f\n",cameras[camera],primitives[primitive],frames,mean,signal/(W*H*3));
        failed |= frames != 128 || mean > .006 || signal < 1;
        forward_3dgs(primitives[primitive],1,false,false,0,primitive >= 2);
        backend::device_synchronize();
        for (const auto& [buffer, name] : kBuffers) {
            if (buffer == Buffer::DepthNormal || (buffer == Buffer::RenderNormal && primitive < 2)) continue;
            const auto ref = compute_display(buffer,W,H,cameras[camera],intr,1);
            frames = 0;
            do { texture = renderer.render(W,H,view,camera,fx,fy,primitive,1,identity,0,false,buffer); ++frames; }
            while (renderer.refining() && frames < 1000);
            const auto [error, worst] = gl_difference(texture,W,H,ref,.006,std::string("stochastic-")+
                                                      cameras[camera]+"-"+primitives[primitive]+"-"+name);
            std::printf("stochastic %s %s %s mean %.5f max %.4f\n",cameras[camera],primitives[primitive],name,error,worst);
            failed |= error > .006;
        }
    }
    renderer.destroy(); engine_reset();
    std::filesystem::remove(path);
    glfwDestroyWindow(window); glfwTerminate();
    return !failed;
}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "--stochastic")
            return stochastic_frames() ? 0 : 1;
        if (argc >= 2 && std::string(argv[1]) == "--sort-benchmark") {
            if (argc < 3 || argc > 4) throw std::runtime_error("Usage: --sort-benchmark COUNT [REPEATS]");
            const auto count = std::stoull(argv[2]);
            const int repeats = argc == 4 ? std::stoi(argv[3]) : 3;
            if (!count || count > std::numeric_limits<uint32_t>::max() || repeats < 1 || repeats > 10)
                throw std::runtime_error("Invalid sort benchmark size or repeat count");
            check_depth_sort(); benchmark_depth_sort(uint32_t(count),repeats); return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--sort-test") {
            check_depth_sort(); return 0;
        }
        if (argc == 4 && std::string(argv[1]) == "--generate") {
            const int64_t count = std::stoll(argv[3]);
            spirula::BufferedOutputFile file(argv[2], std::ios::binary);
            file << spirula::splat_ply_header(count, {3, 0});
            const int stride = spirula::splat_ply_row_floats({3, 0});
            std::vector<float> rows(size_t(65536)*stride);
            std::mt19937 rng(20261117);
            auto uniform = [&] { return float(rng() & 0xffffff)/16777216; };
            for (int64_t first = 0; first < count; first += 65536) {
                const auto n = std::min<int64_t>(65536, count-first);
                std::fill(rows.begin(), rows.end(), 0);
                for (int64_t i = 0; i < n; ++i) {
                    float* r = rows.data()+i*stride;
                    r[0] = (uniform()-.5f)*80; r[1] = (uniform()-.5f)*60; r[2] = -1-uniform()*100;
                    for (int j = 6; j < 9; ++j) r[j] = (uniform()-.5f)*2;
                    for (int j = 9; j < 54; ++j) r[j] = (uniform()-.5f)*.05f;
                    r[54] = 0; r[55] = r[56] = r[57] = -3.5f; r[58] = 1;
                }
                file.write(reinterpret_cast<const char*>(rows.data()), n*stride*sizeof(float));
                if (!file) throw std::runtime_error("Synthetic PLY write failed");
            }
            file.flush(); if (!file) throw std::runtime_error("Synthetic PLY flush failed");
            return 0;
        }
        check_depth_sort();
        if (!glfwInit()) throw std::runtime_error("GLFW initialization failed");
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        auto* window = glfwCreateWindow(256, 256, "hardware splat test", nullptr, nullptr);
        if (!window) throw std::runtime_error("GL context creation failed");
        glfwMakeContextCurrent(window);
        gui::HardwareSplatRenderer renderer;
        float center[3]{};
        const float identity[9] = {1,0,0,0,1,0,0,0,1};
        float view[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
        constexpr int W = 192, H = 160;
        if (argc > 1) {
            const bool compare = argc >= 3 && std::string(argv[1]) == "--compare";
            const std::string path = argv[compare ? 2 : 1];
            const int W = 1280, H = 720;
            std::vector<float> sampled;
            const auto begin = std::chrono::steady_clock::now();
            spirula::read_splat_ply_chunks(path,65536,[&](const spirula::SplatCloud& c,int64_t first,int64_t total) {
                const int64_t stride = std::max<int64_t>(1,total/8192);
                for (int64_t i = (stride-first%stride)%stride; i < c.num; i += stride)
                    sampled.insert(sampled.end(),c.means.begin()+3*i,c.means.begin()+3*i+3);
            },false);
            float radius;
            gui::scene_extent(sampled,int64_t(sampled.size()/3),center,radius);
            const float unit = gui::view_distance_for(radius);
            renderer.load(path, center, unit);
            view[11] = -2;
            int camera_model = 0;
            float fx = W*.5f, fy = W*.5f;
            std::vector<float> cv = {1,0,0,-center[0], 0,-1,0,center[1], 0,0,-1,center[2]+2*unit, 0,0,0,1};
            if (compare && argc >= 4) {
                const auto cameras = json_parse_file(argv[3]);
                const auto& item = cameras.arr.at(argc >= 5 ? std::stoul(argv[4]) : 0);
                const auto& pose = item.find("opencv_w2c_row_major")->arr;
                const auto& camera = *item.find("camera");
                const auto& params = camera.find("params")->arr;
                camera_model = camera.get_double("model",0) == 5 ? 1 : 0;
                fx = float(params[0].num*W/camera.get_double("width",W));
                fy = float(params[1].num*H/camera.get_double("height",H));
                for (int i = 0; i < 16; ++i) cv[i] = float(pose.at(i).num);
                for (int r = 0; r < 3; ++r) {
                    float translation = cv[4*r+3];
                    for (int j = 0; j < 3; ++j) {
                        view[4*r+j] = (r ? -1 : 1)*cv[4*r+j];
                        translation += cv[4*r+j]*center[j];
                    }
                    view[4*r+3] = (r ? -1 : 1)*translation/unit;
                }
            }
            float original_view[16]; std::copy(view,view+16,original_view);
            const auto loaded = std::chrono::steady_clock::now();
            std::printf("load seconds: %.3f\n", std::chrono::duration<double>(loaded-begin).count());
            std::fflush(stdout);
            for (int p = 0; p < 4; ++p) {
                const auto start = std::chrono::steady_clock::now();
                const auto texture = renderer.render(W,H,view,camera_model,fx,fy,p,3,identity,0,false);
                glFinish();
                std::vector<float> pixels(W*H*4);
                glBindTexture(GL_TEXTURE_2D, texture);
                glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_FLOAT,pixels.data());
                double signal = 0;
                for (size_t i = 0; i < pixels.size(); ++i) {
                    if (!std::isfinite(pixels[i])) throw std::runtime_error("Non-finite hardware preview pixel");
                    if (i%4 != 3) signal += pixels[i];
                }
                std::printf("primitive %d seconds %.3f signal %.6f\n", p,
                    std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count(),signal/(W*H*3));
                std::fflush(stdout);
                std::vector<uint8_t> preview(W*H*3);
                for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) for (int j = 0; j < 3; ++j)
                    preview[(y*W+x)*3+j] = uint8_t(std::round(pixels[((H-1-y)*W+x)*4+j]*255));
                stbi_write_png(("hardware-preview-"+std::to_string(p)+".png").c_str(),W,H,3,preview.data(),W*3);
                if (!std::isfinite(signal) || signal < 1) throw std::runtime_error("Large model render is blank or non-finite");
            }
            std::printf("attribute bytes: %zu, last sort seconds: %.3f\n", renderer.attribute_bytes(),renderer.sort_seconds());
            for (int p = 0; p < 4; ++p) {
                const auto start = std::chrono::steady_clock::now();
                for (int frame = 0; frame < 5; ++frame) {
                    view[3] += .01f;
                    renderer.render(W,H,view,camera_model,fx,fy,p,3,identity,0,false);
                    for (int drain = 0; drain < 2; ++drain) {
                        renderer.finish_sort();
                        renderer.render(W,H,view,camera_model,fx,fy,p,3,identity,0,false);
                    }
                    glFinish();
                }
                std::printf("primitive %d coherent camera refresh milliseconds: %.3f\n",p,
                    200*std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count());
                std::fflush(stdout);
            }
            {
                float rotated[16]; std::copy(view,view+16,rotated);
                for (int j = 0; j < 4; ++j) {
                    rotated[j] = std::cos(.02f)*view[j]+std::sin(.02f)*view[8+j];
                    rotated[8+j] = -std::sin(.02f)*view[j]+std::cos(.02f)*view[8+j];
                }
                const auto start = std::chrono::steady_clock::now();
                renderer.render(W,H,rotated,camera_model,fx,fy,0,3,identity,0,false);
                renderer.finish_sort();
                renderer.render(W,H,rotated,camera_model,fx,fy,0,3,identity,0,false);
                renderer.finish_sort();
                renderer.render(W,H,rotated,camera_model,fx,fy,0,3,identity,0,false); glFinish();
                std::printf("rotation settle seconds %.3f, sort seconds %.3f\n",
                    std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count(),renderer.sort_seconds());
            }
            if (compare) {
                auto c = spirula::read_splat_ply(path);
                engine_reset();
                set_data_3dgs(c.num,tv(c.means,{c.num,3}),tv(c.quats,{c.num,4}),tv(c.scales,{c.num,3}),
                    tv(c.opacities,{c.num,1}),tv(c.features_dc,{c.num,3}),tv(c.features_sh,{c.num,c.coeffs(),3}),c.sb_lobes);
                std::copy(original_view,original_view+16,view);
                std::vector<float> intr = {fx,fy,W*.5f,H*.5f}, dist(kCameraDistortionParams,0);
                set_camera_params(W,H,camera_model == 1 ? "FISHEYE" : "PINHOLE","NONE",tv(cv,{1,4,4}),tv(intr,{1,4}),tv(dist,{1,kCameraDistortionParams}));
                const char* names[] = {"3dgs","mip","3dgut","2dgs"};
                for (int p = 0; p < 4; ++p) {
                    const auto start = std::chrono::steady_clock::now();
                    forward_3dgs(names[p],3,false,false,0); backend::device_synchronize();
                    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
                    std::vector<float> reference(W*H*3), pixels(W*H*4);
                    TorchTensorView nil{0,4,{0}};
                    engine_copy_render_to_host(tv(reference,{1,H,W,3}),nil,nil,nil,nil);
                    auto tex = renderer.render(W,H,view,camera_model,fx,fy,p,3,identity,0,false);
                    for (int drain = 0; drain < 2; ++drain) {
                        renderer.finish_sort();
                        tex = renderer.render(W,H,view,camera_model,fx,fy,p,3,identity,0,false);
                    }
                    glBindTexture(GL_TEXTURE_2D,tex); glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_FLOAT,pixels.data());
                    double mse = 0, mean = 0, maximum = 0;
                    std::vector<uint8_t> a(W*H*3), b(a.size());
                    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) for (int j = 0; j < 3; ++j) {
                        const int i = (y*W+x)*3+j;
                        const float raw = reference[i], bv = pixels[((H-1-y)*W+x)*4+j];
                        if (!std::isfinite(raw) || !std::isfinite(bv))
                            throw std::runtime_error("Non-finite real-model comparison pixel");
                        const float av = std::clamp(raw,0.f,1.f);
                        const double d = std::abs(av-bv); mean += d; mse += d*d; maximum = std::max(maximum,d);
                        a[i] = uint8_t(std::round(av*255)); b[i] = uint8_t(std::round(bv*255));
                    }
                    const std::string prefix = std::string("hardware-comparison-")+names[p];
                    stbi_write_png((prefix+"-compute.png").c_str(),W,H,3,a.data(),W*3);
                    stbi_write_png((prefix+"-gl.png").c_str(),W,H,3,b.data(),W*3);
                    std::printf("real %s compute seconds %.3f MAE %.6f max %.6f PSNR %.3f\n",names[p],seconds,
                        mean/a.size(),maximum,-10*std::log10(mse/a.size()));
                    std::fflush(stdout);
                    std::vector<double> compute_ms, hardware_ms;
                    for (int frame = 0; frame < 5; ++frame) {
                        const auto begin = std::chrono::steady_clock::now();
                        forward_3dgs(names[p],c.sh_degree,false,false,0); backend::device_synchronize();
                        compute_ms.push_back(1000*std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count());
                    }
                    for (int frame = 0; frame < 5; ++frame) {
                        const auto begin = std::chrono::steady_clock::now();
                        // Auto and explicit full SH produce the same image while invalidating the frame cache.
                        renderer.render(W,H,view,camera_model,fx,fy,p,frame%2 ? c.sh_degree : -1,identity,0,false);
                        renderer.finish_sort();
                        renderer.render(W,H,view,camera_model,fx,fy,p,frame%2 ? c.sh_degree : -1,identity,0,false);
                        glFinish();
                        hardware_ms.push_back(1000*std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count());
                    }
                    std::sort(compute_ms.begin(),compute_ms.end()); std::sort(hardware_ms.begin(),hardware_ms.end());
                    std::printf("real %s warmed median milliseconds compute %.3f hardware %.3f\n",names[p],compute_ms[2],hardware_ms[2]);
                    forward_3dgs(names[p],3,false,false,0,p >= 2); backend::device_synchronize();
                    for (const auto& [buffer, name] : kBuffers) {
                        if (buffer == Buffer::RenderNormal && p < 2) continue;
                        const auto ref = compute_display(buffer,W,H,camera_model == 1 ? "FISHEYE" : "PINHOLE",intr,3);
                        const auto shown = renderer.render(W,H,view,camera_model,fx,fy,p,3,identity,0,false,buffer);
                        const auto [error, worst] = gl_difference(shown,W,H,ref,1,"");
                        std::vector<double> ms;
                        for (int frame = 0; frame < 5; ++frame) {
                            const auto begin = std::chrono::steady_clock::now();
                            renderer.render(W,H,view,camera_model,fx,fy,p,frame%2 ? c.sh_degree : -1,identity,0,false,buffer);
                            glFinish();
                            ms.push_back(1000*std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count());
                        }
                        std::sort(ms.begin(),ms.end());
                        std::printf("real %s %s mean %.6f max %.4f hardware milliseconds %.3f\n",names[p],name,error,worst,ms[2]);
                    }
                }
                engine_reset();
            }
            renderer.destroy(); glfwDestroyWindow(window); glfwTerminate(); return 0;
        }
        spirula::SplatCloud c; c.num = 400; c.sh_degree = 3; c.sb_lobes = 2;
        const int row = int(c.coeffs());
        c.means.resize(c.num*3); c.quats.resize(c.num*4); c.scales.resize(c.num*3);
        c.opacities.resize(c.num); c.features_dc.resize(c.num*3); c.features_sh.resize(c.num*row*3);
        std::mt19937 rng(20261117);
        auto rand = [&] { return float(rng() & 65535)/65535; };
        for (int i = 0; i < c.num; ++i) {
            c.means[3*i] = (rand()-.5f)*3; c.means[3*i+1] = (rand()-.5f)*2;
            c.means[3*i+2] = -2-rand()*3;
            float length = 0;
            for (int j = 0; j < 4; ++j) { float q = rand()-.5f; c.quats[4*i+j] = q; length += q*q; }
            for (int j = 0; j < 4; ++j) c.quats[4*i+j] /= std::sqrt(length);
            for (int j = 0; j < 3; ++j) {
                c.scales[3*i+j] = -3-rand()*3;
                c.features_dc[3*i+j] = (rand()-.5f)*2;
            }
            c.opacities[i] = rand()*10-3;
        }
        for (int i = 0; i < 20; ++i) c.means[3*i+2] *= -1;
        c.means[60] = .015f; c.means[61] = .01f; c.means[62] = -.05f;
        c.scales[60] = -2.5f; c.scales[61] = -3.f; c.scales[62] = -3.f;
        c.scales[65] = -18.f;
        c.scales[66] = c.scales[67] = c.scales[68] = -10.f;
        c.features_dc[75] = c.features_dc[76] = c.features_dc[77] = 5;
        for (auto& v : c.features_sh) v = (rand()-.5f)*.15f;
        for (int i = 0; i < c.num; ++i) for (int l = 0; l < c.sb_lobes; ++l) {
            float* lobe = c.features_sh.data() + (size_t(i)*row + 15 + 2*l)*3;
            lobe[0] = rand()-.8f; lobe[1] = rand()-.8f; lobe[2] = rand()-.8f;
            lobe[3] = rand()*3.14159265f; lobe[4] = (rand()-.5f)*6.2831853f; lobe[5] = rand()*2-1;
        }
        const auto path = std::filesystem::temp_directory_path()/"spirula-hardware-parity.ply";
        spirula::write_splat_ply(c, path.string());
        const auto metadata = spirula::splat_ply_shape(path.string());
        if (metadata.sh_degree != 3 || metadata.sb_lobes != 2 || metadata.num != c.num || !metadata.means.empty())
            throw std::runtime_error("PLY metadata-only read lost the color basis");
        renderer.load(path.string(), center, 1);
        // Compare arithmetic after attribute quantization, including depth-sign boundaries.
        auto half = [](float x) { return nn::half_to_float(nn::float_to_half(x)); };
        for (auto& x : c.scales) x = half(x);
        for (auto& x : c.features_dc) x = half(x);
        for (int i = 0; i < c.num; ++i) {
            float norm = 0;
            for (int j = 0; j < 4; ++j) norm += c.quats[4*i+j]*c.quats[4*i+j];
            for (int j = 0; j < 4; ++j) c.quats[4*i+j] = half(c.quats[4*i+j]/std::sqrt(norm));
            norm = 0;
            for (int j = 0; j < 4; ++j) norm += c.quats[4*i+j]*c.quats[4*i+j];
            for (int j = 0; j < 4; ++j) c.quats[4*i+j] /= std::sqrt(norm);
            float* sh = c.features_sh.data() + size_t(i)*row*3;
            float scale = 0;
            for (int j = 0; j < 45; ++j) scale = std::max(scale,std::abs(sh[j]));
            scale = std::exp2(std::ceil(std::log2(scale)));
            for (int j = 0; j < 45; ++j) sh[j] = std::round(127*sh[j]/scale)*scale/127;
        }
        std::printf("Reference uses dequantized hardware attributes\n");
        engine_reset();
        set_data_3dgs(c.num, tv(c.means,{c.num,3}),tv(c.quats,{c.num,4}),tv(c.scales,{c.num,3}),
                     tv(c.opacities,{c.num,1}),tv(c.features_dc,{c.num,3}),tv(c.features_sh,{c.num,row,3}),c.sb_lobes);
        const char* cameras[] = {"PINHOLE","FISHEYE","EQUISOLID","EQUIRECTANGULAR","ORTHOGRAPHIC"};
        const char* primitives[] = {"3dgs","mip","3dgut","2dgs"};
        std::vector<float> cv = {1,0,0,0, 0,-1,0,0, 0,0,-1,0, 0,0,0,1};
        std::vector<float> intr = {100,100,W*.5f,H*.5f}, dist(kCameraDistortionParams,0);
        std::vector<float> compute(W*H*3), pixels(W*H*4);
        TorchTensorView nil{0,4,{0}};
        bool failed = false;
        for (int color = 0; color < 3; ++color) {
        const int transfer = color == 1 ? 2 : color == 2 ? 3 : 0;
        const bool linear = color == 1;
        const float gamut[9] = {color ? 1.1f : 1.f,0,0, 0,1,0, 0,0,color ? .9f : 1.f};
        engine_init_color_space(color != 0, transfer, linear,
            std::vector<float>(gamut,gamut+9), false,0,false,{});
        std::fill(view,view+16,0); view[0] = view[5] = view[10] = view[15] = 1;
        for (int pose = 0; pose < 2; ++pose) {
        if (pose) {
            view[0] = std::cos(.15f); view[2] = std::sin(.15f);
            view[8] = -view[2]; view[10] = view[0]; view[3] = -.3f; view[7] = .1f; view[11] = -.05f;
        }
        for (int r = 0; r < 3; ++r) for (int j = 0; j < 4; ++j)
            cv[r*4+j] = (r ? -1 : 1)*view[r*4+j];
        for (int camera = 0; camera < 5; ++camera) for (int primitive = 0; primitive < 4; ++primitive) {
            float fx = 100, fy = 100;
            if (camera == 1 || camera == 3) { fx = W/(2*3.14159265f); fy = H/3.14159265f; }
            if (camera == 2) fx = fy = W*.25f;
            intr[0] = fx; intr[1] = fy;
            set_camera_params(W,H,cameras[camera],"NONE",tv(cv,{1,4,4}),tv(intr,{1,4}),tv(dist,{1,kCameraDistortionParams}));
            forward_3dgs(primitives[primitive],3,false,false,0);
            backend::device_synchronize();
            engine_copy_render_to_host(tv(compute,{1,H,W,3}),nil,nil,nil,nil);
            const auto texture = renderer.render(W,H,view,camera,fx,fy,primitive,3,gamut,transfer,linear);
            glBindTexture(GL_TEXTURE_2D,texture); glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_FLOAT,pixels.data());
            double sum = 0, worst = 0, signal = 0;
            for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) for (int j = 0; j < 3; ++j) {
                const float raw = compute[(y*W+x)*3+j], b = pixels[((H-1-y)*W+x)*4+j];
                if (!std::isfinite(raw) || !std::isfinite(b))
                    throw std::runtime_error("Non-finite synthetic comparison pixel");
                const double a = std::clamp(raw,0.f,1.f);
                double d = std::abs(a-b); sum += d; worst = std::max(worst,d); signal += a;
            }
            const double mean = sum/(W*H*3);
            std::printf("color%d pose%d %s %s mean %.7f max %.7f signal %.7f\n",color,pose,cameras[camera],primitives[primitive],mean,worst,signal/(W*H*3));
            if (color == 0 && (mean > .002 || worst > .06)) {
                std::vector<uint8_t> a(W*H*3), b(a.size());
                for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) for (int j = 0; j < 3; ++j) {
                    const int i = (y*W+x)*3+j;
                    a[i] = uint8_t(std::round(std::clamp(compute[i],0.f,1.f)*255));
                    b[i] = uint8_t(std::round(pixels[((H-1-y)*W+x)*4+j]*255));
                }
                const auto prefix = (std::filesystem::temp_directory_path()/"gl-parity-").string()+std::to_string(pose)+"-"+cameras[camera]+"-"+primitives[primitive];
                stbi_write_png((prefix+"-compute.png").c_str(),W,H,3,a.data(),W*3);
                stbi_write_png((prefix+"-gl.png").c_str(),W,H,3,b.data(),W*3);
            }
            failed |= mean > .002 || worst > .06 || signal < 1;
            if (color) continue;
            forward_3dgs(primitives[primitive],3,false,false,0,primitive >= 2);
            backend::device_synchronize();
            for (const auto& [buffer, name] : kBuffers) {
                if (buffer == Buffer::RenderNormal && primitive < 2) continue;
                const auto ref = compute_display(buffer,W,H,cameras[camera],intr,3);
                const auto tex = renderer.render(W,H,view,camera,fx,fy,primitive,3,gamut,transfer,linear,buffer);
                // A depth stencil on an equisolid rim, where 180 degrees packs into pixels.
                const double limit = buffer == Buffer::DepthNormal ? .004 : .002;
                const auto [error, worst] = gl_difference(tex,W,H,ref,limit,std::to_string(pose)+"-"+
                                                          cameras[camera]+"-"+primitives[primitive]+"-"+name);
                std::printf("pose%d %s %s %s mean %.7f max %.7f\n",pose,cameras[camera],primitives[primitive],name,error,worst);
                failed |= error > limit;
            }
        }
        }
        }
        renderer.destroy(); engine_reset();
        spirula::SplatCloud opaque;
        opaque.num = 2; opaque.sh_degree = 0;
        opaque.means = {.01f,.01f,-2.f,.015f,.015f,-3.f};
        opaque.quats = {1,0,0,0,1,0,0,0}; opaque.scales.assign(6,-.7f);
        opaque.opacities = {std::log(.92f/.08f),20.f};
        const float dc = .5f/.28209479177387814f;
        opaque.features_dc = {dc,-dc,-dc,-dc,dc,-dc};
        const auto opaque_path = std::filesystem::temp_directory_path()/"spirula-hardware-opaque.ply";
        spirula::write_splat_ply(opaque,opaque_path.string());
        renderer.load(opaque_path.string(),center,1);
        set_data_3dgs(2,tv(opaque.means,{2,3}),tv(opaque.quats,{2,4}),tv(opaque.scales,{2,3}),
            tv(opaque.opacities,{2,1}),tv(opaque.features_dc,{2,3}),tv(opaque.features_sh,{2,0,3}),0);
        cv = {1,0,0,0,0,-1,0,0,0,0,-1,0,0,0,0,1};
        intr = {100,100,W*.5f,H*.5f};
        set_camera_params(W,H,"PINHOLE","NONE",tv(cv,{1,4,4}),tv(intr,{1,4}),tv(dist,{1,kCameraDistortionParams}));
        std::fill(view,view+16,0); view[0] = view[5] = view[10] = view[15] = 1;
        // Compute drops the green splat, which would take T under 1e-4; blending
        // cannot test the target's T first, so the GL view draws it.
        std::vector<float> Ts(W*H);
        for (int p = 0; p < 4; ++p) {
            forward_3dgs(primitives[p],0,false,false,0); backend::device_synchronize();
            engine_copy_render_to_host(tv(compute,{1,H,W,3}),nil,tv(Ts,{1,H,W,1}),nil,nil);
            const auto tex = renderer.render(W,H,view,0,100,100,p,0,identity,0,false);
            glBindTexture(GL_TEXTURE_2D,tex); glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_FLOAT,pixels.data());
            const int x = W/2, y = H/2-1;
            double error = 0;
            for (int j = 0; j < 3; ++j) {
                const float a = compute[(y*W+x)*3+j] + (j == 1 ? Ts[y*W+x] : 0.f), b = pixels[((H-1-y)*W+x)*4+j];
                if (!std::isfinite(a) || !std::isfinite(b))
                    throw std::runtime_error("Non-finite opaque-overlap comparison pixel");
                error = std::max(error,double(std::abs(a-b)));
            }
            std::printf("opaque %s center error %.7f, compute T %.4f\n",primitives[p],error,Ts[y*W+x]);
            failed |= error > .003 || Ts[y*W+x] < .05;
        }
        renderer.destroy(); engine_reset(); std::filesystem::remove(opaque_path);
        spirula::SplatCloud near_plane;
        near_plane.num = 1;
        near_plane.means = {0,0,0}; near_plane.quats = {1,0,0,0};
        near_plane.scales.resize(3); near_plane.opacities = {std::log(4.f)};
        near_plane.features_dc = {0,0,0};
        const auto near_path = std::filesystem::temp_directory_path()/"spirula-hardware-near.ply";
        for (int camera = 0; camera < 5; ++camera) {
            for (float depth : {0.f, std::exp2(-8.f), std::exp2(-14.f)}) {
                near_plane.means[2] = -depth;
                const float scale = camera == 4 ? .125f : std::max(depth,.00006103515625f)/8;
                near_plane.scales.assign(3,half(std::log(scale)));
                spirula::write_splat_ply(near_plane,near_path.string());
                renderer.load(near_path.string(),center,1);
                set_data_3dgs(1,tv(near_plane.means,{1,3}),tv(near_plane.quats,{1,4}),
                    tv(near_plane.scales,{1,3}),tv(near_plane.opacities,{1,1}),
                    tv(near_plane.features_dc,{1,3}),tv(near_plane.features_sh,{1,0,3}),0);
                set_camera_params(W,H,cameras[camera],"NONE",tv(cv,{1,4,4}),tv(intr,{1,4}),tv(dist,{1,kCameraDistortionParams}));
                for (int p = 0; p < 4; ++p) {
                    forward_3dgs(primitives[p],0,false,false,0); backend::device_synchronize();
                    engine_copy_render_to_host(tv(compute,{1,H,W,3}),nil,nil,nil,nil);
                    const auto tex = renderer.render(W,H,view,camera,100,100,p,0,identity,0,false);
                    glBindTexture(GL_TEXTURE_2D,tex); glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_FLOAT,pixels.data());
                    double worst = 0, signal = 0, gl_signal = 0;
                    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) for (int j = 0; j < 3; ++j) {
                        const float a = compute[(y*W+x)*3+j], b = pixels[((H-1-y)*W+x)*4+j];
                        if (!std::isfinite(a) || !std::isfinite(b))
                            throw std::runtime_error("Non-finite near-plane comparison pixel");
                        worst = std::max(worst,double(std::abs(a-b))); signal += a; gl_signal += b;
                    }
                    std::printf("near %s %s depth %.8g max %.7f signal %.6f/%.6f\n",
                        cameras[camera],primitives[p],depth,worst,signal,gl_signal);
                    failed |= worst > .015 || (depth == 0 ? signal != 0 || gl_signal != 0 : signal < 1 || gl_signal < 1);
                }
                renderer.destroy(); engine_reset();
            }
        }
        std::filesystem::remove(near_path);
        if (!failed) std::filesystem::remove(path);
        coherent_frames();
        glfwDestroyWindow(window); glfwTerminate();
        return failed ? 1 : 0;
    } catch (const std::exception& e) { std::fprintf(stderr,"%s\n",e.what()); return 1; }
}

// Self-checking test: the viewer's training-camera frusta with more cameras
// than one dispatch dimension holds (65535 on Vulkan). Every camera sits
// behind the view except the last, so its frustum showing up in the blit
// proves the fill reached an index past the limit. Runs under either backend:
//
//   ./viewer_many_cameras

#include <engine/Engine.h>
#include <engine/EngineState.h>
#include <kernels/visualize/Visualizer.cuh>

#include <cstdio>
#include <exception>
#include <vector>

using backend::MemcpyKind;

static constexpr int N_CAM = 65600;
static constexpr int W = 240, H = 180;

template <typename T>
static T* upload(const std::vector<T>& host) {
    T* d = (T*)backend::device_malloc(host.size() * sizeof(T));
    backend::memcpy_sync(d, host.data(), host.size() * sizeof(T),
                         MemcpyKind::HostToDevice);
    return d;
}

static TorchTensorView ttv(const void* p, std::vector<int64_t> shape) {
    return std::make_tuple((uint64_t)p, (uint32_t)4, std::move(shape));
}

int main() {
    std::vector<float> intr((size_t)N_CAM * 4), c2w((size_t)N_CAM * 12, 0.f),
        dist((size_t)N_CAM * kCameraDistortionParams, 0.f);
    std::vector<int32_t> widths(N_CAM, 320), heights(N_CAM, 240),
        models(N_CAM, 0), tiers(N_CAM, 0);
    for (int i = 0; i < N_CAM; i++) {
        intr[4 * i + 0] = 260;
        intr[4 * i + 1] = 260;
        intr[4 * i + 2] = 160;
        intr[4 * i + 3] = 120;
        float* m = &c2w[(size_t)12 * i];
        m[0] = m[5] = m[10] = 1.f;
        m[11] = i == N_CAM - 1 ? 0.f : -100.f;
    }

    // View camera (world -> camera, OpenCV): the origin 3.5 ahead of it.
    std::vector<float> vm = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 3.5f, 0, 0, 0, 1};
    std::vector<float> vintr = {180, 180, W / 2.f, H / 2.f};
    std::vector<float> vdist(kCameraDistortionParams, 0.f);
    float* d_vm = upload(vm);
    float* d_vi = upload(vintr);
    float* d_vd = upload(vdist);
    float* d_rgb = upload(std::vector<float>((size_t)H * W * 3, 0.5f));
    float* d_depth = upload(std::vector<float>((size_t)H * W, 0.f));
    float* d_alpha = upload(std::vector<float>((size_t)H * W, 0.f));
    uint8_t* d_out = (uint8_t*)backend::device_malloc((size_t)H * W * 3);

    std::vector<uint8_t> with_cams((size_t)H * W * 3), without((size_t)H * W * 3);
    try {
        engine_viewer_init(ttv(models.data(), {N_CAM}),
                           ttv(intr.data(), {N_CAM, 4}),
                           ttv(dist.data(), {N_CAM, kCameraDistortionParams}),
                           ttv(tiers.data(), {N_CAM}),
                           ttv(c2w.data(), {N_CAM, 3, 4}),
                           ttv(widths.data(), {N_CAM}),
                           ttv(heights.data(), {N_CAM}), 0.8f);
        for (bool cams : {true, false}) {
            engine_blit_view("rgb", ttv(d_rgb, {H, W, 3}),
                             ttv(d_depth, {H, W, 1}), ttv(d_alpha, {H, W, 1}),
                             false, 0, "NONE", ttv(d_vi, {1, 4}), ttv(d_vm, {4, 4}),
                             ttv(d_vd, {1, kCameraDistortionParams}), cams,
                             /*show_overlay=*/false, 0.f, 0.f, 0.f, 0.f,
                             ttv(d_out, {H, W, 3}));
            backend::device_synchronize();
            if (const char* err = backend::last_error()) {
                std::fprintf(stderr, "backend error: %s\n", err);
                return 1;
            }
            backend::memcpy_sync(cams ? with_cams.data() : without.data(),
                                 d_out, (size_t)H * W * 3,
                                 MemcpyKind::DeviceToHost);
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "viewer_many_cameras: %s\n", e.what());
        std::printf("viewer_many_cameras: FAILED\n");
        return 1;
    }

    int changed = 0;
    for (size_t i = 0; i < with_cams.size(); i += 3)
        changed += with_cams[i] != without[i] ||
                   with_cams[i + 1] != without[i + 1] ||
                   with_cams[i + 2] != without[i + 2];
    // That frustum is ~40x30 px from here.
    const bool pass = changed > 100;
    std::printf("viewer_many_cameras: %d cameras, %d pixels drawn by camera "
                "%d -> %s\n",
                N_CAM, changed, N_CAM - 1, pass ? "PASSED" : "FAILED");
    return pass ? 0 : 1;
}

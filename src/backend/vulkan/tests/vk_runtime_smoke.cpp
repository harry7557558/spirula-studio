// Smoke test for the Vulkan backend runtime (backend/api/BackendRuntime.h).
// Exercises every API the engine layer uses: malloc/free, pinned host
// memory, all memcpy kinds (sync + async, pinned and pageable), memset,
// pointer classification, streams, events (incl. timing), and the sticky
// error channel. Run on each available device via SS_VK_DEVICE.

#include "backend/api/BackendRuntime.h"
#include "core/SourcePath.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

static int g_failures = 0;

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::printf("FAIL %s:%d: %s\n", SS_FILE, __LINE__, #cond);    \
            g_failures++;                                                 \
        }                                                                 \
    } while (0)

#define CHECK_NO_ERROR()                                                  \
    do {                                                                  \
        const char* err_ = backend::last_error();                         \
        if (err_) {                                                       \
            std::printf("FAIL %s:%d: unexpected backend error: %s\n",     \
                        SS_FILE, __LINE__, err_);                         \
            g_failures++;                                                 \
        }                                                                 \
    } while (0)

int main() {
    using namespace backend;

    // --- allocation + classification ---
    const size_t N = 1 << 20;  // 1M floats = 4MB
    void* dev_a = device_malloc(N * sizeof(float));
    void* dev_b = device_malloc(N * sizeof(float));
    CHECK(dev_a != nullptr);
    CHECK(dev_b != nullptr);
    CHECK_NO_ERROR();
    CHECK(is_device_pointer(dev_a));
    CHECK(is_device_pointer((char*)dev_a + 1234));
    CHECK(!is_device_pointer(nullptr));
    int on_stack = 0;
    CHECK(!is_device_pointer(&on_stack));
    CHECK(device_malloc(0) == nullptr);
    CHECK_NO_ERROR();

    float* pinned = (float*)host_malloc_pinned(N * sizeof(float));
    CHECK(pinned != nullptr);
    CHECK(!is_device_pointer(pinned));  // host-pinned is HOST (CUDA parity)
    CHECK_NO_ERROR();

    // --- sync H2D (pageable) + D2H (pageable) round trip ---
    std::vector<float> src(N), dst(N, 0.f);
    for (size_t i = 0; i < N; i++) src[i] = (float)(i % 9973) * 0.25f;
    memcpy_sync(dev_a, src.data(), N * sizeof(float),
                MemcpyKind::HostToDevice);
    CHECK_NO_ERROR();
    memcpy_sync(dst.data(), dev_a, N * sizeof(float),
                MemcpyKind::DeviceToHost);
    CHECK_NO_ERROR();
    CHECK(std::memcmp(src.data(), dst.data(), N * sizeof(float)) == 0);

    // --- D2D + offset arithmetic on device pointers ---
    memcpy_sync(dev_b, dev_a, N * sizeof(float), MemcpyKind::DeviceToDevice);
    std::fill(dst.begin(), dst.end(), 0.f);
    memcpy_sync(dst.data(), (char*)dev_b + 512 * sizeof(float),
                (N - 512) * sizeof(float), MemcpyKind::DeviceToHost);
    CHECK_NO_ERROR();
    CHECK(std::memcmp(src.data() + 512, dst.data(),
                      (N - 512) * sizeof(float)) == 0);

    // --- Auto kind routing ---
    std::fill(dst.begin(), dst.end(), 0.f);
    memcpy_sync(dst.data(), dev_a, N * sizeof(float), MemcpyKind::Auto);
    CHECK(std::memcmp(src.data(), dst.data(), N * sizeof(float)) == 0);
    CHECK_NO_ERROR();

    // --- memset (value and zero, with element tail) ---
    memset_sync(dev_a, 0, N * sizeof(float));
    std::fill(dst.begin(), dst.end(), 1.f);
    memcpy_sync(dst.data(), dev_a, N * sizeof(float),
                MemcpyKind::DeviceToHost);
    for (size_t i = 0; i < N; i++)
        if (dst[i] != 0.f) { CHECK(dst[i] == 0.f); break; }
    memset_sync(dev_a, 0x5A, 64);
    uint8_t bytes64[64];
    memcpy_sync(bytes64, dev_a, 64, MemcpyKind::DeviceToHost);
    for (int i = 0; i < 64; i++)
        if (bytes64[i] != 0x5A) { CHECK(bytes64[i] == 0x5A); break; }
    CHECK_NO_ERROR();

    // --- async path: pinned D2H readback with events (AsyncReadout shape) ---
    memcpy_sync(dev_a, src.data(), N * sizeof(float),
                MemcpyKind::HostToDevice);
    std::memset(pinned, 0, N * sizeof(float));
    Event* evt = event_create(false);
    memcpy_async(pinned, dev_a, N * sizeof(float), MemcpyKind::DeviceToHost,
                 kDefaultStream);
    event_record(evt, kDefaultStream);
    event_synchronize(evt);
    CHECK_NO_ERROR();
    CHECK(std::memcmp(src.data(), pinned, N * sizeof(float)) == 0);
    event_destroy(evt);

    // --- async pinned H2D + async D2D + memset_async, one batch ---
    for (size_t i = 0; i < N; i++) pinned[i] = 2.0f * (float)(i % 4096);
    memcpy_async(dev_a, pinned, N * sizeof(float), MemcpyKind::HostToDevice,
                 kDefaultStream);
    memcpy_async(dev_b, dev_a, N * sizeof(float), MemcpyKind::DeviceToDevice,
                 kDefaultStream);
    memset_async(dev_a, 0, N * sizeof(float), kDefaultStream);
    stream_synchronize(kDefaultStream);
    CHECK_NO_ERROR();
    std::fill(dst.begin(), dst.end(), -1.f);
    memcpy_sync(dst.data(), dev_b, N * sizeof(float),
                MemcpyKind::DeviceToHost);
    CHECK(std::memcmp(pinned, dst.data(), N * sizeof(float)) == 0);
    memcpy_sync(dst.data(), dev_a, N * sizeof(float),
                MemcpyKind::DeviceToHost);
    CHECK(dst[0] == 0.f && dst[N - 1] == 0.f);

    // --- async pageable H2D (staging ring, multi-MB) ---
    memcpy_async(dev_a, src.data(), N * sizeof(float),
                 MemcpyKind::HostToDevice, kDefaultStream);
    device_synchronize();
    CHECK_NO_ERROR();
    std::fill(dst.begin(), dst.end(), 0.f);
    memcpy_sync(dst.data(), dev_a, N * sizeof(float),
                MemcpyKind::DeviceToHost);
    CHECK(std::memcmp(src.data(), dst.data(), N * sizeof(float)) == 0);

    // --- timing events ---
    Event* t0 = event_create(true);
    Event* t1 = event_create(true);
    event_record(t0, kDefaultStream);
    memcpy_async(dev_b, dev_a, N * sizeof(float), MemcpyKind::DeviceToDevice,
                 kDefaultStream);
    event_record(t1, kDefaultStream);
    event_synchronize(t1);
    float ms = event_elapsed_ms(t0, t1);
    const char* timing_err = last_error();
    if (timing_err) {
        std::printf("note: event timing unavailable on this device (%s)\n",
                    timing_err);
    } else {
        CHECK(ms >= 0.0f && ms < 10000.0f);
        std::printf("note: 4MB D2D took %.3f ms\n", ms);
    }
    event_destroy(t0);
    event_destroy(t1);

    // --- error channel: bad free reports, then clears ---
    device_free((char*)dev_a + 64);  // not a base pointer
    CHECK(last_error() != nullptr);
    CHECK(last_error() == nullptr);  // cleared by the read

    // --- cleanup (device_free syncs in-flight work per contract) ---
    memcpy_async(dev_b, dev_a, N * sizeof(float), MemcpyKind::DeviceToDevice,
                 kDefaultStream);  // leave work in flight
    device_free(dev_a);
    device_free(dev_b);
    host_free_pinned(pinned);
    device_synchronize();
    CHECK_NO_ERROR();

    std::printf(g_failures == 0 ? "vk_runtime_smoke: ALL PASSED\n"
                                : "vk_runtime_smoke: %d FAILURES\n",
                g_failures);
    return g_failures == 0 ? 0 : 1;
}

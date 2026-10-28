// Pipeline-layer smoke test: dispatches shaders/test_kernels.slang
// through the embedded-SPIR-V pipeline cache and verifies results, including
// specialization constants, split ranges and batched ordering on one stream.

#include "backend/api/BackendRuntime.h"
#include "backend/vulkan/VulkanPipelines.h"
#include "core/SourcePath.h"

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

struct SaxpyParams {
    uint64_t x;
    uint64_t y;
    float a;
    uint32_t n;
};

struct IotaParams {
    uint64_t out;
    uint32_t n;
    uint32_t offset;
};

static void check_dispatch_ranges(const char* entry) {
    using namespace backend;
    const uint32_t grid[3] = {7, 5, 3};
    const uint32_t n = 7 * 5 * 3 * 4;
    void* dev = device_malloc(n * sizeof(uint32_t));
    CHECK(dev != nullptr);
    if (!dev) return;
    struct Params {
        uint64_t out;
        uint32_t nx, ny;
    } p{(uint64_t)dev, grid[0], grid[1]};
    std::vector<uint32_t> got(n);
    for (int axis = -1; axis < 4; ++axis) {
        memset_sync(dev, 0, n * sizeof(uint32_t));
        if (axis < 0) {
            CHECK(vk::dispatch(kDefaultStream, entry, {},
                               grid[0], grid[1], grid[2], &p, sizeof(p)));
        } else if (axis == 3) {
            const uint32_t base[3] = {1, 1, 1}, count[3] = {5, 3, 1};
            CHECK(vk::dispatch_range(kDefaultStream, entry, {}, base, count,
                                     &p, sizeof(p)));
        } else {
            for (uint32_t offset = 0; offset < grid[axis]; ++offset) {
                uint32_t base[3] = {}, count[3] = {7, 5, 3};
                base[axis] = offset;
                count[axis] = 1;
                CHECK(vk::dispatch_range(kDefaultStream, entry, {}, base, count,
                                         &p, sizeof(p)));
            }
        }
        memcpy_sync(got.data(), dev, n * sizeof(uint32_t),
                    MemcpyKind::DeviceToHost);
        uint32_t bad = 0;
        for (uint32_t z = 0; z < grid[2]; ++z)
            for (uint32_t y = 0; y < grid[1]; ++y)
                for (uint32_t x = 0; x < grid[0] * 4; ++x) {
                    uint32_t i = (z * grid[1] + y) * grid[0] * 4 + x;
                    uint32_t expected = 1 + x + 1024 * y + 1048576 * z;
                    if (axis == 3 &&
                        (x < 4 || x >= 24 || y < 1 || y >= 4 || z != 1))
                        expected = 0;
                    if (got[i] != expected && ++bad <= 3)
                        std::printf("grid mismatch @%u: got %u want %u\n",
                                    i, got[i], expected);
                }
        std::printf("%s axis %d: %u mismatches / %u\n", entry, axis, bad, n);
        CHECK(bad == 0);
        CHECK(last_error() == nullptr);
    }
    device_free(dev);
}

int main() {
    using namespace backend;

    check_dispatch_ranges("test_kernels.dispatch_grid");
    check_dispatch_ranges("test_kernels.dispatch_group_only");

    const uint32_t N = 1 << 20;
    void* dev_x = device_malloc(N * sizeof(float));
    void* dev_y = device_malloc(N * sizeof(float));
    void* dev_i = device_malloc(N * sizeof(uint32_t));
    CHECK(dev_x && dev_y && dev_i);

    std::vector<float> x(N), y(N);
    for (uint32_t i = 0; i < N; i++) {
        x[i] = 0.001f * (float)(i % 1000);
        y[i] = 1.0f + (float)(i % 7);
    }
    memcpy_sync(dev_x, x.data(), N * sizeof(float),
                MemcpyKind::HostToDevice);
    memcpy_sync(dev_y, y.data(), N * sizeof(float),
                MemcpyKind::HostToDevice);

    // Two saxpy dispatches batched on the default stream: the second reads
    // the first's output, exercising the inter-dispatch barrier. The second
    // uses spec constant kScale=2.
    SaxpyParams p{(uint64_t)dev_x, (uint64_t)dev_y, 0.5f, N};
    CHECK(vk::dispatch(kDefaultStream, "test_kernels.saxpy", vk::SpecList{},
                       (N + 255) / 256, 1, 1, &p, sizeof(p)));
    CHECK(vk::dispatch(kDefaultStream, "test_kernels.saxpy",
                       vk::SpecList{2u}, (N + 255) / 256, 1, 1, &p,
                       sizeof(p)));
    IotaParams ip{(uint64_t)dev_i, N, 42};
    CHECK(vk::dispatch(kDefaultStream, "test_kernels.iota_u32",
                       vk::SpecList{}, (N + 255) / 256, 1, 1, &ip,
                       sizeof(ip)));
    memset_sync(dev_i, 0, N * sizeof(uint32_t));
    for (uint32_t offset : {0u, 3u}) {
        const uint32_t base[3] = {offset, 0, 0};
        const uint32_t count[3] = {offset == 0 ? 3u : N / 256 - 3, 1, 1};
        CHECK(vk::dispatch_range(kDefaultStream, "test_kernels.iota_u32", {},
                                 base, count, &ip, sizeof(ip)));
    }
    device_synchronize();
    const char* err = last_error();
    if (err) std::printf("backend error: %s\n", err);
    CHECK(err == nullptr);

    std::vector<float> got(N);
    memcpy_sync(got.data(), dev_y, N * sizeof(float),
                MemcpyKind::DeviceToHost);
    int bad = 0;
    for (uint32_t i = 0; i < N; i++) {
        float expect = (y[i] + 0.5f * x[i]) + 2.0f * (0.5f * x[i]);
        if (got[i] != expect && ++bad <= 3)
            std::printf("saxpy mismatch @%u: got %f want %f\n", i, got[i],
                        expect);
    }
    CHECK(bad == 0);

    std::vector<uint32_t> ids(N);
    memcpy_sync(ids.data(), dev_i, N * sizeof(uint32_t),
                MemcpyKind::DeviceToHost);
    bad = 0;
    for (uint32_t i = 0; i < N; i++)
        if (ids[i] != i + 42 && ++bad <= 3)
            std::printf("iota mismatch @%u: got %u\n", i, ids[i]);
    CHECK(bad == 0);

    // Unknown entry name reports and does not crash.
    CHECK(!vk::dispatch(kDefaultStream, "no.such_kernel", vk::SpecList{}, 1,
                        1, 1, nullptr, 0));
    CHECK(last_error() != nullptr);

    device_free(dev_x);
    device_free(dev_y);
    device_free(dev_i);

    std::printf(g_failures == 0 ? "vk_pipeline_smoke: ALL PASSED\n"
                                : "vk_pipeline_smoke: %d FAILURES\n",
                g_failures);
    return g_failures == 0 ? 0 : 1;
}

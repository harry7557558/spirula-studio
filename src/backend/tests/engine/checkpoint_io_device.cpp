#include "core/CheckpointIO.h"
#include "engine/Engine.h"
#include "engine/EngineInternal.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
namespace fs = std::filesystem;

void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

void check_device_reads() {
    constexpr size_t chunk = spirula::kFileBufferBytes;
    constexpr size_t bytes = chunk + 16;
    constexpr char sentinel = 0x6a, payload = 0x35;
    std::unique_ptr<void, decltype(&backend::device_free)> device(
        backend::device_malloc_checked(bytes), &backend::device_free);
    std::vector<char> staging, result(bytes);
    auto check = [&](size_t available, size_t offset, size_t restored) {
        backend::memset_sync(device.get(), sentinel, bytes);
        std::istringstream source(std::string(available, payload));
        bool failed = false;
        try { ckpt::read_into_device(source, offset, device.get(), bytes, staging); }
        catch (const std::runtime_error&) { failed = true; }
        backend::memcpy_sync(result.data(), device.get(), bytes, backend::MemcpyKind::DeviceToHost);
        require(failed, "incomplete device read succeeded");
        const auto boundary = result.begin() + restored;
        require(std::all_of(result.begin(), boundary, [&](char c) { return c == payload; }),
                "successful device prefix was not restored");
        require(std::all_of(boundary, result.end(), [&](char c) { return c == sentinel; }),
                "failed chunk overwrote its device sentinel");
    };
    check(8, 32, 0);
    check(8, 0, 0);
    check(chunk + 8, 0, chunk);
}

void check_truncated_load(const fs::path& root) {
    engine_reset();
    float means[]{0, 0, 3, 1, 0, 4}, quats[]{1, 0, 0, 0, 1, 0, 0, 0};
    float scales[]{-2, -2, -2, -2, -2, -2}, opacity[]{0, 0}, color[6]{}, sh[18]{};
    auto view = [](float* p, std::vector<int64_t> shape) -> TorchTensorView {
        return {reinterpret_cast<uint64_t>(p), 4, std::move(shape)};
    };
    set_data_3dgs(2, view(means, {2, 3}), view(quats, {2, 4}), view(scales, {2, 3}),
                  view(opacity, {2, 1}), view(color, {2, 3}), view(sh, {2, 3, 3}), 0);
    engine_ensure_optim_state(32, 32, 32, false);
    engine_save_checkpoint(root.string(), true, 97);
    const auto tar = root / "state.tar";
    require(engine_load_checkpoint(tar.string()) == 97, "valid checkpoint failed to restore its step");
    fs::resize_file(tar, fs::file_size(tar) - 1024);
    bool rejected = false;
    try { engine_load_checkpoint(tar.string()); }
    catch (const std::runtime_error& e) {
        rejected = std::string(e.what()).find("checkpoint TAR") != std::string::npos;
    }
    require(rejected, "engine accepted an archive missing its terminator");
    engine_reset();
}
}  // namespace

int main() {
    const auto root = fs::temp_directory_path() / ("ss-checkpoint-device-" + std::to_string(
        static_cast<long long>(std::chrono::steady_clock::now().time_since_epoch().count())));
    if (!fs::create_directory(root)) return 1;
    std::promise<void> completed;
    auto watchdog = std::thread([done = completed.get_future()] {
        if (done.wait_for(std::chrono::seconds(90)) == std::future_status::timeout) {
            std::fputs("checkpoint_io_device timed out\n", stderr);
            std::_Exit(1);
        }
    });
    int status = 0;
    try {
        check_device_reads();
        check_truncated_load(root);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        status = 1;
    }
    std::error_code ec;
    fs::remove_all(root, ec);
    completed.set_value();
    watchdog.join();
    if (!status) std::puts("checkpoint_io_device passed");
    return status;
}

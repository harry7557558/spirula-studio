#include "data/DataManager.h"
#include "external/stb_image_write.h"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

namespace {

constexpr int kWidth = 8, kHeight = 8, kSteps = 64;
constexpr std::array<uint8_t, 3> kColors[] = {{17, 83, 241}, {193, 41, 7}};
using ImagePaths = std::array<std::string, 2>;

void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

void write_image(const std::string& path, int index) {
    std::array<uint8_t, kWidth * kHeight * 3> pixels;
    for (size_t i = 0; i < pixels.size(); ++i) pixels[i] = kColors[index][i % 3];
    require(stbi_write_png(path.c_str(), kWidth, kHeight, 3, pixels.data(), kWidth * 3) != 0,
            "could not write RGB fixture");
}

DataManager make_manager(const ImagePaths& paths, CacheMode mode,
                         std::vector<int32_t> train, std::vector<int32_t> val = {}) {
    DataManagerConfig cfg;
    cfg.cache_mode = mode;
    cfg.load_masks = cfg.load_depths = cfg.load_normals = false;
    cfg.seed = 42;
    cfg.workers_rgb = 1;
    cfg.prefetch_batches = 1;
    std::vector<float> viewmats(32, 0.0f);
    for (int i = 0; i < 4; ++i) viewmats[i * 5] = viewmats[16 + i * 5] = 1.0f;
    return DataManager(cfg, {0, 0}, {0, 0}, {paths[0], paths[1]}, {}, {}, {},
        {kWidth, kWidth}, {kHeight, kHeight}, {}, {}, std::move(viewmats),
        {10, 10, 4, 4, 10, 10, 4, 4}, std::vector<float>(16, 0.0f),
        {}, {}, {}, {}, {}, {}, {}, std::move(train), std::move(val));
}

void check_batch(const DecodedBatch& batch, int index) {
    require(batch.indices.size() == 1 && batch.indices[0] == index,
            "batch did not contain exactly the expected camera");
    require(batch.width == kWidth && batch.height == kHeight && batch.num == 1 &&
            batch.rgb_dtype == PixelDType::UINT8 && batch.rgb_buffer.size() == kWidth * kHeight * 3,
            "unexpected batch shape or dtype");
    for (size_t i = 0; i < batch.rgb_buffer.size(); ++i)
        require(batch.rgb_buffer[i] == kColors[index][i % 3], "wrong RGB bytes");
}

void check_step(DataManager& manager, int index) {
    const auto& step = manager.next_train_step();
    require(step.subs.size() == 1 && step.subs.front(), "expected one training sub-batch");
    check_batch(*step.subs.front(), index);
}

template<typename Scenario>
void run_watched(const char* name, Scenario scenario) {
    std::mutex mu;
    std::condition_variable cv;
    std::atomic<int> steps{0};
    bool done = false;
    std::exception_ptr error;
    std::thread consumer([&] {
        try {
            scenario(steps);
        } catch (...) {
            error = std::current_exception();
        }
        {
            std::lock_guard<std::mutex> lock(mu);
            done = true;
        }
        cv.notify_one();
    });
    {
        std::unique_lock<std::mutex> lock(mu);
        if (!cv.wait_for(lock, 5s, [&] { return done; })) {
            std::fprintf(stderr, "FAIL %s: deadline after %d completed train steps\n", name, steps.load());
            std::fflush(stderr);
            std::_Exit(1);
        }
    }
    consumer.join();
    if (error) {
        try {
            std::rethrow_exception(error);
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string(name) + ": " + e.what());
        }
    }
    std::printf("PASS %s (%d completed train steps)\n", name, steps.load());
    std::fflush(stdout);
}

void train(const ImagePaths& paths, CacheMode mode, bool holdout, std::atomic<int>& steps) {
    auto manager = make_manager(paths, mode, {0}, holdout ? std::vector<int32_t>{1} : std::vector<int32_t>{});
    for (int i = 0; i < kSteps; ++i) {
        check_step(manager, 0);
        ++steps;
    }
    if (holdout) {
        DecodedBatch batch;
        manager.fetch_one(1, batch);
        check_batch(batch, 1);
    }
}

void expect_decode_fault(DataManager& manager, const std::string& missing) {
    try {
        manager.next_train_step();
    } catch (const DataDecodeError& e) {
        const std::string message = manager.data_error();
        require(message == e.what() && message.find(missing) != std::string::npos,
                "wrong parked decode error");
        return;
    }
    throw std::runtime_error("missing RGB file did not park the worker");
}

void full_queue_teardown(const ImagePaths& paths) {
    int second;
    {
        auto cpu = make_manager(paths, CacheMode::CPU, {0, 1});
        cpu.next_train_step();
        second = cpu.next_train_batch().indices.at(0);
    }
    const std::string displaced = paths.at(second) + ".moved";
    fs::rename(paths.at(second), displaced);
    {
        auto disk = make_manager(paths, CacheMode::DISK, {0, 1});
        // Only one worker: it cannot fault on the second image until it has filled the ready queue.
        std::string message;
        while ((message = disk.data_error()).empty()) std::this_thread::yield();
        require(message.find(paths.at(second)) != std::string::npos, "wrong full-queue fault");
    }
    fs::rename(displaced, paths.at(second));
}

void missing_file(const ImagePaths& paths, bool retry, std::atomic<int>& steps) {
    const std::string displaced = paths[0] + ".moved";
    fs::rename(paths[0], displaced);
    {
        auto disk = make_manager(paths, CacheMode::DISK, {0});
        expect_decode_fault(disk, paths[0]);
        if (retry) fs::rename(displaced, paths[0]);
        disk.resolve_data_error(retry);
        if (retry) {
            check_step(disk, 0);
            ++steps;
        } else {
            bool closed = false;
            try {
                disk.next_train_step();
            } catch (const DataDecodeError&) {
                throw std::runtime_error("aborted decode is still parked");
            } catch (const std::runtime_error&) {
                closed = true;
            }
            require(closed, "abort left a training step available");
        }
    }
    if (!retry) fs::rename(displaced, paths[0]);
}

}  // namespace

int main() {
    fs::path dir;
    try {
        auto path = fs::temp_directory_path() / ("spirula-data-manager-validation-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
        require(fs::create_directory(path), "could not create fixture directory");
        dir = std::move(path);
        const ImagePaths paths{(dir / "train.png").string(), (dir / "holdout.png").string()};
        write_image(paths[0], 0);
        write_image(paths[1], 1);
        run_watched("CPU with holdout", [&](auto& steps) { train(paths, CacheMode::CPU, true, steps); });
        run_watched("DISK without holdout", [&](auto& steps) { train(paths, CacheMode::DISK, false, steps); });
        run_watched("full-queue teardown", [&](auto&) { full_queue_teardown(paths); });
        run_watched("missing-file retry", [&](auto& steps) { missing_file(paths, true, steps); });
        run_watched("missing-file abort", [&](auto& steps) { missing_file(paths, false, steps); });
        run_watched("DISK with holdout", [&](auto& steps) { train(paths, CacheMode::DISK, true, steps); });
        fs::remove_all(dir);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL %s\n", e.what());
        std::error_code ec;
        if (!dir.empty()) fs::remove_all(dir, ec);
        return 1;
    }
    return 0;
}

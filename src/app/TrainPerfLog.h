#pragma once

// One CSV row per second of a training run -- steps, how long they waited for
// data versus ran, sampled GPU time, resolution stage, splats and memory -- for
// tools/perf/ to line up against the machine's own counters. log_performance
// writes it to a new <run>/perf/<date-time> per session, so a resumed run keeps
// the earlier recording; SS_TRAIN_PERF=1 to the run folder, else to its value.

#include "core/Env.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <string>

namespace spirula {

class TrainPerfLog {
public:
    ~TrainPerfLog() { if (_file) std::fclose(_file); }

    // `session_dir` is empty without log_performance.
    void open(const std::filesystem::path& run_dir, const std::filesystem::path& session_dir) {
        const char* v = spirula::env("TRAIN_PERF");
        const bool by_env = v && *v && !(v[0] == '0' && !v[1]);
        if (session_dir.empty() && !by_env) return;
        const std::filesystem::path dir = !session_dir.empty() ? session_dir
            : std::string(v) == "1" ? run_dir : std::filesystem::path(v);
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        _file = std::fopen((dir / "train_perf.csv").string().c_str(), "w");
        if (!_file) return;
        std::fprintf(_file, "unix_ms,step,steps,interval_s,step_s,data_wait_s,gpu_sampled_s,gpu_sampled_steps,"
                            "save_s,divisor,splats,vram_bytes,rss_bytes,paused\n");
        _last = std::chrono::steady_clock::now();
    }

    bool enabled() const { return _file != nullptr; }
    static std::filesystem::path new_session_dir(const std::filesystem::path& run_dir) {
        char stamp[32];
        const std::time_t now = std::time(nullptr);
        std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", std::localtime(&now));
        std::filesystem::path dir = run_dir / "perf" / stamp;
        for (int k = 2; std::filesystem::exists(dir); ++k)
            dir = run_dir / "perf" / (std::string(stamp) + "-" + std::to_string(k));
        return dir;
    }

    // `gpu_s` is negative on steps the GPU timer did not bracket.
    void add_step(double step_s, double data_wait_s, double gpu_s, double save_s) {
        if (!_file) return;
        ++_steps; _step_s += step_s; _wait_s += data_wait_s; _save_s += save_s;
        if (gpu_s >= 0.0) { _gpu_s += gpu_s; ++_gpu_steps; }
    }

    // A row once a second has passed; `vram` and `rss` are asked for only then.
    template <class Vram, class Rss>
    void tick(int step, int divisor, int64_t splats, bool paused, Vram&& vram, Rss&& rss) {
        if (!_file) return;
        const auto now = std::chrono::steady_clock::now();
        const double interval = std::chrono::duration<double>(now - _last).count();
        if (interval < 1.0) return;
        const long long unix_ms = (long long)std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        std::fprintf(_file, "%lld,%d,%lld,%.4f,%.4f,%.4f,%.4f,%lld,%.4f,%d,%lld,%llu,%llu,%d\n",
                     unix_ms, step, _steps, interval, _step_s, _wait_s, _gpu_s, _gpu_steps, _save_s, divisor,
                     (long long)splats, (unsigned long long)vram(), (unsigned long long)rss(),
                     paused ? 1 : 0);
        std::fflush(_file);
        _last = now;
        _steps = _gpu_steps = 0;
        _step_s = _wait_s = _gpu_s = _save_s = 0.0;
    }

private:
    std::FILE* _file = nullptr;
    std::chrono::steady_clock::time_point _last{};
    long long _steps = 0, _gpu_steps = 0;
    double _step_s = 0.0, _wait_s = 0.0, _gpu_s = 0.0, _save_s = 0.0;
};

}  // namespace spirula

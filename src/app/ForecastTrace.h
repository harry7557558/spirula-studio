#pragma once

// ForecastTrace -- everything TrainForecast's VRAM model is fed, as text: the
// setup, then one line per step. SS_FORECAST_DUMP=<file> records a run and
// train_forecast_replay runs the model over it again, so a change to the model
// is checked against real runs without training them (docs/notes/train-forecast.md).

#include "app/TrainForecast.h"

#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace spirula {

inline void forecast_trace_setup(std::FILE* f, const ForecastSetup& s) {
    const DensifyConfig& d = s.schedule.densify();
    std::fprintf(f, "setup %d %" PRId64 " %d %d %d %d %d %d %d %d %d %.9g %" PRIu64 " %d",
                 s.schedule.total_steps(), s.schedule.cap(), s.start_step, s.steps_per_save,
                 s.distinct_batches, s.sh_degree, s.sh_degree_every, d.refine_every,
                 d.refine_start_iter, d.refine_stop_iter, d.refine_stop_num_iter,
                 (double)d.growth_factor, s.deferred_bytes, s.deferred_step);
    std::fprintf(f, " %zu", d.growth_caps.size());
    for (const auto& [first, n] : d.growth_caps) std::fprintf(f, " %d %" PRId64, first, (int64_t)n);
    std::fprintf(f, " %zu", s.resolution_stages.size());
    for (const auto& [first, div] : s.resolution_stages) std::fprintf(f, " %d %d", first, div);
    std::fprintf(f, "\n");
}

inline void forecast_trace_sample(std::FILE* f, const MemorySample& m) {
    std::fprintf(f, "m %d %" PRId64 " %" PRId64, m.step, m.splats_ran, m.splats_next);
    for (size_t v : m.pool_used) std::fprintf(f, " %zu", v);
    for (size_t v : m.pool_cap) std::fprintf(f, " %zu", v);
    std::fprintf(f, " %zu %d %d %d %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %zu %zu",
                 m.scratch, (int)m.has_process, (int)m.has_used, (int)m.has_total,
                 m.process_bytes, m.used_bytes, m.total_bytes, m.process_peak_bytes, m.sh_bytes,
                 m.arena_cap);
    for (size_t v : m.phase_pixels) std::fprintf(f, " %zu", v);
    for (size_t v : m.phase_splats) std::fprintf(f, " %zu", v);
    std::fprintf(f, " %" PRId64 " %" PRId64 " %" PRId64 "\n", m.isect_pairs, m.isect_count,
                 m.isect_step);
}

struct ForecastTrace {
    ForecastSetup setup;
    std::vector<MemorySample> samples;
};

// A line's numbers in order; a field the line ends before reads as 0.
struct ForecastTraceLine {
    const char* at;
    uint64_t next() {
        char* end = nullptr;
        const uint64_t v = std::strtoull(at, &end, 10);
        if (end == at) return 0;
        at = end;
        return v;
    }
    double real() {
        char* end = nullptr;
        const double v = std::strtod(at, &end);
        if (end != at) at = end;
        return v;
    }
};

inline bool forecast_trace_read(const std::string& path, ForecastTrace& out) {
    std::FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return false;
    out = ForecastTrace{};
    bool have_setup = false;
    std::vector<char> buf(1 << 16);
    while (std::fgets(buf.data(), (int)buf.size(), f)) {
        if (!std::strncmp(buf.data(), "setup ", 6)) {
            ForecastTraceLine l{buf.data() + 6};
            ForecastSetup& s = out.setup;
            DensifyConfig d;
            const int total = (int)l.next();
            const int64_t cap = (int64_t)l.next();
            s.start_step = (int)l.next();
            s.steps_per_save = (int)std::strtol(l.at, const_cast<char**>(&l.at), 10);
            s.distinct_batches = (int)l.next();
            s.sh_degree = (int)l.next();
            s.sh_degree_every = (int)l.next();
            d.refine_every = (int)l.next();
            d.refine_start_iter = (int)l.next();
            d.refine_stop_iter = (int)l.next();
            d.refine_stop_num_iter = (int)std::strtol(l.at, const_cast<char**>(&l.at), 10);
            d.growth_factor = (float)l.real();
            s.deferred_bytes = l.next();
            s.deferred_step = (int)l.next();
            for (size_t n = (size_t)l.next(); n > 0; --n) {
                const int first = (int)l.next();
                d.growth_caps.push_back({first, (int64_t)l.next()});
            }
            for (size_t n = (size_t)l.next(); n > 0; --n) {
                const int first = (int)l.next();
                s.resolution_stages.push_back({first, (int)l.next()});
            }
            s.schedule = SplatSchedule(d, total, cap);
            have_setup = true;
        } else if (!std::strncmp(buf.data(), "m ", 2)) {
            ForecastTraceLine l{buf.data() + 2};
            MemorySample m;
            m.step = (int)l.next();
            m.splats_ran = (int64_t)l.next();
            m.splats_next = (int64_t)l.next();
            for (size_t& v : m.pool_used) v = (size_t)l.next();
            for (size_t& v : m.pool_cap) v = (size_t)l.next();
            m.scratch = (size_t)l.next();
            m.has_process = l.next() != 0;
            m.has_used = l.next() != 0;
            m.has_total = l.next() != 0;
            m.process_bytes = l.next();
            m.used_bytes = l.next();
            m.total_bytes = l.next();
            m.process_peak_bytes = l.next();
            m.sh_bytes = (size_t)l.next();
            m.arena_cap = (size_t)l.next();
            for (size_t& v : m.phase_pixels) v = (size_t)l.next();
            for (size_t& v : m.phase_splats) v = (size_t)l.next();
            m.isect_pairs = (int64_t)l.next();
            m.isect_count = (int64_t)l.next();
            m.isect_step = (int64_t)l.next();
            out.samples.push_back(m);
        }
    }
    std::fclose(f);
    return have_setup && !out.samples.empty();
}

}  // namespace spirula

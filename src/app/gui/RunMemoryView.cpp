// RunMemoryView.cpp -- see RunMemoryView.h.

#include "app/gui/RunMemoryView.h"

#include "app/gui/ProgressClock.h"
#include "app/gui/ResourceForecastView.h"
#include "backend/api/BackendRuntime.h"
#include "core/HostMemory.h"

#include <algorithm>
#include <cmath>

namespace gui {

using spirula::PlannedPeak;
using spirula::ResourceForecast;
using spirula::RunReport;

namespace {
constexpr size_t kMaxSamples = 2048;
}  // namespace

void RunMemoryView::reset() { *this = RunMemoryView{}; }

bool RunMemoryView::due() {
    const double now = progress_now();
    if (_polled_at >= 0.0 && now - _polled_at < 0.5) return false;
    _polled_at = now;
    return true;
}

void RunMemoryView::poll_report(const std::string& dir, double work) {
    if (!due()) return;
    RunReport r;
    if (!spirula::read_run_report(dir, r) || r.sequence == _sequence) return;
    if (r.sequence < _sequence) {   // a new run reuses the file
        const double polled = _polled_at;
        reset();
        _polled_at = polled;
    }
    _sequence = r.sequence;
    _reported_work = r.work;
    add(r, work >= 0.0 ? work : r.work);
}

void RunMemoryView::poll_self(double work,
                              const std::function<bool(spirula::RunReporter::Device&)>& device) {
    if (!due()) return;
    RunReport r;
    r.host_process = spirula::processRamBytes();
    r.host_available = spirula::availableRamBytes();
    r.host_total = spirula::physicalRamBytes();
    r.host_commit_free = spirula::availableCommitBytes();
    spirula::RunReporter::Device d;
    const backend::MemoryUsage m = backend::memory_usage();
    if (device && device(d) && d.capacity > 0) {
        r.device_known = true;
        r.device_capacity = d.capacity;
        r.device_ours = d.ours;
        r.device_others = d.others;
    } else if (m.has_total && m.has_used && m.total_bytes > 0) {
        if (_device_base < 0.0) _device_base = (double)m.used_bytes;
        r.device_known = true;
        r.device_capacity = m.total_bytes;
        r.device_others = (uint64_t)_device_base;
        r.device_ours = (uint64_t)std::max(0.0, (double)m.used_bytes - _device_base);
    }
    add(r, work);
}

void RunMemoryView::add(const RunReport& r, double work) {
    const double now = progress_now();
    if (_started_at < 0.0) _started_at = now;
    const double t = now - _started_at;
    const double commit = (double)r.host_commit_free;

    const double ours = (double)r.host_process, avail = (double)r.host_available;
    const double total = (double)r.host_total;
    double limit = ours + avail;
    const bool host_commit = commit > 0.0 && ours + commit < limit;
    if (host_commit) limit = ours + commit;
    update(_ram, _ram_track, t, ours, std::max(0.0, total - ours - avail), total, limit,
           host_commit, r.host_peak, work);

    if (r.device_known && r.device_capacity > 0) {
        const double d_ours = (double)r.device_ours, d_others = (double)r.device_others;
        // Allocator fragmentation keeps the last percent out of reach.
        double d_limit = 0.99 * (double)r.device_capacity - d_others;
        bool d_commit = false;
#ifdef _WIN32
        // WDDM charges device memory to the commit limit as well (docs/notes/train-forecast.md).
        d_commit = commit > 0.0 && d_ours + commit < d_limit;
        if (d_commit) d_limit = d_ours + commit;
#endif
        update(_vram, _vram_track, t, d_ours, d_others, (double)r.device_capacity, d_limit,
               d_commit, r.device_peak, work);
    }
}

void RunMemoryView::update(ResourceForecast& f, Track& k, double t, double ours, double others,
                           double capacity, double limit, bool commit_limited,
                           const PlannedPeak& plan, double work) {
    f.known = true;
    f.capacity = capacity;
    f.ours = ours;
    f.others = others;
    f.limit = limit;
    f.commit_limited = commit_limited;
    const double a = k.others_n == 0 ? 1.0 : 0.1;
    k.others_mean += a * (others - k.others_mean);
    k.others_sq += a * (others * others - k.others_sq);
    ++k.others_n;
    f.others_sigma = std::max(std::sqrt(std::max(0.0, k.others_sq - k.others_mean * k.others_mean)),
                              0.01 * capacity);
    k.hw = std::max(k.hw, ours);

    if (f.history.size() >= kMaxSamples) {   // halve the resolution rather than drop the start
        for (size_t i = 0; i < f.history.size() / 2; ++i) f.history[i] = f.history[i * 2];
        f.history.resize(f.history.size() / 2);
    }
    f.history.push_back({t, ours, others});

    if (plan.mean > 0.0 && plan.ceiling) {
        // Anywhere from what has been seen to the bound: the band spans both.
        const double top = std::max(plan.mean, k.hw);
        f.projected = true;
        f.peak_mean = 0.5 * (k.hw + top);
        f.peak_sigma = std::sqrt(0.25 * (top - k.hw) * (top - k.hw) + plan.sigma * plan.sigma);
    } else if (plan.mean > 0.0) {
        f.projected = true;
        f.peak_mean = std::max(plan.mean, k.hw);
        f.peak_sigma = plan.sigma;
    } else if (work >= 0.0) {
        k.growth.add(work, ours);
        k.growth.peak(f.peak_mean, f.peak_sigma);
        f.projected = true;
    } else {
        f.projected = false;
        f.peak_mean = k.hw;
        f.peak_sigma = 0.0;
    }
    spirula::assess_oom(f);
}

void RunMemoryView::draw(const spirula::i18n::Msg& help, float x0, float avail) const {
    memory_strip(_ram, _vram, help, x0, avail);
}

}  // namespace gui

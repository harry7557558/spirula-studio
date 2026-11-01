#pragma once

// A job's RAM and VRAM, measured twice a second and forecast to its peak, for
// the strip above a screen's log. A child reports itself (app/RunReport.h); a
// job this process runs is measured here. Its peak is the one the job planned
// when it has one, and otherwise extrapolated over the job's progress.

#include "app/ResourceForecast.h"
#include "app/RunReport.h"
#include "i18n/Message.h"

#include <cstdint>
#include <functional>
#include <string>

namespace gui {

class RunMemoryView {
public:
    // Forget the previous job; the next poll starts a new history.
    void reset();
    // `work` 0..1 is what an unplanned peak is extrapolated over; < 0 uses the report's own.
    void poll_report(const std::string& dir, double work = -1.0);
    // This process, for a job it runs itself. `device` reports the job's own
    // device memory; without it, what the device held when the job started
    // counts as others and the growth since as the job's.
    void poll_self(double work, const std::function<bool(spirula::RunReporter::Device&)>& device = {});

    bool visible() const { return _ram.known; }
    // The last report's own progress, -1 when it gave none.
    double reported_work() const { return _reported_work; }
    const spirula::ResourceForecast& ram() const { return _ram; }
    const spirula::ResourceForecast& vram() const { return _vram; }
    // One right-aligned row inside the strip that starts at `x0` and is `avail` wide.
    void draw(const spirula::i18n::Msg& help, float x0, float avail) const;

private:
    struct Track {
        spirula::GrowthForecast growth;
        double hw = 0.0;
        double others_mean = 0.0, others_sq = 0.0;
        int others_n = 0;
    };
    bool due();
    void add(const spirula::RunReport& r, double work);
    void update(spirula::ResourceForecast& f, Track& k, double t, double ours, double others,
                double capacity, double limit, bool commit_limited,
                const spirula::PlannedPeak& plan, double work);

    spirula::ResourceForecast _ram, _vram;
    Track _ram_track, _vram_track;
    double _polled_at = -1.0, _started_at = -1.0;
    uint64_t _sequence = 0;
    double _reported_work = -1.0;
    double _device_base = -1.0;
};

}  // namespace gui

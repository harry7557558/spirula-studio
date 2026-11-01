// PrepProgress.cpp -- see PrepProgress.h.

#include "app/gui/PrepProgress.h"

#include <algorithm>

namespace gui {

void RunProgress::reset() {
    std::lock_guard<std::mutex> lk(_mu);
    for (StageProgress& s : _st) s = StageProgress{};
    _cur = Stage::Frames;
    _pending.clear();
    _scan.clear();
}

void RunProgress::scan_reset(std::vector<ScanRow> rows) {
    std::lock_guard<std::mutex> lk(_mu);
    for (ScanRow& r : rows) {
        r.speed.assign(kScanSlices, 0.0f);
        r.hits.assign(kScanSlices, 0);
    }
    _scan = std::move(rows);
}

void RunProgress::scan_open(size_t row) {
    std::lock_guard<std::mutex> lk(_mu);
    if (row < _scan.size()) _scan[row].started = true;
}

void RunProgress::scan_step(size_t row, int64_t at, int64_t of, float cost) {
    std::lock_guard<std::mutex> lk(_mu);
    if (row >= _scan.size()) return;
    ScanRow& r = _scan[row];
    if (r.frames <= 0) r.frames = of;
    if (r.frames <= 0 || r.speed.empty()) return;
    r.started = true;
    const int64_t slice = std::max<int64_t>(0, at) * kScanSlices / r.frames;
    const size_t i = (size_t)std::min<int64_t>(kScanSlices - 1, slice);
    // A running mean rather than a sum: the slices a stride lands in unevenly
    // would otherwise read as motion the capture does not have.
    r.speed[i] += (std::max(0.0f, cost) - r.speed[i]) / (float)(++r.hits[i]);
    r.done = (float)std::min(1.0, (double)at / (double)r.frames);
}

void RunProgress::scan_kept(size_t row, std::vector<float> bars, int64_t kept) {
    std::lock_guard<std::mutex> lk(_mu);
    if (row >= _scan.size()) return;
    _scan[row].kept = std::move(bars);
    _scan[row].kept_n = kept;
    _scan[row].started = true;
    _scan[row].done = 1.0f;
}

std::vector<ScanRow> RunProgress::scan() const {
    std::lock_guard<std::mutex> lk(_mu);
    return _scan;
}

bool RunProgress::scanning() const {
    std::lock_guard<std::mutex> lk(_mu);
    for (const ScanRow& r : _scan)
        if (r.started && r.done < 1.0f) return true;
    return false;
}

void RunProgress::enter(Stage s, const std::string& detail) {
    std::lock_guard<std::mutex> lk(_mu);
    const double now = progress_now();
    for (int i = 0; i < (int)s; i++)
        if (_st[i].status == StageStatus::Pending) {
            _st[i].status = StageStatus::Skipped;
        } else if (_st[i].status == StageStatus::Running) {
            _st[i].status = StageStatus::Done;
            _st[i].clock.finish(now);
        }
    StageProgress& p = _st[(int)s];
    if (p.status != StageStatus::Running || p.detail != detail) {
        p.clock.start(now);
        p.fraction = -1.0f;
        p.done = p.total = 0;
    }
    p.status = StageStatus::Running;
    p.detail = detail;
    _cur = s;
}

void RunProgress::restart(Stage s) {
    std::lock_guard<std::mutex> lk(_mu);
    StageProgress& p = _st[(int)s];
    p.clock.start(progress_now());
    p.fraction = -1.0f;
    p.done = p.total = 0;
}

void RunProgress::count(Stage s, int64_t done, int64_t total) {
    std::lock_guard<std::mutex> lk(_mu);
    StageProgress& p = _st[(int)s];
    p.done = done;
    p.total = total;
    p.fraction = total > 0 ? (float)((double)done / (double)total) : -1.0f;
    if (p.fraction >= 0.0f) p.clock.work(progress_now(), p.fraction);
}

void RunProgress::fraction(Stage s, float f) {
    std::lock_guard<std::mutex> lk(_mu);
    StageProgress& p = _st[(int)s];
    p.fraction = f;
    if (f >= 0.0f) p.clock.work(progress_now(), f);
}

void RunProgress::detail(Stage s, const std::string& text) {
    std::lock_guard<std::mutex> lk(_mu);
    _st[(int)s].detail = text;
}

void RunProgress::mark(Stage s, StageStatus st) {
    std::lock_guard<std::mutex> lk(_mu);
    _st[(int)s].status = st;
    if (st == StageStatus::Done) {
        _st[(int)s].fraction = 1.0f;
        _st[(int)s].clock.finish(progress_now());
    }
}

void RunProgress::finish(StageStatus st) {
    std::lock_guard<std::mutex> lk(_mu);
    const double now = progress_now();
    for (StageProgress& p : _st)
        if (p.status == StageStatus::Running) {
            p.status = st;
            if (st == StageStatus::Done) p.clock.finish(now);
        }
}

void RunProgress::note(const std::string& text, bool detail) {
    std::lock_guard<std::mutex> lk(_mu);
    _pending.push_back({_cur, detail, text});
}

void RunProgress::note(Stage s, const std::string& text, bool detail) {
    std::lock_guard<std::mutex> lk(_mu);
    _pending.push_back({s, detail, text});
}

std::vector<RunLine> RunProgress::drain() {
    std::lock_guard<std::mutex> lk(_mu);
    std::vector<RunLine> out;
    out.swap(_pending);
    return out;
}

StageProgress RunProgress::stage(Stage s) const {
    std::lock_guard<std::mutex> lk(_mu);
    return _st[(int)s];
}

Stage RunProgress::current() const {
    std::lock_guard<std::mutex> lk(_mu);
    return _cur;
}

bool RunProgress::ran(Stage s) const {
    std::lock_guard<std::mutex> lk(_mu);
    const StageStatus st = _st[(int)s].status;
    return st != StageStatus::Pending;
}

}  // namespace gui

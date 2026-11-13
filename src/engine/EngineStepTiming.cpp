// GPU time of a step's splat-proportional stages, for the trainer's ETA model,
// and every kernel of a step classified for the step breakdown (StepTiming.h).

#include "engine/Engine.h"
#include "engine/EngineInternal.h"
#include "engine/StepTiming.h"
#include "engine/StepTimingTable.h"

#include "backend/api/BackendRuntime.h"

#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

struct StageTiming {
    bool armed = false;
    // Reused across steps; one pair per bracketed stage of the armed step.
    std::vector<std::pair<backend::Event*, backend::Event*>> pairs;
    size_t used = 0;
};

StageTiming& timing() {
    static StageTiming t;
    return t;
}

}  // namespace

SplatStageTimer::SplatStageTimer() {
    StageTiming& t = timing();
    if (!t.armed) return;
    if (t.used == t.pairs.size())
        t.pairs.emplace_back(backend::event_create(true), backend::event_create(true));
    _pair = (int)t.used++;
    backend::event_record(t.pairs[_pair].first);
}

SplatStageTimer::~SplatStageTimer() {
    if (_pair < 0) return;
    backend::event_record(timing().pairs[_pair].second);
}

void engine_step_timing_arm() {
    StageTiming& t = timing();
    t.armed = true;
    t.used = 0;
}

double engine_step_timing_read() {
    StageTiming& t = timing();
    if (!t.armed) return -1.0;
    t.armed = false;
    if (t.used == 0) return -1.0;
    backend::event_synchronize(t.pairs[t.used - 1].second);
    double ms = 0.0;
    for (size_t i = 0; i < t.used; ++i)
        ms += backend::event_elapsed_ms(t.pairs[i].first, t.pairs[i].second);
    t.used = 0;
    // 0 means the device has no timestamps, not that the stages were free.
    return ms > 0.0 ? ms * 1e-3 : -1.0;
}

// ================
// Per-kernel step breakdown
// ================

namespace {

constexpr uint16_t kTagForce = 0x100;

uint16_t encode_tag(StepPart part, StepScale scale, bool force) {
    return (uint16_t)(((int)part + 1) | (((int)scale + 1) << 5) | (force ? kTagForce : 0));
}

struct KernelRow {
    const char* entry;
    StepPart part;
    StepScale scale;
};

constexpr KernelRow kKernelRows[] = {
#define SS_KERNEL_ROW(entry, part, scale) {entry, StepPart::part, StepScale::scale},
    SS_KERNEL_TIMING_TABLE(SS_KERNEL_ROW)
#undef SS_KERNEL_ROW
};

const std::unordered_map<std::string_view, const KernelRow*>& kernel_rows() {
    static const auto rows = [] {
        std::unordered_map<std::string_view, const KernelRow*> m;
        for (const KernelRow& r : kKernelRows) m.emplace(r.entry, &r);
        return m;
    }();
    return rows;
}

}  // namespace

StepTimingScope::StepTimingScope(StepPart part, StepScale scale, bool force)
    : _saved(backend::dispatch_timing_tag()) {
    if (_saved & kTagForce) return;
    backend::dispatch_timing_set_tag(encode_tag(part, scale, force));
}

StepTimingScope::~StepTimingScope() { backend::dispatch_timing_set_tag(_saved); }

bool step_kernel_class(std::string_view entry, StepPart& part, StepScale& scale) {
    const auto& rows = kernel_rows();
    const auto it = rows.find(entry);
    if (it == rows.end()) return false;
    part = it->second->part;
    scale = it->second->scale;
    return true;
}

const char* step_part_name(StepPart p) {
    static const char* const names[] = {
        "render", "backward", "loss", "optimizer", "densify", "appearance",
        "calibration", "geometry", "masks", "warp", "color_space", "background",
        "regularizer", "viewer", "other", "unclassified", "loading", "saving"};
    static_assert(sizeof names / sizeof names[0] == kStepParts, "one name per part");
    return (int)p < kStepParts ? names[(int)p] : "?";
}

const char* step_scale_name(StepScale c) {
    static const char* const names[] = {"splats", "splats_pixels", "pixels", "images", "fixed"};
    static_assert(sizeof names / sizeof names[0] == kStepScales, "one name per scale");
    return (int)c < kStepScales ? names[(int)c] : "?";
}

int step_kernel_table_size() { return (int)(sizeof kKernelRows / sizeof kKernelRows[0]); }
const char* step_kernel_table_entry(int i) { return kKernelRows[i].entry; }

void engine_dispatch_timing_arm() { backend::dispatch_timing_arm(); }

bool engine_dispatch_timing_read(StepCells& out) {
    static thread_local std::vector<backend::DispatchTime> times;
    times.clear();
    if (!backend::dispatch_timing_collect(times)) return false;
    for (const backend::DispatchTime& d : times) {
        StepPart part = StepPart::Unclassified;
        StepScale scale = StepScale::Scope;
        step_kernel_class(d.entry, part, scale);
        const int tag_part = (d.tag & 0x1f) - 1;
        const int tag_scale = ((d.tag >> 5) & 0x7) - 1;
        // Training records on the thread that armed; any other one without a
        // forced scope is a GUI or web view rendering between steps.
        if (d.tag & kTagForce)
            part = (StepPart)tag_part;
        else if (d.foreign)
            part = StepPart::Viewer;
        else if (part == StepPart::Scope)
            part = tag_part >= 0 ? (StepPart)tag_part : StepPart::Other;
        if (scale == StepScale::Scope)
            scale = tag_scale >= 0 ? (StepScale)tag_scale : StepScale::Fixed;
        out.at(part, scale) += d.ms * 1e-3;
    }
    return true;
}

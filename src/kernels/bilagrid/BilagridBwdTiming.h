#pragma once
// Bilateral-grid backward timings, harvested one iteration later to avoid stalls.
// See BilagridBackwardSelection.md for the selector design.

#include "kernels/bilagrid/BilagridBwdSelector.h"          // ContextKey
#include "backend/api/BackendRuntime.h"   // backend::Event, Stream, event_*

#include <string>
#include <vector>

namespace spirula {
namespace bilagrid {

struct BwdMeasurement {
    ContextKey key;
    int arm;      // arm index that was dispatched (-1 = pre-selector baseline)
    double ms;    // measured GPU time
};

// Timed channels of the backward hook. RGB carries one of affine/ppisp/
// loglinear (distinguished by ContextKey.family); depth/normal are separate.
enum BwdChannel {
    BWD_RGB = 0,
    BWD_DEPTH = 1,
    BWD_NORMAL = 2,
    BWD_NUM_CHANNELS = 3,
};

inline int family_id(const std::string& type) {
    if (type == "affine") return 0;
    if (type == "ppisp") return 1;
    if (type == "loglinear") return 2;
    if (type == "depth") return 3;
    if (type == "normal") return 4;
    return -1;
}

inline const char* family_name(int id) {
    switch (id) {
        case 0: return "affine";
        case 1: return "ppisp";
        case 2: return "loglinear";
        case 3: return "depth";
        case 4: return "normal";
        default: return "?";
    }
}

// Fixed-slot async timer (one slot per BwdChannel). Timing events are created
// once and reused across iterations (on Vulkan each holds one query-pool slot).
class BwdTimingRing {
public:
    explicit BwdTimingRing(int num_slots = BWD_NUM_CHANNELS)
        : slots_(num_slots) {}

    ~BwdTimingRing() {
        for (Slot& s : slots_) {
            if (s.start) backend::event_destroy(s.start);
            if (s.end) backend::event_destroy(s.end);
        }
    }

    // Harvest before begin() so pending events are at least one iteration old.
    void harvest(std::vector<BwdMeasurement>& out) {
        for (Slot& s : slots_) {
            if (!s.pending) continue;
            backend::event_synchronize(s.end);
            double ms = (double)backend::event_elapsed_ms(s.start, s.end);
            if (ms > 0.0) out.push_back({s.key, s.arm, ms});
            s.pending = false;
        }
    }

    // Record the start event for `slot` (call immediately before the dispatch).
    void begin(int slot, const ContextKey& key, int arm,
               backend::Stream stream) {
        Slot& s = slots_[slot];
        if (!s.start) s.start = backend::event_create(/*enable_timing=*/true);
        if (!s.end) s.end = backend::event_create(/*enable_timing=*/true);
        s.key = key;
        s.arm = arm;
        backend::event_record(s.start, stream);
    }

    // Record the end event for `slot` (call immediately after the dispatch).
    void end(int slot, backend::Stream stream) {
        Slot& s = slots_[slot];
        backend::event_record(s.end, stream);
        s.pending = true;
    }

    int num_slots() const { return (int)slots_.size(); }

private:
    struct Slot {
        backend::Event* start = nullptr;
        backend::Event* end = nullptr;
        ContextKey key;
        int arm = -1;
        bool pending = false;
    };
    std::vector<Slot> slots_;
};

}  // namespace bilagrid
}  // namespace spirula

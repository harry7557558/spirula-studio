// GPU time of a step's splat-proportional stages, for the trainer's ETA model.

#include "engine/Engine.h"
#include "engine/EngineInternal.h"

#include "backend/api/BackendRuntime.h"

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

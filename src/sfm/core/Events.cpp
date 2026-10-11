// Events.cpp -- see Events.h.

#include "sfm/core/Events.h"

#include <mutex>
#include <vector>

namespace sfm {
namespace events {

namespace {
std::mutex g_mu;
Sink g_sink;

// The mapping bar's own state, behind its own lock: map_placed is called from
// the atom workers while a sink runs on whichever thread emitted.
std::mutex g_map_mu;
std::vector<char> g_placed;
std::vector<uint32_t> g_map_ids;
int64_t g_placed_n = 0;
}  // namespace

void set_sink(Sink s) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_sink = std::move(s);
}

bool armed() {
    std::lock_guard<std::mutex> lk(g_mu);
    return (bool)g_sink;
}

// Verification calls this from its worker pool, so the lock is what orders
// the stream; a sink must not call back into sfm::events.
void emit(const Event& e) {
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_sink) g_sink(e);
}

void stage_begin(Stage s, int64_t total) {
    Event e;
    e.kind = Event::Kind::StageBegin;
    e.stage = s;
    e.total = total;
    emit(e);
}

void stage_end(Stage s) {
    Event e;
    e.kind = Event::Kind::StageEnd;
    e.stage = s;
    emit(e);
}

void progress(Stage s, int64_t done, int64_t total) {
    Event e;
    e.kind = Event::Kind::Progress;
    e.stage = s;
    e.done = done;
    e.total = total;
    emit(e);
}

void map_begin(size_t n_images) {
    std::lock_guard<std::mutex> lk(g_map_mu);
    g_placed.assign(n_images, 0);
    g_map_ids.clear();
    g_placed_n = 0;
}

void set_map_image_ids(const std::vector<uint32_t>& ids) {
    std::lock_guard<std::mutex> lk(g_map_mu);
    g_map_ids = ids;
}

void map_placed(uint32_t image) {
    int64_t done = 0, total = 0;
    {
        std::lock_guard<std::mutex> lk(g_map_mu);
        if (!g_map_ids.empty()) {
            if (image >= g_map_ids.size()) return;
            image = g_map_ids[image];
        }
        if (image >= g_placed.size() || g_placed[image]) return;
        g_placed[image] = 1;
        done = ++g_placed_n;
        total = (int64_t)g_placed.size();
    }
    // Once per image over the whole stage, so there is nothing to rate-limit.
    progress(Stage::Map, done, total);
}

}  // namespace events
}  // namespace sfm

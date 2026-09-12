#pragma once

// What a run is doing, as data rather than as text.
//
// The in-process twin of core/Progress.h: that writes snapshots a child's
// front end can poll, this hands the same facts straight to a front end in
// the same process. Both are off by default and cost nothing unarmed.
//
// A front end must not need to read the log to know where a run is. Anything
// a screen keys on -- the stage, the fraction through it, the outcome --
// belongs here; prose belongs in core/Log.h.

#include <cstdint>
#include <functional>
#include <string>

namespace sfm {

// The stages a front end shows: the CLI's subcommands, plus the phases inside
// them that move no bar and so read as a hang unless a screen names them. The
// numbering reaches a front end through status.bin -- append, never slot in.
enum class Stage { Extract, Match, Map, Merge, Orient, Finish, Load, Select, Seed, Refine };
inline constexpr int kNumStages = 10;

struct Event {
    enum class Kind {
        StageBegin,
        StageEnd,
        Progress,        // `done` of `total` within the stage
        ImageExtracted,
        PairVerified,
        ModelUpdated,
        Result,          // once, at the end of a run
    };

    Kind  kind = Kind::Progress;
    Stage stage = Stage::Extract;

    int64_t done = 0, total = 0;

    // ImageExtracted: the image's path under the image directory, and what the
    // detector found on it.
    std::string name;
    int     width = 0, height = 0;
    int64_t features = 0;
    int64_t masked = 0;      // keypoints the image's mask removed

    // PairVerified: `inliers` of 0 means it did not survive verification.
    uint32_t image_a = 0, image_b = 0, inliers = 0;

    // ModelUpdated and Result.
    int64_t registered = 0, images = 0, points = 0, models = 0;
    double  mean_reproj = 0.0;
    // Result only: the model trains, but with the gaps the CLI's exit codes 3
    // and 4 report.
    bool partial = false;
    bool metric = true;
};

namespace events {

// Process-global, like the log sink: one SfM job per process. Unset (the
// default) makes every emit() below a predictable branch and nothing else.
using Sink = std::function<void(const Event&)>;
void set_sink(Sink s);
bool armed();

void emit(const Event& e);

// The shapes every caller uses, so a stage does not build a struct by hand.
void stage_begin(Stage s, int64_t total = 0);
void stage_end(Stage s);
void progress(Stage s, int64_t done, int64_t total);

// How far mapping is, over the capture rather than over one model: a seed
// retry resets the model and an atom numbers from zero, so a bar taken from
// either runs forward and falls back. Idempotent, and safe from the workers.
void map_begin(size_t n_images);
void map_placed(uint32_t image);

}  // namespace events
}  // namespace sfm

#pragma once

// What a dataset run is doing, in the shape the screen draws it.
//
// One object per runner, written by its worker thread and read by the UI
// thread. It replaces a stage string, a progress float and an unclassified
// stream of log lines with the three things the screen actually asks: which
// step is running, how far through it is, and which lines a user who is not
// debugging should be shown.
//
// SfmRunner fills it by parsing the child's stdout; when the SfM module
// becomes a library (docs/notes/sfm-port-plan.md phase 3) only that parser goes.

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace gui {

// The steps a dataset run goes through, in order. Both engines report through
// the same list; a step a run does not need is never entered.
enum class Stage {
    Frames, Masks, Features, Matching, Mapping, Align, Geometry, Finishing
};
inline constexpr int kNumStages = 8;

enum class StageStatus { Pending, Running, Done, Skipped, Failed };

struct StageProgress {
    StageStatus status = StageStatus::Pending;
    float   fraction = -1.0f;    // 0..1, or -1 when the step cannot estimate one
    int64_t done = 0, total = 0;
    std::string detail;          // already formatted; the line under the bar
};

// How much the view changed along one input, for the panel that watches the
// adaptive pass. `speed` is the mean over each slice of the capture and `kept`
// the rate the plan settled on; a folder of photographs has neither.
struct ScanRow {
    std::string name;
    bool video = false;
    int64_t frames = 0;         // source frames, or photographs
    bool started = false;       // false for a row this run will not measure
    float done = 0.0f;          // 0..1 through the measuring pass
    std::vector<float> speed;
    std::vector<int32_t> hits;  // steps that landed in each slice, for the mean
    std::vector<float> kept;    // empty until the plan is made
    int64_t kept_n = 0;         // frames the plan keeps
};

// Enough that a burst of motion is a spike rather than a wide block, and few
// enough that a plan of forty frames still fills some of them.
inline constexpr int kScanSlices = 192;

// A line of a run's output. `detail` is the stream a developer reads; the rest
// is the handful worth showing without asking for it.
struct RunLine {
    Stage stage = Stage::Frames;
    bool  detail = true;
    std::string text;
};

class RunProgress {
public:
    void reset();

    // Enter a step. Everything before it still Pending becomes Skipped, so a
    // run that masks nothing does not leave "Masks" looking like it is next.
    void enter(Stage s, const std::string& detail);
    void count(Stage s, int64_t done, int64_t total);
    void fraction(Stage s, float f);
    void detail(Stage s, const std::string& text);
    void mark(Stage s, StageStatus st);
    // The run ended: whatever was running takes `st`.
    void finish(StageStatus st);

    void note(const std::string& text, bool detail);
    void note(Stage s, const std::string& text, bool detail);

    // The adaptive pass as the panel watching it draws (ScanRow): the inputs
    // it will cover, one measured step folded in, and the spacing a row ended
    // up with. `scanning` is what says the panel is the one worth showing.
    void scan_reset(std::vector<ScanRow> rows);
    void scan_open(size_t row);
    void scan_step(size_t row, int64_t at, int64_t of, float cost);
    void scan_kept(size_t row, std::vector<float> bars, int64_t kept);
    std::vector<ScanRow> scan() const;
    bool scanning() const;

    std::vector<RunLine> drain();
    StageProgress stage(Stage s) const;
    Stage current() const;
    // The step a job would enter next if it started now -- what decides which
    // half of the form is still editable.
    bool ran(Stage s) const;

private:
    mutable std::mutex _mu;
    StageProgress _st[kNumStages];
    Stage _cur = Stage::Frames;
    std::vector<RunLine> _pending;
    std::vector<ScanRow> _scan;
};

}  // namespace gui

#pragma once

// Where a training step's time goes: what each kernel is for (StepPart) and
// what its cost grows with (StepScale). The per-kernel rows are
// StepTimingTable.h; the model and its measurements are
// docs/notes/step-timing.md.

#include <cstdint>
#include <string_view>

enum class StepPart : uint8_t {
    Render, Backward, Loss, Optimizer, Densify,
    Appearance, Calibration, Geometry, Masks, Warp, ColorSpace, Background, Regularizer,
    Viewer, Other, Unclassified,
    Loading, Saving,   // host spans: waiting for images, writing checkpoints
    Count,
    Scope = Count,     // table only: the enclosing StepTimingScope decides
};

enum class StepScale : uint8_t {
    Splats, SplatsPixels, Pixels, Images, Fixed,
    Count,
    Scope = Count,
};

constexpr int kStepParts = (int)StepPart::Count;
constexpr int kStepScales = (int)StepScale::Count;

// Seconds by [part][scale].
struct StepCells {
    double s[kStepParts][kStepScales] = {};

    double& at(StepPart p, StepScale c) { return s[(int)p][(int)c]; }
    double at(StepPart p, StepScale c) const { return s[(int)p][(int)c]; }
    double part(StepPart p) const {
        double t = 0.0;
        for (double v : s[(int)p]) t += v;
        return t;
    }
    double scale(StepScale c) const {
        double t = 0.0;
        for (const auto& row : s) t += row[(int)c];
        return t;
    }
    double total() const {
        double t = 0.0;
        for (const auto& row : s)
            for (double v : row) t += v;
        return t;
    }
    StepCells& add(const StepCells& o, double k = 1.0) {
        for (int p = 0; p < kStepParts; ++p)
            for (int c = 0; c < kStepScales; ++c) s[p][c] += k * o.s[p][c];
        return *this;
    }
};

// Kernels this thread records inside the scope whose row says `Scope` take
// `part` / `scale`. With `force` every kernel's part is `part` (the viewer);
// a forced scope also outlives the scopes opened inside it.
class StepTimingScope {
public:
    StepTimingScope(StepPart part, StepScale scale, bool force = false);
    ~StepTimingScope();
    StepTimingScope(const StepTimingScope&) = delete;
    StepTimingScope& operator=(const StepTimingScope&) = delete;

private:
    uint16_t _saved;
};

// English identifiers, for logs and tests.
const char* step_part_name(StepPart p);
const char* step_scale_name(StepScale c);

// The table's row for `entry`; false when it has none.
bool step_kernel_class(std::string_view entry, StepPart& part, StepScale& scale);

// Every StepTimingTable.h row, for the coverage test.
int step_kernel_table_size();
const char* step_kernel_table_entry(int i);

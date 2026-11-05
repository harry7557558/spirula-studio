#pragma once

// Where a progress bar stands and how long the job has left, from what the job
// says about itself. The bar is a share of the TIME the job will take, not of
// its steps, and it never goes back. The model and its rules:
// docs/notes/progress-bars.md.

#include <cstddef>
#include <vector>

namespace gui {

// Steady-clock seconds, the time base every clock is fed in.
double progress_now();

class ProgressClock {
public:
    void start(double now);
    // `work` is the job's own measure of how far it is, 0..1. It may fall back
    // (a total that grew), which rescales what came before rather than moving the bar.
    void work(double now, double work);
    // The same, with a time left the job computed itself (training's forecast).
    void work(double now, double work, double remaining_s);
    void finish(double now);

    bool started() const { return _t0 >= 0.0; }
    bool finished() const { return _t_end >= 0.0; }
    // Changes only when the job reports, so the bar moves in honest steps.
    float fraction() const { return _frac; }
    double elapsed(double now) const;
    // < 0 while unknown.
    double remaining(double now) const;

private:
    struct Sample { double t, w; };
    void estimate(double now, double w, double external);

    std::vector<Sample> _s;
    double _t0 = -1.0, _t_end = -1.0;
    double _w = 0.0;
    double _rem = -1.0, _rem_at = 0.0;
    float _frac = 0.0f;
};

}  // namespace gui

// progress_clock -- the bars' time model (app/gui/ProgressClock.h) on jobs
// whose timing is known.

#include "app/gui/ProgressClock.h"

#include <cmath>
#include <cstdio>
#include <string>

using gui::ProgressClock;

namespace {

int g_failures = 0;

void expect(bool ok, const std::string& what) {
    std::printf("%s  %s\n", ok ? "ok  " : "BAD ", what.c_str());
    if (!ok) g_failures++;
}

bool close_to(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

}  // namespace

int main() {
    {
        ProgressClock c;
        c.start(0.0);
        expect(c.remaining(0.0) < 0 && c.fraction() == 0.0f, "unknown before any work");
        for (int t = 1; t <= 10; ++t) c.work(t, t / 100.0);
        expect(close_to(c.remaining(10.0), 90.0, 1.0), "a linear job has 90 s of 100 left at 10 s");
        expect(close_to(c.fraction(), 0.1, 0.01), "and its bar is at a tenth");
        const float f = c.fraction();
        expect(c.fraction() == f && close_to(c.remaining(15.0), 85.0, 1.0),
               "between reports the bar holds and the time left counts down");
        expect(close_to(c.remaining(200.0), 18.0, 0.5), "a stall holds at a fifth of the estimate");
        expect(close_to(c.elapsed(12.5), 12.5, 1e-9), "elapsed is the clock since start");
    }
    {
        // Each unit costs more than the last: t = 100 w^2.
        ProgressClock c;
        c.start(0.0);
        for (int i = 1; i <= 50; ++i) {
            const double w = i / 100.0;
            c.work(100.0 * w * w, w);
        }
        expect(close_to(c.remaining(25.0), 75.0, 4.0), "a job slowing as it grows: 75 s left at half its work");
        expect(c.fraction() < 0.3f, "and its bar is at a quarter, not a half");
    }
    {
        // Thirty seconds of start-up, then one unit a second.
        ProgressClock c;
        c.start(0.0);
        c.work(30.0, 0.0);
        for (int i = 1; i <= 10; ++i) c.work(30.0 + i, i / 100.0);
        expect(close_to(c.remaining(40.0), 90.0, 2.0), "a start-up cost is not charged to the work still to do");
    }
    {
        // Matching's second pass: the total grows by a quarter when the first is done.
        ProgressClock c;
        c.start(0.0);
        for (int t = 1; t <= 80; ++t) c.work(t, t / 80.0);
        const float before = c.fraction();
        c.work(81.0, 0.8);
        expect(c.fraction() >= before, "a total that grows does not move the bar back");
        for (int t = 82; t <= 90; ++t) c.work(t, (t - 1) / 100.0);
        expect(close_to(c.remaining(90.0), 11.0, 2.0), "and the time left follows the new total");
    }
    {
        float last = 0.0f;
        bool monotone = true;
        ProgressClock c;
        c.start(0.0);
        // A pace that halves halfway: the estimate rises, the bar must not fall.
        for (int t = 1; t <= 50; ++t) { c.work(t, t / 100.0); monotone &= c.fraction() >= last; last = c.fraction(); }
        for (int t = 1; t <= 100; ++t) { c.work(50.0 + 4 * t, 0.5 + t / 200.0); monotone &= c.fraction() >= last; last = c.fraction(); }
        expect(monotone, "a job that slows down never moves its bar back");
    }
    {
        ProgressClock c;
        c.start(0.0);
        c.work(10.0, 0.25, 30.0);
        expect(close_to(c.fraction(), 0.25, 1e-6) && close_to(c.remaining(10.0), 30.0, 1e-9),
               "a time left the job computed is taken as given");
        c.finish(20.0);
        expect(c.fraction() == 1.0f && c.remaining(30.0) == 0.0 && close_to(c.elapsed(99.0), 20.0, 1e-9),
               "finished is full, nothing left, and the clock stops");
    }

    std::printf(g_failures ? "FAIL %d\n" : "PASS progress clock\n", g_failures);
    return g_failures ? 1 : 0;
}

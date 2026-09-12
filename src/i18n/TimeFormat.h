#pragma once

// A duration as the user reads it: "hh:mm:ss.mmm" -- a zero-padded hour that is
// always present, so "01:23" can only be "1 h 23 min" and never "1 min 23 s"
// (ISO 8601 style, as asked for in review), with milliseconds kept because a
// profiling run needs sub-second precision. Hours are not capped at 24.
//
// Here rather than next to a caller because it has two, on opposite sides of
// the backend seam: the training driver (app/TrainerCore.h) and the SfM
// summary (src/sfm/), which does not link the engine. i18n is the leaf both
// already depend on, so there is one implementation and the log lines, the
// GUI status bar and the CLI progress line cannot drift apart.

#include <cstdio>
#include <string>

namespace spirula {
namespace i18n {

// Negative is a time that is not known yet: "--:--".
inline std::string format_duration(double seconds) {
    if (seconds < 0) return "--:--";
    // Count milliseconds and carry from there: rounding to whole seconds first
    // and then splitting them would print "00:00:60.000" for 59.9995.
    const long long ms = (long long)(seconds * 1000.0 + 0.5);
    char buf[64];
    std::snprintf(buf, sizeof buf, "%02lld:%02lld:%02lld.%03lld",
                  ms / 3600000, (ms / 60000) % 60, (ms / 1000) % 60, ms % 1000);
    return buf;
}

}  // namespace i18n
}  // namespace spirula

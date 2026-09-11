#pragma once

// A duration as the user reads it: "m:ss", and "h:mm:ss" past an hour -- an
// unpadded hour, an unpadded minute, a padded second.
//
// Here rather than next to a caller because it has two, on opposite sides of
// the backend seam: the training driver (app/TrainerCore.h, compiled into the
// engine library) and the SfM summary (src/sfm/), which is a self-contained
// library that does not link the engine. i18n is the leaf both already depend
// on, so there is one implementation, and the log lines, the GUI status bar
// and the CLI progress line cannot drift apart.

#include <cstdio>
#include <string>

namespace spirula {
namespace i18n {

// Negative is a time that is not known yet: "--:--".
inline std::string format_duration(double seconds) {
    if (seconds < 0) return "--:--";
    const int t = (int)(seconds + 0.5);   // whole seconds, rounded
    char buf[32];
    if (t >= 3600)
        std::snprintf(buf, sizeof buf, "%d:%02d:%02d", t / 3600, (t / 60) % 60,
                      t % 60);
    else
        std::snprintf(buf, sizeof buf, "%d:%02d", t / 60, t % 60);
    return buf;
}

}  // namespace i18n
}  // namespace spirula

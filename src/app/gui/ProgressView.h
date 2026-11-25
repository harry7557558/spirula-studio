#pragma once

// A ProgressClock on screen: the bar, and under it the elapsed time and the
// time left as hh:mm:ss, the same on every screen.

#include "app/gui/ProgressClock.h"

#include <string>

namespace gui {

class FileDownload;

// `overlay` may be null. `width` as ImGui takes it: negative is "to the right edge less this".
void timed_progress_bar(const ProgressClock& c, const char* overlay, float width = -1.0f,
                        bool indeterminate = false);
void progress_time_line(const ProgressClock& c, double now = progress_now());
std::string progress_time_text(double elapsed, double remaining);
// Group the bar and timing so a Stop button can sit beside them.
void download_bar(FileDownload& dl, const std::string& label);

}  // namespace gui

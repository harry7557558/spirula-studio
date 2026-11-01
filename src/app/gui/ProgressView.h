#pragma once

// A ProgressClock on screen: the bar, and under it the elapsed time and the
// time left as hh:mm:ss, the same on every screen.

#include "app/gui/ProgressClock.h"

#include <string>

namespace gui {

class FileDownload;

// `overlay` may be null. `width` as ImGui takes it: negative is "to the right edge less this".
void timed_progress_bar(const ProgressClock& c, const char* overlay, float width = -1.0f);
void progress_time_line(const ProgressClock& c);
// "elapsed hh:mm:ss   ETA hh:mm:ss", for a bar with no room for a line under it.
std::string progress_time_text(const ProgressClock& c);
// A download's bar with its byte count and both times inside it, for the rows
// where a Stop button sits beside it.
void download_bar(FileDownload& dl, const std::string& label);

}  // namespace gui

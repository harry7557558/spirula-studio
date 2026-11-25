// ProgressView.cpp -- see ProgressView.h.

#include "app/gui/ProgressView.h"

#include "app/gui/ModelCache.h"
#include "app/gui/Ui.h"
#include "i18n/TimeFormat.h"
#include "i18n/catalog/Gui.h"

namespace gui {

namespace msg = spirula::i18n::msg::gui;
using spirula::i18n::format_duration_coarse;

void timed_progress_bar(const ProgressClock& c, const char* overlay, float width,
                        bool indeterminate) {
    const float fill = c.fraction();
    ui::ProgressBarRaw(indeterminate && fill == 0.0f ? -(float)ImGui::GetTime() : fill,
                       ImVec2(width, 0), overlay);
    progress_time_line(c);
}

std::string progress_time_text(double elapsed, double remaining) {
    return spirula::i18n::format(msg::progress_time,
                                {format_duration_coarse(elapsed), format_duration_coarse(remaining)});
}

void progress_time_line(const ProgressClock& c, double now) {
    if (c.finished()) {
        ui::TextDisabled(msg::progress_elapsed, {format_duration_coarse(c.elapsed(now))});
    } else {
        ui::TextDisabledRaw(progress_time_text(c.elapsed(now), c.remaining(now)));
        ui::help_on_hover(msg::progress_time_help);
    }
}

void download_bar(FileDownload& dl, const std::string& label) {
    const ProgressClock& c = dl.clock();
    ImGui::BeginGroup();
    timed_progress_bar(c, label.c_str(), px(440.0f), dl.progress() < 0.0f);
    ImGui::EndGroup();
}

}  // namespace gui

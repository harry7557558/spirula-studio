// ProgressView.cpp -- see ProgressView.h.

#include "app/gui/ProgressView.h"

#include "app/gui/ModelCache.h"
#include "app/gui/Ui.h"
#include "i18n/TimeFormat.h"
#include "i18n/catalog/Gui.h"

namespace gui {

namespace msg = spirula::i18n::msg::gui;

void timed_progress_bar(const ProgressClock& c, const char* overlay, float width) {
    ui::ProgressBarRaw(c.fraction(), ImVec2(width, 0), overlay);
    progress_time_line(c);
}

std::string progress_time_text(const ProgressClock& c) {
    const double now = progress_now();
    return spirula::i18n::format(msg::progress_time,
                                 {spirula::i18n::format_duration_coarse(c.elapsed(now)),
                                  spirula::i18n::format_duration_coarse(c.remaining(now))});
}

void progress_time_line(const ProgressClock& c) {
    ui::TextDisabledRaw(progress_time_text(c));
    ui::help_on_hover(msg::progress_time_help);
}

void download_bar(FileDownload& dl, const std::string& label) {
    const ProgressClock& c = dl.clock();
    const std::string text = label + "   " + progress_time_text(c);
    ui::ProgressBarRaw(dl.progress() >= 0.0f ? c.fraction() : -(float)ImGui::GetTime(),
                       ImVec2(px(440.0f), 0), text.c_str());
    ui::help_on_hover(msg::progress_time_help);
}

}  // namespace gui

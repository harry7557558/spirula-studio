#include "app/RunLog.h"

#include "i18n/Message.h"
#include "i18n/catalog/Gui.h"
#include "i18n/catalog/Train.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace fs = std::filesystem;

namespace spirula::runlog {

namespace msg = spirula::i18n::msg;

static std::tm local_tm(std::time_t t) {
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return tm;
}

std::string line_stamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    const int ms = int(std::chrono::duration_cast<std::chrono::milliseconds>(
                           now.time_since_epoch()).count() % 1000);
    const std::tm tm = local_tm(t);
    char date[32], frac[16];
    std::strftime(date, sizeof(date), "[%Y-%m-%d %H:%M:%S", &tm);
    std::snprintf(frac, sizeof(frac), ".%03d] ", ms);
    return std::string(date) + frac;
}

std::string file_stamp() {
    const std::tm tm = local_tm(
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    char buf[24];
    std::strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", &tm);
    return buf;
}

std::string dataset_dir(const std::string& data) {
    if (fs::path(data).filename() == "images") return fs::path(data).parent_path().string();
    return data;
}

fs::path open(std::ofstream& f, const std::string& dataset_dir,
              const char* kind, const std::string& stamp) {
    if (f.is_open()) f.close();
    f.clear();
    if (dataset_dir.empty()) return {};
    std::error_code ec;
    const fs::path dir = fs::path(dataset_dir) / "logs";
    fs::create_directories(dir, ec);
    if (ec) return {};
    const fs::path file = dir / (std::string(kind) + "_" + stamp + ".log");
    f.open(file, std::ios::out | std::ios::app);
    if (!f.is_open()) return {};
    return file;
}

void write_line(std::ofstream& f, const std::string& s) {
    if (!f.is_open()) return;
    size_t at = 0;
    do {
        const size_t nl = s.find('\n', at);
        f << line_stamp()
          << s.substr(at, nl == std::string::npos ? nl : nl - at) << '\n';
        at = nl == std::string::npos ? nl : nl + 1;
    } while (at != std::string::npos);
    f.flush();
}

std::string cfg_str(const std::string& v) { return v; }
std::string cfg_str(bool v) { return v ? "true" : "false"; }
std::string cfg_str(int v) { return std::to_string(v); }
std::string cfg_str(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.7g", double(v));
    return buf;
}

std::string section(const std::string& name) { return "[" + name + "]\n"; }
std::string entry(const std::string& key, const std::string& value) {
    return "  " + key + " = " + value + "\n";
}

std::string run_heading() { return section(msg::gui::runlog_section_run.get()); }

std::string train_sections(const TrainConfig& c) {
    std::string out;
    for (int si = 0; si < kTrainNumSections; si++) {
        std::string body;
#define SS_SNAP_FIELD(type, member, default_, s, tier, choices)              \
        if (!std::strcmp(s, kTrainSections[si]))                             \
            body += entry(#member, cfg_str(c.member));
        SS_CONFIG_FIELDS(SS_SNAP_FIELD)
#undef SS_SNAP_FIELD
        if (body.empty()) continue;
        const i18n::Msg* label = msg::train::section_label(kTrainSections[si]);
        out += section(i18n::format(msg::gui::runlog_section_train,
                                    {label ? label->get() : kTrainSections[si]}));
        out += body;
    }
    return out;
}

// Fixed width, so the rule still reads as one whatever the label measures.
std::string settings_end() {
    const std::string end = msg::gui::runlog_settings_end.get();
    const int fill = std::max(72 - i18n::display_width(end) - 2, 6);
    return std::string(fill / 2, '=') + " " + end + " " +
           std::string(fill - fill / 2, '=') + "\n\n";
}

}  // namespace spirula::runlog

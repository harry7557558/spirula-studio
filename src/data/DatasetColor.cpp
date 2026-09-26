#include "data/DatasetColor.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace spirula {

const char* clip_color_token(ClipColor c) {
    switch (c) {
        case ClipColor::Normal:  return "normal";
        case ClipColor::DlogM:   return "dlogm";
        case ClipColor::OtherLog: return "unsupported-log";
        case ClipColor::Unknown: return "unknown";
        default:                 return "unrecorded";
    }
}

ClipColor clip_color_from_token(const std::string& t) {
    if (t == "normal")     return ClipColor::Normal;
    if (t == "dlogm")      return ClipColor::DlogM;
    if (t == "unsupported-log") return ClipColor::OtherLog;
    if (t == "unrecorded") return ClipColor::NotRecorded;
    return ClipColor::Unknown;
}

DatasetColor read_dataset_color(const std::string& dataset_dir) {
    DatasetColor d;
    if (dataset_dir.empty()) return d;
    const fs::path p = fs::path(dataset_dir) / kDatasetColorFile;
    std::error_code ec;
    if (!fs::exists(p, ec)) return d;
    std::ifstream f(p, std::ios::binary);
    // There but unreadable says as little as a torn line does.
    if (!fs::is_regular_file(p, ec) || !f) {
        d.clips.push_back({ClipColor::Unknown, -1, {}, kDatasetColorFile});
        return d;
    }
    std::string line;
    while (std::getline(f, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        if (line.find_first_not_of(" \t") == std::string::npos) continue;
        std::istringstream in(line);
        std::string token;
        ClipColorEntry e;
        std::string source;
        if (in >> token >> e.code >> e.proto && std::getline(in >> std::ws, source) &&
            !source.empty()) {
            e.mode = clip_color_from_token(token);
            e.source = source;
            if (e.proto == "-") e.proto.clear();
        } else {
            e = ClipColorEntry{ClipColor::Unknown, -1, {}, line};
        }
        d.clips.push_back(e);
    }
    return d;
}

namespace {

// Removes the record; FailedStale when it is still there afterwards.
ColorRecordResult drop_record(const fs::path& p, ColorRecordResult r) {
    std::error_code ec;
    fs::remove(p, ec);
    std::error_code gone;
    if (fs::exists(p, gone) || gone) {
        r.status = ColorRecordWrite::FailedStale;
        if (r.error.empty()) r.error = ec ? ec.message() : std::string("not removed");
    }
    return r;
}

}  // namespace

ColorRecordResult write_dataset_color(const std::string& dataset_dir, const DatasetColor& d) {
    if (dataset_dir.empty()) return {};
    const fs::path p = fs::path(dataset_dir) / kDatasetColorFile;
    bool any = false;
    for (const ClipColorEntry& e : d.clips) any = any || e.mode != ClipColor::NotRecorded;
    if (!any) return drop_record(p, {ColorRecordWrite::Removed, {}});

    // Whole or not at all: a half-written record reads as a different verdict.
    const fs::path tmp = fs::path(p.string() + ".tmp");
    std::error_code ec;
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        for (const ClipColorEntry& e : d.clips)
            f << clip_color_token(e.mode) << ' ' << e.code << ' '
              << (e.proto.empty() ? "-" : e.proto) << ' ' << e.source << '\n';
        f.flush();
        if (!f) ec = std::make_error_code(std::errc::io_error);
    }
    if (!ec) {
        // Windows will not rename over a read-only file.
        if (fs::exists(p, ec))
            fs::permissions(p, fs::perms::owner_write, fs::perm_options::add, ec);
        ec.clear();
        fs::rename(tmp, p, ec);
    }
    if (!ec) return {};
    std::error_code ignore;
    if (fs::is_regular_file(tmp, ignore)) fs::remove(tmp, ignore);
    return drop_record(p, {ColorRecordWrite::Failed, ec.message()});
}

DatasetColorSummary summarize_dataset_color(const DatasetColor& d) {
    DatasetColorSummary s;
    for (const ClipColorEntry& e : d.clips) {
        switch (e.mode) {
            case ClipColor::DlogM:
                s.dlogm++;
                if (e.proto == "dvtm_AVATA360.proto") s.dlogm_avata++;
                break;
            case ClipColor::Normal: s.not_log++; break;
            case ClipColor::OtherLog:
                if (s.other_log++ == 0) {
                    s.first_other_log = e.source;
                    s.first_other_code = e.code;
                }
                break;
            case ClipColor::Unknown:
                if (s.unknown++ == 0) s.first_unknown = e.source;
                break;
            default: s.unrecorded++; break;
        }
    }
    const int rest = s.not_log + s.other_log + s.unknown + s.unrecorded;
    if (s.dlogm > 0)          s.verdict = rest > 0 ? DatasetColorVerdict::Mixed : DatasetColorVerdict::DlogM;
    else if (s.other_log > 0) s.verdict = DatasetColorVerdict::UnsupportedLog;
    else if (s.unknown > 0)   s.verdict = DatasetColorVerdict::Unknown;
    else if (s.not_log > 0)   s.verdict = DatasetColorVerdict::NotLog;
    return s;
}

}  // namespace spirula

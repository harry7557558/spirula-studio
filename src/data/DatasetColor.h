#pragma once

// The picture profile each source clip of a dataset was shot in, recorded when
// the dataset is prepared, because extracted frames carry no such tag. The
// trainer reads it to pick `--image-color-log` when the user left it on `auto`.

#include <string>
#include <vector>

namespace spirula {

// In the dataset root, beside .spirula-frames, one line per input:
// "<mode> <code> <proto or -> <source>".
inline constexpr const char* kDatasetColorFile = ".spirula-color";

// NotRecorded: the input carries no profile metadata at all (photos, a
// non-DJI video). Unknown: DJI metadata in a layout not verified here.
// OtherLog: a DJI profile this build cannot decode (D-Log, D-Log2, HLG...).
enum class ClipColor { NotRecorded, Normal, DlogM, OtherLog, Unknown };

struct ClipColorEntry {
    ClipColor mode = ClipColor::NotRecorded;
    int code = -1;
    std::string proto;    // the DJI metadata layout; empty when none
    std::string source;   // file name only
};

struct DatasetColor {
    std::vector<ClipColorEntry> clips;
};

const char* clip_color_token(ClipColor c);
ClipColor clip_color_from_token(const std::string& token);

// Empty when the dataset has no record. A line that does not parse, or a
// record that is there but cannot be read, is an Unknown entry.
DatasetColor read_dataset_color(const std::string& dataset_dir);
enum class ColorRecordWrite { Written, Removed, Failed, FailedStale };
struct ColorRecordResult {
    ColorRecordWrite status = ColorRecordWrite::Written;
    std::string error;   // why, when it failed
};

// Written to a temp file and renamed into place; on any failure the old record
// is removed too, and FailedStale means even that did not work.
ColorRecordResult write_dataset_color(const std::string& dataset_dir, const DatasetColor& d);

enum class DatasetColorVerdict { None, DlogM, NotLog, UnsupportedLog, Unknown, Mixed };

struct DatasetColorSummary {
    DatasetColorVerdict verdict = DatasetColorVerdict::None;
    int dlogm = 0, not_log = 0, other_log = 0, unknown = 0, unrecorded = 0;
    int dlogm_avata = 0;         // of `dlogm`, those shot on an Avata 360
    std::string first_unknown;   // for the line that names one
    std::string first_other_log;
    int first_other_code = -1;
};

// Mixed is D-Log M beside anything that is not: a guess either way decodes
// some clips wrongly.
DatasetColorSummary summarize_dataset_color(const DatasetColor& d);

}  // namespace spirula

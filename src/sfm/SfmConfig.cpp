// The consumers of SFM_CONFIG_FIELDS: the flag parser, the preset appliers,
// the `--help` printer and the run-time rows an editor reads. Each is one
// macro expansion over the table in SfmConfig.h, so a new knob is one row.
#include "sfm/SfmConfig.h"

#include "core/RelativePath.h"
#include "sfm/core/Log.h"
#include "sfm/vk/VkContext.h"
#include "i18n/catalog/SfmFields.h"
#include "i18n/catalog/SfmOptions.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <stdexcept>

namespace sfm {
namespace {

// ---------------------------------------------------------------------------
// Value <-> string, one overload set per field type
// ---------------------------------------------------------------------------

std::string valueString(bool v) { return v ? "on" : "off"; }
std::string valueString(const std::string& v) { return v.empty() ? "none" : v; }

template <class T>
std::enable_if_t<std::is_integral<T>::value && !std::is_same<T, bool>::value, std::string>
valueString(T v) {
    char buf[32];
    if (std::is_signed<T>::value) std::snprintf(buf, sizeof buf, "%lld", (long long)v);
    else std::snprintf(buf, sizeof buf, "%llu", (unsigned long long)v);
    return buf;
}

template <class T>
std::enable_if_t<std::is_floating_point<T>::value, std::string> valueString(T v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%g", (double)v);
    return buf;
}

// ---------------------------------------------------------------------------
// Parsing one token into one field
// ---------------------------------------------------------------------------

std::string rangeText(double lo, double hi) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "expected a value in [%g, %g]", lo, hi);
    return buf;
}

template <class T>
std::enable_if_t<std::is_integral<T>::value && !std::is_same<T, bool>::value, bool>
parseValue(T& out, const std::string& s, double lo, double hi, const char*, std::string& err) {
    long long v = 0;
    try {
        size_t p = 0;
        v = std::stoll(s, &p);
        if (p != s.size()) throw std::invalid_argument("trailing characters");
    } catch (...) {
        err = "expected an integer";
        return false;
    }
    if (std::is_unsigned<T>::value && v < 0) {
        err = "must not be negative";
        return false;
    }
    if (lo < hi && ((double)v < lo || (double)v > hi)) {
        err = rangeText(lo, hi);
        return false;
    }
    out = (T)v;
    return true;
}

template <class T>
std::enable_if_t<std::is_floating_point<T>::value, bool> parseValue(T& out, const std::string& s,
                                                                    double lo, double hi,
                                                                    const char*, std::string& err) {
    double v = 0;
    try {
        size_t p = 0;
        v = std::stod(s, &p);
        if (p != s.size()) throw std::invalid_argument("trailing characters");
    } catch (...) {
        err = "expected a number";
        return false;
    }
    if (lo < hi && (v < lo || v > hi)) {
        err = rangeText(lo, hi);
        return false;
    }
    out = (T)v;
    return true;
}

bool parseValue(std::string& out, const std::string& s, double, double, const char* choices,
                std::string& err) {
    std::string ch = choices ? choices : "";
    if (!ch.empty()) {
        for (size_t pos = 0; pos <= ch.size();) {
            size_t bar = ch.find('|', pos);
            std::string tok = ch.substr(pos, bar == std::string::npos ? bar : bar - pos);
            if (tok == s) { out = s; return true; }
            if (bar == std::string::npos) break;
            pos = bar + 1;
        }
        err = "expected one of: " + ch;
        for (char& c : err) if (c == '|') c = ' ';
        return false;
    }
    out = s;
    return true;
}

// A bool field is a switch, never a value: `--name` / `--no-name`. Keeping it
// valueless is what lets `spirula-sfm map --no-manage matches.bin feats/` parse
// -- a positional right after a switch must stay a positional.
FieldResult trySetField(bool& out, const std::string& key, const char* name, int, char**, int&,
                        double, double, const char*, std::set<std::string>& seen, std::string&) {
    if (key == name) out = true;
    else if (key.size() > 3 && key.compare(0, 3, "no-") == 0 && key.compare(3, std::string::npos, name) == 0)
        out = false;
    else return FieldResult::Unknown;
    seen.insert(name);
    return FieldResult::Ok;
}

template <class T>
FieldResult trySetField(T& out, const std::string& key, const char* name, int argc, char** argv,
                        int& i, double lo, double hi, const char* choices,
                        std::set<std::string>& seen, std::string& error) {
    if (key != name) return FieldResult::Unknown;
    if (i + 1 >= argc) {
        error = std::string("--") + name + ": missing value";
        return FieldResult::Error;
    }
    std::string why;
    if (!parseValue(out, argv[++i], lo, hi, choices, why)) {
        error = std::string("--") + name + " " + argv[i] + ": " + why;
        return FieldResult::Error;
    }
    seen.insert(name);
    return FieldResult::Ok;
}

// "--Max_Error" and "--max-error" are the same flag; '_' is accepted because
// the GUI and the config table spell ids either way.
std::string normalizeKey(const std::string& arg) {
    std::string s = arg;
    while (!s.empty() && s[0] == '-') s.erase(s.begin());
    for (char& c : s) if (c == '_') c = '-';
    return s;
}

// ---------------------------------------------------------------------------
// Help formatting
// ---------------------------------------------------------------------------

// Long choice lists do not fit the flag column; they move into the help text.
constexpr size_t kInlineChoices = 26;

std::string metavarFor(bool, const char*, const char*) { return ""; }
std::string metavarFor(const std::string&, const char* name, const char* choices) {
    if (choices && *choices) {
        // A field WITH choices never takes a path, whatever it is called --
        // `--features` is a directory on `map` and a frontend name on
        // `extract`, and only the latter has choices. Long lists do not fit
        // the flag column and are printed in the help text instead.
        return std::string(choices).size() <= kInlineChoices
                   ? "{" + std::string(choices) + "}"
                   : "VALUE";
    }
    // No metavar column in the table: the flag name says what it takes, and
    // "DIR" reads better than "VALUE" on the handful that take a path.
    std::string n = name;
    if (n.find("dir") != std::string::npos || n == "masks" ||
        n == "images" || n == "features" || n == "resume")
        return "DIR";
    if (n.find("path") != std::string::npos) return "FILE";
    return "VALUE";
}
template <class T>
std::enable_if_t<std::is_integral<T>::value && !std::is_same<T, bool>::value, std::string>
metavarFor(T, const char*, const char*) {
    return "N";
}
template <class T>
std::enable_if_t<std::is_floating_point<T>::value, std::string> metavarFor(T, const char*,
                                                                          const char*) {
    return "X";
}

// A bool is offered in the direction that changes it, which is how it is
// written on a real command line (`--no-verify`, not `--verify 0`).
std::string flagFor(bool v, const char* name) {
    return v ? "--no-" + std::string(name) : "--" + std::string(name);
}
template <class T>
std::string flagFor(const T&, const char* name) {
    return "--" + std::string(name);
}

constexpr int kFlagCol = 42;  // flag column width; the widest flag+metavar fits
constexpr int kWidth = 96;

// Wrapping is i18n::wrap's job rather than a find(' ') loop: it measures in
// terminal columns instead of bytes, and it breaks between characters where a
// language has no spaces to break at -- without which a Japanese help string
// is one line four hundred columns wide.
void printWrapped(FILE* out, const std::string& text, int indent) {
    for (const std::string& line : spirula::i18n::wrap(text, kWidth - indent))
        std::fprintf(out, "%*s%s\n", indent, "", line.c_str());
}

// The heading a block of options is printed under. Keyed by the same string
// the table carries, so a group added there without an entry here still
// prints -- as itself, which is the honest fallback for a heading.
std::string baseGroup(const char* group) {
    const char* slash = std::strchr(group, '/');
    return slash ? std::string(group, slash) : std::string(group);
}

std::string groupLabel(const std::string& group) {
    namespace F = spirula::i18n::msg::sfmfield;
    struct Row { const char* key; const spirula::i18n::Msg* msg; };
    static const Row kRows[] = {
        {"pipeline", &F::group_pipeline}, {"colour", &F::group_colour},
        {"camera", &F::group_camera},
        {"features", &F::group_features}, {"matching", &F::group_matching},
        {"mapper", &F::group_mapper},     {"rig", &F::group_rig},
        {"manage", &F::group_manage},
        {"merge", &F::group_merge},       {"input", &F::group_input},
        {"runtime", &F::group_runtime},
    };
    for (const Row& r : kRows)
        if (group == r.key) return r.msg->get();
    return group;
}

void printOption(FILE* out, const std::string& flag, const std::string& metavar,
                 const std::string& value, const std::string& help, const char* choices) {
    std::string left = "  " + flag + (metavar.empty() ? "" : " " + metavar);
    if (value.empty()) {
        std::fprintf(out, "%s\n", left.c_str());
    } else {
        if ((int)left.size() >= kFlagCol) std::fprintf(out, "%s\n%*s", left.c_str(), kFlagCol, "");
        else std::fprintf(out, "%-*s", kFlagCol, left.c_str());
        std::fprintf(out, "[%s]\n", value.c_str());
    }
    std::string h = help;
    std::string ch = choices ? choices : "";
    if (!ch.empty() && ch.size() > kInlineChoices) {
        for (char& c : ch) if (c == '|') c = ' ';
        h += " (one of: " + ch + ")";
    }
    printWrapped(out, h, 6);
}

}  // namespace

// ---------------------------------------------------------------------------
// setConfigField
// ---------------------------------------------------------------------------

FieldResult setConfigField(SfmConfig& cfg, uint32_t cmd, const std::string& arg, int argc,
                           char** argv, int& i, std::set<std::string>& seen, std::string& error) {
    const std::string key = normalizeKey(arg);
    if (key.empty()) return FieldResult::Unknown;
    // Retired: off was every run's default, on is --pairs sequential minus the mapper's order.
    const bool pairing = cmd & (CMD_AUTO | CMD_MATCH);
    if (pairing && key == "no-prefilter-sequential") return FieldResult::Ok;
    if (pairing && key == "prefilter-sequential") {
        error = "--prefilter-sequential is retired: use --pairs sequential, which also "
                "matches the GPU pre-selection's pairs (--loop-closure)";
        return FieldResult::Error;
    }
#define SFM_TRY_SET(member, name, cmds, tier, group, lo, hi, choices, help)                        \
    if ((uint32_t)(cmds) & cmd) {                                                                  \
        FieldResult r = trySetField(cfg.member, key, name, argc, argv, i, (double)(lo),            \
                                    (double)(hi), choices, seen, error);                           \
        if (r != FieldResult::Unknown) {                                                           \
            if (r == FieldResult::Ok && key == "device")                                           \
                cfg.device_request_set = true;                                                     \
            return r;                                                                               \
        }                                                                                           \
    }
    SFM_CONFIG_FIELDS(SFM_TRY_SET, SFM_TRY_SET)
#undef SFM_TRY_SET
    return FieldResult::Unknown;
}

// ---------------------------------------------------------------------------
// Presets
// ---------------------------------------------------------------------------

namespace {
// Move a field unless the command line already claimed it, and say so. The
// report is what tells a user why `--quality low` produced a 1000 px pipeline,
// and it is the same list the GUI highlights as modified.
template <class T, class V>
void presetSet(std::set<std::string> const& seen, std::vector<PresetChange>& moved,
               const char* flag, T& member, V value) {
    if (seen.count(flag)) return;
    T next = (T)value;
    if (next == member) return;
    moved.push_back({flag, valueString(member), valueString(next)});
    member = next;
}

// Quality mirrors COLMAP's OptionManager::ModifyFor*Quality() as fractions of
// the extractor's 3200 px default; pair-selection breadth follows it (D42).
// A learned frontend runs about two thirds of SIFT's size with half the points.
struct QualityLevel {
    const char* name;
    int sift_size, learned_size, sift_features, learned_features, neighbors;
};
constexpr QualityLevel kQualityLevels[] = {
    {"low", 1000, 800, 2048, 1024, 16},
    {"medium", 1600, 1200, 4096, 2048, 24},
    {"high", 2400, 1600, 8192, 4096, 32},
    {"extreme", 3200, 2400, 16384, 8192, 48},
};
}  // namespace

std::string applyPresets(SfmConfig& cfg, const std::set<std::string>& seen,
                         std::vector<PresetChange>& moved) {
    const bool learned = isAlikedType(cfg.features) || isLomaType(cfg.features);
    const QualityLevel* q = nullptr;
    for (const QualityLevel& l : kQualityLevels)
        if (cfg.quality == l.name) q = &l;
    if (!q) return "unknown --quality '" + cfg.quality + "' (low, medium, high or extreme)";
    presetSet(seen, moved, "max-image-size", cfg.max_image_size,
              learned ? q->learned_size : q->sift_size);
    presetSet(seen, moved, "max-features", cfg.sift.max_num_features, q->sift_features);
    presetSet(seen, moved, "aliked-max-features", cfg.aliked.max_num_features,
              q->learned_features);
    presetSet(seen, moved, "loma-max-features", cfg.loma.max_num_features, q->learned_features);
    presetSet(seen, moved, "prefilter-neighbors", cfg.prefilter.num_neighbors, q->neighbors);

    // A learned descriptor needs a looser ratio than SIFT's 0.8, because its
    // second-best distance sits much closer to its best: measured on a
    // 20-image capture, the median mutual-nearest match has a distance ratio
    // of 0.826, i.e. just the wrong side of SIFT's threshold. The whole point
    // of the ratio test -- that a true match stands out from the runner-up --
    // is weaker for descriptors trained to be smooth, which is also why
    // LightGlue exists.
    //
    // 0.92 is where the measurement put it, on 190 exhaustive pairs:
    //
    //   ratio   0.80   0.85   0.90   0.92   0.95
    //   pairs   65/190 79     111    172    190
    //   inliers 6331   8426   11487  13633  16249
    //   inlier% 90     79     54     44     29
    //
    // Past 0.92 the pair count is bought with junk. For reference, GPU SIFT on
    // the same images at 4x the feature budget managed 68/190 and 7703
    // inliers, so this is not a concession -- it is where the learned frontend
    // wins.
    //
    // COLMAP's own ALIKED defaults (ratio off, min_cossim 0.85) were tried
    // first and are NOT used: on this data an absolute 0.85 cosine rejects
    // ~70% of mutual-nearest matches (their median cosine is 0.726) and left
    // 24/190 pairs. --min-similarity still exists for anyone who wants that
    // shape of test.
    if (learned && !isLearnedMatcher(cfg.matcher))
        presetSet(seen, moved, "ratio", cfg.match.max_ratio, 0.92f);

    // A learned matcher decides its own assignment, so the ratio test and the
    // cross-check are not merely unnecessary -- they are a second filter on a
    // quantity it does not produce. Its own confidence is the only threshold.
    if (isLearnedMatcher(cfg.matcher)) {
        presetSet(seen, moved, "ratio", cfg.match.max_ratio, 1.0f);
        presetSet(seen, moved, "min-similarity", cfg.match.min_similarity, 0.0f);
    }
    return "";
}

void applySequenceDefaults(SfmConfig& cfg, const std::set<std::string>& seen,
                           std::vector<PresetChange>& moved) {
    if (cfg.sequences.empty()) return;
    // COLMAP ModifyForVideoData: the seed cannot be held to a photo set's angle.
    presetSet(seen, moved, "init-min-tri-angle", cfg.mapper.init_min_tri_angle_deg,
              MapperOptions().init_min_tri_angle_deg / 2);
}

// ---------------------------------------------------------------------------
// finalize
// ---------------------------------------------------------------------------

std::string SfmConfig::finalize(uint32_t cmd) {
    if (!parseCamModelName(camera_model, camera.model))
        return "unknown --camera-model '" + camera_model + "'";
    if (!parseCameraMode(camera_mode, camera.mode))
        return "unknown --camera-mode '" + camera_mode + "'";
    camera.mode_explicit = camera_mode_pinned;
    camera.focal = focal;
    if (!distortion.empty() && !parseDistortion(distortion, camera.extra))
        return "bad --distortion '" + distortion + "' (k1,k2,... or PREFIX=k1,k2,...)";
    // The free intrinsics are a prefix of (focal, distortion, principal point):
    // holding the middle holds the tail (D50, D72). Two explicit flags saying
    // otherwise is an error; the finishing pass's default gives way instead.
    if (mapper.refine_principal_point && !mapper.refine_extra_params)
        return "--refine-principal-point cannot be combined with "
               "--no-refine-extra-params: bundle adjustment frees a prefix of "
               "(focal, distortion, principal point)";
    if (!final_extra_params) final_principal_point = false;
    // The model a group with no explicit entry falls back to, which is the same
    // question --camera-model answers for the ones that have entries.
    mapper.camera_model = camera.model;

    // One tolerance, two fields (D47).
    twoview.ransac.max_error = max_error;
    mapper.max_reproj_error = max_error;
    // mapper.sequence_window = overlap;

    if (!colorspace::parse_exposure(image_exposure, exposure))
        return "bad --image-exposure '" + image_exposure + "' (auto, or a number of stops)";

    if (features != "sift" && !isAlikedType(features) && !isLomaType(features))
        return "unknown --features '" + features +
               "' (sift, aliked-n16rot, aliked-n32, loma-b128 or loma-b)";
    if (matcher != "bruteforce" && !isLearnedMatcher(matcher))
        return "unknown --matcher '" + matcher +
               "' (bruteforce, lightglue, flexglue or loma-b128)";
    // Matching SIFT with a learned matcher would run and return nonsense.
    // Only `auto` can check it here; `match` reads features off disk, so its
    // guard is on the descriptors themselves, in LearnedMatcher.cpp.
    if ((cmd & (CMD_AUTO | CMD_EXTRACT)) && matcherReadsDescriptors(matcher) &&
        !isAlikedType(features) && !isLomaType(features))
        return "--matcher " + matcher + " needs learned descriptors; add "
               "--features aliked-n16rot";
    // The two families do not mix either: LightGlue reads ALIKED's descriptors
    // and a LoMa matcher reads DeDoDe's, and each would run on the other's and
    // return nonsense rather than fail.
    if ((cmd & (CMD_AUTO | CMD_EXTRACT)) && isLomaType(matcher) != isLomaType(features) &&
        matcherReadsDescriptors(matcher))
        return "--matcher " + matcher + " and --features " + features +
               " are different frontends; a learned matcher only reads the "
               "descriptors it was trained on";
    // Nor do two LoMa variants whose descriptors differ. The matcher would
    // refuse them anyway, but only after the extraction had run.
    if ((cmd & (CMD_AUTO | CMD_EXTRACT)) && isLomaType(matcher) && isLomaType(features) &&
        lomaDescriptorDim(matcher) != lomaDescriptorDim(features))
        return "--matcher " + matcher + " wants " +
               std::to_string(lomaDescriptorDim(matcher)) + "-D descriptors and "
               "--features " + features + " makes " +
               std::to_string(lomaDescriptorDim(features)) + "-D ones";
    // Two metric references would each claim the gauge, and the fit would be
    // whichever the code happened to try first.
    // `auto` stands down beside a positions file and without images or telemetry.
    const bool gps = metricGps();
    if (!metric_positions.empty() && gps)
        return "--metric-positions and --metric-gps are two references for one "
               "gauge; pass one";
    if (gps && image_dir.empty() && !(cmd & CMD_AUTO))
        return "--metric-gps reads each image's EXIF, so it needs --images";
    if (!telemetry.empty()) {
        bool listed = false;
        for (const TelemetryInput& t : telemetry_inputs)
            if (t.prefix.empty() && t.path == telemetry) listed = true;
        if (!listed) telemetry_inputs.push_back({"", telemetry, 0, 0});
    }

    lightglue.device = device;
    loma.device = loma_match.device = device;
    flexglue.gamut = image_gamut;
    flexglue.is_linear = image_is_linear;
    flexglue.exposure = exposure;
    flexglue.apply_exif_orientation = exif_orientation == "apply";
    if (max_image_size <= 0) max_image_size = defaultMaxImageSize(features);

    sift.device = match.device = prefilter.device = mapper.device = device;
    aliked.device = device;
    // UUID crosses stage and worker boundaries; the int remains legacy input spelling.
    mapper.threads = threads;
    const bool v = !quiet;
    sift.verbose = mapper.verbose = manager.verbose = merge.verbose = aliked.verbose = v;
    lightglue.verbose = loma.verbose = loma_match.verbose = flexglue.verbose = v;

    // Scoring problems are ~1/32 the size of full matching, so the selection
    // pass batches at least as many pairs per submit as the matcher does.
    prefilter.batch_pairs = std::max(prefilter.batch_pairs, match.batch_pairs);

    // Post-merge filtering follows the mapper, because spliced tracks have
    // never seen a bundle adjustment across the seam and should not survive
    // criteria the mapper's own observations would not. `merge` is the
    // exception: there is no mapper in that command, and --filter-error /
    // --min-tri-angle are the flags that set these directly.
    if (cmd != CMD_MERGE) {
        merge.filter_reproj_error = mapper.max_reproj_error;
        merge.min_tri_angle_deg = mapper.min_tri_angle_deg;
        merge.min_image_points = mapper.min_image_points;
    }
    manager.merge = merge;
    manager.merge.verbose = v;
    return "";
}

PairMode SfmConfig::pairMode() const {
    if (pairs == "sequential") return PairMode::Sequential;
    if (pairs == "exhaustive") return PairMode::Exhaustive;
    return PairMode::Prefilter;
}

// ---------------------------------------------------------------------------
// Device resolution
// ---------------------------------------------------------------------------

std::string SfmConfig::selectorForDevice(const std::string& request, bool request_set,
                                          std::string& error) {
    error.clear();
    // The request's own spelling wins; an unset one uses environment, then Auto.
    const bool supplied = request_set || !request.empty();
    const spirula::vkselect::Request req =
        spirula::vkselect::requestFrom(request, supplied);
    const spirula::vkselect::Resolution& res = VkContext::cachedSelector(req);
    if (res.ok()) return res.selector;
    // Only the effective default Auto may use the CPU path when no device exists.
    if (!supplied &&
        res.status == spirula::vkselect::ResolveStatus::NoDevice &&
        req.kind == spirula::vkselect::Request::Kind::Auto)
        return std::string();
    error = res.error;
    return std::string();
}

std::string SfmConfig::resolveDevice() {
    // Resolve the request once before stages inspect it.
    const bool request_set = device_request_set || !device_request.empty() ||
                             !device_selector.empty() || device >= 0;
    const bool request_text = device_request_set || !device_request.empty();
    const std::string request =
        request_text ? device_request
        : !device_selector.empty() ? device_selector
        : device >= 0 ? std::to_string(device) : std::string();
    std::string error;
    device_selector = selectorForDevice(request, request_set, error);
    if (!error.empty()) return error;
    // An ordinal request still sets the legacy int, so a caller that has not
    // migrated keeps working; the UUID is what fans out.
    if (request_text) {
        const spirula::vkselect::Request req = spirula::vkselect::parseRequest(request);
        if (req.kind == spirula::vkselect::Request::Kind::Ordinal) device = req.ordinal;
    }
    sift.device_selector = match.device_selector = prefilter.device_selector =
        mapper.device_selector = aliked.device_selector = lightglue.device_selector =
            loma.device_selector = loma_match.device_selector = flexglue.device_selector =
                device_selector;
    if (!device_selector.empty())
        sfm::slog::diag(sfm::slog::Tag::Device, "[gpu] running on %s",
                        device_selector.c_str());
    return "";
}

// ---------------------------------------------------------------------------
// The table at run time
// ---------------------------------------------------------------------------

namespace {
FieldType typeOf(bool) { return FieldType::Switch; }
FieldType typeOf(const std::string&) { return FieldType::Text; }
template <class T>
FieldType typeOf(T) {
    return std::is_floating_point<T>::value ? FieldType::Real : FieldType::Integer;
}

std::string flagValue(bool v) { return v ? "on" : "off"; }
std::string flagValue(const std::string& v) { return v; }
template <class T>
std::string flagValue(T v) { return valueString(v); }
}  // namespace

const std::vector<FieldInfo>& configFields() {
    static const std::vector<FieldInfo> rows = [] {
        const SfmConfig d;
        std::vector<FieldInfo> out;
#define SFM_INFO_ROW(member, name, cmds, tier, group, lo, hi, choices, label, help) \
    out.push_back({name, (uint32_t)(cmds), tier, group, (double)(lo), (double)(hi), choices, \
                   typeOf(d.member), label, help});
#define SFM_INFO(member, name, cmds, tier, group, lo, hi, choices, help) \
    SFM_INFO_ROW(member, name, cmds, tier, group, lo, hi, choices, nullptr, nullptr)
#define SFM_GUI_INFO(member, name, cmds, tier, group, lo, hi, choices, help) \
    SFM_INFO_ROW(member, name, cmds, tier, group, lo, hi, choices, \
                 &spirula::i18n::msg::sfmoption::help##_label, \
                 &spirula::i18n::msg::sfmoption::help##_help)
        SFM_CONFIG_FIELDS(SFM_INFO, SFM_GUI_INFO)
#undef SFM_GUI_INFO
#undef SFM_INFO
#undef SFM_INFO_ROW
        return out;
    }();
    return rows;
}

const FieldInfo* findConfigField(const std::string& name) {
    for (const FieldInfo& f : configFields())
        if ((f.cmds & CMD_AUTO) && name == f.name) return &f;
    return nullptr;
}

std::string fieldOwner(const FieldInfo& f) {
    const char* slash = std::strchr(f.group, '/');
    return slash ? std::string(slash + 1) : std::string();
}

uint32_t fieldStage(const FieldInfo& f) {
    if (f.cmds & CMD_EXTRACT) return CMD_EXTRACT;
    if (f.cmds & CMD_MATCH) return CMD_MATCH;
    // A row `auto` alone has is a preset, which fans out from extraction on.
    return (f.cmds & CMD_MAP) ? CMD_MAP : CMD_EXTRACT;
}

bool configFieldValue(const SfmConfig& cfg, const std::string& name, std::string& out) {
#define SFM_VALUE(member, fname, cmds, tier, group, lo, hi, choices, help)                         \
    if (((uint32_t)(cmds) & CMD_AUTO) && name == fname) {                                          \
        out = flagValue(cfg.member);                                                               \
        return true;                                                                               \
    }
    SFM_CONFIG_FIELDS(SFM_VALUE, SFM_VALUE)
#undef SFM_VALUE
    return false;
}

std::string checkConfigValue(const FieldInfo& f, const std::string& value) {
    std::string err;
    switch (f.type) {
        case FieldType::Switch:
            if (value != "on" && value != "off") err = "expected on or off";
            break;
        case FieldType::Integer: {
            long long v = 0;
            parseValue(v, value, f.lo, f.hi, f.choices, err);
            break;
        }
        case FieldType::Real: {
            double v = 0;
            parseValue(v, value, f.lo, f.hi, f.choices, err);
            break;
        }
        case FieldType::Text: {
            std::string v;
            parseValue(v, value, f.lo, f.hi, f.choices, err);
            break;
        }
    }
    return err;
}

// ---------------------------------------------------------------------------
// Help
// ---------------------------------------------------------------------------

void printConfigOptions(FILE* out, uint32_t cmd, const SfmConfig& defaults) {
    std::string cur_group;
#define SFM_PRINT_FIELD(member, name, cmds, tier, group, lo, hi, choices, help)                    \
    if (((uint32_t)(cmds) & cmd) && (tier) != Tier::Alias) {                                       \
        if (cur_group != baseGroup(group)) {                                                       \
            cur_group = baseGroup(group);                                                          \
            std::fprintf(out, "\n %s:\n", groupLabel(cur_group).c_str());                          \
        }                                                                                          \
        printOption(out, flagFor(defaults.member, name), metavarFor(defaults.member, name, choices),\
                    valueString(defaults.member),                                                  \
                    spirula::i18n::msg::sfmfield::help##_help.get(), choices);                     \
    }
    SFM_CONFIG_FIELDS(SFM_PRINT_FIELD, SFM_PRINT_FIELD)
#undef SFM_PRINT_FIELD
}

void printOptionLine(FILE* out, const std::string& flag, const std::string& value,
                     const std::string& help) {
    printOption(out, flag, "", value, help, "");
}

// ---------------------------------------------------------------------------
// stageSignature
// ---------------------------------------------------------------------------

namespace {

// Flags of a stage that cannot change what it writes: how fast it goes, on
// which device, how much it says about it.
bool signatureRelevant(const char* name) {
    for (const char* n : {"threads", "decode-threads", "decode-budget", "device",
                          "quiet", "profile", "spv-path"})
        if (std::strcmp(name, n) == 0) return false;
    return true;
}

// Left out while unset, so features extracted before the flag existed still
// match a run that does not use it.
bool signatureOmitsUnset(const char* name) {
    return std::strcmp(name, "image-exposure") == 0;
}

}  // namespace

std::string stageSignature(const SfmConfig& cfg, uint32_t cmd,
                           const std::string& workspace) {
    std::string out;
    auto appendParams = [&](const std::vector<double>& params) {
        char buf[32];
        for (double p : params) {
            std::snprintf(buf, sizeof buf, "%.17g,", p);
            out += buf;
        }
    };
    auto fieldValue = [&](const char* name, std::string val) {
        if (!workspace.empty() && !val.empty() && val != "none" &&
            std::strcmp(name, "masks") == 0)
            return spirula::relative_to_dir(val, workspace);
        return val;
    };
#define SFM_SIG_FIELD(member, name, cmds, tier, group, lo, hi, choices, help)   \
    if (((uint32_t)(cmds) & cmd) && (tier) != Tier::Alias && signatureRelevant(name) && \
        !(signatureOmitsUnset(name) && valueString(cfg.member) == "none"))              \
        out += std::string(name) + "=" + fieldValue(name, valueString(cfg.member)) + "\n";
    SFM_CONFIG_FIELDS(SFM_SIG_FIELD, SFM_SIG_FIELD)
#undef SFM_SIG_FIELD
    if ((cmd & CMD_EXTRACT) && !cfg.image_colors.empty())
        out += "image-colors=" + spirula::write_image_colors(cfg.image_colors) + "\n";
    // Per-group lenses reach the camera setup, and so verification, without
    // being table rows: `--camera-model cam0=opencv-fisheye` and the manifest
    // both land here.
    if (cmd & (CMD_MATCH | CMD_MAP)) {
        if (!cfg.camera.params.empty()) {
            out += "camera-params=";
            appendParams(cfg.camera.params);
            out += "\n";
        }
        for (const CameraOverride& o : cfg.camera.overrides) {
            out += "override " + o.prefix + "=";
            if (o.has_model) out += camInfo(o.model).cli_name;
            if (o.has_focal) out += "," + valueString(o.focal);
            for (double e : o.extra) out += "," + valueString(e);
            if (!o.params.empty()) {
                out += ";params=";
                appendParams(o.params);
            }
            out += "\n";
        }
    }
    // Verification reads the gyro and GPS proposes pairs. Only what is held
    // back is listed, so a state matched before the switches existed still fits.
    if (cmd & (CMD_MATCH | CMD_MAP)) {
        for (const TelemetryInput& t : cfg.telemetry_inputs)
            if (!t.imu || !t.gps)
                out += "telemetry " + t.prefix + (t.imu ? "" : " no-imu") + (t.gps ? "" : " no-gps") +
                       "\n";
        for (const std::string& p : cfg.exif_gps_off) out += "exif-gps-off " + p + "\n";
    }
    // A sequence adds its temporal window to the pair list.
    if (cmd & (CMD_MATCH | CMD_MAP))
        for (const SequenceDef& d : cfg.sequences) {
            out += "sequence ";
            for (size_t m = 0; m < d.members.size(); m++)
                out += (m ? "," : "") + d.members[m];
            out += "\n";
        }
    // A rig adds its mates to the pairs that verified (sfm/feature/RigPairs.h):
    // which images share a frame, and which way each lens faces.
    if ((cmd & CMD_MAP) || ((cmd & CMD_MATCH) && cfg.rig_pairs))
        for (const RigDef& d : cfg.rigs) {
            out += "rig " + d.kind + ":";
            for (size_t c = 0; c < d.captures.size(); c++)
                out += (c ? "," : "") + d.captures[c];
            for (const RigMemberDef& m : d.members) {
                out += " " + m.prefix;
                if (!m.has_ext) continue;
                out += "=";
                appendParams(std::vector<double>(m.ext.R.begin(), m.ext.R.end()));
            }
            out += "\n";
        }
    return out;
}

}  // namespace sfm

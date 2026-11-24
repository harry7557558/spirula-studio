// SfmOptions.cpp -- see SfmOptions.h.

#include "app/gui/SfmOptions.h"

#ifdef SS_TOOL_SFM
#include "sfm/SfmConfig.h"
#endif

#include <algorithm>
#include <cstring>
#include <iterator>

namespace gui {

std::string sfm_detector_owner(int features) {
    const std::string f = sfm_pick(kSfmFeatures, features);
    return f.rfind("aliked", 0) == 0 ? "aliked" : f.rfind("loma", 0) == 0 ? "loma" : "sift";
}

std::string sfm_matcher_owner(int features, int matcher) {
    const std::string m = sfm_matcher_for(features, matcher);
    return m.rfind("loma", 0) == 0 ? "loma" : m;
}

std::string sfm_mapper_owner(int mapper) { return sfm_pick(kSfmMapper, mapper); }

const char* sfm_feature_count_flag(int features) {
    const std::string owner = sfm_detector_owner(features);
    return owner == "aliked" ? "aliked-max-features"
           : owner == "loma" ? "loma-max-features"
                             : "max-features";
}

std::string sfm_option_field(const std::string& flag) {
    std::string key = flag;
    std::replace(key.begin(), key.end(), '-', '_');
    return key;
}

void append_sfm_option_args(const SfmOptionMap& options, std::vector<std::string>& argv) {
    for (const auto& [flag, value] : options) {
        SfmOption row;
        const bool known = find_sfm_option(flag, row);
        if (known && row.type == SfmOptionType::Switch) {
            argv.push_back((value == "on" ? "--" : "--no-") + flag);
            continue;
        }
        argv.push_back("--" + flag);
        argv.push_back(value);
    }
}

std::string sfm_option_value(const SfmReconSettings& s, const std::string& flag) {
    const auto it = s.options.find(flag);
    return it != s.options.end() ? it->second : sfm_option_default(s, flag);
}

void set_sfm_option(SfmReconSettings& s, const std::string& flag, const std::string& value) {
    if (value == sfm_option_default(s, flag)) s.options.erase(flag);
    else s.options[flag] = value;
}

#ifdef SS_TOOL_SFM

namespace {

SfmOption row_of(const sfm::FieldInfo& f) {
    SfmOption o;
    o.flag = f.name;
    o.type = f.type == sfm::FieldType::Switch    ? SfmOptionType::Switch
             : f.type == sfm::FieldType::Integer ? SfmOptionType::Integer
             : f.type == sfm::FieldType::Real    ? SfmOptionType::Real
                                                 : SfmOptionType::Text;
    o.lo = f.lo;
    o.hi = f.hi;
    for (std::string rest = f.choices; !rest.empty();) {
        const size_t bar = rest.find('|');
        o.choices.push_back(rest.substr(0, bar));
        rest = bar == std::string::npos ? std::string() : rest.substr(bar + 1);
    }
    o.help = f.help;
    o.owner = sfm::fieldOwner(f);
    const uint32_t stage = sfm::fieldStage(f);
    o.part = stage == sfm::CMD_EXTRACT ? ModelPart::Features
             : stage == sfm::CMD_MATCH ? ModelPart::Matching
                                       : ModelPart::Mapping;
    return o;
}

bool offered(const sfm::FieldInfo& f) {
    return (f.cmds & sfm::CMD_AUTO) && f.tier != sfm::Tier::Alias;
}

// The table's values under these three choices. One entry per thread: the
// panel asks for the same choices many times a frame.
const sfm::SfmConfig& defaults_for(const SfmReconSettings& s) {
    struct Cache {
        int quality = -1, features = -1, matcher = -1;
        sfm::SfmConfig cfg;
    };
    thread_local Cache c;
    if (c.quality != s.quality || c.features != s.features || c.matcher != s.matcher) {
        c.cfg = sfm::SfmConfig();
        c.cfg.quality = sfm_pick(kSfmQuality, s.quality, 2);
        c.cfg.features = sfm_pick(kSfmFeatures, s.features);
        c.cfg.matcher = sfm_matcher_for(s.features, s.matcher);
        std::vector<sfm::PresetChange> moved;
        sfm::applyPresets(c.cfg, {}, moved);
        c.quality = s.quality;
        c.features = s.features;
        c.matcher = s.matcher;
    }
    return c.cfg;
}

bool owner_selected(const SfmReconSettings& s, const std::string& owner) {
    if (owner.empty()) return true;
    const int pairs = sfm_pairs(s.pairs);
    // Sequential pairing reads the shortlist for its loop closure.
    if (owner == "prefilter")
        return pairs == kPairsPrefilter || (pairs == kPairsSequential && s.loop_closure);
    return owner == sfm_detector_owner(s.features) ||
           owner == sfm_matcher_owner(s.features, s.matcher) ||
           owner == sfm_mapper_owner(s.mapper);
}

}  // namespace

std::vector<SfmOption> sfm_owned_options(const std::string& owner) {
    std::vector<SfmOption> out;
    for (const sfm::FieldInfo& f : sfm::configFields())
        if (offered(f) && sfm::fieldOwner(f) == owner) out.push_back(row_of(f));
    return out;
}

std::vector<SfmOption> sfm_mapper_options(int mapper) {
    // SfmRunner::recon_args writes these from the panel's own controls, and
    // the telemetry goes in the manifest.
    static const char* const kElsewhere[] = {
        "mapper", "refine-extra-params", "final-extra-params", "final-per-image-intrinsics",
        "ba-real", "ba-real-coarse", "metric-gps", "sensor-gauge", "exif-attitude",
        "telemetry",
    };
    const std::string schedule = sfm_mapper_owner(mapper);
    std::vector<SfmOption> out;
    for (const sfm::FieldInfo& f : sfm::configFields()) {
        const std::string group = f.group;
        if (!offered(f) || group.substr(0, group.find('/')) != "mapper") continue;
        const std::string owner = sfm::fieldOwner(f);
        if (!owner.empty() && owner != schedule) continue;
        if (std::any_of(std::begin(kElsewhere), std::end(kElsewhere),
                        [&](const char* n) { return std::strcmp(n, f.name) == 0; }))
            continue;
        out.push_back(row_of(f));
    }
    return out;
}

bool find_sfm_option(const std::string& flag, SfmOption& out) {
    const sfm::FieldInfo* f = sfm::findConfigField(flag);
    if (!f || !offered(*f)) return false;
    out = row_of(*f);
    return true;
}

std::string sfm_option_default(const SfmReconSettings& s, const std::string& flag) {
    std::string value;
    sfm::configFieldValue(defaults_for(s), flag, value);
    return value;
}

SfmOptionMap active_sfm_options(const SfmReconSettings& s) {
    SfmOptionMap out;
    for (const auto& [flag, value] : s.options) {
        const sfm::FieldInfo* f = sfm::findConfigField(flag);
        if (!f || !offered(*f) || !owner_selected(s, sfm::fieldOwner(*f))) continue;
        if (!sfm::checkConfigValue(*f, value).empty()) continue;
        if (value == sfm_option_default(s, flag)) continue;
        out[flag] = value;
    }
    return out;
}

void sanitize_sfm_options(SfmOptionMap& options) {
    for (auto it = options.begin(); it != options.end();) {
        const sfm::FieldInfo* f = sfm::findConfigField(it->first);
        const bool ok = f && offered(*f) && sfm::checkConfigValue(*f, it->second).empty();
        it = ok ? std::next(it) : options.erase(it);
    }
}

bool sfm_option_part(const std::string& field, ModelPart& part) {
    std::string flag = field;
    std::replace(flag.begin(), flag.end(), '_', '-');
    SfmOption row;
    if (!find_sfm_option(flag, row)) return false;
    part = row.part;
    return true;
}

#else  // no reconstruction module: no table to check against

std::vector<SfmOption> sfm_owned_options(const std::string&) { return {}; }
std::vector<SfmOption> sfm_mapper_options(int) { return {}; }
bool find_sfm_option(const std::string&, SfmOption&) { return false; }
std::string sfm_option_default(const SfmReconSettings&, const std::string&) { return {}; }
SfmOptionMap active_sfm_options(const SfmReconSettings& s) { return s.options; }
void sanitize_sfm_options(SfmOptionMap&) {}
bool sfm_option_part(const std::string&, ModelPart&) { return false; }

#endif

}  // namespace gui

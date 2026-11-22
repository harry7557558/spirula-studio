#pragma once

// Flat JSON <-> TrainConfig, in one place.
//
// Three writers have to agree down to how an unset std::optional is spelled:
// a run's config.json (app/TrainerCore.cpp), --resume (checkpoint/Resume.cpp)
// and the GUI's saved presets (app/gui/TrainPreset.cpp).
//
// One key per flag, spelled as the flag is, flat -- never nested under the
// field table's `section`, which is presentational. Values encode as
// data/JsonField.h says. A key not in the table is ignored and a field with
// no key keeps its default, so a file from another build still loads; a key
// that IS in the table holding the wrong shape throws.

#include "config/TrainConfig.h"
#include "data/Json.h"
#include "data/JsonField.h"

#include <string>
#include <utility>
#include <vector>


// Every flag as (key, JSON value text), in field-table order. The caller
// decides the punctuation and the indent, because the three writers wrap it
// differently -- a run's config.json puts these at the top level, a preset
// file nests them under "config".
inline std::vector<std::pair<const char*, std::string>>
train_config_json_pairs(const TrainConfig& c) {
    std::vector<std::pair<const char*, std::string>> out;
#define SS_JSON_PAIR(type, member, default_, section, tier, choices)          \
    out.emplace_back(#member, json_field::emit(c.member));
    SS_CONFIG_FIELDS(SS_JSON_PAIR)
#undef SS_JSON_PAIR
    return out;
}

// The inverse. `root` is the object holding the flat keys; anything it does
// not name keeps whatever `out` already had, which is what lets a caller pass
// a preset-applied config as the baseline.
inline void train_config_from_json(const JsonValue& root, TrainConfig& out) {
#define SS_JSON_LOAD(type, member, default_, section, tier, choices)          \
    if (const JsonValue* v = root.find(#member))                              \
        json_field::assign(out.member, *v);
    SS_CONFIG_FIELDS(SS_JSON_LOAD)
#undef SS_JSON_LOAD
    train_upgrade_background_mode(out);
}

// Does this object name any training flag at all? The probe that tells a
// config.json (or a preset) from some other JSON file that was dropped on the
// window by mistake.
inline bool train_config_json_has_fields(const JsonValue& root) {
    if (!root.is_object()) return false;
    bool any = false;
#define SS_JSON_PROBE(type, member, default_, section, tier, choices)         \
    any = any || root.find(#member) != nullptr;
    SS_CONFIG_FIELDS(SS_JSON_PROBE)
#undef SS_JSON_PROBE
    return any;
}

// SfmPreset.cpp -- see SfmPreset.h.

#include "app/gui/SfmPreset.h"

#include "app/gui/SfmRunner.h"
#include "data/JsonField.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace gui {

namespace {

#define SS_SFM_PRESET_FIELDS(X)                                               \
    X("quality",        quality)                                              \
    X("features",       features)                                             \
    X("matcher",        matcher)                                              \
    X("max_features",   max_features)                                         \
    X("max_image_size", max_image_size)                                       \
    X("options",        options)                                              \
    /* end */

}  // namespace


SfmPreset capture_sfm_preset(const SfmJob& job) {
    SfmPreset p;
#define SS_SFM_TAKE(key, member) p.member = job.member;
    SS_SFM_PRESET_FIELDS(SS_SFM_TAKE)
#undef SS_SFM_TAKE
    return p;
}


void apply_sfm_preset(const SfmPreset& p, SfmJob& job) {
#define SS_SFM_GIVE(key, member) job.member = p.member;
    SS_SFM_PRESET_FIELDS(SS_SFM_GIVE)
#undef SS_SFM_GIVE
}


void save_sfm_preset(const SfmPreset& p, const std::string& path) {
    PresetHeader head{p.name, p.description, path};
    JsonWriter w = preset_writer(PresetKind::Sfm, head);
    w.key("settings").object();
#define SS_SFM_EMIT(key, member) w.field_raw(key, json_field::emit(p.member));
    SS_SFM_PRESET_FIELDS(SS_SFM_EMIT)
#undef SS_SFM_EMIT
    w.end();
    w.end();
    write_preset_file(path, w.str());
}


SfmPreset load_sfm_preset(const std::string& path) {
    PresetHeader head;
    const JsonValue root = read_preset_file(path, PresetKind::Sfm, head);
    const JsonValue* fields = root.find("settings");
    if (!fields || !fields->is_object())
        throw std::runtime_error(path + " holds no reconstruction settings");

    SfmPreset p;
    p.path = path;
    p.name = head.name;
    p.description = head.description;
#define SS_SFM_LOAD(key, member)                                              \
    if (const JsonValue* v = fields->find(key)) json_field::assign(p.member, *v);
    SS_SFM_PRESET_FIELDS(SS_SFM_LOAD)
#undef SS_SFM_LOAD

    p.quality = std::clamp(p.quality, 0, (int)std::size(kSfmQuality) - 1);
    p.features = std::clamp(p.features, 0, (int)std::size(kSfmFeatures) - 1);
    p.matcher = std::clamp(p.matcher, 0, 1);
    p.max_features = std::max(p.max_features, 0);
    p.max_image_size = std::max(p.max_image_size, 0);
    if (p.name.empty()) p.name = std::filesystem::path(path).stem().string();
    return p;
}


std::vector<SfmPreset> list_sfm_presets() {
    return list_preset_files(PresetKind::Sfm, load_sfm_preset);
}

}  // namespace gui

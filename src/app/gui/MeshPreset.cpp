// MeshPreset.cpp -- see MeshPreset.h.

#include "app/gui/MeshPreset.h"

#include "data/JsonField.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace gui {

namespace {

// X(key in the file, member of MeshJob). `checkpoint`, `data_dir` and
// `output` are deliberately absent: they are what the preset is applied TO.
#define SS_MESH_PRESET_FIELDS(X)                                              \
    X("use_data",          use_data)                                          \
    X("max_cameras",       max_cameras)                                       \
    X("tsdf",              tsdf)                                              \
    X("texture_size",      texture_size)                                      \
    X("iso",               iso)                                               \
    X("bisection_iters",   bisection_iters)                                   \
    X("merge_factor",      merge_factor)                                      \
    X("quality_iters",     quality_iters)                                     \
    X("floater_min_faces", floater_min_faces)                                 \
    X("cull_unseen",       cull_unseen)                                       \
    X("carve_k",           carve_k)                                           \
    X("extra_args",        extra_args)                                        \
    /* end */

// The two sets, written as arrays of the tokens the CLI spells rather than as
// indices: a preset file is meant to be readable, and an index that shifted
// would quietly mean something else.
void write_tokens(JsonWriter& w, const char* key, const bool* on,
                  const char* const* names, int n) {
    w.key(key).array();
    for (int i = 0; i < n; i++)
        if (on[i]) w.value(names[i]);
    w.end();
}

bool read_tokens(const JsonValue* v, bool* on, const char* const* names, int n) {
    if (!v || !v->is_array()) return false;
    for (int i = 0; i < n; i++) on[i] = false;
    for (const JsonValue& e : v->arr)
        for (int i = 0; i < n; i++)
            if (e.as_string() == names[i]) on[i] = true;
    return true;
}

}  // namespace


void sanitize_mesh_job(MeshJob& job) {
    job.max_cameras = std::clamp(job.max_cameras, 0, 1000000);
    job.texture_size = std::clamp(job.texture_size, 0, 16384);
    job.bisection_iters = std::clamp(job.bisection_iters, 0, 32);
    job.quality_iters = std::clamp(job.quality_iters, 0, 32);
    job.floater_min_faces = std::clamp(job.floater_min_faces, 0, 10000000);
    job.carve_k = std::clamp(job.carve_k, 0, 64);
    job.merge_factor = std::clamp(job.merge_factor, 0.05f, 16.0f);

    bool any_color = false, any_format = false;
    for (int i = 0; i < kNumMeshColorModes; i++) any_color |= job.colors[i];
    for (int i = 0; i < kNumMeshFormats; i++) any_format |= job.formats[i];
    if (!any_color) job.colors[1] = true;
    if (!any_format) job.formats[0] = true;
    // Every requested colour was ruled out by every requested format (texture
    // with PLY alone, say): add the format that carries each of them, rather
    // than starting a run whose whole output is nothing.
    if (!mesh_job_writes_nothing(job)) return;
    for (int i = 0; i < kNumMeshColorModes; i++)
        if (job.colors[i]) job.formats[i == 2 ? 3 : 0] = true;
}


void save_mesh_preset(const MeshPreset& p, const std::string& path) {
    PresetHeader head{p.name, p.description, path};
    JsonWriter w = preset_writer(PresetKind::Mesh, head);
    w.key("settings").object();
#define SS_MESH_EMIT(key, member) w.field_raw(key, json_field::emit(p.job.member));
    SS_MESH_PRESET_FIELDS(SS_MESH_EMIT)
#undef SS_MESH_EMIT
    write_tokens(w, "colors", p.job.colors, kMeshColorModes, kNumMeshColorModes);
    write_tokens(w, "formats", p.job.formats, kMeshFormats, kNumMeshFormats);
    w.end();
    w.end();
    write_preset_file(path, w.str());
}


MeshPreset load_mesh_preset(const std::string& path) {
    PresetHeader head;
    const JsonValue root = read_preset_file(path, PresetKind::Mesh, head);
    const JsonValue* fields = root.find("settings");
    if (!fields || !fields->is_object())
        throw std::runtime_error(path + " holds no meshing settings");

    MeshPreset p;
    p.path = path;
    p.name = head.name;
    p.description = head.description;
#define SS_MESH_LOAD(key, member)                                             \
    if (const JsonValue* v = fields->find(key))                               \
        json_field::assign(p.job.member, *v);
    SS_MESH_PRESET_FIELDS(SS_MESH_LOAD)
#undef SS_MESH_LOAD

    read_tokens(fields->find("formats"), p.job.formats, kMeshFormats,
                kNumMeshFormats);
    // A preset written before a run could carry more than one colour names a
    // single index; either spelling still loads.
    if (!read_tokens(fields->find("colors"), p.job.colors, kMeshColorModes,
                     kNumMeshColorModes)) {
        if (const JsonValue* v = fields->find("color")) {
            const int one = (int)v->as_int(1);
            for (int i = 0; i < kNumMeshColorModes; i++) p.job.colors[i] = i == one;
        }
    }
    sanitize_mesh_job(p.job);
    if (p.name.empty()) p.name = std::filesystem::path(path).stem().string();
    return p;
}


void delete_mesh_preset(const std::string& path) {
    delete_preset_file(path, PresetKind::Mesh);
}


std::vector<MeshPreset> list_mesh_presets() {
    return list_preset_files(PresetKind::Mesh, load_mesh_preset);
}

}  // namespace gui

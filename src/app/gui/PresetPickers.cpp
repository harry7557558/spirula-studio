// Presets on screen: applying train, dataset and mesh presets, their pickers,
// and the save and delete dialogs.

#include "app/gui/GuiApp.h"

#include "app/gui/DatasetPrep.h"
#include "app/gui/GuiAppShared.h"
#include "app/gui/Ui.h"
#include "data/RoiDocument.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/Gui.h"
#include "i18n/catalog/Train.h"

#include "imgui.h"

#include <filesystem>

namespace fs = std::filesystem;
namespace i18n = spirula::i18n;
namespace msg = spirula::i18n::msg::gui;
namespace dmsg = spirula::i18n::msg::dataset;

namespace gui {

std::string preset_label(const std::string& name) {
    const auto* t = spirula::i18n::msg::train::preset_text(name.c_str());
    if (!t) return name;
    return std::string(t->label->get()) + " (" + name + ")";
}

std::string preset_help(const std::string& name) {
    const auto* t = spirula::i18n::msg::train::preset_text(name.c_str());
    return t ? t->help->get() : "";
}

void preset_hover(const std::string& description, const std::string& path) {
    if (description.empty() && path.empty()) return;
    if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) ||
        !ImGui::BeginTooltip())
        return;
    ImGui::PushTextWrapPos(px(kTooltipWrapW));
    if (!description.empty()) ui::TextRaw(description);
    // A path is a path in every language, and not a sentence about the one
    // above it -- so it is its own line.
    if (!path.empty()) ui::TextDisabledRaw(path);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

void preset_hover(const TrainPreset& p) { preset_hover(p.description, p.path); }

std::vector<BuiltinRow> dataset_builtin_rows() {
    std::vector<BuiltinRow> out;
    for (const DatasetPresetInfo& p : kDatasetPresets) {
        const auto* t = i18n::msg::dataset::preset_text(p.name);
        BuiltinRow r;
        r.name = p.name;
        r.label = t ? std::string(t->label->get()) + " (" + p.name + ")" : p.name;
        r.help = t ? std::string(t->help->get()) : std::string();
        out.push_back(std::move(r));
    }
    return out;
}

template <class T>
PresetChoice preset_combo(const char* id, const PresetPicker<T>& pick,
                          const std::string& file, const std::string& builtin,
                          const std::vector<BuiltinRow>& builtins,
                          bool offer_file) {
    PresetChoice out;
    std::string preview, desc;
    if (!file.empty()) {
        // A file outside the saved folder still shows as itself.
        preview = fs::path(file).stem().string();
        for (const T& p : pick.items)
            if (p.path == file) { preview = p.name; desc = p.description; }
    } else if (builtins.empty()) {
        preview = msg::batch_preset_stock.get();
    } else {
        // Nothing selected (a deleted preset, a fresh session) is the first
        // built-in, which is that kind's base settings.
        preview = builtins.front().label;
        desc = builtins.front().help;
        for (const BuiltinRow& b : builtins)
            if (b.name == builtin) { preview = b.label; desc = b.help; }
    }
    const bool open = ui::BeginComboRaw(id, preview.c_str());
    // On the closed combo too: a row shows only a name, and two saved presets
    // can share one -- the path in the tooltip is what tells them apart.
    preset_hover(desc, file);
    if (!open) return out;

    if (builtins.empty()) {
        if (ui::Selectable(msg::batch_preset_stock, file.empty())) out.moved = true;
    } else {
        ui::SeparatorText(msg::preset_builtin_group);
        for (const BuiltinRow& b : builtins) {
            const bool sel = file.empty() &&
                             (b.name == builtin ||
                              (builtin.empty() && b.name == builtins.front().name));
            if (ui::SelectableRaw(b.label, sel)) {
                out.moved = true;
                out.builtin = b.name;
            }
            preset_hover(b.help, {});
            if (sel) ImGui::SetItemDefaultFocus();
        }
    }
    ui::SeparatorText(msg::preset_user_group);
    if (pick.items.empty()) ui::TextDisabled(msg::preset_none_saved);
    for (size_t i = 0; i < pick.items.size(); i++) {
        const T& p = pick.items[i];
        ImGui::PushID((int)i);
        const bool sel = !file.empty() && file == p.path;
        if (ui::SelectableRaw(p.name, sel)) {
            out.moved = true;
            out.index = (int)i;
        }
        preset_hover(p.description, p.path);
        if (sel) ImGui::SetItemDefaultFocus();
        ImGui::PopID();
    }
    if (offer_file) {
        ImGui::Separator();
        if (ui::Selectable(msg::batch_preset_from_file)) {
            out.moved = true;
            out.from_file = true;
        }
    }
    ImGui::EndCombo();
    return out;
}

template PresetChoice preset_combo<DatasetPreset>(const char*, const PresetPicker<DatasetPreset>&,
                                                  const std::string&, const std::string&,
                                                  const std::vector<BuiltinRow>&, bool);
template PresetChoice preset_combo<MeshPreset>(const char*, const PresetPicker<MeshPreset>&,
                                               const std::string&, const std::string&,
                                               const std::vector<BuiltinRow>&, bool);

void GuiApp::apply_preset(const std::string& preset) {
    if (native_work_busy() || _batch_active) return;
    TrainConfig fresh;
    train_apply_preset(fresh, preset);
    // Keep GUI-managed context across preset switches.
    fresh.data = _cfg.data;
    fresh.image_dir = _cfg.image_dir;
    fresh.seed_pointcloud = _cfg.seed_pointcloud;
    fresh.output_dir_prefix = _cfg.output_dir_prefix;
    fresh.output_dir_name = _cfg.output_dir_name;
    fresh.roi_region = _cfg.roi_region;
    // The native viewport replaces the web viewer by default; it can be
    // re-enabled in Basic Options for remote monitoring.
    fresh.disable_viewer = true;
    fresh.save_full_checkpoint = _keep_full_ckpt;
    _preset = preset;
    _train_presets.file.clear();
    _train_presets.display.clear();
    _train_presets.desc.clear();
    _train_presets.msg.clear();   // whatever it reported was about the last preset
    _defaults = fresh;
    // Not a preset value, so it stays out of _defaults and shows as edited.
    fresh.finish_command = _cfg.finish_command;
    _cfg = fresh;
    _cfg_ui.touched.clear();
    if (!_cfg.data.empty()) {
        detach_session_views();
        _runner.load_dataset(_cfg, _preset);
    }
}

// A saved preset carries the whole config, so this replaces more than a
// built-in preset does -- but the same five fields stay out of its reach
// (SS_PRESET_CONTEXT_FIELDS), because a preset is "how", not "where".
void GuiApp::apply_user_preset(const TrainPreset& p) {
    if (native_work_busy() || _batch_active) return;
    const TrainConfig stock;
    TrainConfig fresh = p.cfg;
    fresh.data = _cfg.data;
    fresh.resume = _cfg.resume;
    fresh.output_dir_prefix = _cfg.output_dir_prefix;
    fresh.output_dir_name = _cfg.output_dir_name;
    fresh.roi_region = _cfg.roi_region;
    // A preset that never moved the image and mask folders must not move them:
    // the handoff out of a reconstruction points them at folders only this
    // dataset has.
    if (fresh.image_dir == stock.image_dir) fresh.image_dir = _cfg.image_dir;
    if (fresh.mask_dir == stock.mask_dir) fresh.mask_dir = _cfg.mask_dir;
    fresh.disable_viewer = true;   // as apply_preset: the native viewport
    fresh.save_full_checkpoint = p.cfg.save_full_checkpoint || _keep_full_ckpt;

    _preset = p.base;
    _train_presets.file = p.path;
    _train_presets.display = p.name;
    _train_presets.desc = p.description;
    _train_presets.msg.clear();   // whatever it reported was about the last preset
    _defaults = fresh;
    fresh.finish_command = _cfg.finish_command;   // as apply_preset
    _cfg = fresh;
    // What the preset spelled out is off limits to the macro options, exactly
    // as if the user had just typed it (train_resolve_macros).
    _cfg_ui.touched = p.touched;
    if (!_cfg.data.empty()) {
        detach_session_views();
        _runner.load_dataset(_cfg, _preset);
    }
}

void GuiApp::load_preset_file(const std::string& path) {
    if (path.empty()) return;
    // Applying a preset re-parses the dataset, which takes a live session
    // down with it. Refuse while native work is running.
    if (native_work_busy() || _batch_active) {
        _train_presets.msg = dmsg::log_drop_while_training.get();
        _train_presets.msg_err = true;
        log(_train_presets.msg);
        return;
    }
    try {
        TrainPreset p = load_preset(path);
        apply_user_preset(p);
        _train_presets.msg = i18n::format(msg::preset_loaded, {p.name});
        _train_presets.msg_err = false;
    } catch (const std::exception& e) {
        _train_presets.msg = i18n::format(msg::preset_failed, {e.what()});
        _train_presets.msg_err = true;
    }
    log(_train_presets.msg);
    refresh_presets();
}

void GuiApp::refresh_presets() {
    // The picker rescans while its dropdown is open, so rate-limit: a scan is
    // a directory listing plus a small JSON parse per file, which is cheap
    // twice a second and silly at 120 Hz.
    double now = ImGui::GetTime();
    if (_train_presets.scanned_at >= 0.0 && now - _train_presets.scanned_at < 2.0) return;
    _train_presets.scanned_at = now;
    _train_presets.items = list_presets();
}

void GuiApp::refresh_dataset_presets() {
    double now = ImGui::GetTime();
    if (_ds_presets.scanned_at >= 0.0 && now - _ds_presets.scanned_at < 2.0) return;
    _ds_presets.scanned_at = now;
    _ds_presets.items = list_dataset_presets();
}

void GuiApp::refresh_mesh_presets() {
    double now = ImGui::GetTime();
    if (_mesh_presets.scanned_at >= 0.0 && now - _mesh_presets.scanned_at < 2.0) return;
    _mesh_presets.scanned_at = now;
    _mesh_presets.items = list_mesh_presets();
}


// ---------------------------------------------------------------------------
// Dataset and meshing presets
//
// A preset carries the "how" (DatasetPreset.h); the inputs, the output folder,
// the clicks and the stencils stay where they are.
// ---------------------------------------------------------------------------

DatasetSettings GuiApp::capture_dataset_settings() const {
    DatasetSettings s;
    s.colmap_engine = effective_engine() == Engine::Colmap;
    s.sfm = _sfm_job;
    s.colmap = _colmap_job;
    s.mask = _mask;
    s.mask_model_id = _model_id;
    s.mask_detector_id = _mask_detector_id;
    s.use_found_masks = _use_found_masks;
    s.border_enable = _border_enable;
    s.frame_shapes = _frame_shapes;
    // The panel-level copies are what the user edits; sync_dataset_jobs() fans
    // them out, and a preset has to carry what was edited rather than what a
    // sync happened to leave behind.
    s.sfm.prep.resume = _resume;
    s.sfm.prep.photo_import = _photo_import;
    s.sfm.prep.flip_found_masks = _flip_found_masks;
    s.sfm.prep.mask_enable = _mask_enable;
    s.sfm.prep.mask_memory = _mask_memory;
    s.sfm.prep.mask_detect_every = _mask_detect_every;
    s.sfm.prep.mask_memory_frames = _mask_memory_frames;
    s.sfm.mask_features = _mask_features;
    s.sfm.geometry = _geometry;
    s.sfm.dense = _dense;
    bool any_video = false, in_order = true;
    for (const PrepInput& in : _sources) {
        any_video = any_video || in.is_video;
        in_order = in_order && (!in.is_video || in.sequential);
    }
    s.videos_in_order = any_video && in_order;
    return s;
}

void GuiApp::apply_dataset_settings(const DatasetSettings& in) {
    DatasetSettings s = in;
    sanitize_dataset_settings(s);

    _engine = s.colmap_engine ? Engine::Colmap : Engine::BuiltIn;
    _sfm_job = s.sfm;
    _colmap_job = s.colmap;
    // Clicks belong to the frames they were drawn on, so a preset neither
    // carries them nor is allowed to throw away the ones on screen.
    const std::vector<MaskClick> clicks = _mask.clicks;
    const int objects = _mask.object_count, current = _mask.current_object;
    _mask = s.mask;
    _mask.clicks = clicks;
    _mask.object_count = objects;
    _mask.current_object = current;
    if (find_model(s.mask_model_id)) _model_id = s.mask_model_id;
    if (find_detector(s.mask_detector_id)) _mask_detector_id = s.mask_detector_id;
    _use_found_masks = s.use_found_masks;
    _border_enable = s.border_enable;
    _frame_shapes = s.frame_shapes;
    apply_frame_shapes(_sources, _frame_shapes);
    _resume = s.sfm.prep.resume;
    _photo_import = s.sfm.prep.photo_import;
    _flip_found_masks = s.sfm.prep.flip_found_masks;
    _mask_enable = s.sfm.prep.mask_enable && backends().builtin_masking;
    _mask_memory = s.sfm.prep.mask_memory;
    _mask_detect_every = s.sfm.prep.mask_detect_every;
    _mask_memory_frames = s.sfm.prep.mask_memory_frames;
    _mask_features = s.sfm.mask_features;
    _geometry = s.sfm.geometry;
    _dense = s.sfm.dense;
    _dense_config_error.clear();
    resolve_source_lenses(_sources, _sfm_job, _colmap_job);
    // A preset answers "how", never "where": refresh_sources() re-derives the
    // output folder whenever the user has not taken it over, and a fresh one
    // beside the last would silently orphan the run already in it.
    const std::string ws = _workspace, ws_auto = _workspace_auto;
    rescan_found_masks();
    _workspace = ws;
    _workspace_auto = ws_auto;
}

void GuiApp::apply_dataset_preset(const DatasetPreset& p) {
    apply_dataset_settings(p.s);
    // The rows' Sequential switches, as a batch row reads them too.
    for (PrepInput& in : _sources)
        if (in.is_video) in.sequential = p.s.videos_in_order;
    _ds_presets.file = p.path;
    _ds_presets.builtin.clear();
    _ds_presets.display = p.name;
    _ds_presets.desc = p.description;
    _ds_presets.msg.clear();
}

// A built-in is the capture's own answers with the preset's over them, and
// then the one question only the frames can settle -- which is why it is not
// simply a DatasetSettings the way a saved one is.
void GuiApp::apply_dataset_builtin(const std::string& name) {
    const std::string use = name.empty() ? std::string("general") : name;
    DatasetSettings s;
    apply_capture_defaults(_sources, s.sfm, s.colmap);
    if (!dataset_apply_preset(s, use)) return;
    dataset_adapt_preset(use, _sources, s.sfm, s.colmap, _ffmpeg_exe);
    apply_dataset_settings(s);
    _ds_presets.file.clear();
    _ds_presets.builtin = use;
    _ds_presets.display.clear();
    _ds_presets.desc.clear();
    _ds_presets.msg.clear();
}

// The capture's own defaults have just moved settings a built-in preset
// decided, so it goes back on top of them and asks the new frames its own
// question. Only a built-in: re-applying a FILE would undo every edit since.
void GuiApp::reapply_dataset_builtin() {
    if (!_ds_presets.file.empty() || _ds_presets.builtin.empty()) return;
    DatasetSettings s = capture_dataset_settings();
    if (dataset_apply_preset(s, _ds_presets.builtin)) apply_dataset_settings(s);
    dataset_adapt_preset(_ds_presets.builtin, _sources, _sfm_job, _colmap_job,
                         _ffmpeg_exe);
}

void GuiApp::load_dataset_preset_file(const std::string& path) {
    if (path.empty()) return;
    if (dataset_busy()) {
        _ds_presets.msg = dmsg::log_drop_while_training.get();
        _ds_presets.msg_err = true;
        log(_ds_presets.msg);
        return;
    }
    try {
        DatasetPreset p = load_dataset_preset(path);
        apply_dataset_preset(p);
        _ds_presets.msg = i18n::format(msg::preset_loaded, {p.name});
        _ds_presets.msg_err = false;
    } catch (const std::exception& e) {
        _ds_presets.msg = i18n::format(msg::preset_failed, {e.what()});
        _ds_presets.msg_err = true;
    }
    log(_ds_presets.msg);
    _ds_presets.scanned_at = -1.0;
    refresh_dataset_presets();
}

void GuiApp::apply_mesh_preset(const MeshPreset& p) {
    const std::string checkpoint = _mesh_job.checkpoint;
    const std::string data_dir = _mesh_job.data_dir;
    const std::string output = _mesh_job.output;
    _mesh_job = p.job;
    _mesh_job.checkpoint = checkpoint;
    _mesh_job.data_dir = data_dir;
    _mesh_job.output = output;
    sanitize_mesh_job(_mesh_job);
    _mesh_presets.file = p.path;
    _mesh_presets.display = p.name;
    _mesh_presets.desc = p.description;
    _mesh_presets.msg.clear();
    _mesh_data_probe_key.clear();   // use_data may have moved
}

void GuiApp::load_mesh_preset_file(const std::string& path) {
    if (path.empty()) return;
    if (_mesh.busy()) {
        _mesh_presets.msg = dmsg::log_drop_while_training.get();
        _mesh_presets.msg_err = true;
        log(_mesh_presets.msg);
        return;
    }
    try {
        MeshPreset p = load_mesh_preset(path);
        apply_mesh_preset(p);
        _mesh_presets.msg = i18n::format(msg::preset_loaded, {p.name});
        _mesh_presets.msg_err = false;
    } catch (const std::exception& e) {
        _mesh_presets.msg = i18n::format(msg::preset_failed, {e.what()});
        _mesh_presets.msg_err = true;
    }
    log(_mesh_presets.msg);
    _mesh_presets.scanned_at = -1.0;
    refresh_mesh_presets();
}

// Seed the shared save dialog from what is on screen: a saved preset being
// adjusted usually wants to be saved back over itself.
void GuiApp::open_preset_save(PresetKind kind) {
    _preset_save_kind = kind;
    switch (kind) {
        case PresetKind::Dataset:
            _preset_save_name = _ds_presets.display;
            _preset_save_desc = _ds_presets.desc;
            _preset_save_path = _ds_presets.file;
            break;
        case PresetKind::Mesh:
            _preset_save_name = _mesh_presets.display;
            _preset_save_desc = _mesh_presets.desc;
            _preset_save_path = _mesh_presets.file;
            break;
        default:
            _preset_save_name = _train_presets.display;
            _preset_save_desc = _train_presets.desc;
            _preset_save_path = _train_presets.file;
            break;
    }
    _preset_path_edited = !_preset_save_path.empty();
    _preset_save_open = true;
}


// The dataset screen's picker. Picking a built-in is a reset as much as a
// choice: it starts from the settings a freshly picked capture would have had.
void GuiApp::draw_dataset_preset_picker() {
    refresh_dataset_presets();
    ImGui::SetNextItemWidth(px(-8.0f));
    const PresetChoice picked =
        preset_combo("##dspreset", _ds_presets, _ds_presets.file,
                     _ds_presets.builtin, dataset_builtin_rows());
    if (picked.moved) {
        if (picked.index >= 0 && picked.index < (int)_ds_presets.items.size())
            apply_dataset_preset(_ds_presets.items[(size_t)picked.index]);
        else
            apply_dataset_builtin(picked.builtin);
    }

    if (ui::Button(msg::preset_save)) open_preset_save(PresetKind::Dataset);
    ui::help_on_hover(msg::preset_save_help);
    ImGui::SameLine();
    if (ui::Button(msg::preset_load))
        open_pick(PickAction::DatasetPresetFile, msg::preset_pick_file.get(),
                  FileDialog::Mode::File, {".json"});
    ui::help_on_hover(msg::preset_load_help_plain);
    if (!_ds_presets.file.empty()) {
        ImGui::SameLine();
        if (ui::Button(msg::preset_delete)) {
            _preset_delete_kind = PresetKind::Dataset;
            _preset_delete_open = true;
        }
        ui::help_on_hover(msg::preset_delete_help);
    }
    if (!_ds_presets.msg.empty())
        ui::TextColoredWrappedRaw(_ds_presets.msg_err ? kErr : kOk, _ds_presets.msg);
    else if (!_ds_presets.desc.empty())
        ui::TextColoredWrappedRaw(kDim, _ds_presets.desc);
    else
        ui::TextColoredWrapped(kDim, msg::preset_drop_hint_plain);
}


void GuiApp::draw_mesh_preset_picker() {
    refresh_mesh_presets();
    ImGui::SetNextItemWidth(px(-8.0f));
    const PresetChoice picked = preset_combo("##meshpreset", _mesh_presets,
                                             _mesh_presets.file, {}, {});
    if (picked.moved) {
        if (picked.index >= 0 && picked.index < (int)_mesh_presets.items.size()) {
            apply_mesh_preset(_mesh_presets.items[(size_t)picked.index]);
        } else {
            MeshPreset stock;
            apply_mesh_preset(stock);
            _mesh_presets.file.clear();
            _mesh_presets.display.clear();
            _mesh_presets.desc.clear();
            _mesh_presets.msg.clear();
        }
    }

    if (ui::Button(msg::preset_save)) open_preset_save(PresetKind::Mesh);
    ui::help_on_hover(msg::preset_save_help);
    ImGui::SameLine();
    if (ui::Button(msg::preset_load))
        open_pick(PickAction::MeshPresetFile, msg::preset_pick_file.get(),
                  FileDialog::Mode::File, {".json"});
    ui::help_on_hover(msg::preset_load_help_plain);
    if (!_mesh_presets.file.empty()) {
        ImGui::SameLine();
        if (ui::Button(msg::preset_delete)) {
            _preset_delete_kind = PresetKind::Mesh;
            _preset_delete_open = true;
        }
        ui::help_on_hover(msg::preset_delete_help);
    }
    if (!_mesh_presets.msg.empty())
        ui::TextColoredWrappedRaw(_mesh_presets.msg_err ? kErr : kOk,
                                  _mesh_presets.msg);
    else if (!_mesh_presets.desc.empty())
        ui::TextColoredWrappedRaw(kDim, _mesh_presets.desc);
}


// The built-in NAME is the command line's word for it and is not translated:
// the picker shows the label from i18n/catalog/Train.h with the name beside
// it, so a user who read the README still recognises the row they want.
bool GuiApp::draw_preset_picker() {
    static_assert(sizeof(kTrainPresets) / sizeof(kTrainPresets[0]) ==
                      i18n::msg::train::kNumPresetText,
                  "config/TrainConfig.h and i18n/catalog/Train.h disagree "
                  "about how many presets there are");

    bool applied = false;
    const std::string preview =
        _train_presets.file.empty() ? preset_label(_preset) : _train_presets.display;
    ImGui::SetNextItemWidth(px(-8.0f));
    const bool combo_open = ui::BeginComboRaw("##preset", preview.c_str());
    // On the closed combo too: two saved presets can share a name, and this
    // is where the one in use says which file it is.
    preset_hover(_train_presets.file.empty() ? preset_help(_preset) : _train_presets.desc,
                 _train_presets.file);
    if (combo_open) {
        refresh_presets();
        ui::SeparatorText(msg::preset_builtin_group);
        for (const auto& p : kTrainPresets) {
            bool sel = _train_presets.file.empty() && _preset == p.name;
            if (ui::SelectableRaw(preset_label(p.name), sel)) {
                apply_preset(p.name);
                applied = true;
            }
            preset_hover(preset_help(p.name), {});
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ui::SeparatorText(msg::preset_user_group);
        if (_train_presets.items.empty()) ui::TextDisabled(msg::preset_none_saved);
        for (const auto& p : _train_presets.items) {
            ImGui::PushID(p.path.c_str());
            bool sel = _train_presets.file == p.path;
            if (ui::SelectableRaw(p.name, sel)) {
                apply_user_preset(p);
                applied = true;
            }
            preset_hover(p);
            if (sel) ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ui::TextColoredWrappedRaw(
        kDim, _train_presets.file.empty() ? preset_help(_preset) : _train_presets.desc);

    if (ui::Button(msg::preset_save)) open_preset_save(PresetKind::Train);
    ui::help_on_hover(msg::preset_save_help);
    ImGui::SameLine();
    if (ui::Button(msg::preset_load)) {
        open_pick(PickAction::PresetFile, msg::preset_pick_file.get(),
                  FileDialog::Mode::File, {".json"});
    }
    ui::help_on_hover(msg::preset_load_help);

    // Only for a saved preset: there is nothing to delete for a built-in one.
    if (!_train_presets.file.empty()) {
        ImGui::SameLine();
        if (ui::Button(msg::preset_delete)) {
            _preset_delete_kind = PresetKind::Train;
            _preset_delete_open = true;
        }
        ui::help_on_hover(msg::preset_delete_help);
    }

    if (!_train_presets.msg.empty())
        ui::TextColoredWrappedRaw(_train_presets.msg_err ? kErr : kOk, _train_presets.msg);
    else
        ui::TextColoredWrapped(kDim, msg::preset_drop_hint);
    return applied;
}

// Deleting removes a file, so it asks first -- and names both the preset and
// the path, because two presets can share a name and only the path says which
// one is about to go.
void GuiApp::draw_preset_delete_modal() {
    if (_preset_delete_open) {
        ui::OpenPopup(msg::preset_delete_title);
        _preset_delete_open = false;
        _preset_delete_shown = true;
    }
    if (!_preset_delete_shown) return;

    if (!ui::BeginPopupModal(msg::preset_delete_title, nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
        _preset_delete_shown = false;
        return;
    }
    // The picker the modal was armed from; every kind deletes the same way.
    std::string* file = &_train_presets.file;
    std::string* display = &_train_presets.display;
    std::string* desc = &_train_presets.desc;
    std::string* out = &_train_presets.msg;
    bool* out_err = &_train_presets.msg_err;
    double* scanned = &_train_presets.scanned_at;
    if (_preset_delete_kind == PresetKind::Dataset) {
        file = &_ds_presets.file; display = &_ds_presets.display;
        desc = &_ds_presets.desc; out = &_ds_presets.msg;
        out_err = &_ds_presets.msg_err; scanned = &_ds_presets.scanned_at;
    } else if (_preset_delete_kind == PresetKind::Mesh) {
        file = &_mesh_presets.file; display = &_mesh_presets.display;
        desc = &_mesh_presets.desc; out = &_mesh_presets.msg;
        out_err = &_mesh_presets.msg_err; scanned = &_mesh_presets.scanned_at;
    }

    ImGui::PushTextWrapPos(px(kModalWrapW));
    ui::Text(msg::preset_delete_confirm, {*display});
    ui::TextDisabledRaw(*file);
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    if (ui::Button(msg::preset_delete_button, ImVec2(px(kModalButtonW), 0))) {
        const std::string path = *file, name = *display;
        try {
            delete_preset_file(path, _preset_delete_kind);
            // The options on screen stay exactly as they are -- what went is
            // the saved copy, not the work.
            file->clear();
            display->clear();
            desc->clear();
            *out = i18n::format(msg::preset_deleted, {name});
            *out_err = false;
            *scanned = -1.0;
            refresh_presets();
            refresh_dataset_presets();
            refresh_mesh_presets();
            // Rows pointing at the file that just went would fail at launch;
            // the next check says so instead.
            _batch_checked = false;
        } catch (const std::exception& e) {
            *out = i18n::format(msg::preset_delete_failed, {e.what()});
            *out_err = true;
        }
        log(*out);
        _preset_delete_shown = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ui::Button(msg::cancel, ImVec2(px(kModalButtonW), 0))) {
        _preset_delete_shown = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void GuiApp::draw_preset_save_modal() {
    if (_preset_save_open) {
        ui::OpenPopup(msg::preset_save_title);
        _preset_save_open = false;
        _preset_save_shown = true;
    }
    if (!_preset_save_shown) return;

    ImGui::SetNextWindowSize(ImVec2(px(540.0f), 0), ImGuiCond_Appearing);
    if (!ui::BeginPopupModal(msg::preset_save_title, nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
        _preset_save_shown = false;   // dismissed with Esc / a click away
        return;
    }

    const float fw = 360.0f;
    ImGui::SetNextItemWidth(fw);
    // A preset name is the user's own words in the user's own language, so it
    // is not interface copy and not translated -- only the label is.
    ui::InputTextWithHint(msg::preset_name, msg::preset_name_hint,
                          &_preset_save_name);
    ImGui::SetNextItemWidth(fw);
    ui::InputTextWithHint(msg::preset_desc, msg::preset_desc_hint,
                          &_preset_save_desc);

    // The path follows the name until the user takes it over.
    if (!_preset_path_edited)
        _preset_save_path = (fs::path(preset_dir(_preset_save_kind)) /
                             preset_file_name(_preset_save_name))
                                .string();

    ImGui::Spacing();
    ImGui::SetNextItemWidth(fw);
    // A path is a path in every language.
    if (ui::InputTextRaw("##preset_path", &_preset_save_path))
        _preset_path_edited = true;
    ImGui::SameLine();
    if (ui::ButtonRaw("...##preset_dir")) {
        _preset_path_edited = true;
        open_pick(PickAction::PresetSaveFolder, msg::preset_pick_folder.get(),
                  FileDialog::Mode::Folder, {},
                  fs::path(_preset_save_path).parent_path().string());
        // ImGui shows one modal at a time and the dialog is one too, so this
        // one steps aside and is re-armed when the pick comes back. Nothing is
        // lost: the name, the description and the path are all state here.
        _preset_save_shown = false;
        _preset_save_reopen = true;
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    ImGui::SameLine();
    ui::Text(msg::preset_path_label);
    ui::help_on_hover(msg::preset_path_help);
    if (_preset_path_edited && ui::Button(msg::preset_use_default_folder))
        _preset_path_edited = false;

    std::error_code ec;
    const bool named = !_preset_save_name.empty();
    const bool exists = fs::is_regular_file(_preset_save_path, ec);
    if (!named) ui::TextColored(kWarn, msg::preset_name_required);
    else if (exists) ui::TextColored(kWarn, msg::preset_overwrite_warn);

    ImGui::Spacing();
    ImGui::BeginDisabled(!named);
    if (ui::Button(exists ? msg::preset_overwrite_button
                          : msg::preset_save_button,
                   ImVec2(px(kModalButtonW), 0))) {
        // Whichever screen armed the dialog; the three write the same header
        // and then their own settings (PresetFile.h).
        std::string* out = &_train_presets.msg;
        bool* out_err = &_train_presets.msg_err;
        if (_preset_save_kind == PresetKind::Dataset) {
            out = &_ds_presets.msg; out_err = &_ds_presets.msg_err;
        } else if (_preset_save_kind == PresetKind::Mesh) {
            out = &_mesh_presets.msg; out_err = &_mesh_presets.msg_err;
        }
        try {
            switch (_preset_save_kind) {
                case PresetKind::Dataset: {
                    DatasetPreset p{_preset_save_name, _preset_save_desc,
                                    _preset_save_path, capture_dataset_settings()};
                    save_dataset_preset(p, _preset_save_path);
                    _ds_presets.file = _preset_save_path;
                    _ds_presets.builtin.clear();
                    _ds_presets.display = p.name;
                    _ds_presets.desc = p.description;
                    _ds_presets.scanned_at = -1.0;
                    break;
                }
                case PresetKind::Mesh: {
                    MeshPreset p;
                    p.name = _preset_save_name;
                    p.description = _preset_save_desc;
                    p.job = _mesh_job;
                    save_mesh_preset(p, _preset_save_path);
                    _mesh_presets.file = _preset_save_path;
                    _mesh_presets.display = p.name;
                    _mesh_presets.desc = p.description;
                    _mesh_presets.scanned_at = -1.0;
                    break;
                }
                default: {
                    TrainPreset p;
                    p.name = _preset_save_name;
                    p.description = _preset_save_desc;
                    p.base = _preset;
                    p.cfg = _cfg;
                    p.touched = _cfg_ui.touched;
                    save_preset(p, _preset_save_path);
                    // Saving is also selecting: the screen now shows this one.
                    _train_presets.file = _preset_save_path;
                    _train_presets.display = p.name;
                    _train_presets.desc = p.description;
                    _train_presets.scanned_at = -1.0;
                    _defaults = _cfg;
                    break;
                }
            }
            *out = i18n::format(msg::preset_saved, {_preset_save_path});
            *out_err = false;
            refresh_presets();
            refresh_dataset_presets();
            refresh_mesh_presets();
        } catch (const std::exception& e) {
            *out = i18n::format(msg::preset_failed, {e.what()});
            *out_err = true;
        }
        log(*out);
        _preset_save_shown = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ui::Button(msg::cancel, ImVec2(px(kModalButtonW), 0))) {
        _preset_save_shown = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

}  // namespace gui

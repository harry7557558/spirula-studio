// GuiApp's lifecycle: construction, gui.conf, the per-frame dispatch and the
// app-wide modals. Each screen and job has its own file (src/app/README.md).

#include "app/gui/GuiApp.h"

#include "app/AppPaths.h"
#include "app/gui/GuiAppShared.h"
#include "app/gui/Ui.h"
#include "app/gui/ViewBookmarks.h"
#include "checkpoint/Resume.h"
#include "core/LicenseConsent.h"
#include "data/Json.h"
#include "dense/Clouds.h"
#include "i18n/Locale.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/Edit.h"
#include "i18n/catalog/Gui.h"
#include "i18n/catalog/Render.h"
#include "i18n/catalog/Train.h"

#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
namespace i18n = spirula::i18n;
namespace msg = spirula::i18n::msg::gui;
namespace emsg = spirula::i18n::msg::edit;
namespace dmsg = spirula::i18n::msg::dataset;
namespace tmsg = spirula::i18n::msg::train;
namespace rmsg = spirula::i18n::msg::render;

namespace gui {

// ===========================================================================
// Lifecycle + persistence
// ===========================================================================

GuiApp::GuiApp() {
    init_dense();
    load_settings();
    // Before the first frame, so an entry that is gone is never drawn.
    _recent.start_probe(classify_recent_model);
    _recent_probed_at = 0.0;
    _batch = load_batch_list();
    apply_preset("3dgs");
    // Built-in when it is there, COLMAP when it is not; effective_engine()
    // overrides this anyway if the stored choice is unavailable.
    if (!builtin_sfm_available()) _engine = Engine::Colmap;
    _compare.set_pick_file([this] {
        open_pick(PickAction::AddSplatFile, msg::viewer_pick_file.get(),
                  FileDialog::Mode::FileOrFolder, kOpenableExtensions);
    });
    // A saved reconstruction or dense seed invalidates the trainer's preview.
    _compare.set_on_model_saved([this](const std::string& saved) {
        if (_cfg.data.empty()) return;
        try {
            std::error_code ec;
            if (fs::equivalent(saved, _cfg.data, ec) || spirula::dense::is_dense_seed(_cfg.data, saved)) {
                _parse_dirty = true;
                _viewport.invalidate_framing();
            }
        } catch (const std::exception&) {}
    });
    _compare.set_siblings_of([this](const std::string& path) {
        std::vector<std::string> outs = _mesh.output_paths();
        for (const std::string& o : outs)
            if (o == path) return outs;
        return std::vector<std::string>{};
    });
    _compare.edit().set_to_trainer(
        [this](const std::string& dataset, const std::string& source, const std::string& seed) {
            _edit_train_dataset = dataset;
            _edit_train_source = source;
            _edit_train_seed = seed;
        });
    _compare.edit().set_pick_save(
        [this](int target, const std::string& ext, bool folder,
               const std::string& suggested, const std::string& start_dir) {
            _edit_save_target = target;
            open_pick(folder ? PickAction::EditSaveFolder
                             : PickAction::EditSaveFile,
                      emsg::save_pick_title.get(),
                      folder ? FileDialog::Mode::Folder : FileDialog::Mode::Save,
                      ext.empty() ? std::vector<std::string>{}
                                  : std::vector<std::string>{ext},
                      start_dir, false, suggested);
        });
    using RPick = gui::render::RenderSession::Pick;
    _compare.render().set_pick(
        [this](RPick kind, const std::string& start, const std::string& suggested) {
            // The folder a render's files are suggested into may not be there
            // yet, and a picker handed a missing folder opens somewhere else.
            if (kind == RPick::SaveProject || kind == RPick::Output) {
                std::error_code ec;
                const fs::path d = fs::u8path(start);
                if (!start.empty() && !fs::exists(d, ec) && fs::is_directory(d.parent_path(), ec))
                    fs::create_directories(d, ec);
            }
            switch (kind) {
                case RPick::SaveProject:
                    open_pick(PickAction::RenderProjectSave, rmsg::pick_save_project.get(),
                              FileDialog::Mode::Save, {".json"}, start, false, suggested);
                    break;
                case RPick::OpenProject:
                    open_pick(PickAction::RenderProjectOpen, rmsg::pick_open_project.get(),
                              FileDialog::Mode::File, {".json"}, start);
                    break;
                case RPick::Output:
                    open_render_output_pick(start, suggested);
                    break;
                case RPick::AddModel:
                    open_pick(PickAction::RenderAddModel, msg::viewer_pick_file.get(),
                              FileDialog::Mode::FileOrFolder, kOpenableExtensions);
                    break;
            }
        });
}

// Where a render goes: a file for a photo or a video, a folder for frames.
void GuiApp::open_render_output_pick(const std::string& start, const std::string& suggested) {
    using gui::render::OutputKind;
    const gui::render::Output& o = _compare.render().output();
    std::string dir = start;
    std::string name = suggested;
    if (!o.path.empty()) {
        gui::render::Output fitted = o;
        gui::render::fit_output_path(fitted);
        dir = fs::u8path(fitted.path).parent_path().string();
        name = fs::u8path(fitted.path).filename().string();
    }
    if (o.kind == OutputKind::Frames) {
        open_pick(PickAction::RenderOutput, rmsg::pick_output_folder.get(),
                  FileDialog::Mode::Folder, {}, dir);
        return;
    }
    using gui::render::Codec;
    const bool video = o.kind == OutputKind::Video;
    const std::string ext = video ? (o.codec == Codec::Gif ? ".gif"
                                     : o.codec == Codec::Av1Webm ? ".webm" : ".mp4")
                            : o.format == gui::render::ImageFormat::Jpeg ? ".jpg" : ".png";
    if (name.empty()) name = (video ? "render" : "photo") + ext;
    open_pick(PickAction::RenderOutput, rmsg::pick_output_file.get(),
              FileDialog::Mode::Save, {ext}, dir, false, name);
}

GuiApp::~GuiApp() = default;

void GuiApp::shutdown() {
    if (_dense_delete.valid()) _dense_delete.wait();
    save_settings();
    if (!_batch_save.save(_batch)) {
        try {
            const std::string message = i18n::format(emsg::save_failed.terminal(), {_batch_save.error()});
            std::fprintf(stderr, "%s\n", message.c_str());
        } catch (...) {
            std::fprintf(stderr, "batch.json: %s\n", std::strerror(EIO));
        }
        std::fflush(stderr);
    }
    _batch_active = false;
    detach_session_views();
    _mesh_monitor.clear();
    _viewport.destroy_gl();
    _images.destroy_gl();
    _mesh.cancel();
    // destroy_gl while the context is still current; close() then stops the
    // render workers before the engine goes back.
    _compare.destroy_gl();
    _compare.close();
    _mesh_preview_open = false;
    stop_inference_users();
    _segment.destroy_gl();
    // Same ordering as _compare above: destroy_gl while GL is still current.
    _mask_editor.destroy_gl();
    _mask_editor.close();
    _mask_editor.sam_drain_retiring();   // before nn::shutdown() frees the device
    _geometry_panel.destroy_gl();
    _partition_panel.destroy_gl();
    _roi_editor.destroy_gl();
    if (_merge_thread.joinable()) _merge_thread.join();
    _colmap.cancel();
    _sfm.cancel();
    reset_dataset_preview();
    close_source_preview();
    _source_probe.stop();
    _download.cancel();
    _geom_download.cancel();
    _feat_download.cancel();
    _font_download.cancel();
    _runner.shutdown();
}

// One line per remembered pick directory, keyed by dir_key().
static constexpr char kDirPrefix[] = "dialog_dir.";

// Escape line breaks to keep each gui.conf setting on one line.
static std::string escape_setting(const std::string& v) {
    std::string out;
    for (char c : v) {
        if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c != '\r') out += c;
    }
    return out;
}
static std::string unescape_setting(const std::string& v) {
    std::string out;
    for (size_t i = 0; i < v.size(); i++) {
        if (v[i] != '\\' || i + 1 >= v.size()) { out += v[i]; continue; }
        const char next = v[i + 1];
        if (next == 'n') { out += '\n'; i++; }
        else if (next == '\\') { out += '\\'; i++; }
        else out += v[i];
    }
    return out;
}

std::string tool_in_folder(const std::string& path, const std::string& name) {
    const fs::path dir(path);
    std::error_code ec;
    if (!dir.has_parent_path() || !fs::is_directory(dir, ec)) return path;
#ifdef _WIN32
    const fs::path exe = dir / (name + ".exe");
#else
    const fs::path exe = dir / name;
#endif
    return fs::is_regular_file(exe, ec) ? exe.string() : path;
}

std::string GuiApp::settings_path() {
    return (fs::path(app::config_dir()) / "gui.conf").string();
}

void GuiApp::load_settings() {
    // Presets have a folder of their own; every other kind of pick starts at
    // the home directory until one of that kind has been made.
    _dialog_dirs["preset"] = preset_dir(PresetKind::Train);

    std::string saved_lang;
    std::ifstream f(settings_path());
    if (!f) {
        spirula::i18n::init(spirula::i18n::lang_arg(), nullptr);
        return;
    }
    for (std::string s; std::getline(f, s);) {
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        size_t eq = s.find('=');
        if (eq == std::string::npos) continue;
        std::string k = s.substr(0, eq), v = s.substr(eq + 1);
        if (_recent.read_setting(k, v)) continue;
        if (k == "colmap_exe" && !v.empty()) _colmap_exe = tool_in_folder(v, "colmap");
        else if (k == "ffmpeg_exe" && !v.empty()) _ffmpeg_exe = tool_in_folder(v, "ffmpeg");
        else if (k == "sfm_engine") _engine = v == "colmap" ? Engine::Colmap
                                                            : Engine::BuiltIn;
        else if (k == "batch_command") _batch_cmd = unescape_setting(v);
        else if (k == "dataset_finish_command") _dataset_finish_command = unescape_setting(v);
        else if (k == "additional_gpus") _additional_gpus = unescape_setting(v);
        // accepted_license= is read from the file on demand (license_accepted).
        else if (k == "lang" && !v.empty()) saved_lang = v;
        else if (k == "ui_scale") _scale.set_user(std::clamp((float)atof(v.c_str()),
                                                             0.0f, 3.0f));
        else if (k == "panel_w") _panel_w = (float)atof(v.c_str());
        else if (k == "ds_panel_w") _ds_panel_w = (float)atof(v.c_str());
        else if (k == "mesh_panel_w") _mesh_panel_w = (float)atof(v.c_str());
        else if (k == "edit_panel_w") _edit_panel_w = (float)atof(v.c_str());
        else if (k == "log_h") _log_h = (float)atof(v.c_str());
        else if (k == "show_log") _show_log = v != "0";
        else if (k == "log_details") _log_details = v != "0";
        else if (k == "show_preview") _show_preview = v != "0";
        else if (k == "preview_h") _preview_h = (float)atof(v.c_str());
        else if (k == "show_settings") _show_settings = v != "0";
        else if (k == "native_dialogs") _dialog.use_native(v != "0");
        // Absent -- an upgrade from a build that did not write it -- leaves the
        // default, which is ON: the files are what makes a run resumable.
        else if (k == "keep_intermediate") _sfm_job.keep_intermediate = v != "0";
        else if (k == "save_full_checkpoint") _keep_full_ckpt = v != "0";
        else if (k.rfind(kDirPrefix, 0) == 0 && !v.empty())
            _dialog_dirs[k.substr(sizeof kDirPrefix - 1)] = v;
        else if (read_screenshot_setting(_shot, k, v)) {}
    }
    // A stored extent from a build whose defaults have moved, or a hand-edited
    // file, must not be able to leave a panel unreachably small or wide.
    _panel_w = std::clamp(_panel_w, 220.0f, 900.0f);
    _log_h = std::clamp(_log_h, 60.0f, 800.0f);
    _preview_h = std::clamp(_preview_h, 60.0f, 800.0f);
    _ds_panel_w = std::clamp(_ds_panel_w, 320.0f, 1200.0f);
    _mesh_panel_w = std::max(320.0f, _mesh_panel_w);
    _edit_panel_w = std::clamp(_edit_panel_w, 220.0f, 900.0f);

    // The settings file is the third step of the chain and loses to both
    // --lang and SS_LANG, so the whole chain is re-run rather than the stored
    // value simply applied. Passing lang_arg() back in is what keeps --lang
    // winning even though it was parsed in main() long before this ran.
    spirula::i18n::init(spirula::i18n::lang_arg(), saved_lang.c_str());
}

void GuiApp::save_settings() {
    // Read before the file is truncated: the CLI may have added to it since load.
    const std::vector<std::string> accepted = spirula::license::accepted_all();
    // Written beside and renamed over: a crash or a full disk mid-write must not
    // leave a truncated gui.conf, which would also forget every accepted licence.
    const std::string tmp = spirula::license::scratch_path_for(settings_path());
    FILE* f = std::fopen(tmp.c_str(), "w");
    if (!f) return;
    _recent.write_settings(f);
    std::fprintf(f, "colmap_exe=%s\n", _colmap_exe.c_str());
    std::fprintf(f, "ffmpeg_exe=%s\n", _ffmpeg_exe.c_str());
    std::fprintf(f, "sfm_engine=%s\n",
                 _engine == Engine::Colmap ? "colmap" : "builtin");
    std::fprintf(f, "batch_command=%s\n", escape_setting(_batch_cmd).c_str());
    std::fprintf(f, "dataset_finish_command=%s\n",
                 escape_setting(_dataset_finish_command).c_str());
    std::fprintf(f, "additional_gpus=%s\n", escape_setting(_additional_gpus).c_str());
    std::fprintf(f, "lang=%s\n", spirula::i18n::code(spirula::i18n::current()));
    std::fprintf(f, "ui_scale=%.3f\n", _scale.user());
    std::fprintf(f, "panel_w=%.1f\n", _panel_w);
    std::fprintf(f, "ds_panel_w=%.1f\n", _ds_panel_w);
    std::fprintf(f, "mesh_panel_w=%.1f\n", _mesh_panel_w);
    std::fprintf(f, "edit_panel_w=%.1f\n", _edit_panel_w);
    std::fprintf(f, "log_h=%.1f\n", _log_h);
    std::fprintf(f, "show_log=%d\n", _show_log ? 1 : 0);
    std::fprintf(f, "log_details=%d\n", _log_details ? 1 : 0);
    std::fprintf(f, "show_preview=%d\n", _show_preview ? 1 : 0);
    std::fprintf(f, "preview_h=%.1f\n", _preview_h);
    std::fprintf(f, "show_settings=%d\n", _show_settings ? 1 : 0);
    std::fprintf(f, "native_dialogs=%d\n", _dialog.native_enabled() ? 1 : 0);
    std::fprintf(f, "keep_intermediate=%d\n", _sfm_job.keep_intermediate ? 1 : 0);
    std::fprintf(f, "save_full_checkpoint=%d\n", _keep_full_ckpt ? 1 : 0);
    for (const auto& [key, dir] : _dialog_dirs)
        std::fprintf(f, "%s%s=%s\n", kDirPrefix, key.c_str(), dir.c_str());
    write_screenshot_settings(f, _shot);
    for (const auto& l : accepted)
        std::fprintf(f, "accepted_license=%s\n", l.c_str());
    const bool ok = std::ferror(f) == 0;
    const bool closed = std::fclose(f) == 0;
    std::error_code ec;
    if (ok && closed) fs::rename(tmp, settings_path(), ec);
    if (!ok || !closed || ec) fs::remove(tmp, ec);
}

void GuiApp::remember(RecentKind kind, std::string path) {
    if (path.empty()) return;
    // A run folder opened in the viewer is still that run, not a second entry.
    if (kind == RecentKind::Model && _recent.contains(RecentKind::Run, path))
        kind = RecentKind::Run;
    _recent.add(kind, path, (int64_t)std::time(nullptr));
    save_settings();
}

void set_ss_env(const char* suffix, const std::string& value) {
    const std::string name = std::string("SS_") + suffix;
#ifdef _WIN32
    _putenv_s(name.c_str(), value.c_str());
#else
    setenv(name.c_str(), value.c_str(), 1);
#endif
}

static std::string cached_model_path(const std::string& id) {
    const ModelEntry* e = find_model(id);
    if (!e || !model_is_cached(*e)) return "";
    return model_path(*e);
}

// ===========================================================================
// Frame
// ===========================================================================

std::string GuiApp::state_json() {
    auto quoted = [](const std::string& s) {
        std::string q = "\"";
        for (char c : s) {
            if ((unsigned char)c < 0x20) continue;
            if (c == '"' || c == '\\') q += '\\';
            q += c;
        }
        return q + "\"";
    };
    // Index order is the declaration order of Screen and TrainRunner::Phase.
    static const char* kScreens[] = {"home", "new_dataset", "train", "viewer",
                                     "batch", "mesh"};
    static const char* kPhases[] = {"idle", "loading", "ready", "load_error",
                                    "preparing", "training", "evaluating", "done",
                                    "train_error"};
    std::string out = "\"screen\":\"";
    out += kScreens[(int)_screen];
    out += "\",\"train_phase\":\"";
    out += kPhases[(int)_runner.phase()];
    out += "\",\"step\":" + std::to_string(_runner.latest_progress().step);
    out += ",\"busy\":";
    out += native_work_busy() ? "true" : "false";
    // A dataset run stopped for its frames to be looked at: `review_continue`
    // is the widget that lets it go on.
    out += ",\"reviewing_frames\":";
    out += _sfm.reviewing() ? "true" : "false";
    out += ",\"dialog_open\":";
    out += _dialog.is_open() ? "true" : "false";
    out += ",\"models_open\":" + std::to_string(_compare.count());
    out += ",\"dataset\":" + quoted(_cfg.data);
    out += ",\"mask_editor_open\":";
    out += _mask_editor.is_open() ? "true" : "false";
    out += ",\"partition_panel_open\":";
    out += _partition_panel.is_open() ? "true" : "false";
    out += ",\"roi_editor_open\":";
    out += _roi_editor.is_open() ? "true" : "false";
    // The app's one segmentation checkpoint, which both screens pick and fetch.
    static const char* kDownload[] = {"idle", "running", "done", "failed", "cancelled"};
    out += ",\"model_id\":" + quoted(_model_id);
    out += ",\"detector_id\":" + quoted(_mask_detector_id);
    out += ",\"model_path\":" + quoted(selected_mask_model().model);
    out += ",\"model_download\":\"";
    out += kDownload[(int)_download.state()];
    out += "\",\"license_prompt\":" + quoted(_license_prompt);
    out += ",\"license_tick\":";
    out += _license_tick ? "true" : "false";
    out += ",\"license_notice\":" + quoted(_license_notice);
    out += ",\"batch_fetching\":";
    out += _batch_fetching ? "true" : "false";
    // Index order is the declaration order of mask::CanvasMode.
    static const char* kCanvasModes[] = {"shape", "eraser", "path", "pen", "sam"};
    static_assert(sizeof(kCanvasModes) / sizeof(kCanvasModes[0]) == (size_t)mask::CanvasMode::Sam + 1,
                  "one name per mask::CanvasMode");
    const mask::MaskDoc* mdoc = _mask_editor.doc();
    out += ",\"mask_editor_mode\":\"";
    out += kCanvasModes[(int)_mask_editor.mode()];
    out += "\",\"mask_editor_frame\":" + std::to_string(_mask_editor.frame_index());
    out += ",\"mask_editor_key\":" + quoted(mdoc ? mdoc->key() : std::string());
    out += ",\"mask_editor_history\":" + std::to_string(mdoc ? mdoc->history_size() : -1);
    out += ",\"mask_editor_kept\":" + std::to_string(mdoc ? mdoc->kept() : (int64_t)-1);
    out += ",\"mask_editor_clicks\":" + std::to_string(_mask_editor.sam_click_count());
    out += ",\"mask_editor_model\":" + quoted(_mask_editor.sam_model_path());
    out += ",\"sam_model_changes\":" + std::to_string(_mask_editor.sam_model_changes());
    out += ",\"sam_text_ok\":";
    out += _mask_editor.sam_text_supported() ? "true" : "false";
    out += ",\"sam_busy\":";
    out += _mask_editor.sam_busy() ? "true" : "false";
    out += ",\"sam_results\":" + std::to_string(_mask_editor.sam_results());
    out += ",\"sam_dropped\":" + std::to_string(_mask_editor.sam_dropped());
    out += ",\"sam_last_ms\":" + std::to_string(_mask_editor.sam_last_ms());
    out += ",\"sam_last_job_ms\":" + std::to_string(_mask_editor.sam_last_job_ms());
    out += ",\"sam_last_area\":" + std::to_string(_mask_editor.sam_last_area());
    out += ",\"sam_last_detections\":" + std::to_string(_mask_editor.sam_last_detections());
    out += ",\"sam_last_score\":" + std::to_string(_mask_editor.sam_last_score());
    out += ",\"sam_status\":" + quoted(_mask_editor.sam_status());
    out += ",\"sam_error\":" + quoted(_mask_editor.sam_error());
    out += ",\"sam_vram_mib\":" + std::to_string(_mask_editor.sam_vram_mib());
    out += ",\"sam_pool_mib\":" + std::to_string(mask::MaskSession::sam_pool_mib());
    out += ",\"sam_close_ms\":" + std::to_string(_mask_editor.sam_close_ms());
    out += ",\"sam_retiring\":";
    out += _mask_editor.sam_retiring() ? "true" : "false";
    out += ",\"sam_retire_ms\":" + std::to_string(_mask_editor.sam_retire_ms());
    out += ",\"sam_loads\":" + std::to_string(mask::MaskSession::sam_loads());
    out += ",\"sam_ui_ms\":" + std::to_string(_mask_editor.sam_ui_ms());
    out += ",\"sam_click\":[" + std::to_string(_mask_editor.sam_click_x()) + "," +
           std::to_string(_mask_editor.sam_click_y()) + "]";
    out += ",\"mask_editor_canvas_h\":" + std::to_string(_mask_editor.canvas_height());
    out += ",\"mask_editor_anchors\":" + std::to_string(_mask_editor.path_anchors());
    out += ",\"mask_editor_pen_anchors\":" + std::to_string(_mask_editor.pen_anchors());
    out += ",\"sam_margin\":" + std::to_string(_mask_editor.sam_margin());
    out += ",\"mask_dilate_ratio\":" + std::to_string(_mask.dilate_ratio);
    // The dataset screen's clicked objects, which no editor click may reach,
    // beside the editor's own, which a widget wired to nothing leaves at zero.
    out += ",\"mask_clicks\":" + std::to_string(_mask.clicks.size());
    out += ",\"mask_object_count\":" + std::to_string(_mask.object_count);
    out += ",\"mask_current_object\":" + std::to_string(_mask.current_object);
    out += ",\"mask_prompt\":" + quoted(_mask.prompt);
    out += ",\"mask_editor_objects\":" + std::to_string(_mask_editor.sam_object_count());
    out += ",\"sam_reapply_ms\":" + std::to_string(_mask_editor.sam_reapply_ms());
    out += ",\"sam_reapply_job_ms\":" + std::to_string(_mask_editor.sam_reapply_job_ms());
    out += ",\"sam_margin_start_ms\":" + std::to_string(_mask_editor.sam_margin_start_ms());
    out += ",\"sam_held_bytes\":" + std::to_string(_mask_editor.sam_held_bytes());
    static const char* kPeek[] = {"none", "photo", "mask"};
    out += ",\"mask_peek\":\"";
    out += kPeek[(int)_mask_editor.peek()];
    out += "\",\"mask_peek_total\":" + std::to_string(_mask_editor.peek_total());
    out += ",\"nav_visible\":";
    out += ImGui::GetIO().NavVisible ? "true" : "false";
    static const char* kView[] = {"overlay", "mask", "side"};
    out += ",\"mask_view\":\"";
    out += kView[(int)_mask_editor.view_mode()];
    out += "\",\"mask_slideshow\":";
    out += _mask_editor.slideshow_playing() ? "true" : "false";
    out += ",\"mask_slide_stop_ms\":" + std::to_string(_mask_editor.slide_stop_ms());
    out += ",\"mask_slide_join_ms\":" + std::to_string(_mask_editor.slide_join_ms());
    out += ",\"mask_slide_shown_fps\":" + std::to_string(_mask_editor.slide_shown_fps());
    out += ",\"mask_slide_gap_ms\":" + std::to_string(_mask_editor.slide_max_gap_ms());
    out += ",\"mask_slide_window\":" + std::to_string(_mask_editor.slide_window());
    out += ",\"mask_slide_threads\":" + std::to_string(_mask_editor.slide_threads());
    out += ",\"mask_slide_decoded\":" + std::to_string(_mask_editor.slide_decoded());
    out += ",\"mask_slide_index\":" + std::to_string(_mask_editor.slide_index());
    out += ",\"mask_scanned\":" + std::to_string(_mask_editor.scanned_count());
    out += ",\"mask_missing\":" + std::to_string(_mask_editor.missing_count());
    out += ",\"mask_scan_ms\":" + std::to_string(_mask_editor.scan_ms());
    out += ",\"mask_editor_error\":" + quoted(_mask_editor.error());
    return out;
}

void GuiApp::frame() {
    // Before the first widget: a style swap halfway through a frame would
    // measure half the window against one scale and half against the other.
    _scale.update(ImGui::GetIO().DisplaySize);

    append_logs();
    poll_dataset_finish_command();
    pump_dense_delete();
    run_pending_if_stopped();
    if (!_edit_train_dataset.empty()) open_edited_dataset();
    // vit-giant2 is two files, and so is ALIKED with LightGlue: both fetches
    // are queues, stepped on from somewhere that runs whatever screen is up.
    _geom_download.pump();
    _feat_download.pump();
    pump_batch_fetch();
    if (!_download_model_id.empty() && _download.state() == FileDownload::State::Done) {
        const ModelEntry* e = find_model(_download_model_id);
        if (!e || !_download.start(*e, detector_for(*e, _download_detector_id)))
            _download_model_id.clear();
    }
    pump_source_probes();
    advance_dataset_training();
    // Before the reload check below: between two batch rows the runner is
    // briefly idle, and a stale _parse_dirty would start a dataset parse right
    // where the next row wants the engine.
    advance_batch();

    // A run that saved its model is one to come back to; a batch's too.
    if (const TrainRunner::Phase ph = _runner.phase(); ph != _seen_phase) {
        _seen_phase = ph;
        const spirula::TrainerSession* s = _runner.session();
        if (ph == TrainRunner::Phase::Done && _runner.saved_on_stop() && s) {
            std::error_code ec;
            remember(RecentKind::Run, fs::absolute(s->out_dir, ec).u8string());
        }
        // Not after a stop: someone is at the screen, and the command may be a shutdown.
        if ((ph == TrainRunner::Phase::Done || ph == TrainRunner::Phase::TrainError) &&
            s && !s->stop_requested.load()) {
            std::error_code ec;
            run_finish_command(
                s->cfg.finish_command,
                ph == TrainRunner::Phase::Done
                    ? i18n::format(tmsg::finish_cmd_trained,
                                   {fs::absolute(s->out_dir, ec).u8string()})
                    : i18n::format(tmsg::finish_cmd_failed, {_runner.error()}));
        }
    }

    // The queue survives a restart, so it is written back once the widget
    // being edited is idle -- the same deferral the dataset options use.
    if (_batch_save.needs_autosave() && !ImGui::IsAnyItemActive()) save_batch();
    // Same deferral for a dragged splitter: the file would otherwise be
    // rewritten on every frame of the drag.
    if (_layout_dirty && !ImGui::IsAnyItemActive()) {
        _layout_dirty = false;
        save_settings();
    }

    // Dataset-parsing option changed: re-parse once the user finishes
    // editing (no active widget), so the preview / camera counts stay in
    // sync with the config. Never fires while native work is active.
    if (_parse_dirty && !native_work_busy() && !_batch_active &&
        !ImGui::IsAnyItemActive()) {
        _parse_dirty = false;
        if (!_cfg.data.empty()) {
            log(dmsg::log_dataset_settings_changed.get());
            detach_session_views();
            _runner.load_dataset(_cfg, _preset);
        }
    }

    // Viewport backend transitions: open model files once they are on the
    // device, the GL dataset preview once a dataset is parsed, the engine
    // renderer once training set the engine up -- the first excludes the rest.
    if (_compare.holds_engine()) {
        _compare.poll();
        _images.detach();   // the files own the engine while they are open
    } else if (_runner.engine_ready()) {
        if (!_viewport.attached() && _runner.session()) {
            _viewport.attach(*_runner.session());
            bind_session_views(_viewport, *_runner.session());
        }
        // The photograph-vs-render mode needs the DataManager as well as the
        // splats, so it waits for engine setup exactly as the viewport does.
        if (!_images.attached() && _runner.session())
            _images.attach(*_runner.session());
    } else if (_runner.phase() == TrainRunner::Phase::Ready) {
        if (!_viewport.preview_active() && _runner.session()) {
            _viewport.attach_preview(*_runner.session());
            bind_session_views(_viewport, *_runner.session());
        }
    }
    update_roi_overlay();

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_MenuBar;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::Begin("##host", nullptr, flags);
    ImGui::PopStyleVar();

    draw_menu_bar();
    switch (_screen) {
        case Screen::Home:   draw_home();   break;
        case Screen::NewDataset: draw_new_dataset(); break;
        case Screen::Train:  draw_train();  break;
        case Screen::Viewer: draw_viewer(); break;
        case Screen::Batch:  draw_batch();  break;
        case Screen::Mesh:   draw_mesh();   break;
    }
    if (_roi_editor.is_open()) _roi_editor.draw();

    // A job cancelled by the editor's close finishes its stage off this thread.
    _mask_editor.sam_poll_retiring();
    if (_mask_editor.is_open()) {
        // Read now, not at the top of the frame: a preview opened above has
        // already taken the device.
        _mask_editor.set_sam_blocker(mask::MaskSession::sam_blocker(
            _segment.is_open(), _geometry_panel.is_open(), native_work_busy()));
        // Every frame: a pick, or a finished download, lands now.
        // The editor drives a SAM session itself and pairs no text detector,
        // so SAM 2.1 there is clicks, no words.
        const ModelEntry* me = find_model(_mask_editor_model_id);
        _mask_editor.set_sam_model(cached_model_path(_mask_editor_model_id),
                                   me && me->text_prompts && me->kind == MaskModelKind::Sam);
        _mask_editor.draw();
    }
    // After every screen and the editor, so either can raise the one consent modal.
    draw_license_modal();

    if (_dialog.draw()) handle_dialog_result(_dialog.results());
    // The save dialog steps aside while the folder picker is up; bring it
    // back once the picker is gone, confirmed or cancelled either way.
    if (_preset_save_reopen && !_dialog.is_open()) {
        _preset_save_reopen = false;
        _preset_save_open = true;
    }
    // A camera move saved on the way out: once the picker is gone, out --
    // unless it was cancelled and the move is still unsaved.
    if (_quit_after_render_save && !_dialog.is_open()) {
        _quit_after_render_save = false;
        if (!_compare.render().dirty()) request_close();
    }
    draw_dense_versions();
    draw_preset_save_modal();
    draw_preset_delete_modal();
    draw_edit_exit_modal();
    draw_render_exit_modal();
    draw_confirm_modal();
    draw_resume_error();
    draw_data_error_modal();

    ImGui::End();
}

// The training thread is blocked inside its step until one of these buttons
// answers, so Esc puts the modal straight back rather than stranding the run.
void GuiApp::draw_data_error_modal() {
    const std::string what = _runner.data_error();
    if (!what.empty() && !_data_error_shown) {
        ui::OpenPopup(msg::data_error_title);
        _data_error_shown = true;
    }
    if (!_data_error_shown) return;
    if (!ui::BeginPopupModal(msg::data_error_title, nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
        if (!what.empty()) ui::OpenPopup(msg::data_error_title);
        else               _data_error_shown = false;
        return;
    }
    if (what.empty()) {                  // answered elsewhere (a stop request)
        _data_error_shown = false;
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    ui::TextWrappedRaw(what);
    ImGui::Spacing();
    ui::Text(msg::data_error_intro);
    ImGui::Spacing();
    auto answer = [&](bool retry) {
        _runner.resolve_data_error(retry);
        _data_error_shown = false;
        ImGui::CloseCurrentPopup();
    };
    const float bw = px(190.0f);
    if (ui::Button(msg::data_error_retry, ImVec2(bw, 0))) answer(true);
    ui::help_on_hover(msg::data_error_retry_help);
    ImGui::SameLine();
    if (ui::Button(msg::stop_and_save, ImVec2(bw, 0))) answer(false);
    ui::help_on_hover(msg::stop_and_save_help);
    ImGui::EndPopup();
}

// Unsaved edits at quit: the file is the only place they are recorded, so
// this is the last chance to write them.
void GuiApp::draw_edit_exit_modal() {
    if (_edit_exit_confirm) {
        ui::OpenPopup(emsg::exit_title);
        _edit_exit_confirm = false;
    }
    ImGui::SetNextWindowSize(ImVec2(px(440.0f), 0.0f), ImGuiCond_Appearing);
    if (!ui::BeginPopupModal(emsg::exit_title)) return;
    EditSession& e = _compare.edit();
    EditDoc* doc = e.doc();
    ui::TextWrapped(emsg::exit_body,
                    {doc ? fs::path(doc->source_path()).filename().string()
                         : std::string()});
    ImGui::BeginDisabled(!e.can_save_in_place());
    if (ui::Button(emsg::save_over)) {
        e.save_in_place();
        ImGui::CloseCurrentPopup();
        request_close();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!e.can_save_copy());
    if (ui::Button(emsg::save_copy)) {
        e.ask_save_copy();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ui::Button(emsg::discard_yes)) {
        _compare.end_edit();
        ImGui::CloseCurrentPopup();
        request_close();
    }
    ImGui::SameLine();
    if (ui::Button(emsg::discard_no)) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

// An unsaved camera move at quit: saved over its project, saved somewhere
// new, or let go.
void GuiApp::draw_render_exit_modal() {
    if (_render_exit_confirm) {
        ui::OpenPopup(rmsg::quit_title);
        _render_exit_confirm = false;
    }
    ImGui::SetNextWindowSize(ImVec2(px(440.0f), 0.0f), ImGuiCond_Appearing);
    if (!ui::BeginPopupModal(rmsg::quit_title)) return;
    gui::render::RenderSession& r = _compare.render();
    const std::string& file = r.project_file();
    if (file.empty()) ui::TextWrapped(rmsg::quit_body_new);
    else ui::TextWrapped(rmsg::quit_body, {fs::u8path(file).filename().string()});
    ImGui::BeginDisabled(file.empty());
    if (ui::Button(rmsg::project_save)) {
        ImGui::CloseCurrentPopup();
        if (r.save_in_place()) request_close();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ui::Button(rmsg::project_save_as)) {
        ImGui::CloseCurrentPopup();
        r.ask_save_as();
        _quit_after_render_save = true;
    }
    ImGui::SameLine();
    if (ui::Button(rmsg::quit_discard)) {
        ImGui::CloseCurrentPopup();
        _render_discarded = true;
        request_close();
    }
    ImGui::SameLine();
    if (ui::Button(rmsg::render_cancel)) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void GuiApp::open_resume(const std::string& path) {
    if (path.empty() || native_work_busy() || training_busy()) return;
    TrainConfig cfg;
    std::string preset = _preset;
    try {
        TrainConfig request;
        request.resume = path;
        request.finish_command = _cfg.finish_command;
        cfg = ckpt::build_resume_config(request, "", {});
        if (!fs::is_directory(fs::u8path(cfg.data)))
            throw std::runtime_error("the run's dataset folder no longer exists: " + cfg.data);
        const auto run = ckpt::resolve_checkpoint(path).run_dir;
        if (const auto* name = json_parse_file((run / "config.json").string()).find("preset"))
            if (!name->as_string().empty()) preset = name->as_string();
    } catch (const std::exception& e) {
        _resume_error = i18n::format(msg::resume_failed, {e.what()});
        _open_resume_error = true;
        log(_resume_error);
        return;
    }
    cfg.disable_viewer = true;   // as apply_preset: the native viewport
    _preset = preset;
    _cfg = cfg;
    open_dataset(cfg.data, cfg.image_dir, cfg.mask_dir, cfg.flip_mask);
    // open_dataset re-defaults the output beside the dataset; a resume writes into its own run.
    _cfg.output_dir_prefix = cfg.output_dir_prefix;
    _cfg.output_dir_name = cfg.output_dir_name;
    _cfg.resume = cfg.resume;
}

void GuiApp::draw_resume_error() {
    if (_open_resume_error) { ui::OpenPopup(msg::resume_failed_title); _open_resume_error = false; }
    if (!ui::BeginPopupModal(msg::resume_failed_title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    ImGui::PushTextWrapPos(px(520.0f));
    ui::TextRaw(_resume_error);
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    if (ui::Button(msg::resume_failed_close, ImVec2(px(kModalButtonW), 0))) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void GuiApp::draw_confirm_modal() {
    if (_open_confirm) {
        ui::OpenPopup(msg::confirm_title);
        _open_confirm = false;
        _confirm_shown = true;
        _stop_confirmed = false;
        _confirm_was_paused = _runner.paused();
        _runner.set_paused(true);
    }
    // Every way out of the modal restores the pause state first: a stop
    // request breaks out of the pause gate on its own, so this only decides
    // what the run goes back to when the user keeps training.
    auto resume = [this] { _runner.set_paused(_confirm_was_paused); };
    if (ui::BeginPopupModal(msg::confirm_title, nullptr,
                            ImGuiWindowFlags_AlwaysAutoResize)) {
        ui::Text(msg::confirm_intro);
        // Whole questions rather than one with a swappable tail: the clause
        // order differs by language, and Japanese, Korean and Turkish put the
        // verb last, so "... and {0}?" cannot be translated at all.
        ui::Text(_pending == Pending::Quit       ? msg::confirm_quit
               : _pending == Pending::GoHome     ? msg::confirm_home
               : _pending == Pending::OpenSplat  ? msg::confirm_open_splat
               : _pending == Pending::StartBatch ? msg::confirm_batch
               : _pending == Pending::StopHere   ? msg::confirm_stop
                                                 : msg::confirm_open);
        ImGui::Spacing();
        auto stop = [&](bool save) {
            // A batch is a training session too, and this is the user saying
            // they want the engine back. Give up the queue with it -- except
            // when the queue is what they are starting.
            if (_pending != Pending::StartBatch) cancel_batch();
            resume();
            _runner.request_stop(save);
            _stop_confirmed = true;
            _confirm_shown = false;
            // run_pending_if_stopped() completes the action once the stop
            // lands (phase leaves Training).
            ImGui::CloseCurrentPopup();
        };
        const float bw = px(190.0f);
        if (ui::Button(msg::stop_and_save, ImVec2(bw, 0))) stop(true);
        ui::help_on_hover(msg::stop_and_save_help);
        ImGui::SameLine();
        if (ui::Button(msg::stop_without_saving, ImVec2(bw, 0))) stop(false);
        ui::help_on_hover(msg::stop_without_saving_help);
        ImGui::SameLine();
        if (ui::Button(msg::keep_training, ImVec2(bw, 0))) {
            _pending = Pending::None;
            _pending_path.clear();
            _edit_after_open = false;
            _confirm_shown = false;
            resume();
            ImGui::CloseCurrentPopup();
        }
        ui::help_on_hover(msg::keep_training_help);
        ImGui::EndPopup();
    } else if (_confirm_shown) {
        // Dismissed (Esc / click-away): treat as "keep training".
        _pending = Pending::None;
        _pending_path.clear();
        _edit_after_open = false;
        _confirm_shown = false;
        resume();
    }
}

}  // namespace gui

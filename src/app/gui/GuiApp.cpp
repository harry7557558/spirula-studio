// GuiApp.cpp -- see GuiApp.h.

#include "app/gui/GuiApp.h"

#include "core/ColorSpace.h"
#include "core/ExrImage.h"

#include "checkpoint/SplatPly.h"
#include "data/Json.h"
#include "app/AppPaths.h"
#include "app/CrashLog.h"
#include "app/gui/DatasetPrep.h"
#include "app/gui/MaskPrompt.h"
#include "app/gui/Subprocess.h"
#include "mesh/MeshImport.h"
#include "app/gui/Ui.h"

#include "i18n/Locale.h"
#include "i18n/catalog/Brand.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/Geometry.h"
#include "i18n/catalog/Gui.h"
#include "i18n/catalog/Train.h"
#include "i18n/catalog/TrainFields.h"

#include "app/gui/GlLoader.h"
#include "app_generated/app_banner.h"
#include "external/stb_image.h"

#include "imgui.h"
#include "imgui_stdlib.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>

namespace fs = std::filesystem;
namespace i18n = spirula::i18n;
namespace msg = spirula::i18n::msg::gui;
namespace fld = spirula::i18n::msg::field;
namespace dmsg = spirula::i18n::msg::dataset;
namespace gmsg = spirula::i18n::msg::geometry;
using spirula::i18n::Msg;
using spirula::format_duration;

namespace gui {

namespace {

const ImVec4 kOk(0.35f, 0.85f, 0.45f, 1.0f);
const ImVec4 kErr(1.0f, 0.42f, 0.42f, 1.0f);
const ImVec4 kWarn(0.95f, 0.75f, 0.30f, 1.0f);
const ImVec4 kDim(0.6f, 0.6f, 0.6f, 1.0f);

std::string format_gib(uint64_t bytes) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.2f", (double)bytes / (1024.0 * 1024.0 * 1024.0));
    return buf;
}

std::string format_count(double n) {
    char buf[32];
    if (n >= 1e6) std::snprintf(buf, sizeof buf, "%.2fM", n / 1e6);
    else if (n >= 1e3) std::snprintf(buf, sizeof buf, "%.1fk", n / 1e3);
    else std::snprintf(buf, sizeof buf, "%d", (int)n);
    return buf;
}

// "General purpose (3dgs)" -- the translated label plus the name the command
// line and the documentation use, which is the only thing tying a row here to
// `spirula train <preset>`.
std::string preset_label(const std::string& name) {
    const auto* t = spirula::i18n::msg::train::preset_text(name.c_str());
    if (!t) return name;
    return std::string(t->label->get()) + " (" + name + ")";
}

std::string preset_help(const std::string& name) {
    const auto* t = spirula::i18n::msg::train::preset_text(name.c_str());
    return t ? t->help->get() : "";
}

// The hover help behind a preset row, wherever one is drawn: what it is for,
// and -- dimmed under it -- the file it came from. The path is what tells two
// presets with the same name apart, which nothing else on screen does; a
// built-in has none and shows only the sentence.
void preset_hover(const std::string& description, const std::string& path) {
    if (description.empty() && path.empty()) return;
    if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) ||
        !ImGui::BeginTooltip())
        return;
    ImGui::PushTextWrapPos(px(360.0f));
    if (!description.empty()) ui::TextRaw(description);
    // A path is a path in every language, and it is not a sentence about the
    // one above it -- so it is its own line, not appended to it.
    if (!path.empty()) ui::TextDisabledRaw(path);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

void preset_hover(const TrainPreset& p) { preset_hover(p.description, p.path); }

// True when two configs parse to the same dataset -- every field
// load_dataset() consumes, listed as SS_DATASET_PARSE_FIELDS.
bool parse_settings_equal(const TrainConfig& a, const TrainConfig& b) {
    bool eq = true;
#define SS_CMP(member) eq = eq && (a.member == b.member);
    SS_DATASET_PARSE_FIELDS(SS_CMP)
#undef SS_CMP
    return eq;
}

// The file-dialog filter list, from DatasetPrep's one list of containers.
// What the viewer can open: Gaussians and SfM clouds (.ply), and every mesh
// format the mesher writes -- one list, so the picker and the drop handler
// cannot disagree about what is openable.
const std::vector<std::string> kViewableExtensions = {".ply", ".obj", ".gltf",
                                                      ".glb"};

std::vector<std::string> video_dialog_filters() {
    return std::vector<std::string>(kVideoExtensions,
                                    kVideoExtensions + kNumVideoExtensions);
}

}  // namespace


// ===========================================================================
// Lifecycle + persistence
// ===========================================================================

GuiApp::GuiApp() {
    load_settings();
    _batch = load_batch_list();
    apply_preset("3dgs");
    // Built-in when it is there, COLMAP when it is not; effective_engine()
    // overrides this anyway if the stored choice is unavailable.
    if (!builtin_sfm_available()) _engine = Engine::Colmap;
    _compare.set_pick_file([this] {
        _pick = PickAction::AddSplatFile;
        _dialog.open(msg::viewer_pick_file.get(), FileDialog::Mode::File,
                     kViewableExtensions);
    });
}

GuiApp::~GuiApp() = default;

void GuiApp::shutdown() {
    save_settings();
    save_batch_list(_batch);
    _batch_active = false;
    detach_session_views();
    _viewport.destroy_gl();
    _images.destroy_gl();
    _mesh.cancel();
    // destroy_gl while the context is still current; close() then stops the
    // render workers before the engine goes back.
    _compare.destroy_gl();
    _compare.close();
    _mesh_preview_open = false;
    _segment.close();
    _segment.destroy_gl();
    _geometry_panel.close();
    _geometry_panel.destroy_gl();
    _colmap.cancel();
    _sfm.cancel();
    reset_dataset_preview();
    _download.cancel();
    _geom_download.cancel();
    _feat_download.cancel();
    _font_download.cancel();
    _runner.shutdown();
}

std::string GuiApp::settings_path() {
    return (fs::path(app::config_dir()) / "gui.conf").string();
}

void GuiApp::load_settings() {
    std::string saved_lang;
    FILE* f = std::fopen(settings_path().c_str(), "r");
    if (!f) {
        spirula::i18n::init(spirula::i18n::lang_arg(), nullptr);
        return;
    }
    char line[1024];
    while (std::fgets(line, sizeof line, f)) {
        std::string s = line;
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        size_t eq = s.find('=');
        if (eq == std::string::npos) continue;
        std::string k = s.substr(0, eq), v = s.substr(eq + 1);
        if (k == "recent" && !v.empty() &&
            std::find(_recents.begin(), _recents.end(), v) == _recents.end())
            _recents.push_back(v);
        else if (k == "recent_model" && !v.empty() &&
                 std::find(_model_recents.begin(), _model_recents.end(), v) ==
                     _model_recents.end())
            _model_recents.push_back(v);
        else if (k == "colmap_exe" && !v.empty()) _colmap_exe = v;
        else if (k == "ffmpeg_exe" && !v.empty()) _ffmpeg_exe = v;
        else if (k == "python_exe" && !v.empty()) _python_exe = v;
        else if (k == "sfm_engine") _engine = v == "colmap" ? Engine::Colmap
                                                            : Engine::BuiltIn;
        else if (k == "accepted_license" && !v.empty() && !license_accepted(v))
            _accepted_licenses.push_back(v);
        else if (k == "lang" && !v.empty()) saved_lang = v;
        else if (k == "ui_scale") _scale.set_user(std::clamp((float)atof(v.c_str()),
                                                             0.0f, 3.0f));
        else if (k == "panel_w") _panel_w = (float)atof(v.c_str());
        else if (k == "ds_panel_w") _ds_panel_w = (float)atof(v.c_str());
        else if (k == "log_h") _log_h = (float)atof(v.c_str());
        else if (k == "show_log") _show_log = v != "0";
        else if (k == "log_details") _log_details = v != "0";
        else if (k == "show_preview") _show_preview = v != "0";
        else if (k == "preview_h") _preview_h = (float)atof(v.c_str());
        else if (k == "show_settings") _show_settings = v != "0";
        else if (k == "native_dialogs") _dialog.use_native(v != "0");
    }
    std::fclose(f);

    // A stored extent from a build whose defaults have moved, or a hand-edited
    // file, must not be able to leave a panel unreachably small or wide.
    _panel_w = std::clamp(_panel_w, 220.0f, 900.0f);
    _log_h = std::clamp(_log_h, 60.0f, 800.0f);
    _preview_h = std::clamp(_preview_h, 60.0f, 800.0f);
    _ds_panel_w = std::clamp(_ds_panel_w, 320.0f, 1200.0f);

    // The settings file is the third step of the chain and loses to both
    // --lang and SS_LANG, so the whole chain is re-run rather than the stored
    // value simply applied. Passing lang_arg() back in is what keeps --lang
    // winning even though it was parsed in main() long before this ran.
    spirula::i18n::init(spirula::i18n::lang_arg(), saved_lang.c_str());
}

void GuiApp::save_settings() {
    FILE* f = std::fopen(settings_path().c_str(), "w");
    if (!f) return;
    for (const auto& r : _recents)
        std::fprintf(f, "recent=%s\n", r.c_str());
    for (const auto& r : _model_recents)
        std::fprintf(f, "recent_model=%s\n", r.c_str());
    std::fprintf(f, "colmap_exe=%s\n", _colmap_exe.c_str());
    std::fprintf(f, "ffmpeg_exe=%s\n", _ffmpeg_exe.c_str());
    std::fprintf(f, "python_exe=%s\n", _python_exe.c_str());
    std::fprintf(f, "sfm_engine=%s\n",
                 _engine == Engine::Colmap ? "colmap" : "builtin");
    std::fprintf(f, "lang=%s\n", spirula::i18n::code(spirula::i18n::current()));
    std::fprintf(f, "ui_scale=%.3f\n", _scale.user());
    std::fprintf(f, "panel_w=%.1f\n", _panel_w);
    std::fprintf(f, "ds_panel_w=%.1f\n", _ds_panel_w);
    std::fprintf(f, "log_h=%.1f\n", _log_h);
    std::fprintf(f, "show_log=%d\n", _show_log ? 1 : 0);
    std::fprintf(f, "log_details=%d\n", _log_details ? 1 : 0);
    std::fprintf(f, "show_preview=%d\n", _show_preview ? 1 : 0);
    std::fprintf(f, "preview_h=%.1f\n", _preview_h);
    std::fprintf(f, "show_settings=%d\n", _show_settings ? 1 : 0);
    std::fprintf(f, "native_dialogs=%d\n", _dialog.native_enabled() ? 1 : 0);
    for (const auto& l : _accepted_licenses)
        std::fprintf(f, "accepted_license=%s\n", l.c_str());
    std::fclose(f);
}

void GuiApp::add_recent(std::string path) {
    _recents.erase(std::remove(_recents.begin(), _recents.end(), path),
                   _recents.end());
    _recents.insert(_recents.begin(), path);
    if (_recents.size() > 10) _recents.resize(10);
}

void GuiApp::add_model_recent(std::string path) {
    _model_recents.erase(
        std::remove(_model_recents.begin(), _model_recents.end(), path),
        _model_recents.end());
    _model_recents.insert(_model_recents.begin(), std::move(path));
    if (_model_recents.size() > 10) _model_recents.resize(10);
}

// One entry per SCREEN line: the panel clips with a list clipper and
// compensates its scroll for trimmed lines, and both want a uniform height.
void GuiApp::log(const std::string& s, bool detail) {
    size_t at = 0;
    do {
        const size_t nl = s.find('\n', at);
        _log.push_back({s.substr(at, nl == std::string::npos ? nl : nl - at),
                        detail});
        at = nl == std::string::npos ? nl : nl + 1;
    } while (at != std::string::npos);
    while (_log.size() > 4000) {
        if (!_log.front().detail || _log_details) _log_dropped++;
        _log.pop_front();
    }
    _log_shown_dirty = true;
}

void GuiApp::clear_log() {
    _log.clear();
    _log_shown.clear();
    _log_dropped = 0;
    _log_shown_dirty = true;
}

void GuiApp::append_logs() {
    for (auto& s : _runner.drain_log()) log(s);
    for (auto& l : _colmap.steps().drain()) log(l.text, l.detail);
    for (auto& l : _sfm.steps().drain()) log(l.text, l.detail);
    for (auto& s : _compare.drain_log()) log(s);
    for (auto& s : _mesh.drain_log()) log(s);
    for (auto& s : _download.drain_log()) log(s);
    for (auto& s : _font_download.drain_log()) log(s);
    // A finished font download is the one thing besides a language switch
    // that changes what the atlas should hold; GuiMain rebuilds it between
    // frames.
    if (_font_fetching &&
        _font_download.state() == FileDownload::State::Done) {
        _font_fetching = nullptr;
        _fonts.invalidate();
    }
}


// ===========================================================================
// Actions
// ===========================================================================

bool GuiApp::training_busy() const {
    TrainRunner::Phase ph = _runner.phase();
    return ph == TrainRunner::Phase::Training ||
           ph == TrainRunner::Phase::Preparing;
}

void GuiApp::apply_preset(const std::string& preset) {
    TrainConfig fresh;
    train_apply_preset(fresh, preset);
    // Keep GUI-managed context across preset switches.
    fresh.data = _cfg.data;
    fresh.image_dir = _cfg.image_dir;
    fresh.output_dir_prefix = _cfg.output_dir_prefix;
    fresh.output_dir_name = _cfg.output_dir_name;
    // The native viewport replaces the web viewer by default; it can be
    // re-enabled in Basic Options for remote monitoring.
    fresh.disable_viewer = true;
    _preset = preset;
    _preset_file.clear();
    _preset_display.clear();
    _preset_desc.clear();
    _preset_msg.clear();   // whatever it reported was about the last preset
    _cfg = fresh;
    _defaults = fresh;
    _cfg_ui.touched.clear();
    if (!_cfg.data.empty()) {
        detach_session_views();
        _runner.load_dataset(_cfg, _preset);
    }
}

// A saved preset carries the whole config, so this replaces more than a
// built-in preset does -- but the same four fields stay out of its reach
// (SS_PRESET_CONTEXT_FIELDS), because a preset is "how", not "where".
void GuiApp::apply_user_preset(const TrainPreset& p) {
    const TrainConfig stock;
    TrainConfig fresh = p.cfg;
    fresh.data = _cfg.data;
    fresh.resume = _cfg.resume;
    fresh.output_dir_prefix = _cfg.output_dir_prefix;
    fresh.output_dir_name = _cfg.output_dir_name;
    // The image and mask folders are ordinary dataset options and travel with
    // a preset, but a preset that never moved them must not move them here:
    // the handoff out of a reconstruction points them at folders that only
    // this dataset has.
    if (fresh.image_dir == stock.image_dir) fresh.image_dir = _cfg.image_dir;
    if (fresh.mask_dir == stock.mask_dir) fresh.mask_dir = _cfg.mask_dir;
    fresh.disable_viewer = true;   // as apply_preset: the native viewport

    _preset = p.base;
    _preset_file = p.path;
    _preset_display = p.name;
    _preset_desc = p.description;
    _preset_msg.clear();   // whatever it reported was about the last preset
    _cfg = fresh;
    _defaults = fresh;
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
    // Applying a preset re-parses the dataset, which takes the live session
    // down with it. Refuse rather than kill a run somebody is watching.
    if (training_busy() || _batch_active) {
        _preset_msg = dmsg::log_drop_while_training.get();
        _preset_msg_err = true;
        log(_preset_msg);
        return;
    }
    try {
        TrainPreset p = load_preset(path);
        apply_user_preset(p);
        _preset_msg = i18n::format(msg::preset_loaded, {p.name});
        _preset_msg_err = false;
    } catch (const std::exception& e) {
        _preset_msg = i18n::format(msg::preset_failed, {e.what()});
        _preset_msg_err = true;
    }
    log(_preset_msg);
    refresh_presets();
}

void GuiApp::refresh_presets() {
    // The picker rescans while its dropdown is open, so rate-limit: a scan is
    // a directory listing plus a small JSON parse per file, which is cheap
    // twice a second and silly at 120 Hz.
    double now = ImGui::GetTime();
    if (_presets_scanned_at >= 0.0 && now - _presets_scanned_at < 2.0) return;
    _presets_scanned_at = now;
    _presets = list_presets();
}

void GuiApp::open_dataset(std::string dir, std::string image_dir,
                          std::string mask_dir, bool mask_flipped,
                          bool keep_log) {
    if (dir.empty()) return;
    app::set_crash_note("opening dataset " + dir);
    close_mesh_preview();
    close_splat();
    // A different dataset means the log so far is about something else --
    // another capture's reconstruction, another run's warnings -- and keeping
    // it makes the panel read as if this dataset had already been worked on.
    //
    // Two exceptions, both "the log IS about this dataset": reopening the same
    // one (a reload, not a new job), and the handoff straight out of a
    // reconstruction, where the log is that reconstruction's and is the first
    // thing anyone would look at if the result seems wrong.
    if (dir != _cfg.data && !keep_log) clear_log();
    _cfg.data = dir;
    // image_dir / mask_dir: the runner hands its (possibly external) folders
    // over in-memory right after a run -- photos indexed where they are keep
    // their masks there too. Otherwise the dataparser defaults apply and the
    // user can set data.image_dir / data.mask_dir under Advanced.
    _cfg.image_dir = !image_dir.empty() ? image_dir : _defaults.image_dir;
    _cfg.mask_dir = !mask_dir.empty() ? mask_dir : _defaults.mask_dir;
    _cfg.flip_mask = mask_flipped;
    // Default the output next to the dataset -- much easier to find than a
    // CWD-relative "outputs" for someone who launched from a desktop icon.
    // Follows the dataset unless the user customized it (i.e. it still
    // matches the previous auto default).
    if (_cfg.output_dir_prefix == "outputs" ||
        _cfg.output_dir_prefix.empty() ||
        _cfg.output_dir_prefix == _defaults.output_dir_prefix)
        _cfg.output_dir_prefix = (fs::path(dir) / "outputs").string();
    _defaults.data = _cfg.data;
    _defaults.image_dir = _cfg.image_dir;
    _defaults.mask_dir = _cfg.mask_dir;
    _defaults.output_dir_prefix = _cfg.output_dir_prefix;
    add_recent(dir);
    save_settings();
    detach_session_views();
    // Training is the end of the dataset screen's business with the run, so
    // this is where what it left for the screen to read goes.
    reset_dataset_preview();
    _runner.load_dataset(_cfg, _preset);
    _screen = Screen::Train;
}

void GuiApp::request_open_dataset(std::string dir) {
    if (training_busy()) {
        _pending = Pending::OpenDataset;
        _pending_path = dir;
        _open_confirm = true;
        return;
    }
    open_dataset(dir);
}

void GuiApp::request_go_home() {
    if (training_busy()) {
        _pending = Pending::GoHome;
        _open_confirm = true;
        return;
    }
    // The mesh preview holds the engine through _splat and two attached
    // viewports; leaving the screen by ANY route (the button, the menu, the
    // deferred confirmation) has to hand them back.
    close_mesh_preview();
    close_splat();
    _screen = Screen::Home;
}

// A splat file, a step-*.ckpt directory, or a run directory. Loading is
// asynchronous (a large model is a few hundred MB), so this only starts it;
// frame() attaches the viewport once the splats are on the device.
void GuiApp::open_splat(std::string path) {
    if (path.empty()) return;
    app::set_crash_note("opening model " + path);
    clear_log();
    close_mesh_preview();
    detach_session_views();
    // Opening a file resets the engine; a finished run's session cannot be
    // rendered from once that has happened.
    _runner.note_engine_taken();
    _compare.open(path);
    add_model_recent(path);
    save_settings();
    _screen = Screen::Viewer;
}

void GuiApp::add_splat(std::string path) {
    if (path.empty()) return;
    _compare.add(path);
    add_model_recent(path);
    save_settings();
}

void GuiApp::request_open_splat(std::string path) {
    if (training_busy()) {
        _pending = Pending::OpenSplat;
        _pending_path = std::move(path);
        _open_confirm = true;
        return;
    }
    open_splat(std::move(path));
}

// The engine is a process-global singleton and the viewer is holding it, so
// this has to run before a training session can be set up. CompareView::close
// stops its render workers before handing the engine back.
void GuiApp::close_splat() {
    _compare.close();
    _mesh_preview_open = false;
}

void GuiApp::launch_training(const TrainConfig& cfg, const std::string& preset) {
    if (cfg.data.empty()) return;
    app::set_crash_note("training " + cfg.data);
    close_mesh_preview();
    close_splat();      // the engine is one object; the viewer has to let go
    detach_session_views();
    // Engine setup initializes the backend on the selected device; from
    // here on the device combo is display-only (one device per process).
    _device_locked = true;
    _runner.start_training(cfg, preset);
}

void GuiApp::start_training() {
    launch_training(_cfg, _preset);
}

void GuiApp::detach_session_views() {
    _viewport.detach();
    _images.detach();
    // Every caller is about to take the session away, so whatever the image
    // view was showing is gone and the next thing to look at is the sparse
    // points the 3D view puts up as soon as the dataset parses. Land there
    // rather than on the image view's empty message.
    _preview_images = false;
}

void GuiApp::request_close() {
    if (training_busy()) {
        _pending = Pending::Quit;
        _open_confirm = true;
        return;
    }
    _quit = true;
}

void GuiApp::run_pending_if_stopped() {
    if (_pending == Pending::None || training_busy()) return;
    // Only fire once the user confirmed the stop (or training was never
    // busy); a dismissed modal must not leave a delayed action armed.
    if (!_stop_confirmed) { _pending = Pending::None; return; }
    _stop_confirmed = false;
    Pending p = _pending;
    _pending = Pending::None;
    switch (p) {
        case Pending::GoHome:     _screen = Screen::Home; break;
        case Pending::OpenDataset: open_dataset(_pending_path); break;
        case Pending::OpenSplat:  open_splat(_pending_path); break;
        case Pending::Quit:       _quit = true; break;
        case Pending::StartBatch: start_batch(_pending_batch_skip); break;
        default: break;
    }
}


// ===========================================================================
// Batch training
//
// The queue itself is BatchTrain.h; what lives here is the three lines of
// state machine that turn it into runs. Everything a row needs -- parse,
// engine setup, the step loop, the checkpoint -- is one TrainRunner session,
// the same one the Start button makes, which is why a batch shows up on the
// trainer screen with a live viewport and plots rather than as a progress bar
// over a black box.
// ===========================================================================

// Append a row for `dataset`, seeded with whatever preset the trainer screen
// is on -- a queue is usually built right after tuning the settings it should
// run with. Shared by the picker, the recents menu and drag-and-drop.
void GuiApp::add_batch_row(const std::string& dataset) {
    if (dataset.empty()) return;
    BatchJob j;
    j.dataset = dataset;
    j.preset_path = _preset_file;
    j.preset_name = _preset_file.empty() ? _preset : _preset_display;
    _batch.push_back(std::move(j));
    _batch_dirty = true;
    _batch_checked = false;
}

void GuiApp::check_batch() {
    for (int i = 0; i < (int)_batch.size(); i++)
        _batch[i].issues = batch_check(_batch[i], _batch, i);
    _batch_checked = true;
    _batch_msg.clear();
    _batch_msg_err = false;
    bool any = false;
    for (const BatchJob& j : _batch) any = any || !j.issues.empty();
    if (!any && !_batch.empty()) _batch_msg = msg::batch_checked_ok.get();
}

void GuiApp::request_start_batch(bool skip_invalid) {
    if (training_busy()) {
        _pending = Pending::StartBatch;
        _pending_batch_skip = skip_invalid;
        _open_confirm = true;
        return;
    }
    start_batch(skip_invalid);
}

void GuiApp::start_batch(bool skip_invalid) {
    check_batch();

    int bad = 0, runnable = 0;
    for (const BatchJob& j : _batch) (batch_has_error(j) ? bad : runnable)++;
    if (bad > 0 && !skip_invalid) {
        _batch_msg = i18n::format(msg::batch_blocked, {(long long)bad});
        _batch_msg_err = true;
        return;
    }
    if (runnable == 0) {
        _batch_msg = msg::batch_no_runnable.get();
        _batch_msg_err = true;
        return;
    }

    for (BatchJob& j : _batch) {
        j.status = batch_has_error(j) ? BatchJob::Status::Skipped
                                      : BatchJob::Status::Pending;
        j.message.clear();
        j.out_dir.clear();
        j.steps = 0;
    }
    _batch_dirty = false;
    save_batch_list(_batch);

    _batch_active = true;
    _batch_launched = false;
    _batch_current = -1;
    _batch_stop_after = false;
    _batch_stop_now = false;
    _batch_msg = i18n::format(msg::batch_log_started, {(long long)runnable});
    _batch_msg_err = false;
    log(_batch_msg);
    // The run is worth watching even when nobody has to: same viewport, same
    // metrics, same log as a hand-started one.
    _screen = Screen::Train;
}

void GuiApp::advance_batch() {
    if (!_batch_active) return;

    if (_batch_launched) {
        const TrainRunner::Phase ph = _runner.phase();
        if (ph == TrainRunner::Phase::Preparing ||
            ph == TrainRunner::Phase::Training)
            return;   // still going
        if (_batch_current < 0 || _batch_current >= (int)_batch.size()) {
            finish_batch();   // the list moved under us; nothing to record
            return;
        }

        BatchJob& j = _batch[_batch_current];
        const long long n = _batch_current + 1;
        if (ph == TrainRunner::Phase::Done && !_batch_stop_now) {
            j.status = BatchJob::Status::Done;
            j.steps = _runner.latest_progress().step + 1;
            if (auto* s = _runner.session()) j.out_dir = s->out_dir.string();
            log(i18n::format(msg::batch_log_job_done, {n, j.out_dir}));
        } else if (ph == TrainRunner::Phase::Done) {
            j.status = BatchJob::Status::Stopped;
            log(i18n::format(msg::batch_log_job_stopped, {n}));
        } else {
            // Anything the pipeline threw: an unreadable dataset, an OOM, a
            // driver fault. Recorded on the row and left behind -- the point
            // of a queue is that the next dataset still gets its turn.
            j.status = BatchJob::Status::Failed;
            j.message = _runner.error();
            log(i18n::format(msg::batch_log_job_failed, {n, j.message}));
        }
        _batch_launched = false;
        _batch_current = -1;
        if (_batch_stop_after) { finish_batch(); return; }
    }

    int next = -1;
    for (int i = 0; i < (int)_batch.size(); i++)
        if (_batch[i].status == BatchJob::Status::Pending) { next = i; break; }
    if (next < 0) { finish_batch(); return; }

    BatchJob& j = _batch[next];
    TrainConfig cfg;
    std::string base, error;
    if (!batch_build_config(j, cfg, base, error)) {
        // The preset went missing between the pre-flight and now. Same
        // treatment as any other failure; the loop picks up the next row on
        // the following frame.
        j.status = BatchJob::Status::Failed;
        j.message = error;
        log(i18n::format(msg::batch_log_job_failed,
                         {(long long)(next + 1), error}));
        return;
    }

    j.status = BatchJob::Status::Running;
    _batch_current = next;
    _batch_launched = true;
    log(i18n::format(msg::batch_log_job_start,
                     {(long long)(next + 1), j.dataset}));
    launch_training(cfg, base);
}

void GuiApp::finish_batch() {
    int done = 0, failed = 0, other = 0;
    for (const BatchJob& j : _batch) {
        if (j.status == BatchJob::Status::Done) done++;
        else if (j.status == BatchJob::Status::Failed) failed++;
        else other++;
    }
    _batch_active = false;
    _batch_launched = false;
    _batch_current = -1;
    _batch_stop_after = false;
    _batch_stop_now = false;
    _batch_msg = i18n::format(msg::batch_log_summary,
                              {(long long)done, (long long)failed,
                               (long long)other});
    _batch_msg_err = failed > 0;
    log(_batch_msg);
}

void GuiApp::cancel_batch() {
    if (!_batch_active) return;
    if (_batch_current >= 0 && _batch_current < (int)_batch.size())
        _batch[_batch_current].status = BatchJob::Status::Stopped;
    _batch_launched = false;
    _batch_current = -1;
    finish_batch();
}

static bool is_image_ext(const fs::path& p) {
    std::string e = p.extension().string();
    for (auto& c : e) c = (char)std::tolower((unsigned char)c);
    return e == ".jpg" || e == ".jpeg" || e == ".png" || e == ".webp" ||
           e == ".tif" || e == ".tiff" || e == ".bmp";
}

// Is this folder a finished model rather than a dataset -- a run directory, or
// one of its checkpoints? Then it belongs in the viewer.
static bool looks_like_model(const fs::path& p) {
    std::error_code ec;
    if (fs::is_regular_file(p / "splat.ply", ec)) return true;
    for (fs::directory_iterator it(p, ec), end; !ec && it != end; it.increment(ec)) {
        const std::string b = it->path().filename().string();
        if (it->is_directory(ec) &&
            (b.rfind("step-", 0) == 0 || b.find(".ckpt") != std::string::npos) &&
            fs::is_regular_file(it->path() / "splat.ply", ec))
            return true;
    }
    return false;
}

void GuiApp::handle_drop(const std::vector<std::string>& paths) {
    std::error_code ec;
    // The mesh screen is asking two specific questions -- which model, and
    // which photos -- so a drop there ANSWERS one of them instead of
    // navigating away. Dropping a splat .ply on a screen whose first field
    // wants a splat .ply and landing in the viewer is the kind of thing that
    // makes a user stop trusting drag and drop.
    // A finished preview does not block it: dropping a second model right
    // after looking at the first is exactly how someone works through a
    // folder of runs. (A run in flight does -- its inputs are already gone
    // to the child.)
    if (_screen == Screen::Mesh && !_mesh.busy() && paths.size() == 1) {
        const fs::path p(paths[0]);
        std::string ext = p.extension().string();
        for (char& c : ext) c = (char)std::tolower((unsigned char)c);
        const bool is_file = fs::is_regular_file(p, ec);
        const bool is_dir = fs::is_directory(p, ec);
        // A splat .ply, a *.ckpt / run folder -> the model. A folder that is
        // only a dataset -> the photos. A folder that is BOTH (a dataset with
        // a splat.ply sitting in it, which is what an in-place workflow
        // produces) is the model, and the child reads the dataset out of the
        // run's config.json or out of the same folder.
        if (is_file && ext == ".ply" && !meshing::ply_is_mesh(paths[0])) {
            set_mesh_source(paths[0]);
            return;
        }
        if (is_dir && looks_like_model(p)) {
            set_mesh_source(paths[0]);
            // The same folder is often the dataset too; offer it rather than
            // making the user type it again.
            if (folder_looks_like_dataset(paths[0]))
                _mesh_job.data_dir = paths[0];
            return;
        }
        if (is_dir && folder_looks_like_dataset(paths[0])) {
            close_mesh_preview();
            _mesh_job.use_data = true;
            _mesh_job.data_dir = paths[0];
            return;
        }
    }

    // A preset, or the config.json of a run that came out well. Checked first
    // and by content rather than by name: it is a file that changes settings,
    // never a dataset or a model, so there is nothing else it could be.
    if (paths.size() == 1 && fs::is_regular_file(paths[0], ec) &&
        is_preset_file(paths[0])) {
        load_preset_file(paths[0]);
        if (!_preset_msg_err && !_cfg.data.empty()) _screen = Screen::Train;
        return;
    }
    // Dropping dataset folders onto the batch screen extends the queue, which
    // is how a five-dataset run gets set up without typing five paths.
    if (_screen == Screen::Batch && !_batch_active) {
        std::vector<std::string> datasets;
        for (const std::string& p : paths)
            if (fs::is_directory(p, ec) && folder_looks_like_dataset(p))
                datasets.push_back(p);
        if (datasets.size() == paths.size() && !datasets.empty()) {
            for (const std::string& d : datasets) add_batch_row(d);
            return;
        }
    }
    // Several models dropped together open side by side, which is what the
    // viewer is for.
    {
        std::vector<std::string> models;
        for (const std::string& p : paths) {
            if (!fs::is_regular_file(p, ec)) break;
            std::string ext = fs::path(p).extension().string();
            for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
            if (std::find(kViewableExtensions.begin(), kViewableExtensions.end(),
                          ext) == kViewableExtensions.end())
                break;
            models.push_back(p);
        }
        if (models.size() == paths.size() && models.size() > 1 &&
            !training_busy()) {
            open_splat(models[0]);
            for (size_t i = 1; i < models.size() &&
                               i < (size_t)CompareView::kMaxModels; i++)
                add_splat(models[i]);
            if (models.size() > (size_t)CompareView::kMaxModels)
                log(msg::compare_full.get());
            return;
        }
    }
    // A dataset is opened, not added to a list, so it only makes sense alone --
    // and dropping one alongside videos is far more likely a mis-drag than a
    // request to do both.
    if (paths.size() == 1 && fs::is_directory(paths[0], ec) &&
        looks_like_model(paths[0])) {
        // Checked before the dataset test: a run directory sits INSIDE the
        // dataset it was trained from often enough that both would match.
        request_open_splat(paths[0]);
        return;
    }
    if (paths.size() == 1 && fs::is_directory(paths[0], ec) &&
        folder_looks_like_dataset(paths[0])) {
        request_open_dataset(paths[0]);
        return;
    }
    if (paths.size() == 1 && fs::is_regular_file(paths[0], ec) &&
        !is_video_path(paths[0])) {
        // A file from inside a dataset (transforms.json, database.db, a
        // COLMAP .bin/.txt, a Metashape camera .xml) opens the dataset it
        // belongs to.
        const fs::path p(paths[0]);
        std::string ext = p.extension().string();
        for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
        // A .ply is a model to look at -- Gaussians, a point cloud or a
        // mesh, which the viewer works out for itself; .obj/.gltf/.glb can
        // only be a mesh. (A Metashape dataset FOLDER is caught above, before
        // this, so dropping one still opens the dataset.)
        if (std::find(kViewableExtensions.begin(), kViewableExtensions.end(),
                      ext) != kViewableExtensions.end()) {
            request_open_splat(p.string());
            return;
        }
        if (p.filename() == "transforms.json" || ext == ".db" ||
            ext == ".bin" || ext == ".txt" || ext == ".xml") {
            request_open_dataset(p.parent_path().string());
            return;
        }
    }

    // Everything else is raw input: videos, and folders of photos.
    std::vector<std::string> sources;
    for (const std::string& path : paths) {
        const fs::path p(path);
        if (fs::is_directory(p, ec)) {
            if (folder_has_images(path)) sources.push_back(path);
            else log(i18n::format(dmsg::log_drop_no_images, {path}));
        } else if (fs::is_regular_file(p, ec)) {
            if (is_video_path(path)) sources.push_back(path);
            else log(i18n::format(dmsg::log_drop_unsupported, {path}));
        }
    }
    if (sources.empty()) return;
    if (training_busy()) {
        log(dmsg::log_drop_while_training.get());
        return;
    }
    // Dropping onto the dataset screen adds to what is already listed there;
    // dropping from anywhere else starts a new dataset.
    add_sources(sources, /*replace=*/_screen != Screen::NewDataset);
    _screen = Screen::NewDataset;
}

// Default COLMAP workspace: never point at an existing non-empty directory
// (e.g. a previous run) -- append _2, _3, ... instead of overwriting.
static std::string fresh_workspace(const std::string& base) {
    std::error_code ec;
    if (!fs::exists(base, ec) || fs::is_empty(base, ec)) return base;
    for (int i = 2; i < 1000; i++) {
        std::string cand = base + "_" + std::to_string(i);
        if (!fs::exists(cand, ec) || fs::is_empty(cand, ec)) return cand;
    }
    return base;
}

// The Insta360 X5 focal length: fx = fy ~ 0.269 * image width on every X5
// dataset measured. A known focal makes fisheye initialization reliable, and a
// fisheye started from the generic guess often does not initialize at all.
static constexpr float kInsta360FocalFactor = 0.269f;

// Is this folder the `images/` of a dataset folder, rather than a folder of
// photos that happens to hold them? (resolve_photo_folder is what put us here.)
static bool named_images(const fs::path& p) {
    std::string n = p.filename().empty() ? p.parent_path().filename().string()
                                         : p.filename().string();
    for (char& c : n) c = (char)std::tolower((unsigned char)c);
    return n == "images";
}

// A file or folder name that can be a directory of its own: what a path
// separator, a colon or a space would do to `--camera-model DIR=MODEL` is not
// worth finding out.
static std::string sanitize_name(std::string s) {
    for (char& c : s) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
        if (!ok) c = '_';
    }
    while (!s.empty() && s.front() == '.') s.erase(s.begin());
    return s.empty() ? std::string("input") : s;
}

// Folder names that say what is inside rather than which capture it is.
// `/lab/images` and `/lab/omni/images` are two different inputs whose own
// names are both "images", so the folder ABOVE is what tells them apart.
static bool is_generic_folder_name(std::string n) {
    for (char& c : n) c = (char)std::tolower((unsigned char)c);
    return n == "images" || n == "image" || n == "img" || n == "imgs" ||
           n == "photos" || n == "pictures" || n == "pics" || n == "frames" ||
           n == "input" || n == "inputs" || n == "data";
}

// Names that mean something else inside a dataset: a per-input folder called
// `images` produces `<ws>/images/images`, which is the layout `sfm auto`'s
// nested-images shorthand exists for -- and taking that shorthand there drops
// every other input from the reconstruction. Cheaper to never write the name.
static bool is_reserved_dataset_name(std::string n) {
    for (char& c : n) c = (char)std::tolower((unsigned char)c);
    return n == "images" || n == "masks" || n == "sparse" || n == "features" ||
           n == "depths" || n == "normals" || n == "colmap" || n == "outputs";
}

// The sub-folder one input's frames go into, before de-duplication: the
// video's own name, or the photo folder's -- climbing past a folder whose name
// only describes its contents, so two captures picked as `X/images` and
// `Y/images` come out as `X` and `Y` rather than `images` and `images_2`.
static std::string source_folder_base(const PrepInput& s) {
    fs::path p(s.path);
    if (s.is_video) return sanitize_name(p.stem().string());
    if (!p.empty() && p.filename().empty()) p = p.parent_path();  // trailing '/'
    for (int up = 0; up < 2 && !p.empty(); up++) {
        std::string n = p.filename().string();
        if (n.empty()) break;
        if (!is_generic_folder_name(n)) return sanitize_name(n);
        p = p.parent_path();
    }
    // Nothing but generic names all the way up: keep the leaf, but never as a
    // name the dataset layout already uses.
    fs::path leaf(s.path);
    if (!leaf.empty() && leaf.filename().empty()) leaf = leaf.parent_path();
    std::string n = sanitize_name(leaf.filename().string());
    return is_reserved_dataset_name(n) ? n + "_input" : n;
}

// Click sources that name an input the list no longer holds.
static std::vector<std::string> dropped_click_sources(
    const std::vector<PrepInput>& sources, const std::vector<MaskClick>& clicks) {
    std::vector<std::string> gone;
    for (const MaskClick& c : clicks) {
        if (c.source.empty()) continue;
        bool here = false;
        for (const PrepInput& s : sources) here = here || s.path == c.source;
        if (!here && std::find(gone.begin(), gone.end(), c.source) == gone.end())
            gone.push_back(c.source);
    }
    return gone;
}

// Inputs no click prompts, named as the picker names them. A clicks-only job
// needs this empty; DatasetPrep::run refuses it otherwise.
static std::string inputs_without_clicks(const std::vector<PrepInput>& sources,
                                         const std::vector<MaskClick>& clicks) {
    std::string out;
    for (const PrepInput& s : sources) {
        bool prompted = false;
        for (const MaskClick& c : clicks)
            prompted = prompted || c.source.empty() || c.source == s.path;
        if (prompted) continue;
        if (!out.empty()) out += ", ";
        out += s.subdir.empty() ? s.path : s.subdir;
    }
    return out;
}

void GuiApp::refresh_sources() {
    // One input keeps the layout a one-video dataset has always had: frames
    // straight into images/ (and cam0/, cam1/ under it for a dual-lens file).
    // Several need a folder each, which is also what makes them separate
    // cameras -- named after the file so the log and the camera list read like
    // what the user picked.
    std::vector<std::string> taken;
    for (size_t i = 0; i < _sources.size(); i++) {
        PrepInput& s = _sources[i];
        if (_sources.size() < 2) {
            s.subdir.clear();
            continue;
        }
        std::string base = source_folder_base(s);
        if (is_reserved_dataset_name(base)) base += "_input";
        std::string name = base;
        for (int n = 2; std::find(taken.begin(), taken.end(), name) != taken.end();
             n++)
            name = base + "_" + std::to_string(n);
        taken.push_back(name);
        s.subdir = name;
    }

    if (_mask_preview_input >= (int)_sources.size()) _mask_preview_input = 0;

    // Clicks describe one input's frames; when it leaves the list, they go.
    const std::vector<std::string> gone_inputs =
        dropped_click_sources(_sources, _mask.clicks);
    for (const std::string& gone : gone_inputs) {
        log(i18n::format(dmsg::log_clicks_dropped_input_gone, {gone}));
        auto& v = _mask.clicks;
        v.erase(std::remove_if(v.begin(), v.end(),
                               [&](const MaskClick& c) { return c.source == gone; }),
                v.end());
    }
    if (!gone_inputs.empty() && _mask.clicks.empty()) {
        _mask.object_count = 1;
        _mask.current_object = 0;
    }

    // Camera folders inside each input. A capture that arrives already split
    // into cam/, cam0/, cam1/ needs one lens each -- one of them being a
    // fisheye does not make the others one -- and the folders are the only
    // place that shows.
    for (PrepInput& s : _sources) {
        std::vector<std::string> found;
        if (!s.is_video && !s.path.empty()) {
            std::error_code ec;
            if (fs::is_directory(s.path, ec)) found = camera_subfolders(s.path);
        }
        // One folder is a nested layout, not a choice to make.
        if (found.size() < 2) {
            s.subcameras.clear();
            continue;
        }
        std::vector<SubCamera> next;
        next.reserve(found.size());
        for (const std::string& rel : found) {
            SubCamera sc;
            sc.rel = rel;
            for (const SubCamera& old : s.subcameras)
                if (old.rel == rel) sc = old;
            next.push_back(std::move(sc));
        }
        s.subcameras.swap(next);
    }

    // The output folder follows the input until the user takes it over.
    if (_workspace.empty() || _workspace == _workspace_auto) {
        std::string base;
        // ... and normally lands on a folder of its own, suffixed _2, _3, ...
        // rather than pointing at something that already has content in it.
        bool exact = false;
        if (_sources.empty()) {
            base.clear();
        } else if (_sources.size() == 1) {
            const fs::path p(_sources[0].path);
            if (_sources[0].is_video) {
                base = (p.parent_path() / (p.stem().string() + "_dataset")).string();
            } else if (named_images(p)) {
                // A dataset folder: images/ (and masks/) are already where every
                // parser looks for them, so the reconstruction belongs beside
                // them as sparse/ -- in that folder, not in a copy of it with a
                // suffix. Whatever it would replace is warned about instead
                // (draw_dataset_source).
                base = p.parent_path().string();
                exact = true;
            } else {
                base = _sources[0].path + "_dataset";
            }
        } else {
            // Several inputs have no single name; the folder they came from is
            // the closest thing to one.
            const fs::path p(_sources[0].path);
            const fs::path dir = p.parent_path();
            base = (dir / (dir.filename().string() + "_dataset")).string();
        }
        _workspace = base.empty() ? "" : (exact ? base : fresh_workspace(base));
        _workspace_auto = _workspace;
    }
}

// One picked path -> the input it describes, with the defaults its kind wants.
static PrepInput make_source(const std::string& path,
                             bool use_found_masks) {
    std::error_code ec;
    PrepInput s;
    s.path = path;
    s.is_video = !fs::is_directory(path, ec) && is_video_path(path);
    // What a folder of photos means: the images/ under it when there is one,
    // and the masks/ that belongs to them. Resolved here, on the path the user
    // picked, so the row shows the folder that will actually be indexed rather
    // than one that merely contains it.
    if (!s.is_video) {
        resolve_photo_folder(path, s.path, s.mask_dir);
        if (!use_found_masks) s.mask_dir.clear();
    }
    // Every input carries a concrete lens, so a list holding a 360 camera and a
    // phone cannot end up applying one of them to the other.
    //
    // 360-camera preset for Insta360 .insv files: the two fisheye tracks land
    // in one folder per lens (one camera each), the thin-prism fisheye model
    // fits them, and the known focal length above starts them off.
    s.camera_model = "opencv";
    if (is_dual_fisheye_path(path)) {
        s.camera_model = "thin-prism-fisheye";
        // Commented - We don't assume every camera is Insta360 X5
        // s.focal_factor = kInsta360FocalFactor;
    }
    return s;
}

// Attach a picked `masks/` folder to the input whose images it describes --
// the one it sits beside (`<root>/images` + `<root>/masks`) or the one it sits
// under (photos at `<root>` with `<root>/masks`). Returns false when no input
// on the list owns it, which is the only case worth a message.
static bool attach_mask_folder(std::vector<PrepInput>& sources,
                               const std::string& masks) {
    std::error_code ec;
    const fs::path parent = fs::absolute(masks, ec).parent_path();
    for (PrepInput& s : sources) {
        if (s.is_video) continue;
        const fs::path images = fs::absolute(s.path, ec);
        if (fs::equivalent(images.parent_path(), parent, ec) ||
            fs::equivalent(images, parent, ec)) {
            s.mask_dir = fs::absolute(masks, ec).string();
            return true;
        }
    }
    return false;
}

// Re-answer "are there masks beside these photos?" after the switch moves.
// Turning it off forgets them; turning it back on has to look again, because
// the answer was thrown away rather than remembered.
bool GuiApp::any_found_masks() const {
    for (const PrepInput& s : _sources)
        if (!s.is_video && !s.mask_dir.empty()) return true;
    return false;
}

void GuiApp::rescan_found_masks() {
    for (PrepInput& s : _sources) {
        if (s.is_video) continue;
        if (!_use_found_masks) {
            s.mask_dir.clear();
        } else {
            std::string images = s.path;
            resolve_photo_folder(s.path, images, s.mask_dir);
        }
    }
    refresh_sources();
}

void GuiApp::add_sources(const std::vector<std::string>& paths, bool replace) {
    // A folder of masks is not an input of its own: it belongs to one, and the
    // images half may be in the same pick, in either order. Split before
    // anything else so that picking masks alone cannot clear the list.
    std::vector<std::string> inputs, mask_folders;
    for (const std::string& path : paths) {
        std::error_code ec;
        if (fs::is_directory(path, ec) && is_mask_folder(path))
            mask_folders.push_back(path);
        else
            inputs.push_back(path);
    }
    if (replace && !inputs.empty()) {
        _sources.clear();
        _mask_preview_input = 0;
        // A different capture is a different job, and the geometry options
        // are remembered nowhere: a run must never quietly cost an hour of
        // inference nobody asked for. (Not mid-run: that would disable it.)
        if (!dataset_busy()) _geometry = GeometryJob{};
        // ... and a different capture is a different colour space.
        _sfm_job.image_gamut.clear();
        _sfm_job.image_is_linear.reset();
        _color_space_touched = false;
    }
    for (const std::string& path : inputs) _sources.push_back(make_source(path, _use_found_masks));
    for (const std::string& masks : mask_folders) {
        if (attach_mask_folder(_sources, masks))
            log(i18n::format(dmsg::log_masks_attached, {masks}));
        else
            log(i18n::format(dmsg::log_masks_orphaned, {masks}));
    }
    if (_sources.empty()) return;

    // The engine-wide settings follow whatever the list is now. A video is a
    // capture in order; a folder of photos is not.
    const bool video = _sources[0].is_video;
    const bool fisheye = is_dual_fisheye_path(_sources[0].path);
    _sfm_job.data_type = video ? 1 : 0;
    _sfm_job.pairs = 0;               // automatic
    // Matching stays on "Automatic", which is NOT sequential for a dual-lens
    // video: the two lens tracks are concatenated rather than temporally
    // interleaved, so temporal neighbours miss every cross-lens pair (on a real
    // X5 capture that topped out at 68/118 registered frames). Automatic gives
    // every pair below a hundred images -- which is what beat it, at 116/118 --
    // and GPU pair selection above that, which is content-based and so keeps
    // the cross-lens pairs without the quadratic cost. Forcing exhaustive made
    // a long capture unusably slow, and it also suppresses that switch.
    _colmap_job.matcher = (video && !fisheye) ? 2 : 1;
    _colmap_job.seq_loop_closure = true;   // if switched to sequential
    if (fisheye) {
        _colmap_job.camera_model = "THIN_PRISM_FISHEYE";
        _colmap_job.init_focal_factor = kInsta360FocalFactor;
    }
    // Several inputs are several cameras, and so is one dual-lens file.
    if (_sources.size() > 1 || fisheye) {
        _sfm_job.camera_mode = 1;
        _colmap_job.camera_mode = 1;
    }
    if (_mask_preview_input >= (int)_sources.size()) _mask_preview_input = 0;
    adopt_exr_color_space();
    refresh_sources();
}

// A folder of EXRs declares its own colour space, and the picker for it is
// under Advanced where nobody would think to look. Fill it in and say so.
void GuiApp::adopt_exr_color_space() {
    if (_color_space_touched) return;
    std::error_code ec;
    for (const PrepInput& s : _sources) {
        if (s.is_video) continue;
        for (fs::recursive_directory_iterator it(s.path, ec), end;
             !ec && it != end; it.increment(ec)) {
            if (!it->is_regular_file(ec)) continue;
            std::string e = it->path().extension().string();
            for (char& c : e) c = (char)std::tolower((unsigned char)c);
            if (e != ".exr") continue;
            exr::Info info;
            if (!exr::declared_color_space(it->path().string(), info)) return;
            _sfm_job.image_gamut = info.gamut;
            _sfm_job.image_is_linear = info.is_linear;
            log(i18n::format(dmsg::log_exr_color_space,
                             {info.gamut.empty() ? "Rec.709" : info.gamut}));
            return;
        }
    }
}

void GuiApp::handle_dialog_result(const std::vector<std::string>& paths) {
    const std::string path = paths.empty() ? std::string() : paths[0];
    switch (_pick) {
        case PickAction::OpenDataset:
            request_open_dataset(path);
            break;
        case PickAction::SourceImages:
        case PickAction::SourceVideo:
            add_sources(paths, /*replace=*/_screen != Screen::NewDataset);
            _screen = Screen::NewDataset;
            break;
        case PickAction::SourceReplace:
            if (!path.empty() && _pick_source >= 0 &&
                _pick_source < (int)_sources.size()) {
                _sources[_pick_source] = make_source(path, _use_found_masks);
                refresh_sources();
            }
            _pick_source = -1;
            break;
        case PickAction::Workspace:
            _workspace = path;
            break;
        case PickAction::OutputPrefix:
            _cfg.output_dir_prefix = path;
            break;
        case PickAction::VocabTree:
            _colmap_job.vocab_tree_path = path;
            break;
        case PickAction::SplatFile:
            request_open_splat(path);
            break;
        case PickAction::AddSplatFile:
            add_splat(path);
            break;
        case PickAction::MeshSource:
            set_mesh_source(path);
            break;
        case PickAction::MeshPhotos:
            _mesh_job.data_dir = path;
            break;
        case PickAction::MeshOutput:
            // Only the folder is picked; the file name keeps whatever
            // set_mesh_source derived, as the preset save dialog does.
            if (!path.empty())
                _mesh_job.output =
                    (fs::path(path) /
                     fs::path(_mesh_job.output.empty() ? "mesh"
                                                       : _mesh_job.output)
                         .filename())
                        .string();
            break;
        case PickAction::PresetFile:
            load_preset_file(path);
            break;
        case PickAction::PresetSaveFolder:
            // Only the folder is picked; the file name stays whatever the
            // name field derived, so the two halves of the path keep their
            // separate owners.
            if (!path.empty())
                _preset_save_path =
                    (fs::path(path) / fs::path(_preset_save_path).filename())
                        .string();
            break;
        case PickAction::BatchDataset:
            if (!path.empty()) {
                if (_pick_row >= 0 && _pick_row < (int)_batch.size()) {
                    _batch[_pick_row].dataset = path;
                    _batch_dirty = true;
                    _batch_checked = false;
                } else {
                    // Several folders picked at once become several rows.
                    for (const std::string& p : paths) add_batch_row(p);
                }
            }
            _pick_row = -1;
            break;
        case PickAction::BatchOutput:
            if (_pick_row >= 0 && _pick_row < (int)_batch.size()) {
                _batch[_pick_row].output_dir = path;
                _batch_dirty = true;
                _batch_checked = false;
            }
            _pick_row = -1;
            break;
        case PickAction::BatchPresetFile:
            if (!path.empty() && _pick_row >= 0 &&
                _pick_row < (int)_batch.size()) {
                try {
                    TrainPreset p = load_preset(path);
                    _batch[_pick_row].preset_path = p.path;
                    _batch[_pick_row].preset_name = p.name;
                    _batch_dirty = true;
                    _batch_checked = false;
                } catch (const std::exception& e) {
                    _batch_msg = i18n::format(msg::preset_failed, {e.what()});
                    _batch_msg_err = true;
                }
            }
            _pick_row = -1;
            break;
        default:
            break;
    }
    _pick = PickAction::None;
}


// ===========================================================================
// Frame
// ===========================================================================

void GuiApp::frame() {
    // Before the first widget: a style swap halfway through a frame would
    // measure half the window against one scale and half against the other.
    _scale.update(ImGui::GetIO().DisplaySize);

    append_logs();
    run_pending_if_stopped();
    // vit-giant2 is two files, and so is ALIKED with LightGlue: both fetches
    // are queues, stepped on from somewhere that runs whatever screen is up.
    _geom_download.pump();
    _feat_download.pump();
    // Before the reload check below: between two batch rows the runner is
    // briefly idle, and a stale _parse_dirty would start a dataset parse right
    // where the next row wants the engine.
    advance_batch();

    // The queue survives a restart, so it is written back once the widget
    // being edited is idle -- the same deferral the dataset options use.
    if (_batch_dirty && !ImGui::IsAnyItemActive()) {
        _batch_dirty = false;
        save_batch_list(_batch);
    }
    // Same deferral for a dragged splitter: the file would otherwise be
    // rewritten on every frame of the drag.
    if (_layout_dirty && !ImGui::IsAnyItemActive()) {
        _layout_dirty = false;
        save_settings();
    }

    // Dataset-parsing option changed: re-parse once the user finishes
    // editing (no active widget), so the preview / camera counts stay in
    // sync with the config. Never fires while training (options are locked).
    if (_parse_dirty && !training_busy() && !_batch_active &&
        _runner.phase() != TrainRunner::Phase::Loading &&
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
        if (!_viewport.attached() && _runner.session())
            _viewport.attach(*_runner.session());
        // The photograph-vs-render mode needs the DataManager as well as the
        // splats, so it waits for engine setup exactly as the viewport does.
        if (!_images.attached() && _runner.session())
            _images.attach(*_runner.session());
    } else if (_runner.phase() == TrainRunner::Phase::Ready) {
        if (!_viewport.preview_active() && _runner.session())
            _viewport.attach_preview(*_runner.session());
    }

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

    if (_dialog.draw()) handle_dialog_result(_dialog.results());
    // The save dialog steps aside while the folder picker is up; bring it
    // back once the picker is gone, confirmed or cancelled either way.
    if (_preset_save_reopen && !_dialog.is_open()) {
        _preset_save_reopen = false;
        _preset_save_open = true;
    }
    draw_preset_save_modal();
    draw_preset_delete_modal();
    draw_confirm_modal();
    draw_data_error_modal();

    ImGui::End();
}

void GuiApp::draw_menu_bar() {
    if (!ImGui::BeginMenuBar()) return;
    if (ui::BeginMenu(msg::menu_file)) {
        if (ui::MenuItem(msg::menu_open_dataset)) {
            _pick = PickAction::OpenDataset;
            _dialog.open(msg::menu_open_dataset.get(), FileDialog::Mode::Folder);
        }
        if (ui::MenuItem(msg::menu_new_dataset)) _screen = Screen::NewDataset;
        if (ui::MenuItem(msg::menu_open_splat)) {
            _pick = PickAction::SplatFile;
            _dialog.open(msg::viewer_pick_file.get(), FileDialog::Mode::File,
                         {".ply"});
        }
        ImGui::Separator();
        if (ui::MenuItem(msg::menu_batch)) _screen = Screen::Batch;
        ImGui::Separator();
        if (ui::MenuItem(msg::menu_quit)) request_close();
        ImGui::EndMenu();
    }
    if (ui::BeginMenu(msg::menu_view)) {
        if (ui::MenuItem(msg::menu_show_log, nullptr, &_show_log))
            _layout_dirty = true;
        if (ui::MenuItem(msg::menu_show_settings, nullptr, &_show_settings))
            _layout_dirty = true;
        if (ui::MenuItem(msg::menu_reset_layout)) {
            _panel_w = kDefaultPanelW;
            _log_h = kDefaultLogH;
            _show_log = _show_settings = true;
            _layout_dirty = true;
        }
        ImGui::Separator();
        if (ui::BeginMenu(msg::menu_ui_size)) {
            const float cur = _scale.user();
            if (ui::MenuItem(msg::ui_size_auto, nullptr, cur <= 0.0f)) {
                _scale.set_user(0.0f);
                _layout_dirty = true;
            }
            ImGui::Separator();
            // Percentages, which are the same in every language.
            for (float f : {0.75f, 0.9f, 1.0f, 1.15f, 1.35f, 1.6f, 2.0f}) {
                char label[16];
                std::snprintf(label, sizeof label, "%d%%", (int)(f * 100.0f + 0.5f));
                if (ui::MenuItemRaw(label, std::fabs(cur - f) < 1e-3f)) {
                    _scale.set_user(f);
                    _layout_dirty = true;
                }
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        {
            bool native = _dialog.native_enabled();
            ImGui::BeginDisabled(!_dialog.native_available());
            if (ui::MenuItem(msg::menu_native_dialogs, nullptr, &native)) {
                _dialog.use_native(native);
                _layout_dirty = true;
            }
            ImGui::EndDisabled();
            ui::help_on_hover_disabled(msg::native_dialogs_help);
        }
        ImGui::EndMenu();
    }
    draw_language_menu();
    if (ui::BeginMenu(msg::menu_help)) {
        if (ui::BeginMenu(msg::menu_about)) {
            ui::Text(spirula::i18n::msg::brand::product);
            ui::TextDisabled(spirula::i18n::msg::brand::about_line);
            ui::TextDisabledRaw("github.com/harry7557558/spirulae-splat");
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
}

// The picker. Everything about it is designed for a user who cannot read the
// language currently on screen, because that is who needs it:
//
//   - the menu bar entry is SS_LANG_MENU_ICON (文A) rather than the word
//     "Language" translated into the language they cannot read, followed by
//     the current language's own name so the menu also reports its state
//   - the entries are the native names, never translated: someone looking for
//     their own language is looking for the word they call it by
//   - all thirteen render without a download, which is what the embedded
//     subsets in assets/fonts/ are for (src/app/gui/Fonts.h)
//
// Below the list, when the full face for this language is not installed, the
// offer to fetch it. Not a warning: the UI reads fine without it, and what it
// adds is coverage for text this program did not write.
void GuiApp::draw_language_menu() {
    const i18n::Lang before = i18n::current();
    const std::string title =
        std::string(SS_LANG_MENU_ICON " ") + i18n::native_name(before);
    if (!ui::BeginMenuRaw(ui::detail::label(title, msg::menu_language))) return;

    for (unsigned i = 0; i < i18n::kLangCount; i++) {
        const i18n::Lang l = i18n::Lang(i);
        if (ui::SelectableRaw(i18n::native_name(l), l == before) && l != before) {
            i18n::set_current(l);
            save_settings();
        }
    }

    if (const CjkFace* f = _fonts.optional_face()) {
        ImGui::Separator();
        ImGui::PushTextWrapPos(px(360.0f));
        // The language's own name, not CjkFace::label -- that one is English
        // ("Korean"), which is the one word in this sentence a reader of a
        // Korean UI did not ask for.
        ui::TextDisabled(msg::font_needed,
                         {i18n::native_name(before), human_bytes(f->bytes)});
        ImGui::PopTextWrapPos();
        if (!FontSet::fetch_enabled()) {
            ui::TextDisabled(msg::font_no_fetch);
        } else if (_font_download.state() == FileDownload::State::Running) {
            ui::ProgressBar(std::max(_font_download.progress(), 0.0f),
                            ImVec2(340, 0), msg::font_downloading);
        } else {
            if (ui::Button(msg::font_download, ImVec2(340, 0))) {
                _font_fetching = f;
                _font_download.start(f->url, cjk_face_download_path(*f), f->bytes);
            }
            if (_font_download.state() == FileDownload::State::Failed)
                ui::TextColoredWrapped(kErr, msg::font_failed,
                                       {_font_download.status()});
        }
    }
    ImGui::EndMenu();
}


// ===========================================================================
// Home
// ===========================================================================

namespace {

// The masthead texture, decoded and uploaded on the first home screen. There
// is one window and the image never changes, so it outlives every caller and
// the driver frees it with the context.
unsigned banner_texture(int* w, int* h) {
    static int bw = 0, bh = 0;
    static const unsigned tex = [] {
        int comp = 0;
        unsigned char* px = stbi_load_from_memory(
            kAppBanner, (int)kAppBannerSize, &bw, &bh, &comp, 4);
        if (!px) return 0u;
        unsigned id = 0;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bw, bh, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, px);
        stbi_image_free(px);
        return id;
    }();
    *w = bw;
    *h = bh;
    return tex;
}

}  // namespace

// The home masthead: the artwork across the whole window, with the product
// name and tagline over its foot. The words are drawn here rather than baked
// into the image so they stay translatable, and the image carries none.
void GuiApp::draw_home_banner(float avail, float indent) {
    int bw = 0, bh = 0;
    const unsigned tex = banner_texture(&bw, &bh);
    if (!tex || bw <= 0 || bh <= 0) return;

    // A share of the window rather than a fixed height: a short window should
    // not lose a third of itself to the masthead. Clamped at both ends -- the
    // floor is what the name and tagline need, the ceiling keeps it a band.
    const float h = std::clamp(ImGui::GetContentRegionAvail().y * 0.21f,
                               px(120.0f), px(320.0f));
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 p1(p0.x + avail, p0.y + h);

    // Cover, not fit: crop to the band's aspect so the artwork fills the width
    // whatever the window is doing.
    const float want = avail / h, have = (float)bw / (float)bh;
    ImVec2 uv0(0.0f, 0.0f), uv1(1.0f, 1.0f);
    const float f = have > want ? want / have : have / want;
    if (have > want) {
        uv0.x = 0.5f - 0.5f * f;
        uv1.x = 0.5f + 0.5f * f;
    } else {
        uv0.y = 0.5f - 0.5f * f;
        uv1.y = 0.5f + 0.5f * f;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddImage((ImTextureID)(intptr_t)tex, p0, p1, uv0, uv1);
    // The artwork is bright everywhere, so the text needs its own ground.
    const ImU32 clear = IM_COL32(14, 15, 18, 0), dark = IM_COL32(14, 15, 18, 232);
    dl->AddRectFilledMultiColor(ImVec2(p0.x, p0.y + h * 0.30f), p1,
                                clear, clear, dark, dark);

    ImGui::SetCursorScreenPos(ImVec2(p0.x + indent, p1.y - px(76.0f)));
    ImGui::SetWindowFontScale(1.7f);
    ui::Text(spirula::i18n::msg::brand::product);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::SetCursorScreenPos(ImVec2(p0.x + indent, p1.y - px(30.0f)));
    ui::TextColored(kDim, spirula::i18n::msg::brand::tagline);

    ImGui::SetCursorScreenPos(ImVec2(p0.x, p1.y));
}

void GuiApp::draw_home() {
    // The column is centred and never wider than the window: at 480 px it is
    // wide enough for the longest button label in any language, and on a
    // narrow window it gives up width rather than running text off the edge.
    const float avail = ImGui::GetContentRegionAvail().x;
    const float w = std::max(std::min(px(480.0f), avail - px(16.0f)), px(200.0f));
    const float bh = px(42.0f);
    const float indent = std::max(0.0f, (avail - w) * 0.5f);

    draw_home_banner(avail, indent);

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::BeginChild("##home", ImVec2(w, 0));
    ImGui::Dummy(ImVec2(0, px(24.0f)));

    // A session (possibly still training) exists -- offer the way back.
    if (_runner.phase() != TrainRunner::Phase::Idle) {
        if (ui::Button(training_busy() ? msg::home_back_to_training
                                       : msg::home_back_to_trainer,
                       ImVec2(-1, bh)))
            _screen = Screen::Train;
        ImGui::Dummy(ImVec2(0, px(10.0f)));
    }

    if (ui::Button(msg::home_open_dataset, ImVec2(-1, bh))) {
        _pick = PickAction::OpenDataset;
        _dialog.open(msg::menu_open_dataset.get(), FileDialog::Mode::Folder);
    }
    ui::help_on_hover(msg::home_open_dataset_help);

    // Photos and video are one screen and one input list: a capture can hold
    // both, so splitting the entry point in two only asked a question with no
    // right answer. The list is added to there, or dropped onto the window.
    if (ui::Button(msg::home_new_dataset, ImVec2(-1, bh)))
        _screen = Screen::NewDataset;
    ui::help_on_hover(msg::home_new_dataset_help);

    if (ui::Button(msg::home_open_splat, ImVec2(-1, bh))) {
        _pick = PickAction::SplatFile;
        _dialog.open(msg::viewer_pick_file.get(), FileDialog::Mode::File,
                     kViewableExtensions);
    }
    ui::help_on_hover(msg::home_open_splat_help);

    if (ui::Button(msg::home_make_mesh, ImVec2(-1, bh))) _screen = Screen::Mesh;
    ui::help_on_hover(msg::home_make_mesh_help);

    if (ui::Button(msg::home_batch, ImVec2(-1, bh))) _screen = Screen::Batch;
    ui::help_on_hover(msg::home_batch_help);

    ImGui::Spacing();
    ui::TextDisabledWrapped(msg::home_drop_hint);

    if (!_recents.empty()) {
        ImGui::Dummy(ImVec2(0, px(18.0f)));
        ui::SeparatorText(msg::home_recent);
        const float row_w = ImGui::GetContentRegionAvail().x;
        for (size_t i = 0; i < _recents.size(); i++) {
            ImGui::PushID((int)i);
            // A path is a path in every language. It is elided from the
            // middle, where a dataset path repeats what the ones above it
            // already said; the ends are what tells two of them apart.
            if (ui::SelectableRaw(elide_middle(_recents[i], row_w)))
                request_open_dataset(_recents[i]);
            if (ImGui::IsItemHovered()) ui::SetTooltipRaw(_recents[i]);
            ImGui::PopID();
        }
    }

    ImGui::Dummy(ImVec2(0, px(18.0f)));
    // Only worth saying when there is genuinely nothing that can do the job.
    if (!builtin_sfm_available() && !colmap_available())
        ui::TextColored(kDim, msg::home_no_engine);

    ImGui::EndChild();
}


// ===========================================================================
// New Dataset screen
//
// One screen for both reconstruction engines. The top half -- where the
// photos or video are, where the dataset goes, and what to mask out -- is the
// same either way and is all a first-time user should have to read. The
// engine-specific knobs are below a collapsed "Advanced" header, and the
// engine selector only appears when there is genuinely a choice to make.
// ===========================================================================

bool GuiApp::builtin_sfm_available() const {
    return SfmRunner::availability().empty();
}

bool GuiApp::colmap_available() const {
    return command_exists(_colmap_exe);
}

GuiApp::Engine GuiApp::effective_engine() const {
    if (!builtin_sfm_available()) return Engine::Colmap;
    if (!colmap_available()) return Engine::BuiltIn;
    return _engine;
}

bool GuiApp::dataset_busy() const {
    return _sfm.state() == SfmRunner::State::Running ||
           _colmap.state() == ColmapRunner::State::Running;
}

RunProgress* GuiApp::dataset_steps() {
    return effective_engine() == Engine::BuiltIn ? &_sfm.steps() : &_colmap.steps();
}

bool GuiApp::dataset_locked(Stage s) {
    if (!dataset_busy()) return false;
    const bool builtin = _sfm.state() == SfmRunner::State::Running;
    return (builtin ? _sfm.steps() : _colmap.steps()).ran(s);
}

void GuiApp::cancel_dataset_job() {
    _sfm.cancel();
    _colmap.cancel();
}

bool GuiApp::license_accepted(const std::string& family) const {
    return std::find(_accepted_licenses.begin(), _accepted_licenses.end(),
                     family) != _accepted_licenses.end();
}

std::string GuiApp::selected_model_path() const {
    const ModelEntry* e = find_model(_model_id);
    if (!e || !model_is_cached(*e)) return "";
    return model_path(*e);
}

// Fetch the selected checkpoint, from wherever the screen offers it. Consent
// first, every time the family has not been agreed to (ModelCache.h says why).
void GuiApp::request_model_download() {
    const ModelEntry* e = find_model(_model_id);
    if (!e || model_is_cached(*e)) return;
    if (_download.state() == ModelDownload::State::Running) return;
    if (license_accepted(e->family)) {
        _download.start(*e);
    } else {
        _license_prompt = e->family;
        _license_tick = false;
    }
}

// Would the run reach the built-in masker and find no checkpoint? That is a
// download, not something the run can do anything about, so it is asked before
// starting rather than reported twenty minutes in.
bool GuiApp::mask_model_missing() const {
    if (!_mask_enable || _sfm_job.prep.force_external_masking) return false;
    if (!backends().builtin_masking) return false;
    // Inputs that arrived with their own masks are never segmented, so a job
    // made only of those needs no model at all.
    bool all_bring_masks = !_sources.empty();
    for (const PrepInput& s : _sources)
        all_bring_masks = all_bring_masks && !s.mask_dir.empty();
    if (all_bring_masks) return false;
    return selected_model_path().empty();
}

const WorkspaceState& GuiApp::workspace_state() {
    std::string key = _workspace;
    for (const PrepInput& s : _sources) key += '\n' + s.path + '\n' + s.mask_dir;
    const double now = ImGui::GetTime();
    if (key != _ws_state_key || _ws_state_at < 0.0 || now - _ws_state_at > 1.0) {
        _ws_state_key = std::move(key);
        _ws_state_at = now;
        _ws_state = probe_workspace(_workspace, _sources);
    }
    return _ws_state;
}

// The panel edits one set of fields; each runner gets its own struct because
// their remaining options do not overlap. This is the one place they are
// copied across, so a field cannot be set on the screen and silently not run.
void GuiApp::sync_dataset_jobs() {
    PrepJob prep;
    prep.inputs = _sources;
    // The checkbox decides whether the run is given the stencils; the drawings
    // stay on _sources either way, so switching it off does not lose them.
    if (!_border_enable)
        for (PrepInput& in : prep.inputs) in.stencil = app::FrameStencil{};
    prep.workspace = _workspace;
    prep.resume = _resume;
    prep.video_fps = _sfm_job.prep.video_fps;
    prep.sharp_window = _sfm_job.prep.sharp_window;
    prep.max_frames = _sfm_job.prep.max_frames;
    prep.force_external_decode = _sfm_job.prep.force_external_decode;
    prep.ffmpeg_exe = _ffmpeg_exe;
    prep.python_exe = _python_exe;
    prep.mask_enable = _mask_enable;
    prep.flip_found_masks = _use_found_masks && _flip_found_masks;
    prep.mask_prompt = _mask.prompt;
    prep.mask_negative_prompt = _mask.negative_prompt;
    prep.mask_keep_subject = _mask.keep_subject;
    prep.mask_max_image_size = _mask.max_image_size;
    prep.mask_threshold = _mask.threshold;
    prep.mask_nms = _mask.nms;
    prep.mask_memory = _mask_memory;
    prep.mask_detect_every = _mask_detect_every;
    prep.mask_memory_frames = _mask_memory_frames;
    prep.mask_clicks = _mask.clicks;
    prep.mask_model_path = selected_model_path();
    prep.force_external_masking = _sfm_job.prep.force_external_masking;
    if (const ModelEntry* e = find_model(_model_id))
        prep.mask_model_name = e->legacy_name;
    _sfm_job.prep = prep;
    // The dataset-wide lens is the lone input's; with several, each input names
    // its own and this is only what an image no override covers would get.
    if (!_sources.empty()) _sfm_job.camera_model = _sources[0].camera_model;

    _colmap_job.inputs = prep.inputs;
    _colmap_job.workspace = prep.workspace;
    _colmap_job.resume = prep.resume;
    _colmap_job.video_fps = prep.video_fps;
    _colmap_job.sharp_window = prep.sharp_window;
    _colmap_job.max_frames = prep.max_frames;
    _colmap_job.force_external_decode = prep.force_external_decode;
    _colmap_job.force_external_masking = prep.force_external_masking;
    _colmap_job.colmap_exe = _colmap_exe;
    _colmap_job.ffmpeg_exe = _ffmpeg_exe;
    _colmap_job.python_exe = _python_exe;
    _colmap_job.mask_enable = prep.mask_enable;
    _colmap_job.mask_prompt = prep.mask_prompt;
    _colmap_job.mask_negative_prompt = prep.mask_negative_prompt;
    _colmap_job.mask_keep_subject = prep.mask_keep_subject;
    _colmap_job.mask_max_image_size = prep.mask_max_image_size;
    _colmap_job.mask_threshold = prep.mask_threshold;
    _colmap_job.mask_nms = prep.mask_nms;
    _colmap_job.mask_memory = prep.mask_memory;
    _colmap_job.mask_detect_every = prep.mask_detect_every;
    _colmap_job.mask_memory_frames = prep.mask_memory_frames;
    _colmap_job.mask_clicks = prep.mask_clicks;
    _colmap_job.mask_model_path = prep.mask_model_path;
    _colmap_job.mask_model = prep.mask_model_name;

    _sfm_job.prep.redo_frames = _colmap_job.redo_frames = _redo_frames;
    _sfm_job.prep.redo_masks = _colmap_job.redo_masks = _redo_masks;
    _sfm_job.redo_model = _colmap_job.redo_model = _redo_model;
    // The same step either way: `spirula geometry` over the finished dataset.
    _sfm_job.geometry = _colmap_job.geometry = _geometry;
    _sfm_job.geometry.overwrite = _colmap_job.geometry.overwrite =
        _geometry.overwrite || _redo_geometry;
    // A settings file written by a build that HAS the inference layer must not
    // make a run on one that has not fail at the last step.
    _sfm_job.geometry.enable = _colmap_job.geometry.enable =
        _geometry.enable && geometry_availability().empty();
    // One colour space for the whole run: SfM, masking and geometry all read
    // the same photographs.
    _colmap_job.image_gamut = _sfm_job.image_gamut;
    _colmap_job.image_is_linear = _sfm_job.image_is_linear;
    _sfm_job.geometry.image_gamut = _colmap_job.geometry.image_gamut =
        _sfm_job.image_gamut;
    _sfm_job.geometry.image_is_linear = _colmap_job.geometry.image_is_linear =
        _sfm_job.image_is_linear;
    _sfm_job.prep.image_gamut = _sfm_job.image_gamut;
    _sfm_job.prep.image_is_linear = _sfm_job.image_is_linear;
}

void GuiApp::update_dataset_job() {
    if (!dataset_busy()) return;
    sync_dataset_jobs();
    if (_sfm.state() == SfmRunner::State::Running) _sfm.update(_sfm_job);
    else                                           _colmap.update(_colmap_job);
}

void GuiApp::start_dataset_job() {
    app::set_crash_note("building dataset " + _workspace);
    sync_dataset_jobs();
    // The preview holds a multi-gigabyte backbone; the run about to start
    // wants that VRAM for reconstruction.
    _segment.close();
    _geometry_panel.close();
    reset_dataset_preview();
    const RunFilms films{&_film_frames, &_film_masks, &_film_geometry};
    if (effective_engine() == Engine::BuiltIn) _sfm.start(_sfm_job, films);
    else                                      _colmap.start(_colmap_job, films);
    // One run each: a re-do that stayed armed would throw the same step away
    // again the next time the button is pressed.
    _redo_frames = _redo_masks = _redo_model = _redo_geometry = false;
}

// ---------------------------------------------------------------------------
// Source, destination, resume
// ---------------------------------------------------------------------------

void GuiApp::draw_dataset_source() {
    // One row per input. Several videos reconstruct as one scene -- each gets
    // its own folder of frames under images/, and so its own camera.
    int remove = -1;
    bool edited = false;
    for (size_t i = 0; i < _sources.size(); i++) {
        PrepInput& s = _sources[i];
        ImGui::PushID((int)i);
        // Room for Browse + Remove + where the frames go, which is longer than
        // any other row on the screen.
        ImGui::SetNextItemWidth(px(-380.0f));
        if (ui::InputTextRaw("##in", &s.path)) {
            std::error_code ec;
            s.is_video = !fs::is_directory(s.path, ec) && is_video_path(s.path);
            edited = true;
        }
        // A typed path is only worth resolving once it is finished -- rewriting
        // `<folder>` to `<folder>/images` under the cursor is not helpful.
        if (ImGui::IsItemDeactivatedAfterEdit() && !s.is_video) {
            resolve_photo_folder(s.path, s.path, s.mask_dir);
            if (!_use_found_masks) s.mask_dir.clear();
            edited = true;
        }
        ImGui::SameLine();
        if (ui::Button(dmsg::browse)) {
            _pick = PickAction::SourceReplace;
            _pick_source = (int)i;
            if (s.is_video)
                _dialog.open(msg::pick_video_file.get(), FileDialog::Mode::File,
                             video_dialog_filters());
            else
                _dialog.open(msg::pick_photo_folder.get(), FileDialog::Mode::Folder);
        }
        ImGui::SameLine();
        if (ui::Button(dmsg::remove)) remove = (int)i;
        ImGui::SameLine();
        // What this input is, and -- the part worth seeing before pressing the
        // button -- whether masks were found for it. Four whole messages
        // rather than a kind + a "+ masks" tail: the two do not compose in
        // that order in every language.
        const bool masked = !s.mask_dir.empty();
        if (_sources.size() > 1) {
            const Msg& row = s.is_video
                ? (masked ? dmsg::row_video_masks_to : dmsg::row_video_to)
                : (masked ? dmsg::row_photos_masks_to : dmsg::row_photos_to);
            ui::TextDisabled(row, {s.subdir});
        } else {
            ui::Text(s.is_video
                         ? (masked ? dmsg::kind_video_file_masks
                                   : dmsg::kind_video_file)
                         : (masked ? dmsg::kind_photo_folder_masks
                                   : dmsg::kind_photo_folder));
        }
        if (masked && ImGui::IsItemHovered())
            ui::SetTooltip(dmsg::existing_masks_tooltip, {s.mask_dir});
        ImGui::PopID();
    }
    if (remove >= 0) {
        _sources.erase(_sources.begin() + remove);
        edited = true;
    }
    if (edited) refresh_sources();

    if (ui::Button(dmsg::add_video)) {
        _pick = PickAction::SourceVideo;
        _dialog.open(msg::pick_videos.get(), FileDialog::Mode::File,
                     video_dialog_filters(), "", /*multi_select=*/true);
    }
    ui::help_on_hover(dmsg::add_video_help);
    ImGui::SameLine();
    if (ui::Button(dmsg::add_photos)) {
        _pick = PickAction::SourceImages;
        _dialog.open(msg::pick_photo_folder.get(), FileDialog::Mode::Folder);
    }
    ui::help_on_hover(dmsg::add_photos_help);
    if (_sources.empty()) {
        ImGui::SameLine();
        ui::TextDisabled(dmsg::no_input_yet);
    }

    // Masks that came WITH the photos are adopted automatically, which is
    // right for a prepared capture and wrong for a folder whose masks/ happens
    // to describe something else. Only offered when there are photo inputs to
    // find masks for -- a video has none by definition.
    bool any_photos = false;
    for (const PrepInput& s : _sources) any_photos = any_photos || !s.is_video;
    if (any_photos) {
        if (ui::Checkbox(dmsg::use_found_masks, &_use_found_masks))
            rescan_found_masks();
        ui::help_on_hover(dmsg::use_found_masks_help);
        // Which way round they are is a property of those files, so it is asked
        // here rather than per stage: everything the run writes comes out in
        // the one convention, whatever the folder arrived in.
        if (_use_found_masks && any_found_masks()) {
            ImGui::Indent();
            ui::Checkbox(dmsg::flip_found_masks, &_flip_found_masks);
            ui::help_on_hover(dmsg::flip_found_masks_help);
            ImGui::Unindent();
        }
    }

    ImGui::SetNextItemWidth(px(-220.0f));
    ui::InputTextRaw("##ws", &_workspace);
    ImGui::SameLine();
    // Two "Browse..." buttons in one scope: the message supplies the ID, so
    // they need distinguishing exactly as two identical literals would.
    ImGui::PushID("ws");
    if (ui::Button(dmsg::browse)) {
        _pick = PickAction::Workspace;
        _dialog.open(msg::pick_output_folder.get(), FileDialog::Mode::Folder);
    }
    ImGui::PopID();
    ImGui::SameLine();
    ui::Text(dmsg::output_folder);

    // What is in there already, split into what a run can pick up and what it
    // would write over. The output folder is often the input folder -- that is
    // the point of the images/ + masks/ layout -- so the input's own images do
    // not count as either (probe_workspace).
    const WorkspaceState& prior = workspace_state();
    if (prior.resumable()) {
        ui::Checkbox(dmsg::resume_previous, &_resume);
        ui::help_on_hover(dmsg::resume_previous_help);
        ImGui::SameLine();
        ui::TextDisabled(dmsg::unfinished_run_detected);
    }
    // A finished dataset in the output folder is REUSED, which is what lets a
    // capture somebody else reconstructed be given masks, depth and normals
    // without an hour of rebuilding it.
    if (prior.model) {
        ui::TextColoredWrapped(_redo_model ? kWarn : kOk,
                               _redo_model ? dmsg::model_will_be_replaced
                                           : dmsg::model_found_reuse);
        ui::Checkbox(dmsg::reconstruct_again, &_redo_model);
        ui::help_on_hover(dmsg::reconstruct_again_help);
    }
}

// ---------------------------------------------------------------------------
// The basics
// ---------------------------------------------------------------------------

namespace {

// The closed picker's tooltip: what the control decides, then what the model
// standing in it is for. Two paragraphs rather than one message, because only
// the second one changes with the selection.
void lens_tooltip(const Msg& model_help) {
    if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) ||
        !ImGui::BeginTooltip())
        return;
    ImGui::PushTextWrapPos(px(420.0f));
    ui::Text(dmsg::camera_lens_help);
    ImGui::Separator();
    ui::Text(model_help);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

// The lens pickers. ImGui's Combo takes a flat list of strings and has
// nowhere to hang a description on a row, so the popup is built by hand:
// which physical camera each model is for is the whole of the question, and
// one tooltip on the closed combo cannot answer it row by row.
bool sfm_lens_combo(const char* id, int* idx) {
    const auto labels = sfm_camera_model_labels();
    const auto helps = sfm_camera_model_helps();
    if (!ui::BeginComboRaw(id, labels[(size_t)*idx]->get())) return false;
    bool changed = false;
    for (int i = 0; i < (int)labels.size(); i++) {
        if (ui::Selectable(*labels[(size_t)i], i == *idx)) {
            *idx = i;
            changed = true;
        }
        ui::help_on_hover(*helps[(size_t)i]);
    }
    ImGui::EndCombo();
    return changed;
}

// The same for COLMAP's, whose rows are COLMAP's own model names -- what a
// user who read its documentation is looking for -- with the translated
// sentence beside them rather than instead of them.
bool colmap_lens_combo(const char* id, int* idx) {
    const auto helps = colmap_camera_model_helps();
    if (!ui::BeginComboRaw(id, kColmapCameraModels[*idx])) return false;
    bool changed = false;
    for (int i = 0; i < kNumColmapCameraModels; i++) {
        if (ui::SelectableRaw(kColmapCameraModels[i], i == *idx)) {
            *idx = i;
            changed = true;
        }
        ui::help_on_hover(*helps[(size_t)i]);
    }
    ImGui::EndCombo();
    return changed;
}

}  // namespace

void GuiApp::draw_dataset_basics() {
    const bool builtin = effective_engine() == Engine::BuiltIn;

    // Everything down to the frame-rate control is read by feature extraction
    // and after; the frame settings below it were read the moment the run
    // began. Hence two guards rather than one round the lot.
    ImGui::BeginDisabled(dataset_locked(Stage::Features));

    // Quality means the same thing to both engines even though it moves
    // different knobs, so it is one control.
    ImGui::SetNextItemWidth(px(220.0f));
    if (builtin) {
        ui::Combo(dmsg::quality, &_sfm_job.quality,
                  {&dmsg::quality_fast, &dmsg::quality_balanced,
                   &dmsg::quality_high_recommended, &dmsg::quality_maximum});
        ui::help_on_hover(dmsg::quality_help_builtin);
    } else {
        ui::Combo(dmsg::quality, &_colmap_job.quality,
                  {&dmsg::quality_fast, &dmsg::quality_balanced,
                   &dmsg::quality_high});
        ui::help_on_hover(dmsg::quality_help_colmap);
    }

    // Lens model. The two engines spell the models differently; the labels
    // are the same either way, which is what the user is choosing between.
    //
    // With several inputs the built-in engine gets one lens per input instead
    // (draw_source_cameras) -- a rig that carries a 360 camera and a phone is
    // one capture with two lens models in it, and forcing one on both makes
    // half of it unusable. COLMAP's feature_extractor takes a single model for
    // the run, so that path keeps one control and says so.
    // One lens each once there is more than one camera to tell apart: several
    // inputs, or one input that arrived already split into camera folders.
    // Only under "one camera per folder", though -- a single shared camera
    // makes the choice meaningless, and one camera per image makes it a list
    // nobody wants to read.
    bool any_subcameras = false;
    for (const PrepInput& s : _sources)
        any_subcameras = any_subcameras || !s.subcameras.empty();
    const bool per_folder = _sfm_job.camera_mode == 1;
    const bool per_input_lens =
        builtin && per_folder && (_sources.size() > 1 || any_subcameras);
    if (!per_input_lens) {
        ImGui::SetNextItemWidth(px(220.0f));
        if (builtin) {
            std::string& model =
                _sources.empty() ? _sfm_job.camera_model : _sources[0].camera_model;
            int idx = 0;
            for (int i = 0; i < kNumSfmCameraModels; i++)
                if (model == kSfmCameraModels[i]) idx = i;
            if (sfm_lens_combo(ui::detail::label(dmsg::camera_lens), &idx))
                model = kSfmCameraModels[idx];
            lens_tooltip(*sfm_camera_model_helps()[idx]);
        } else {
            int idx = 0;
            for (int i = 0; i < kNumColmapCameraModels; i++)
                if (_colmap_job.camera_model == kColmapCameraModels[i]) idx = i;
            if (colmap_lens_combo(ui::detail::label(dmsg::camera_lens), &idx))
                _colmap_job.camera_model = kColmapCameraModels[idx];
            lens_tooltip(*colmap_camera_model_helps()[idx]);
        }
        if (!_sources.empty())
            draw_lens_warning(_sources[0].path, _sources[0].is_video,
                              builtin ? _sources[0].camera_model
                                      : _colmap_job.camera_model,
                              builtin);
        if (!builtin && _sources.size() > 1)
            ui::TextColoredWrapped(kWarn, dmsg::colmap_one_lens_warning);
    } else {
        draw_source_cameras();
    }

    ImGui::SetNextItemWidth(px(220.0f));
    ui::Combo(dmsg::camera_sharing,
              builtin ? &_sfm_job.camera_mode : &_colmap_job.camera_mode,
              {&dmsg::camera_sharing_one, &dmsg::camera_sharing_folder,
               &dmsg::camera_sharing_image});
    ui::help_on_hover(dmsg::camera_sharing_help);

    if (builtin) {
        ImGui::SetNextItemWidth(px(220.0f));
        ui::Combo(dmsg::image_matching, &_sfm_job.pairs,
                  {&dmsg::matching_automatic, &dmsg::matching_every_pair,
                   &dmsg::matching_neighbours, &dmsg::matching_gpu_preselect});
        ui::help_on_hover(dmsg::matching_help_builtin);
    } else {
        int matcher_idx = _colmap_job.matcher - 1;
        if (matcher_idx < 0 || matcher_idx > 2)
            matcher_idx = (!_sources.empty() && _sources[0].is_video) ? 1 : 0;
        ImGui::SetNextItemWidth(px(220.0f));
        ui::Combo(dmsg::image_matching, &matcher_idx,
                  {&dmsg::matching_exhaustive, &dmsg::matching_sequential,
                   &dmsg::matching_vocab_tree});
        _colmap_job.matcher = matcher_idx + 1;
        ui::help_on_hover(dmsg::matching_help_colmap);
        if (_colmap_job.matcher == 2) {
            ImGui::Indent();
            ui::Checkbox(dmsg::loop_closure, &_colmap_job.seq_loop_closure);
            ui::help_on_hover(dmsg::loop_closure_help_colmap);
            ImGui::Unindent();
        }
    }

    ImGui::EndDisabled();

    bool any_video = false;
    for (const PrepInput& s : _sources) any_video = any_video || s.is_video;
    if (any_video) {
        ImGui::BeginDisabled(dataset_locked(Stage::Frames));
        ImGui::SetNextItemWidth(px(220.0f));
        ui::InputFloat(dmsg::frames_per_second, &_sfm_job.prep.video_fps,
                       0, 0, "%.2g");
        ui::help_on_hover(dmsg::frames_per_second_help);
        ImGui::SetNextItemWidth(px(220.0f));
        ui::SliderInt(dmsg::sharpness_window, &_sfm_job.prep.sharp_window, 1, 8);
        ui::help_on_hover(dmsg::sharpness_window_help);
        if (!backends().builtin_video) {
            // What the note says is a build-configuration diagnostic and
            // stays English; the sentence around it does not.
            ImGui::PushTextWrapPos();
            ui::TextDisabled(dmsg::build_note, {backends().video_note});
            ImGui::PopTextWrapPos();
            ui::help_on_hover_raw(backends().video_reason.c_str());
        }
        ImGui::EndDisabled();
    }
    if (dataset_busy()) ui::help_on_hover_disabled(dmsg::step_locked);
}

// One lens per input, in place of the single "Camera / lens" control, once
// there is more than one input to tell apart. The reconstruction takes these as
// per-folder overrides (SfmRunner::append_camera_overrides), which is what lets
// a 360 clip and a phone clip reconstruct as one scene without either being
// fitted with the other's model.
void GuiApp::draw_source_cameras() {
    ui::Text(dmsg::camera_lens_per_input);
    ui::help_on_hover(dmsg::camera_lens_per_input_help);
    ImGui::Indent();
    // `label` names the row, `dir` is the folder it measures, and model/focal
    // are what the row edits -- an input with no camera folders under it is
    // one row, an input with three is three.
    auto row = [&](const char* id, const std::string& label,
                   const std::string& dir, std::string& model, float& focal) {
        ImGui::PushID(id);
        ui::TextRaw(label);
        if (ImGui::IsItemHovered()) ui::SetTooltipRaw(dir);
        ImGui::SameLine(px(200.0f));
        ImGui::SetNextItemWidth(px(220.0f));
        int idx = 0;
        for (int m = 0; m < kNumSfmCameraModels; m++)
            if (model == kSfmCameraModels[m]) idx = m;
        if (sfm_lens_combo("##lens", &idx)) model = kSfmCameraModels[idx];
        lens_tooltip(*sfm_camera_model_helps()[(size_t)idx]);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(px(90.0f));
        ui::InputFloat(dmsg::focal_x_width, &focal, 0, 0, "%.4g");
        ui::help_on_hover(dmsg::focal_x_width_help);
        draw_lens_warning(dir, /*is_video=*/false, model, /*builtin=*/true);
        ImGui::PopID();
    };

    for (size_t i = 0; i < _sources.size(); i++) {
        PrepInput& s = _sources[i];
        ImGui::PushID((int)i);
        if (s.subcameras.empty()) {
            const std::string label =
                s.subdir.empty() ? fs::path(s.path).filename().string() : s.subdir;
            row("input", label, s.path, s.camera_model, s.focal_factor);
        } else {
            // The input itself is a heading here: every image under it is in
            // one of the camera folders, so the input's own lens covers none.
            if (_sources.size() > 1) {
                ui::TextDisabledRaw(s.subdir.empty() ? s.path : s.subdir);
                ImGui::Indent();
            }
            for (size_t k = 0; k < s.subcameras.size(); k++) {
                SubCamera& sc = s.subcameras[k];
                if (sc.camera_model.empty()) sc.camera_model = s.camera_model;
                row(sc.rel.c_str(), sc.rel,
                    (fs::path(s.path) / sc.rel).string(), sc.camera_model,
                    sc.focal_factor);
            }
            if (_sources.size() > 1) ImGui::Unindent();
        }
        ImGui::PopID();
    }
    ImGui::Unindent();
}

bool GuiApp::input_pixel_size(const std::string& path, bool is_video,
                              int& w, int& h) {
    w = h = 0;
    if (path.empty()) return false;
    const std::string& s_path = path;
    auto it = _input_size.find(path);
    if (it == _input_size.end()) {
        std::pair<int, int> size{0, 0};
        std::error_code ec;
        if (is_video) {
            // One `ffmpeg -i`, and only for a file that is already there:
            // this runs from the draw, and a path being typed names nothing.
            if (fs::is_regular_file(s_path, ec)) {
                static const std::atomic<bool> never{false};
                VideoFacts f;
                if (ffmpeg_probe_video(_ffmpeg_exe, s_path, f, never))
                    size = {f.width, f.height};
            }
        } else if (fs::is_directory(s_path, ec)) {
            int iw = 0, ih = 0;
            if (DatasetPrep::first_image_dims(s_path, iw, ih)) size = {iw, ih};
        }
        it = _input_size.emplace(path, size).first;
    }
    w = it->second.first;
    h = it->second.second;
    return w > 0 && h > 0;
}

void GuiApp::draw_lens_warning(const std::string& path, bool is_video,
                               const std::string& model, bool builtin) {
    const bool fisheye = builtin ? sfm_model_is_fisheye(model)
                                 : colmap_model_is_fisheye(model);
    // COLMAP has no panorama model, so the question only arises for the
    // built-in engine.
    const bool pano = builtin && model == "equirectangular";
    // A dual-lens file is two fisheye circles per frame whatever its pixel
    // dimensions are, so this one needs no measurement.
    if (is_dual_fisheye_path(path)) {
        if (pano) ui::TextColoredWrapped(kWarn, dmsg::lens_warn_dual_fisheye);
        else if (!fisheye)
            ui::TextColoredWrapped(kWarn, dmsg::lens_warn_needs_fisheye);
        return;
    }
    if (!pano) return;
    int w = 0, h = 0;
    if (!input_pixel_size(path, is_video, w, h)) return;
    if (std::fabs((double)w / (double)h - 2.0) <= 0.02) return;
    ui::TextColoredWrapped(kWarn, dmsg::lens_warn_not_2to1, {w, h});
}

// ---------------------------------------------------------------------------
// Masking
// ---------------------------------------------------------------------------

void GuiApp::open_mask_preview() {
    if (_sources.empty()) {
        log(dmsg::mask_pick_input_first.get());
        return;
    }
    const PrepInput& s = _sources[(size_t)_mask_preview_input];
    // One backbone on the device at a time; see open_geometry_preview.
    _geometry_panel.close();
    // The preview reads the video the same way preparation will: in process
    // where the driver can, ffmpeg where it cannot or where the job said to.
    _segment.open(s.path, s.is_video, _mask_enable ? selected_model_path() : "",
                  _ffmpeg_exe, _sfm_job.prep.force_external_decode);
}

void GuiApp::draw_masking_options() {
    // Masks that came with an input need no checkbox and no model: they are
    // already the answer. Say so where the question is asked, because the
    // alternative is a user turning masking on to "make sure" and waiting
    // twenty minutes for masks they already had.
    int with_masks = 0;
    for (const PrepInput& s : _sources)
        if (!s.mask_dir.empty()) with_masks++;
    if (with_masks > 0) {
        if (with_masks == (int)_sources.size())
            ui::TextColoredWrapped(kOk, dmsg::masks_found_all);
        else
            ui::TextColoredWrapped(kOk, dmsg::masks_found_some,
                                   {with_masks, (int)_sources.size()});
    }

    ui::Checkbox(dmsg::mask_enable, &_mask_enable);
    ui::help_on_hover(dmsg::mask_enable_help);

    // A sibling, not a child: the stencil is geometry, so it works with no
    // model downloaded and on a build with no segmentation in it at all.
    if (ui::Checkbox(dmsg::mask_border_enable, &_border_enable) && _border_enable) {
        // Ticking it has to do something on its own. The fisheye border is
        // what it is for, so an input with nothing drawn on it yet gets the
        // fit switched on; one that has been edited is left alone.
        for (PrepInput& in : _sources)
            if (in.stencil.empty()) in.stencil.detect_border = true;
    }
    ui::help_on_hover(dmsg::mask_border_enable_help);
    if (!_mask_enable && !_border_enable) return;

    ImGui::Indent();

    const ModelEntry* entry = find_model(_model_id);
    const bool builtin_masking = backends().builtin_masking;

    if (_mask_enable && builtin_masking) {
        // ---- model selection + download ----
        int model_idx = 0;
        const auto& catalog = model_catalog();
        for (size_t i = 0; i < catalog.size(); i++)
            if (_model_id == catalog[i].id) model_idx = (int)i;
        ImGui::SetNextItemWidth(px(260.0f));
        if (ui::BeginCombo(dmsg::mask_model, catalog[model_idx].label->get())) {
            for (size_t i = 0; i < catalog.size(); i++) {
                const bool cached = model_is_cached(catalog[i]);
                const std::string label =
                    cached ? std::string(catalog[i].label->get())
                           : i18n::format(dmsg::mask_model_needs_download,
                                          {catalog[i].label->get()});
                if (ui::SelectableRaw(label, (int)i == model_idx))
                    _model_id = catalog[i].id;
                if (ImGui::IsItemHovered()) ui::SetTooltip(*catalog[i].blurb);
            }
            ImGui::EndCombo();
        }
        entry = find_model(_model_id);
        if (entry) ui::TextDisabled(*entry->blurb);

        const bool downloading = _download.state() == ModelDownload::State::Running;
        if (entry && !model_is_cached(*entry) && !downloading) {
            if (ui::Button(dmsg::mask_get_model)) request_model_download();
            ImGui::SameLine();
            ui::TextDisabled(dmsg::mask_one_time_download);
        } else if (downloading) {
            // The overlay is a byte count from curl, not a sentence.
            ui::ProgressBarRaw(std::max(_download.progress(), 0.0f),
                               ImVec2(260, 0), _download.status().c_str());
            ImGui::SameLine();
            if (ui::Button(dmsg::stop)) _download.cancel();
        } else if (entry) {
            ui::TextColored(kOk, dmsg::mask_model_ready);
        }
        if (_download.state() == ModelDownload::State::Failed)
            ui::TextColoredWrappedRaw(kErr, _download.status());
        if (entry && !entry->text_prompts && _mask.clicks.empty())
            ui::TextColored(kWarn, dmsg::mask_no_text_prompts);
        if (!_mask.clicks.empty()) {
            int objects = 0;
            for (const MaskClick& c : _mask.clicks)
                objects = std::max(objects, c.object + 1);
            ui::Text(dmsg::mask_clicked_objects, {objects});
            ImGui::SameLine();
            if (ui::SmallButton(dmsg::mask_forget_clicks)) {
                _mask.clicks.clear();
                _mask.object_count = 1;
                _mask.current_object = 0;
            }
            ui::help_on_hover(dmsg::mask_forget_clicks_help);
            const std::string unprompted =
                inputs_without_clicks(_sources, _mask.clicks);
            if (!unprompted.empty() && _mask.prompt.empty())
                ui::TextColoredWrapped(kWarn, dmsg::mask_inputs_need_clicks,
                                       {unprompted});
        }
    } else if (_mask_enable) {
        ui::TextWrappedRaw(backends().masking_note);
        ui::help_on_hover_raw(backends().masking_reason.c_str());
    }

    // One button for both halves: it is the same panel on the same input, and
    // the stencil half of it needs no model, so it must not sit behind one.
    if (ui::Button(dmsg::mask_try)) open_mask_preview();
    ui::help_on_hover(dmsg::mask_try_help);
    // Which input it opens, and so which input a NEW click prompts; the ones
    // already made stay with their own (MaskClick::source).
    if (_sources.size() > 1) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(px(220.0f));
        std::vector<const char*> names;
        for (const PrepInput& s : _sources) names.push_back(s.subdir.c_str());
        int pick = _mask_preview_input;
        if (ui::ComboRaw(ui::detail::label(dmsg::mask_on_input), &pick,
                         names.data(), (int)names.size()) &&
            pick != _mask_preview_input) {
            _mask_preview_input = pick;
            // An open panel is showing the old input's frames and editing its
            // stencil; point both at the new one.
            if (_segment.is_open()) open_mask_preview();
        }
        ui::help_on_hover(dmsg::mask_on_input_help);
    }

    if (!_mask_enable) {
        ImGui::Unindent();
        return;
    }

    // Polarity above the two prompts, which are labelled by it -- the same
    // box means "take this out" or "this is the subject" depending on the
    // radio. Same order and same wording as the preview panel.
    int polarity = _mask.keep_subject ? 1 : 0;
    if (ui::RadioButton(dmsg::mask_remove_named, polarity == 0)) polarity = 0;
    ImGui::SameLine();
    if (ui::RadioButton(dmsg::mask_keep_named, polarity == 1)) polarity = 1;
    _mask.keep_subject = polarity == 1;
    ui::help_on_hover(dmsg::mask_polarity_help);
    const bool keep_subject = _mask.keep_subject;

    // English, whatever the interface language is -- see MaskPrompt.h. The
    // placeholder examples are English for the same reason.
    ImGui::SetNextItemWidth(px(320.0f));
    ui::InputTextEnglish(
        keep_subject ? dmsg::mask_what_to_keep : dmsg::mask_what_to_remove,
        keep_subject ? "the statue; its pedestal" : "person; car; shadow of a person",
        &_mask.prompt);
    ui::help_on_hover(keep_subject ? dmsg::mask_prompt_help_keep
                                   : dmsg::mask_prompt_help_remove);
    ImGui::SetNextItemWidth(px(320.0f));
    ui::InputTextEnglish(
        keep_subject ? dmsg::mask_but_remove : dmsg::mask_but_keep,
        keep_subject ? "the hand holding it" : "person in a painting",
        &_mask.negative_prompt);
    ui::help_on_hover(keep_subject ? dmsg::mask_negative_help_keep
                                   : dmsg::mask_negative_help_remove);

    // Only worth saying when the interface is not already English: for an
    // English user the box being English is not news.
    if (i18n::current() != i18n::Lang::en) {
        ImGui::PushTextWrapPos(px(560.0f));
        ui::TextDisabled(dmsg::mask_english_only);
        ImGui::PopTextWrapPos();
    }
    draw_subject_palette(_mask.prompt, _mask.negative_prompt, keep_subject);

    if (ui::CollapsingHeader(dmsg::mask_advanced)) {
        // The preview reads these three off the same fields, so a prompt tried
        // there is tried at the settings that will run.
        ImGui::SetNextItemWidth(px(220.0f));
        ui::SliderFloat(dmsg::mask_threshold, &_mask.threshold, 0.05f, 0.95f,
                        "%.2f");
        ui::help_on_hover(dmsg::mask_threshold_help);
        ImGui::SetNextItemWidth(px(220.0f));
        ui::SliderFloat(dmsg::mask_nms, &_mask.nms, 0.05f, 0.95f, "%.2f");
        ui::help_on_hover(dmsg::mask_nms_help);
        ImGui::SetNextItemWidth(px(220.0f));
        if (ui::InputInt(dmsg::mask_max_size, &_mask.max_image_size))
            _mask.max_image_size = std::max(0, _mask.max_image_size);
        ui::help_on_hover(dmsg::mask_max_size_help);

        // The rest is the memory bank, which photos never get.
        bool any_video = false;
        for (const PrepInput& s : _sources) any_video = any_video || s.is_video;
        if (any_video) {
            // A clicked object means nothing on any frame but its own without
            // the bank, so it forces the option on and the box shows what will
            // run.
            const bool forced = !_mask.clicks.empty();
            bool memory = _mask_memory || forced;
            ImGui::BeginDisabled(forced);
            if (ui::Checkbox(dmsg::mask_memory, &memory)) _mask_memory = memory;
            ImGui::EndDisabled();
            ui::help_on_hover_disabled(dmsg::mask_memory_help);

            ImGui::Indent();
            ImGui::BeginDisabled(!memory);
            ImGui::SetNextItemWidth(px(200.0f));
            if (ui::InputInt(dmsg::mask_detect_every, &_mask_detect_every))
                _mask_detect_every = std::clamp(_mask_detect_every, 1, 1000);
            ui::help_on_hover_disabled(dmsg::mask_detect_every_help);
            ImGui::SetNextItemWidth(px(200.0f));
            if (ui::InputInt(dmsg::mask_memory_frames, &_mask_memory_frames))
                _mask_memory_frames = std::clamp(_mask_memory_frames, 0, 7);
            ui::help_on_hover_disabled(dmsg::mask_memory_frames_help);
            ImGui::EndDisabled();
            ImGui::Unindent();
        }
    }

    ImGui::Unindent();
}

// ---------------------------------------------------------------------------
// Depth and normals
// ---------------------------------------------------------------------------

bool GuiApp::geometry_model_missing() const {
    if (!_geometry.enable) return false;
    if (!geometry_availability().empty()) return false;
    return !geometry_model_cached(_geometry.model);
}

void GuiApp::request_geometry_download() {
    _geom_download.start(geometry_model_downloads(_geometry.model));
}

void GuiApp::open_geometry_preview() {
    // One multi-gigabyte backbone at a time: the mask preview holds SAM and
    // this one holds Metric3D, and the inference layer's pool is process-wide.
    _segment.close();
    const PrepInput* in =
        _sources.empty() ? nullptr
                         : &_sources[(size_t)std::min((size_t)_mask_preview_input,
                                                      _sources.size() - 1)];
    _geometry_panel.open(in ? in->path : std::string(), in && in->is_video,
                         _workspace, planned_image_dir(_sources, _workspace),
                         in ? in->camera_model : std::string("opencv"),
                         in ? in->focal_factor : 0.0f, _ffmpeg_exe,
                         _sfm_job.prep.force_external_decode);
}

void GuiApp::draw_geometry_options() {
    const std::string why = geometry_availability();
    if (!why.empty()) {
        // Nothing here can work in this build, so offer no switch at all --
        // one that fails when the run reaches it is worse than its absence.
        ui::TextDisabledWrapped(dmsg::geom_unavailable);
        ui::help_on_hover_raw(why.c_str());
        return;
    }

    ui::Checkbox(dmsg::geom_enable, &_geometry.enable);
    ui::help_on_hover(dmsg::geom_enable_help);
    if (!_geometry.enable) return;

    ImGui::Indent();

    // ---- the checkpoint ----
    const auto& catalog = geometry_models();
    int model_idx = 1;
    for (size_t i = 0; i < catalog.size(); i++)
        if (_geometry.model == catalog[i].id) model_idx = (int)i;
    ImGui::SetNextItemWidth(px(260.0f));
    if (ui::BeginCombo(dmsg::geom_model, catalog[(size_t)model_idx].label->get())) {
        for (size_t i = 0; i < catalog.size(); i++) {
            const bool cached = geometry_model_cached(catalog[i].id);
            const std::string label =
                cached ? std::string(catalog[i].label->get())
                       : i18n::format(dmsg::mask_model_needs_download,
                                      {catalog[i].label->get()});
            if (ui::SelectableRaw(label, (int)i == model_idx))
                _geometry.model = catalog[i].id;
            if (ImGui::IsItemHovered()) ui::SetTooltip(*catalog[i].blurb);
        }
        ImGui::EndCombo();
    }
    ui::TextDisabled(*catalog[(size_t)model_idx].blurb);

    FileDownload& geom_dl = _geom_download.current();
    if (_geom_download.running()) {
        // The overlay is a byte count from curl, not a sentence.
        ui::ProgressBarRaw(std::max(geom_dl.progress(), 0.0f),
                           ImVec2(px(260.0f), 0), geom_dl.status().c_str());
        ImGui::SameLine();
        // The mask download's Stop button carries the same message; two of
        // them can be on screen at once, so this one needs its own ID.
        ImGui::PushID("geomdl");
        if (ui::Button(dmsg::stop)) _geom_download.cancel();
        ImGui::PopID();
    } else if (geometry_model_missing()) {
        if (ui::Button(dmsg::geom_get_model)) request_geometry_download();
        ImGui::SameLine();
        ui::TextDisabledRaw(human_bytes(catalog[(size_t)model_idx].bytes));
        if (geom_dl.state() == FileDownload::State::Failed)
            ui::TextColoredWrappedRaw(kErr, geom_dl.status());
    } else {
        ui::TextColored(kOk, dmsg::geom_model_ready);
    }

    // ---- what it writes ----
    ui::Checkbox(dmsg::geom_write_normals, &_geometry.want_normal);
    ImGui::SameLine();
    ui::Checkbox(dmsg::geom_write_depth, &_geometry.want_depth);
    ui::help_on_hover(gmsg::opt_depth);
    if (!_geometry.want_normal && !_geometry.want_depth)
        ui::TextColoredWrapped(kWarn, dmsg::geom_nothing_to_write);

    // Behind the checkpoint, unlike "Try the mask": there is no half of this
    // panel that works without one, and opening it would have the panel fetch
    // the weights itself.
    ImGui::BeginDisabled(geometry_model_missing());
    if (ui::Button(dmsg::geom_try)) open_geometry_preview();
    ImGui::EndDisabled();
    ui::help_on_hover_disabled(geometry_model_missing() ? dmsg::geom_model_first
                                                        : dmsg::geom_try_help);

    if (ui::CollapsingHeader(dmsg::geom_advanced)) {
        ImGui::SetNextItemWidth(px(220.0f));
        if (ui::InputInt(dmsg::geom_max_size, &_geometry.max_size))
            _geometry.max_size = std::clamp(_geometry.max_size, 224, 4096);
        ui::help_on_hover(gmsg::opt_max_size);

        // Enabled whatever the checkpoint is: a Metric3D run passes it and
        // ignores it, and disabling it would need this screen to know which
        // family an .onnx the user pointed at belongs to.
        ImGui::SetNextItemWidth(px(220.0f));
        if (ui::InputInt(dmsg::geom_num_tokens, &_geometry.num_tokens))
            _geometry.num_tokens = std::clamp(_geometry.num_tokens, 256, 8192);
        ui::help_on_hover(gmsg::opt_num_tokens);

        // png / jpg / relative / mm are what config.json and the flag spell,
        // so the values stay as they are and the label carries the meaning.
        ImGui::SetNextItemWidth(px(220.0f));
        int fmt = _geometry.normal_jpg ? 1 : 0;
        static const char* kFormats[] = {"png", "jpg"};
        if (ui::ComboRaw(ui::detail::label(dmsg::geom_normal_format), &fmt,
                         kFormats, 2))
            _geometry.normal_jpg = fmt == 1;
        ui::help_on_hover(gmsg::opt_normal_format);
        if (_geometry.normal_jpg) {
            ImGui::SetNextItemWidth(px(220.0f));
            if (ui::InputInt(dmsg::geom_jpeg_quality, &_geometry.jpeg_quality))
                _geometry.jpeg_quality = std::clamp(_geometry.jpeg_quality, 1, 99);
            ui::help_on_hover(gmsg::opt_jpeg_quality);
        }

        ImGui::BeginDisabled(!_geometry.want_depth);
        ImGui::SetNextItemWidth(px(220.0f));
        int units = _geometry.depth_mm ? 1 : 0;
        static const char* kUnits[] = {"relative", "mm"};
        if (ui::ComboRaw(ui::detail::label(dmsg::geom_depth_units), &units,
                         kUnits, 2))
            _geometry.depth_mm = units == 1;
        ui::help_on_hover_disabled(gmsg::opt_depth_units);
        ImGui::EndDisabled();

        static const char* kTri[] = {"auto", "yes", "no"};
        ImGui::SetNextItemWidth(px(220.0f));
        ui::ComboRaw(ui::detail::label(dmsg::geom_split), &_geometry.split, kTri, 3);
        ui::help_on_hover(gmsg::opt_split);
        ImGui::SetNextItemWidth(px(220.0f));
        ui::ComboRaw(ui::detail::label(dmsg::geom_ray_depth), &_geometry.ray_depth,
                     kTri, 3);
        ui::help_on_hover(gmsg::opt_ray_depth);

        ui::Checkbox(dmsg::geom_overwrite, &_geometry.overwrite);
        ui::help_on_hover(gmsg::opt_overwrite);
    }

    ImGui::Unindent();
}

// ---------------------------------------------------------------------------
// What the run is doing
// ---------------------------------------------------------------------------

namespace {

const Msg& step_name(Stage s) {
    switch (s) {
        case Stage::Frames:    return dmsg::step_frames;
        case Stage::Masks:     return dmsg::step_masks;
        case Stage::Features:  return dmsg::step_features;
        case Stage::Matching:  return dmsg::step_matching;
        case Stage::Mapping:   return dmsg::step_mapping;
        case Stage::Geometry:  return dmsg::step_geometry;
        case Stage::Finishing: return dmsg::step_finishing;
    }
    return dmsg::step_frames;
}

ImVec4 step_color(StageStatus st) {
    switch (st) {
        case StageStatus::Running: return kOk;
        case StageStatus::Failed:  return kErr;
        case StageStatus::Done:    return ImGui::GetStyle().Colors[ImGuiCol_Text];
        default:                   return kDim;
    }
}

}  // namespace

// The six steps as a row, the running one with its own bar under it. This is
// what the log used to be the only view of.
void GuiApp::draw_dataset_steps() {
    RunProgress* prog = dataset_steps();
    Stage running = Stage::Frames;
    bool any = false;
    for (int i = 0; i < kNumStages; i++) {
        const Stage s = (Stage)i;
        const StageProgress p = prog->stage(s);
        if (i) {
            ImGui::SameLine(0.0f, px(6.0f));
            ui::TextDisabledRaw(">");
            ImGui::SameLine(0.0f, px(6.0f));
        }
        ui::TextColored(step_color(p.status), step_name(s));
        if (p.status == StageStatus::Running) {
            running = s;
            any = true;
        }
    }

    if (!any) return;
    const StageProgress p = prog->stage(running);
    // A step that can say how far through it is gets a bar; one that cannot
    // (the mapper counts registrations, not a total) gets its own sentence.
    if (p.fraction >= 0.0f)
        ui::ProgressBarRaw(p.fraction, ImVec2(-1, 0),
                           p.detail.empty() ? nullptr : p.detail.c_str());
    else if (!p.detail.empty())
        ui::TextDisabledRaw(p.detail);
}

// Which of the three previews the running step implies. Frames, masks and
// feature extraction all show pictures, so they share one.
int GuiApp::preview_for_stage() {
    // A finished run keeps showing what its last step was on: the model is
    // what somebody was watching when it ended, and falling back to the frames
    // would hide it behind a click nobody knows to make.
    if (!dataset_busy()) return _preview_last_stage;
    switch (dataset_steps()->current()) {
        case Stage::Frames:    return 0;
        case Stage::Masks:     return 1;
        case Stage::Features:  return 2;
        case Stage::Matching:  return 3;
        case Stage::Geometry:  return 5;
        case Stage::Mapping:
        case Stage::Finishing: return 4;
    }
    return -1;
}

// What the child has written about itself since the last look. Two stats and,
// when something changed, a file of about a megabyte -- so it runs at 2 Hz
// rather than every frame.
void GuiApp::poll_sfm_progress() {
    if (effective_engine() != Engine::BuiltIn) return;
    const double now = ImGui::GetTime();
    if (_sfm_polled_at > 0.0 && now - _sfm_polled_at < 0.5) return;
    _sfm_polled_at = now;

    // The frames of the extraction step are shown by whoever writes them; the
    // features are read off disk, which is what this watcher is for.
    if (dataset_busy() && dataset_steps()->current() == Stage::Features)
        _features.start(_sfm.sfm_image_dir(), _sfm.sfm_mask_dir(),
                        _sfm.features_dir(), &_film_features);
    _pairs_view.configure(_sfm.sfm_image_dir(), _sfm.sfm_mask_dir(),
                          _sfm.features_dir(), _sfm.matches_path());

    const std::string dir = _sfm.progress_dir();
    if (dir.empty()) return;

    PairMatrix pm;
    if (read_pair_matrix(dir, _pairs_mtime, pm)) _matrix.set(pm);
    // matches.bin is the whole truth and outlives the live file, which the run
    // deletes with the rest of the intermediates.
    if (!dataset_busy() &&
        read_pair_matrix_from_matches(_sfm.matches_path(), _matches_mtime, pm))
        _matrix.set(pm);

    LiveModel lm;
    if (read_live_model(dir, _model_mtime, lm)) {
        _live_model = std::move(lm);
        // The mapper's own output is a wall of per-registration detail, so the
        // default log used to go quiet for the whole of the longest step. The
        // snapshot has the exact counts and needs no line parsed out of a
        // translated sentence, so say it here instead -- and give the step a
        // real bar rather than a spinner.
        if (dataset_busy() && _live_model.n_images) {
            RunProgress& p = _sfm.steps();
            p.count(Stage::Mapping, _live_model.n_registered, _live_model.n_images);
            p.note(Stage::Mapping,
                   i18n::format(dmsg::model_live_counts,
                                {(long long)_live_model.n_registered,
                                 (long long)_live_model.n_images,
                                 (long long)_live_model.n_points}),
                   /*detail=*/false);
        }
        // Same key every time: the pose the user navigated to belongs to the
        // scene, not to the snapshot, and re-framing on every one of them
        // would make the view unusable while it is most worth watching.
        _model_view.attach_preview_data(_live_model.ds, _live_model.post,
                                        "sfm-live", /*radius=*/1.0f,
                                        /*with_cameras=*/true);
        _model_attached = true;
    }
}

// Everything the dataset screen holds about a run, given up: the previews, and
// the files behind them. The run leaves its features and its matches on disk
// precisely so that this screen can go on reading them after it ends, so this
// -- the screen being done with them -- is where they go.
void GuiApp::reset_dataset_preview() {
    _sfm.sweep_intermediates();
    _features.stop();
    _model_view.detach();
    _model_view.destroy_gl();
    _model_attached = false;
    _live_model = LiveModel{};
    _matrix.clear();
    _matrix.destroy_gl();
    _pairs_view.clear();
    _pairs_view.destroy_gl();
    for (FilmReel* f : {&_film_frames, &_film_masks, &_film_features,
                        &_film_geometry}) {
        f->clear();
        f->destroy_gl();
    }
    _model_mtime = _pairs_mtime = _matches_mtime = 0;
    _preview_tab = -1;
    _preview_last_stage = -1;
}

bool GuiApp::preview_has_content() const {
    return _film_frames.has_frames() || _film_masks.has_frames() ||
           _film_features.has_frames() || !_matrix.empty() || _model_attached ||
           _film_geometry.has_frames();
}

// The frames / match map / model area. Which one it shows follows the running
// step until the user picks one, because the step that is running is the one
// worth looking at and nobody should have to keep up with it by hand.
//
// `height` of 0 asks for the splitter, which is what the one-column layout
// needs; a column of its own hands its own height down instead.
void GuiApp::draw_dataset_preview(float height) {
    // Null where the view is not a reel: the match map and the model.
    FilmReel* reels[6] = {&_film_frames, &_film_masks, &_film_features,
                          nullptr, nullptr, &_film_geometry};
    const bool avail[6] = {_film_frames.has_frames(), _film_masks.has_frames(),
                           _film_features.has_frames(), !_matrix.empty(),
                           _model_attached, _film_geometry.has_frames()};
    bool any_avail = false;
    for (bool v : avail) any_avail = any_avail || v;
    if (!any_avail) return;

    if (ui::Checkbox(dmsg::show_run_preview, &_show_preview)) save_settings();
    ui::help_on_hover(dmsg::show_run_preview_help);
    if (!_show_preview) return;

    const int implied = preview_for_stage();
    if (dataset_busy() && implied >= 0) _preview_last_stage = implied;
    int tab = _preview_tab >= 0 ? _preview_tab : (implied >= 0 ? implied : 0);
    if (!avail[tab]) {
        for (int i = 0; i < 6; i++)
            if (avail[i]) { tab = i; break; }
    }

    const Msg* names[6] = {&dmsg::view_frames, &dmsg::view_masks,
                           &dmsg::view_features, &dmsg::view_matrix,
                           &dmsg::view_model, &dmsg::view_geometry};
    bool first = true;
    for (int i = 0; i < 6; i++) {
        if (!avail[i]) continue;
        if (!first) ImGui::SameLine();
        first = false;
        if (ui::RadioButton(*names[i], tab == i)) {
            tab = i;
            // Picking one pins it: following the run is the default, not the
            // rule, and a user who opened the match map to look at a seam
            // should not lose it the moment mapping starts.
            _preview_tab = i;
        }
    }
    if (tab == 3) ui::help_on_hover(dmsg::matrix_help);

    // In a column of its own the height is the column's; otherwise a splitter,
    // as the log has -- what any of these views is worth depends entirely on
    // which one the user is looking at, so it is theirs to set.
    float h = height;
    if (h <= 0.0f) {
        h = px(_preview_h);
        const float line = ImGui::GetTextLineHeightWithSpacing();
        float want = h;
        if (splitter_h("##previewsplit", &want, 2.0f * line, px(600.0f),
                       ImGui::GetContentRegionAvail().x)) {
            _preview_h = want / ui_scale();
            _layout_dirty = true;
            h = want;
        }
    } else {
        h = std::max(px(60.0f), h - ImGui::GetCursorPosY() +
                                    ImGui::GetCursorStartPos().y);
    }

    if (reels[tab]) {
        reels[tab]->draw(h - px(8.0f));
        return;
    }
    if (tab == 3) {
        ImGui::BeginChild("##matrix", ImVec2(0, h));
        const float avail = ImGui::GetContentRegionAvail().x;
        // The map keeps its square and the pair it points at gets the rest of
        // the row, down to nothing on a column too narrow to hold both. The
        // legend goes under the map, inside the same height.
        const float side =
            std::min(h - px(8.0f) - ImGui::GetTextLineHeightWithSpacing(),
                     avail > px(520.0f) ? avail * 0.5f : avail);
        uint32_t a = 0, b = 0;
        // Grouped so the pair view sits beside the map rather than beside the
        // legend under it.
        ImGui::BeginGroup();
        if (_matrix.draw(side, a, b)) _pairs_view.show(a, b);
        ImGui::EndGroup();
        const float rest = avail - side - ImGui::GetStyle().ItemSpacing.x;
        if (rest > px(160.0f)) {
            ImGui::SameLine();
            ImGui::BeginChild("##pairview", ImVec2(rest, h - px(8.0f)));
            _pairs_view.draw(ImGui::GetContentRegionAvail());
            ImGui::EndChild();
        }
        ImGui::EndChild();
        return;
    }
    ui::Text(dmsg::model_live_counts,
             {(long long)_live_model.n_registered, (long long)_live_model.n_images,
              (long long)_live_model.n_points});
    if (_live_model.empty()) {
        ui::TextDisabledWrapped(dmsg::model_waiting);
        return;
    }
    // Width capped against the height: the band is as wide as the window, and
    // a 1600x150 letterbox of a 90-degree view shows a slice of the scene with
    // everything in it apparently enormous.
    const float w = std::min(ImGui::GetContentRegionAvail().x, h * 16.0f / 9.0f);
    ImGui::BeginChild("##livemodel", ImVec2(w, h));
    _model_view.draw(/*training=*/false);
    ImGui::EndChild();
}

// Re-doing one step of what is already in the output folder, instead of the
// whole run. The rules are probe_workspace's; this only names them.
void GuiApp::draw_dataset_rerun(const WorkspaceState& prior) {
    if (!prior.resumable() && !prior.model && !prior.geometry) return;
    if (!ui::CollapsingHeader(dmsg::rerun_section)) return;
    ImGui::Indent();
    ui::TextDisabledWrapped(dmsg::rerun_section_help);

    // Each button starts a run of its own, so each is behind exactly the
    // checkpoints its steps read -- a missing geometry model must not stop a
    // rerun of the masks.
    const bool need_mask_model = mask_model_missing();
    const bool need_feat_model = feature_model_missing();

    bool go = false;
    if (prior.frames) {
        ImGui::BeginDisabled(need_mask_model || need_feat_model);
        if (ui::Button(dmsg::rerun_frames)) {
            _redo_frames = _redo_masks = true;   // the masks describe the frames
            _redo_model = go = true;
        }
        ImGui::EndDisabled();
        if (need_mask_model || need_feat_model)
            ui::help_on_hover_disabled(need_mask_model ? dmsg::mask_model_first
                                                       : dmsg::feat_model_first);
        ImGui::SameLine();
    }
    if (prior.masks) {
        ImGui::BeginDisabled(need_mask_model);
        if (ui::Button(dmsg::rerun_masks)) {
            _redo_masks = true;
            _redo_model = go = true;
        }
        ImGui::EndDisabled();
        if (need_mask_model) ui::help_on_hover_disabled(dmsg::mask_model_first);
        ImGui::SameLine();
    }
    ImGui::BeginDisabled(need_feat_model);
    if (ui::Button(dmsg::rerun_model)) {
        _redo_model = go = true;
    }
    ImGui::EndDisabled();
    if (need_feat_model) ui::help_on_hover_disabled(dmsg::feat_model_first);
    // Depth and normals are the one step that reruns on its own: they are read
    // off the finished dataset and nothing downstream of them exists.
    if (prior.geometry) {
        ImGui::SameLine();
        const bool need_geom_model = geometry_model_missing();
        ImGui::BeginDisabled(!_geometry.enable || need_geom_model);
        if (ui::Button(dmsg::rerun_geometry)) {
            _redo_geometry = go = true;
        }
        ImGui::EndDisabled();
        ui::help_on_hover_disabled(need_geom_model ? dmsg::geom_model_first
                                                   : dmsg::rerun_geometry_help);
    }
    ImGui::NewLine();
    ImGui::Unindent();
    // Every route but the last re-reconstructs: a model built from frames or
    // masks that have just been replaced describes neither.
    if (go) start_dataset_job();
}

// ---------------------------------------------------------------------------
// Advanced: the photographs' colour space
// ---------------------------------------------------------------------------

void GuiApp::draw_color_space_options(bool with_point_color) {
    ui::SeparatorText(dmsg::section_color_space);

    // Item 0 of both pickers means "leave it to the file", which is what the
    // child processes do when the flag is absent; anything else is stated on
    // their command line, so an EXR header that lies can be overruled.
    ImGui::SetNextItemWidth(px(260.0f));
    int gamut = 0;
    for (int i = 0; i < (int)std::size(colorspace::kGamuts); i++)
        if (_sfm_job.image_gamut == colorspace::kGamuts[i]) gamut = i + 1;
    if (ui::Combo(dmsg::input_gamut, &gamut,
                  {&dmsg::space_from_file, &dmsg::gamut_rec709,
                   &dmsg::gamut_aces2065_1, &dmsg::gamut_acescg,
                   &dmsg::gamut_rec2020, &dmsg::gamut_adobergb,
                   &dmsg::gamut_dcip3})) {
        _sfm_job.image_gamut = gamut == 0 ? "" : colorspace::kGamuts[gamut - 1];
        _color_space_touched = true;
    }
    ui::help_on_hover(dmsg::input_gamut_help);

    ImGui::SetNextItemWidth(px(260.0f));
    int transfer = !_sfm_job.image_is_linear.has_value() ? 0
                                                        : (*_sfm_job.image_is_linear ? 1 : 2);
    if (ui::Combo(dmsg::input_is_linear, &transfer,
                  {&dmsg::space_from_file, &dmsg::transfer_linear,
                   &dmsg::transfer_display})) {
        _sfm_job.image_is_linear =
            transfer == 0 ? std::optional<bool>{} : std::optional<bool>(transfer == 1);
        _color_space_touched = true;
    }
    ui::help_on_hover(dmsg::input_is_linear_help);

    // COLMAP writes its own point cloud, so the choice is the built-in SfM's.
    if (!with_point_color) return;
    ImGui::BeginDisabled(colorspace::is_identity(
        _sfm_job.image_gamut, _sfm_job.image_is_linear.value_or(false)));
    ui::Checkbox(dmsg::point_color_image_space,
                 &_sfm_job.point_color_in_image_space);
    ui::help_on_hover(dmsg::point_color_image_space_help);
    ImGui::EndDisabled();
}

// ---------------------------------------------------------------------------
// Advanced: built-in SfM
// ---------------------------------------------------------------------------

// Would the reconstruction reach the learned frontend and find no checkpoint?
// The same question mask_model_missing() asks, of the other download.
bool GuiApp::feature_model_missing() const {
    if (effective_engine() != Engine::BuiltIn) return false;
    return !sfm_features_cached(_sfm_job.features, _sfm_job.matcher);
}

void GuiApp::request_feature_download() {
    _feat_download.start(
        sfm_feature_downloads(_sfm_job.features, _sfm_job.matcher));
}

// The detector and, with LightGlue, the matcher: what they cost and a button
// that gets them. Under the two combos that chose them.
void GuiApp::draw_feature_download() {
    FileDownload& dl = _feat_download.current();
    if (_feat_download.running()) {
        ui::ProgressBarRaw(std::max(dl.progress(), 0.0f), ImVec2(px(260.0f), 0),
                           dl.status().c_str());
        ImGui::SameLine();
        ImGui::PushID("featdl");
        if (ui::Button(dmsg::stop)) _feat_download.cancel();
        ImGui::PopID();
        return;
    }
    if (!feature_model_missing()) {
        ui::TextColored(kOk, dmsg::feat_model_ready);
        return;
    }
    uint64_t bytes = 0;
    for (const PendingDownload& d :
         sfm_feature_downloads(_sfm_job.features, _sfm_job.matcher))
        bytes += d.bytes;
    if (ui::Button(dmsg::feat_get_model)) request_feature_download();
    ImGui::SameLine();
    ui::TextDisabledRaw(human_bytes(bytes));
    if (dl.state() == FileDownload::State::Failed)
        ui::TextColoredWrappedRaw(kErr, dl.status());
}

void GuiApp::draw_sfm_advanced() {
    if (!ui::CollapsingHeader(dmsg::section_advanced)) return;

    ImGui::SetNextItemWidth(px(260.0f));
    ui::Combo(dmsg::capture_type, &_sfm_job.data_type,
              {&dmsg::capture_photos, &dmsg::capture_video,
               &dmsg::capture_internet});
    ui::help_on_hover(dmsg::capture_type_help);

    ImGui::SetNextItemWidth(px(260.0f));
    ui::Combo(dmsg::features, &_sfm_job.features,
              {&dmsg::features_sift, &dmsg::features_aliked_n16,
               &dmsg::features_aliked_n32});
    ui::help_on_hover(dmsg::features_help);

    {
        // Brute force is the only option for SIFT, so say so by disabling the
        // combo rather than by letting the run fail.
        const bool learned = _sfm_job.features != 0;
        ImGui::BeginDisabled(!learned);
        ImGui::SetNextItemWidth(px(260.0f));
        int shown = learned ? _sfm_job.matcher : 0;
        if (ui::Combo(dmsg::matcher, &shown,
                      {&dmsg::matcher_brute_force, &dmsg::matcher_lightglue}) &&
            learned)
            _sfm_job.matcher = shown;
        ImGui::EndDisabled();
        ui::help_on_hover(learned ? dmsg::matcher_help
                                  : dmsg::matcher_needs_learned);
        if (learned) draw_feature_download();
    }

    ImGui::SetNextItemWidth(px(260.0f));
    ui::Combo(dmsg::mapper_schedule, &_sfm_job.mapper,
              {&dmsg::mapper_flat, &dmsg::mapper_bottom_up});
    ui::help_on_hover(dmsg::mapper_schedule_help);

    if (_sfm_job.pairs == 2) {
        ImGui::SetNextItemWidth(px(260.0f));
        ui::InputInt(dmsg::sequential_overlap, &_sfm_job.overlap);
        ui::help_on_hover(dmsg::sequential_overlap_help);
    }
    // "Automatic" resolves to sequential for a short video, so offer this
    // whenever sequential can be what runs, not only when it was named.
    if (_sfm_job.pairs == 2 || (_sfm_job.pairs == 0 && _sfm_job.data_type == 1)) {
        ui::Checkbox(dmsg::loop_closure, &_sfm_job.loop_closure);
        ui::help_on_hover(dmsg::loop_closure_help_builtin);
    }

    ImGui::SetNextItemWidth(px(260.0f));
    ui::InputFloat(dmsg::initial_focal_px, &_sfm_job.init_focal_px, 0, 0, "%.4g");
    ui::help_on_hover(dmsg::initial_focal_px_help);

    ImGui::SetNextItemWidth(px(260.0f));
    ui::InputTextWithHint(dmsg::initial_distortion, dmsg::initial_distortion_hint,
                          &_sfm_job.init_distortion);
    ui::help_on_hover(dmsg::initial_distortion_help);

    ImGui::SetNextItemWidth(px(260.0f));
    ui::Combo(dmsg::sfm_distortion_refinement, &_sfm_job.distortion_refine,
              {&dmsg::sfm_distortion_during, &dmsg::sfm_distortion_final,
               &dmsg::sfm_distortion_never});
    ui::help_on_hover(dmsg::sfm_distortion_refinement_help);

    // Per-image cameras already give every image its own intrinsics, so the
    // pass would have nothing to split.
    ImGui::BeginDisabled(_sfm_job.camera_mode == 2);
    ui::Checkbox(dmsg::sfm_per_image_intrinsics,
                 &_sfm_job.final_per_image_intrinsics);
    ImGui::EndDisabled();
    ui::help_on_hover(dmsg::sfm_per_image_intrinsics_help);

    ImGui::SetNextItemWidth(px(260.0f));
    ui::InputInt(dmsg::max_features_auto, &_sfm_job.max_features);
    ui::help_on_hover(dmsg::max_features_auto_help);
    ImGui::SetNextItemWidth(px(260.0f));
    ui::InputInt(dmsg::max_image_size_auto, &_sfm_job.max_image_size);
    ui::help_on_hover(dmsg::max_image_size_auto_help);

    ui::Checkbox(dmsg::keep_intermediate, &_sfm_job.keep_intermediate);
    ui::help_on_hover(dmsg::keep_intermediate_help);

    ImGui::SetNextItemWidth(-1);
    ui::InputTextWithHintRaw("##sfmextra", dmsg::extra_sfm_flags_hint,
                             &_sfm_job.extra_args);
    ui::help_on_hover(dmsg::extra_sfm_flags_help);

    draw_color_space_options(/*with_point_color=*/true);

    ui::SeparatorText(dmsg::section_fallbacks);
    ImGui::BeginDisabled(!backends().builtin_video);
    ui::Checkbox(dmsg::use_ffmpeg, &_sfm_job.prep.force_external_decode);
    ImGui::EndDisabled();
    ui::help_on_hover(backends().builtin_video ? dmsg::use_ffmpeg_help
                                               : dmsg::use_ffmpeg_always);
    ImGui::BeginDisabled(!backends().builtin_masking);
    ui::Checkbox(dmsg::use_python_masking, &_sfm_job.prep.force_external_masking);
    ImGui::EndDisabled();
    ui::help_on_hover(dmsg::use_python_masking_help);

    ui::Checkbox(dmsg::sfm_ba_cpu, &_sfm_job.ba_cpu);
    ui::help_on_hover(dmsg::sfm_ba_cpu_help);
}

// ---------------------------------------------------------------------------
// Advanced: external COLMAP
// ---------------------------------------------------------------------------

void GuiApp::draw_colmap_options() {
    if (ui::CollapsingHeader(dmsg::section_advanced)) {
        bool fisheye = _colmap_job.camera_model.find("FISHEYE") != std::string::npos;
        ImGui::SetNextItemWidth(px(180.0f));
        ui::InputFloat(dmsg::colmap_initial_focal,
                       &_colmap_job.init_focal_factor, 0, 0, "%.4g");
        ui::help_on_hover(dmsg::colmap_initial_focal_help);
        ImGui::SetNextItemWidth(px(280.0f));
        ui::InputTextWithHint(dmsg::colmap_camera_params,
                              dmsg::colmap_camera_params_hint,
                              &_colmap_job.camera_params);
        ui::help_on_hover(dmsg::colmap_camera_params_help);
        ImGui::SetNextItemWidth(px(180.0f));
        ui::InputInt(dmsg::colmap_max_features, &_colmap_job.max_num_features);
        ui::help_on_hover(dmsg::colmap_max_features_help);
        ImGui::SetNextItemWidth(px(180.0f));
        ui::InputInt(dmsg::colmap_max_image_size, &_colmap_job.max_image_size);
        ui::help_on_hover(dmsg::colmap_max_image_size_help);
        ImGui::SetNextItemWidth(px(180.0f));
        ui::InputInt(dmsg::sequential_overlap, &_colmap_job.seq_overlap);
        ui::help_on_hover(dmsg::colmap_seq_overlap_help);
        ui::Checkbox(dmsg::colmap_quadratic_overlap,
                     &_colmap_job.seq_quadratic_overlap);
        ui::help_on_hover(dmsg::colmap_quadratic_overlap_help);
        ui::Checkbox(dmsg::colmap_lightglue, &_colmap_job.lightglue);
        ui::help_on_hover(dmsg::colmap_lightglue_help);
        if (_colmap_job.feature_type == 0) {
            ui::Checkbox(dmsg::colmap_affine_sift,
                         &_colmap_job.estimate_affine_shape);
            ui::help_on_hover(dmsg::colmap_affine_sift_help);
        }
        ImGui::SetNextItemWidth(px(180.0f));
        ui::Combo(dmsg::colmap_distortion_refinement,
                  &_colmap_job.mapper_extra_params,
                  {&dmsg::colmap_extra_auto, &dmsg::colmap_extra_during,
                   &dmsg::colmap_extra_final});
        ui::help_on_hover(dmsg::colmap_distortion_refinement_help);
        ImGui::SetNextItemWidth(px(180.0f));
        ui::InputInt(dmsg::colmap_min_matches, &_colmap_job.min_num_matches);
        ui::help_on_hover(dmsg::colmap_min_matches_help);

        ui::SeparatorText(dmsg::colmap_repetitive);
        ui::help_on_hover(dmsg::colmap_repetitive_help);
        // Preset levels filling the five fields below (editing any field
        // afterwards shows "Custom"). Stricter = fewer wrong welds but
        // fewer registered images on genuinely weak overlap.
        struct RepLevel {
            const Msg* name;
            float ratio; int pair_in; int reg_in; float reg_ratio; float err;
        };
        static const RepLevel kRepLevels[] = {
            {&dmsg::colmap_rep_off,    0.0f,    0,   0, 0.0f,  0.0f},
            {&dmsg::colmap_rep_low,    0.75f,  30,  40, 0.30f, 10.0f},
            {&dmsg::colmap_rep_medium, 0.70f,  60,  60, 0.40f,  8.0f},
            {&dmsg::colmap_rep_high,   0.62f, 100, 100, 0.50f,  6.0f},
        };
        int rep_idx = -1;
        for (int i = 0; i < 4; i++)
            if (_colmap_job.match_max_ratio == kRepLevels[i].ratio &&
                _colmap_job.min_inliers_per_pair == kRepLevels[i].pair_in &&
                _colmap_job.abs_pose_min_num_inliers == kRepLevels[i].reg_in &&
                _colmap_job.abs_pose_min_inlier_ratio == kRepLevels[i].reg_ratio &&
                _colmap_job.abs_pose_max_error == kRepLevels[i].err) {
                rep_idx = i;
                break;
            }
        ImGui::SetNextItemWidth(px(180.0f));
        if (ui::BeginCombo(dmsg::colmap_repetitive_level,
                           rep_idx < 0 ? dmsg::colmap_rep_custom.get()
                                       : kRepLevels[rep_idx].name->get())) {
            for (int i = 0; i < 4; i++)
                if (ui::Selectable(*kRepLevels[i].name, rep_idx == i)) {
                    _colmap_job.match_max_ratio = kRepLevels[i].ratio;
                    _colmap_job.min_inliers_per_pair = kRepLevels[i].pair_in;
                    _colmap_job.abs_pose_min_num_inliers = kRepLevels[i].reg_in;
                    _colmap_job.abs_pose_min_inlier_ratio = kRepLevels[i].reg_ratio;
                    _colmap_job.abs_pose_max_error = kRepLevels[i].err;
                }
            ImGui::EndCombo();
        }
        ui::help_on_hover(dmsg::colmap_repetitive_level_help);
        ImGui::SetNextItemWidth(px(180.0f));
        ui::InputFloat(dmsg::colmap_match_ratio, &_colmap_job.match_max_ratio,
                       0, 0, "%.3g");
        ui::help_on_hover(dmsg::colmap_match_ratio_help);
        ImGui::SetNextItemWidth(px(180.0f));
        ui::InputInt(dmsg::colmap_min_inliers_pair,
                     &_colmap_job.min_inliers_per_pair);
        ui::help_on_hover(dmsg::colmap_min_inliers_pair_help);
        ImGui::SetNextItemWidth(px(180.0f));
        ui::InputInt(dmsg::colmap_min_inliers_reg,
                     &_colmap_job.abs_pose_min_num_inliers);
        ui::help_on_hover(dmsg::colmap_min_inliers_reg_help);
        ImGui::SetNextItemWidth(px(180.0f));
        ui::InputFloat(dmsg::colmap_min_inlier_ratio,
                       &_colmap_job.abs_pose_min_inlier_ratio, 0, 0, "%.3g");
        ui::help_on_hover(dmsg::colmap_min_inlier_ratio_help);
        ImGui::SetNextItemWidth(px(180.0f));
        ui::InputFloat(dmsg::colmap_max_reg_error,
                       &_colmap_job.abs_pose_max_error, 0, 0, "%.3g");
        ui::help_on_hover(dmsg::colmap_max_reg_error_help);
        ImGui::Separator();
        if (fisheye) ImGui::BeginDisabled();
        ui::Checkbox(dmsg::colmap_gpu_ba, &_colmap_job.ba_use_gpu);
        if (fisheye) ImGui::EndDisabled();
        ui::help_on_hover(fisheye ? dmsg::colmap_gpu_ba_fisheye
                                  : dmsg::colmap_gpu_ba_help);
        ui::Checkbox(dmsg::colmap_merge_models, &_colmap_job.merge_models);
        ui::help_on_hover(dmsg::colmap_merge_models_help);
        ui::Checkbox(dmsg::colmap_final_ba, &_colmap_job.final_bundle_adjust);
        ui::help_on_hover(dmsg::colmap_final_ba_help);
        ImGui::SetNextItemWidth(px(-160.0f));
        ui::InputTextWithHintRaw("##vocab", dmsg::colmap_vocab_tree_hint,
                                 &_colmap_job.vocab_tree_path);
        ImGui::SameLine();
        ImGui::PushID("vt");
        if (ui::Button(dmsg::browse)) {
            _pick = PickAction::VocabTree;
            _dialog.open(msg::pick_vocab_tree.get(), FileDialog::Mode::File,
                         {".bin"});
        }
        ImGui::PopID();
        ImGui::SameLine();
        ui::Text(dmsg::colmap_vocab_tree);

        draw_color_space_options(/*with_point_color=*/false);
    }
}

void GuiApp::draw_tool_locations() {
    if (ui::CollapsingHeader(dmsg::section_tool_locations)) {
        bool ch = false;
        if (effective_engine() == Engine::Colmap) {
            ImGui::SetNextItemWidth(px(300.0f));
            ch |= ui::InputText(dmsg::colmap_executable, &_colmap_exe);
        }
        ImGui::SetNextItemWidth(px(300.0f));
        ch |= ui::InputText(dmsg::ffmpeg_executable, &_ffmpeg_exe);
        ui::help_on_hover(backends().builtin_video
                              ? dmsg::ffmpeg_executable_help_fallback
                              : dmsg::ffmpeg_executable_help_always);
        ImGui::SetNextItemWidth(px(300.0f));
        ch |= ui::InputText(dmsg::python_executable, &_python_exe);
        ui::help_on_hover(dmsg::python_executable_help);
        if (ch) save_settings();
    }
}

// ---------------------------------------------------------------------------
// The screen
// ---------------------------------------------------------------------------

// The form, and under it the button that acts on it. One column of the screen
// when a run has something to show beside it, the whole of it otherwise.
void GuiApp::draw_dataset_form(float height, bool running) {
    // The action band's height is measured from the last frame, because how
    // tall it is depends on what it is saying.
    ImGui::BeginChild("##dsform", ImVec2(0, height - _ds_action_h));

    // What is greyed out is decided per section, by whether the step that
    // reads it has started -- see dataset_locked. The whole form used to grey
    // the instant a run began, which left nothing to do for the twenty minutes
    // a capture takes to extract, and nothing to look at but the log.
    //
    // The inputs and the engine define the run and are the exception: they are
    // fixed the moment it starts.
    ImGui::BeginDisabled(running);
    draw_dataset_source();

    // The selector appears only when both engines are usable: a Vulkan user
    // with no COLMAP installed never learns that COLMAP exists, and a CUDA
    // user is never offered a back end this build does not have.
    if (builtin_sfm_available() && colmap_available()) {
        ImGui::Spacing();
        int eng = _engine == Engine::BuiltIn ? 0 : 1;
        ui::Text(dmsg::reconstruction);
        ImGui::SameLine();
        if (ui::RadioButton(dmsg::engine_builtin, eng == 0)) eng = 0;
        ui::help_on_hover(dmsg::engine_builtin_help);
        ImGui::SameLine();
        if (ui::RadioButton(dmsg::engine_colmap, eng == 1)) eng = 1;
        ui::help_on_hover(dmsg::engine_colmap_help);
        if ((eng == 1) != (_engine == Engine::Colmap)) {
            _engine = eng == 1 ? Engine::Colmap : Engine::BuiltIn;
            save_settings();
        }
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
    ui::SeparatorText(dmsg::section_settings);
    draw_dataset_basics();
    ImGui::Spacing();
    ImGui::BeginDisabled(dataset_locked(Stage::Masks));
    draw_masking_options();
    ImGui::EndDisabled();
    ImGui::Spacing();

    ImGui::BeginDisabled(dataset_locked(Stage::Geometry));
    draw_geometry_options();
    ImGui::EndDisabled();
    ImGui::Spacing();

    ImGui::BeginDisabled(dataset_locked(Stage::Features));
    if (effective_engine() == Engine::BuiltIn) draw_sfm_advanced();
    else                                       draw_colmap_options();
    ImGui::EndDisabled();
    // Tool locations are settings of the application, not of the run.
    draw_tool_locations();

    ImGui::EndChild();

    // Edits made above reach the running job at the step that reads them.
    update_dataset_job();

    // ---- run / status ----
    const float action_y0 = ImGui::GetCursorPosY();
    ImGui::Spacing();
    if (!running) {
        bool ready = !_sources.empty() && !_workspace.empty();
        for (const PrepInput& s : _sources) ready = ready && !s.path.empty();
        const bool need_mask_model = mask_model_missing();
        const bool need_feat_model = feature_model_missing();
        const bool need_geom_model = geometry_model_missing();
        const bool need_model = need_mask_model || need_feat_model || need_geom_model;
        // The button names what pressing it does: a folder that already holds
        // a reconstruction is added to, not built.
        const bool adding = workspace_state().model && !_redo_model;
        ImGui::BeginDisabled(!ready || need_model);
        if (ui::Button(adding ? dmsg::update_dataset : dmsg::create_dataset,
                       ImVec2(px(200.0f), px(34.0f))))
            start_dataset_job();
        ImGui::EndDisabled();
        if (!ready) {
            ImGui::SameLine();
            ui::TextDisabled(dmsg::pick_input_first);
        } else if (need_model) {
            // The options above carry the same buttons, but they are a scroll
            // away by the time somebody is reaching for this one. One missing
            // checkpoint at a time; the next takes its place once this lands.
            FileDownload& dl = need_mask_model ? _download
                               : need_feat_model ? _feat_download.current()
                                                 : _geom_download.current();
            ImGui::SameLine();
            ui::TextDisabled(need_mask_model   ? dmsg::mask_model_first
                             : need_feat_model ? dmsg::feat_model_first
                                               : dmsg::geom_model_first);
            ImGui::SameLine();
            if (dl.state() == FileDownload::State::Running)
                ui::ProgressBarRaw(std::max(dl.progress(), 0.0f),
                                   ImVec2(px(200.0f), 0), dl.status().c_str());
            else if (ui::Button(need_mask_model   ? dmsg::mask_get_model
                                : need_feat_model ? dmsg::feat_get_model
                                                  : dmsg::geom_get_model)) {
                if (need_mask_model)      request_model_download();
                else if (need_feat_model) request_feature_download();
                else                      request_geometry_download();
            }
        }
        if (ready) draw_dataset_rerun(workspace_state());
    } else if (ui::Button(dmsg::cancel, ImVec2(px(200.0f), px(34.0f)))) {
        cancel_dataset_job();
    }

    // Both runners report through the same three states.
    struct {
        bool done, failed, cancelled;
        std::string dir, image_dir, mask_dir, err;
        bool mask_flipped;
    } st{};
    if (effective_engine() == Engine::BuiltIn) {
        st.done = _sfm.state() == SfmRunner::State::Done;
        st.failed = _sfm.state() == SfmRunner::State::Failed;
        st.cancelled = _sfm.state() == SfmRunner::State::Cancelled;
        st.dir = _sfm.dataset_dir();
        st.image_dir = _sfm.image_dir();
        st.mask_dir = _sfm.mask_dir();
        st.mask_flipped = _sfm.mask_flipped();
        st.err = _sfm.error();
    } else {
        st.done = _colmap.state() == ColmapRunner::State::Done;
        st.failed = _colmap.state() == ColmapRunner::State::Failed;
        st.cancelled = _colmap.state() == ColmapRunner::State::Cancelled;
        st.dir = _colmap.dataset_dir();
        st.image_dir = _colmap.image_dir();
        st.mask_dir = _colmap.mask_dir();
        st.mask_flipped = _colmap.mask_flipped();
        st.err = _colmap.error();
    }
    if (st.done) {
        if (effective_engine() == Engine::BuiltIn && _sfm.partial())
            ui::TextColoredWrapped(kWarn, dmsg::partial_reconstruction);
        ui::TextColoredWrapped(kOk, dmsg::done_at, {st.dir});
        if (ui::Button(dmsg::open_in_trainer)) {
            if (training_busy()) {
                _pending = Pending::OpenDataset;
                _pending_path = st.dir;
                _open_confirm = true;
            } else {
                open_dataset(st.dir, st.image_dir, st.mask_dir, st.mask_flipped,
                             /*keep_log=*/true);
            }
        }
    } else if (st.failed) {
        ui::TextColoredWrapped(kErr, dmsg::failed, {st.err});
    } else if (st.cancelled) {
        ui::TextColored(kDim, dmsg::cancelled);
    }
    _ds_action_h = ImGui::GetCursorPosY() - action_y0;
}

void GuiApp::draw_new_dataset() {
    const bool running = dataset_busy();

    if (ui::Button(msg::back_home)) {
        _screen = Screen::Home;
        // The preview holds GL buffers and a decoding thread for a screen that
        // is no longer up.
        if (!dataset_busy()) reset_dataset_preview();
    }
    ImGui::SameLine();
    ImGui::SetWindowFontScale(1.2f);
    const bool from_video = !_sources.empty() && _sources[0].is_video;
    ui::Text(from_video ? dmsg::title_from_video : dmsg::title_from_photos);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Spacing();

    // What the run says about itself, read before the layout is decided:
    // whether there is anything to show is what decides whether the screen is
    // one column or two.
    poll_sfm_progress();

    // The log spans the bottom whatever the body does above it: it is the one
    // panel that is read across everything, and it is what a run used to be
    // watched entirely through.
    const float log_h = log_height(ImGui::GetContentRegionAvail().y);
    const float body_h = ImGui::GetContentRegionAvail().y - log_h -
                         (log_h > 0 ? splitter_extent() : 0);

    // Side by side only when there is a preview AND room for both. On a narrow
    // window the preview goes under the form instead, which is worth less but
    // costs the form nothing it cannot scroll.
    const bool wide = ImGui::GetContentRegionAvail().x >= px(1000.0f);
    const bool two_col = preview_has_content() && wide;

    ImGui::BeginChild("##dsbody", ImVec2(0, body_h));
    if (two_col) {
        const float w = std::clamp(_ds_panel_w * ui_scale(), px(320.0f),
                                   std::max(px(320.0f),
                                            ImGui::GetContentRegionAvail().x * 0.6f));
        const float col_h = ImGui::GetContentRegionAvail().y;
        ImGui::BeginChild("##dsleft", ImVec2(w, col_h));
        draw_dataset_form(col_h, running);
        ImGui::EndChild();
        float dragged = w;
        if (splitter_v("##dspanelsplit", &dragged, px(320.0f),
                       ImGui::GetWindowWidth() * 0.75f, col_h)) {
            _ds_panel_w = dragged / ui_scale();
            _layout_dirty = true;
        }
        ImGui::BeginGroup();
        draw_dataset_steps();
        draw_dataset_preview(ImGui::GetContentRegionAvail().y);
        ImGui::EndGroup();
    } else {
        const float avail = ImGui::GetContentRegionAvail().y;
        // The step strip belongs beside the Cancel button when it has no
        // column of its own.
        draw_dataset_form(avail - _ds_preview_h_used, running);
        const float y0 = ImGui::GetCursorPosY();
        if (running) draw_dataset_steps();
        draw_dataset_preview(0.0f);
        _ds_preview_h_used = ImGui::GetCursorPosY() - y0;
    }
    ImGui::EndChild();

    draw_log_panel(log_h);

    if (_segment.is_open()) {
        // It edits one input's stencil and shows one input's frames, so it
        // cannot outlive the list it was opened against.
        if (_sources.empty()) {
            _segment.close();
        } else {
            if (_mask_preview_input >= (int)_sources.size()) _mask_preview_input = 0;
            _segment.draw(_mask, _sources[(size_t)_mask_preview_input].stencil);
        }
    }
    if (_geometry_panel.is_open()) _geometry_panel.draw(_geometry);
    draw_license_modal();
}

// ---------------------------------------------------------------------------
// Licence consent
//
// Shown once per model family, before the first download. Deliberately short:
// what it is, whose it is, whether anything unusual is being agreed to, and a
// link. A wall of text here would be read by nobody, which is the outcome the
// requirement exists to avoid.
// ---------------------------------------------------------------------------

void GuiApp::draw_license_modal() {
    if (_license_prompt.empty()) return;
    const LicenseInfo& li = license_for(_license_prompt);

    ui::OpenPopup(dmsg::license_modal_title);
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(540, 0), ImGuiCond_Always);
    if (!ui::BeginPopupModal(dmsg::license_modal_title, nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ImGui::SetWindowFontScale(1.15f);
    ui::Text(*li.title);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Spacing();
    ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
    ui::Text(*li.summary);
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    // The link is a button, not decoration: the tick below says the user has
    // read the terms, so getting to them has to be one obvious click. Copying
    // the address is the fallback for a session with no browser to launch.
    if (ui::Button(dmsg::license_read, ImVec2(180, 0))) {
        if (!open_url(li.url)) {
            ImGui::SetClipboardText(li.url);
            log(i18n::format(dmsg::license_no_browser, {li.url}));
        }
    }
    ui::help_on_hover_raw(li.url);
    ImGui::SameLine();
    if (ui::Button(dmsg::license_copy_link, ImVec2(110, 0)))
        ImGui::SetClipboardText(li.url);
    ImGui::Spacing();
    ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
    ui::TextDisabledRaw(li.url);
    ImGui::PopTextWrapPos();
    if (const ModelEntry* e = find_model(_model_id))
        ui::TextDisabled(dmsg::license_download_size, {human_bytes(e->bytes)});
    ImGui::Spacing();

    if (li.needs_tick)
        ui::Checkbox(dmsg::license_accept_tick, &_license_tick);
    ImGui::Spacing();

    ImGui::BeginDisabled(li.needs_tick && !_license_tick);
    if (ui::Button(dmsg::license_download, ImVec2(150, 0))) {
        _accepted_licenses.push_back(_license_prompt);
        save_settings();
        if (const ModelEntry* e = find_model(_model_id)) _download.start(*e);
        _license_prompt.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ui::Button(dmsg::cancel, ImVec2(120, 0))) {
        _license_prompt.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}


// ===========================================================================
// Train screen
// ===========================================================================

void GuiApp::draw_train() {
    bool preparing = _runner.phase() == TrainRunner::Phase::Preparing;
    ImGui::BeginDisabled(preparing);   // no clean interruption point
    if (ui::Button(msg::back_home)) request_go_home();
    ImGui::EndDisabled();
    if (training_busy()) ui::help_on_hover(msg::leaving_stops_training);
    ImGui::SameLine();
    if (_batch_active) {
        if (ui::Button(msg::batch_show_list)) _screen = Screen::Batch;
        ImGui::SameLine();
        ui::TextDisabledRaw(_batch_current >= 0 ? _batch[_batch_current].dataset
                                                : std::string());
    } else {
        ui::TextDisabledRaw(_cfg.data);
    }
    // Two ways to watch a run: the scene in 3D, or one training photograph
    // beside the render of the same camera. Right-aligned on the header row so
    // it costs the preview below no height.
    {
        const float w = px(160.0f);
        ImGui::SameLine(std::max(0.0f, ImGui::GetContentRegionMax().x - w));
        ImGui::SetNextItemWidth(w);
        int mode = _preview_images ? 1 : 0;
        if (ui::ComboRaw("##previewmode", &mode,
                         {&msg::preview_mode_3d, &msg::preview_mode_images}))
            _preview_images = mode == 1;
        ui::help_on_hover(msg::preview_mode_help);
    }

    const float body_avail = ImGui::GetContentRegionAvail().y;
    if (_show_settings) {
        // Never more than 45% of the window: the viewport is the point of the
        // screen, and a settings column sized for a 1600 px window swallows a
        // 1100 px one.
        const float w = std::clamp(_panel_w * ui_scale(), px(220.0f),
                                   std::max(px(220.0f),
                                            ImGui::GetContentRegionAvail().x * 0.45f));
        ImGui::BeginChild("##settings", ImVec2(w, 0), ImGuiChildFlags_Borders);
        draw_train_settings();
        ImGui::EndChild();
        float dragged = w;
        if (splitter_v("##panelsplit", &dragged, px(220.0f),
                       ImGui::GetWindowWidth() * 0.7f, body_avail)) {
            _panel_w = dragged / ui_scale();
            _layout_dirty = true;
        }
    }

    ImGui::BeginGroup();
    float status_h = ImGui::GetFrameHeightWithSpacing() +
                     ImGui::GetTextLineHeightWithSpacing() + px(10.0f);
    float spacing = ImGui::GetStyle().ItemSpacing.y;
    float log_h = log_height(body_avail - status_h - spacing);
    float vp_h = -(log_h + (log_h > 0 ? splitter_extent() : 0) + status_h + spacing);
    ImGui::BeginChild("##viewport", ImVec2(0, vp_h), ImGuiChildFlags_Borders);
    const bool stepping = _runner.phase() == TrainRunner::Phase::Training;
    // The step both previews pace their refresh by while nobody is steering.
    const int step = stepping ? _runner.latest_progress().step : -1;
    if (_preview_images) _images.draw(stepping, step);
    else                 _viewport.draw(stepping, step);
    ImGui::EndChild();
    draw_status_strip();
    draw_log_panel(log_h);
    ImGui::EndGroup();
}


// ===========================================================================
// Viewer screen
//
// A finished model, with nothing being trained: the same viewport, the same
// navigation and the same camera models as during a run, over splats read
// from a file instead of ones an optimizer is still moving. What it is for is
// the thing a trainer cannot do -- open somebody else's result, or your own
// from last week, and look at it.
// ===========================================================================

void GuiApp::draw_viewer() {
    if (ui::Button(msg::back_home)) request_go_home();
    ImGui::SameLine();
    if (ui::Button(msg::viewer_open_another)) {
        _pick = PickAction::SplatFile;
        _dialog.open(msg::viewer_pick_file.get(), FileDialog::Mode::File,
                     kViewableExtensions);
    }
    ImGui::SameLine();
    _compare.set_recents(_model_recents);
    _compare.draw_toolbar();

    const float log_h = log_height(ImGui::GetContentRegionAvail().y);
    ImGui::BeginChild("##viewer", ImVec2(0, body_height(log_h)));
    _compare.draw(0.0f);
    ImGui::EndChild();
    draw_log_panel(log_h);
}


// ===========================================================================
// Mesh screen
//
// One screen for the whole thing: what to mesh, the four choices that matter
// (color, formats, how many photos, where it goes), an Advanced header for the
// rest, and -- once it has run -- the splats and the extracted surface side by
// side on one linked camera, which is the only honest way to look at a mesh.
// ===========================================================================

void GuiApp::set_mesh_source(const std::string& path) {
    if (path.empty()) return;
    _mesh_job.checkpoint = path;
    // The output name follows the source until the user edits it. A file
    // becomes <name>_mesh, NEVER <name> -- meshing `splat.ply` to base
    // `splat` writes `splat.ply`, i.e. over the model being meshed.
    std::error_code ec;
    fs::path base(path);
    if (fs::is_regular_file(base, ec)) {
        const std::string stem = base.stem().string();
        base.replace_filename(stem + "_mesh");
    } else {
        base /= "mesh";
    }
    _mesh_job.output = base.string();
    // A run folder usually records its dataset; leave the field empty and let
    // the child read config.json, which is what the help text promises.
    _mesh_job.data_dir.clear();
    close_mesh_preview();
}

bool GuiApp::mesh_dataset_found() {
    if (!_mesh_job.use_data) return false;
    const std::string key = _mesh_job.checkpoint + '\n' + _mesh_job.data_dir;
    if (key == _mesh_data_probe_key) return _mesh_data_probe_found;
    _mesh_data_probe_key = key;
    _mesh_data_probe_found = false;
    std::error_code ec;
    if (!_mesh_job.data_dir.empty()) {
        // Exactly the child's test: a folder that is not there is no dataset,
        // and it meshes without one rather than failing.
        _mesh_data_probe_found = fs::exists(_mesh_job.data_dir, ec);
    } else if (!_mesh_job.checkpoint.empty()) {
        // Empty field: the child reads `data` out of the run's config.json,
        // relative to the run directory. Both steps throw on a path that is
        // not a checkpoint at all, which simply means no dataset.
        try {
            auto [ply, run_dir] = spirula::find_splat_ply(_mesh_job.checkpoint);
            (void)ply;
            const fs::path cfg = fs::path(run_dir) / "config.json";
            if (fs::is_regular_file(cfg, ec)) {
                const JsonValue run_cfg = json_parse_file(cfg.string());
                const JsonValue* d = run_cfg.find("data");
                if (d && !d->is_null()) {
                    fs::path cand = d->as_string();
                    if (cand.is_relative()) cand = fs::path(run_dir) / cand;
                    _mesh_data_probe_found = fs::exists(cand, ec);
                }
            }
        } catch (const std::exception&) {
        }
    }
    return _mesh_data_probe_found;
}

void GuiApp::start_meshing() {
    if (_mesh_job.checkpoint.empty()) return;
    close_mesh_preview();
    _mesh.start(_mesh_job);
}

void GuiApp::close_mesh_preview() {
    if (!_mesh_preview_open) return;
    _compare.close();
    _mesh_preview_open = false;
}

void GuiApp::open_mesh_preview() {
    // Whatever else had the engine (a file opened from the viewer screen) has
    // to let go before the splats take it.
    close_splat();
    const std::string out = _mesh.output_path();
    if (out.empty()) return;
    // The model the mesh came from goes FIRST, which puts the surface in the
    // splats' frame rather than one fitted to itself. Skipped only while a run
    // is USING the engine -- requiring an IDLE one hid every train-then-mesh.
    if (!training_busy()) {
        detach_session_views();
        _runner.note_engine_taken();
        _compare.add(_mesh_job.checkpoint, &msg::mesh_side_splats);
    }
    _compare.add(out, &msg::mesh_side_mesh);
    _mesh_preview_open = true;
}

void GuiApp::draw_mesh_options() {
    // A path row is [field][...][label]. The field takes what is left after
    // the button and the label, measured rather than guessed -- a fixed
    // reserve pushes the label off the edge in the longer languages.
    const float style_x = ImGui::GetStyle().ItemSpacing.x;
    auto field_width = [&](const Msg& label) {
        return -(60.0f + 2.0f * style_x + ImGui::CalcTextSize(label.get()).x);
    };

    // ---- what to mesh ----
    ui::SeparatorText(msg::mesh_source);
    ImGui::SetNextItemWidth(px(-140.0f));
    if (ui::InputTextRaw("##meshsrc", &_mesh_job.checkpoint)) {
        // Typed by hand: keep the derived output in step until it is edited.
    }
    ImGui::SameLine();
    if (ui::ButtonRaw("...##meshsrcdir", ImVec2(60, 0))) {
        _pick = PickAction::MeshSource;
        _dialog.open(msg::mesh_pick_model.get(), FileDialog::Mode::Folder);
    }
    ImGui::SameLine();
    if (ui::ButtonRaw(".ply##meshsrcfile", ImVec2(60, 0))) {
        _pick = PickAction::MeshSource;
        _dialog.open(msg::mesh_pick_model.get(), FileDialog::Mode::File,
                     {".ply"});
    }
    ui::help_on_hover(msg::mesh_source_help);
    ui::TextDisabled(msg::mesh_drop_hint);

    // ---- the photos ----
    ui::Checkbox(msg::mesh_use_photos, &_mesh_job.use_data);
    ui::help_on_hover(msg::mesh_use_photos_help);
    // Meshing without cameras is a real choice, and a much worse mesh, so it
    // is said out loud on the screen rather than left to the child's warning
    // in the log panel -- which scrolls past, and only after the run starts.
    if (!_mesh_job.use_data)
        ui::TextColoredWrapped(kWarn, msg::mesh_no_photos_warn);
    if (_mesh_job.use_data) {
        ImGui::Indent();
        ImGui::SetNextItemWidth(field_width(msg::mesh_photos_dir));
        ui::InputTextWithHintRaw("##meshdata", msg::mesh_photos_dir_help,
                                 &_mesh_job.data_dir);
        ImGui::SameLine();
        if (ui::ButtonRaw("...##meshdatapick", ImVec2(60, 0))) {
            _pick = PickAction::MeshPhotos;
            _dialog.open(msg::mesh_pick_photos.get(), FileDialog::Mode::Folder);
        }
        ImGui::SameLine();
        ui::TextDisabled(msg::mesh_photos_dir);
        // The box is ticked but nothing will be found: a loose splat.ply, a
        // run whose config.json records a dataset that has since moved, or a
        // folder typed wrong. Same outcome as unticking it, so same warning.
        // (Silent while there is no model yet: with nothing picked there is
        // nothing to have found a dataset FOR, and the Create button already
        // says what is missing.)
        if (!_mesh_job.checkpoint.empty() && !mesh_dataset_found())
            ui::TextColoredWrapped(kWarn, msg::mesh_photos_missing_warn);

        ImGui::SetNextItemWidth(px(120.0f));
        ui::InputInt(msg::mesh_max_cameras, &_mesh_job.max_cameras);
        _mesh_job.max_cameras = std::max(0, _mesh_job.max_cameras);
        ui::help_on_hover(msg::mesh_max_cameras_help);
        ImGui::Unindent();
    }

    // ---- color ----
    ImGui::SetNextItemWidth(px(220.0f));
    ui::Combo(msg::mesh_color, &_mesh_job.color,
              {&msg::mesh_color_none, &msg::mesh_color_vertex,
               &msg::mesh_color_texture});
    if (_mesh_job.color == 2) {
        ImGui::Indent();
        static const int kTexSizes[] = {0, 1024, 2048, 4096, 8192};
        int idx = 0;
        for (int i = 0; i < 5; i++)
            if (kTexSizes[i] == _mesh_job.texture_size) idx = i;
        // Mixed list: one translated entry and four resolutions, which are
        // numbers in every language -- hence ComboRaw with a separate label.
        char items[5][64];
        const char* ptrs[5];
        for (int i = 0; i < 5; i++) {
            if (i == 0) std::snprintf(items[i], sizeof items[i], "%s",
                                      msg::mesh_texture_size_auto.get());
            else std::snprintf(items[i], sizeof items[i], "%d", kTexSizes[i]);
            ptrs[i] = items[i];
        }
        ImGui::SetNextItemWidth(px(160.0f));
        if (ui::ComboRaw("##texsize", &idx, ptrs, 5))
            _mesh_job.texture_size = kTexSizes[idx];
        ImGui::SameLine();
        ui::TextDisabled(msg::mesh_texture_size);
        ImGui::Unindent();
    }

    // ---- formats ----
    // PLY cannot carry a texture and OBJ has no standard place for vertex
    // colors; the child refuses those combinations outright, so they are
    // greyed out here rather than failing the run three minutes in.
    ui::SeparatorText(msg::mesh_formats);
    for (int i = 0; i < kNumMeshFormats; i++) {
        if (i) ImGui::SameLine();
        const bool ok = !(i == 0 && _mesh_job.color == 2) &&
                        !(i == 1 && _mesh_job.color == 1);
        if (!ok) _mesh_job.formats[i] = false;
        ImGui::BeginDisabled(!ok);
        // A file extension is a file extension in every language.
        ImGui::PushID(i);
        ui::CheckboxRaw(kMeshFormats[i], &_mesh_job.formats[i]);
        ImGui::PopID();
        ImGui::EndDisabled();
    }
    // Every format was ruled out by the color choice: fall back to one that
    // can carry it, so the Create button is never armed with nothing to write.
    bool any = false;
    for (int i = 0; i < kNumMeshFormats; i++) any |= _mesh_job.formats[i];
    if (!any) _mesh_job.formats[_mesh_job.color == 2 ? 3 : 0] = true;
    ImGui::SetNextItemWidth(field_width(msg::mesh_output));
    ui::InputTextRaw("##meshout", &_mesh_job.output);
    ImGui::SameLine();
    if (ui::ButtonRaw("...##meshoutpick", ImVec2(60, 0))) {
        _pick = PickAction::MeshOutput;
        _dialog.open(msg::mesh_pick_output.get(), FileDialog::Mode::Folder);
    }
    ImGui::SameLine();
    ui::TextDisabled(msg::mesh_output);
    ui::help_on_hover(msg::mesh_output_help);

    // ---- advanced ----
    if (ui::CollapsingHeader(msg::mesh_advanced)) {
        ImGui::SetNextItemWidth(px(220.0f));
        ui::SliderFloat(msg::mesh_detail, &_mesh_job.merge_factor, 0.25f, 4.0f,
                        "%.2f");
        ui::help_on_hover(msg::mesh_detail_help);
        ImGui::SetNextItemWidth(px(220.0f));
        ui::InputInt(msg::mesh_drop_specks, &_mesh_job.floater_min_faces);
        _mesh_job.floater_min_faces = std::max(0, _mesh_job.floater_min_faces);
        if (_mesh_job.use_data)
            ui::Checkbox(msg::mesh_cull_unseen, &_mesh_job.cull_unseen);
        ImGui::SetNextItemWidth(
            -(style_x + ImGui::CalcTextSize(msg::mesh_extra_args.get()).x));
        ui::InputTextRaw("##meshextra", &_mesh_job.extra_args);
        ImGui::SameLine();
        ui::TextDisabled(msg::mesh_extra_args);
    }
}

void GuiApp::draw_mesh() {
    // The run finished while this screen was up: show what it made, ONCE.
    // Doing this here rather than in frame() keeps the engine take-over on the
    // one screen that asked for it; keying on the run id rather than on
    // "Done && not open" is what lets the preview be closed (by the button,
    // or by leaving for Home) without it springing straight back.
    if (_mesh.state() == MeshRunner::State::Done && !_mesh_preview_open &&
        _mesh.run_id() != _mesh_shown_run) {
        _mesh_shown_run = _mesh.run_id();
        open_mesh_preview();
    }

    if (ui::Button(msg::back_home)) request_go_home();
    ImGui::SameLine();
    ui::Text(msg::mesh_title);

    const bool running = _mesh.busy();
    const float log_h = log_height(ImGui::GetContentRegionAvail().y);

    ImGui::BeginChild("##meshbody", ImVec2(0, body_height(log_h)));

    if (_mesh_preview_open) {
        // ---- results ----
        if (_mesh.num_verts() > 0)
            ui::Text(msg::mesh_done, {(long long)_mesh.num_verts(),
                                      (long long)_mesh.num_faces()});
        ui::TextDisabledRaw(_mesh.output_path());
        if (ui::Button(msg::mesh_open_in_viewer)) {
            // Hand both panels back first: the viewer screen reuses _compare,
            // which is holding this preview's models.
            const std::string out = _mesh.output_path();
            close_mesh_preview();
            request_open_splat(out);
        }
        ImGui::SameLine();
        // Same message as the button that started this run, so it needs its
        // own ImGui ID (a label IS the ID -- see AGENTS.md).
        ImGui::PushID("again");
        if (ui::Button(msg::mesh_start)) close_mesh_preview();
        ImGui::PopID();
        _compare.set_recents(_model_recents);
        _compare.draw_toolbar();
        _compare.draw(0.0f);
    } else {
        ImGui::BeginDisabled(running);
        draw_mesh_options();
        ImGui::EndDisabled();

        ImGui::Spacing();
        if (running) {
            const float p = _mesh.progress();
            ui::ProgressBar(p >= 0 ? p : 0.0f, ImVec2(-1, 0), msg::mesh_running);
            const std::string st = _mesh.stage();
            if (!st.empty()) ui::TextDisabledRaw(st);
            if (ui::Button(msg::mesh_cancel)) _mesh.cancel();
        } else {
            ImGui::BeginDisabled(_mesh_job.checkpoint.empty());
            if (ui::Button(msg::mesh_start, ImVec2(220, 34))) start_meshing();
            ImGui::EndDisabled();
            if (_mesh_job.checkpoint.empty()) {
                ImGui::SameLine();
                ui::TextColored(kDim, msg::mesh_no_model);
            }
            if (_mesh.state() == MeshRunner::State::Failed) {
                ui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), msg::mesh_failed);
                ui::TextDisabledRaw(_mesh.error());
            } else if (_mesh.state() == MeshRunner::State::Cancelled) {
                ui::TextColored(kDim, dmsg::cancelled);
            }
        }
    }
    ImGui::EndChild();
    draw_log_panel(log_h);
}


// ===========================================================================
// Batch screen
//
// The list, and the two things that can be done to it: check it, and run it.
// It stays editable and readable while a batch is in flight -- the rows report
// what became of them -- which is what makes an unattended run something you
// can come back to rather than something you have to watch.
// ===========================================================================

void GuiApp::draw_batch() {
    // Keeps the saved presets warm for the rows' pickers and their tooltips
    // (rate-limited inside, so this is a no-op most frames).
    refresh_presets();

    if (ui::Button(msg::back_home)) request_go_home();
    if (_batch_active) {
        ImGui::SameLine();
        if (ui::Button(msg::batch_show_training)) _screen = Screen::Train;
    }
    ImGui::SameLine();
    ui::Text(msg::batch_title);

    ImGui::PushTextWrapPos();
    ui::TextDisabled(msg::batch_intro);
    ImGui::PopTextWrapPos();

    const float log_h = log_height(ImGui::GetContentRegionAvail().y);
    ImGui::BeginChild("##batchlist", ImVec2(0, body_height(log_h)));

    draw_batch_table();

    ImGui::Spacing();
    ImGui::BeginDisabled(_batch_active);
    if (ui::Button(msg::batch_add_row)) {
        _pick = PickAction::BatchDataset;
        _pick_row = -1;   // append
        _dialog.open(msg::batch_pick_dataset.get(), FileDialog::Mode::Folder);
    }
    ImGui::SameLine();
    // The datasets already opened in the trainer, which is where a queue is
    // usually assembled from -- they are the ones already known to parse.
    if (ui::Button(msg::batch_add_recent)) ImGui::OpenPopup("##batchrecent");
    if (ImGui::BeginPopup("##batchrecent")) {
        if (_recents.empty()) {
            ui::TextDisabled(msg::batch_no_recent);
        } else {
            for (size_t i = 0; i < _recents.size(); i++) {
                ImGui::PushID((int)i);
                // A path is a path in every language.
                if (ui::SelectableRaw(_recents[i])) add_batch_row(_recents[i]);
                ImGui::PopID();
            }
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(_batch.empty());
    if (ui::Button(msg::batch_clear)) {
        _batch.clear();
        _batch_dirty = true;
        _batch_checked = false;
        _batch_msg.clear();
    }
    ImGui::SameLine();
    if (ui::Button(msg::batch_check)) check_batch();
    ui::help_on_hover(msg::batch_check_help);

    ImGui::SameLine();
    if (ui::Button(msg::batch_start, ImVec2(160, 0)))
        request_start_batch(/*skip_invalid=*/false);
    // The way past a row that cannot be fixed right now. Only offered once a
    // check has actually found one, so it is never the first thing tried.
    int bad = 0;
    if (_batch_checked)
        for (const BatchJob& j : _batch) bad += batch_has_error(j) ? 1 : 0;
    if (bad > 0 && bad < (int)_batch.size()) {
        ImGui::SameLine();
        if (ui::Button(msg::batch_start_skip))
            request_start_batch(/*skip_invalid=*/true);
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    if (!_batch_msg.empty())
        ui::TextColoredWrappedRaw(_batch_msg_err ? kErr : kOk, _batch_msg);

    draw_batch_issues();
    ImGui::EndChild();

    draw_log_panel(log_h);
}

void GuiApp::draw_batch_table() {
    if (_batch.empty()) {
        ui::TextDisabled(msg::batch_empty);
        ui::TextDisabled(msg::batch_drop_hint);
        return;
    }

    const ImGuiTableFlags flags = ImGuiTableFlags_Borders |
                                  ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_SizingStretchProp;
    if (!ImGui::BeginTable("##batch", 8, flags)) return;

    // "#" is a numeral column, not a word.
    ui::TableSetupColumnRaw("#", ImGuiTableColumnFlags_WidthFixed, 26.0f);
    ui::TableSetupColumn(msg::batch_col_dataset,
                         ImGuiTableColumnFlags_WidthStretch, 3.0f);
    ui::TableSetupColumn(msg::batch_col_preset,
                         ImGuiTableColumnFlags_WidthStretch, 2.0f);
    // Wide enough for the longest of the thirteen headings, not just the
    // English one: a fixed column sized to "Max splats" truncates
    // "Максимум сплатов" and every CJK heading, whose characters are twice
    // as wide as they are numerous.
    ui::TableSetupColumn(msg::batch_col_splats,
                         ImGuiTableColumnFlags_WidthFixed, 116.0f);
    ui::TableSetupColumn(msg::batch_col_sh,
                         ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ui::TableSetupColumn(msg::batch_col_steps,
                         ImGuiTableColumnFlags_WidthFixed, 84.0f);
    ui::TableSetupColumn(msg::batch_col_output,
                         ImGuiTableColumnFlags_WidthStretch, 3.0f);
    ui::TableSetupColumn(msg::batch_col_status,
                         ImGuiTableColumnFlags_WidthFixed, 190.0f);
    ImGui::TableHeadersRow();

    const float bw = ImGui::GetFrameHeight() + 6.0f;
    int remove = -1;
    for (int i = 0; i < (int)_batch.size(); i++) {
        BatchJob& j = _batch[i];
        ImGui::PushID(i);
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ui::TextDisabledRaw(std::to_string(i + 1));

        // A row is frozen while the batch runs: its config has already been
        // taken, and editing it would describe a run that is not happening.
        ImGui::BeginDisabled(_batch_active);

        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-bw);
        if (ui::InputTextWithHintRaw("##ds", msg::batch_dataset_hint,
                                     &j.dataset)) {
            _batch_dirty = true;
            _batch_checked = false;
        }
        ImGui::SameLine(0, 2);
        if (ui::ButtonRaw("...##ds")) {
            _pick = PickAction::BatchDataset;
            _pick_row = i;
            _dialog.open(msg::batch_pick_dataset.get(),
                         FileDialog::Mode::Folder, {}, j.dataset);
        }

        ImGui::TableNextColumn();
        draw_batch_preset_combo(j, i);

        // The three numbers worth changing without making a preset for each
        // combination. Empty means "whatever the preset says", which is why
        // these are text boxes and not integer spinners -- 0 is a legal SH
        // degree, so a spinner has no way to spell "unset".
        const char* ids[] = {"##cap", "##sh", "##iters"};
        std::string* fields[] = {&j.cap_max_override, &j.sh_degree_override,
                                 &j.iterations_override};
        for (int k = 0; k < 3; k++) {
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ui::InputTextWithHintRaw(ids[k], msg::batch_override_hint,
                                         fields[k],
                                         ImGuiInputTextFlags_CharsDecimal)) {
                _batch_dirty = true;
                _batch_checked = false;
            }
            ui::help_on_hover(msg::batch_override_help);
        }

        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-bw);
        if (ui::InputTextWithHintRaw("##out", msg::batch_output_hint,
                                     &j.output_dir)) {
            _batch_dirty = true;
            _batch_checked = false;
        }
        ui::help_on_hover(msg::batch_output_help);
        ImGui::SameLine(0, 2);
        if (ui::ButtonRaw("...##out")) {
            _pick = PickAction::BatchOutput;
            _pick_row = i;
            _dialog.open(msg::batch_pick_output.get(), FileDialog::Mode::Folder,
                         {}, j.output_dir);
        }
        ImGui::EndDisabled();

        ImGui::TableNextColumn();
        switch (j.status) {
            case BatchJob::Status::Running:
                ui::TextColored(kWarn, msg::batch_status_running); break;
            case BatchJob::Status::Done:
                ui::TextColored(kOk, msg::batch_status_done); break;
            case BatchJob::Status::Failed:
                ui::TextColored(kErr, msg::batch_status_failed); break;
            case BatchJob::Status::Skipped:
                ui::TextColored(kDim, msg::batch_status_skipped); break;
            case BatchJob::Status::Stopped:
                ui::TextColored(kDim, msg::batch_status_stopped); break;
            default:
                ui::TextColored(kDim, msg::batch_status_pending); break;
        }
        // Where it went, or why it did not: engine text and paths, both raw.
        const std::string& detail = j.message.empty() ? j.out_dir : j.message;
        if (!detail.empty()) ui::help_on_hover_raw(detail.c_str());
        ImGui::SameLine();
        ImGui::BeginDisabled(_batch_active);
        // The same word the dataset screen's input list uses for the same job.
        if (ui::Button(dmsg::remove)) remove = i;
        ImGui::EndDisabled();

        ImGui::PopID();
    }
    ImGui::EndTable();

    if (remove >= 0) {
        _batch.erase(_batch.begin() + remove);
        _batch_dirty = true;
        _batch_checked = false;
    }
}

void GuiApp::draw_batch_preset_combo(BatchJob& job, int row) {
    const std::string preview = job.preset_path.empty()
                                    ? preset_label(job.preset_name)
                                    : job.preset_name;
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool open = ui::BeginComboRaw("##preset", preview.c_str());
    // On the closed combo too: a row shows only a name, and two saved presets
    // can share one -- the path in the tooltip is what tells them apart.
    // draw_batch() keeps _presets warm, so the description is there as well.
    std::string desc;
    if (job.preset_path.empty()) {
        desc = preset_help(job.preset_name);
    } else {
        for (const TrainPreset& p : _presets)
            if (p.path == job.preset_path) { desc = p.description; break; }
    }
    preset_hover(desc, job.preset_path);
    if (!open) return;
    refresh_presets();

    ui::SeparatorText(msg::preset_builtin_group);
    for (const auto& p : kTrainPresets) {
        bool sel = job.preset_path.empty() && job.preset_name == p.name;
        if (ui::SelectableRaw(preset_label(p.name), sel)) {
            job.preset_path.clear();
            job.preset_name = p.name;
            _batch_dirty = true;
            _batch_checked = false;
        }
        preset_hover(preset_help(p.name), {});
        if (sel) ImGui::SetItemDefaultFocus();
    }

    ui::SeparatorText(msg::preset_user_group);
    if (_presets.empty()) ui::TextDisabled(msg::preset_none_saved);
    for (const auto& p : _presets) {
        ImGui::PushID(p.path.c_str());
        bool sel = job.preset_path == p.path;
        if (ui::SelectableRaw(p.name, sel)) {
            job.preset_path = p.path;
            job.preset_name = p.name;
            _batch_dirty = true;
            _batch_checked = false;
        }
        preset_hover(p);
        if (sel) ImGui::SetItemDefaultFocus();
        ImGui::PopID();
    }

    ImGui::Separator();
    if (ui::Selectable(msg::batch_preset_from_file)) {
        _pick = PickAction::BatchPresetFile;
        _pick_row = row;
        _dialog.open(msg::preset_pick_file.get(), FileDialog::Mode::File,
                     {".json"}, preset_dir());
    }
    ImGui::EndCombo();
}

void GuiApp::draw_batch_issues() {
    for (int i = 0; i < (int)_batch.size(); i++) {
        for (const BatchIssue& issue : _batch[i].issues) {
            ui::TextColoredWrappedRaw(
                issue.fatal ? kErr : kWarn,
                i18n::format(msg::batch_issue_row,
                             {(long long)(i + 1), batch_issue_line(issue)}));
        }
    }
}


void GuiApp::draw_train_settings() {
    TrainRunner::Phase ph = _runner.phase();
    bool busy = ph == TrainRunner::Phase::Loading ||
                ph == TrainRunner::Phase::Preparing ||
                ph == TrainRunner::Phase::Training;

    // ---- dataset ----
    ui::SeparatorText(msg::section_dataset);
    switch (ph) {
        case TrainRunner::Phase::Loading:
            ui::TextColored(kDim, msg::parsing_dataset);
            break;
        case TrainRunner::Phase::LoadError:
            // Parser errors name files and formats; they stay English so that
            // searching for one finds the same text everyone else saw.
            ui::TextColoredWrappedRaw(kErr, _runner.error());
            break;
        default:
            if (auto* s = _runner.session()) {
                if (ph == TrainRunner::Phase::Ready ||
                    ph == TrainRunner::Phase::Training ||
                    ph == TrainRunner::Phase::Done) {
                    ui::Text(msg::dataset_summary,
                             {(long long)s->ds.num_cameras,
                              (long long)s->post.n_post,
                              format_count((double)s->ds.points.num())});
                }
            } else {
                ui::TextColored(kDim, msg::no_dataset_loaded);
            }
            break;
    }
    ImGui::BeginDisabled(busy && ph != TrainRunner::Phase::Loading);
    if (ui::Button(msg::change_dataset)) {
        _pick = PickAction::OpenDataset;
        _dialog.open(msg::menu_open_dataset.get(), FileDialog::Mode::Folder);
    }
    ImGui::EndDisabled();

    // ---- device ----
    ui::SeparatorText(msg::section_device);
    {
        int n_dev = backend::device_count();
        int cur = backend::device_current();
        backend::DeviceInfo curd = backend::device_info(cur);
        ImGui::BeginDisabled(_device_locked || busy || n_dev == 0);
        ImGui::SetNextItemWidth(px(-8.0f));
        // Device names come from the driver; only the "none found" and
        // "unsupported" notes are ours.
        if (ui::BeginComboRaw("##device",
                              cur >= 0 ? curd.name : msg::no_device_found.get())) {
            for (int i = 0; i < n_dev; i++) {
                backend::DeviceInfo d = backend::device_info(i);
                char label[324];
                std::snprintf(label, sizeof(label), "%s (%s, %llu MB)%s##d%d",
                              d.name, d.type,
                              (unsigned long long)(d.vram_bytes >> 20),
                              d.usable ? "" : msg::device_unsupported.get(), i);
                ImGui::BeginDisabled(!d.usable);
                bool sel = i == cur;
                if (ui::SelectableRaw(label, sel)) backend::device_select(i);
                if (sel) ImGui::SetItemDefaultFocus();
                ImGui::EndDisabled();
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        if (_device_locked)
            ui::TextColoredWrapped(kDim, msg::device_locked);
    }

    // ---- preset + options ----
    // A batch owns the config while it runs -- each row's comes from its own
    // preset -- so the editor would be showing something that is not what is
    // training. What the row IS training goes here instead.
    if (_batch_active) {
        draw_batch_progress();
    } else {
        // Snapshot: any change to a dataset-parsing option below triggers an
        // automatic reload (deferred until the edited widget loses focus).
        TrainConfig parse_before = _cfg;
        ImGui::BeginDisabled(busy);
        ui::SeparatorText(msg::section_preset);
        draw_preset_picker();

        ui::SeparatorText(msg::section_basic_options);
        draw_basic_options();

        ImGui::Spacing();
        if (ui::CollapsingHeader(msg::section_all_options))
            draw_config_editor(_cfg, _defaults, _cfg_ui);
        ImGui::EndDisabled();

        // The macro options (quality, floater_suppression, ...) fill in the
        // flags they stand for, skipping any the user has edited by hand -- so
        // the two panels above always show the values the run will actually
        // use. None of what they write is a dataset-parsing field, so this
        // cannot make the snapshot below think the dataset went stale.
        train_resolve_macros(_cfg, _cfg_ui.touched);

        if (!parse_settings_equal(parse_before, _cfg)) _parse_dirty = true;
    }

    // ---- controls + metrics ----
    ui::SeparatorText(msg::section_training);
    draw_train_controls();
    draw_metrics();
}

// The preset picker: the built-in presets and the user's saved ones in one
// dropdown, plus the two buttons that move settings between the screen and a
// file. The built-in NAME is the command line's word for it and is not
// translated -- what the picker shows is the label from i18n/catalog/Train.h
// with the name kept alongside, so a user who has read the README still
// recognises the row they want. A saved preset shows the name its author gave
// it, which is the only name it has.
void GuiApp::draw_preset_picker() {
    static_assert(sizeof(kTrainPresets) / sizeof(kTrainPresets[0]) ==
                      i18n::msg::train::kNumPresetText,
                  "config/TrainConfig.h and i18n/catalog/Train.h disagree "
                  "about how many presets there are");

    const std::string preview =
        _preset_file.empty() ? preset_label(_preset) : _preset_display;
    ImGui::SetNextItemWidth(px(-8.0f));
    const bool combo_open = ui::BeginComboRaw("##preset", preview.c_str());
    // On the closed combo too: two saved presets can share a name, and this
    // is where the one in use says which file it is.
    preset_hover(_preset_file.empty() ? preset_help(_preset) : _preset_desc,
                 _preset_file);
    if (combo_open) {
        refresh_presets();
        ui::SeparatorText(msg::preset_builtin_group);
        for (const auto& p : kTrainPresets) {
            bool sel = _preset_file.empty() && _preset == p.name;
            if (ui::SelectableRaw(preset_label(p.name), sel))
                apply_preset(p.name);
            preset_hover(preset_help(p.name), {});
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ui::SeparatorText(msg::preset_user_group);
        if (_presets.empty()) ui::TextDisabled(msg::preset_none_saved);
        for (const auto& p : _presets) {
            ImGui::PushID(p.path.c_str());
            bool sel = _preset_file == p.path;
            if (ui::SelectableRaw(p.name, sel)) apply_user_preset(p);
            preset_hover(p);
            if (sel) ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ui::TextColoredWrappedRaw(
        kDim, _preset_file.empty() ? preset_help(_preset) : _preset_desc);

    if (ui::Button(msg::preset_save)) {
        // Seed the dialog from what is on screen: a saved preset being
        // adjusted usually wants to be saved back over itself.
        _preset_save_name = _preset_display;
        _preset_save_desc = _preset_desc;
        _preset_save_path = _preset_file;
        _preset_path_edited = !_preset_file.empty();
        _preset_save_open = true;
    }
    ui::help_on_hover(msg::preset_save_help);
    ImGui::SameLine();
    if (ui::Button(msg::preset_load)) {
        _pick = PickAction::PresetFile;
        _dialog.open(msg::preset_pick_file.get(), FileDialog::Mode::File,
                     {".json"}, preset_dir());
    }
    ui::help_on_hover(msg::preset_load_help);

    // Only for a saved preset: there is nothing to delete for a built-in one.
    if (!_preset_file.empty()) {
        ImGui::SameLine();
        if (ui::Button(msg::preset_delete)) _preset_delete_open = true;
        ui::help_on_hover(msg::preset_delete_help);
    }

    if (!_preset_msg.empty())
        ui::TextColoredWrappedRaw(_preset_msg_err ? kErr : kOk, _preset_msg);
    else
        ui::TextColoredWrapped(kDim, msg::preset_drop_hint);
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
    ImGui::PushTextWrapPos(px(460.0f));
    ui::Text(msg::preset_delete_confirm, {_preset_display});
    ui::TextDisabledRaw(_preset_file);
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    if (ui::Button(msg::preset_delete_button, ImVec2(150, 0))) {
        const std::string path = _preset_file, name = _preset_display;
        try {
            delete_preset(path);
            // The options on screen stay exactly as they are -- what went is
            // the saved copy, not the work. The picker falls back to naming
            // the built-in this config descends from, the same thing it shows
            // once any field has been edited by hand.
            _preset_file.clear();
            _preset_display.clear();
            _preset_desc.clear();
            _preset_msg = i18n::format(msg::preset_deleted, {name});
            _preset_msg_err = false;
            _presets_scanned_at = -1.0;
            refresh_presets();
            // Rows pointing at the file that just went would fail at launch;
            // the next check says so instead.
            _batch_checked = false;
        } catch (const std::exception& e) {
            _preset_msg = i18n::format(msg::preset_delete_failed, {e.what()});
            _preset_msg_err = true;
        }
        log(_preset_msg);
        _preset_delete_shown = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ui::Button(msg::cancel, ImVec2(150, 0))) {
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

    ImGui::SetNextWindowSize(ImVec2(540, 0), ImGuiCond_Appearing);
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
        _preset_save_path =
            (fs::path(preset_dir()) / preset_file_name(_preset_save_name))
                .string();

    ImGui::Spacing();
    ImGui::SetNextItemWidth(fw);
    // A path is a path in every language.
    if (ui::InputTextRaw("##preset_path", &_preset_save_path))
        _preset_path_edited = true;
    ImGui::SameLine();
    if (ui::ButtonRaw("...##preset_dir")) {
        _pick = PickAction::PresetSaveFolder;
        _preset_path_edited = true;
        _dialog.open(msg::preset_pick_folder.get(), FileDialog::Mode::Folder,
                     {}, fs::path(_preset_save_path).parent_path().string());
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
                   ImVec2(150, 0))) {
        TrainPreset p;
        p.name = _preset_save_name;
        p.description = _preset_save_desc;
        p.base = _preset;
        p.cfg = _cfg;
        p.touched = _cfg_ui.touched;
        try {
            save_preset(p, _preset_save_path);
            p.path = _preset_save_path;
            // Saving is also selecting: the screen now shows this preset.
            _preset_file = p.path;
            _preset_display = p.name;
            _preset_desc = p.description;
            _defaults = _cfg;
            _preset_msg = i18n::format(msg::preset_saved, {_preset_save_path});
            _preset_msg_err = false;
            _presets_scanned_at = -1.0;   // the new file must show up at once
            refresh_presets();
        } catch (const std::exception& e) {
            _preset_msg = i18n::format(msg::preset_failed, {e.what()});
            _preset_msg_err = true;
        }
        log(_preset_msg);
        _preset_save_shown = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ui::Button(msg::cancel, ImVec2(150, 0))) {
        _preset_save_shown = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

// What the trainer screen's left panel says while a batch owns it: which row
// is running, out of how many, and the two ways to stop.
void GuiApp::draw_batch_progress() {
    ui::SeparatorText(msg::batch_title);
    const long long total = (long long)_batch.size();
    ui::Text(msg::batch_running_banner,
             {(long long)(_batch_current + 1), total});
    if (_batch_current >= 0 && _batch_current < (int)_batch.size()) {
        const BatchJob& j = _batch[_batch_current];
        ImGui::PushTextWrapPos();
        ui::TextDisabled(msg::batch_running_dataset, {j.dataset});
        ui::TextDisabled(msg::batch_running_preset, {j.preset_name});
        ImGui::PopTextWrapPos();
    }
    if (_batch_stop_after) ui::TextColored(kWarn, msg::batch_stopping);

    if (ui::Button(msg::batch_show_list, ImVec2(-8, 0)))
        _screen = Screen::Batch;
    ImGui::BeginDisabled(_batch_stop_after);
    if (ui::Button(msg::batch_stop_after, ImVec2(-8, 0)))
        _batch_stop_after = true;
    ui::help_on_hover(msg::batch_stop_after_help);
    ImGui::EndDisabled();
    if (ui::Button(msg::batch_stop_now, ImVec2(-8, 0))) {
        _batch_stop_after = true;
        _batch_stop_now = true;
        _runner.request_stop();
    }
    ui::help_on_hover(msg::batch_stop_now_help);
}

// Every edit here records itself in _cfg_ui.touched, the same way the
// generated editor does: a flag the user set by hand is off limits to the
// macro options (see train_resolve_macros()).
void GuiApp::draw_basic_options() {
    // Wide enough for the longest value any of these dropdowns offers, and
    // scaled: at 200% the labels are twice the size and the box is not.
    const float w = px(170.0f);

    // A macro option: one dropdown standing for the several flags
    // train_resolve_macros() fills in behind it. The value written is the
    // token the flag takes; what the user picks is the word for it.
    auto macro_option = [&](const char* key, std::string& value,
                            const Msg& label, const Msg& help,
                            std::initializer_list<const char*> values) {
        ImGui::SetNextItemWidth(w);
        const bool open = ui::BeginComboRaw(ui::detail::label(label),
                                            gui::choice_display(key, value).c_str());
        if (!open) { ui::help_on_hover(help); return; }
        for (const char* v : values) {
            bool sel = value == v;
            if (ui::SelectableRaw(gui::choice_display(key, v), sel)) {
                value = v;
                _cfg_ui.touched.insert(key);
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    };

    // Output location first -- the thing every new user looks for.
    ImGui::SetNextItemWidth(w);
    if (ui::InputTextRaw("##outdir", &_cfg.output_dir_prefix))
        _cfg_ui.touched.insert("output_dir_prefix");
    ImGui::SameLine();
    if (ui::ButtonRaw("...##outdir")) {
        _pick = PickAction::OutputPrefix;
        _dialog.open(msg::pick_output_folder.get(), FileDialog::Mode::Folder,
                     {}, _cfg.output_dir_prefix);
    }
    ImGui::SameLine();
    ui::Text(msg::opt_output_folder);
    ui::help_on_hover(msg::opt_output_folder_help);
    ImGui::SetNextItemWidth(w);
    if (ui::InputTextWithHint(msg::opt_run_name, msg::opt_run_name_hint,
                              &_cfg.output_dir_name))
        _cfg_ui.touched.insert("output_dir_name");
    ui::help_on_hover(msg::opt_run_name_help);
    {
        std::string run = _cfg.output_dir_name.empty()
            ? fs::path(_cfg.data).stem().string() + "_<time>"
            : _cfg.output_dir_name;
        ui::TextColoredWrappedRaw(
            kDim, "-> " + (fs::path(_cfg.output_dir_prefix) / run).string());
    }
    ImGui::Spacing();

    // Ahead of the two flags it sets, so the usual path is to pick a quality
    // and move on, and the numbers below are what that choice came to.
    macro_option("quality", _cfg.quality, fld::quality, fld::quality_help,
                 {"low", "medium", "high", "ultra"});

    ImGui::SetNextItemWidth(w);
    if (ui::InputInt(msg::opt_steps, &_cfg.num_iterations))
        _cfg_ui.touched.insert("num_iterations");
    ui::help_on_hover(msg::opt_steps_help);

    ImGui::SetNextItemWidth(w);
    if (ui::InputInt(msg::opt_max_splats, &_cfg.cap_max))
        _cfg_ui.touched.insert("cap_max");
    ui::help_on_hover(msg::opt_max_splats_help);

    {
        // Primitive names are the config's own values, written into
        // config.json and accepted by `--primitive`; they are not translated.
        static const char* prims[] = {"3dgs", "mip", "3dgut"};
        int pi = _cfg.primitive == "mip" ? 1 : _cfg.primitive == "3dgut" ? 2 : 0;
        ImGui::SetNextItemWidth(w);
        if (ui::ComboRaw(ui::detail::label(msg::opt_primitive), &pi, prims, 3)) {
            _cfg.primitive = prims[pi];
            _cfg_ui.touched.insert("primitive");
        }
        ui::help_on_hover(msg::opt_primitive_help);
    }

    int ds_idx = _cfg.train_resolution_divisor == 2.0f ? 1
               : _cfg.train_resolution_divisor == 4.0f ? 2
               : _cfg.train_resolution_divisor == 8.0f ? 3 : 0;
    {
        // "1/2" is a fraction, not a word; only "native" is translated.
        const char* items[] = {msg::opt_resolution_native.get(), "1/2", "1/4", "1/8"};
        ImGui::SetNextItemWidth(w);
        if (ui::ComboRaw(ui::detail::label(msg::opt_resolution), &ds_idx, items, 4)) {
            const float vals[] = {0.0f, 2.0f, 4.0f, 8.0f};
            _cfg.train_resolution_divisor = vals[ds_idx];
            _cfg_ui.touched.insert("train_resolution_divisor");
        }
    }
    ui::help_on_hover(msg::opt_resolution_help);

    {
        int mi = !_cfg.load_masks ? 2 : _cfg.apply_loss_for_mask ? 1 : 0;
        ImGui::SetNextItemWidth(w);
        if (ui::Combo(msg::opt_mask_mode, &mi,
                      {&msg::opt_mask_mode_exclude,
                       &msg::opt_mask_mode_cut_out,
                       &msg::opt_mask_mode_off})) {
            _cfg.load_masks = mi != 2;
            _cfg.apply_loss_for_mask = mi == 1;
            _cfg_ui.touched.insert("load_masks");
            _cfg_ui.touched.insert("apply_loss_for_mask");
        }
        ui::help_on_hover(msg::opt_mask_mode_help);
    }

    ImGui::SetNextItemWidth(w);
    if (ui::SliderInt(msg::opt_sh_degree, &_cfg.sh_degree, 0, 4))
        _cfg_ui.touched.insert("sh_degree");
    ui::help_on_hover(msg::opt_sh_degree_help);

    if (ui::Checkbox(msg::opt_bilateral_grid, &_cfg.use_bilateral_grid))
        _cfg_ui.touched.insert("use_bilateral_grid");
    ui::help_on_hover(msg::opt_bilateral_grid_help);

    if (ui::Checkbox(msg::opt_ppisp, &_cfg.use_ppisp))
        _cfg_ui.touched.insert("use_ppisp");
    ui::help_on_hover(msg::opt_ppisp_help);

    ImGui::Spacing();

    // The two "if the capture went wrong" dials, last because a first run
    // should leave them alone and come back to them only if the result has
    // the problem they name.
    macro_option("floater_suppression", _cfg.floater_suppression,
                 fld::floater_suppression, fld::floater_suppression_help,
                 {"off", "mild", "strong"});
    macro_option("distraction_robustness", _cfg.distraction_robustness,
                 fld::distraction_robustness, fld::distraction_robustness_help,
                 {"off", "mild", "strong"});
}

void GuiApp::draw_train_controls() {
    TrainRunner::Phase ph = _runner.phase();
    switch (ph) {
        case TrainRunner::Phase::Idle:
        case TrainRunner::Phase::Ready:
        case TrainRunner::Phase::LoadError:
        case TrainRunner::Phase::Done:
        case TrainRunner::Phase::TrainError: {
            if (ph == TrainRunner::Phase::TrainError)
                ui::TextColoredWrappedRaw(kErr, _runner.error());
            if (ph == TrainRunner::Phase::Done) {
                const bool saved = _runner.saved_on_stop();
                ui::TextColored(saved ? kOk : kWarn,
                                saved ? msg::training_complete
                                      : msg::training_stopped_unsaved);
                if (auto* s = _runner.session()) {
                    ImGui::PushTextWrapPos();
                    ui::TextDisabled(saved ? msg::saved_to : msg::unsaved_note,
                                     {s->out_dir.string()});
                    ImGui::PopTextWrapPos();
                }
            }
            bool can_start = ph == TrainRunner::Phase::Ready ||
                             ph == TrainRunner::Phase::Done ||
                             ph == TrainRunner::Phase::TrainError;
            ImGui::BeginDisabled(!can_start);
            if (ui::Button(ph == TrainRunner::Phase::Done ? msg::train_again
                                                          : msg::start_training,
                           ImVec2(-8, 36)))
                start_training();
            ImGui::EndDisabled();
            break;
        }
        case TrainRunner::Phase::Loading:
            ui::TextColored(kDim, msg::parsing_dataset);
            break;
        case TrainRunner::Phase::Preparing:
            ui::TextColored(kDim, msg::preparing_engine);
            break;
        case TrainRunner::Phase::Training: {
            bool paused = _runner.paused();
            // While a batch runs, stopping is the batch panel's business --
            // "stop and save" here would end one job and silently start the
            // next, which is not what anybody pressing it means.
            float half = _batch_active
                             ? -8.0f
                             : (ImGui::GetContentRegionAvail().x - 12) * 0.5f;
            if (ui::Button(paused ? msg::resume : msg::pause, ImVec2(half, 32)))
                _runner.set_paused(!paused);
            if (_batch_active) break;
            ImGui::SameLine();
            bool stopping = _runner.session() &&
                            _runner.session()->stop_requested.load();
            ImGui::BeginDisabled(stopping);
            // Asks first: whether the run's last minutes reach the disk is
            // the one thing about stopping that cannot be undone afterwards.
            if (ui::Button(stopping ? msg::stopping : msg::stop,
                           ImVec2(half, 32))) {
                _pending = Pending::StopHere;
                _open_confirm = true;
            }
            ImGui::EndDisabled();
            ui::help_on_hover(msg::stop_help);
            break;
        }
    }
}

void GuiApp::draw_metrics() {
    static std::vector<TrainRunner::MetricPoint> pts;
    _runner.get_metrics(pts);
    if (pts.empty()) return;

    ui::SeparatorText(msg::section_metrics);
    // Downsample to <= 240 plot points.
    int stride = std::max<size_t>(1, pts.size() / 240);
    static std::vector<float> psnr, splats;
    psnr.clear();
    splats.clear();
    for (size_t i = 0; i < pts.size(); i += stride) {
        psnr.push_back(pts[i].psnr);
        splats.push_back(pts[i].num_splats);
    }
    const auto& last = pts.back();
    // PSNR / SSIM / loss are the metric names the literature and the logs use;
    // they are not translated, only the numbers change.
    ui::PlotLinesRaw("##psnr", psnr.data(), (int)psnr.size(), nullptr,
                     ImVec2(-8, px(64.0f)));
    // The score, placed in the plot's own rectangle rather than handed to
    // PlotLines as its overlay: that one is drawn against the TOP of the
    // frame, and the leading blank lines that used to push it down vanished
    // whenever a larger interface size made them taller than the frame.
    {
        char v[32];
        std::snprintf(v, sizeof v, "PSNR %.2f", last.psnr);
        const ImVec2 lo = ImGui::GetItemRectMin();
        const ImVec2 hi = ImGui::GetItemRectMax();
        const ImVec2 sz = ImGui::CalcTextSize(v);
        const float pad = ImGui::GetStyle().FramePadding.y;
        const ImVec2 at(std::max(lo.x, (lo.x + hi.x - sz.x) * 0.5f),
                        std::max(lo.y, hi.y - sz.y - pad));
        ImGui::GetWindowDrawList()->AddText(
            at, ImGui::GetColorU32(ImGuiCol_Text), v);
    }
    char line[96];
    std::snprintf(line, sizeof line, "%.3f", last.ssim);
    char loss[96];
    std::snprintf(loss, sizeof loss, "%.4f", last.rgb_loss);
    ui::TextWrapped(msg::status_metrics,
                    {format_count(last.num_splats), line, loss});
}

void GuiApp::draw_status_strip() {
    // Strip content region, captured before any widget consumes it -- used to
    // right-align the VRAM readout on the second (text) row.
    const float x0 = ImGui::GetCursorPosX();
    const float avail = ImGui::GetContentRegionAvail().x;

    TrainRunner::Phase ph = _runner.phase();
    spirula::TrainerProgress p = _runner.latest_progress();
    if (ph == TrainRunner::Phase::Training && p.total_steps > 0) {
        float frac = (float)(p.step + 1) / (float)p.total_steps;
        ui::ProgressBar(frac, ImVec2(-8, 0), msg::status_step,
                        {p.step + 1, p.total_steps, (int)(frac * 100.0f)});
        const double avg = _runner.avg_step_latency();
        char ms[32];
        std::snprintf(ms, sizeof ms, "%.0f",
                      (avg >= 0.0 ? avg : p.step_latency) * 1000.0);
        ui::Text(_runner.paused() ? msg::status_rate_paused : msg::status_rate,
                 {ms, format_duration(_runner.elapsed_seconds()),
                  format_duration(_runner.eta_seconds()),
                  format_count((double)p.num_splats)});
    } else if (ph == TrainRunner::Phase::Done && p.total_steps > 0) {
        ui::ProgressBar(1.0f, ImVec2(-8, 0), msg::status_done_steps,
                        {p.step + 1, format_duration(_runner.elapsed_seconds())});
        ui::TextDisabled(msg::status_explore);
    } else if (ph == TrainRunner::Phase::Preparing) {
        ui::ProgressBar(-(float)ImGui::GetTime(), ImVec2(-8, 0),
                        msg::status_preparing);
        ui::TextDisabledRaw(" ");
    } else if (ph == TrainRunner::Phase::Ready) {
        ui::ProgressBar(0.0f, ImVec2(-8, 0), msg::status_ready);
        ui::TextDisabled(msg::status_ready_hint);
    } else {
        ui::ProgressBar(0.0f, ImVec2(-8, 0), msg::status_idle);
        ui::TextDisabledRaw(" ");
    }

    draw_vram_readout(x0, avail);
}

void GuiApp::draw_vram_readout(float x0, float avail) {
    // Poll the backend at ~2 Hz; the driver queries are cheap but there is no
    // reason to hit them every frame.
    double now = ImGui::GetTime();
    if (_vram_polled_at < 0.0 || now - _vram_polled_at > 0.5) {
        _vram = backend::memory_usage();
        _vram_polled_at = now;
    }
    const backend::MemoryUsage& m = _vram;

    // Nothing queryable (no device / all queries failed): stay silent rather
    // than show a confusing "n/a".
    if (!m.has_process && !m.has_used && !m.has_total) return;

    auto part = [](bool has, uint64_t bytes) {
        return has ? format_gib(bytes) : std::string("?");
    };

    // A bar, because what matters here is a proportion: how close the device
    // is to full, and how much of that is this program rather than everything
    // else on the card. Three numbers in a row said neither without arithmetic.
    const bool sized = m.has_total && m.total_bytes > 0;
    const double total = sized ? (double)m.total_bytes : 0.0;
    const double used = m.has_used ? (double)m.used_bytes : (double)m.process_bytes;
    const double proc = m.has_process ? (double)m.process_bytes : 0.0;
    const float used_f = sized ? (float)std::min(used / total, 1.0) : 0.0f;
    const float proc_f = sized ? (float)std::min(proc / total, (double)used_f) : 0.0f;

    // Pressure is a property of the whole device, so the fill colour follows
    // system-wide use even though the bright segment is this process's share.
    ImVec4 color = kDim;
    if (sized && m.has_used)
        color = used_f >= 0.9f ? kErr : used_f >= 0.7f ? kWarn : kOk;

    // The same three numbers vram_help names, in that order: what this run
    // costs is the one a user is deciding on, and it is not recoverable from
    // the other two.
    const std::string label =
        sized ? part(m.has_process, m.process_bytes) + " / " +
                    part(m.has_used, m.used_bytes) + " / " +
                    format_gib(m.total_bytes) + " GiB"
              : "VRAM " + part(m.has_process, m.process_bytes) + " GiB";

    const ImGuiStyle& st = ImGui::GetStyle();
    const float text_w = ImGui::CalcTextSize(label.c_str()).x;
    ImGui::SameLine();
    // The bar is the first thing to give when the row is short -- the numbers
    // beside it say everything it does. Without this the readout ran off the
    // right edge of a narrow panel, or of any panel at a large interface size.
    float bar_w = sized ? px(120.0f) : 0.0f;
    float gap = sized ? st.ItemInnerSpacing.x : 0.0f;
    float target = x0 + avail - bar_w - gap - text_w - px(8.0f);
    if (target <= ImGui::GetCursorPosX()) {
        bar_w = gap = 0.0f;
        target = x0 + avail - text_w - px(8.0f);
    }
    if (target > ImGui::GetCursorPosX()) ImGui::SetCursorPosX(target);

    if (bar_w > 0.0f) {
        const float h = ImGui::GetTextLineHeight();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float r = st.FrameRounding;
        dl->AddRectFilled(p, ImVec2(p.x + bar_w, p.y + h),
                          ImGui::GetColorU32(ImGuiCol_FrameBg), r);
        // Everything in use, dim; this process's share of it, solid on top.
        ImVec4 rest = color;
        rest.w = 0.35f;
        if (used_f > 0.0f)
            dl->AddRectFilled(p, ImVec2(p.x + bar_w * used_f, p.y + h),
                              ImGui::GetColorU32(rest), r);
        if (proc_f > 0.0f)
            dl->AddRectFilled(p, ImVec2(p.x + bar_w * proc_f, p.y + h),
                              ImGui::GetColorU32(color), r);
        dl->AddRect(p, ImVec2(p.x + bar_w, p.y + h),
                    ImGui::GetColorU32(ImGuiCol_Border), r);
        ui::InvisibleButtonRaw("##vram", ImVec2(bar_w, h));
        ui::help_on_hover(msg::vram_help);
        ImGui::SameLine(0.0f, gap);
    }
    ui::TextColoredRaw(sized ? kDim : color, label);
    ui::help_on_hover(msg::vram_help);
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

float GuiApp::log_height(float avail) const {
    if (!_show_log) return 0.0f;
    const float line = ImGui::GetTextLineHeightWithSpacing();
    const float pad = ImGui::GetStyle().WindowPadding.y * 2.0f;
    float h = std::max(_log_h * ui_scale(), 4.0f * line + pad);
    // The cap wins over the floor: on a window too short for both, a log that
    // took the four lines it wants would leave the screen above it with none.
    return std::max(0.0f, std::min(h, avail * 0.60f));
}

float GuiApp::body_height(float log_h) {
    return log_h > 0.0f ? -(log_h + splitter_extent()) : 0.0f;
}

void GuiApp::draw_log_panel(float height) {
    if (height <= 0.0f) return;

    {
        const float line = ImGui::GetTextLineHeightWithSpacing();
        float h = height;
        if (splitter_h("##logsplit", &h, 3.0f * line,
                       std::max(3.0f * line, ImGui::GetWindowHeight() * 0.8f),
                       ImGui::GetContentRegionAvail().x)) {
            _log_h = h / ui_scale();
            _layout_dirty = true;
        }
    }

    ImGui::BeginChild("##log", ImVec2(0, height), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar);

    // Reading back where the view sits is the whole state: pinned to the
    // bottom means "follow", and a wheel-up on the previous frame has already
    // moved it by the time this runs.
    _log_follow = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f;
    if (_log_scroll_end) {
        _log_scroll_end = false;
        _log_follow = true;
    }
    if (_log_dropped) {
        if (!_log_follow)
            ImGui::SetScrollY(std::max(0.0f,
                ImGui::GetScrollY() -
                    (float)_log_dropped * ImGui::GetTextLineHeightWithSpacing()));
        _log_dropped = 0;
    }

    // A dataset run says far more than it means. The default view is the lines
    // that answer "what happened"; everything else is behind Details, which is
    // where a bug report is copied from.
    if (_log_shown_dirty) {
        _log_shown.clear();
        for (size_t i = 0; i < _log.size(); i++)
            if (_log_details || !_log[i].detail) _log_shown.push_back(i);
        _log_shown_dirty = false;
    }

    // Log lines arrive already formatted, from here and from the engine, one
    // entry per screen line -- which is what lets the clipper skip the 4000
    // that are not visible.
    ImGuiListClipper clip;
    clip.Begin((int)_log_shown.size());
    while (clip.Step())
        for (int i = clip.DisplayStart; i < clip.DisplayEnd; i++)
            ui::TextRaw(_log[_log_shown[(size_t)i]].text);
    clip.End();
    if (_log_follow) ImGui::SetScrollHereY(1.0f);

    if (ImGui::BeginPopupContextWindow()) {
        if (ui::MenuItem(msg::log_follow, nullptr, _log_follow))
            _log_scroll_end = true;
        if (ui::MenuItem(msg::log_details, nullptr, _log_details)) {
            _log_details = !_log_details;
            _log_shown_dirty = true;
            _log_scroll_end = true;
            save_settings();
        }
        if (ui::MenuItem(msg::log_copy)) {
            // Everything, whichever view is up: a log copied for somebody else
            // to read is not the summary.
            std::string all;
            for (const auto& e : _log) { all += e.text; all += '\n'; }
            ImGui::SetClipboardText(all.c_str());
        }
        if (ui::MenuItem(msg::log_clear)) clear_log();
        ImGui::EndPopup();
    }
    ImGui::EndChild();

    // The way back, offered only when it is needed: scrolled off the bottom
    // with the run still writing. Drawn over the panel's bottom-right corner
    // so it costs no height.
    if (!_log_follow) {
        const ImVec2 corner = ImGui::GetItemRectMax();
        const float bw = ImGui::CalcTextSize(msg::log_jump.get()).x +
                         ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SetNextWindowPos(ImVec2(corner.x - bw - px(24.0f),
                                       corner.y - ImGui::GetFrameHeight() - px(8.0f)));
        if (ImGui::Begin("##logjump", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoBackground |
                         ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoFocusOnAppearing)) {
            if (ui::Button(msg::log_jump)) _log_scroll_end = true;
        }
        ImGui::End();
    }
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
        _confirm_shown = false;
        resume();
    }
}

}  // namespace gui

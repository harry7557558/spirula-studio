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
#include "data/SparseEdit.h"
#include "i18n/catalog/Edit.h"
#include "i18n/catalog/Gui.h"
#include "i18n/catalog/MaskEdit.h"
#include "i18n/catalog/Render.h"
#include "i18n/catalog/Train.h"
#include "i18n/catalog/TrainFields.h"

#include "app/gui/GlLoader.h"
#include "app_generated/app_banner.h"
#include "external/stb_image.h"

#include "core/Env.h"
#if defined(SS_BUILD_SAM) || defined(SS_TOOL_SFM) || defined(SS_BACKEND_VULKAN)
#include "core/VulkanDeviceSelection.h"
#endif

#include "imgui.h"
#include "imgui_stdlib.h"

// Side-effect-free native listing; it must not publish the engine selection.
#if defined(SS_BUILD_SAM)
#include "nn/Device.h"
#include "nn/vk/Context.h"
#elif defined(SS_TOOL_SFM)
#include "sfm/vk/VkContext.h"
#endif

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
namespace i18n = spirula::i18n;
namespace msg = spirula::i18n::msg::gui;
namespace emsg = spirula::i18n::msg::edit;
namespace fld = spirula::i18n::msg::field;
namespace dmsg = spirula::i18n::msg::dataset;
namespace gmsg = spirula::i18n::msg::geometry;
namespace tmsg = spirula::i18n::msg::train;
namespace rmsg = spirula::i18n::msg::render;
namespace mmsg = spirula::i18n::msg::maskedit;
using spirula::i18n::Msg;
using spirula::format_duration;

namespace gui {

namespace {

const ImVec4 kOk(0.35f, 0.85f, 0.45f, 1.0f);
const ImVec4 kErr(1.0f, 0.42f, 0.42f, 1.0f);
const ImVec4 kWarn(0.95f, 0.75f, 0.30f, 1.0f);
const ImVec4 kDim(0.6f, 0.6f, 0.6f, 1.0f);
#if defined(SS_BUILD_SAM) || defined(SS_TOOL_SFM) || defined(SS_BACKEND_VULKAN)
inline const Msg& device_detail(spirula::vkselect::ResolveStatus s) {
    switch (s) {
        case spirula::vkselect::ResolveStatus::Malformed:
            return msg::device_detail_malformed;
        case spirula::vkselect::ResolveStatus::OutOfRange:
            return msg::device_detail_out_of_range;
        case spirula::vkselect::ResolveStatus::Unusable:
            return msg::device_unusable_native;
        case spirula::vkselect::ResolveStatus::NoDevice:
            return msg::device_detail_no_device;
        case spirula::vkselect::ResolveStatus::Missing:
        case spirula::vkselect::ResolveStatus::Ok:
            break;
    }
    return msg::device_detail_missing;
}
#endif

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
                                                      ".glb", ".stl"};

// What "Open a Model or Reconstruction" shows. The extra four are how a
// reconstruction is named when the desktop's own picker cannot return a
// folder: transforms.json, cameras.bin, points3D.txt, a Metashape .xml.
const std::vector<std::string> kOpenableExtensions = {
    ".ply", ".obj", ".gltf", ".glb", ".stl", ".json", ".bin", ".txt", ".xml"};

// Can this format carry this color? The child's own answer, asked through the
// same function it refuses the run with.
bool mesh_format_carries(int format, int color) {
    meshing::MeshFormatSpec spec;
    spec.fmt = kMeshFormats[format];
    return meshing::check_export_support(spec, (meshing::MeshColorMode)color)
        .empty();
}

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
        open_pick(PickAction::AddSplatFile, msg::viewer_pick_file.get(),
                  FileDialog::Mode::FileOrFolder, kOpenableExtensions);
    });
    _compare.set_siblings_of([this](const std::string& path) {
        std::vector<std::string> outs = _mesh.output_paths();
        for (const std::string& o : outs)
            if (o == path) return outs;
        return std::vector<std::string>{};
    });
    _compare.edit().set_to_trainer(
        [this](const std::string& dataset, const std::string& source) {
            _edit_train_dataset = dataset;
            _edit_train_source = source;
        });
    _compare.edit().set_pick_save(
        [this](int target, const std::string& ext, bool folder,
               const std::string& suggested) {
            _edit_save_target = target;
            open_pick(folder ? PickAction::EditSaveFolder
                             : PickAction::EditSaveFile,
                      emsg::save_pick_title.get(),
                      folder ? FileDialog::Mode::Folder : FileDialog::Mode::Save,
                      ext.empty() ? std::vector<std::string>{}
                                  : std::vector<std::string>{ext},
                      "", false, suggested);
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
    stop_inference_users();
    _segment.destroy_gl();
    // Same ordering as _compare above: destroy_gl while GL is still current.
    _mask_editor.destroy_gl();
    _mask_editor.close();
    _mask_editor.sam_drain_retiring();   // before nn::shutdown() frees the device
    _geometry_panel.destroy_gl();
    _colmap.cancel();
    _sfm.cancel();
    reset_dataset_preview();
    _source_probe.stop();
    _download.cancel();
    _geom_download.cancel();
    _feat_download.cancel();
    _font_download.cancel();
    _runner.shutdown();
}

// One line per remembered pick directory, keyed by dir_key().
static constexpr char kDirPrefix[] = "dialog_dir.";

// gui.conf is one setting per line, so a setting that may HOLD line breaks --
// the batch's finish command -- is written with them escaped. An escape that
// means nothing is left alone, so a hand-typed `C:\Users` still reads back.
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

std::string GuiApp::settings_path() {
    return (fs::path(app::config_dir()) / "gui.conf").string();
}

void GuiApp::load_settings() {
    // Presets have a folder of their own; every other kind of pick starts at
    // the home directory until one of that kind has been made.
    _dialog_dirs["preset"] = preset_dir(PresetKind::Train);

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
        else if (k == "batch_command") _batch_cmd = unescape_setting(v);
        else if (k == "accepted_license" && !v.empty() && !license_accepted(v))
            _accepted_licenses.push_back(v);
        else if (k == "lang" && !v.empty()) saved_lang = v;
        else if (k == "ui_scale") _scale.set_user(std::clamp((float)atof(v.c_str()),
                                                             0.0f, 3.0f));
        else if (k == "panel_w") _panel_w = (float)atof(v.c_str());
        else if (k == "ds_panel_w") _ds_panel_w = (float)atof(v.c_str());
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
    }
    std::fclose(f);

    // A stored extent from a build whose defaults have moved, or a hand-edited
    // file, must not be able to leave a panel unreachably small or wide.
    _panel_w = std::clamp(_panel_w, 220.0f, 900.0f);
    _log_h = std::clamp(_log_h, 60.0f, 800.0f);
    _preview_h = std::clamp(_preview_h, 60.0f, 800.0f);
    _ds_panel_w = std::clamp(_ds_panel_w, 320.0f, 1200.0f);
    _edit_panel_w = std::clamp(_edit_panel_w, 220.0f, 900.0f);

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
    std::fprintf(f, "batch_command=%s\n", escape_setting(_batch_cmd).c_str());
    std::fprintf(f, "lang=%s\n", spirula::i18n::code(spirula::i18n::current()));
    std::fprintf(f, "ui_scale=%.3f\n", _scale.user());
    std::fprintf(f, "panel_w=%.1f\n", _panel_w);
    std::fprintf(f, "ds_panel_w=%.1f\n", _ds_panel_w);
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

// Local wall-clock stamp for log lines: [yyyy-MM-dd HH:mm:ss.fff].
static std::string log_stamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    const int ms = int(std::chrono::duration_cast<std::chrono::milliseconds>(
                           now.time_since_epoch()).count() % 1000);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char date[32], frac[16];
    std::strftime(date, sizeof(date), "[%Y-%m-%d %H:%M:%S", &tm);
    std::snprintf(frac, sizeof(frac), ".%03d] ", ms);
    return std::string(date) + frac;
}

// Run-start stamp for log file names: yyyyMMddHHmmss.
static std::string run_log_stamp() {
    const std::time_t t =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[24];
    std::strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", &tm);
    return buf;
}

// Open this run's log file: <dir>/logs/<kind>_<stamp>.log, where dir is the
// folder the images folder lives in. A run that cannot name that folder keeps
// its log on the panel only. Returns the file's path, empty when none.
static fs::path open_run_log(std::ofstream& f, const std::string& dataset_dir,
                             const char* kind, const std::string& stamp) {
    if (f.is_open()) f.close();
    f.clear();
    if (dataset_dir.empty()) return {};
    std::error_code ec;
    const fs::path dir = fs::path(dataset_dir) / "logs";
    fs::create_directories(dir, ec);
    if (ec) return {};
    const fs::path file = dir / (std::string(kind) + "_" + stamp + ".log");
    f.open(file, std::ios::out | std::ios::app);
    return file;
}

// Hand a value to the reconstruction engines through the process environment
// (the SS_* namespace core/Env.h owns); the SfM subprocess inherits it too.
// Empty clears.
static void set_ss_env(const char* suffix, const std::string& value) {
    const std::string name = std::string("SS_") + suffix;
#ifdef _WIN32
    _putenv_s(name.c_str(), value.c_str());
#else
    setenv(name.c_str(), value.c_str(), 1);
#endif
}

// Mirror a log() message into the run's file, stamped the same way.
static void file_log_line(std::ofstream& f, const std::string& s) {
    if (!f.is_open()) return;
    size_t at = 0;
    do {
        const size_t nl = s.find('\n', at);
        f << log_stamp()
          << s.substr(at, nl == std::string::npos ? nl : nl - at) << '\n';
        at = nl == std::string::npos ? nl : nl + 1;
    } while (at != std::string::npos);
    f.flush();
}

static std::string cached_model_path(const std::string& id) {
    const ModelEntry* e = find_model(id);
    if (!e || !model_is_cached(*e)) return "";
    return model_path(*e);
}

// Value formatting for the settings snapshot.
static std::string cfg_str(const std::string& v) { return v; }
static std::string cfg_str(bool v) { return v ? "true" : "false"; }
static std::string cfg_str(int v) { return std::to_string(v); }
static std::string cfg_str(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.7g", double(v));
    return buf;
}
template <typename T>
static std::string cfg_str(const std::optional<T>& v) {
    return v ? cfg_str(*v) : std::string("none");
}
template <typename T, size_t N>
static std::string cfg_str(const std::array<T, N>& v) {
    std::string s;
    for (size_t i = 0; i < N; i++) {
        if (i) s += ',';
        s += cfg_str(v[i]);
    }
    return s;
}

static const char* photo_import_name(PhotoImport m) {
    switch (m) {
        case PhotoImport::ConvertJpeg: return "convert-jpeg";
        case PhotoImport::Copy:        return "copy";
        case PhotoImport::Move:        return "move";
        case PhotoImport::InPlace:     return "in-place";
    }
    return "?";
}

// One entry per SCREEN line: the panel clips with a list clipper and
// compensates its scroll for trimmed lines, and both want a uniform height.
void GuiApp::log(const std::string& s, bool detail) {
    const std::string stamp = log_stamp();
    size_t at = 0;
    do {
        const size_t nl = s.find('\n', at);
        _log.push_back({stamp + s.substr(at, nl == std::string::npos ? nl : nl - at),
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

// The panel snapshot at the top of every run log: plain key=value lines in
// sections, no timestamps -- the timestamps start below the separator.
void GuiApp::write_run_settings(std::ofstream& f) {
    if (!f.is_open()) return;
    std::string out;
    auto section = [&out](const std::string& name) { out += "[" + name + "]\n"; };
    auto line = [&out](const std::string& k, const std::string& v) {
        out += "  " + k + " = " + v + "\n";
    };

    section(msg::runlog_section_run.get());
    line("run_started", run_log_stamp());
    line("engine", effective_engine() == Engine::BuiltIn ? "builtin" : "colmap");
#if defined(SS_BUILD_SAM) || defined(SS_TOOL_SFM) || defined(SS_BACKEND_VULKAN)
    line("native_device", _native_device_uuid.empty() ? "auto" : _native_device_uuid);
    if (!_native_device_name.empty()) line("native_device_name", _native_device_name);
#endif
#ifndef SS_BACKEND_VULKAN
    if (_cuda_device_index >= 0)
        line("cuda_device", std::to_string(_cuda_device_index));
#endif
    line("preset", _preset);
    if (!_train_presets.file.empty()) line("preset_file", _train_presets.file);
    if (!_ds_presets.file.empty()) line("dataset_preset_file", _ds_presets.file);

    section(msg::runlog_section_prep.get());
    line("workspace", _workspace);
    line("photo_import", photo_import_name(_photo_import));
    for (const PrepInput& s : _sources)
        line(s.is_video ? "video_source" : s.sequential ? "photo_source_in_order" : "photo_source",
             s.path);
    line("use_found_masks", cfg_str(_use_found_masks));
    line("flip_found_masks", cfg_str(_flip_found_masks));
    line("masking_enabled", cfg_str(_mask_enable));
    line("mask_features", cfg_str(_mask_features));
    line("mask_detect_every", std::to_string(_mask_detect_every));

    section(i18n::format(msg::runlog_section_recon, {"SfM"}));
    const SfmJob& j = _sfm_job;
    line("quality", std::to_string(j.quality));
    line("data_type", std::to_string(j.data_type));
    line("camera_model", j.camera_model);
    line("camera_mode", std::to_string(j.camera_mode));
    line("pairs", std::to_string(j.pairs));
    line("overlap", std::to_string(j.overlap));
    line("loop_closure", cfg_str(j.loop_closure));
    // line("prefilter_sequential", cfg_str(j.prefilter_sequential));
    line("use_sequence", cfg_str(j.use_sequence));
    line("features", std::to_string(j.features));
    line("matcher", std::to_string(j.matcher));
    line("mapper", std::to_string(j.mapper));
    line("init_focal_px", cfg_str(j.init_focal_px));
    if (!j.init_distortion.empty()) line("init_distortion", j.init_distortion);
    line("distortion_refine", std::to_string(j.distortion_refine));
    line("final_per_image_intrinsics", cfg_str(j.final_per_image_intrinsics));
    line("final_free_rig", cfg_str(j.final_free_rig));
    line("max_features", std::to_string(j.max_features));
    line("max_image_size", std::to_string(j.max_image_size));
    line("metric_gps", std::to_string(j.metric_gps));
    line("sensor_gauge", std::to_string(j.sensor_gauge));
    line("exif_attitude", std::to_string(j.exif_attitude));
    line("keep_intermediate", cfg_str(j.keep_intermediate));
    line("ba_cpu", cfg_str(j.ba_cpu));
    line("subprocess", cfg_str(j.subprocess));
    line("force_external_decode", cfg_str(j.prep.force_external_decode));
    line("frame_bits", std::to_string(j.prep.frame_bits));
    line("force_external_masking", cfg_str(j.prep.force_external_masking));
    line("video_fps", cfg_str(j.prep.video_fps));
    line("adaptive_fps", cfg_str(j.prep.adaptive_fps));
    if (j.prep.adaptive_fps) line("adaptive_range", cfg_str(j.prep.adaptive_range));
    for (const PrepInput& s : _sources)
        if (s.is_video && s.fps != 0.0f)
            line("video_fps:" + (s.subdir.empty() ? s.path : s.subdir),
                 cfg_str(std::max(s.fps, 0.0f)));
    line("sharp_window", std::to_string(j.prep.sharp_window));
    line("sync_tracks", cfg_str(j.prep.sync_tracks));
    line("max_frames", std::to_string(j.prep.max_frames));
    if (!j.extra_args.empty()) line("extra_args", j.extra_args);

    if (effective_engine() == Engine::Colmap) {
        section(i18n::format(msg::runlog_section_recon, {"COLMAP"}));
        line("colmap_exe", _colmap_job.colmap_exe);
        line("camera_model", _colmap_job.camera_model);
        line("camera_mode", std::to_string(_colmap_job.camera_mode));
    }

    section(msg::runlog_section_geometry.get());
    line("enabled", cfg_str(_geometry.enable));
    line("model", _geometry.model);
    line("want_depth", cfg_str(_geometry.want_depth));
    line("want_normal", cfg_str(_geometry.want_normal));
    line("max_size", std::to_string(_geometry.max_size));

    // The training config, grouped the way the options editor groups it.
    for (int si = 0; si < kTrainNumSections; si++) {
        const TrainConfig& c = _cfg;
        std::string body;
#define SS_SNAP_FIELD(type, member, default_, s, tier, choices)              \
        if (!std::strcmp(s, kTrainSections[si]))                             \
            body += std::string("  ") + #member + " = " +                    \
                    cfg_str(c.member) + "\n";
        SS_CONFIG_FIELDS(SS_SNAP_FIELD)
#undef SS_SNAP_FIELD
        if (!body.empty()) {
            const Msg* label = tmsg::section_label(kTrainSections[si]);
            section(i18n::format(
                msg::runlog_section_train,
                {label ? label->get() : kTrainSections[si]}));
            out += body;
        }
    }

    // A fixed-width rule, so the separator still reads as one whatever the
    // translated label measures.
    const std::string end = msg::runlog_settings_end.get();
    const int fill = std::max(72 - i18n::display_width(end) - 2, 6);
    out += std::string(fill / 2, '=') + " " + end + " " +
           std::string(fill - fill / 2, '=') + "\n\n";
    f << out;
    f.flush();
}

void GuiApp::append_logs() {
    for (auto& s : _runner.drain_log()) {
        file_log_line(_train_log, s);
        log(s);
    }
    for (auto& l : _colmap.steps().drain()) {
        file_log_line(_prep_log, l.text);
        log(l.text, l.detail);
    }
    for (auto& l : _sfm.steps().drain()) {
        file_log_line(_prep_log, l.text);
        log(l.text, l.detail);
    }
    for (auto& s : _compare.drain_log()) log(s);
    for (auto& s : _mesh.drain_log()) log(s);
    poll_batch_command();
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
    if (native_work_busy() || _batch_active) return;
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
    fresh.save_full_checkpoint = _keep_full_ckpt;
    _preset = preset;
    _train_presets.file.clear();
    _train_presets.display.clear();
    _train_presets.desc.clear();
    _train_presets.msg.clear();   // whatever it reported was about the last preset
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
    if (native_work_busy() || _batch_active) return;
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
    fresh.save_full_checkpoint = p.cfg.save_full_checkpoint || _keep_full_ckpt;

    _preset = p.base;
    _train_presets.file = p.path;
    _train_presets.display = p.name;
    _train_presets.desc = p.description;
    _train_presets.msg.clear();   // whatever it reported was about the last preset
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
    _use_found_masks = s.use_found_masks;
    _border_enable = s.border_enable;
    _frame_shapes = s.frame_shapes;
    apply_frame_shapes();
    _resume = s.sfm.prep.resume;
    _photo_import = s.sfm.prep.photo_import;
    _flip_found_masks = s.sfm.prep.flip_found_masks;
    _mask_enable = s.sfm.prep.mask_enable;
    _mask_memory = s.sfm.prep.mask_memory;
    _mask_detect_every = s.sfm.prep.mask_detect_every;
    _mask_memory_frames = s.sfm.prep.mask_memory_frames;
    _mask_features = s.sfm.mask_features;
    _geometry = s.sfm.geometry;
    // A colour space the preset spelled out is a decision, so the EXR probe
    // must not overwrite it later.
    _color_space_touched =
        !s.sfm.image_gamut.empty() || s.sfm.image_is_linear.has_value();
    resolve_source_lenses(_sources, _sfm_job, _colmap_job);
    // A preset answers "how", never "where": refresh_sources() re-derives the
    // output folder whenever the user has not taken it over, and a fresh one
    // beside the last would silently orphan the run already in it.
    const std::string ws = _workspace, ws_auto = _workspace_auto;
    rescan_found_masks();
    _workspace = ws;
    _workspace_auto = ws_auto;
    // These settings did not build whatever model is sitting in the output
    // folder, so its stamp no longer describes the screen.
    _built_workspace.clear();
}

void GuiApp::apply_dataset_preset(const DatasetPreset& p) {
    apply_dataset_settings(p.s);
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


void GuiApp::open_dataset(std::string dir, std::string image_dir,
                          std::string mask_dir, bool mask_flipped,
                          bool keep_log) {
    if (dir.empty() || native_work_busy()) return;
    app::set_crash_note("opening dataset " + dir);
    close_native_previews();
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
    // However it was opened -- picked, dropped, from the recents list -- this
    // is where the picker starts next time.
    remember_dir("dataset", dir);
    save_settings();
    detach_session_views();
    // Training is the end of the dataset screen's business with the run, so
    // this is where what it left for the screen to read goes.
    reset_dataset_preview();
    _runner.load_dataset(_cfg, _preset);
    _screen = Screen::Train;
}

void GuiApp::open_edited_dataset() {
    const std::string dataset = std::exchange(_edit_train_dataset, {});
    const std::string source = std::exchange(_edit_train_source, {});
    if (native_work_busy()) return;
    auto same = [](const std::string& a, const std::string& b) {
        std::error_code ec;
        return !a.empty() && !b.empty() && fs::equivalent(a, b, ec);
    };
    DatasetFolders from;
    if (same(source, _sparse_edit_src.dir) ||
        same(source, spirula::resolve_sparse_dir(_sparse_edit_src.dir)))
        from = _sparse_edit_src;
    else if (same(source, _cfg.data))
        from = {_cfg.data, _cfg.image_dir, _cfg.mask_dir, _cfg.flip_mask};
    // A copy sits elsewhere, so the source's folders are named absolutely;
    // the dataparser's own defaults are what an unnamed one means.
    if (!same(dataset, source)) {
        const TrainConfig stock;
        auto absolute = [&](std::string d, const std::string& fallback) {
            if (d.empty()) d = fallback;
            const fs::path p = fs::path(source) / d;
            std::error_code ec;
            return fs::is_directory(p, ec) ? p.lexically_normal().string() : std::string();
        };
        from.image_dir = absolute(from.image_dir, stock.image_dir);
        from.mask_dir = absolute(from.mask_dir, stock.mask_dir);
    }
    open_dataset(dataset, from.image_dir, from.mask_dir, from.mask_flipped);
}

void GuiApp::request_open_dataset(std::string dir) {
    const bool loading = _runner.phase() == TrainRunner::Phase::Loading;
    if (training_busy() || loading) {
        _pending = Pending::OpenDataset;
        _pending_path = dir;
        if (loading) {
            _stop_confirmed = true;
            _runner.request_stop();
        } else {
            _open_confirm = true;
        }
        return;
    }
    if (native_work_busy()) return;
    open_dataset(dir);
}

void GuiApp::request_go_home() {
    const bool loading = _runner.phase() == TrainRunner::Phase::Loading;
    if (training_busy() || loading) {
        _pending = Pending::GoHome;
        if (loading) {
            _stop_confirmed = true;
            _runner.request_stop();
        } else {
            _open_confirm = true;
        }
        return;
    }
    if (native_work_busy()) return;
    // The mesh preview holds the engine through _splat and two attached
    // viewports; leaving the screen by ANY route (the button, the menu, the
    // deferred confirmation) has to hand them back.
    close_native_previews();
    close_mesh_preview();
    close_splat();
    _screen = Screen::Home;
}

// A splat file, a step-*.ckpt directory, or a run directory. Loading is
// asynchronous (a large model is a few hundred MB), so this only starts it;
// frame() attaches the viewport once the splats are on the device.
void GuiApp::open_splat(std::string path) {
    if (path.empty() || native_work_busy()) return;
    clear_log();
    close_native_previews();
    close_splat();
#ifdef SS_BACKEND_VULKAN
    if (!freeze_native_device()) return;
#else
    if (!freeze_cuda_device()) return;
#endif
    app::set_crash_note("opening model " + path);
    close_mesh_preview();
    detach_session_views();
    _runner.note_engine_taken();
    _compare.open(path);
    if (_edit_after_open) {
        _compare.edit_first_when_ready();
        _edit_after_open = false;
    }
    if (_render_after_open) {
        _compare.render_first_when_ready();
        _render_after_open = false;
    }
    if (!_render_project_after_open.empty()) {
        _compare.render_project_when_ready(_render_project_after_open);
        _render_project_after_open.clear();
    }
    add_model_recent(path);
    remember_dir("model", path);
    save_settings();
    _screen = Screen::Viewer;
}

void GuiApp::add_splat(std::string path) {
    if (path.empty() || native_work_busy()) return;
    close_native_previews();
#ifdef SS_BACKEND_VULKAN
    if (!freeze_native_device()) return;
#else
    if (!freeze_cuda_device()) return;
#endif
    _compare.add(path);
    add_model_recent(path);
    remember_dir("model", path);
    save_settings();
}

void GuiApp::request_open_splat(std::string path) {
    if (training_busy()) {
        _pending = Pending::OpenSplat;
        _pending_path = std::move(path);
        _open_confirm = true;
        return;
    }
    if (native_work_busy()) {
        // The request is dropped, so the intent behind it goes with it: a
        // model opened later must not arrive in edit mode by surprise.
        _edit_after_open = false;
        _render_after_open = false;
        _render_project_after_open.clear();
        return;
    }
    open_splat(std::move(path));
}

// A camera project dropped or picked: its first model opens on the viewer
// screen, the render starts on it with the project, and the project opens
// whatever other models it names. False when the file is not a project.
bool GuiApp::open_render_project(const std::string& path) {
    gui::render::RenderProject p;
    try {
        p = gui::render::load_project(path);
    } catch (const std::exception&) {
        return false;
    }
    std::error_code ec;
    const std::string model = p.sources.empty() ? std::string() : p.sources[0].path;
    if (model.empty() || !fs::exists(fs::u8path(model), ec)) {
        log(i18n::format(rmsg::project_model_missing, {model.empty() ? path : model}));
        return true;
    }
    _render_project_after_open = path;
    request_open_splat(model);
    return true;
}

// The engine is a process-global singleton and the viewer is holding it, so
// this has to run before a training session can be set up. CompareView::close
// stops its render workers before handing the engine back.
void GuiApp::close_splat() {
    _compare.close();
    _mesh_preview_open = false;
}
void GuiApp::close_native_previews() {
    _segment.close();
    _geometry_panel.close();
}

// Every site that starts inference (or tears it down) calls this, never the
// bare close: the editor's SAM session shares the pool's slots and one
// unsynchronised stream, and a dataset run ends in nn::shutdown().
void GuiApp::stop_inference_users() {
    close_native_previews();
    _mask_editor.sam_yield();
}

void GuiApp::launch_training(const TrainConfig& cfg, const std::string& preset) {
    if (cfg.data.empty() || native_work_busy()) return;
    stop_inference_users();
    close_splat();
#ifdef SS_BACKEND_VULKAN
    if (!freeze_native_device()) return;
#else
    if (!freeze_cuda_device()) return;
#endif
    app::set_crash_note("training " + cfg.data);
    close_mesh_preview();
    detach_session_views();
    std::string data_dir = cfg.data;
    if (fs::path(data_dir).filename() == "images")
        data_dir = fs::path(data_dir).parent_path().string();
    open_run_log(_train_log, data_dir, "train", run_log_stamp());
    set_ss_env("UNREG_LOG", "");
    write_run_settings(_train_log);
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
    if (_compare.edit_dirty()) {
        _edit_exit_confirm = true;
        return;
    }
    if (!_render_discarded && _compare.render().dirty()) {
        _render_exit_confirm = true;
        return;
    }
    if (training_busy()) {
        _pending = Pending::Quit;
        _open_confirm = true;
        return;
    }
    _quit = true;
}

void GuiApp::run_pending_if_stopped() {
    if (_pending == Pending::None || native_work_busy()) return;
    // Only fire once the user confirmed the stop (or training was never
    // busy); a dismissed modal must not leave a delayed action armed.
    if (!_stop_confirmed) {
        _pending = Pending::None;
        _render_project_after_open.clear();
        return;
    }
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
// Batch processing
//
// The queue is BatchProcess.h; this drives the three live runners rather than
// reimplementing them, so a batch shows up on their screens as a run does.
// ===========================================================================

// Append a row for `dataset`, seeded with whatever preset the trainer screen
// is on -- a queue is usually built right after tuning the settings it should
// run with. Shared by the picker, the recents menu and drag-and-drop.
BatchRun GuiApp::batch_run_on_screen() const {
    BatchRun run;
    run.preset.path = _train_presets.file;
    run.preset.name =
        _train_presets.file.empty() ? _preset : _train_presets.display;
    return run;
}

void GuiApp::add_batch_row(const std::string& dataset) {
    if (dataset.empty()) return;
    BatchRow r;
    r.dataset = dataset;
    r.does(BatchStage::Train) = true;
    r.runs.push_back(batch_run_on_screen());
    _batch.push_back(std::move(r));
    _batch_open_row = (int)_batch.size() - 1;
    batch_edited();
}

void GuiApp::add_batch_source_row(const std::vector<std::string>& sources) {
    if (sources.empty()) return;
    BatchRow r;
    r.sources = sources;
    // Building a dataset and then training it is the whole reason a row can
    // do more than one thing; the checkboxes still say so, and still undo it.
    r.does(BatchStage::Dataset) = true;
    r.does(BatchStage::Train) = true;
    r.dataset_preset.path = _ds_presets.file;
    r.dataset_preset.name =
        _ds_presets.file.empty() ? _ds_presets.builtin : _ds_presets.display;
    r.runs.push_back(batch_run_on_screen());
    _batch.push_back(std::move(r));
    _batch_open_row = (int)_batch.size() - 1;
    batch_edited();
}

void GuiApp::add_batch_mesh_row(const std::string& model) {
    if (model.empty()) return;
    BatchRow r;
    r.model = model;
    r.does(BatchStage::Train) = false;
    r.does(BatchStage::Mesh) = true;
    r.mesh.preset.path = _mesh_presets.file;
    r.mesh.preset.name = _mesh_presets.display;
    _batch.push_back(std::move(r));
    _batch_open_row = (int)_batch.size() - 1;
    batch_edited();
}

void GuiApp::batch_edited() {
    _batch_dirty = true;
    _batch_checked = false;
}

BatchCapabilities GuiApp::batch_capabilities() const {
    BatchCapabilities caps;
    caps.device = backend::device_count() > 0 && backend::device_current() >= 0;
    caps.builtin_sfm = builtin_sfm_available();
    caps.colmap = colmap_available();
    caps.masking = backends().builtin_masking;
    caps.geometry = geometry_availability().empty();
    caps.mask_model_ready = [](const std::string& id) {
        const ModelEntry* e = find_model(id);
        return e && model_is_cached(*e);
    };
    caps.geometry_model_ready = [](const std::string& id) {
        return geometry_model_cached(id);
    };
    return caps;
}

void GuiApp::check_batch() {
    const BatchCapabilities caps = batch_capabilities();
    for (int i = 0; i < (int)_batch.size(); i++)
        _batch[i].issues = batch_check_row(_batch[i], _batch, i, caps);
    _batch_checked = true;
    _batch_checked_at = ImGui::GetTime();
}

// Nothing waits for a button: an edit re-checks at once, and a timer catches
// a preset file or a dataset folder that went missing while the screen was
// up. A few stats and a small JSON per preset, so the cost is noise.
void GuiApp::check_batch_if_stale() {
    if (_batch_active) return;
    const double now = ImGui::GetTime();
    if (_batch_checked && _batch_checked_at >= 0.0 && now - _batch_checked_at < 2.0)
        return;
    check_batch();
}

void GuiApp::request_start_batch(bool skip_invalid) {
    // A reconstruction or a meshing child owns the device and has no
    // stop-and-do-this-instead confirmation; only training does.
    if (dataset_busy() || _mesh.busy()) {
        _batch_msg = msg::batch_busy_elsewhere.get();
        _batch_msg_err = true;
        return;
    }
    if (training_busy()) {
        _pending = Pending::StartBatch;
        _pending_batch_skip = skip_invalid;
        _open_confirm = true;
        return;
    }
    if (native_work_busy()) return;
    start_batch(skip_invalid);
}

void GuiApp::start_batch(bool skip_invalid) {
    check_batch();

    int bad = 0, runnable = 0;
    for (const BatchRow& r : _batch) {
        if (!r.enabled) continue;
        (batch_has_error(r.issues) ? bad : runnable)++;
    }
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
    // Frozen once for the whole queue, so no task halfway down it can be
    // refused the device the ones before it ran on.
    if (!freeze_native_device() || !freeze_cuda_device()) {
        _batch_msg = _native_device_error.empty() ? msg::no_device_found.get()
                                                  : _native_device_error;
        _batch_msg_err = true;
        return;
    }

    _batch_tasks = batch_plan(_batch);
    for (BatchTask& t : _batch_tasks)
        if (batch_has_error(_batch[(size_t)t.row].issues))
            t.status = BatchStatus::Skipped;
    _batch_dirty = false;
    save_batch_list(_batch);

    _batch_active = true;
    _batch_launched = false;
    _batch_current = -1;
    _batch_stop_after = false;
    _batch_stop_now = false;
    int todo = 0;
    for (const BatchTask& t : _batch_tasks)
        todo += t.status == BatchStatus::Pending ? 1 : 0;
    _batch_msg = i18n::format(msg::batch_log_started, {(long long)todo});
    _batch_msg_err = false;
    log(_batch_msg);
}

bool GuiApp::batch_stage_busy(BatchStage stage) const {
    switch (stage) {
        case BatchStage::Dataset: return dataset_busy();
        case BatchStage::Mesh:    return _mesh.busy();
        case BatchStage::Train: {
            const TrainRunner::Phase ph = _runner.phase();
            return ph == TrainRunner::Phase::Preparing ||
                   ph == TrainRunner::Phase::Training;
        }
    }
    return false;
}

const BatchTask* GuiApp::batch_task_of(int row, BatchStage stage,
                                       int variant) const {
    for (const BatchTask& t : _batch_tasks)
        if (t.row == row && t.stage == stage && t.variant == variant) return &t;
    return nullptr;
}

void GuiApp::fail_batch_task(BatchTask& task, const std::string& error) {
    task.status = BatchStatus::Failed;
    task.message = error;
    log(i18n::format(msg::batch_log_job_failed,
                     {(long long)(_batch_current + 1), error}));
}

// What became of the task that was running, read off the runner that owned it.
void GuiApp::record_batch_task() {
    if (_batch_current < 0 || _batch_current >= (int)_batch_tasks.size()) return;
    BatchTask& t = _batch_tasks[(size_t)_batch_current];
    const long long n = _batch_current + 1;
    t.seconds = std::max(0.0, ImGui::GetTime() - t.started_at);

    bool ok = false, cancelled = false;
    std::string error;
    switch (t.stage) {
        case BatchStage::Dataset: {
            const bool builtin = effective_engine() == Engine::BuiltIn;
            ok = builtin ? _sfm.state() == SfmRunner::State::Done
                         : _colmap.state() == ColmapRunner::State::Done;
            cancelled = builtin
                            ? _sfm.state() == SfmRunner::State::Cancelled
                            : _colmap.state() == ColmapRunner::State::Cancelled;
            error = builtin ? _sfm.error() : _colmap.error();
            if (ok) {
                t.result = builtin ? _sfm.dataset_dir() : _colmap.dataset_dir();
                t.image_dir = builtin ? _sfm.image_dir() : _colmap.image_dir();
                t.mask_dir = builtin ? _sfm.mask_dir() : _colmap.mask_dir();
                t.mask_flipped = builtin ? _sfm.mask_flipped()
                                         : _colmap.mask_flipped();
                // The rest of the row trains and meshes this folder, so the
                // row remembers it even for a re-run started later.
                if (t.row < (int)_batch.size() && _batch[(size_t)t.row].dataset.empty()) {
                    _batch[(size_t)t.row].dataset = t.result;
                    _batch_dirty = true;
                }
            }
            break;
        }
        case BatchStage::Train: {
            ok = _runner.phase() == TrainRunner::Phase::Done && !_batch_stop_now;
            error = _runner.error();
            if (ok) {
                t.steps = _runner.latest_progress().step + 1;
                if (auto* s = _runner.session()) t.result = s->out_dir.string();
            }
            break;
        }
        case BatchStage::Mesh: {
            ok = _mesh.state() == MeshRunner::State::Done;
            cancelled = _mesh.state() == MeshRunner::State::Cancelled;
            error = _mesh.error();
            if (ok) t.result = _mesh.output_path();
            break;
        }
    }

    if (ok) {
        t.status = BatchStatus::Done;
        log(i18n::format(msg::batch_log_job_done, {n, t.result}));
    } else if (_batch_stop_now || cancelled) {
        t.status = BatchStatus::Stopped;
        log(i18n::format(msg::batch_log_job_stopped, {n}));
    } else {
        // Anything the pipeline threw: an unreadable dataset, an OOM, a driver
        // fault. Recorded and left behind -- the point of a queue is that the
        // next row still gets its turn.
        t.status = BatchStatus::Failed;
        t.message = error;
        log(i18n::format(msg::batch_log_job_failed, {n, error}));
    }
}

bool GuiApp::launch_batch_dataset(BatchTask& task, const BatchRow& row) {
    std::vector<PrepInput> sources;
    DatasetSettings settings;
    std::string workspace, error;
    if (!batch_build_dataset_job(row, _ffmpeg_exe, sources, settings, workspace,
                                 error)) {
        fail_batch_task(task, error);
        return false;
    }
    // The screen is driven rather than bypassed: one implementation of "start
    // a dataset run", and whoever is watching sees what is running.
    detach_session_views();
    close_splat();
    // A reconstruction brings up a Vulkan device of its own and wants the
    // VRAM the last training run is still holding.
    _runner.release_engine();
    _sources = sources;
    _workspace = _workspace_auto = workspace;
    apply_dataset_settings(settings);
    _sources = sources;          // rescan_found_masks() re-derived the list
    _source_path_edits.clear();  // re-seeded from _sources by the next draw
    _redo_frames = _redo_masks = _redo_model = _redo_geometry = false;
    log(i18n::format(msg::batch_log_build,
                     {(long long)(_batch_current + 1), workspace}));
    follow_batch_screen(BatchStage::Dataset);
    if (!launch_dataset_job()) {
        fail_batch_task(task, _native_device_error);
        return false;
    }
    return true;
}

bool GuiApp::launch_batch_train(BatchTask& task, const BatchRow& row) {
    // What this row produced upstream, or what it was pointed at.
    std::string dataset = row.dataset, image_dir, mask_dir;
    bool flipped = false;
    if (const BatchTask* made = batch_task_of(task.row, BatchStage::Dataset, 0)) {
        if (made->status != BatchStatus::Done) {
            task.status = BatchStatus::Skipped;
            log(i18n::format(msg::batch_log_task_skipped,
                             {(long long)(_batch_current + 1)}));
            return false;
        }
        dataset = made->result;
        image_dir = made->image_dir;
        mask_dir = made->mask_dir;
        flipped = made->mask_flipped;
    }

    TrainConfig cfg;
    std::string base, error;
    if (!batch_build_train_config(row, task.variant, dataset, image_dir, mask_dir,
                                  flipped, cfg, base, error)) {
        fail_batch_task(task, error);
        return false;
    }
    log(i18n::format(msg::batch_log_train,
                     {(long long)(_batch_current + 1), dataset}));
    follow_batch_screen(BatchStage::Train);
    // A run's own dataset options must not be re-parsed under it by a stale
    // edit made on the trainer screen before the queue started.
    _parse_dirty = false;
    launch_training(cfg, base);
    return true;
}

bool GuiApp::launch_batch_mesh(BatchTask& task, const BatchRow& row) {
    std::string model = row.model, dataset = row.dataset;
    if (const BatchTask* made = batch_task_of(task.row, BatchStage::Dataset, 0))
        if (made->status == BatchStatus::Done) dataset = made->result;
    if (model.empty()) {
        const BatchTask* run = batch_task_of(task.row, BatchStage::Train,
                                             task.variant);
        if (!run || run->status != BatchStatus::Done) {
            task.status = BatchStatus::Skipped;
            log(i18n::format(msg::batch_log_task_skipped,
                             {(long long)(_batch_current + 1)}));
            return false;
        }
        model = run->result;
    }

    MeshJob job;
    std::string error;
    if (!batch_build_mesh_job(row, model, dataset, job, error)) {
        fail_batch_task(task, error);
        return false;
    }
    log(i18n::format(msg::batch_log_mesh,
                     {(long long)(_batch_current + 1), model}));
    follow_batch_screen(BatchStage::Mesh);
    // The engine has to be free: the mesh child wants the VRAM, and both the
    // last preview and the run that trained this model are holding it.
    stop_inference_users();
    close_mesh_preview();
    close_splat();
    _runner.release_engine();
    job.device_uuid = _native_device_uuid;
    job.cuda_device = _cuda_device_index;
    _mesh_job = job;
    _mesh_shown_run = _mesh.run_id();   // a batch opens no preview
    _mesh.start(job);
    return true;
}

bool GuiApp::launch_batch_task(BatchTask& task) {
    const BatchRow& row = _batch[(size_t)task.row];
    task.started_at = ImGui::GetTime();
    try {
        switch (task.stage) {
            case BatchStage::Dataset: return launch_batch_dataset(task, row);
            case BatchStage::Train:   return launch_batch_train(task, row);
            case BatchStage::Mesh:    return launch_batch_mesh(task, row);
        }
    } catch (const std::exception& e) {
        // Nothing below here is supposed to throw, and a queue that dies on
        // one row that does is the failure this whole screen exists to avoid.
        fail_batch_task(task, e.what());
    }
    return false;
}

void GuiApp::advance_batch() {
    if (!_batch_active) return;

    if (_batch_launched) {
        if (_batch_current >= 0 && _batch_current < (int)_batch_tasks.size() &&
            batch_stage_busy(_batch_tasks[(size_t)_batch_current].stage))
            return;   // still going
        record_batch_task();
        _batch_launched = false;
        _batch_current = -1;
        if (_batch_stop_after) { finish_batch(); return; }
    }

    // The next task, skipping the ones whose input never arrived. Nothing is
    // started on a pass that skips, so the loop is bounded by the task count.
    for (;;) {
        int next = -1;
        for (int i = 0; i < (int)_batch_tasks.size(); i++)
            if (_batch_tasks[(size_t)i].status == BatchStatus::Pending) {
                next = i;
                break;
            }
        if (next < 0) { finish_batch(); return; }
        _batch_current = next;
        BatchTask& t = _batch_tasks[(size_t)next];
        if (t.row < 0 || t.row >= (int)_batch.size()) {
            t.status = BatchStatus::Skipped;
            continue;
        }
        if (launch_batch_task(t)) {
            t.status = BatchStatus::Running;
            _batch_launched = true;
            return;
        }
        // launch_batch_task always recorded an outcome, so the next pass
        // picks a different task.
    }
}

void GuiApp::finish_batch() {
    int done = 0, failed = 0, other = 0;
    for (const BatchTask& t : _batch_tasks) {
        if (t.status == BatchStatus::Done) done++;
        else if (t.status == BatchStatus::Failed) failed++;
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
    if (_batch_dirty) {
        _batch_dirty = false;
        save_batch_list(_batch);
    }
    run_batch_command(i18n::format(msg::batch_cmd_message,
                                   {(long long)done, (long long)failed,
                                    (long long)other}));
}

// The whole point of an unattended queue is not watching it, so the one thing
// it can do on its own behalf is tell somebody it is over. Runs however the
// queue ended -- finished, failed or stopped; the message says which.
void GuiApp::run_batch_command(const std::string& message) {
    if (_batch_cmd.empty()) return;
    const std::vector<std::string> argv =
        command_argv(_batch_cmd, kBatchMessageToken, message);
    if (argv.empty()) return;
    log(i18n::format(msg::batch_cmd_running, {_batch_cmd}));
    if (!_batch_cmd_run.start(argv)) log(msg::batch_cmd_busy.get());
}

// What it printed and how it ended, in the log like any other runner's.
void GuiApp::poll_batch_command() {
    for (const std::string& s : _batch_cmd_run.drain_log()) log(s);
    const int code = _batch_cmd_run.take_exit_code();
    if (code == CommandRunner::kNoResult || code == kCancelled) return;
    if (code == kSpawnFailed)
        log(i18n::format(msg::batch_cmd_missing, {_batch_cmd_run.program()}));
    else if (code != 0)
        log(i18n::format(msg::batch_cmd_exit, {(long long)code}));
    else
        log(msg::batch_cmd_ok.get());
}

void GuiApp::cancel_batch() {
    if (!_batch_active) return;
    if (_batch_current >= 0 && _batch_current < (int)_batch_tasks.size())
        _batch_tasks[(size_t)_batch_current].status = BatchStatus::Stopped;
    _batch_launched = false;
    _batch_current = -1;
    finish_batch();
}

// The screen a running stage belongs to, unless whoever is here has walked off
// to the list or the viewer to look at something else.
void GuiApp::follow_batch_screen(BatchStage stage) {
    if (_screen == Screen::Batch || _screen == Screen::Viewer) return;
    switch (stage) {
        case BatchStage::Dataset: _screen = Screen::NewDataset; break;
        case BatchStage::Train:   _screen = Screen::Train; break;
        case BatchStage::Mesh:    _screen = Screen::Mesh; break;
    }
}

// A path's extension, lowercased, for the tables that list them.
static std::string lower_ext(const fs::path& p) {
    std::string e = p.extension().string();
    for (char& c : e) c = (char)std::tolower((unsigned char)c);
    return e;
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
    if (_screen == Screen::Mesh && !native_work_busy() && paths.size() == 1) {
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
    // and by content rather than by name: nothing else it could be. Its own
    // kind decides which screen it lands on.
    if (paths.size() == 1 && fs::is_regular_file(paths[0], ec)) {
        const std::optional<PresetKind> kind = probe_preset_kind(paths[0]);
        if (kind == PresetKind::Dataset) {
            load_dataset_preset_file(paths[0]);
            if (!_ds_presets.msg_err) _screen = Screen::NewDataset;
            return;
        }
        if (kind == PresetKind::Mesh) {
            load_mesh_preset_file(paths[0]);
            if (!_mesh_presets.msg_err) _screen = Screen::Mesh;
            return;
        }
        if (kind == PresetKind::Train && is_preset_file(paths[0])) {
            load_preset_file(paths[0]);
            if (!_train_presets.msg_err && !_cfg.data.empty())
                _screen = Screen::Train;
            return;
        }
    }
    // A camera project: its model on the viewer screen, and the render with it.
    if (paths.size() == 1 && lower_ext(paths[0]) == ".json" && open_render_project(paths[0]))
        return;
    // Dropping onto the batch screen extends the queue, which is how a
    // five-dataset run gets set up without typing five paths. What each path
    // is decides what its row does.
    if (_screen == Screen::Batch && !_batch_active) {
        std::vector<std::string> datasets, models, raw;
        for (const std::string& p : paths) {
            if (fs::is_directory(p, ec) && looks_like_model(p)) models.push_back(p);
            else if (fs::is_directory(p, ec) && folder_looks_like_dataset(p))
                datasets.push_back(p);
            else if (fs::is_directory(p, ec) && folder_has_images(p)) raw.push_back(p);
            else if (fs::is_regular_file(p, ec) && is_video_path(p)) raw.push_back(p);
            else if (fs::is_regular_file(p, ec) &&
                     std::find(kViewableExtensions.begin(), kViewableExtensions.end(),
                               lower_ext(p)) != kViewableExtensions.end())
                models.push_back(p);
        }
        if (datasets.size() + models.size() + raw.size() == paths.size() &&
            !paths.empty()) {
            for (const std::string& d : datasets) add_batch_row(d);
            for (const std::string& m : models) add_batch_mesh_row(m);
            // Videos dropped together are one capture, as they are everywhere
            // else; folders are one each.
            std::vector<std::string> clips;
            for (const std::string& r : raw) {
                if (fs::is_directory(r, ec)) add_batch_source_row({r});
                else clips.push_back(r);
            }
            if (!clips.empty()) add_batch_source_row(clips);
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
            !native_work_busy()) {
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
    // A reconstruction that is not a trainable dataset -- a bare `sparse/`,
    // one of its models, or a folder holding one without the images -- is
    // something to look at and clean up, not something to train on.
    if (paths.size() == 1 && fs::is_directory(paths[0], ec) &&
        !folder_looks_like_dataset(paths[0]) &&
        !spirula::resolve_sparse_dir(paths[0]).empty()) {
        request_open_splat(paths[0]);
        return;
    }
    // ... except on the dataset screen, where a finished dataset is an input
    // like any other: that is how one gets masks, depth and normals added to
    // it without its cameras being solved a second time.
    if (paths.size() == 1 && fs::is_directory(paths[0], ec) &&
        folder_looks_like_dataset(paths[0])) {
        if (_screen == Screen::NewDataset && !dataset_busy())
            add_existing_dataset(paths[0]);
        else
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
        // A .ply is a model to look at -- Gaussians, a point cloud or a mesh,
        // which the viewer works out for itself. (A Metashape dataset FOLDER is
        // caught above, so dropping one still opens the dataset.)
        if (std::find(kViewableExtensions.begin(), kViewableExtensions.end(),
                      ext) != kViewableExtensions.end()) {
            request_open_splat(p.string());
            return;
        }
        if (p.filename() == "transforms.json" || ext == ".db" ||
            ext == ".bin" || ext == ".txt" || ext == ".xml") {
            const std::string dir = p.parent_path().string();
            // A file from a COLMAP model folder names a reconstruction, not a
            // dataset: `sparse/0` has no images beside it to train from.
            if (!folder_looks_like_dataset(dir) &&
                !spirula::resolve_sparse_dir(paths[0]).empty())
                request_open_splat(paths[0]);
            else
                request_open_dataset(dir);
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
    if (native_work_busy()) {
        if (training_busy()) log(dmsg::log_drop_while_training.get());
        return;
    }
    // Dropping onto the dataset screen adds to what is already listed there;
    // dropping from anywhere else starts a new dataset.
    if (add_sources(sources, /*replace=*/_screen != Screen::NewDataset))
        _screen = Screen::NewDataset;
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
    assign_source_subdirs(_sources);
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

    refresh_subcameras(_sources);
    std::string rig_key;
    for (const CameraGroup& g : camera_groups(_sources))
        if (!_sources[g.input].is_video) rig_key += _sources[g.input].path + '\n' + g.rel + '\n';
    if (rig_key != _rig_guess_key) {
        _rig_guess_key = rig_key;
        guess_source_rigs(_sources, /*force=*/false);
    }
    normalize_source_lenses(_sources, _sfm_job.camera_model);
    normalize_source_fps(_sources, _sfm_job.prep.video_fps);

    // The output folder follows the input until the user takes it over.
    if (_workspace.empty() || _workspace == _workspace_auto) {
        _workspace = default_workspace(_sources);
        _workspace_auto = _workspace;
    }
}
void GuiApp::mark_source_metadata_dirty() {
    _source_probe.invalidate();
    _source_probes_ready = true;
    for (const PrepInput& s : _sources) {
        if (s.is_video && !s.path.empty()) {
            _source_probes_ready = false;
            return;
        }
    }
}
void GuiApp::pump_source_probes() {
    if (native_work_busy()) return;
    bool pending = false;
    bool pano_changed = false;
    for (size_t i = 0; i < _sources.size(); i++) {
        PrepInput& s = _sources[i];
        if (!s.is_video || s.path.empty()) continue;
        const std::string path = s.path;
        const SourceProbeInfo info = _source_probe.get(path, _ffmpeg_exe);
        if (!info.done) {
            pending = true;
            continue;
        }
        if (_sources[i].path != path || !_sources[i].is_video) continue;
        if (info.width > 0 && info.height > 0)
            _input_size[path] = {info.width, info.height};
        s.pano360_unsupported = info.pano360_unsupported;
        if (!s.pano360.valid() && info.pano360.valid()) {
            s.pano360 = info.pano360;
            pano_changed = true;
        }
        // A file with several lenses starts as a rig of its own, once; the row
        // can still say otherwise.
        if (s.video_tracks == 0 && info.video_tracks > 0) {
            s.video_tracks = info.video_tracks;
            if (s.pano360.valid() || s.video_tracks >= 2) s.rig = kRigOwn;
        }
        const int packed = s.packed_lenses;
        set_packed_lenses(s, info.width, info.height);
        pano_changed = pano_changed || packed != s.packed_lenses;
    }
    if (native_work_busy()) return;
    _source_probes_ready = !pending;
    if (!pano_changed) return;
    apply_capture_defaults(_sources, _sfm_job, _colmap_job);
    reapply_dataset_builtin();
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

void GuiApp::replace_source(size_t input, const std::string& path) {
    if (input >= _sources.size() || native_work_busy()) return;
    close_native_previews();
    close_splat();
    _sources[input] = path.empty() ? PrepInput{}
                                   : make_source(path, _use_found_masks);
    if (input >= _source_path_edits.size())
        _source_path_edits.resize(input + 1);
    _source_path_edits[input] = _sources[input].path;
    mark_source_metadata_dirty();
    apply_capture_defaults(_sources, _sfm_job, _colmap_job);
    reapply_dataset_builtin();
    if (!_color_space_touched) {
        _sfm_job.image_gamut.clear();
        _sfm_job.image_is_linear.reset();
    }
    adopt_exr_color_space();
    refresh_sources();
}
bool GuiApp::add_sources(const std::vector<std::string>& paths, bool replace) {
    if (paths.empty() || native_work_busy()) return false;
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
    if (inputs.empty() && _sources.empty()) {
        for (const std::string& masks : mask_folders)
            log(i18n::format(dmsg::log_masks_orphaned, {masks}));
        return false;
    }
    close_native_previews();
    close_splat();
    if (replace && !inputs.empty()) {
        _sources.clear();
        _source_path_edits.clear();
        _mask_preview_input = 0;
        // apply_capture_defaults() is about to move settings the stamp is made
        // of, so it no longer describes anything this panel built.
        _built_workspace.clear();
        // A different capture is a different job, and the geometry options
        // are remembered nowhere: a run must never quietly cost an hour of
        // inference nobody asked for. (Not mid-run: that would disable it.)
        if (!dataset_busy()) _geometry = GeometryJob{};
        // ... and a different capture is a different colour space.
        _sfm_job.image_gamut.clear();
        _sfm_job.image_is_linear.reset();
        _color_space_touched = false;
    }
    const size_t first_new = _sources.size();
    for (const std::string& path : inputs) {
        _sources.push_back(make_source(path, _use_found_masks));
        _source_path_edits.push_back(_sources.back().path);
    }
    // A 360 camera's own files always carry the lens border, so they tick the
    // box themselves -- only on arrival, so unticking it sticks.
    for (size_t i = first_new; i < _sources.size(); i++) {
        PrepInput& in = _sources[i];
        if (!has_fisheye_lens(in)) continue;
        _border_enable = true;
        if (in.stencil.empty()) in.stencil.detect_border = true;
    }
    apply_frame_shapes(first_new);
    for (const std::string& masks : mask_folders) {
        if (attach_mask_folder(_sources, masks))
            log(i18n::format(dmsg::log_masks_attached, {masks}));
        else
            log(i18n::format(dmsg::log_masks_orphaned, {masks}));
    }
    if (!inputs.empty()) mark_source_metadata_dirty();
    if (_sources.empty()) return false;
    apply_capture_defaults(_sources, _sfm_job, _colmap_job);
    reapply_dataset_builtin();
    if (_mask_preview_input >= (int)_sources.size()) _mask_preview_input = 0;
    adopt_exr_color_space();
    refresh_sources();
    return true;
}

void GuiApp::add_existing_dataset(const std::string& dir) {
    if (dir.empty()) return;
    // The ordinary route already lands on the right pair for the usual
    // layout: resolve_photo_folder picks images/ out of the folder and
    // refresh_sources makes its parent -- this folder -- the output.
    if (!add_sources({dir}, /*replace=*/true)) return;
    std::error_code ec;
    if (!folder_looks_like_dataset(dir) ||
        !fs::equivalent(_sources[0].path, dir, ec))
        return;
    // A dataset whose photographs sit at its root rather than under images/.
    // They are read where they are: gathering them into an images/ beside the
    // model would leave the folder holding the capture twice.
    _photo_import = PhotoImport::InPlace;
    _workspace = _workspace_auto = fs::absolute(dir, ec).string();
}

// What this run draws on its inputs, kept in the output folder for next time.
void GuiApp::save_run_stencils() {
    if (!_border_enable || _workspace.empty()) return;
    std::vector<std::pair<std::string, std::vector<app::MaskShape>>> inputs;
    for (const PrepInput& in : _sources) inputs.push_back({in.path, in.stencil.mask.shapes});
    std::string err;
    const std::vector<std::string> written = save_dataset_stencils(_workspace, inputs, err);
    if (!err.empty()) log(i18n::format(dmsg::stencil_save_failed, {err}));
    for (const std::string& f : written) log(i18n::format(dmsg::stencil_autosaved, {f}));
}

void GuiApp::apply_frame_shapes(size_t first_input) {
    if (_frame_shapes.empty()) return;
    std::vector<app::MaskShape> shapes;
    std::string err;
    if (!load_stencil_preset(_frame_shapes, shapes, err)) {
        log(i18n::format(dmsg::stencil_load_failed, {err}));
        _frame_shapes.clear();
        return;
    }
    for (size_t i = first_input; i < _sources.size(); i++)
        _sources[i].stencil.mask.shapes = shapes;
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

const char* GuiApp::dir_key(PickAction a, FileDialog::Mode m) {
    switch (a) {
        case PickAction::OpenDataset:
        case PickAction::BatchDataset:
        case PickAction::SourceDataset:
        case PickAction::MeshPhotos:        return "dataset";
        case PickAction::SourceImages:
        case PickAction::BatchSourceImages: return "photos";
        case PickAction::SourceVideo:
        case PickAction::BatchSourceVideo:  return "video";
        case PickAction::SourceReplace:
            return m == FileDialog::Mode::File ? "video" : "photos";
        case PickAction::SplatFile:
        case PickAction::AddSplatFile:
        case PickAction::SplatFolder:
        case PickAction::BatchModel:
        case PickAction::EditSaveFile:
        case PickAction::EditSaveFolder:
        case PickAction::RenderAddModel:
        case PickAction::MeshSource:        return "model";
        case PickAction::StencilFile:       return "stencil";
        case PickAction::RenderProjectSave:
        case PickAction::RenderProjectOpen: return "render_project";
        case PickAction::RenderOutput:      return "render_output";
        case PickAction::Workspace:
        case PickAction::OutputPrefix:
        case PickAction::BatchOutput:
        case PickAction::MeshOutput:        return "output";
        case PickAction::VocabTree:         return "vocab";
        case PickAction::PresetFile:
        case PickAction::DatasetPresetFile:
        case PickAction::MeshPresetFile:
        case PickAction::PresetSaveFolder:
        case PickAction::BatchPresetFile:
        case PickAction::BatchDatasetPresetFile:
        case PickAction::BatchMeshPresetFile: return "preset";
        case PickAction::MaskModelFile:
        case PickAction::None:              return "";
    }
    return "";
}

void GuiApp::remember_dir(const std::string& key, const std::string& path) {
    if (key.empty() || path.empty()) return;
    std::string dir = fs::path(path).parent_path().string();
    if (!dir.empty()) _dialog_dirs[key] = dir;
}

void GuiApp::open_pick(PickAction a, const std::string& title,
                       FileDialog::Mode mode,
                       const std::vector<std::string>& extensions,
                       const std::string& start_dir, bool multi,
                       const std::string& suggested_name) {
    _pick = a;
    _pick_key = dir_key(a, mode);
    std::string dir = start_dir;
    if (dir.empty()) {
        auto it = _dialog_dirs.find(_pick_key);
        if (it != _dialog_dirs.end()) dir = it->second;
    }
    _dialog.open(title, mode, extensions, dir, multi, suggested_name);
}

void GuiApp::handle_dialog_result(const std::vector<std::string>& paths) {
    const std::string path = paths.empty() ? std::string() : paths[0];
    remember_dir(_pick_key, path);
    switch (_pick) {
        case PickAction::OpenDataset:
            request_open_dataset(path);
            break;
        case PickAction::SourceImages:
        case PickAction::SourceVideo:
            if (add_sources(paths, /*replace=*/_screen != Screen::NewDataset))
                _screen = Screen::NewDataset;
            break;
        case PickAction::SourceDataset:
            add_existing_dataset(path);
            _screen = Screen::NewDataset;
            break;
        case PickAction::SourceReplace:
            if (!path.empty() && _pick_source >= 0 &&
                _pick_source < (int)_sources.size())
                replace_source((size_t)_pick_source, path);
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
        case PickAction::SplatFolder:
            request_open_splat(path);
            break;
        case PickAction::EditSaveFile:
        case PickAction::EditSaveFolder:
            _compare.edit().save_to(_edit_save_target, path);
            break;
        case PickAction::RenderProjectSave:
            _compare.render().picked(gui::render::RenderSession::Pick::SaveProject, path);
            break;
        case PickAction::RenderProjectOpen:
            _compare.render().picked(gui::render::RenderSession::Pick::OpenProject, path);
            break;
        case PickAction::RenderOutput:
            _compare.render().picked(gui::render::RenderSession::Pick::Output, path);
            break;
        case PickAction::RenderAddModel:
            add_splat(path);
            break;
        case PickAction::MeshSource:
            set_mesh_source(path);
            break;
        case PickAction::StencilFile:
            if (_segment.is_open() && _mask_preview_input < (int)_sources.size())
                _segment.load_file(_sources[(size_t)_mask_preview_input].stencil, path);
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
        case PickAction::DatasetPresetFile:
            load_dataset_preset_file(path);
            break;
        case PickAction::MeshPresetFile:
            load_mesh_preset_file(path);
            break;
        case PickAction::BatchDataset:
            if (!path.empty()) {
                if (_pick_row >= 0 && _pick_row < (int)_batch.size()) {
                    _batch[_pick_row].dataset = path;
                    batch_edited();
                } else {
                    // Several folders picked at once become several rows.
                    for (const std::string& p : paths) add_batch_row(p);
                }
            }
            _pick_row = -1;
            break;
        case PickAction::BatchSourceImages:
        case PickAction::BatchSourceVideo:
            if (!paths.empty()) {
                if (_pick_row >= 0 && _pick_row < (int)_batch.size()) {
                    for (const std::string& p : paths)
                        _batch[_pick_row].sources.push_back(p);
                    batch_edited();
                } else {
                    // Videos picked together are one capture, which is what
                    // the dataset screen does with the same pick. A folder of
                    // photos is a capture on its own.
                    if (_pick == PickAction::BatchSourceVideo)
                        add_batch_source_row(paths);
                    else
                        for (const std::string& p : paths)
                            add_batch_source_row({p});
                }
            }
            _pick_row = -1;
            break;
        case PickAction::BatchModel:
            if (!path.empty()) {
                if (_pick_row >= 0 && _pick_row < (int)_batch.size()) {
                    _batch[_pick_row].model = path;
                    batch_edited();
                } else {
                    for (const std::string& p : paths) add_batch_mesh_row(p);
                }
            }
            _pick_row = -1;
            break;
        case PickAction::BatchOutput:
            if (_pick_row >= 0 && _pick_row < (int)_batch.size()) {
                _batch[_pick_row].output_dir = path;
                batch_edited();
            }
            _pick_row = -1;
            break;
        case PickAction::BatchDatasetPresetFile:
            if (!path.empty() && _pick_row >= 0 && _pick_row < (int)_batch.size()) {
                try {
                    DatasetPreset p = load_dataset_preset(path);
                    _batch[_pick_row].dataset_preset = {p.path, p.name};
                    batch_edited();
                } catch (const std::exception& e) {
                    _batch_msg = i18n::format(msg::preset_failed, {e.what()});
                    _batch_msg_err = true;
                }
            }
            _pick_row = -1;
            break;
        case PickAction::BatchMeshPresetFile:
            if (!path.empty() && _pick_row >= 0 && _pick_row < (int)_batch.size()) {
                try {
                    MeshPreset p = load_mesh_preset(path);
                    _batch[_pick_row].mesh.preset = {p.path, p.name};
                    batch_edited();
                } catch (const std::exception& e) {
                    _batch_msg = i18n::format(msg::preset_failed, {e.what()});
                    _batch_msg_err = true;
                }
            }
            _pick_row = -1;
            break;
        case PickAction::BatchPresetFile:
            if (!path.empty() && _pick_row >= 0 &&
                _pick_row < (int)_batch.size()) {
                try {
                    TrainPreset p = load_preset(path);
                    BatchRow& row = _batch[_pick_row];
                    if (_pick_slot >= 0 && _pick_slot < (int)row.runs.size()) {
                        row.runs[_pick_slot].preset.path = p.path;
                        row.runs[_pick_slot].preset.name = p.name;
                    } else {
                        BatchRun run;
                        run.preset = {p.path, p.name};
                        row.runs.push_back(std::move(run));
                    }
                    batch_edited();
                } catch (const std::exception& e) {
                    _batch_msg = i18n::format(msg::preset_failed, {e.what()});
                    _batch_msg_err = true;
                }
            }
            _pick_row = -1;
            _pick_slot = -1;
            break;
        default:
            break;
    }
    _pick = PickAction::None;
    _pick_key.clear();
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
                                    "preparing", "training", "done",
                                    "train_error"};
    std::string out = "\"screen\":\"";
    out += kScreens[(int)_screen];
    out += "\",\"train_phase\":\"";
    out += kPhases[(int)_runner.phase()];
    out += "\",\"step\":" + std::to_string(_runner.latest_progress().step);
    out += ",\"busy\":";
    out += native_work_busy() ? "true" : "false";
    out += ",\"dialog_open\":";
    out += _dialog.is_open() ? "true" : "false";
    out += ",\"models_open\":" + std::to_string(_compare.count());
    out += ",\"dataset\":" + quoted(_cfg.data);
    out += ",\"mask_editor_open\":";
    out += _mask_editor.is_open() ? "true" : "false";
    // The app's one segmentation checkpoint, which both screens pick and fetch.
    static const char* kDownload[] = {"idle", "running", "done", "failed", "cancelled"};
    out += ",\"model_id\":" + quoted(_model_id);
    out += ",\"model_path\":" + quoted(selected_model_path());
    out += ",\"model_download\":\"";
    out += kDownload[(int)_download.state()];
    out += "\",\"license_prompt\":" + quoted(_license_prompt);
    // Index order is the declaration order of mask::CanvasMode.
    static const char* kCanvasModes[] = {"shape", "eraser", "path", "sam"};
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
    run_pending_if_stopped();
    if (!_edit_train_dataset.empty()) open_edited_dataset();
    // vit-giant2 is two files, and so is ALIKED with LightGlue: both fetches
    // are queues, stepped on from somewhere that runs whatever screen is up.
    _geom_download.pump();
    _feat_download.pump();
    pump_source_probes();
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

    // A job cancelled by the editor's close finishes its stage off this thread.
    _mask_editor.sam_poll_retiring();
    if (_mask_editor.is_open()) {
        // Read now, not at the top of the frame: a preview opened above has
        // already taken the device.
        _mask_editor.set_sam_blocker(mask::MaskSession::sam_blocker(
            _segment.is_open(), _geometry_panel.is_open(), native_work_busy()));
        // Every frame: a pick, or a finished download, lands now.
        const ModelEntry* me = find_model(_mask_editor_model_id);
        _mask_editor.set_sam_model(cached_model_path(_mask_editor_model_id),
                                   me && me->text_prompts);
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
    draw_preset_save_modal();
    draw_preset_delete_modal();
    draw_edit_exit_modal();
    draw_render_exit_modal();
    draw_confirm_modal();
    draw_data_error_modal();

    ImGui::End();
}

void GuiApp::draw_menu_bar() {
    if (!ImGui::BeginMenuBar()) return;
    if (ui::BeginMenu(msg::menu_file)) {
        if (ui::MenuItem(msg::menu_open_dataset)) {
            open_pick(PickAction::OpenDataset, msg::menu_open_dataset.get(),
                      FileDialog::Mode::Folder);
        }
        if (ui::MenuItem(msg::menu_new_dataset) && !native_work_busy()) {
            close_splat();
            _screen = Screen::NewDataset;
        }
        if (ui::MenuItem(msg::menu_open_splat)) {
            open_pick(PickAction::SplatFile, msg::viewer_pick_file.get(),
                      FileDialog::Mode::File, {".ply"});
        }
        ImGui::Separator();
        if (ui::MenuItem(msg::menu_batch) && !native_work_busy())
            _screen = Screen::Batch;
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
            _edit_panel_w = kEditPanelW;
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
#if defined(SS_BUILD_SAM) || defined(SS_TOOL_SFM) || defined(SS_BACKEND_VULKAN)
    {
        // Native choice is reachable before previews and engine work.
        if (ui::BeginMenu(msg::menu_device)) {
            draw_device_picker(/*as_menu=*/true);
            ImGui::EndMenu();
        }
    }
#endif
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

    // Full bleed: the artwork is painted out over the host window's padding on
    // the three sides it touches, so the band meets the menu bar and both
    // edges. Only the drawing grows -- p1 stays where the layout below it is.
    const ImVec2 pad = ImGui::GetStyle().WindowPadding;
    const ImVec2 q0(p0.x - pad.x, p0.y - pad.y);
    const ImVec2 q1(p1.x + pad.x, p1.y);
    const float band_w = q1.x - q0.x, band_h = q1.y - q0.y;

    // Cover, not fit: crop to the band's aspect so the artwork fills the width
    // whatever the window is doing.
    const float want = band_w / band_h, have = (float)bw / (float)bh;
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
    dl->AddImage((ImTextureID)(intptr_t)tex, q0, q1, uv0, uv1);
    // The artwork is bright everywhere, so the text needs its own ground.
    const ImU32 clear = IM_COL32(14, 15, 18, 0), dark = IM_COL32(14, 15, 18, 232);
    dl->AddRectFilledMultiColor(ImVec2(q0.x, q0.y + band_h * 0.30f), q1,
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
        open_pick(PickAction::OpenDataset, msg::menu_open_dataset.get(),
                  FileDialog::Mode::Folder);
    }
    ui::help_on_hover(msg::home_open_dataset_help);

    // Photos and video are one screen and one input list: a capture can hold
    // both, so splitting the entry point in two only asked a question with no
    // right answer. The list is added to there, or dropped onto the window.
    ImGui::BeginDisabled(native_work_busy());
    if (ui::Button(msg::home_new_dataset, ImVec2(-1, bh)) &&
        !native_work_busy()) {
        close_splat();
        _screen = Screen::NewDataset;
    }
    ImGui::EndDisabled();
    ui::help_on_hover(msg::home_new_dataset_help);

    if (ui::Button(msg::home_open_splat, ImVec2(-1, bh))) {
        open_pick(PickAction::SplatFile, msg::viewer_pick_file.get(),
                  FileDialog::Mode::FileOrFolder, kOpenableExtensions);
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
bool GuiApp::native_work_busy() const {
    const TrainRunner::Phase phase = _runner.phase();
    return _mesh.busy() || dataset_busy() ||
           phase == TrainRunner::Phase::Loading ||
           phase == TrainRunner::Phase::Preparing ||
           phase == TrainRunner::Phase::Training;
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
    // Cancelling the run on screen gives up the queue driving it, which is
    // what the trainer's stop confirmation already does.
    if (_batch_active) _batch_stop_after = _batch_stop_now = true;
    _sfm.cancel();
    _colmap.cancel();
}

bool GuiApp::license_accepted(const std::string& family) const {
    return std::find(_accepted_licenses.begin(), _accepted_licenses.end(),
                     family) != _accepted_licenses.end();
}

std::string GuiApp::selected_model_path() const { return cached_model_path(_model_id); }

// Fetch the selected checkpoint, from wherever the screen offers it. Consent
// first, every time the family has not been agreed to (ModelCache.h says why).
void GuiApp::request_model_download(const std::string& id) {
    const ModelEntry* e = find_model(id);
    if (!e || model_is_cached(*e)) return;
    if (_download.state() == ModelDownload::State::Running) return;
    if (license_accepted(e->family)) {
        _download.start(*e);
    } else {
        _license_prompt = e->family;
        _license_model_id = id;
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
        _ws_artifacts = workspace_artifacts(_workspace, _sources);
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
    prep.adaptive_fps = _sfm_job.prep.adaptive_fps;
    prep.adaptive_range = _sfm_job.prep.adaptive_range;
    prep.sharp_window = _sfm_job.prep.sharp_window;
    prep.pano = _sfm_job.prep.pano;
    prep.max_frames = _sfm_job.prep.max_frames;
    prep.force_external_decode = _sfm_job.prep.force_external_decode;
    prep.frame_bits = _sfm_job.prep.frame_bits;
    prep.sync_tracks = _sfm_job.prep.sync_tracks;
    prep.ffmpeg_exe = _ffmpeg_exe;
    prep.python_exe = _python_exe;
    prep.mask_enable = _mask_enable;
    prep.flip_found_masks = _use_found_masks && _flip_found_masks;
    prep.photo_import = _photo_import;
    prep.mask_prompt = _mask.prompt;
    prep.mask_negative_prompt = _mask.negative_prompt;
    prep.mask_keep_subject = _mask.keep_subject;
    prep.mask_max_image_size = _mask.max_image_size;
    prep.mask_dilate_ratio = _mask.boundary_ratio();
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
    // The one frozen choice, re-applied here because this function rebuilds
    // prep from panel state and would otherwise drop it. Empty before the
    // freeze, which is exactly what an unstarted job wants.
    prep.device = _native_device_uuid;
    _sfm_job.prep = prep;

    _colmap_job.inputs = prep.inputs;
    _colmap_job.workspace = prep.workspace;
    _colmap_job.resume = prep.resume;
    // The frozen choice, so a later edit cannot drop the UUID the run has
    // already committed to. Empty before the freeze.
    _colmap_job.device = _native_device_uuid;
    _colmap_job.video_fps = prep.video_fps;
    _colmap_job.adaptive_fps = prep.adaptive_fps;
    _colmap_job.adaptive_range = prep.adaptive_range;
    _colmap_job.sharp_window = prep.sharp_window;
    _colmap_job.pano = prep.pano;
    _colmap_job.max_frames = prep.max_frames;
    _colmap_job.force_external_decode = prep.force_external_decode;
    _colmap_job.frame_bits = prep.frame_bits;
    _colmap_job.photo_import = prep.photo_import;
    _colmap_job.force_external_masking = prep.force_external_masking;
    _colmap_job.colmap_exe = _colmap_exe;
    _colmap_job.ffmpeg_exe = _ffmpeg_exe;
    _colmap_job.python_exe = _python_exe;
    _colmap_job.mask_enable = prep.mask_enable;
    _colmap_job.mask_prompt = prep.mask_prompt;
    _colmap_job.mask_negative_prompt = prep.mask_negative_prompt;
    _colmap_job.mask_keep_subject = prep.mask_keep_subject;
    _colmap_job.mask_max_image_size = prep.mask_max_image_size;
    _colmap_job.mask_dilate_ratio = prep.mask_dilate_ratio;
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
    _sfm_job.mask_features = _colmap_job.mask_features = _mask_features;
    _sfm_job.settings_built_model = _colmap_job.settings_built_model =
        !_workspace.empty() && _workspace == _built_workspace;
    // The same step either way: `spirula geometry` over the finished dataset.
    _sfm_job.geometry = _colmap_job.geometry = _geometry;
    _sfm_job.geometry.overwrite = _colmap_job.geometry.overwrite =
        _geometry.overwrite || _redo_geometry;
    // The frozen choice survives the copy above, so a later panel edit cannot
    // drop the UUID a run already committed to.
    _sfm_job.geometry.device_uuid = _colmap_job.geometry.device_uuid =
        _geometry.device_uuid = _native_device_uuid;
    // ... and the reconstruction's own selector, which SfmRunner::update must
    // not be able to overwrite once the run has started.
    _sfm_job.device_selector = _native_device_uuid;
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

// Masks the reconstruction being kept has never seen, with the panel asking
// for masked feature points: the run can add them for training alone or spend
// the reconstruction again on them, and only the user knows which.
bool GuiApp::masks_miss_kept_model() {
    if (!_mask_enable || !_mask_features || _redo_model) return false;
    if (!workspace_state().model) return false;
    // A model this panel built is one these settings built, masks included --
    // and if they have moved since, the run replaces it anyway.
    if (_workspace == _built_workspace) return false;
    // An input that arrived with its own masks is not segmented, so the
    // dataset gains nothing the model could have missed.
    for (const PrepInput& s : _sources)
        if (s.mask_dir.empty()) return true;
    return false;
}

void GuiApp::start_dataset_job() {
    if (native_work_busy()) return;
    pump_source_probes();
    if (!_source_probes_ready) {
        log(dmsg::sensors_reading.get());
        return;
    }
    if (masks_miss_kept_model()) {
        _mask_recon_open = true;
        return;
    }
    launch_dataset_job();
}

bool GuiApp::launch_dataset_job() {
    // The first GPU-consuming operation of this session freezes the one native
    // choice, before any preview, decode or child is dispatched. A rejected or
    // conflicting request is reported and the run does not start.
    if (native_work_busy()) return false;
    stop_inference_users();
    close_splat();
    if (!freeze_native_device()) return false;
    app::set_crash_note("building dataset " + _workspace);
    // The stamp this run leaves behind describes the settings on the screen
    // only when the run actually reconstructs; after one that keeps the model
    // it goes on describing whoever built it.
    if (_redo_model || !workspace_state().model) _built_workspace = _workspace;
    sync_dataset_jobs();
    save_run_stencils();
    const std::string stamp = run_log_stamp();
    const fs::path prep_log_file =
        open_run_log(_prep_log,
                     fs::path(planned_image_dir(_sources, _workspace,
                                                _photo_import)).parent_path().string(),
                     "prep", stamp);
    // The unregistered-images list, beside this log with the same stamp,
    // written by whichever engine runs (sfm/Pipeline.cpp or ColmapRunner).
    set_ss_env("UNREG_LOG", prep_log_file.empty()
                                ? std::string()
                                : (prep_log_file.parent_path() /
                                   ("unreg_" + stamp + ".log")).string());
    write_run_settings(_prep_log);
    // The preview holds a multi-gigabyte backbone; the run about to start
    // wants that VRAM for reconstruction.
    reset_dataset_preview(effective_engine() != Engine::BuiltIn);
    const RunFilms films{&_film_frames, &_film_masks, &_film_geometry};
    if (effective_engine() == Engine::BuiltIn) _sfm.start(_sfm_job, films);
    else                                      _colmap.start(_colmap_job, films);
    // One run each: a re-do that stayed armed would throw the same step away
    // again the next time the button is pressed.
    _redo_frames = _redo_masks = _redo_model = _redo_geometry = false;
    return true;
}

// ---------------------------------------------------------------------------
// Source, destination, resume
// ---------------------------------------------------------------------------

namespace {

// Built by hand rather than with ui::Combo for the same reason as the lens
// pickers: what each row costs is the whole of the question, and one tooltip
// on the closed combo cannot answer it row by row.
void photo_import_combo(PhotoImport* mode, bool several_inputs) {
    const std::vector<const Msg*> labels{
        &dmsg::photo_import_convert, &dmsg::photo_import_copy,
        &dmsg::photo_import_move, &dmsg::photo_import_inplace};
    const std::vector<const Msg*> helps{
        &dmsg::photo_import_convert_help, &dmsg::photo_import_copy_help,
        &dmsg::photo_import_move_help, &dmsg::photo_import_inplace_help};
    int idx = (int)*mode;
    ImGui::SetNextItemWidth(px(260.0f));
    if (ui::BeginCombo(dmsg::photo_import, labels[(size_t)idx]->get())) {
        for (int i = 0; i < kNumPhotoImports; i++) {
            const bool blocked =
                several_inputs && (PhotoImport)i == PhotoImport::InPlace;
            ImGui::BeginDisabled(blocked);
            if (ui::Selectable(*labels[(size_t)i], i == idx))
                *mode = (PhotoImport)i;
            ImGui::EndDisabled();
            ui::help_on_hover(*helps[(size_t)i]);
        }
        ImGui::EndCombo();
    }
    ui::help_on_hover(dmsg::photo_import_help);
}

}  // namespace

// What the sensors of this input hold, beside the row that chose it. The
// reconstruction uses them without being asked (SfM's sensor and attitude
// gauges), so what is worth seeing here is whether there is anything to use.
void GuiApp::draw_sensor_badge(const PrepInput& s) {
    std::error_code ec;
    if (s.path.empty() ||
        (s.is_video ? !fs::is_regular_file(s.path, ec)
                    : !fs::is_directory(s.path, ec)))
        return;
    const TelemetryInfo t = _telemetry.get(s.path, s.is_video);
    if (!t.done) {
        ImGui::SameLine();
        ui::TextDisabled(dmsg::sensors_reading);
        return;
    }
    if (!s.is_video) {
        if (t.photos == 0) return;
        ImGui::SameLine();
        const long long n = t.photos, gps = t.with_gps, att = t.with_attitude;
        if (gps == 0 && att == 0) ui::TextDisabled(dmsg::sensors_none);
        else if (att == 0) ui::TextDisabled(dmsg::sensors_photo_gps, {gps, n});
        else if (gps == 0) ui::TextDisabled(dmsg::sensors_photo_attitude, {att, n});
        else if (gps == att) ui::TextDisabled(dmsg::sensors_photo_gps_attitude, {gps, n});
        else ui::TextDisabled(dmsg::sensors_photo_gps_attitude_split, {gps, att, n});
        if (att > 0) ui::help_on_hover(dmsg::sensors_photo_attitude_help);
        return;
    }
    ImGui::SameLine();
    const bool imu = t.gyro || t.accel || t.attitude;
    if (!imu && !t.gps) { ui::TextDisabled(dmsg::sensors_none); return; }
    ui::TextDisabled(imu && t.gps ? dmsg::sensors_imu_gps
                     : imu        ? dmsg::sensors_imu
                                  : dmsg::sensors_gps);
    if (!t.carrier.empty() && ImGui::IsItemHovered())
        ui::SetTooltip(dmsg::sensors_carrier_tooltip, {t.carrier});
}

namespace {

// The rate box's text, with the unit; "0 fps" is every frame. A row following
// the one above shows the caret the lens column uses for the same idea.
std::string fps_label(float fps) {
    char b[32];
    std::snprintf(b, sizeof b, "%g fps", (double)std::max(fps, 0.0f));
    return b;
}

// And back: a number is a rate, with or without the unit typed back, and 0 is
// every frame. Nothing readable is nullopt.
std::optional<float> parse_fps(const std::string& text) {
    const char* first = text.c_str();
    char* end = nullptr;
    const double v = std::strtod(first, &end);
    if (end == first || !(v >= 0.0)) return std::nullopt;
    return (float)v;
}

// The row whose box holds the dataset's own rate: it has nothing above it to
// follow, so it is never a caret.
size_t first_video_row(const std::vector<PrepInput>& sources) {
    for (size_t i = 0; i < sources.size(); i++)
        if (sources[i].is_video) return i;
    return sources.size();
}

}  // namespace

void GuiApp::draw_dataset_source() {
    // One row per input. Several videos reconstruct as one scene -- each gets
    // its own folder of frames under images/, and so its own camera.
    int remove = -1;
    bool edited = false;
    if (_source_path_edits.size() != _sources.size()) {
        const size_t old = _source_path_edits.size();
        _source_path_edits.resize(_sources.size());
        for (size_t i = old; i < _sources.size(); i++)
            _source_path_edits[i] = _sources[i].path;
    }
    // The rate lives beside the video it describes rather than in the settings
    // below, so a list of clips reads as the one decision it usually is.
    bool any_video = false;
    for (const PrepInput& s : _sources) any_video = any_video || s.is_video;
    any_video = true;  // TODO: "Shot in order" button
    // The path box takes what the rows leave, measured on the frame before: a
    // translated label or a badge still being read has no width until drawn.
    // Below a dozen characters, the label and badge get a line of their own.
    const float avail = ImGui::GetContentRegionAvail().x;
    const float min_path = ImGui::GetFontSize() * 12.0f;
    const float controls_w =
        _source_controls_w > 0.0f ? _source_controls_w : px(any_video ? 250.0f : 160.0f);
    const float info_w =
        _source_info_w > 0.0f ? _source_info_w : px(any_video ? 340.0f : 240.0f);
    const bool one_line = avail - controls_w - info_w >= min_path;
    const float path_w =
        std::max(min_path, avail - controls_w - (one_line ? info_w : 0.0f));
    float row_controls = 0.0f, row_info = 0.0f;
    for (size_t i = 0; i < _sources.size(); i++) {
        PrepInput& s = _sources[i];
        ImGui::PushID((int)i);
        ImGui::SetNextItemWidth(path_w);
        std::string& path_edit = _source_path_edits[i];
        ui::InputTextRaw("##in", &path_edit);
        const float path_right = ImGui::GetItemRectMax().x;
        // A typed path is only worth resolving once it is finished -- rewriting
        // `<folder>` to `<folder>/images` under the cursor is not helpful.
        if (ImGui::IsItemDeactivatedAfterEdit() && path_edit != s.path)
            replace_source(i, path_edit);
        ImGui::SameLine();
        if (ui::Button(dmsg::browse)) {
            _pick_source = (int)i;
            if (s.is_video)
                open_pick(PickAction::SourceReplace, msg::pick_video_file.get(),
                          FileDialog::Mode::File, video_dialog_filters());
            else
                open_pick(PickAction::SourceReplace,
                          msg::pick_photo_folder.get(),
                          FileDialog::Mode::Folder);
        }
        ImGui::SameLine();
        if (ui::Button(dmsg::remove)) remove = (int)i;
        {
            ImGui::SameLine();
            if (s.is_video) {
                ImGui::BeginDisabled(dataset_locked(Stage::Frames));
                ImGui::SetNextItemWidth(px(84.0f));
                // The first video's box IS the dataset's rate -- that is what a
                // preset carries and what every item of a batch starts from --
                // so its own `fps` stays 0 and the rest follow it.
                const bool head = i == first_video_row(_sources);
                const float above =
                    head ? 0.0f
                         : input_fps(_sources, _sfm_job.prep.video_fps, i - 1);
                if (_fps_text.size() != _sources.size()) {
                    _fps_text.assign(_sources.size(), std::string());
                    _fps_editing = -1;
                }
                std::string& text = _fps_text[i];
                if (_fps_editing != (int)i)
                    text = head ? fps_label(_sfm_job.prep.video_fps)
                                : s.fps == 0.0f ? std::string("^") : fps_label(s.fps);
                ui::InputTextRaw("##fps", &text);
                if (ImGui::IsItemActive()) _fps_editing = (int)i;
                else if (_fps_editing == (int)i) _fps_editing = -1;
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    const std::optional<float> v = parse_fps(text);
                    // A head row cannot be a caret: an unreadable answer there
                    // leaves the rate where it was rather than at nothing.
                    // Below it, that answer or the rate above is the caret.
                    if (head) {
                        if (v) _sfm_job.prep.video_fps = *v;
                    } else {
                        s.fps = !v || *v == above ? 0.0f
                                : *v > 0.0f       ? *v
                                                  : kFpsEveryFrame;
                    }
                    edited = true;
                }
                ui::help_on_hover(head ? (_sfm_job.prep.adaptive_fps
                                              ? dmsg::frames_per_second_help_adaptive
                                              : dmsg::frames_per_second_help)
                                       : dmsg::video_fps_this_one_help);
                ImGui::EndDisabled();
            } else {
                ui::Checkbox(dmsg::frames_in_order, &s.sequential);
                ui::help_on_hover(dmsg::frames_in_order_help);
            }
        }
        row_controls = std::max(row_controls, ImGui::GetItemRectMax().x - path_right +
                                                  ImGui::GetStyle().ItemSpacing.x);
        if (one_line) ImGui::SameLine();
        const float info_left = ImGui::GetCursorScreenPos().x;
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
        if (s.pano360_unsupported) {
            ImGui::SameLine();
            ui::TextColored(kWarn, dmsg::pano360_unsupported);
            ui::help_on_hover(dmsg::pano360_unsupported_help);
        }
        draw_sensor_badge(s);
        row_info = std::max(row_info, ImGui::GetItemRectMax().x - info_left);
        ImGui::PopID();
    }
    if (!_sources.empty()) {
        _source_controls_w = row_controls;
        _source_info_w = row_info;
    }
    if (remove >= 0 && !native_work_busy()) {
        close_native_previews();
        close_splat();
        _sources.erase(_sources.begin() + remove);
        _source_path_edits.erase(_source_path_edits.begin() + remove);
        mark_source_metadata_dirty();
        edited = true;
    }
    if (edited) refresh_sources();

    if (ui::Button(dmsg::add_video)) {
        open_pick(PickAction::SourceVideo, msg::pick_videos.get(),
                  FileDialog::Mode::File, video_dialog_filters(), "",
                  /*multi_select=*/true);
    }
    ui::help_on_hover(dmsg::add_video_help);
    ImGui::SameLine();
    if (ui::Button(dmsg::add_photos)) {
        open_pick(PickAction::SourceImages, msg::pick_photo_folder.get(),
                  FileDialog::Mode::Folder);
    }
    ui::help_on_hover(dmsg::add_photos_help);
    if (_sources.empty()) {
        ImGui::SameLine();
        ui::TextDisabled(dmsg::no_input_yet);
    }
    // A row of its own: a finished dataset is not raw input, and three button
    // labels in a row run off the edge of a narrow panel in several languages.
    if (ui::Button(dmsg::add_dataset)) {
        open_pick(PickAction::SourceDataset, msg::pick_existing_dataset.get(),
                  FileDialog::Mode::Folder);
    }
    ui::help_on_hover(dmsg::add_dataset_help);

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
        photo_import_combo(&_photo_import, _sources.size() > 1);
    }

    ImGui::SetNextItemWidth(px(-220.0f));
    ui::InputTextRaw("##ws", &_workspace);
    ImGui::SameLine();
    // Two "Browse..." buttons in one scope: the message supplies the ID, so
    // they need distinguishing exactly as two identical literals would.
    ImGui::PushID("ws");
    if (ui::Button(dmsg::browse)) {
        open_pick(PickAction::Workspace, msg::pick_output_folder.get(),
                  FileDialog::Mode::Folder);
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

// Two paragraphs rather than one message, because only the second one changes
// with the selection. `inherited` names the model too, for a row reading "same
// as above" -- the picker is no longer showing which lens that is.
void lens_tooltip(const Msg& model_help, const Msg* inherited = nullptr) {
    if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) ||
        !ImGui::BeginTooltip())
        return;
    ImGui::PushTextWrapPos(px(420.0f));
    ui::Text(dmsg::camera_lens_help);
    ImGui::Separator();
    if (inherited) ui::Text(*inherited);
    ui::Text(model_help);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

// ImGui's Combo has nowhere to hang a description on a row, so the popup is
// built by hand: which physical camera each model is for is the whole of the
// question. `inherit` adds "(same as above)" and makes -1 a selection.
bool sfm_lens_combo(const char* id, int* idx, bool inherit = false) {
    const auto labels = sfm_camera_model_labels();
    const auto helps = sfm_camera_model_helps();
    const char* shown = *idx < 0 ? dmsg::lens_same_as_above.get()
                                 : labels[(size_t)*idx]->get();
    if (!ui::BeginComboRaw(id, shown)) return false;
    bool changed = false;
    if (inherit) {
        if (ui::Selectable(dmsg::lens_same_as_above, *idx < 0)) {
            *idx = -1;
            changed = true;
        }
        ui::help_on_hover(dmsg::lens_same_as_above_help);
        ImGui::Separator();
    }
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

    // A model in the output folder is reused, so these settings reach it only
    // through a rebuild -- which a mismatch against the stamp THIS panel wrote
    // forces (ReconStamp.h). One it did not build is kept regardless.
    const WorkspaceState& prior = workspace_state();
    const bool reusing = !dataset_busy() && prior.model && !_redo_model;
    const bool inert =
        reusing && !(prior.recon_stamp && _workspace == _built_workspace);
    if (reusing)
        ui::TextColoredWrapped(inert ? kWarn : kDim,
                               inert ? dmsg::recon_reuse_locked
                                     : dmsg::recon_reuse_rebuild);

    // Everything down to the frame-rate control is read by feature extraction
    // and after; the frame settings below it were read the moment the run
    // began. Hence two guards rather than one round the lot.
    ImGui::BeginDisabled(inert || dataset_locked(Stage::Features));

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
            // Kept equal to the first row's by normalize_source_lenses, so
            // this reads the same lens whichever control last wrote it.
            const std::string& model = _sfm_job.camera_model;
            int idx = 0;
            for (int i = 0; i < kNumSfmCameraModels; i++)
                if (model == kSfmCameraModels[i]) idx = i;
            if (sfm_lens_combo(ui::detail::label(dmsg::camera_lens), &idx))
                apply_lens_to_sources(_sources, _sfm_job, kSfmCameraModels[idx]);
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
                              builtin ? _sfm_job.camera_model
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
        // The rate itself is a column of the input list, beside the video it
        // describes; what is left here is what it means for all of them.
        ImGui::BeginDisabled(dataset_locked(Stage::Frames));
        ui::Checkbox(dmsg::adaptive_fps, &_sfm_job.prep.adaptive_fps);
        ui::help_on_hover(dmsg::adaptive_fps_help);
        if (_sfm_job.prep.adaptive_fps &&
            all_videos_every_frame(_sources, _sfm_job.prep.video_fps))
            ui::TextColoredWrapped(kWarn, dmsg::adaptive_fps_every_frame);
        if (_sfm_job.prep.adaptive_fps) {
            ImGui::Indent();
            ImGui::SetNextItemWidth(px(220.0f));
            ui::SliderFloat(dmsg::adaptive_range, &_sfm_job.prep.adaptive_range,
                            1.0f, 16.0f, "%.1f");
            ui::help_on_hover(dmsg::adaptive_range_help);
            ImGui::Unindent();
        }
        ImGui::SetNextItemWidth(px(220.0f));
        ui::SliderInt(dmsg::sharpness_window, &_sfm_job.prep.sharp_window, 1, 8);
        ui::help_on_hover(dmsg::sharpness_window_help);
        bool any_multi = false;
        for (const PrepInput& s : _sources)
            any_multi = any_multi || (s.is_video && !s.pano360.valid() && s.video_tracks >= 2);
        if (any_multi) {
            ui::Checkbox(dmsg::sync_lenses, &_sfm_job.prep.sync_tracks);
            ui::help_on_hover(dmsg::sync_lenses_help);
        }
        if (any_pano360(_sources)) draw_pano360_options();
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

// What a 360 capture is unwrapped into, drawn among the video settings only
// when one was detected. The lens follows the choice exactly (apply_pano_lens),
// so there is no camera model to pick here.
void GuiApp::draw_pano360_options() {
    app::Pano360Options& p = _sfm_job.prep.pano;
    int mode = p.mode == app::Pano360Mode::Equirect ? 1 : 0;
    ImGui::SetNextItemWidth(px(220.0f));
    if (ui::Combo(dmsg::pano360_output, &mode,
                  {&dmsg::pano360_output_faces, &dmsg::pano360_output_equirect})) {
        p.mode = mode == 1 ? app::Pano360Mode::Equirect : app::Pano360Mode::Faces;
        // A face side and a panorama width are not the same number, so the one
        // in the box is meaningless the moment the other is chosen.
        reset_pano_size(_sources, _sfm_job.prep.pano);
        apply_pano_lens(_sources, _sfm_job, _colmap_job);
    }
    ui::help_on_hover(dmsg::pano360_output_help);
    if (mode == 1 && effective_engine() != Engine::BuiltIn)
        ui::TextColoredWrapped(kWarn, dmsg::pano360_colmap_warning);
}

// The size of those views, which is under Advanced because the default is
// derived from the source and is what almost everyone should use.
void GuiApp::draw_pano360_size() {
    ImGui::SetNextItemWidth(px(260.0f));
    ui::InputInt(dmsg::pano360_size, &_sfm_job.prep.pano.size, 32, 128);
    if (_sfm_job.prep.pano.size < 0) _sfm_job.prep.pano.size = 0;
    ui::help_on_hover(dmsg::pano360_size_help);
}


// One lens control for the whole capture has to reach every input:
// append_camera_overrides emits a --camera-model per input folder, and one
// still holding the model its file type suggested wins on longest prefix.
// The lens and starting focal an input's images are fitted with: its first
// camera group's, once "same as above" has been followed down the list.
void GuiApp::source_lens(size_t input, std::string& model, float& focal) const {
    const std::vector<CameraGroup> groups = camera_groups(_sources);
    const std::vector<std::string> models =
        camera_group_models(_sources, groups, _sfm_job.camera_model);
    for (size_t i = 0; i < groups.size(); i++) {
        if (groups[i].input != input) continue;
        model = models[i];
        focal = group_focal(_sources, groups[i]);
        return;
    }
}

// One lens per camera group, in place of the single "Camera / lens" control,
// once there is more than one camera to tell apart -- which lets a 360 clip and
// a phone clip reconstruct as one scene, neither fitted with the other's model.
void GuiApp::draw_source_cameras() {
    ui::Text(dmsg::camera_lens_per_input);
    ui::help_on_hover(dmsg::camera_lens_per_input_help);
    ImGui::Indent();
    const std::vector<CameraGroup> groups = camera_groups(_sources);
    const std::vector<std::string> models =
        camera_group_models(_sources, groups, _sfm_job.camera_model);
    const auto labels = sfm_camera_model_labels();
    const auto helps = sfm_camera_model_helps();

    // A row names the folder it measures, which is also the prefix the
    // override matches on -- so an input with several camera folders repeats
    // its own name, and that is what says which rows belong together.
    std::vector<std::string> names(groups.size());
    float col = px(200.0f);
    for (size_t i = 0; i < groups.size(); i++) {
        fs::path p(_sources[groups[i].input].path);
        if (p.filename().empty()) p = p.parent_path();   // trailing separator
        names[i] = groups[i].rel.empty() ? p.filename().string() : groups[i].rel;
        col = std::max(col, ImGui::CalcTextSize(names[i].c_str()).x + px(24.0f));
    }
    col = std::min(col, px(420.0f));

    // One letter per two rows, and never fewer than a row already holds.
    int letters = std::max<int>(1, (int)groups.size() / 2);
    for (const CameraGroup& g : groups)
        letters = std::max(letters, group_rig(_sources, g) - kRigFirstShared + 1);
    letters = std::min(letters, kRigShared);
    std::vector<std::string> letter_names;
    for (int l = 0; l < letters; l++) letter_names.push_back(rig_letter(l));

    bool edited = false;
    for (size_t i = 0; i < groups.size(); i++) {
        const CameraGroup& g = groups[i];
        const PrepInput& in = _sources[g.input];
        const std::string dir =
            g.sub < 0 ? in.path
                      : (fs::path(in.path) /
                         in.subcameras[(size_t)g.sub].rel).string();
        // The row's own prefix as its ID, so adding or dropping an input does
        // not carry an open picker or a half-typed focal onto another row.
        ImGui::PushID(g.rel.c_str());
        ui::TextRaw(names[i]);
        if (ImGui::IsItemHovered()) ui::SetTooltipRaw(dir);
        // A name too long for the column takes the row and leaves the controls
        // on the next one, rather than being drawn through them.
        if (ImGui::CalcTextSize(names[i].c_str()).x + px(8.0f) < col)
            ImGui::SameLine(col);
        ImGui::SetNextItemWidth(px(220.0f));
        std::string& stored = group_model(_sources, g);
        int idx = stored.empty() ? -1 : 0;
        for (int m = 0; m < kNumSfmCameraModels; m++)
            if (stored == kSfmCameraModels[m]) idx = m;
        if (sfm_lens_combo("##lens", &idx, /*inherit=*/i > 0)) {
            stored = idx < 0 ? std::string() : kSfmCameraModels[idx];
            edited = true;
        }
        int shown = 0;
        for (int m = 0; m < kNumSfmCameraModels; m++)
            if (models[i] == kSfmCameraModels[m]) shown = m;
        lens_tooltip(*helps[(size_t)shown],
                     idx < 0 ? labels[(size_t)shown] : nullptr);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(px(90.0f));
        ui::InputFloat(dmsg::focal_x_width, &group_focal(_sources, g), 0, 0,
                       "%.4g");
        ui::help_on_hover(dmsg::focal_x_width_help);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(px(150.0f));
        {
            // "This input's lenses" only means something for an input with
            // several: a lone photo folder or a one-lens video has none to rig.
            const bool multi = g.sub >= 0 || !lens_dirs(_sfm_job.prep, in).empty();
            int& rig = group_rig(_sources, g);
            if (!multi && rig == kRigOwn) rig = kRigNone;
            std::vector<const char*> items = {ui::detail::label(dmsg::rig_none)};
            std::vector<int> ids = {kRigNone};
            if (multi) {
                items.push_back(ui::detail::label(dmsg::rig_own));
                ids.push_back(kRigOwn);
            }
            for (int l = 0; l < letters; l++) {
                items.push_back(letter_names[(size_t)l].c_str());
                ids.push_back(kRigFirstShared + l);
            }
            int idx = (int)(std::find(ids.begin(), ids.end(), rig) - ids.begin());
            if (idx >= (int)ids.size()) idx = 0;
            if (ui::ComboRaw("##rig", &idx, items.data(), (int)items.size()))
                rig = ids[(size_t)idx];
            ui::help_on_hover(dmsg::rig_help);
        }
        draw_lens_warning(dir, in.is_video, models[i], /*builtin=*/true);
        ImGui::PopID();
    }
    draw_rig_kinds(groups, names);
    if (groups.size() >= 2) {
        if (ui::SmallButton(dmsg::rig_guess)) guess_source_rigs(_sources, /*force=*/true);
        ui::help_on_hover(dmsg::rig_guess_help);
    }
    ImGui::Unindent();
    // Only now: the edit is the row's own, and collapsing it into the row above
    // rewrites what the loop was holding references into.
    if (edited) {
        normalize_source_lenses(_sources, _sfm_job.camera_model);
        normalize_source_fps(_sources, _sfm_job.prep.video_fps);
    }
}

// Two photo folders on one rig may be the two lenses of one dual-fisheye
// camera, which a video's own file says for itself (SfmRunner::build_rigs).
// Offered per rig of exactly two folders; any row of it holding the flag counts.
void GuiApp::draw_rig_kinds(const std::vector<CameraGroup>& groups,
                            const std::vector<std::string>& names) {
    for (int id = kRigOwn; id < kRigFirstShared + kRigShared; id++) {
        // "own" is a rig per input, so it is asked per input; a letter once.
        const size_t n_inputs = id == kRigOwn ? _sources.size() : 1;
        for (size_t input = 0; input < n_inputs; input++) {
            std::vector<size_t> rows;
            bool video = false;
            for (size_t i = 0; i < groups.size(); i++) {
                const CameraGroup& g = groups[i];
                if (group_rig(_sources, g) != id) continue;
                if (id == kRigOwn && (g.input != input || g.sub < 0)) continue;
                video = video || _sources[g.input].is_video;
                rows.push_back(i);
            }
            if (video || rows.size() != 2) continue;
            bool dual = false;
            for (size_t i : rows) dual = dual || group_rig_dual_fisheye(_sources, groups[i]);
            fs::path own(_sources[input].path);
            if (own.filename().empty()) own = own.parent_path();
            const std::string rig_name =
                id == kRigOwn ? own.filename().string()
                              : rig_letter(id - kRigFirstShared);
            const std::string label =
                i18n::format(dmsg::rig_dual_fisheye, {rig_name, names[rows[0]], names[rows[1]]}) +
                "###rig_dual_fisheye_" + std::to_string(id) + "_" + std::to_string(input);
            if (ui::CheckboxRaw(label.c_str(), &dual))
                for (size_t i : rows) group_rig_dual_fisheye(_sources, groups[i]) = dual;
            ui::help_on_hover(dmsg::rig_dual_fisheye_help);
        }
    }
}

bool GuiApp::input_pixel_size(const std::string& path, bool is_video,
                              int& w, int& h) {
    w = h = 0;
    if (path.empty()) return false;
    auto it = _input_size.find(path);
    if (it == _input_size.end()) {
        if (is_video) return false;
        std::pair<int, int> size{0, 0};
        std::error_code ec;
        if (fs::is_directory(path, ec)) {
            int iw = 0, ih = 0;
            if (DatasetPrep::first_image_dims(path, iw, ih)) size = {iw, ih};
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
    // A 360 capture's lens describes the WARPED views, which are 2:1 (or square
    // faces) by construction; the frame this would measure is the packing.
    for (const PrepInput& s : _sources)
        if (s.pano360.valid() && s.path == path) return;
    // A dual-lens file is two fisheye circles per frame whatever its pixel
    // dimensions are, so this one needs no measurement.
    bool known_fisheye = is_dual_fisheye_path(path);
    for (const PrepInput& s : _sources)
        if (s.path == path) known_fisheye = known_fisheye || has_fisheye_lens(s);
    if (known_fisheye) {
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

PreviewSource GuiApp::preview_source(size_t input) const {
    PreviewSource src;
    if (input >= _sources.size()) return src;
    const PrepInput& in = _sources[input];
    src.input = in.path;
    src.is_video = in.is_video;
    src.ffmpeg_exe = _ffmpeg_exe;
    // In process where the driver can, ffmpeg where it cannot or where the job
    // said to -- the choice preparation itself makes.
    src.builtin_decode =
        !_sfm_job.prep.force_external_decode && backends().builtin_video;
    src.tracks = std::max(in.video_tracks, 1);
    src.look.packed_lenses = in.packed_lenses;
    // The one frozen choice, so a preview decodes and segments on the GPU the
    // run will use; empty leaves the panel's own precedence in charge.
    src.device = _native_device_uuid;
    src.image_gamut = _sfm_job.image_gamut;
    src.image_is_linear = _sfm_job.image_is_linear;
    src.look.auto_rotate = _sfm_job.prep.auto_rotate;
    if (in.pano360.valid() && _sfm_job.prep.pano.mode != app::Pano360Mode::Off) {
        src.look.eac = in.pano360;
        src.look.views = app::pano360_views(in.pano360, _sfm_job.prep.pano);
    }
    return src;
}

// ---------------------------------------------------------------------------
// The one native GPU choice
//
// A session-level choice: freeze one canonical UUID before native work and
// pass it to every native job and child. No persisted preference.
// ---------------------------------------------------------------------------

void GuiApp::load_native_devices() {
    if (_native_devices_loaded) return;
    _native_devices.clear();
#if defined(SS_BUILD_SAM)
    // The inference layer's own side-effect-free enumeration.
    for (const nn::DeviceInfo& d : nn::list_devices()) {
        NativeDeviceRow r;
        r.name = d.name;
        r.type = d.type;
        r.uuid = d.uuid;
        r.vram_bytes = d.vram_bytes;
        r.usable = d.usable;
        _native_devices.push_back(std::move(r));
    }
#elif defined(SS_TOOL_SFM)
    // No inference layer, but SfM carries its own Vulkan context: a listing
    // that creates nothing. A build with neither has no native GPU work to
    // choose a device for, and an empty list is the honest answer.
    for (const VkDeviceRecord& d : VkContext::listDevices()) {
        NativeDeviceRow r;
        r.name = d.name;
        r.type = d.type;
        r.uuid = spirula::vkselect::selectorFor(d);
        r.vram_bytes = d.vram_bytes;
        r.usable = d.usable;
        _native_devices.push_back(std::move(r));
    }
#elif defined(SS_BACKEND_VULKAN)
    // Engine-only Vulkan build: the backend's own enumeration, which is also
    // side-effect free. Its identity helpers exist only on this backend, so
    // the whole branch is Vulkan-guarded.
    for (int i = 0; i < backend::device_count(); i++) {
        const backend::DeviceInfo d = backend::device_info(i);
        NativeDeviceRow r;
        r.name = d.name;
        r.type = d.type;
        r.vram_bytes = d.vram_bytes;
        r.usable = d.usable;
        r.uuid = d.uuid;
        _native_devices.push_back(std::move(r));
    }
#endif
    _native_devices_loaded = true;
}

void GuiApp::draw_device_picker(bool as_menu) {
    load_native_devices();

    const bool frozen = _native_device_frozen;
    const bool no_devices = std::none_of(
        _native_devices.begin(), _native_devices.end(),
        [](const NativeDeviceRow& d) { return d.usable && !d.uuid.empty(); });
    const bool disabled = frozen || no_devices;

    auto row_label = [](const NativeDeviceRow& d, size_t index, bool with_id,
                         char* buf, size_t n) {
        char suffix[64] = {};
        if (with_id)
            std::snprintf(suffix, sizeof suffix, "##native_device_%zu", index);
        std::snprintf(buf, n, "%s [%zu]%s", d.name.c_str(), index, suffix);
    };

    const bool auto_sel =
        !frozen && _native_device_choice_set && _native_device_request.empty();
    const char* env_sel = spirula::env("VK_DEVICE");
    const bool inherited = !frozen && !_native_device_choice_set &&
                           env_sel && env_sel[0];

    size_t selected = _native_devices.size();
    const std::string* selected_selector =
        frozen ? &_native_device_uuid
               : !_native_device_request.empty() ? &_native_device_request
                                                 : nullptr;
    if (selected_selector && !selected_selector->empty()) {
        for (size_t i = 0; i < _native_devices.size(); ++i) {
            if (_native_devices[i].uuid == *selected_selector) {
                selected = i;
                break;
            }
        }
    } else if (inherited) {
        for (size_t i = 0; i < _native_devices.size(); ++i) {
            if (_native_devices[i].uuid == env_sel) {
                selected = i;
                break;
            }
        }
    }

    char selected_label[400];
    const char* shown = nullptr;
    if (selected < _native_devices.size()) {
        row_label(_native_devices[selected], selected, false, selected_label,
                  sizeof selected_label);
        shown = selected_label;
    } else if (frozen) {
        shown = !_native_device_name.empty() ? _native_device_name.c_str()
                                              : msg::no_device_found.get();
    } else if (selected_selector) {
        shown = msg::no_device_found.get();
    } else {
        shown = msg::device_auto.get();
    }

    if (as_menu) {
        ImGui::BeginDisabled(disabled);
        if (ui::MenuItem(msg::device_auto, nullptr, auto_sel)) {
            _native_device_choice_set = true;
            _native_device_request.clear();
        }
        ui::help_on_hover(msg::device_auto_help);
        for (size_t i = 0; i < _native_devices.size(); ++i) {
            const NativeDeviceRow& d = _native_devices[i];
            char label[400];
            row_label(d, i, true, label, sizeof label);
            const bool sel = !_native_device_request.empty() &&
                             _native_device_request == d.uuid;
            ImGui::BeginDisabled(!d.usable || d.uuid.empty());
            if (ui::MenuItemRaw(label, sel) && !d.uuid.empty()) {
                _native_device_choice_set = true;
                _native_device_request = d.uuid;
            }
            ImGui::EndDisabled();
            ui::help_on_hover_raw(d.uuid.empty() ? "" : d.uuid.c_str());
        }
        ImGui::EndDisabled();
    } else {
        ImGui::BeginDisabled(disabled);
        ImGui::SetNextItemWidth(px(220.0f));
        if (ui::BeginCombo(msg::menu_device,
                           no_devices ? msg::no_device_found.get() : shown)) {
            if (ui::Selectable(msg::device_auto, auto_sel)) {
                _native_device_choice_set = true;
                _native_device_request.clear();
            }
            ui::help_on_hover(msg::device_auto_help);
            for (size_t i = 0; i < _native_devices.size(); ++i) {
                const NativeDeviceRow& d = _native_devices[i];
                char label[400];
                row_label(d, i, true, label, sizeof label);
                const bool sel = !_native_device_request.empty() &&
                                 _native_device_request == d.uuid;
                ImGui::BeginDisabled(!d.usable || d.uuid.empty());
                if (ui::SelectableRaw(label, sel) && !d.uuid.empty()) {
                    _native_device_choice_set = true;
                    _native_device_request = d.uuid;
                }
                if (sel) ImGui::SetItemDefaultFocus();
                ImGui::EndDisabled();
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
    }

    if (!_native_device_error.empty())
        ui::TextColoredWrappedRaw(kErr, _native_device_error);
}

bool GuiApp::freeze_native_device() {
#if !defined(SS_BUILD_SAM) && !defined(SS_TOOL_SFM) && !defined(SS_BACKEND_VULKAN)
    _native_device_frozen = true;
    return true;
#else
    if (_native_device_frozen) {
        // Already frozen: the request must name the same device or the app has
        // to be restarted. This is the invariant, not a new winner.
        if (_native_device_request.empty() || _native_device_uuid.empty()) return true;
        if (_native_device_request == _native_device_uuid) return true;
        _native_device_error = spirula::i18n::format(
            msg::device_conflict, {_native_device_request, _native_device_uuid});
        return false;
    }
    load_native_devices();

    const bool explicit_set = _native_device_choice_set;
    const spirula::vkselect::Request req =
        spirula::vkselect::requestFrom(_native_device_request, explicit_set);

    std::string uuid, name;
    const std::string requested = req.text.empty() ? "auto" : req.text;
    std::string diagnostic;
    const Msg* detail = nullptr;
#if defined(SS_BUILD_SAM)
    spirula::vkselect::Resolution res;
    if (req.kind == spirula::vkselect::Request::Kind::Malformed) {
        detail = &msg::device_detail_malformed;
    } else {
        try {
            res = nn::vk::Context::resolveSelector(req);
        } catch (const std::exception&) {
            detail = &msg::device_detail_no_device;
        }
    }
    if (!detail && !res.ok()) {
        detail = &device_detail(res.status);
    } else if (!detail) {
        std::string live = nn::configured_device_selector();
        if (live.empty()) live = nn::current_device_selector();
        if (!live.empty() && live != res.selector) {
            _native_device_error = spirula::i18n::format(
                msg::device_conflict, {res.selector, live});
            log(_native_device_error);
            return false;
        }
#ifdef SS_BACKEND_VULKAN
        if (!backend::device_select_identity(res.selector.c_str())) {
            diagnostic = backend::device_selection_error();
            if (diagnostic.empty()) detail = &msg::device_unusable_native;
        }
#endif
        if (!detail && diagnostic.empty()) {
            try {
                nn::configure_device(res.selector);
            } catch (const std::exception& e) {
                _native_device_error = spirula::i18n::format(
                    msg::device_error, {requested, e.what()});
                log(_native_device_error);
                return false;
            }
            uuid = nn::configured_device_selector();
        }
    }
#elif defined(SS_TOOL_SFM)
    const spirula::vkselect::Resolution res = VkContext::resolveSelector(req);
    if (!res.ok()) detail = &device_detail(res.status);
    else           uuid = res.selector;
#elif defined(SS_BACKEND_VULKAN)
    if (req.kind == spirula::vkselect::Request::Kind::Malformed) {
        detail = &msg::device_detail_malformed;
    } else if (!backend::device_select_identity(requested.c_str())) {
        diagnostic = backend::device_selection_error();
        if (diagnostic.empty()) detail = &msg::device_unusable_native;
    } else {
        uuid = backend::device_current_selector();
    }
#endif
#if defined(SS_BACKEND_VULKAN)
    if (!detail && !uuid.empty() &&
        !backend::device_select_identity(uuid.c_str())) {
        diagnostic = backend::device_selection_error();
        if (diagnostic.empty()) detail = &msg::device_unusable_native;
    }
#endif
    if (!detail && !diagnostic.empty()) {
        _native_device_error = spirula::i18n::format(
            msg::device_error, {requested, diagnostic});
        log(_native_device_error);
        return false;
    }
    if (!detail && uuid.empty())
        detail = &msg::device_detail_no_device;
    if (detail) {
        _native_device_error = spirula::i18n::format(
            msg::device_error, {requested, detail->get()});
        log(_native_device_error);
        return false;
    }

    _native_device_uuid = uuid;
    // The driver's name for the resolved device, for the display line and the
    // startup log. Falls back to the UUID when a listing is unavailable.
    for (const NativeDeviceRow& d : _native_devices)
        if (d.uuid == uuid) { name = d.name; break; }
    _native_device_name = name.empty() ? uuid : name;
    _native_device_frozen = true;
    _native_device_error.clear();
    propagate_frozen_device();
    // The actual resolved identity at workload start, through the log the
    // screen already shows -- not only the argv a child was handed.
    log(spirula::i18n::format(
        msg::device_frozen_at, {_native_device_name + " [" + uuid + "]"}));
    return true;
#endif
}
bool GuiApp::freeze_cuda_device() {
#ifdef SS_BACKEND_VULKAN
    return true;
#else
    if (_cuda_device_locked) return true;
    const int n = backend::device_count();
    const int current = backend::device_current();
    if (n <= 0 || current < 0 || current >= n ||
        !backend::device_select(current)) {
        log(msg::no_device_found.get());
        return false;
    }
    _cuda_device_index = current;
    _cuda_device_locked = true;
    return true;
#endif
}

void GuiApp::propagate_frozen_device() {
    const std::string& uuid = _native_device_uuid;
    // Copy the frozen UUID into every native job and child launch field.
    _sfm_job.prep.device = uuid;
    _colmap_job.device = uuid;
    _sfm_job.device_selector = uuid;
    _sfm_job.geometry.device_uuid = uuid;
    _colmap_job.geometry.device_uuid = uuid;
    _geometry.device_uuid = uuid;
    _mesh_job.device_uuid = uuid;
}

void GuiApp::open_mask_preview() {
    if (native_work_busy()) return;
    if (_sources.empty()) {
        log(dmsg::mask_pick_input_first.get());
        return;
    }
    pump_source_probes();
    if (!_source_probes_ready) {
        log(dmsg::sensors_reading.get());
        return;
    }
    // A preview decodes and segments on the GPU, so it is a GPU-consuming
    // operation like the run itself: it freezes the same one choice first.
    // The other preview owns the same process-wide inference pool.
    stop_inference_users();
    close_splat();
    if (!freeze_native_device()) return;
    _segment.open(preview_source((size_t)_mask_preview_input),
                  _mask_enable ? selected_model_path() : "");
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
    if (_border_enable) {
        ImGui::Indent();
        ImGui::SetNextItemWidth(px(240.0f));
        const std::string shown =
            _frame_shapes.empty() ? dmsg::stencil_areas_per_input.get() : _frame_shapes;
        if (ui::BeginCombo(dmsg::stencil_areas_preset, shown.c_str())) {
            if (ImGui::IsWindowAppearing()) _frame_shapes_list = list_stencil_presets();
            if (ui::Selectable(dmsg::stencil_areas_per_input, _frame_shapes.empty()))
                _frame_shapes.clear();
            for (const StencilPreset& p : _frame_shapes_list)
                if (ui::SelectableRaw(p.name, p.name == _frame_shapes)) {
                    _frame_shapes = p.name;
                    apply_frame_shapes();
                }
            ImGui::EndCombo();
        }
        ui::help_on_hover(dmsg::stencil_areas_preset_help);
        ImGui::Unindent();
    }

    // Asked wherever the dataset ends up with masks at all, including ones
    // that arrived with the photographs: what it decides is the reconstruction,
    // not whether they are written.
    if (_mask_enable || _border_enable || with_masks > 0) {
        ui::Checkbox(dmsg::mask_for_features, &_mask_features);
        ui::help_on_hover(dmsg::mask_for_features_help);
    }
    if (!_mask_enable && !_border_enable) return;

    ImGui::Indent();

    const ModelEntry* entry = find_model(_model_id);
    const bool builtin_masking = backends().builtin_masking;

    if (_mask_enable && builtin_masking) {
        draw_mask_model_picker(_model_id, _download,
                               [this] { request_model_download(_model_id); });
        entry = find_model(_model_id);
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

    const bool mask_preview_busy = native_work_busy();
    const bool mask_preview_blocked = mask_preview_busy || !_source_probes_ready;
    ImGui::BeginDisabled(mask_preview_blocked);
    if (ui::Button(dmsg::mask_try)) open_mask_preview();
    ImGui::EndDisabled();
    ui::help_on_hover_disabled(
        mask_preview_busy ? dmsg::step_locked
        : !_source_probes_ready ? dmsg::sensors_reading : dmsg::mask_try_help);
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
        draw_margin_slider(_mask.dilate_ratio, _mask.shrink_ratio, keep_subject, px(220.0f),
                           /*inline_label=*/true);

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
    // The preview loads a depth backbone, so it is a GPU-consuming operation
    // and freezes the one choice before the panel starts.
    if (native_work_busy()) return;
    pump_source_probes();
    if (!_source_probes_ready) {
        log(dmsg::sensors_reading.get());
        return;
    }
    // One multi-gigabyte backbone at a time: the mask preview holds SAM and
    // this one holds Metric3D, and the inference layer's pool is process-wide.
    stop_inference_users();
    close_splat();
    if (!freeze_native_device()) return;
    const size_t idx =
        _sources.empty() ? 0
                         : std::min((size_t)_mask_preview_input,
                                    _sources.size() - 1);
    std::string lens = _sfm_job.camera_model;
    float focal = 0.0f;
    if (!_sources.empty()) source_lens(idx, lens, focal);
    _geometry_panel.open(preview_source(idx), _workspace,
                         planned_image_dir(_sources, _workspace, _photo_import),
                         lens, focal);
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
    const bool geometry_preview_busy = native_work_busy();
    const bool geometry_preview_blocked =
        geometry_preview_busy || !_source_probes_ready || geometry_model_missing();
    ImGui::BeginDisabled(geometry_preview_blocked);
    if (ui::Button(dmsg::geom_try)) open_geometry_preview();
    ImGui::EndDisabled();
    ui::help_on_hover_disabled(
        geometry_preview_busy ? dmsg::step_locked
        : !_source_probes_ready ? dmsg::sensors_reading
        : geometry_model_missing() ? dmsg::geom_model_first : dmsg::geom_try_help);

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

    // A step that can say how far through it is gets a bar; one that cannot
    // (the mapper counts registrations, not a total) gets its own sentence.
    if (any) {
        const StageProgress p = prog->stage(running);
        if (p.fraction >= 0.0f)
            ui::ProgressBarRaw(p.fraction, ImVec2(-1, 0),
                               p.detail.empty() ? nullptr : p.detail.c_str());
        else if (!p.detail.empty())
            ui::TextDisabledRaw(p.detail);
    }
}

// How much the view changed along every input, as the adaptive pass measures
// it. One row per input in the order they were given, so a folder of photos
// among the clips is a row that says it has no motion rather than a gap.
void GuiApp::draw_scan_view(float h) {
    const std::vector<ScanRow> rows = dataset_steps()->scan();
    ImGui::BeginChild("##scanview", ImVec2(0, h));
    // One scale over all of them: a clip that moves twice as much as its
    // neighbour has to LOOK it, since that is what took the frames off it.
    float top = 0.0f;
    for (const ScanRow& r : rows)
        for (float v : r.speed) top = std::max(top, v);

    const float band = px(38.0f), strip = px(12.0f);
    const ImU32 back = ImGui::GetColorU32(ImGuiCol_FrameBg);
    const ImU32 ink = ImGui::GetColorU32(ImGuiCol_PlotHistogram);
    const ImU32 keep = ImGui::GetColorU32(kOk);
    const ImU32 veil = ImGui::GetColorU32(ImGuiCol_WindowBg, 0.55f);
    for (const ScanRow& r : rows) {
        ui::TextRaw(r.name);
        ImGui::SameLine();
        if (!r.video)
            ui::TextDisabled(dmsg::scan_photos, {(long long)r.frames});
        else if (r.kept_n > 0)
            ui::TextDisabled(dmsg::scan_kept_frames, {(long long)r.kept_n});
        else
            ui::TextDisabledRaw("");

        const float w = ImGui::GetContentRegionAvail().x;
        const float tall = band + (r.kept.empty() ? 0.0f : strip + px(2.0f));
        if (w < px(48.0f)) break;
        const ImVec2 at = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(at, ImVec2(at.x + w, at.y + band), back);
        const float step = w / (float)kScanSlices;
        for (size_t i = 0; i < r.speed.size(); i++) {
            if (r.hits[i] <= 0 || top <= 0.0f) continue;
            const float v = std::min(1.0f, r.speed[i] / top);
            const float x0 = at.x + step * (float)i;
            dl->AddRectFilled(ImVec2(x0, at.y + band * (1.0f - v)),
                              ImVec2(x0 + std::max(step - 1.0f, 1.0f), at.y + band),
                              ink);
        }
        // What is not measured yet, dimmed rather than left blank: an empty
        // stretch of a slow clip looks the same as one nobody has reached.
        if (r.video && r.done < 1.0f)
            dl->AddRectFilled(ImVec2(at.x + w * r.done, at.y),
                              ImVec2(at.x + w, at.y + band), veil);
        if (!r.kept.empty()) {
            const float y = at.y + band + px(2.0f);
            dl->AddRectFilled(ImVec2(at.x, y), ImVec2(at.x + w, y + strip), back);
            for (size_t i = 0; i < r.kept.size(); i++) {
                const float v = std::min(1.0f, std::max(0.0f, r.kept[i]));
                if (v <= 0.0f) continue;
                const float x0 = at.x + step * (float)i;
                // The slowest stretch still keeps frames, so it still gets a
                // mark: a strip that thins to nothing reads as a gap.
                const float tall = std::max(strip * v, px(2.0f));
                dl->AddRectFilled(ImVec2(x0, y + strip - tall),
                                  ImVec2(x0 + std::max(step - 1.0f, 1.0f), y + strip),
                                  keep);
            }
        }
        ImGui::Dummy(ImVec2(w, tall));
        ImGui::Spacing();
    }
    ImGui::EndChild();
}

// Which of the three previews the running step implies. Frames, masks and
// feature extraction all show pictures, so they share one.
int GuiApp::preview_for_stage() {
    // A finished run keeps showing what its last step was on: the model is
    // what somebody was watching when it ended, and falling back to the frames
    // would hide it behind a click nobody knows to make.
    if (!dataset_busy()) return _preview_last_stage;
    switch (dataset_steps()->current()) {
        // Nothing is written while the motion is being measured, so the reel
        // has nothing to show and the curves have everything.
        case Stage::Frames:    return dataset_steps()->scanning() ? 6 : 0;
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
                        _sfm.features_dir(), &_film_features,
                        _sfm.thumbs_dir());
    _pairs_view.configure(_sfm.sfm_image_dir(), _sfm.sfm_mask_dir(),
                          _sfm.features_dir(), _sfm.matches_path(),
                          _sfm.live_matches_path());

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
    #if 0
        // Each snapshot is framed on its own cameras, and the last is re-gauged
        // as well; carrying the camera along keeps the picture still through
        // both, where re-framing would jump.
        float moved[12];
        if (_model_attached && snapshot_motion(_live_model, lm, moved))
            _model_view.move_view(moved);
    #endif
        _live_model = std::move(lm);
    #if 1
        // The mapper's frame is the seed pair's, upside down as often as not:
        // navigate about the cameras' up until the model is levelled.
        float up[3] = {0, 0, 1};
        if (!_live_model.ds.gauge_oriented) snapshot_up(_live_model, up);
        _model_view.set_nav_up(up);
    #endif
        // The mapper's own output is a wall of per-registration detail, so the
        // default log used to go quiet for the longest step. These are the
        // model in hand, not the bar -- a seed retry starts one over.
        if (dataset_busy() && _live_model.n_images) {
            RunProgress& p = _sfm.steps();
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
void GuiApp::reset_dataset_preview(bool sweep) {
    close_native_previews();
    _features.stop();
    if (sweep) _sfm.sweep_intermediates();
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
    // Null where the view is not a reel: the match map, the model, the motion.
    FilmReel* reels[7] = {&_film_frames, &_film_masks, &_film_features,
                          nullptr, nullptr, &_film_geometry, nullptr};
    const bool avail[7] = {_film_frames.has_frames(), _film_masks.has_frames(),
                           _film_features.has_frames(), !_matrix.empty(),
                           _model_attached, _film_geometry.has_frames(),
                           !dataset_steps()->scan().empty()};
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
        for (int i = 0; i < 7; i++)
            if (avail[i]) { tab = i; break; }
    }

    const Msg* names[7] = {&dmsg::view_frames, &dmsg::view_masks,
                           &dmsg::view_features, &dmsg::view_matrix,
                           &dmsg::view_model, &dmsg::view_geometry,
                           &dmsg::view_motion};
    bool first = true;
    for (int i = 0; i < 7; i++) {
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
    if (tab == 6) ui::help_on_hover(dmsg::frame_spacing_help);

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
    if (tab == 6) {
        draw_scan_view(h - px(8.0f));
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
        PairBlock block;
        // Grouped so the pair view sits beside the map rather than beside the
        // legend under it.
        ImGui::BeginGroup();
        if (_matrix.draw(side, block)) _pairs_view.show(block);
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

GuiApp::DatasetFolders GuiApp::workspace_folders(const WorkspaceState& prior) const {
    DatasetFolders f;
    f.dir = _workspace;
    f.image_dir = planned_image_dir(_sources, _workspace, _photo_import);
    std::error_code ec;
    if (prior.masks) {
        // The run's own masks/, which every branch of DatasetPrep writes in
        // the app's convention -- only a bundled folder is ever left flipped.
        f.mask_dir = (fs::path(_workspace) / "masks").string();
    } else if (prior.input_masks && _sources.size() == 1 && !_sources[0].is_video &&
               _sources[0].packed_lenses == 0) {
        // Masks that came with the photos pair with them where they lie.
        f.image_dir = fs::absolute(_sources[0].path, ec).string();
        f.mask_dir = fs::absolute(_sources[0].mask_dir, ec).string();
        f.mask_flipped = _flip_found_masks;
    }
    return f;
}

void GuiApp::draw_dataset_open_buttons(const DatasetFolders& f, bool model) {
    if (model) {
        if (ui::Button(dmsg::open_in_trainer)) {
            if (training_busy()) {
                _pending = Pending::OpenDataset;
                _pending_path = f.dir;
                _open_confirm = true;
            } else {
                open_dataset(f.dir, f.image_dir, f.mask_dir, f.mask_flipped,
                             /*keep_log=*/true);
            }
        }
        // Cleaning the seed cloud belongs here rather than after training: a
        // floater removed now is one the run never fits to.
        ImGui::SameLine();
        if (ui::Button(emsg::sparse_edit)) {
            _edit_after_open = true;
            _sparse_edit_src = f;
            request_open_splat(f.dir);
        }
        ui::help_on_hover(emsg::sparse_edit_help);
    }
    if (!f.mask_dir.empty()) {
        if (model) ImGui::SameLine();
        ImGui::BeginDisabled(dataset_busy() || native_work_busy());
        if (ui::Button(mmsg::correct_masks)) {
            // A run reports its folders as the config spells them: relative to
            // the dataset, and empty for the parser's default.
            TrainConfig stock;
            auto under = [&](const std::string& d, const std::string& fallback) {
                return (fs::path(f.dir) / (d.empty() ? fallback : d)).lexically_normal().string();
            };
            open_mask_editor(f.dir, under(f.image_dir, stock.image_dir),
                             under(f.mask_dir, stock.mask_dir), f.mask_flipped);
        }
        ImGui::EndDisabled();
        ui::help_on_hover(mmsg::correct_masks_help);
    }
}

void GuiApp::open_mask_editor(const std::string& workspace, const std::string& image_dir,
                              const std::string& mask_dir, bool mask_flipped) {
    if (dataset_busy() || native_work_busy()) return;
    close_native_previews();
    _mask_editor.set_log([this](const std::string& s) { log(s); });
    // The editor's own checkpoint, over the app's one download.
    _mask_editor.set_model_picker([this] {
        draw_mask_model_picker(_mask_editor_model_id, _download,
                               [this] { request_model_download(_mask_editor_model_id); });
    });
    // SAM loads on the device every other inference user freezes, never nn's
    // default; a failed freeze refuses the prompt with the same sentence.
    _mask_editor.set_sam_device_gate([this](std::string& device, std::string& error) {
        if (!freeze_native_device()) {
            error = _native_device_error.empty() ? msg::no_device_found.get()
                                                 : _native_device_error;
            return false;
        }
        device = _native_device_uuid;
        return true;
    });
    std::string err;
    if (!_mask_editor.open(workspace, image_dir, mask_dir, mask_flipped, err)) log(err);
}

// ---------------------------------------------------------------------------
// Starting over
// ---------------------------------------------------------------------------

void GuiApp::reset_recon_options() {
    // The photographs' colour space was read off the files, not chosen, so it
    // is not one of the options this puts back -- and keeping the intermediate
    // files is a setting of the program's rather than of this project's.
    const std::string gamut = _sfm_job.image_gamut;
    const std::optional<bool> linear = _sfm_job.image_is_linear;
    const bool keep = _sfm_job.keep_intermediate;
    _sfm_job = SfmJob{};
    _colmap_job = ColmapJob{};
    _geometry = GeometryJob{};
    _sfm_job.image_gamut = gamut;
    _sfm_job.image_is_linear = linear;
    _sfm_job.keep_intermediate = keep;
    for (PrepInput& s : _sources) {
        s.camera_model = default_lens(s);
        s.focal_factor = 0.0f;
        for (SubCamera& sc : s.subcameras) {
            sc.camera_model = s.camera_model;
            sc.focal_factor = 0.0f;
        }
    }
    apply_capture_defaults(_sources, _sfm_job, _colmap_job);
    normalize_source_lenses(_sources, _sfm_job.camera_model);
    _redo_frames = _redo_masks = _redo_model = _redo_geometry = false;
    _resume = true;
    log(dmsg::reset_options_done.get());
}

void GuiApp::draw_dataset_reset() {
    if (!ui::CollapsingHeader(dmsg::reset_section)) return;
    ImGui::Indent();
    ui::TextDisabledWrapped(dmsg::reset_section_help);
    ImGui::BeginDisabled(native_work_busy());
    // A transforms.json or a Metashape export is not on the list -- it is the
    // dataset somebody handed over, not something a run here wrote -- so a
    // folder can hold a model and still have nothing to delete.
    ImGui::BeginDisabled(_ws_artifacts.empty());
    if (ui::Button(dmsg::clear_project)) {
        _clear_targets = _ws_artifacts;
        _clear_open = true;
    }
    ImGui::EndDisabled();
    ui::help_on_hover(dmsg::clear_project_help);
    ImGui::SameLine();
    if (ui::Button(dmsg::reset_options)) reset_recon_options();
    ui::help_on_hover(dmsg::reset_options_help);
    ImGui::EndDisabled();
    ImGui::NewLine();
    ImGui::Unindent();
}

// Deleting is the one thing here that cannot be undone, so the modal lists the
// paths themselves rather than describing them.
void GuiApp::draw_clear_project_modal() {
    if (_clear_open) {
        ui::OpenPopup(dmsg::clear_project_title);
        _clear_open = false;
        _clear_shown = true;
    }
    if (!_clear_shown) return;
    if (!ui::BeginPopupModal(dmsg::clear_project_title, nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
        _clear_shown = false;
        return;
    }
    ImGui::PushTextWrapPos(px(460.0f));
    ui::Text(dmsg::clear_project_confirm, {_workspace});
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    for (const std::string& path : _clear_targets) ui::TextDisabledRaw(path);
    ImGui::Spacing();

    if (ui::Button(dmsg::clear_project_button, ImVec2(px(150.0f), 0)) &&
        !native_work_busy()) {
        // The screen is reading several of these; it has to let go first.
        reset_dataset_preview();
        for (const std::string& path : _clear_targets) {
            std::error_code ec;
            fs::remove_all(path, ec);
            if (ec) log(i18n::format(dmsg::clear_project_failed,
                                     {path, ec.message()}));
        }
        log(i18n::format(dmsg::clear_project_done, {_workspace}));
        _clear_targets.clear();
        _ws_state_at = -1.0;      // the answer changed; do not wait a second
        _clear_shown = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ui::Button(dmsg::cancel, ImVec2(px(150.0f), 0))) {
        _clear_shown = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void GuiApp::draw_drop_intermediate_modal() {
    if (_drop_intermediate_open) {
        ui::OpenPopup(dmsg::drop_intermediate_title);
        _drop_intermediate_open = false;
        _drop_intermediate_shown = true;
    }
    if (!_drop_intermediate_shown) return;
    // Closed by the window's own means rather than by a button: the setting
    // stays as it was, which is the safe half of the question.
    if (!ui::BeginPopupModal(dmsg::drop_intermediate_title, nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
        _drop_intermediate_shown = false;
        _sfm_job.keep_intermediate = true;
        return;
    }
    ImGui::PushTextWrapPos(px(460.0f));
    ui::Text(dmsg::drop_intermediate_confirm);
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    if (ui::Button(dmsg::drop_intermediate_button, ImVec2(px(150.0f), 0))) {
        _sfm_job.keep_intermediate = false;
        save_settings();
        _drop_intermediate_shown = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ui::Button(dmsg::cancel, ImVec2(px(150.0f), 0))) {
        _sfm_job.keep_intermediate = true;
        _drop_intermediate_shown = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

// Masks the reconstruction being kept has never seen. Adding them is minutes
// and rebuilding with the masked feature points is the whole hour again; both
// are what somebody means by the button, so it asks rather than choosing.
void GuiApp::draw_mask_recon_modal() {
    if (_mask_recon_open) {
        ui::OpenPopup(dmsg::mask_recon_title);
        _mask_recon_open = false;
        _mask_recon_shown = true;
    }
    if (!_mask_recon_shown) return;
    if (!ui::BeginPopupModal(dmsg::mask_recon_title, nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
        _mask_recon_shown = false;
        return;
    }
    ImGui::PushTextWrapPos(px(460.0f));
    ui::Text(dmsg::mask_recon_confirm, {_workspace});
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    bool go = false;
    if (ui::Button(dmsg::mask_recon_rebuild, ImVec2(px(220.0f), 0))) {
        _redo_model = go = true;
    }
    ImGui::SameLine();
    if (ui::Button(dmsg::mask_recon_masks_only, ImVec2(px(220.0f), 0))) {
        // Answered for good rather than per run: the checkbox now shows what
        // was decided here, and the question does not come back.
        _mask_features = false;
        go = true;
    }
    ui::help_on_hover(dmsg::mask_recon_masks_only_help);
    ImGui::SameLine();
    if (ui::Button(dmsg::cancel, ImVec2(px(120.0f), 0))) {
        _mask_recon_shown = false;
        ImGui::CloseCurrentPopup();
    }
    if (go) {
        _mask_recon_shown = false;
        ImGui::CloseCurrentPopup();
        launch_dataset_job();
    }
    ImGui::EndPopup();
}

// ---------------------------------------------------------------------------
// Advanced: the photographs' colour space
// ---------------------------------------------------------------------------

void GuiApp::draw_color_space_options(bool with_point_color) {
    ui::SeparatorText(dmsg::section_color_space);
    bool source_changed = false;

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
        source_changed = true;
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
        source_changed = true;
    }
    ui::help_on_hover(dmsg::input_is_linear_help);
    if (source_changed) close_native_previews();

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
    if (any_pano360(_sources)) draw_pano360_size();

    ImGui::SetNextItemWidth(px(260.0f));
    ui::Combo(dmsg::features, &_sfm_job.features,
              {&dmsg::features_sift, &dmsg::features_aliked_n16,
               &dmsg::features_aliked_n32, &dmsg::features_loma_b128,
               &dmsg::features_loma_b});
    ui::help_on_hover(dmsg::features_help);

    {
        // Brute force is the only option for SIFT, so say so by disabling the
        // combo. The second entry is whichever learned matcher goes with the
        // frontend above: each reads only the descriptors it was trained on.
        const bool learned = _sfm_job.features != 0;
        const bool is_loma = _sfm_job.features >= 3;
        ImGui::BeginDisabled(!learned);
        ImGui::SetNextItemWidth(px(260.0f));
        int shown = learned ? _sfm_job.matcher : 0;
        if (ui::Combo(dmsg::matcher, &shown,
                      {&dmsg::matcher_brute_force,
                       is_loma ? &dmsg::matcher_loma : &dmsg::matcher_lightglue}) &&
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

    // "Automatic" resolves to sequential for a short video and to pair
    // selection at 100 images, so each is offered whenever it can be what runs.
    if (_sfm_job.pairs == 2 || (_sfm_job.pairs == 0 && _sfm_job.data_type == 1)) {
        ui::Checkbox(dmsg::loop_closure, &_sfm_job.loop_closure);
        ui::help_on_hover(dmsg::loop_closure_help_builtin);
    }
    if (_sfm_job.pairs == 0 || _sfm_job.pairs == 3) {
        ui::Checkbox(dmsg::prefilter_sequential, &_sfm_job.prefilter_sequential);
        ui::help_on_hover(dmsg::prefilter_sequential_help);
    }
    ui::Checkbox(dmsg::use_sequence, &_sfm_job.use_sequence);
    ui::help_on_hover(dmsg::use_sequence_help);
    if (sequential_window_applies(_sfm_job)) {
        ImGui::SetNextItemWidth(px(260.0f));
        ui::InputInt(dmsg::sequential_overlap, &_sfm_job.overlap);
        ui::help_on_hover(dmsg::sequential_overlap_help);
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
    ui::Checkbox(dmsg::sfm_final_free_rig, &_sfm_job.final_free_rig);
    ui::help_on_hover(dmsg::sfm_final_free_rig_help);

    ImGui::SetNextItemWidth(px(260.0f));
    ui::InputInt(dmsg::max_features_auto, &_sfm_job.max_features);
    ui::help_on_hover(dmsg::max_features_auto_help);
    ImGui::SetNextItemWidth(px(260.0f));
    ui::InputInt(dmsg::max_image_size_auto, &_sfm_job.max_image_size);
    ui::help_on_hover(dmsg::max_image_size_auto_help);

    // ---- what the sensors are allowed to settle ----
    // A video's own IMU and GPS track, and per photograph the EXIF position
    // and a drone's recorded attitude. Any one can be all an input has.
    ImGui::Spacing();
    ui::SeparatorText(dmsg::section_sensors);
    ImGui::SetNextItemWidth(px(260.0f));
    ui::Combo(dmsg::sfm_sensor_gauge, &_sfm_job.sensor_gauge,
              {&dmsg::sfm_sensor_gauge_off, &dmsg::sfm_sensor_gauge_up,
               &dmsg::sfm_sensor_gauge_auto});
    ui::help_on_hover(dmsg::sfm_sensor_gauge_help);

    ImGui::SetNextItemWidth(px(260.0f));
    ui::Combo(dmsg::sfm_metric_gps, &_sfm_job.metric_gps,
              {&dmsg::sfm_metric_gps_off, &dmsg::sfm_metric_gps_horizontal,
               &dmsg::sfm_metric_gps_full});
    ui::help_on_hover(dmsg::sfm_metric_gps_help);

    ImGui::SetNextItemWidth(px(260.0f));
    ui::Combo(dmsg::sfm_exif_attitude, &_sfm_job.exif_attitude,
              {&dmsg::sfm_sensor_gauge_off, &dmsg::sfm_sensor_gauge_up,
               &dmsg::sfm_exif_attitude_auto});
    ui::help_on_hover(dmsg::sfm_exif_attitude_help);
    ImGui::Spacing();

    // Unticking it is what throws the resumable state away, so it asks first
    // and the answer is remembered; ticking it back needs no ceremony.
    if (ui::Checkbox(dmsg::keep_intermediate, &_sfm_job.keep_intermediate)) {
        if (_sfm_job.keep_intermediate) save_settings();
        else _drop_intermediate_open = true;
    }
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
    {
        int& bits = _sfm_job.prep.frame_bits;
        int pick = bits == 8 ? 1 : bits == 16 ? 2 : 0;
        ImGui::SetNextItemWidth(px(260.0f));
        if (ui::Combo(dmsg::frame_bits, &pick,
                      {&dmsg::frame_bits_auto, &dmsg::frame_bits_8, &dmsg::frame_bits_16}))
            bits = pick == 1 ? 8 : pick == 2 ? 16 : 0;
        ui::help_on_hover(dmsg::frame_bits_help);
    }
    ImGui::BeginDisabled(!backends().builtin_masking);
    ui::Checkbox(dmsg::use_python_masking, &_sfm_job.prep.force_external_masking);
    ImGui::EndDisabled();
    ui::help_on_hover(dmsg::use_python_masking_help);

    ui::Checkbox(dmsg::sfm_ba_cpu, &_sfm_job.ba_cpu);
    ui::help_on_hover(dmsg::sfm_ba_cpu_help);

    ui::Checkbox(dmsg::sfm_subprocess, &_sfm_job.subprocess);
    ui::help_on_hover(dmsg::sfm_subprocess_help);
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
            open_pick(PickAction::VocabTree, msg::pick_vocab_tree.get(),
                      FileDialog::Mode::File, {".bin"});
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
        const bool ffmpeg_changed =
            ui::InputText(dmsg::ffmpeg_executable, &_ffmpeg_exe);
        ch |= ffmpeg_changed;
        ui::help_on_hover(backends().builtin_video
                              ? dmsg::ffmpeg_executable_help_fallback
                              : dmsg::ffmpeg_executable_help_always);
        ImGui::SetNextItemWidth(px(300.0f));
        ch |= ui::InputText(dmsg::python_executable, &_python_exe);
        ui::help_on_hover(dmsg::python_executable_help);
        if (ffmpeg_changed) mark_source_metadata_dirty();
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
    ImGui::BeginDisabled(native_work_busy());
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
    ui::SeparatorText(msg::section_preset);
    // A batch owns these settings while it runs -- each row's come from its
    // own preset -- so what the running row IS building goes here instead.
    if (_batch_active) {
        draw_batch_progress();
    } else {
        ImGui::BeginDisabled(running);
        draw_dataset_preset_picker();
        ImGui::EndDisabled();
    }

    ImGui::Spacing();
    ui::SeparatorText(dmsg::section_settings);
    // The GPU choice sits at the top of the settings it constrains: a preview,
    // a masking step and the reconstruction all run on it, so it is offered
    // before any of them can be started.
#if defined(SS_BUILD_SAM) || defined(SS_TOOL_SFM) || defined(SS_BACKEND_VULKAN)
    draw_device_picker();
    ImGui::Spacing();
#endif
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
    bool input_missing = _sources.empty() || _workspace.empty();
    for (const PrepInput& s : _sources)
        input_missing = input_missing || s.path.empty();
    const bool ready = !running && !input_missing && _source_probes_ready;
    if (!running) {
        const bool need_mask_model = mask_model_missing();
        const bool need_feat_model = feature_model_missing();
        const bool need_geom_model = geometry_model_missing();
        const bool need_model = need_mask_model || need_feat_model || need_geom_model;
        // The button names what pressing it does: a folder that already holds
        // a reconstruction is added to, not built.
        const bool adding = workspace_state().model && !_redo_model;
        ImGui::BeginDisabled(!ready || need_model || native_work_busy() ||
                             _batch_active);
        if (ui::Button(adding ? dmsg::update_dataset : dmsg::create_dataset,
                       ImVec2(px(200.0f), px(34.0f))))
            start_dataset_job();
        ImGui::EndDisabled();
        if (input_missing) {
            ImGui::SameLine();
            ui::TextDisabled(dmsg::pick_input_first);
        } else if (!_source_probes_ready) {
            ImGui::SameLine();
            ui::TextDisabled(dmsg::sensors_reading);
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
                if (need_mask_model)      request_model_download(_model_id);
                else if (need_feat_model) request_feature_download();
                else                      request_geometry_download();
            }
        }
        if (ready) {
            draw_dataset_rerun(workspace_state());
            draw_dataset_reset();
        }
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
        if (effective_engine() == Engine::BuiltIn && _sfm.not_metric())
            ui::TextColoredWrapped(kWarn, dmsg::not_metric_reconstruction);
        ui::TextColoredWrapped(kOk, dmsg::done_at, {st.dir});
    } else if (st.failed) {
        ui::TextColoredWrapped(kErr, dmsg::failed, {st.err});
    } else if (st.cancelled) {
        ui::TextColored(kDim, dmsg::cancelled);
    }
    // A dataset loaded from disk offers the same buttons as one just built.
    if (st.done) {
        DatasetFolders f{st.dir, st.image_dir, st.mask_dir, st.mask_flipped};
        const fs::path masks = fs::path(st.dir) / st.mask_dir;
        std::error_code ec;
        if (st.mask_dir.empty() || !fs::is_directory(masks, ec) || fs::is_empty(masks, ec))
            f.mask_dir.clear();
        draw_dataset_open_buttons(f, /*model=*/true);
    } else if (ready) {
        const WorkspaceState& prior = workspace_state();
        draw_dataset_open_buttons(workspace_folders(prior), prior.model);
    }
    _ds_action_h = ImGui::GetCursorPosY() - action_y0;
}

void GuiApp::draw_new_dataset() {
    const bool running = dataset_busy();

    ImGui::BeginDisabled(native_work_busy());
    if (ui::Button(msg::back_home)) {
        _screen = Screen::Home;
        // The preview holds GL buffers and a decoding thread for a screen that
        // is no longer up.
        if (!native_work_busy()) reset_dataset_preview();
    }
    ImGui::EndDisabled();
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

    // Side by side only when there is a preview AND room for both. On a narrow
    // window the preview goes under the form instead, which is worth less but
    // costs the form nothing it cannot scroll.
    const bool wide = ImGui::GetContentRegionAvail().x >= px(1000.0f);
    const bool two_col = preview_has_content() && wide;

    ImGui::BeginChild("##dsbody", ImVec2(0, body_height(log_h)));
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
            _segment.set_workspace(_workspace);
            _segment.draw(_mask, _sources[(size_t)_mask_preview_input].stencil);
            if (_segment.take_shapes_edited()) _frame_shapes.clear();
            if (_segment.take_browse_request())
                open_pick(PickAction::StencilFile, dmsg::stencil_pick_file.get(),
                          FileDialog::Mode::File, {".svg"});
        }
    }
    if (_geometry_panel.is_open()) _geometry_panel.draw(_geometry);
    draw_clear_project_modal();
    draw_drop_intermediate_modal();
    draw_mask_recon_modal();
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
    if (const ModelEntry* e = find_model(_license_model_id))
        ui::TextDisabled(dmsg::license_download_size, {human_bytes(e->bytes)});
    ImGui::Spacing();

    if (li.needs_tick)
        ui::Checkbox(dmsg::license_accept_tick, &_license_tick);
    ImGui::Spacing();

    ImGui::BeginDisabled(li.needs_tick && !_license_tick);
    if (ui::Button(dmsg::license_download, ImVec2(150, 0))) {
        _accepted_licenses.push_back(_license_prompt);
        save_settings();
        if (const ModelEntry* e = find_model(_license_model_id)) _download.start(*e);
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
        const BatchRow* row = batch_running_row();
        ui::TextDisabledRaw(row ? row->dataset : std::string());
    } else {
        ui::TextDisabledRaw(_cfg.data);
        // Offered when the dataset's mask folder exists; probed at most once
        // a second, as workspace_state() does.
        const std::string key = _cfg.data + "\n" + _cfg.mask_dir;
        const double now = ImGui::GetTime();
        if (key != _train_masks_key || now - _train_masks_at > 1.0) {
            std::error_code ec;
            fs::path md(_cfg.mask_dir);
            if (md.is_relative()) md = fs::path(_cfg.data) / md;
            _train_masks_key = key;
            _train_masks_at = now;
            _train_has_masks = !_cfg.data.empty() && fs::is_directory(md, ec) && !fs::is_empty(md, ec);
            _train_color = spirula::dataset_color_label(
                spirula::summarize_dataset_color(spirula::read_dataset_color(_cfg.data)));
        }
        if (!_train_color.empty()) {
            ImGui::SameLine();
            ui::TextDisabledRaw(i18n::format(msg::dataset_color_profile, {_train_color}));
            ui::help_on_hover(msg::dataset_color_profile_help);
        }
        if (_train_has_masks) {
            ImGui::SameLine();
            ImGui::BeginDisabled(training_busy() || native_work_busy());
            if (ui::Button(mmsg::correct_masks)) {
                fs::path id(_cfg.image_dir), md(_cfg.mask_dir);
                if (id.is_relative()) id = fs::path(_cfg.data) / id;
                if (md.is_relative()) md = fs::path(_cfg.data) / md;
                open_mask_editor(_cfg.data, id.string(), md.string(), _cfg.flip_mask);
            }
            ImGui::EndDisabled();
            ui::help_on_hover(mmsg::correct_masks_help);
        }
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
    // Every route off this screen throws the open document away, so each one
    // goes past the same question first.
    if (ui::Button(msg::back_home))
        _compare.confirm_discard_edits([this] { request_go_home(); });
    ImGui::SameLine();
    if (ui::Button(msg::viewer_open_another)) {
        _compare.confirm_discard_edits([this] {
            open_pick(PickAction::SplatFile, msg::viewer_pick_file.get(),
                      FileDialog::Mode::FileOrFolder, kOpenableExtensions);
        });
    }
    ui::help_on_hover(msg::home_open_splat_help);
    // The way into render mode for someone who has not found it on the pane.
    if (_compare.rendering() < 0 && _compare.count() > 0) {
        ImGui::SameLine();
        if (ui::Button(rmsg::train_render)) _compare.begin_render(std::max(0, _compare.editing()));
        ui::help_on_hover(rmsg::enter_render_help);
    }
    ImGui::SameLine();
    _compare.set_recents(_model_recents);
    _compare.draw_toolbar();

    const float log_h = log_height(ImGui::GetContentRegionAvail().y);
    ImGui::BeginChild("##viewer", ImVec2(0, body_height(log_h)));
    draw_compare_panes();
    ImGui::EndChild();
    draw_log_panel(log_h);
}

void GuiApp::draw_compare_panes() {
    // Rendering is the viewer screen's; the meshing preview shows a result.
    _compare.set_render_allowed(_screen == Screen::Viewer);
    _compare.render().set_ffmpeg(_ffmpeg_exe);
    const CompareView::Sidebar side = _compare.sidebar();
    if (side != CompareView::Sidebar::None) {
        const float avail = ImGui::GetContentRegionAvail().x;
        const float h = ImGui::GetContentRegionAvail().y;
        const float w = std::clamp(_edit_panel_w * ui_scale(), px(220.0f),
                                   std::max(px(220.0f), avail * 0.6f));
        ImGui::BeginChild("##editpanel", ImVec2(w, 0), ImGuiChildFlags_Borders);
        if (side == CompareView::Sidebar::Render) _compare.render().draw_panel();
        else _compare.edit().draw_panel();
        ImGui::EndChild();
        float dragged = w;
        if (splitter_v("##editpanelsplit", &dragged, px(220.0f), avail * 0.7f, h)) {
            _edit_panel_w = dragged / ui_scale();
            _layout_dirty = true;
        }
        ImGui::BeginChild("##editpanes", ImVec2(0, 0));
        if (side == CompareView::Sidebar::Render) {
            // The timeline runs under the panes, the width of all of them.
            const float tl = _compare.render().timeline_height();
            _compare.draw(std::max(px(120.0f), ImGui::GetContentRegionAvail().y - tl));
            _compare.render().draw_timeline();
        } else {
            _compare.draw(0.0f);
        }
        ImGui::EndChild();
        return;
    }
    _compare.draw(0.0f);
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
        _mesh_data_probe_found = fs::exists(_mesh_job.data_dir, ec);
    } else if (!_mesh_job.checkpoint.empty()) {
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
    if (_mesh_job.checkpoint.empty() || native_work_busy()) return;
    stop_inference_users();
    close_splat();
#ifdef SS_BACKEND_VULKAN
    if (!freeze_native_device()) return;
#else
    if (!freeze_cuda_device()) return;
#endif
    // Again at dispatch: a mesh preset applied since the freeze replaced the job.
    _mesh_job.device_uuid = _native_device_uuid;
#ifndef SS_BACKEND_VULKAN
    _mesh_job.cuda_device = _cuda_device_index;
#endif
    close_mesh_preview();
    _mesh.start(_mesh_job);
}

void GuiApp::close_mesh_preview() {
    if (!_mesh_preview_open) return;
    _compare.close();
    _mesh_preview_open = false;
}

void GuiApp::open_mesh_preview() {
    const std::string out = _mesh.output_path();
    if (out.empty()) return;
    close_native_previews();
    close_splat();
#ifdef SS_BACKEND_VULKAN
    if (!freeze_native_device()) return;
#else
    if (!freeze_cuda_device()) return;
#endif
    // Whatever else had the engine (a file opened from the viewer screen) has
    // to let go before the splats take it.
    if (!training_busy()) {
        detach_session_views();
        _runner.note_engine_taken();
        _compare.add(_mesh_job.checkpoint, &msg::mesh_side_splats);
    }
    _compare.add(out, &msg::mesh_side_mesh);
    _mesh_preview_open = true;
}

void GuiApp::draw_mesh_options() {
    ui::SeparatorText(msg::section_preset);
    draw_mesh_preset_picker();
    ImGui::Spacing();

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
        open_pick(PickAction::MeshSource, msg::mesh_pick_model.get(),
                  FileDialog::Mode::Folder);
    }
    ImGui::SameLine();
    if (ui::ButtonRaw(".ply##meshsrcfile", ImVec2(60, 0))) {
        open_pick(PickAction::MeshSource, msg::mesh_pick_model.get(),
                  FileDialog::Mode::File, {".ply"});
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
            open_pick(PickAction::MeshPhotos, msg::mesh_pick_photos.get(),
                      FileDialog::Mode::Folder);
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
    // A set rather than a choice: one extraction, written in each of them, so
    // a textured GLB and a plain PLY cost one run between them, not two.
    ui::SeparatorText(msg::mesh_color);
    const Msg* color_names[kNumMeshColorModes] = {
        &msg::mesh_color_none, &msg::mesh_color_vertex, &msg::mesh_color_texture};
    for (int i = 0; i < kNumMeshColorModes; i++) {
        if (i) ImGui::SameLine();
        ImGui::PushID(i);
        ui::Checkbox(*color_names[i], &_mesh_job.colors[i]);
        ImGui::PopID();
    }
    ui::help_on_hover(msg::mesh_color_help);
    if (_mesh_job.wants_color(2)) {
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
    // The child refuses a format that cannot carry the color asked of it, so
    // one no ticked color travels in is greyed out rather than failing the run.
    ui::SeparatorText(msg::mesh_formats);
    for (int i = 0; i < kNumMeshFormats; i++) {
        if (i) ImGui::SameLine();
        bool ok = false;
        for (int c = 0; c < kNumMeshColorModes; c++)
            ok = ok || (_mesh_job.colors[c] && mesh_format_carries(i, c));
        if (!ok) _mesh_job.formats[i] = false;
        ImGui::BeginDisabled(!ok);
        // A file extension is a file extension in every language.
        ImGui::PushID(i);
        ui::CheckboxRaw(kMeshFormats[i], &_mesh_job.formats[i]);
        ImGui::PopID();
        ImGui::EndDisabled();
    }
    // Nothing left that a requested color and a requested format can both
    // carry. Said here rather than fixed silently: which of the two to give up
    // is the user's choice, and the Create button stays disabled until it is.
    if (mesh_job_writes_nothing(_mesh_job))
        ui::TextColoredWrapped(kErr, msg::mesh_no_output_warn);
    ImGui::SetNextItemWidth(field_width(msg::mesh_output));
    ui::InputTextRaw("##meshout", &_mesh_job.output);
    ImGui::SameLine();
    if (ui::ButtonRaw("...##meshoutpick", ImVec2(60, 0))) {
        open_pick(PickAction::MeshOutput, msg::mesh_pick_output.get(),
                  FileDialog::Mode::Folder);
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
        // Which of them is on screen. One run writes several formats of one
        // surface, and four panes of the same mesh is not a comparison.
        ui::TextDisabled(emsg::mesh_shown);
        if (!training_busy() && !_mesh_job.checkpoint.empty()) {
            ImGui::SameLine();
            bool on = _compare.index_of(_mesh_job.checkpoint) >= 0;
            if (ui::Checkbox(emsg::mesh_side_splats_pane, &on))
                _compare.set_shown(_mesh_job.checkpoint, on,
                                   &msg::mesh_side_splats);
        }
        for (const std::string& out : _mesh.output_paths()) {
            ImGui::SameLine();
            ImGui::PushID(out.c_str());
            bool on = _compare.index_of(out) >= 0;
            if (ui::CheckboxRaw(fs::path(out).filename().string().c_str(), &on))
                _compare.set_shown(out, on, &msg::mesh_side_mesh);
            ImGui::PopID();
        }
        _compare.set_recents(_model_recents);
        _compare.draw_toolbar();
        draw_compare_panes();
    } else {
        // A batch owns the options while it runs; what it IS meshing goes here.
        if (_batch_active) {
            draw_batch_progress();
            ImGui::Spacing();
        }
        ImGui::BeginDisabled(running);
        draw_mesh_options();
        ImGui::EndDisabled();

        ImGui::Spacing();
        if (running) {
            const float p = _mesh.progress();
            ui::ProgressBar(p >= 0 ? p : 0.0f, ImVec2(-1, 0), msg::mesh_running);
            const std::string st = _mesh.stage();
            if (!st.empty()) ui::TextDisabledRaw(st);
            if (ui::Button(msg::mesh_cancel)) {
                if (_batch_active) _batch_stop_after = _batch_stop_now = true;
                _mesh.cancel();
            }
        } else {
            ImGui::BeginDisabled(_mesh_job.checkpoint.empty() || _batch_active ||
                                 mesh_job_writes_nothing(_mesh_job));
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
// The list stays readable while a batch is in flight, which is what makes an
// unattended run something you come back to rather than something you watch.
// ===========================================================================

namespace {

// The one word a row reports: the worst thing that happened to its tasks.
BatchStatus row_status(const std::vector<BatchTask>& tasks, int row) {
    bool any = false, all_done = true, failed = false, running = false;
    bool stopped = false, skipped = false;
    for (const BatchTask& t : tasks) {
        if (t.row != row) continue;
        any = true;
        running = running || t.status == BatchStatus::Running;
        failed = failed || t.status == BatchStatus::Failed;
        stopped = stopped || t.status == BatchStatus::Stopped;
        skipped = skipped || t.status == BatchStatus::Skipped;
        all_done = all_done && t.status == BatchStatus::Done;
    }
    if (!any) return BatchStatus::Pending;
    if (running) return BatchStatus::Running;
    if (failed) return BatchStatus::Failed;
    if (stopped) return BatchStatus::Stopped;
    if (all_done) return BatchStatus::Done;
    if (skipped) return BatchStatus::Skipped;
    return BatchStatus::Pending;
}

void draw_status_word(BatchStatus st) {
    switch (st) {
        case BatchStatus::Running: ui::TextColored(kWarn, msg::batch_status_running); break;
        case BatchStatus::Done:    ui::TextColored(kOk, msg::batch_status_done); break;
        case BatchStatus::Failed:  ui::TextColored(kErr, msg::batch_status_failed); break;
        case BatchStatus::Skipped: ui::TextColored(kDim, msg::batch_status_skipped); break;
        case BatchStatus::Stopped: ui::TextColored(kDim, msg::batch_status_stopped); break;
        default:                   ui::TextColored(kDim, msg::batch_status_pending); break;
    }
}

// One built-in row of a preset combo: the name the file and the command line
// spell, the translated label beside it, and the sentence it hovers to.
struct BuiltinRow {
    std::string name, label, help;
};

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

// What a preset combo came back with. Nothing moved unless `moved`; a
// built-in is `builtin`, a saved file is `index`, and `from_file` is the row
// that opens a file dialog.
struct PresetChoice {
    bool moved = false;
    bool from_file = false;
    std::string builtin;
    int index = -1;
};

// The combo every preset picker draws: the built-ins of that kind (or a
// single stock row for a kind that has none), then the saved files, then the
// way to a file outside the folder.
template <class T>
PresetChoice preset_combo(const char* id, const PresetPicker<T>& pick,
                          const std::string& file, const std::string& builtin,
                          const std::vector<BuiltinRow>& builtins,
                          bool offer_file = false) {
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

}  // namespace


const Msg& GuiApp::batch_stage_name(BatchStage s) const {
    switch (s) {
        case BatchStage::Dataset: return msg::batch_stage_dataset;
        case BatchStage::Mesh:    return msg::batch_stage_mesh;
        default:                  return msg::batch_stage_train;
    }
}

// What a collapsed row is about: the thing it works on, which is the only
// part of it worth reading at a glance.
std::string GuiApp::batch_row_summary(const BatchRow& row) const {
    if (row.does(BatchStage::Dataset) && !row.sources.empty()) {
        std::string s = row.sources[0];
        if (row.sources.size() > 1)
            s += " (+" + std::to_string(row.sources.size() - 1) + ")";
        return s;
    }
    if (!row.dataset.empty()) return row.dataset;
    if (!row.model.empty()) return row.model;
    return {};
}


void GuiApp::draw_batch() {
    // Keeps each kind's saved presets warm for the rows' pickers and their
    // tooltips (rate-limited inside, so this is a no-op most frames).
    refresh_presets();
    refresh_dataset_presets();
    refresh_mesh_presets();
    check_batch_if_stale();

    if (ui::Button(msg::back_home)) request_go_home();
    ImGui::SameLine();
    ui::Text(msg::batch_title);

    ImGui::PushTextWrapPos();
    ui::TextDisabled(msg::batch_intro);
    ImGui::PopTextWrapPos();

    const float log_h = log_height(ImGui::GetContentRegionAvail().y);
    ImGui::BeginChild("##batchlist", ImVec2(0, body_height(log_h)));

    if (_batch_active) {
        draw_batch_progress();
        ImGui::Spacing();
    }
    draw_batch_rows();

    ImGui::Spacing();
    ImGui::BeginDisabled(_batch_active);
    if (ui::Button(msg::batch_add_create)) {
        _pick_row = -1;
        open_pick(PickAction::BatchSourceVideo, msg::batch_pick_source.get(),
                  FileDialog::Mode::File,
                  std::vector<std::string>(kVideoExtensions,
                                           kVideoExtensions + kNumVideoExtensions),
                  "", /*multi=*/true);
    }
    ui::help_on_hover(msg::batch_add_create_help);
    ImGui::SameLine();
    if (ui::Button(msg::batch_add_photos)) {
        _pick_row = -1;
        open_pick(PickAction::BatchSourceImages, msg::batch_pick_photos.get(),
                  FileDialog::Mode::Folder);
    }
    ImGui::SameLine();
    if (ui::Button(msg::batch_add_row)) {
        _pick_row = -1;   // append
        open_pick(PickAction::BatchDataset, msg::batch_pick_dataset.get(),
                  FileDialog::Mode::Folder);
    }
    ImGui::SameLine();
    if (ui::Button(msg::batch_add_mesh)) {
        _pick_row = -1;
        open_pick(PickAction::BatchModel, msg::batch_pick_model.get(),
                  FileDialog::Mode::Folder);
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

    ImGui::Spacing();
    // A batch's own dataset or meshing task is not something else.
    const bool busy_elsewhere =
        !_batch_active && (dataset_busy() || _mesh.busy());
    ImGui::BeginDisabled(_batch.empty());
    if (ui::Button(msg::batch_clear)) {
        _batch.clear();
        _batch_tasks.clear();
        _batch_open_row = -1;
        _batch_msg.clear();
        batch_edited();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(busy_elsewhere);
    if (ui::Button(msg::batch_start, ImVec2(px(160.0f), 0)))
        request_start_batch(/*skip_invalid=*/false);
    // The way past a row that cannot be fixed right now. Only offered once a
    // check has actually found one, so it is never the first thing tried.
    int bad = 0, live = 0;
    if (_batch_checked)
        for (const BatchRow& r : _batch) {
            if (!r.enabled) continue;
            live++;
            bad += batch_has_error(r.issues) ? 1 : 0;
        }
    if (bad > 0 && bad < live) {
        ImGui::SameLine();
        if (ui::Button(msg::batch_start_skip))
            request_start_batch(/*skip_invalid=*/true);
    }
    ImGui::EndDisabled();
    if (busy_elsewhere) {
        ImGui::SameLine();
        ui::TextColored(kDim, msg::batch_busy_elsewhere);
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    if (!_batch_msg.empty())
        ui::TextColoredWrappedRaw(_batch_msg_err ? kErr : kOk, _batch_msg);

    draw_batch_issues();
    draw_batch_command();
    draw_batch_plan();
    ImGui::EndChild();

    draw_log_panel(log_h);
}


void GuiApp::draw_batch_rows() {
    if (_batch.empty()) {
        ui::TextDisabled(msg::batch_empty);
        ui::TextDisabled(msg::batch_drop_hint);
        return;
    }
    int remove = -1, move = 0, move_from = -1;
    for (int i = 0; i < (int)_batch.size(); i++) {
        int one_move = 0;
        draw_batch_row(_batch[(size_t)i], i, remove, one_move);
        if (one_move != 0) { move = one_move; move_from = i; }
    }
    if (remove >= 0) {
        _batch.erase(_batch.begin() + remove);
        if (_batch_open_row == remove) _batch_open_row = -1;
        else if (_batch_open_row > remove) _batch_open_row--;
        // The last run's record is indexed by row; the rows have moved.
        _batch_tasks.clear();
        batch_edited();
    } else if (move != 0 && move_from >= 0) {
        const int to = move_from + move;
        if (to >= 0 && to < (int)_batch.size()) {
            std::swap(_batch[(size_t)move_from], _batch[(size_t)to]);
            if (_batch_open_row == move_from) _batch_open_row = to;
            else if (_batch_open_row == to) _batch_open_row = move_from;
            _batch_tasks.clear();
            batch_edited();
        }
    }
}


void GuiApp::draw_batch_row(BatchRow& row, int index, int& remove, int& move) {
    ImGui::PushID(index);
    const bool open = _batch_open_row == index;

    // ---- the header line ----
    if (ui::ButtonRaw(open ? "-##expand" : "+##expand",
                      ImVec2(ImGui::GetFrameHeight(), 0)))
        _batch_open_row = open ? -1 : index;
    ui::help_on_hover(msg::batch_row_expand_help);
    ImGui::SameLine();
    ImGui::BeginDisabled(_batch_active);
    if (ui::CheckboxRaw("##en", &row.enabled)) batch_edited();
    ui::help_on_hover(msg::batch_row_enabled_help);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ui::TextDisabledRaw(std::to_string(index + 1) + ".");

    // The stages as chips, each its own widget: a row of labels, never a
    // sentence stitched together out of them.
    for (int st = 0; st < kNumBatchStages; st++) {
        if (!row.stages[st]) continue;
        ImGui::SameLine();
        ui::TextColored(row.enabled ? kOk : kDim,
                        batch_stage_name((BatchStage)st));
    }
    if (row.does(BatchStage::Train) && batch_num_runs(row) > 1) {
        ImGui::SameLine();
        ui::TextDisabled(msg::batch_runs_count, {(long long)batch_num_runs(row)});
    }
    if (const std::string what = batch_row_summary(row); !what.empty()) {
        ImGui::SameLine();
        ui::TextDisabledRaw(what);
    }

    // ---- the right-hand controls ----
    const float bw = ImGui::GetFrameHeight();
    const float style_x = ImGui::GetStyle().ItemSpacing.x;
    ImGui::SameLine(std::max(ImGui::GetContentRegionAvail().x - 3 * bw -
                                 2 * style_x + ImGui::GetCursorPosX(),
                             ImGui::GetCursorPosX() + style_x));
    ImGui::BeginDisabled(_batch_active);
    if (ui::ButtonRaw("^##up", ImVec2(bw, 0))) move = -1;
    ui::help_on_hover(msg::batch_move_up);
    ImGui::SameLine(0, style_x);
    if (ui::ButtonRaw("v##down", ImVec2(bw, 0))) move = 1;
    ui::help_on_hover(msg::batch_move_down);
    ImGui::SameLine(0, style_x);
    if (ui::ButtonRaw("x##rm", ImVec2(bw, 0))) remove = index;
    ui::help_on_hover(dmsg::remove);
    ImGui::EndDisabled();

    // ---- status, and why ----
    const BatchStatus st = row_status(_batch_tasks, index);
    if (st != BatchStatus::Pending || _batch_active) {
        ImGui::Indent(ImGui::GetFrameHeight() * 2.0f);
        draw_status_word(st);
        // The newest thing this row has to say: what went wrong if anything
        // did, and otherwise the last thing it produced.
        std::string detail;
        for (const BatchTask& t : _batch_tasks) {
            if (t.row != index) continue;
            if (!t.message.empty()) { detail = t.message; break; }
            if (!t.result.empty()) detail = t.result;
        }
        if (!detail.empty()) {
            ImGui::SameLine();
            ui::TextDisabledRaw(detail);
        }
        ImGui::Unindent(ImGui::GetFrameHeight() * 2.0f);
    }

    if (open) {
        ImGui::Indent();
        ImGui::BeginDisabled(_batch_active);
        for (int i = 0; i < kNumBatchStages; i++) {
            if (i) ImGui::SameLine();
            ImGui::PushID(i);
            if (ui::Checkbox(batch_stage_name((BatchStage)i), &row.stages[i]))
                batch_edited();
            ImGui::PopID();
        }
        if (row.does(BatchStage::Dataset) || row.does(BatchStage::Train))
            draw_batch_row_common_dataset(row, index);
        if (row.does(BatchStage::Dataset)) draw_batch_row_dataset(row, index);
        if (row.does(BatchStage::Train)) draw_batch_row_train(row, index);
        if (row.does(BatchStage::Mesh)) draw_batch_row_mesh(row, index);
        ImGui::EndDisabled();
        ImGui::Unindent();
    }
    ImGui::Separator();
    ImGui::PopID();
}


void GuiApp::draw_batch_row_dataset(BatchRow& row, int index) {
    ui::SeparatorText(msg::batch_stage_dataset);
    ui::Text(msg::batch_row_inputs);
    int drop = -1;
    const float bw = ImGui::GetFrameHeight() + 6.0f;
    for (size_t i = 0; i < row.sources.size(); i++) {
        ImGui::PushID((int)i);
        ImGui::SetNextItemWidth(-bw);
        if (ui::InputTextRaw("##src", &row.sources[i])) batch_edited();
        ImGui::SameLine(0, 2);
        if (ui::ButtonRaw("x##rmsrc")) drop = (int)i;
        ImGui::PopID();
    }
    if (drop >= 0) {
        row.sources.erase(row.sources.begin() + drop);
        batch_edited();
    }
    if (ui::Button(msg::batch_add_video)) {
        _pick_row = index;
        open_pick(PickAction::BatchSourceVideo, msg::batch_pick_source.get(),
                  FileDialog::Mode::File,
                  std::vector<std::string>(kVideoExtensions,
                                           kVideoExtensions + kNumVideoExtensions),
                  "", /*multi=*/true);
    }
    ImGui::SameLine();
    if (ui::Button(msg::batch_add_photos)) {
        _pick_row = index;
        open_pick(PickAction::BatchSourceImages, msg::batch_pick_photos.get(),
                  FileDialog::Mode::Folder);
    }

    ui::Text(msg::batch_preset_dataset);
    ImGui::SetNextItemWidth(px(-8.0f));
    const PresetChoice picked =
        preset_combo("##dspreset", _ds_presets, row.dataset_preset.path,
                     row.dataset_preset.name, dataset_builtin_rows(), true);
    if (!picked.moved) return;
    if (picked.from_file) {
        _pick_row = index;
        open_pick(PickAction::BatchDatasetPresetFile, msg::preset_pick_file.get(),
                  FileDialog::Mode::File, {".json"});
    } else if (picked.index >= 0 && picked.index < (int)_ds_presets.items.size()) {
        row.dataset_preset.path = _ds_presets.items[(size_t)picked.index].path;
        row.dataset_preset.name = _ds_presets.items[(size_t)picked.index].name;
        batch_edited();
    } else {
        row.dataset_preset = BatchPreset{"", picked.builtin};
        batch_edited();
    }
}


void GuiApp::draw_batch_row_train(BatchRow& row, int index) {
    ui::SeparatorText(msg::batch_stage_train);
    ui::Text(msg::batch_runs);
    const float bw = ImGui::GetFrameHeight() + 6.0f;
    int drop = -1;
    for (size_t i = 0; i < row.runs.size(); i++) {
        ImGui::PushID((int)i);
        if (draw_batch_run(row.runs[i], index, (int)i, row.does(BatchStage::Mesh)))
            batch_edited();
        ImGui::SameLine(0, 2);
        if (ui::ButtonRaw("x##rmrun")) drop = (int)i;
        ImGui::PopID();
    }
    if (drop >= 0) {
        row.runs.erase(row.runs.begin() + drop);
        batch_edited();
    }
    if (row.runs.empty()) ui::TextDisabled(msg::batch_runs_default);
    if (ui::Button(msg::batch_add_run)) {
        row.runs.push_back(batch_run_on_screen());
        batch_edited();
    }
    ui::help_on_hover(msg::batch_add_run_help);

    ui::Text(msg::batch_col_output);
    ImGui::SetNextItemWidth(-bw);
    if (ui::InputTextWithHintRaw("##out", msg::batch_output_hint, &row.output_dir))
        batch_edited();
    ui::help_on_hover(msg::batch_output_help);
    ImGui::SameLine(0, 2);
    if (ui::ButtonRaw("...##out")) {
        _pick_row = index;
        open_pick(PickAction::BatchOutput, msg::batch_pick_output.get(),
                  FileDialog::Mode::Folder, {}, row.output_dir);
    }
}


// One training run on one line: which preset, the three numbers worth
// changing without a preset for each combination, and -- when the row meshes
// -- whether this is one of the runs it meshes.
bool GuiApp::draw_batch_run(BatchRun& run, int row, int slot, bool meshing) {
    bool moved = false;
    const float num_w = px(84.0f);
    const float sp = ImGui::GetStyle().ItemSpacing.x;
    const float bw = ImGui::GetFrameHeight() + 6.0f;
    const float boxes = 3.0f * (num_w + sp) + (meshing ? bw + sp : 0.0f);
    ImGui::SetNextItemWidth(std::max(px(120.0f),
                                     ImGui::GetContentRegionAvail().x - bw - boxes));
    moved |= draw_batch_train_preset(run.preset, "##trpreset", row, slot);

    // Text boxes rather than spinners because empty means "whatever the preset
    // says", and 0 is a legal SH degree. The hint is the column name, which is
    // shown exactly when the box is empty and there is nothing else to say.
    const Msg* labels[] = {&msg::batch_col_splats, &msg::batch_col_sh,
                           &msg::batch_col_steps};
    std::string* fields[] = {&run.cap_max, &run.sh_degree, &run.iterations};
    for (int k = 0; k < 3; k++) {
        ImGui::SameLine(0, sp);
        ImGui::PushID(k);
        ImGui::SetNextItemWidth(num_w);
        if (ui::InputTextWithHintRaw("##ovr", *labels[k], fields[k],
                                     ImGuiInputTextFlags_CharsDecimal))
            moved = true;
        ui::help_on_hover(msg::batch_override_help);
        ImGui::PopID();
    }
    if (meshing) {
        ImGui::SameLine(0, sp);
        if (ui::CheckboxRaw("##runmesh", &run.mesh)) moved = true;
        ui::help_on_hover(msg::batch_run_mesh_help);
    }
    return moved;
}


void GuiApp::draw_batch_row_mesh(BatchRow& row, int index) {
    ui::SeparatorText(msg::batch_stage_mesh);
    ui::Text(msg::batch_mesh_model_label);
    const float bw = ImGui::GetFrameHeight() + 6.0f;
    ImGui::SetNextItemWidth(-bw);
    if (ui::InputTextWithHintRaw("##meshmodel",
                                 row.does(BatchStage::Train)
                                     ? msg::batch_mesh_model_hint
                                     : msg::batch_mesh_model_pick,
                                 &row.model))
        batch_edited();
    ImGui::SameLine(0, 2);
    if (ui::ButtonRaw("...##meshmodel")) {
        _pick_row = index;
        open_pick(PickAction::BatchModel, msg::batch_pick_model.get(),
                  FileDialog::Mode::Folder, {}, row.model);
    }

    ui::Text(msg::batch_preset_mesh);
    ImGui::SetNextItemWidth(px(-8.0f));
    const PresetChoice picked = preset_combo("##meshpreset", _mesh_presets,
                                             row.mesh.preset.path, {}, {}, true);
    if (picked.moved) {
        if (picked.from_file) {
            _pick_row = index;
            open_pick(PickAction::BatchMeshPresetFile, msg::preset_pick_file.get(),
                      FileDialog::Mode::File, {".json"});
        } else if (picked.index >= 0 &&
                   picked.index < (int)_mesh_presets.items.size()) {
            row.mesh.preset.path = _mesh_presets.items[(size_t)picked.index].path;
            row.mesh.preset.name = _mesh_presets.items[(size_t)picked.index].name;
            batch_edited();
        } else {
            row.mesh.preset = BatchPreset{"", ""};
            batch_edited();
        }
    }
    if (draw_batch_mesh_outputs(row.mesh)) batch_edited();
}


// What the row writes, over what its preset says. Nothing ticked in a set is
// "whatever the preset says" rather than "write nothing", which is why the
// two sets are ticked rather than selected.
bool GuiApp::draw_batch_mesh_outputs(BatchMeshOptions& mesh) {
    bool moved = false;
    const Msg* color_names[kNumMeshColorModes] = {
        &msg::mesh_color_none, &msg::mesh_color_vertex, &msg::mesh_color_texture};

    ui::Text(msg::mesh_color);
    ui::help_on_hover(msg::batch_mesh_override_help);
    for (int i = 0; i < kNumMeshColorModes; i++) {
        ImGui::SameLine();
        ImGui::PushID(i);
        bool on = (mesh.colors & (1 << i)) != 0;
        if (ui::Checkbox(*color_names[i], &on)) {
            mesh.colors = on ? (mesh.colors | (1 << i)) : (mesh.colors & ~(1 << i));
            moved = true;
        }
        ImGui::PopID();
    }

    ui::Text(msg::mesh_formats);
    ui::help_on_hover(msg::batch_mesh_override_help);
    for (int i = 0; i < kNumMeshFormats; i++) {
        ImGui::SameLine();
        ImGui::PushID(i);
        bool on = (mesh.formats & (1 << i)) != 0;
        // A format no ticked colour can travel in is greyed out; with no
        // colour ticked at all the preset decides, so all of them are offered.
        bool ok = mesh.colors == 0;
        for (int c = 0; c < kNumMeshColorModes && !ok; c++)
            ok = (mesh.colors & (1 << c)) && mesh_format_carries(i, c);
        ImGui::BeginDisabled(!ok);
        if (ui::CheckboxRaw(kMeshFormats[i], &on)) {
            mesh.formats = on ? (mesh.formats | (1 << i)) : (mesh.formats & ~(1 << i));
            moved = true;
        }
        ImGui::EndDisabled();
        ImGui::PopID();
    }
    return moved;
}


// The dataset folder: written by the Dataset stage, read by the Train stage,
// so one field serves both and the two cannot point at different places.
void GuiApp::draw_batch_row_common_dataset(BatchRow& row, int index) {
    ui::Text(msg::batch_dataset_label);
    const float bw = ImGui::GetFrameHeight() + 6.0f;
    ImGui::SetNextItemWidth(-bw);
    if (ui::InputTextWithHintRaw("##ds",
                                 row.does(BatchStage::Dataset)
                                     ? msg::batch_dataset_auto_hint
                                     : msg::batch_dataset_hint,
                                 &row.dataset))
        batch_edited();
    ImGui::SameLine(0, 2);
    if (ui::ButtonRaw("...##ds")) {
        _pick_row = index;
        open_pick(PickAction::BatchDataset, msg::batch_pick_dataset.get(),
                  FileDialog::Mode::Folder, {}, row.dataset);
    }
    if (row.does(BatchStage::Dataset) && row.dataset.empty()) {
        const std::string ws = batch_dataset_workspace(row);
        if (!ws.empty()) ui::TextDisabledRaw(ws);
    }
}


bool GuiApp::draw_batch_train_preset(BatchPreset& p, const char* id, int row,
                                     int slot) {
    bool moved = false;
    const std::string preview = p.path.empty() ? preset_label(p.name) : p.name;
    const bool open = ui::BeginComboRaw(id, preview.c_str());
    std::string desc;
    if (p.path.empty()) {
        desc = preset_help(p.name);
    } else {
        for (const TrainPreset& t : _train_presets.items)
            if (t.path == p.path) { desc = t.description; break; }
    }
    preset_hover(desc, p.path);
    if (!open) return moved;
    refresh_presets();

    ui::SeparatorText(msg::preset_builtin_group);
    for (const auto& b : kTrainPresets) {
        const bool sel = p.path.empty() && p.name == b.name;
        if (ui::SelectableRaw(preset_label(b.name), sel)) {
            p.path.clear();
            p.name = b.name;
            moved = true;
        }
        preset_hover(preset_help(b.name), {});
        if (sel) ImGui::SetItemDefaultFocus();
    }

    ui::SeparatorText(msg::preset_user_group);
    if (_train_presets.items.empty()) ui::TextDisabled(msg::preset_none_saved);
    for (const auto& t : _train_presets.items) {
        ImGui::PushID(t.path.c_str());
        const bool sel = p.path == t.path;
        if (ui::SelectableRaw(t.name, sel)) {
            p.path = t.path;
            p.name = t.name;
            moved = true;
        }
        preset_hover(t);
        if (sel) ImGui::SetItemDefaultFocus();
        ImGui::PopID();
    }

    ImGui::Separator();
    if (ui::Selectable(msg::batch_preset_from_file)) {
        _pick_row = row;
        _pick_slot = slot;
        open_pick(PickAction::BatchPresetFile, msg::preset_pick_file.get(),
                  FileDialog::Mode::File, {".json"});
    }
    ImGui::EndCombo();
    return moved;
}


void GuiApp::draw_batch_issues() {
    for (int i = 0; i < (int)_batch.size(); i++) {
        if (!_batch[(size_t)i].enabled) continue;
        for (const BatchIssue& issue : _batch[(size_t)i].issues) {
            ui::TextColoredWrappedRaw(
                issue.fatal ? kErr : kWarn,
                i18n::format(msg::batch_issue_row,
                             {(long long)(i + 1), batch_issue_line(issue)}));
        }
    }
}


// A box that grows with what is in it. A real command is a curl invocation
// with a JSON body, which nobody writes on one line; the empty state is still
// one line, because that is what the screen looks like for everyone else.
static ImVec2 batch_cmd_box_size(const std::string& text, float room) {
    const ImGuiStyle& st = ImGui::GetStyle();
    const float least = px(560.0f);
    float widest = 0.0f;
    int lines = 0;
    for (size_t at = 0; at <= text.size();) {
        const size_t nl = text.find('\n', at);
        const std::string line =
            text.substr(at, nl == std::string::npos ? nl : nl - at);
        widest = std::max(widest, ImGui::CalcTextSize(line.c_str()).x);
        lines++;
        if (nl == std::string::npos) break;
        at = nl + 1;
    }
    return ImVec2(std::clamp(widest + st.FramePadding.x * 4.0f, least,
                             std::max(least, room)),
                  ImGui::GetTextLineHeight() * (float)std::clamp(lines, 1, 12) +
                      st.FramePadding.y * 2.0f);
}

// The command a finished queue runs. What {message} means is on the screen
// rather than behind a hover, because nobody guesses a placeholder.
void GuiApp::draw_batch_command() {
    ui::SeparatorText(msg::batch_cmd_title);
    // Split rather than empty: a field holding only spaces names no program,
    // and a Test button that does nothing at all is worse than a greyed one.
    const bool can_test =
        !split_args(_batch_cmd).empty() && !_batch_cmd_run.busy();
    const float button_w =
        ImGui::CalcTextSize(msg::batch_cmd_test.get()).x +
        ImGui::GetStyle().FramePadding.x * 2.0f;
    const float room = ImGui::GetContentRegionAvail().x - button_w -
                       ImGui::GetStyle().ItemSpacing.x - px(8.0f);
    // The content is a command line: English wherever the interface is.
    ui::InputTextMultilineRaw("##batchcmd", &_batch_cmd,
                              batch_cmd_box_size(_batch_cmd, room));
    if (ImGui::IsItemDeactivatedAfterEdit()) save_settings();
    if (_batch_cmd.empty())
        ui::hint_over_last_item_raw("python notify_me.py --message \"{message}\"");
    ImGui::SameLine();
    ImGui::BeginDisabled(!can_test);
    if (ui::Button(msg::batch_cmd_test))
        run_batch_command(msg::batch_cmd_test_message.get());
    ui::help_on_hover_disabled(msg::batch_cmd_test_help);
    ImGui::EndDisabled();
    ui::TextDisabledWrapped(msg::batch_cmd_help);
}


// What a start would actually run, in order. The whole point of the screen is
// that most of it does not exist yet, so seeing the list beforehand is the
// only way to tell a five-hour queue from a mistake.
void GuiApp::draw_batch_plan() {
    if (_batch.empty()) return;
    if (!ui::CollapsingHeader(msg::batch_plan_title)) return;
    const std::vector<BatchTask> plan =
        _batch_active ? _batch_tasks : batch_plan(_batch);
    if (plan.empty()) {
        ui::TextDisabled(msg::batch_plan_empty);
        return;
    }
    for (size_t i = 0; i < plan.size(); i++) {
        const BatchTask& t = plan[i];
        if (t.row < 0 || t.row >= (int)_batch.size()) continue;
        const BatchRow& row = _batch[(size_t)t.row];
        const long long n = (long long)i + 1;
        std::string line;
        switch (t.stage) {
            case BatchStage::Dataset:
                line = i18n::format(msg::batch_plan_dataset,
                                    {n, batch_dataset_workspace(row)});
                break;
            case BatchStage::Train: {
                const BatchPreset p = batch_run_of(row, t.variant).preset;
                const std::string name =
                    p.path.empty() ? preset_label(p.name) : p.name;
                line = row.does(BatchStage::Dataset)
                           ? i18n::format(msg::batch_plan_train_new, {n, name})
                           : i18n::format(msg::batch_plan_train,
                                          {n, row.dataset, name});
                break;
            }
            case BatchStage::Mesh: {
                if (!row.model.empty()) {
                    line = i18n::format(msg::batch_plan_mesh, {n, row.model});
                } else if (batch_num_runs(row) > 1) {
                    // Which run, once a row meshes some of them and not others.
                    const BatchPreset p = batch_run_of(row, t.variant).preset;
                    line = i18n::format(msg::batch_plan_mesh_run,
                                        {n, p.path.empty() ? preset_label(p.name)
                                                           : p.name});
                } else {
                    line = i18n::format(msg::batch_plan_mesh_new, {n});
                }
                break;
            }
        }
        ImVec4 color = kDim;
        if (_batch_active) {
            if (t.status == BatchStatus::Done) color = kOk;
            else if (t.status == BatchStatus::Failed) color = kErr;
            else if (t.status == BatchStatus::Running) color = kWarn;
        }
        ui::TextColoredWrappedRaw(color, line);
    }
}


// How far through the running task its own runner says it is. Every stage
// answers in its own terms, which is why this is not one number somewhere.
float GuiApp::batch_task_fraction() {
    if (!_batch_active || _batch_current < 0 ||
        _batch_current >= (int)_batch_tasks.size())
        return -1.0f;
    switch (_batch_tasks[(size_t)_batch_current].stage) {
        case BatchStage::Dataset: {
            // The steps a dataset run goes through, with the running one's own
            // fraction inside its slot. Both engines report through that list.
            RunProgress* p = dataset_steps();
            const int at = std::clamp((int)p->current(), 0, kNumStages - 1);
            const StageProgress sp = p->stage((Stage)at);
            const float inner = sp.fraction >= 0.0f ? sp.fraction : 0.5f;
            return std::min(1.0f, ((float)at + inner) / (float)kNumStages);
        }
        case BatchStage::Train: {
            const spirula::TrainerProgress pr = _runner.latest_progress();
            if (pr.total_steps <= 0) return -1.0f;
            return std::min(1.0f, (float)(pr.step + 1) / (float)pr.total_steps);
        }
        case BatchStage::Mesh: return _mesh.progress();
    }
    return -1.0f;
}

double GuiApp::batch_task_elapsed() const {
    if (!_batch_active || _batch_current < 0 ||
        _batch_current >= (int)_batch_tasks.size())
        return 0.0;
    return std::max(0.0, ImGui::GetTime() -
                             _batch_tasks[(size_t)_batch_current].started_at);
}

const BatchRow* GuiApp::batch_running_row() const {
    if (!_batch_active || _batch_current < 0 ||
        _batch_current >= (int)_batch_tasks.size())
        return nullptr;
    const int row = _batch_tasks[(size_t)_batch_current].row;
    if (row < 0 || row >= (int)_batch.size()) return nullptr;
    return &_batch[(size_t)row];
}

void GuiApp::draw_batch_stop_buttons() {
    ImGui::BeginDisabled(_batch_stop_after);
    if (ui::Button(msg::batch_stop_after, ImVec2(-8, 0)))
        _batch_stop_after = true;
    ui::help_on_hover(msg::batch_stop_after_help);
    ImGui::EndDisabled();
    if (ui::Button(msg::batch_stop_now, ImVec2(-8, 0))) {
        _batch_stop_after = true;
        _batch_stop_now = true;
        if (_batch_current >= 0 && _batch_current < (int)_batch_tasks.size()) {
            switch (_batch_tasks[(size_t)_batch_current].stage) {
                case BatchStage::Dataset: cancel_dataset_job(); break;
                case BatchStage::Mesh:    _mesh.cancel(); break;
                default:                  _runner.request_stop(); break;
            }
        }
    }
    ui::help_on_hover(msg::batch_stop_now_help);
}

// What is running, on the batch screen and on the work screen that owns the
// running task: two bars, what each has left, and the ways to stop. The work
// screen shows the job; this says where in the QUEUE it is.
void GuiApp::draw_batch_progress() {
    ui::SeparatorText(msg::batch_title);
    const float frac = batch_task_fraction();
    const double elapsed = batch_task_elapsed();
    const BatchProgress p =
        batch_progress(_batch_tasks, _batch_current, frac, elapsed);

    ui::Text(msg::batch_running_banner,
             {(long long)(_batch_current + 1), (long long)_batch_tasks.size()});
    ui::ProgressBarRaw(p.total > 0 ? (float)p.done / (float)p.total : 0.0f,
                       ImVec2(-8, 0), nullptr);
    ui::TextDisabled(msg::batch_eta_total,
                     {spirula::i18n::format_duration_coarse(p.remaining)});

    if (_batch_current >= 0 && _batch_current < (int)_batch_tasks.size()) {
        const BatchTask& t = _batch_tasks[(size_t)_batch_current];
        ImGui::PushTextWrapPos();
        ui::TextDisabled(batch_stage_name(t.stage));
        if (t.row >= 0 && t.row < (int)_batch.size()) {
            const std::string what = batch_row_summary(_batch[(size_t)t.row]);
            if (!what.empty()) ui::TextDisabledRaw(what);
        }
        ImGui::PopTextWrapPos();
        ui::ProgressBarRaw(frac >= 0.0f ? frac : 0.0f, ImVec2(-8, 0), nullptr);
        ui::TextDisabled(msg::batch_eta_task,
                         {spirula::i18n::format_duration_coarse(elapsed),
                          spirula::i18n::format_duration_coarse(p.task_remaining)});
    }
    if (_batch_stop_after) ui::TextColored(kWarn, msg::batch_stopping);

    // One button, whichever way round the two screens are: the list from a
    // work screen, the running step from the list.
    if (_screen == Screen::Batch) {
        if (ui::Button(msg::batch_show_training, ImVec2(-8, 0))) {
            const BatchStage st =
                _batch_current >= 0 && _batch_current < (int)_batch_tasks.size()
                    ? _batch_tasks[(size_t)_batch_current].stage
                    : BatchStage::Train;
            switch (st) {
                case BatchStage::Dataset: _screen = Screen::NewDataset; break;
                case BatchStage::Mesh:    _screen = Screen::Mesh; break;
                default:                  _screen = Screen::Train; break;
            }
        }
    } else if (ui::Button(msg::batch_show_list, ImVec2(-8, 0))) {
        _screen = Screen::Batch;
    }
    draw_batch_stop_buttons();
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
        open_pick(PickAction::OpenDataset, msg::menu_open_dataset.get(),
                  FileDialog::Mode::Folder);
    }
    ImGui::EndDisabled();

    // Vulkan builds share the native picker with every built-in workflow.
#ifdef SS_BACKEND_VULKAN
    draw_device_picker();
#else
    ui::SeparatorText(msg::section_device);
    {
        int n_dev = backend::device_count();
        int cur = backend::device_current();
        backend::DeviceInfo curd = backend::device_info(cur);
        ImGui::BeginDisabled(_cuda_device_locked || busy || n_dev == 0);
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
        if (_cuda_device_locked)
            ui::TextColoredWrapped(kDim, msg::device_cuda_locked);
    }
#endif

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

        if (_cfg.save_full_checkpoint != parse_before.save_full_checkpoint) {
            _keep_full_ckpt = _cfg.save_full_checkpoint;
            save_settings();
        }
        if (!parse_settings_equal(parse_before, _cfg)) _parse_dirty = true;
    }

    // ---- controls + metrics ----
    ui::SeparatorText(msg::section_training);
    draw_train_controls();
    draw_metrics();
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
void GuiApp::draw_preset_picker() {
    static_assert(sizeof(kTrainPresets) / sizeof(kTrainPresets[0]) ==
                      i18n::msg::train::kNumPresetText,
                  "config/TrainConfig.h and i18n/catalog/Train.h disagree "
                  "about how many presets there are");

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
            if (ui::SelectableRaw(preset_label(p.name), sel))
                apply_preset(p.name);
            preset_hover(preset_help(p.name), {});
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ui::SeparatorText(msg::preset_user_group);
        if (_train_presets.items.empty()) ui::TextDisabled(msg::preset_none_saved);
        for (const auto& p : _train_presets.items) {
            ImGui::PushID(p.path.c_str());
            bool sel = _train_presets.file == p.path;
            if (ui::SelectableRaw(p.name, sel)) apply_user_preset(p);
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

    ImGui::PushTextWrapPos(px(460.0f));
    ui::Text(msg::preset_delete_confirm, {*display});
    ui::TextDisabledRaw(*file);
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    if (ui::Button(msg::preset_delete_button, ImVec2(150, 0))) {
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
                   ImVec2(150, 0))) {
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
    if (ui::Button(msg::cancel, ImVec2(150, 0))) {
        _preset_save_shown = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
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
        open_pick(PickAction::OutputPrefix, msg::pick_output_folder.get(),
                  FileDialog::Mode::Folder, {}, _cfg.output_dir_prefix);
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
        if (_cfg.primitive == "3dgut")
            ui::TextColoredWrapped(kWarn, msg::opt_primitive_3dgut_warn);
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
        // Unset, the mode is whatever the parsed dataset resolved it to --
        // cut out for masks that are only the images' alpha (TrainerCore).
        bool cut_out = _cfg.apply_loss_for_mask.value_or(false);
        const TrainRunner::Phase ph = _runner.phase();
        if (!_cfg.apply_loss_for_mask.has_value() &&
            (ph == TrainRunner::Phase::Ready || ph == TrainRunner::Phase::Training ||
             ph == TrainRunner::Phase::Done))
            if (auto* s = _runner.session())
                cut_out = s->cfg.apply_loss_for_mask.value_or(false);
        int mi = !_cfg.load_masks ? 2 : cut_out ? 1 : 0;
        ImGui::SetNextItemWidth(w);
        if (ui::Combo(msg::opt_mask_mode, &mi,
                      {&msg::opt_mask_mode_exclude,
                       &msg::opt_mask_mode_cut_out,
                       &msg::opt_mask_mode_off})) {
            _cfg.load_masks = mi != 2;
            _cfg_ui.touched.insert("load_masks");
            // Off says nothing about what masks mean, so it keeps auto.
            if (mi != 2) {
                _cfg.apply_loss_for_mask = mi == 1;
                _cfg_ui.touched.insert("apply_loss_for_mask");
            }
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
    if (_cfg.distraction_robustness != "off")
        ui::TextColoredWrapped(kWarn, msg::opt_distraction_warn);
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
            // A batch owns the runner between its tasks, so the queue's own
            // next row is what starts -- never a click here.
            bool can_start = !_batch_active &&
                             (ph == TrainRunner::Phase::Ready ||
                              ph == TrainRunner::Phase::Done ||
                              ph == TrainRunner::Phase::TrainError);
            ImGui::BeginDisabled(!can_start);
            if (ui::Button(ph == TrainRunner::Phase::Done ? msg::train_again
                                                          : msg::start_training,
                           ImVec2(-8, 36)))
                start_training();
            ImGui::EndDisabled();
            // What a finished run is for: a picture of it, or a cleaned copy.
            if (ph == TrainRunner::Phase::Done && _runner.saved_on_stop() &&
                _runner.session() && !_batch_active) {
                const std::string out = _runner.session()->out_dir.string();
                const float half = (ImGui::GetContentRegionAvail().x - 8.0f -
                                    ImGui::GetStyle().ItemSpacing.x) * 0.5f;
                if (ui::Button(rmsg::train_render, ImVec2(half, 30))) {
                    _render_after_open = true;
                    request_open_splat(out);
                }
                ui::help_on_hover(rmsg::train_render_help);
                ImGui::SameLine();
                if (ui::Button(rmsg::train_edit, ImVec2(half, 30))) {
                    _edit_after_open = true;
                    request_open_splat(out);
                }
                ui::help_on_hover(rmsg::train_edit_help);
            }
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
    static std::vector<float> steps, psnr, splats;
    steps.clear();
    psnr.clear();
    splats.clear();
    auto take = [&](size_t i) {
        steps.push_back((float)pts[i].step);
        psnr.push_back(pts[i].psnr);
        splats.push_back(pts[i].num_splats);
    };
    size_t last_i = 0;
    for (size_t i = 0; i < pts.size(); i += stride) { take(i); last_i = i; }
    // The stride can stop short of the newest point, which on a step axis
    // leaves the curve ending before the score printed beside it.
    if (last_i != pts.size() - 1) take(pts.size() - 1);
    const auto& last = pts.back();
    // PSNR / SSIM / loss are the metric names the literature and the logs use;
    // they are not translated, only the numbers change.
    ui::PlotLinesRaw(steps.data(), psnr.data(), (int)psnr.size(),
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
    // ItemSpacing twice: once above the splitter's grab and once below it.
    // Counting it once left the body a spacing too tall, which the host window
    // answered by becoming scrollable by exactly that much.
    if (log_h <= 0.0f) return 0.0f;
    return -(log_h + splitter_extent() + ImGui::GetStyle().ItemSpacing.y);
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

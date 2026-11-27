// Opening datasets, models and render projects; moving between screens and
// back; and the guard in front of anything that would stop running work.

#include "app/gui/GuiApp.h"

#include "app/CrashLog.h"
#include "data/RoiDocument.h"
#include "data/SparseEdit.h"
#include "dense/Artifact.h"
#include "i18n/catalog/Gui.h"
#include "i18n/catalog/Render.h"

#include <algorithm>
#include <filesystem>
#include <utility>

namespace fs = std::filesystem;
namespace i18n = spirula::i18n;
namespace msg = spirula::i18n::msg::gui;
namespace rmsg = spirula::i18n::msg::render;

namespace gui {

bool GuiApp::training_busy() const {
    TrainRunner::Phase ph = _runner.phase();
    return ph == TrainRunner::Phase::Training ||
           ph == TrainRunner::Phase::Evaluating ||
           ph == TrainRunner::Phase::Preparing;
}

void GuiApp::open_dataset(std::string dir, std::string image_dir,
                          std::string mask_dir, bool mask_flipped,
                          bool keep_log, std::string seed) {
    if (dir.empty() || native_work_busy()) return;
    app::set_crash_note("opening dataset " + dir);
    close_native_previews();
    close_mesh_preview();
    close_splat();
    // A different dataset clears a log that is about something else. Kept on
    // a reload of the same one, and on the handoff out of a reconstruction,
    // whose log is the first thing to read if the result seems wrong.
    if (dir != _cfg.data && !keep_log) clear_log();
    // Another dataset's region choice and resume target mean nothing here.
    if (dir != _cfg.data) { _cfg.roi_region.clear(); _cfg.resume.clear(); }
    _cfg.data = dir;
    // The trainer picks a dense cloud up by itself; only the dense step's
    // "not for training" needs saying, and only for the dataset it made.
    if (!_app_seed.empty() && _cfg.seed_pointcloud == _app_seed) {
        _cfg.seed_pointcloud.clear(); _app_seed.clear();
    }
    // An edit sent here from the editor starts from what was just edited,
    // whatever the dense options say.
    if (!seed.empty()) {
        _app_seed = seed;
        _cfg.seed_pointcloud = seed;
    } else if (dir == _workspace && _dense.enable && !_dense.use_for_training &&
               !_cfg_ui.touched.count("seed_pointcloud") && _cfg.seed_pointcloud.empty()) {
        _app_seed = spirula::dense::kSparseSeed;
        _cfg.seed_pointcloud = _app_seed;
    }
    // A run hands its (possibly external) image and mask folders over in
    // memory, so photos indexed in place keep their masks there too. Otherwise
    // the dataparser defaults apply, editable under Advanced.
    _cfg.image_dir = !image_dir.empty() ? image_dir : _defaults.image_dir;
    _cfg.mask_dir = !mask_dir.empty() ? mask_dir : _defaults.mask_dir;
    _cfg.flip_mask = mask_flipped;
    // Output defaults to beside the dataset, not a CWD-relative "outputs" that
    // a desktop launch hides. Follows the dataset unless the user changed it.
    if (_cfg.output_dir_prefix == "outputs" ||
        _cfg.output_dir_prefix.empty() ||
        _cfg.output_dir_prefix == _defaults.output_dir_prefix)
        _cfg.output_dir_prefix = (fs::path(dir) / "outputs").string();
    _defaults.data = _cfg.data;
    _defaults.image_dir = _cfg.image_dir;
    _defaults.mask_dir = _cfg.mask_dir;
    _defaults.output_dir_prefix = _cfg.output_dir_prefix;
    // However it was opened -- picked, dropped, from the recents list -- this
    // is where the picker starts next time.
    remember_dir("dataset", dir);
    remember(RecentKind::Dataset, dir);
    detach_session_views();
    // Training is the end of the dataset screen's business with the run, so
    // this is where what it left for the screen to read goes.
    reset_dataset_preview();
    close_source_preview();
    _parse_dirty = false;
    _runner.load_dataset(_cfg, _preset);
    go_to(Screen::Train);
}

void GuiApp::open_edited_dataset() {
    const std::string dataset = std::exchange(_edit_train_dataset, {});
    const std::string source = std::exchange(_edit_train_source, {});
    const std::string seed = std::exchange(_edit_train_seed, {});
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
    open_dataset(dataset, from.image_dir, from.mask_dir, from.mask_flipped, false, seed);
}

void GuiApp::request_open_dataset(std::string dir) {
    request_open_dataset(DatasetFolders{std::move(dir)});
}

void GuiApp::request_open_dataset(const DatasetFolders& f, bool keep_log) {
    request_leave(Screen::Train, [this, f, keep_log] {
        open_dataset(f.dir, f.image_dir, f.mask_dir, f.mask_flipped, keep_log);
    }, Pending::OpenDataset);
}

void GuiApp::open_dataset_screen(const std::string& dir) {
    if (dir.empty()) return;
    std::error_code ec;
    if (!_workspace.empty() && fs::equivalent(fs::u8path(_workspace), fs::u8path(dir), ec)) {
        go_to(Screen::NewDataset);
        return;
    }
    open_reconstruction(dir);
}

void GuiApp::request_go_home() {
    request_leave(Screen::Home, [this] { go_to(Screen::Home); }, Pending::GoHome);
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
    _viewer_base_mode = ViewMode::View;
    if (_edit_after_open) {
        _compare.edit_first_when_ready();
        _edit_after_open = false;
        _viewer_base_mode = ViewMode::Edit;
    }
    if (_render_after_open) {
        _compare.render_first_when_ready();
        _render_after_open = false;
        _viewer_base_mode = ViewMode::Render;
    }
    if (!_render_project_after_open.empty()) {
        _compare.render_project_when_ready(_render_project_after_open);
        _render_project_after_open.clear();
        _viewer_base_mode = ViewMode::Render;
    }
    remember_dir("model", path);
    remember(RecentKind::Model, path);
    go_to(Screen::Viewer);
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
    remember_dir("model", path);
    remember(RecentKind::Model, path);
}

void GuiApp::request_open_splat(std::string path) {
    request_leave(Screen::Viewer, [this, path] {
        // A model opened later must not arrive in edit mode by surprise.
        if (native_work_busy()) forget_open_intents();
        else open_splat(path);
    }, Pending::OpenSplat);
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
    remember(RecentKind::Project, path);
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
    _render_discarded = false;
}
void GuiApp::close_native_previews() {
    // Back to idle on the device before whatever closes these opens the next user.
    _source_view.yield_gpu();
    _segment.close();
    _geometry_panel.close();
    _partition_panel.close();
}

// Every site that starts inference (or tears it down) calls this, never the
// bare close: the editor's SAM session shares the pool's slots and one
// unsynchronised stream, and a dataset run ends in nn::shutdown().
void GuiApp::stop_inference_users() {
    close_native_previews();
    _mask_editor.sam_yield();
}

void GuiApp::detach_session_views() {
    _viewport.detach();
    _images.detach();
    // Every caller is about to take the session away; land on the 3D view,
    // which shows sparse points as soon as the dataset parses, rather than on
    // the image view's empty message.
    _preview_images = false;
}

void GuiApp::request_close() {
    if (_compare.edit_dirty()) {
        _edit_exit_confirm = true;
        return;
    }
    if (!_render_discarded && _compare.render().dirty()) {
        _render_exit_then = [this] { request_close(); };
        _render_exit_confirm = true;
        return;
    }
    if (training_busy()) {
        _pending = Pending::Quit;
        _pending_then = [this] { _quit = true; };
        _open_confirm = true;
        return;
    }
    if (!running_work().empty()) {
        _quit_ask = true;
        return;
    }
    _quit = true;
}

void GuiApp::run_pending_if_stopped() {
    if (_pending == Pending::None || native_work_busy()) return;
    // A queue told to stop now winds down over the next frame or two.
    if (_batch_active && _batch_stop_now) return;
    // Only fire once the user confirmed the stop (or training was never
    // busy); a dismissed modal must not leave a delayed action armed.
    if (!_stop_confirmed) {
        _pending = Pending::None;
        _pending_then = nullptr;
        forget_open_intents();
        return;
    }
    _stop_confirmed = false;
    _pending = Pending::None;
    if (std::function<void()> then = std::exchange(_pending_then, nullptr)) then();
}

// ===========================================================================
// Screens and the way back
// ===========================================================================

const i18n::Msg& GuiApp::screen_name(Screen s) {
    switch (s) {
        case Screen::NewDataset: return msg::screen_dataset;
        case Screen::Train:      return msg::screen_training;
        case Screen::Viewer:     return msg::screen_viewer;
        case Screen::Batch:      return msg::screen_batch;
        case Screen::Mesh:       return msg::screen_meshing;
        case Screen::Home:       break;
    }
    return msg::screen_home;
}

void GuiApp::go_to(Screen s) {
    if (s == _screen) return;
    // What the screen being left holds that no other screen shows.
    switch (_screen) {
        case Screen::Viewer:
            // A route that closed the models first had them noted on its way.
            if (_compare.count() > 0) remember_viewer();
            close_splat();
            break;
        case Screen::NewDataset:
            // GL buffers and a decoding thread for a screen that is not up.
            if (!native_work_busy()) reset_dataset_preview();
            close_source_preview();
            break;
        default:
            break;
    }
    if (s == Screen::Home) {
        _nav_back.clear();
        _viewer_reopen = {};
    } else {
        const auto at = std::find(_nav_back.begin(), _nav_back.end(), s);
        if (at != _nav_back.end()) _nav_back.erase(at, _nav_back.end());
        else _nav_back.push_back(_screen);
    }
    _screen = s;
}

void GuiApp::follow_to(Screen s) {
    if (s == _screen) return;
    // From the list, the list is where Back returns to.
    if (_screen == Screen::Batch) {
        go_to(s);
        return;
    }
    const std::vector<Screen> back = _nav_back;
    go_to(s);
    _nav_back = back;
}

GuiApp::Screen GuiApp::back_target() const {
    return _nav_back.empty() ? Screen::Home : _nav_back.back();
}

GuiApp::ViewMode GuiApp::viewer_mode() const {
    if (_compare.rendering() >= 0) return ViewMode::Render;
    if (_compare.editing() >= 0) return ViewMode::Edit;
    return ViewMode::View;
}

const i18n::Msg& GuiApp::back_name() const {
    if (_screen == Screen::Viewer && viewer_mode() > _viewer_base_mode)
        return viewer_mode() == ViewMode::Render && _compare.editing() >= 0
                   ? msg::screen_editing : msg::screen_viewer;
    return screen_name(back_target());
}

void GuiApp::request_back() {
    if (_screen == Screen::Home) return;
    if (_screen == Screen::Viewer && viewer_mode() > _viewer_base_mode) {
        if (viewer_mode() == ViewMode::Edit) {
            _compare.confirm_discard_edits([this] { _compare.end_edit(); });
        } else if (_compare.render().exporting()) {
            _leave.work = Work::Render;
            _leave.go = [this] { _compare.end_render(); };
            _leave.open = true;
        } else {
            _compare.end_render();
        }
        return;
    }
    const Screen to = back_target();
    request_leave(to, [this, to] {
        go_to(to);
        if (to == Screen::Viewer) reopen_viewer();
    });
}

void GuiApp::request_screen(Screen s) {
    if (s == _screen) return;
    request_leave(s, [this, s] { go_to(s); });
}

void GuiApp::reopen_viewer() {
    const ViewerReopen r = std::exchange(_viewer_reopen, {});
    if (_compare.count() > 0 || native_work_busy()) return;
    if (r.mesh) {
        open_mesh_preview();
        return;
    }
    if (r.paths.empty()) return;
    open_splat(r.paths[0]);
    for (size_t i = 1; i < r.paths.size(); i++) add_splat(r.paths[i]);
}

GuiApp::Work GuiApp::screen_work(Screen s) const {
    switch (s) {
        case Screen::NewDataset: return dataset_busy() ? Work::Dataset : Work::None;
        case Screen::Mesh:       return _mesh.busy() ? Work::Meshing : Work::None;
        case Screen::Viewer:
            return _compare.render().exporting() ? Work::Render : Work::None;
        case Screen::Batch:      return _batch_active ? Work::Batch : Work::None;
        default:                 return Work::None;
    }
}

bool GuiApp::batch_view_switch(Screen to) const {
    if (!_batch_active) return false;
    auto works = [](Screen s) {
        return s == Screen::NewDataset || s == Screen::Train || s == Screen::Mesh;
    };
    return (_screen == Screen::Batch && works(to)) || (to == Screen::Batch && works(_screen));
}

void GuiApp::request_leave(Screen to, std::function<void()> go, Pending reason) {
    const bool leaving = to != _screen;
    if (_screen == Screen::Viewer && leaving) {
        if (_compare.edit_dirty()) {
            _compare.confirm_discard_edits([this, to, go, reason] { request_leave(to, go, reason); });
            return;
        }
        if (!_render_discarded && _compare.render().dirty()) {
            _render_exit_then = [this, to, go, reason] { request_leave(to, go, reason); };
            _render_exit_confirm = true;
            return;
        }
    }
    if (!batch_view_switch(to)) {
        // Opening another model on the viewer ends its render as surely as leaving.
        const Work w = leaving || _screen == Screen::Viewer ? screen_work(_screen) : Work::None;
        if (w != Work::None) {
            _leave.work = w;
            _leave.go = std::move(go);
            _leave.open = true;
            return;
        }
        // A dataset still parsing is stopped without asking: nothing is lost.
        const bool engine = _screen == Screen::Train || to == Screen::Train || to == Screen::Viewer;
        if (_runner.phase() == TrainRunner::Phase::Loading && engine) {
            _pending = reason;
            _pending_then = std::move(go);
            _stop_confirmed = true;
            _runner.request_stop();
            return;
        }
        if (training_busy()) {
            _pending = reason;
            _pending_then = std::move(go);
            _open_confirm = true;
            return;
        }
    }
    if (_screen == Screen::Viewer && leaving && _compare.count() > 0) remember_viewer();
    go();
}

void GuiApp::remember_viewer() {
    _viewer_reopen.mesh = _mesh_preview_open;
    _viewer_reopen.paths = _compare.paths();
}

void GuiApp::forget_open_intents() {
    _edit_after_open = false;
    _render_after_open = false;
    _render_project_after_open.clear();
}

void GuiApp::stop_work(Work w) {
    switch (w) {
        case Work::Dataset: cancel_dataset_job(); break;
        case Work::Meshing:
            if (_batch_active) _batch_stop_after = _batch_stop_now = true;
            _mesh.cancel();
            break;
        case Work::Batch:   stop_batch_now(); break;
        // Leaving the viewer closes the render, and that stops it.
        case Work::Render:
        case Work::None:    break;
    }
}

std::vector<const i18n::Msg*> GuiApp::running_work() const {
    std::vector<const i18n::Msg*> out;
    if (dataset_busy()) out.push_back(&msg::work_dataset);
    if (_mesh.busy()) out.push_back(&msg::work_meshing);
    if (_batch_active) out.push_back(&msg::work_batch);
    if (_compare.render().exporting()) out.push_back(&msg::work_render);
    if (_compare.edit_busy() || _compare.edit().busy()) out.push_back(&msg::work_edit);
    if (_merge_busy.load()) out.push_back(&msg::work_merge);
    const bool downloading = _download.state() == FileDownload::State::Running ||
        _geom_download.running() || _feat_download.running() ||
        _dense_download.state() == FileDownload::State::Running ||
        _font_download.state() == FileDownload::State::Running;
    if (downloading) out.push_back(&msg::work_download);
    return out;
}

}  // namespace gui

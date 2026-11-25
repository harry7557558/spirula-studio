// Opening datasets, models and render projects, and the stop-and-save guard
// in front of anything that would end a training session.

#include "app/gui/GuiApp.h"

#include "app/CrashLog.h"
#include "data/RoiDocument.h"
#include "data/SparseEdit.h"
#include "dense/Artifact.h"
#include "i18n/catalog/Render.h"

#include <filesystem>

namespace fs = std::filesystem;
namespace i18n = spirula::i18n;
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
    _screen = Screen::Train;
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
    remember_dir("model", path);
    remember(RecentKind::Model, path);
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
    remember_dir("model", path);
    remember(RecentKind::Model, path);
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
        case Pending::StartBatch: begin_batch(_pending_batch_skip); break;
        default: break;
    }
}

}  // namespace gui

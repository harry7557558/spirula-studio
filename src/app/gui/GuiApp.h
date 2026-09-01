#pragma once

// GuiApp -- top-level UI: screens, layout, state wiring
// between the config editor, COLMAP runner, train runner, and the native
// viewport.

#include "backend/api/BackendRuntime.h"
#include "config/TrainConfig.h"
#include "app/gui/BatchTrain.h"
#include "app/gui/ColmapRunner.h"
#include "app/gui/CompareView.h"
#include "app/gui/Fonts.h"
#include "app/gui/ConfigUI.h"
#include "app/gui/FileDialog.h"
#include "app/gui/FeatureWatcher.h"
#include "app/gui/FilmReel.h"
#include "app/gui/GeometryPanel.h"
#include "app/gui/ImageCompare.h"
#include "app/gui/MatchMatrix.h"
#include "app/gui/PairPreview.h"
#include "app/gui/SfmProgress.h"
#include "app/gui/Layout.h"
#include "app/gui/MeshRunner.h"
#include "app/gui/ModelCache.h"
#include "app/gui/SegmentPanel.h"
#include "app/gui/SfmRunner.h"
#include "app/gui/TrainPreset.h"
#include "app/gui/TrainRunner.h"
#include "app/gui/ViewportPanel.h"

#include <deque>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace gui {

class GuiApp {
public:
    static constexpr float kDefaultPanelW = 420.0f;
    static constexpr float kDefaultLogH = 150.0f;
    static constexpr float kDefaultPreviewH = 260.0f;
    static constexpr float kDefaultDsPanelW = 560.0f;

    GuiApp();
    ~GuiApp();

    // Draw one frame (between ImGui::NewFrame and ImGui::Render).
    void frame();

    // Window close button pressed; may open a confirmation dialog instead
    // of quitting when training is in flight.
    void request_close();
    bool wants_exit() const { return _quit; }

    // Stop worker threads + free GL resources; call while the GL context is
    // still current.
    void shutdown();

    // The font atlas. GuiMain calls ensure() between frames -- swapping a face
    // invalidates every ImFont pointer, so it cannot happen mid-frame.
    FontSet& fonts() { return _fonts; }

    // The window's content scale, from GLFW. The interface size is this times
    // the user's preference (or times a factor read off the window, which is
    // the default).
    void set_dpi_scale(float s) { _scale.set_dpi(s); }

private:
    enum class Screen { Home, NewDataset, Train, Viewer, Batch, Mesh };
    enum class PickAction {
        None, OpenDataset, SourceImages, SourceVideo, SourceReplace, Workspace,
        OutputPrefix, VocabTree, MaskModelFile, SplatFile,
        PresetFile, PresetSaveFolder, BatchDataset, BatchOutput, BatchPresetFile,
        MeshSource, MeshPhotos, MeshOutput, AddSplatFile
    };
    // Which reconstruction back end the New Dataset screen runs.
    enum class Engine { BuiltIn, Colmap };
    // Session-destroying actions deferred behind the "stop training?"
    // confirmation (and executed once the stop has completed).
    // StopHere is the Stop button itself: nothing follows the stop, but it
    // goes through the same confirmation so that "without saving" is offered
    // on every route out of a run.
    enum class Pending {
        None, GoHome, OpenDataset, OpenSplat, Quit, StartBatch, StopHere
    };

    // ---- persistence (recents + tool paths) ----
    static std::string settings_path();
    void load_settings();
    void save_settings();
    // By value: callers pass elements of _recents, which this mutates.
    void add_recent(std::string path);
    void add_model_recent(std::string path);

    // ---- actions ----
    // By value: callers pass elements of _recents, which open_dataset
    // mutates via add_recent (a const& here would dangle).
    // Clears the log panel unless `keep_log`: what is in it belongs to
    // whatever was open before. The reconstruction handoff passes true,
    // because there the log is this dataset's own build log.
    void open_dataset(std::string dir, std::string image_dir = "",
                      std::string mask_dir = "", bool mask_flipped = false,
                      bool keep_log = false);
    // Route for user-initiated opens: confirms first when training.
    void request_open_dataset(std::string dir);

    // The viewer screen: a splat file (or a checkpoint / run directory) opened
    // for looking at. Takes the engine over, so it goes through the same
    // confirmation as any other session-destroying action.
    void open_splat(std::string path);
    void request_open_splat(std::string path);
    // Another model beside the ones already open, in the same view.
    void add_splat(std::string path);
    // Give the engine back and leave the screen. Called before anything that
    // needs the engine for itself.
    void close_splat();

public:
    // Drag-and-drop entry (GLFW drop callback, main thread): auto-detects
    // whether each path is an SfM dataset folder, a folder of photos, or a
    // video file, and routes them like the corresponding Home-screen action.
    // Several videos dropped together become several inputs of one dataset.
    void handle_drop(const std::vector<std::string>& paths);

private:
    void request_go_home();
    void apply_preset(const std::string& preset);
    // A preset the user saved (or a run's config.json). Replaces the whole
    // config the way a built-in preset does, but keeps the GUI-owned context
    // -- which dataset is open, where its runs go.
    void apply_user_preset(const TrainPreset& p);
    // Read `path` and apply it. Reports the outcome in the preset panel and
    // the log rather than throwing; refuses while training, because applying
    // one re-parses the dataset and would take the running session down.
    void load_preset_file(const std::string& path);
    // Re-scan preset_dir(), rate-limited: this runs while a dropdown is open.
    void refresh_presets();
    void start_training();
    // Everything that renders from the training session, released together.
    // Every path that replaces or destroys the session goes through this.
    void detach_session_views();
    // Hand a config to the runner. Everything a new session invalidates --
    // the splat viewer holding the engine, the viewport's render worker --
    // is released here, so both the Start button and the batch queue go
    // through it.
    void launch_training(const TrainConfig& cfg, const std::string& preset);
    bool training_busy() const;   // Preparing or Training

    // ---- batch ----
    // Append a row for this dataset, seeded with the preset the trainer
    // screen is on. Shared by the picker, the recents menu and the drop
    // handler.
    void add_batch_row(const std::string& dataset);
    void check_batch();                     // pre-flight every row
    void request_start_batch(bool skip_invalid);
    void start_batch(bool skip_invalid);
    // Called once per frame while a batch is live: records the row that just
    // finished and launches the next one.
    void advance_batch();
    void finish_batch();
    // Give up on the queue without waiting for the current row (the stop
    // confirmation took the session away).
    void cancel_batch();

    // ---- dataset creation ----
    // Which engines this build and this machine can actually offer.
    bool builtin_sfm_available() const;
    bool colmap_available() const;
    Engine effective_engine() const;
    bool dataset_busy() const;
    // Which step a running job is on, or nullptr when none is. Both runners
    // report through the same object, so the screen reads one thing.
    RunProgress* dataset_steps();
    // Has the running job already read what `s` decides? A section of the form
    // is greyed only once the step that consumes it has started -- the whole
    // form used to grey the moment a run began, which left nothing to do for
    // the twenty minutes a capture takes to extract.
    bool dataset_locked(Stage s);
    void start_dataset_job();
    void cancel_dataset_job();
    // Push edits made while a job runs into the runner, which takes each half
    // as it reaches it (SfmRunner::update).
    void update_dataset_job();
    // Copies the panel-level state into whichever job struct will run.
    void sync_dataset_jobs();
    // Path of the selected checkpoint, or "" when it is not downloaded yet.
    std::string selected_model_path() const;
    // Fetch it (with consent), and whether a run would need it and not find it.
    void request_model_download();
    bool mask_model_missing() const;
    bool license_accepted(const std::string& family) const;

    // ---- screens ----
    void draw_menu_bar();
    void draw_home();
    void draw_home_banner(float avail, float indent);
    void draw_new_dataset();
    void draw_dataset_source();       // input list / output / resume
    void draw_dataset_basics();       // the four or five knobs a beginner needs
    void draw_source_cameras();       // one lens per input, when there are several
    // The line under a lens picker when the input contradicts the model --
    // a panorama model on frames that are not 2:1, an ordinary lens on a
    // dual-fisheye capture. Silent unless it is sure: an input whose size
    // could not be measured is not a complaint.
    void draw_lens_warning(const std::string& path, bool is_video,
                           const std::string& model, bool builtin);
    // First photograph in a folder, or one frame of a video. Probed once per
    // path (an image header, or one `ffmpeg -i`) and remembered.
    bool input_pixel_size(const std::string& path, bool is_video,
                          int& w, int& h);
    void draw_masking_options();
    void draw_geometry_options();
    // Opens the geometry preview on the output folder when it already holds a
    // reconstruction -- the only case where the real cameras are known -- and
    // on the input the mask combo points at otherwise.
    void open_geometry_preview();
    // Fetch the selected checkpoint. No consent gate: the Metric3D exports are
    // CC0 and BSD (src/metric3d/model/Fetch.h), unlike the SAM weights. The
    // largest is two files, so what is pending is a queue.
    void request_geometry_download();
    bool geometry_model_missing() const;
    // The same for the learned frontend, whose detector and matcher are two
    // downloads of their own (src/aliked/model/Fetch.h).
    void request_feature_download();
    bool feature_model_missing() const;
    // The step list and its bars, in place of the one stage line.
    void draw_dataset_steps();
    // The form and the button that acts on it, `height` tall.
    void draw_dataset_form(float height, bool running);
    // The frames / match map / model area, and the poll that feeds it from what
    // the running child writes. `height` of 0 means "as tall as the splitter
    // says", which is what the one-column layout uses.
    void draw_dataset_preview(float height);
    bool preview_has_content() const;
    void poll_sfm_progress();
    // Which of the three the running step implies, or -1 for none.
    int preview_for_stage();
    // Release everything the preview holds -- GL buffers, the watcher thread,
    // the snapshot. Called when the screen is left and at shutdown.
    void reset_dataset_preview();
    // "Re-run masking only" and friends: what probe_workspace already knows,
    // as the actions it implies.
    void draw_dataset_rerun(const WorkspaceState& prior);
    // What the output folder holds, at 1 Hz rather than per frame: the answer
    // now costs a directory scan (a Metashape export is found by extension).
    const WorkspaceState& workspace_state();
    // Opens "Try the mask" on the input the combo points at, which is also the
    // input whose clicks and stencil it edits.
    void open_mask_preview();
    void draw_color_space_options(bool with_point_color);
    void draw_sfm_advanced();
    void draw_feature_download();
    void draw_colmap_options();
    void draw_tool_locations();
    void draw_license_modal();
    void draw_language_menu();       // the picker, and the CJK font prompt
    void draw_train();
    void draw_viewer();
    void draw_mesh();
    void draw_mesh_options();
    // Set the meshing source (and derive the output name from it). Shared by
    // the picker, the drop handler and the "mesh this run" shortcut.
    void set_mesh_source(const std::string& path);
    void start_meshing();
    // Would the run about to start actually get cameras? Resolves the dataset
    // the way the child does -- the folder typed here, else the `data` entry
    // in the run's config.json -- so the screen can warn BEFORE the run that
    // the mesh is about to be the density-only, much rougher kind. Touches
    // the disk, so the answer is cached until the source or folder changes.
    bool mesh_dataset_found();
    // Load whatever the run wrote, plus the model it came from, into the two
    // preview panels. No-op (mesh side only) while training holds the engine.
    void open_mesh_preview();
    void close_mesh_preview();
    void draw_batch();
    void draw_batch_table();
    void draw_batch_preset_combo(BatchJob& job, int row);
    void draw_batch_issues();
    void draw_batch_progress();      // the running-job block on the trainer screen
    void draw_train_settings();      // left panel
    void draw_preset_picker();       // built-in + saved presets, save / load
    void draw_preset_save_modal();
    void draw_preset_delete_modal();
    void draw_basic_options();
    void draw_train_controls();
    void draw_metrics();
    void draw_status_strip();
    // Right-aligned VRAM readout on the status strip; x0/avail describe the
    // strip's content region (window-local left edge and width).
    void draw_vram_readout(float x0, float avail);

    // ---- layout ----
    // The height the log panel gets when `avail` vertical pixels are shared
    // between a screen's body, the splitter and the log. 0 when it is hidden.
    // Never more than 60% of what there is, so a short window still shows the
    // screen; the body is a scrolling child and takes the rest.
    float log_height(float avail) const;
    // The body height to hand BeginChild, given what log_height() returned.
    static float body_height(float log_h);
    // The splitter and then the panel. Call after the body child has ended.
    void draw_log_panel(float height);
    void draw_confirm_modal();
    void draw_data_error_modal();
    void handle_dialog_result(const std::vector<std::string>& paths);
    // Take paths onto the input list, `replace` clearing what was there (a
    // fresh pick from Home) rather than adding to it (the panel's Add buttons).
    // Sets the per-input defaults and, unless the user has edited it, the
    // output folder.
    void add_sources(const std::vector<std::string>& paths, bool replace);
    // Re-derive what is a function of the list: the sub-folder each input's
    // images go into, and the default workspace.
    void refresh_sources();
    void rescan_found_masks();
    // Did any input arrive with masks of its own?
    bool any_found_masks() const;
    // Adopt the EXR colour space when the pictures are EXRs, unless the user
    // has already set one by hand.
    void adopt_exr_color_space();
    void run_pending_if_stopped();
    void append_logs();
    void log(const std::string& s, bool detail = false);
    // The only way to empty the log: _log_shown indexes into _log, so a
    // bare _log.clear() leaves the panel dereferencing stale indices.
    void clear_log();

    Screen _screen = Screen::Home;
    bool _quit = false;
    bool _open_confirm = false;      // arm the stop-training modal
    bool _confirm_shown = false;     // modal currently expected open
    bool _stop_confirmed = false;    // user chose one of the two stops
    bool _data_error_shown = false;  // unreadable-file modal currently open
    // Training is paused for as long as the modal is up -- deciding should not
    // cost GPU time. This is what it goes back to if the user keeps training.
    bool _confirm_was_paused = false;
    Pending _pending = Pending::None;
    std::string _pending_path;       // dataset dir for Pending::OpenDataset
    bool _pending_batch_skip = false;  // Pending::StartBatch's argument
    bool _parse_dirty = false;       // dataparser option edited -> reload
    bool _color_space_touched = false;  // see adopt_exr_color_space
    bool _device_locked = false;     // backend initialized -> device fixed

    // Config being edited + the preset baseline it diffs against.
    TrainConfig _cfg;
    TrainConfig _defaults;
    // The built-in preset the config descends from. Still set when a saved
    // preset is in use -- it is what the run's config.json records, and what
    // the options editor's "preset default" tooltips are relative to.
    std::string _preset = "3dgs";
    ConfigUIState _cfg_ui;

    // Saved presets. `_preset_file` is "" while a built-in preset is selected;
    // otherwise it is the file in use and names the row shown in the picker.
    std::vector<TrainPreset> _presets;
    double _presets_scanned_at = -1.0;
    std::string _preset_file;
    std::string _preset_display;
    std::string _preset_desc;
    // The last save / load outcome, already formatted, and whether it failed.
    std::string _preset_msg;
    bool _preset_msg_err = false;
    // The save dialog.
    bool _preset_save_open = false;    // arm the modal
    bool _preset_save_shown = false;
    // The modal stepped aside for the file dialog and wants to come back.
    bool _preset_save_reopen = false;
    std::string _preset_save_name;
    std::string _preset_save_desc;
    std::string _preset_save_path;
    // The path field follows the name until the user edits it by hand.
    bool _preset_path_edited = false;
    // The delete confirmation, which always targets _preset_file.
    bool _preset_delete_open = false;
    bool _preset_delete_shown = false;

    TrainRunner _runner;
    ViewportPanel _viewport;
    // The trainer screen's other preview: one training photograph beside the
    // render of the same camera. Reads from the session's engine, so it is
    // released together with the viewport (detach_session_views).
    ImageCompare _images;
    bool _preview_images = false;    // which of the two the trainer screen shows
    // Finished models opened for looking at, up to four side by side. The one
    // object, shared by the viewer screen and the meshing preview: it owns the
    // engine while it is open, and the engine is a singleton.
    CompareView _compare;
    // Model files opened here, most recent first, offered by the "add a
    // model" menu. Separate from _recents, which holds datasets.
    std::vector<std::string> _model_recents;

    // ---- meshing ----
    // The extraction runs as a child process (MeshRunner); the preview after
    // it is _compare holding the splats it came from and the surface.
    MeshRunner _mesh;
    MeshJob _mesh_job;
    bool _mesh_preview_open = false; // _compare is showing this screen's result
    // The MeshRunner::run_id() whose result has already been opened. The
    // runner stays Done for the rest of the session, so without this the
    // screen would reopen the preview the frame after it is closed -- which
    // is what made "make another mesh" impossible without a restart.
    uint64_t _mesh_shown_run = 0;
    // mesh_dataset_found()'s cache: the (source, folder) it was asked about
    // and what it answered.
    std::string _mesh_data_probe_key;
    bool _mesh_data_probe_found = false;

    // Dataset creation. Both runners exist; only one runs, chosen by _engine
    // (and forced when only one is available).
    Engine _engine = Engine::BuiltIn;
    ColmapRunner _colmap;
    ColmapJob _colmap_job;
    SfmRunner _sfm;
    SfmJob _sfm_job;
    // ALIKED and LightGlue, when the advanced options ask for them. Fetched
    // here rather than by the run's child process, whose download nobody
    // asked for and nobody can see.
    DownloadQueue _feat_download;
    // ---- what a running job shows about itself ----
    // One reel per step that produces pictures: the frames as they are
    // written, the masks as they are made, and the frames again with the
    // features found on them. Separate because they answer different questions
    // and a run that does all three would otherwise overwrite its own evidence.
    FilmReel _film_frames, _film_masks, _film_features, _film_geometry;
    FeatureWatcher _features;
    MatchMatrix _matrix;
    // The two images behind whichever cell of the match map the cursor is on.
    PairPreview _pairs_view;
    // The model as the mapper builds it, drawn by the same GL preview the
    // trainer screen uses before training starts.
    ViewportPanel _model_view;
    LiveModel _live_model;
    bool _model_attached = false;
    // Snapshot files already read, by their write time; 0 means "not yet".
    int64_t _model_mtime = 0, _pairs_mtime = 0, _matches_mtime = 0;
    double _sfm_polled_at = -1.0;
    bool _show_preview = true;
    // Which view the panel shows: -1 follows the running step, otherwise the
    // one the user picked and wants to keep.
    int _preview_tab = -1;
    // The last step a run was on, so a finished run keeps showing its result.
    int _preview_last_stage = -1;
    // Stages a re-run is to redo rather than reuse, set by draw_dataset_rerun
    // and by the checkbox beside the output folder, consumed by
    // start_dataset_job. Without one, what is there is reused.
    bool _redo_frames = false, _redo_masks = false, _redo_model = false;
    // ... and the same for the maps, which is an overwrite rather than a step
    // to skip. Separate from GeometryJob::overwrite so pressing the button
    // does not leave the option ticked for every run after it.
    bool _redo_geometry = false;
    // Panel-level state, copied into whichever job runs. The inputs are kept as
    // the struct both runners take (PrepInput), so the panel edits the thing
    // that runs instead of a parallel copy of it: a video file or photo folder
    // each, plus the sub-folder and the lens that belong to it.
    std::vector<PrepInput> _sources;
    std::string _workspace;
    // The output folder this screen derived from the inputs. Kept so a folder
    // the user typed is never overwritten when the input list changes.
    std::string _workspace_auto;
    bool _resume = true;
    bool _mask_enable = false;
    // PrepJob::mask_memory. Off by default: a prompt that matches a crowd pays
    // one model pass per object per frame for it. The two below only apply
    // with it on, and are kept here rather than in MaskSettings because the
    // preview segments one still frame and has no bank to tune.
    bool _mask_memory = false;
    int  _mask_detect_every = 1;
    int  _mask_memory_frames = 0;
    // The static stencils are kept on the inputs themselves (PrepInput::
    // stencil); this only says whether the run is given them, so that turning
    // the option off and on again does not throw away what was drawn.
    bool _border_enable = false;
    MaskSettings _mask;
    SegmentPanel _segment;
    // Which input "Try the mask" runs on: which input a new clicked object
    // prompts (MaskClick::source) and which one's stencil the panel edits.
    int _mask_preview_input = 0;

    // ---- depth and normals ----
    // One job for both engines (SfmJob / ColmapJob carry a copy), the panel
    // that tries it on one frame, and the checkpoint fetch.
    GeometryJob _geometry;
    GeometryPanel _geometry_panel;
    DownloadQueue _geom_download;
    // input_pixel_size()'s cache, keyed by input path. A zero pair is a
    // remembered "could not tell", so nothing is probed twice.
    std::map<std::string, std::pair<int, int>> _input_size;

    // The segmentation checkpoint in use. Not persisted -- neither is any
    // other masking or geometry setting, so a fresh session never runs a
    // model the last one happened to pick.
    std::string _model_id = "sam3-q4_0";
    ModelDownload _download;

    // Interface language and the glyphs to draw it with. The font download is
    // separate from _download so that fetching a face cannot cancel a
    // half-finished 700 MB checkpoint.
    FontSet _fonts;
    FileDownload _font_download;
    const CjkFace* _font_fetching = nullptr;
    // Families whose licence the user has accepted, persisted in the settings.
    std::vector<std::string> _accepted_licenses;
    std::string _license_prompt;      // family whose modal is open
    bool _license_tick = false;

    // Batch training. The queue is data; the driver is advance_batch(), so a
    // running batch is an ordinary training session the trainer screen shows
    // exactly as it shows a hand-started one.
    std::vector<BatchJob> _batch;
    bool _batch_dirty = false;        // edited -> persist once the widget is idle
    bool _batch_checked = false;      // a pre-flight has run since the last edit
    bool _batch_active = false;
    bool _batch_launched = false;     // a row's session is in flight
    int  _batch_current = -1;         // which row that is
    bool _batch_stop_after = false;   // finish this row, then stop
    bool _batch_stop_now = false;     // ... and record it as stopped, not done
    std::string _batch_msg;           // already formatted; "" when there is none
    bool _batch_msg_err = false;

    FileDialog _dialog;
    PickAction _pick = PickAction::None;
    int _pick_source = -1;            // which input PickAction::SourceReplace edits
    // Which batch row the pending pick edits; -1 appends a new row.
    int _pick_row = -1;

    // Settings (persisted).
    std::vector<std::string> _recents;
    std::string _colmap_exe = "colmap";
    std::string _ffmpeg_exe = "ffmpeg";
#ifdef _WIN32
    std::string _python_exe = "python";
#else
    std::string _python_exe = "python3";
#endif

    // Log console. `_log_dropped` counts the lines trimmed off the front since
    // the panel was last drawn: every one of them moves the remaining text up
    // by a line, which walks the passage a user has scrolled back to off the
    // top of the panel unless the scroll position is moved with it.
    struct LogEntry {
        std::string text;
        // A line of the stream behind the progress bars, rather than one of the
        // handful that answer "what happened". Hidden unless Details is on.
        bool detail = false;
    };
    std::deque<LogEntry> _log;
    // Which entries the panel is showing, rebuilt when the log or the Details
    // toggle changes rather than every frame.
    std::vector<size_t> _log_shown;
    bool _log_shown_dirty = true;
    bool _log_details = false;
    size_t _log_dropped = 0;
    bool _log_follow = true;
    // Set from the jump button and the context menu, which are both drawn in
    // windows of their own -- ImGui's scroll calls act on the current window,
    // so neither can move the panel directly.
    bool _log_scroll_end = false;
    bool _show_log = true;

    // ---- interface size and panel extents ----
    // The extents are UNSCALED: multiplied by ui_scale() where they are used,
    // so what is saved survives a change of monitor or of size preference.
    UiScale _scale;
    float _panel_w = kDefaultPanelW;
    float _log_h = kDefaultLogH;
    float _preview_h = kDefaultPreviewH;
    // The New Dataset screen's own column width; the trainer's _panel_w is a
    // different screen with a different sensible size.
    float _ds_panel_w = kDefaultDsPanelW;
    bool _show_settings = true;
    bool _layout_dirty = false;      // a splitter moved -> persist once idle
    // The New Dataset screen's run/status band, measured last frame: the form
    // above it is sized against this, and how tall it is depends on what the
    // run is saying.
    float _ds_action_h = 0.0f;
    // ... and what the preview took under it when the window is too narrow for
    // two columns, measured the same way.
    float _ds_preview_h_used = 0.0f;
    // Masks sitting beside the photos are adopted automatically; this is the
    // way out for a folder whose masks/ describes something else.
    bool _use_found_masks = true;
    // Those masks are white where the image is REMOVED, the other convention
    // in the wild. Declared once here; the run normalizes what it writes.
    bool _flip_found_masks = false;

    // workspace_state()'s cache: what it was asked about and when.
    WorkspaceState _ws_state;
    std::string _ws_state_key;
    double _ws_state_at = -1.0;

    // VRAM readout on the status strip, polled from the backend at ~2 Hz.
    backend::MemoryUsage _vram;
    double _vram_polled_at = -1.0;
};

}  // namespace gui

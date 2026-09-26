#pragma once

// GuiApp -- top-level UI: screens, layout, state wiring
// between the config editor, COLMAP runner, train runner, and the native
// viewport.

#include "backend/api/BackendRuntime.h"
#include "config/TrainConfig.h"
#include "app/gui/BatchProcess.h"
#include "app/gui/ColmapRunner.h"
#include "app/gui/CommandRunner.h"
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
#include "app/gui/mask/MaskSession.h"
#include "app/gui/SfmRunner.h"
#include "app/gui/SourceList.h"
#include "app/gui/SourceProbe.h"
#include "app/gui/TelemetryProbe.h"
#include "app/gui/TrainPreset.h"
#include "app/gui/TrainRunner.h"
#include "app/gui/ViewportPanel.h"

#include <cstdint>
#include <deque>
#include <fstream>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace gui {

// One kind of saved preset as the screen sees it: what is on disk, which one
// is in use, and what the last save or load said. `file` is "" while the
// built-in (or stock) settings are selected.
template <class T>
struct PresetPicker {
    std::vector<T> items;
    double scanned_at = -1.0;
    std::string file;
    // Which built-in is selected while `file` is empty, for the kinds that
    // have them. "" means the stock settings.
    std::string builtin;
    std::string display;
    std::string desc;
    std::string msg;          // already formatted
    bool msg_err = false;
};

class GuiApp {
public:
    static constexpr float kDefaultPanelW = 420.0f;
    static constexpr float kDefaultLogH = 150.0f;
    static constexpr float kDefaultPreviewH = 260.0f;
    static constexpr float kDefaultDsPanelW = 560.0f;
    static constexpr float kEditPanelW = 300.0f;

    GuiApp();
    ~GuiApp();

    // Draw one frame (between ImGui::NewFrame and ImGui::Render).
    void frame();
    // Something is moving without the user touching anything -- a render
    // playing back or being written -- so frames must keep coming.
    bool animating() const { return _compare.animating() || _mask_editor.animating(); }

    // What a script needs to know that is not on screen as a widget: the
    // screen, what is running, what is open. A JSON object body without the
    // braces, for gui::automation::set_state_source.
    std::string state_json();

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
        None, OpenDataset, SourceImages, SourceVideo, SourceDataset,
        SourceReplace, Workspace,
        OutputPrefix, VocabTree, MaskModelFile, SplatFile,
        PresetFile, DatasetPresetFile, MeshPresetFile, PresetSaveFolder,
        BatchDataset, BatchOutput, BatchPresetFile, BatchDatasetPresetFile,
        BatchMeshPresetFile, BatchSourceImages, BatchSourceVideo, BatchModel,
        MeshSource, MeshPhotos, MeshOutput, AddSplatFile, SplatFolder,
        EditSaveFile, EditSaveFolder, RenderProjectSave, RenderProjectOpen,
        RenderOutput, RenderAddModel, StencilFile
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
    // Which remembered directory a pick starts from. Several actions share one
    // key -- a dataset is a dataset wherever it is picked -- and the mode is
    // what separates the two things SourceReplace picks.
    static const char* dir_key(PickAction a, FileDialog::Mode m);
    // Where the next pick of `key` starts: the PARENT of `path`, since the
    // next scene or model of a kind is usually its sibling, not inside it.
    void remember_dir(const std::string& key, const std::string& path);
    // Arm the dialog for `a`. It opens in `start_dir`, or where a pick of the
    // same kind last landed when that is empty.
    void open_pick(PickAction a, const std::string& title,
                   FileDialog::Mode mode,
                   const std::vector<std::string>& extensions = {},
                   const std::string& start_dir = "", bool multi = false,
                   const std::string& suggested_name = {});

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

    // The comparison panes, with the editing panel beside them when a pane is
    // being edited. The viewer screen and the meshing preview share it.
    void draw_compare_panes();
    void open_render_output_pick(const std::string& start, const std::string& suggested);

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
    // Close GPU-backed previews before another native handoff.
    void close_native_previews();
    // close_native_previews(), and the mask editor releases its SAM session.
    void stop_inference_users();

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
    // Re-scan each kind's preset folder, rate-limited: these run while a
    // dropdown is open.
    void refresh_presets();
    void refresh_dataset_presets();
    void refresh_mesh_presets();

    // ---- dataset and meshing presets ----
    // The New Dataset screen's settings as a preset carries them, and back.
    // Applying one never touches the inputs or the output folder.
    DatasetSettings capture_dataset_settings() const;
    void apply_dataset_settings(const DatasetSettings& s);
    void apply_dataset_preset(const DatasetPreset& p);
    // A built-in, which is the capture's own answers with the preset's over
    // them -- and then the one question only the frames can settle.
    void apply_dataset_builtin(const std::string& name);
    // ... and again once the inputs change, so the order they were picked in
    // does not decide which of the two wins.
    void reapply_dataset_builtin();
    void load_dataset_preset_file(const std::string& path);
    // Meshing, the same way: the model, its photographs and the output path
    // are what the preset is applied TO.
    void apply_mesh_preset(const MeshPreset& p);
    void load_mesh_preset_file(const std::string& path);
    // The picker a screen draws above its options: combo, save, load, delete.
    void draw_dataset_preset_picker();
    void draw_mesh_preset_picker();
    // Arm the shared save dialog for `kind`, seeded from what is on screen.
    void open_preset_save(PresetKind kind);
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
    // Append a row that trains this dataset, seeded with the preset the
    // trainer screen is on. Shared by the picker, recents and the drop handler.
    void add_batch_row(const std::string& dataset);
    // ... and one that builds a dataset from these inputs, or meshes a model.
    void add_batch_source_row(const std::vector<std::string>& sources);
    void add_batch_mesh_row(const std::string& model);
    // The training run a new row (or a new run on one) starts with: whatever
    // the trainer screen is on.
    BatchRun batch_run_on_screen() const;
    void batch_edited();                    // persist, and re-check
    // Pre-flight every row. Cheap enough (a few stats and a small JSON per
    // preset) that nothing waits for a button: check_batch_if_stale() runs it
    // whenever an edit or a couple of seconds have gone by.
    void check_batch();
    void check_batch_if_stale();
    BatchCapabilities batch_capabilities() const;
    void request_start_batch(bool skip_invalid);
    void start_batch(bool skip_invalid);
    // Called once per frame while a batch is live: records the task that just
    // finished and launches the next one.
    void advance_batch();
    // Is the runner that owns this stage still working?
    bool batch_stage_busy(BatchStage stage) const;
    // Record the finished task, and skip what depended on it.
    void record_batch_task();
    bool launch_batch_task(BatchTask& task);
    bool launch_batch_dataset(BatchTask& task, const BatchRow& row);
    bool launch_batch_train(BatchTask& task, const BatchRow& row);
    bool launch_batch_mesh(BatchTask& task, const BatchRow& row);
    // The dataset / model a task reads, from what its row has produced so far.
    const BatchTask* batch_task_of(int row, BatchStage stage, int variant) const;
    void fail_batch_task(BatchTask& task, const std::string& error);
    void finish_batch();
    // Give up on the queue without waiting for the current task (the stop
    // confirmation took the session away).
    void cancel_batch();
    // Show the screen the running stage belongs to, unless the user has
    // walked off to look at something else.
    void follow_batch_screen(BatchStage stage);

    // ---- dataset creation ----
    // Which engines this build and this machine can actually offer.
    bool builtin_sfm_available() const;
    bool colmap_available() const;
    Engine effective_engine() const;
    void draw_pano360_options();
    void draw_pano360_size();
    bool dataset_busy() const;
    bool native_work_busy() const;
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
    void request_model_download(const std::string& id);
    bool mask_model_missing() const;
    bool license_accepted(const std::string& family) const;

    // ---- screens ----
    void draw_menu_bar();
    void draw_home();
    void draw_home_banner(float avail, float indent);
    void draw_new_dataset();
    void draw_dataset_source();       // input list / output / resume
    void draw_sensor_badge(const PrepInput& s);
    void draw_dataset_basics();       // the four or five knobs a beginner needs
    void draw_source_cameras();       // one lens per input, when there are several
    void draw_rig_kinds(const std::vector<CameraGroup>& groups,
                        const std::vector<std::string>& names);
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
    void draw_scan_view(float h);
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
    void reset_dataset_preview(bool sweep = true);
    // "Re-run masking only" and friends: what probe_workspace already knows,
    // as the actions it implies.
    void draw_dataset_rerun(const WorkspaceState& prior);
    void open_mask_editor(const std::string& workspace, const std::string& image_dir,
                          const std::string& mask_dir, bool mask_flipped);
    // Throwing the whole project away rather than one step of it: the run's
    // own files, and the options, each on its own button.
    void draw_dataset_reset();
    void draw_clear_project_modal();
    // Turning "keep intermediate files" OFF is the one option here that
    // destroys work: it is what makes a cancelled run resumable.
    void draw_drop_intermediate_modal();
    // Masks that the kept reconstruction was not built with: the one question
    // "Update dataset" cannot answer by itself (see masks_miss_kept_model).
    void draw_mask_recon_modal();
    // Is this run about to write masks the reconstruction it is keeping has
    // never seen, with the panel asking for masked feature points? Then
    // pressing the button means one of two runs, and it has to be asked which.
    bool masks_miss_kept_model();
    // Everything start_dataset_job does once that question is settled. False
    // when the run did not start (busy, or the device could not be frozen).
    bool launch_dataset_job();
    // An existing dataset as an input: its images/ become the source and the
    // folder itself the output, so the run adds to it instead of building a
    // copy beside it.
    void add_existing_dataset(const std::string& dir);
    // Every option back to what a freshly picked input would have given it.
    // The inputs, the output folder and the mask prompt are not options.
    void reset_recon_options();
    // What one input's images are fitted with, resolved down the row list.
    void source_lens(size_t input, std::string& model, float& focal) const;
    // What the output folder holds, at 1 Hz rather than per frame: the answer
    // now costs a directory scan (a Metashape export is found by extension).
    const WorkspaceState& workspace_state();
    // How a panel must read one input's frames: the decoder the run will use,
    // and the FrameLook it will write them with.
    PreviewSource preview_source(size_t input) const;
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
    void draw_batch_rows();
    void draw_batch_row(BatchRow& row, int index, int& remove, int& move);
    void draw_batch_row_dataset(BatchRow& row, int index);
    void draw_batch_row_train(BatchRow& row, int index);
    void draw_batch_row_mesh(BatchRow& row, int index);
    // The dataset folder: written by the Dataset stage, read by the Train
    // stage, so one field serves both and they cannot disagree.
    void draw_batch_row_common_dataset(BatchRow& row, int index);
    // The one-line summary on a collapsed row: what it does, to what.
    std::string batch_row_summary(const BatchRow& row) const;
    // The built-in / saved picker one stage of one row uses. True when the
    // choice moved.
    bool draw_batch_train_preset(BatchPreset& p, const char* id, int row, int slot);
    // One training run: its preset, its three overrides and whether the row's
    // Mesh stage covers it, on one line.
    bool draw_batch_run(BatchRun& run, int row, int slot, bool meshing);
    // The colour and format sets a row writes, over what its preset says.
    bool draw_batch_mesh_outputs(BatchMeshOptions& mesh);
    void draw_batch_issues();
    void draw_batch_plan();          // the tasks a start would run, in order
    // What is running, as bars: the queue, the task, and how long each has
    // left. Drawn on the batch screen and on whichever work screen the
    // running task belongs to, so neither has to be guessed from the other.
    void draw_batch_progress();
    void draw_batch_stop_buttons();
    // How far through the running task its own runner says it is, -1 when it
    // cannot say, and how long it has been going.
    float batch_task_fraction();
    double batch_task_elapsed() const;
    // The row the running task belongs to, null when nothing is running.
    // `_batch_current` indexes the TASKS, which outnumber the rows.
    const BatchRow* batch_running_row() const;
    // The command the queue runs when it is over: the field and its Test
    // button, starting it with a message, and reporting how it went.
    void draw_batch_command();
    void run_batch_command(const std::string& message);
    void poll_batch_command();
    const spirula::i18n::Msg& batch_stage_name(BatchStage s) const;
    void draw_train_settings();      // left panel
    // Native picker and frozen identity, available from shared settings/View menu.
    void draw_device_picker(bool as_menu = false);
    // Lists the native devices once per session. Enumeration is side-effect
    // free (a throwaway Vulkan instance) and never creates a logical device.
    void load_native_devices();
    // Resolve and register the canonical UUID without creating the inference
    // context; reject later conflicting requests as restart-required.
    bool freeze_native_device();
    bool freeze_cuda_device();
    // Copies the frozen UUID into every native job field this session owns, so
    // one choice reaches the dataset, mask/geometry previews, reconstruction,
    // geometry child and mesh child. Called from the freeze point.
    void propagate_frozen_device();
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
    // Take paths onto the input list; `replace` clears a fresh pick's inputs.
    // Sets defaults and the output folder; false means no input was accepted.
    bool add_sources(const std::vector<std::string>& paths, bool replace);
    void replace_source(size_t input, const std::string& path);
    // Re-derive what is a function of the list: the sub-folder each input's
    // images go into, and the default workspace.
    void refresh_sources();
    void pump_source_probes();
    void mark_source_metadata_dirty();
    void rescan_found_masks();
    // Did any input arrive with masks of its own?
    bool any_found_masks() const;
    // Adopt the EXR colour space when the pictures are EXRs, unless the user
    // has already set one by hand.
    void adopt_exr_color_space();
    void run_pending_if_stopped();
    void append_logs();
    void log(const std::string& s, bool detail = false);
    // The full panel snapshot, written at the top of each run's log file
    // before the separator that the timestamped log lines follow.
    void write_run_settings(std::ofstream& f);
    // The only way to empty the log: _log_shown indexes into _log, so a
    // bare _log.clear() leaves the panel dereferencing stale indices.
    void clear_log();

    Screen _screen = Screen::Home;
    // Which save target the editor's file picker was armed for, and whether
    // the next model opened is being opened in order to edit it.
    int _edit_save_target = 0;
    bool _edit_after_open = false;
    // The dataset screen's run behind a sparse edit it opened: its photos and
    // masks may live outside the dataset, and training the edit needs them.
    struct DatasetFolders {
        std::string dir, image_dir, mask_dir;
        bool mask_flipped = false;
    };
    DatasetFolders _sparse_edit_src;
    // A dataset already in the output folder, read the way a run would read it.
    DatasetFolders workspace_folders(const WorkspaceState& prior) const;
    // Open in trainer / edit reconstruction / correct masks, for one dataset.
    void draw_dataset_open_buttons(const DatasetFolders& f, bool model);
    // A saved sparse edit on its way to the trainer: the dataset to open and
    // the one it was edited from. Taken up at the top of the next frame,
    // outside the edit session that asked for it.
    std::string _edit_train_dataset, _edit_train_source;
    void open_edited_dataset();
    // The same, for a model opened in order to render it.
    bool _render_after_open = false;
    std::string _render_project_after_open;
    bool open_render_project(const std::string& path);
    // Quitting with unsaved edits: the question is asked before the training
    // one, because the answer decides whether anything is written at all.
    bool _edit_exit_confirm = false;
    void draw_edit_exit_modal();
    // The same for the render's camera move; "discard" lets it go for good.
    bool _render_exit_confirm = false, _render_discarded = false;
    bool _quit_after_render_save = false;
    void draw_render_exit_modal();
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

    // ---- the one frozen native GPU choice ----
    // Typed request, including explicit Auto; frozen flag makes it immutable.
    std::string _native_device_request;
    bool _native_device_choice_set = false;
    std::string _native_device_uuid;   // canonical uuid:<hex>, "" before freeze
    std::string _native_device_name;   // driver name of the frozen device
    std::string _native_device_error;  // last rejected request, shown inline
    bool _native_device_frozen = false;

    // The session-level CUDA engine picker is an ordinal the native side never
    // sees. Enforced here because CUDA's device_select cannot reject a later
    // change.
    bool _cuda_device_locked = false;
    int _cuda_device_index = -1;

    // What the picker lists: the native Vulkan records when the build has one,
    // backend rows otherwise. Cached: enumeration spins up a throwaway
    // instance, and the machine's device set does not change under a session.
    struct NativeDeviceRow {
        std::string name, type, uuid;   // uuid canonical, "" when unreported
        uint64_t vram_bytes = 0;
        bool usable = false;
    };
    std::vector<NativeDeviceRow> _native_devices;
    bool _native_devices_loaded = false;

    // Config being edited + the preset baseline it diffs against.
    TrainConfig _cfg;
    TrainConfig _defaults;
    // The built-in preset the config descends from. Still set when a saved
    // preset is in use -- it is what the run's config.json records, and what
    // the options editor's "preset default" tooltips are relative to.
    std::string _preset = "3dgs";
    ConfigUIState _cfg_ui;
    // Persisted: the last value the user gave save_full_checkpoint by hand. A
    // preset only overrides it by turning it on.
    bool _keep_full_ckpt = false;

    // Saved presets, one picker per kind.
    PresetPicker<TrainPreset> _train_presets;
    PresetPicker<DatasetPreset> _ds_presets;
    PresetPicker<MeshPreset> _mesh_presets;
    // The save dialog, which the three kinds share -- it asks the same three
    // questions whatever is being saved.
    PresetKind _preset_save_kind = PresetKind::Train;
    bool _preset_save_open = false;    // arm the modal
    bool _preset_save_shown = false;
    // The modal stepped aside for the file dialog and wants to come back.
    bool _preset_save_reopen = false;
    std::string _preset_save_name;
    std::string _preset_save_desc;
    std::string _preset_save_path;
    // The path field follows the name until the user edits it by hand.
    bool _preset_path_edited = false;
    // The delete confirmation, which always targets the picker's own file.
    PresetKind _preset_delete_kind = PresetKind::Train;
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
    // The photo folders rigs were last guessed for; guessed again only when
    // they change, so clearing the guessed letters sticks.
    std::string _rig_guess_key;
    // Keep the committed source stable while a path is edited.
    std::vector<std::string> _source_path_edits;
    // What an input row draws after its path box, as last measured: the
    // buttons, then the label and sensor badge. 0 before any row was drawn.
    float _source_controls_w = 0.0f, _source_info_w = 0.0f;
    // Video headers are read off the UI thread; the results are applied in
    // pump_source_probes().
    SourceProbe _source_probe;
    bool _source_probes_ready = true;
    // The rate box's text per row, kept while it is being typed into: what is
    // in the model is a number or nothing, and the box shows a caret for the
    // nothing (draw_dataset_source).
    std::vector<std::string> _fps_text;
    int _fps_editing = -1;
    // What each input's IMU / GPS holds, read on its own thread and keyed by
    // path, so re-choosing a file already read costs nothing.
    TelemetryProbe _telemetry;
    std::string _workspace;
    // The output folder this screen derived from the inputs. Kept so a folder
    // the user typed is never overwritten when the input list changes.
    std::string _workspace_auto;
    // The output folder this panel has reconstructed into, so its stamp
    // describes the settings still on the screen (SfmJob::settings_built_model).
    // Cleared when the input list is replaced, which resets settings of its own.
    std::string _built_workspace;
    bool _resume = true;
    bool _mask_enable = false;
    // Hide what the masks cover from feature detection too, not only from
    // training (SfmJob::mask_features).
    bool _mask_features = true;
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
    // A saved stencil drawn on every input, by name (StencilPreset.h); cleared
    // once the panel edits what it drew, since the name no longer says what is.
    std::string _frame_shapes;
    std::vector<StencilPreset> _frame_shapes_list;   // read when the picker opens
    void apply_frame_shapes(size_t first_input = 0);
    void save_run_stencils();
    MaskSettings _mask;
    SegmentPanel _segment;
    // The mask correction editor (app/gui/mask/). Opened from the dataset
    // screen and the train screen; drawn from frame() so both can reach it.
    mask::MaskSession _mask_editor;
    // draw_train()'s mask-folder probe: what it last checked, and when.
    std::string _train_masks_key;
    double _train_masks_at = -1.0;
    bool _train_has_masks = false;
    std::string _train_color;   // the dataset's colour profile; empty without a record
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

    // The dataset run's checkpoint and the mask editor's (clicks, so the fast
    // one). Not persisted, like every masking setting: a fresh session never
    // runs a model the last one happened to pick.
    std::string _model_id = "sam3-q4_0";
    std::string _mask_editor_model_id = "sam2.1-base-plus";
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
    std::string _license_model_id;    // the checkpoint it downloads
    bool _license_tick = false;

    // Batch processing. The queue is data; the driver is advance_batch(), so a
    // running task is an ordinary dataset / training / meshing job, shown by
    // the same screen that shows a hand-started one.
    std::vector<BatchRow> _batch;
    // The rows expanded into the units of work they will run as, built by
    // start_batch() and by the plan preview. Only the running copy carries
    // results, so the preview may be rebuilt whenever the list changes.
    std::vector<BatchTask> _batch_tasks;
    bool _batch_dirty = false;        // edited -> persist once the widget is idle
    bool _batch_checked = false;      // a pre-flight has run since the last edit
    // When it ran. The list is re-checked on a timer as well as on an edit:
    // a preset file or a dataset folder can go missing while the screen is up.
    double _batch_checked_at = -1.0;
    bool _batch_active = false;
    bool _batch_launched = false;     // a task is in flight
    int  _batch_current = -1;         // which task that is
    bool _batch_stop_after = false;   // finish this task, then stop
    bool _batch_stop_now = false;     // ... and record it as stopped, not done
    // Which row is open in the list, so an unattended queue is not a wall of
    // expanded forms. -1 = none.
    int  _batch_open_row = -1;
    bool _batch_show_plan = false;
    std::string _batch_msg;           // already formatted; "" when there is none
    bool _batch_msg_err = false;
    // What to run once the queue is over -- a notification, usually. Empty is
    // "run nothing", and it is kept in gui.conf rather than in the list: it
    // belongs to this machine, not to the rows.
    std::string _batch_cmd;
    CommandRunner _batch_cmd_run;

    FileDialog _dialog;
    PickAction _pick = PickAction::None;
    std::string _pick_key;            // dir_key() of the pick in flight
    int _pick_source = -1;            // which input PickAction::SourceReplace edits
    // Which batch row the pending pick edits; -1 appends a new row.
    int _pick_row = -1;
    // ... and which of that row's training runs, for a pick made in one of
    // their preset combos.
    int _pick_slot = -1;

    // Settings (persisted).
    std::vector<std::string> _recents;
    // Where a pick of each kind last landed, so a session opens where the last
    // one left off rather than at the home directory.
    std::map<std::string, std::string> _dialog_dirs;
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
    // One file per run, opened at its start and named then:
    // <dataset>/logs/train_...log and <dataset>/logs/prep_...log.
    std::ofstream _train_log, _prep_log;

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
    // The editing panel's own width, dragged like the other two.
    float _edit_panel_w = kEditPanelW;
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
    // What a folder of photos does on its way into the dataset. The default
    // gives the dataset an images/ of its own, which is what makes it open
    // again without image_dir being named by hand.
    PhotoImport _photo_import = PhotoImport::ConvertJpeg;

    // "Clear this project's data": what the modal is about to delete, listed
    // when it opens so the user reads the same paths that go.
    bool _clear_open = false, _clear_shown = false;
    std::vector<std::string> _clear_targets;
    bool _drop_intermediate_open = false, _drop_intermediate_shown = false;
    bool _mask_recon_open = false, _mask_recon_shown = false;

    // workspace_state()'s cache: what it was asked about and when.
    WorkspaceState _ws_state;
    std::string _ws_state_key;
    std::vector<std::string> _ws_artifacts;
    double _ws_state_at = -1.0;

    // VRAM readout on the status strip, polled from the backend at ~2 Hz.
    backend::MemoryUsage _vram;
    double _vram_polled_at = -1.0;
};

}  // namespace gui

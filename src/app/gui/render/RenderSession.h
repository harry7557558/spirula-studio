#pragma once

// Render mode: what a viewer pane embeds to make a photo or a video of what
// it shows. The camera move is edited on the pane itself -- keyframes drawn
// as cameras, moved with the editor's G / R / S grammar -- and on the
// timeline under it; the frame it will produce is previewed beside it.
// docs/notes/render-video.md.
//
// It reaches the pane through ViewportInteractor, as the editor does, and the
// models through the SourceViews its owner hands it every frame.

#include "app/gui/ViewportInput.h"
#include "app/gui/edit/TransformTool.h"
#include "app/gui/render/FlightFit.h"
#include "app/gui/render/FrameRenderer.h"
#include "app/gui/render/FrameSink.h"
#include "app/gui/render/LensPresets.h"
#include "app/gui/render/RenderProject.h"
#include "app/gui/render/Trajectory.h"
#include "data/FrustumTemplate.h"
#include "i18n/Message.h"

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace gui {

class ViewportPanel;

namespace render {

// What the owner knows about one model beside the SourceView itself.
struct SourceInfo {
    SourceView view;
    std::string name;                   // for lists: a file name, never translated
    std::string path;                   // what the project records
    // Splats and meshes: the dataset the model came from, when known, for
    // its cameras and lenses.
    const ParsedDataset* dataset = nullptr;
    // Which way is up in the model's file frame, when a dataset says.
    bool has_up = false;
    double up[3] = {0, 0, 1};
    // Changes each time the file is read again, and which pane shows it and
    // its uid, which lasts as long as the pane.
    uint64_t load_id = 0;
    int pane = -1;
    uint64_t uid = 0;
};

class RenderSession : public ViewportInteractor {
public:
    RenderSession();
    ~RenderSession() override;

    // Take over `panel`, the primary model's pane. With no project yet, one
    // starts from what the pane is looking at.
    void open(ViewportPanel* panel);
    // Stop drawing on the pane; the project and its history stay. A switch
    // to the editor autosaves without saying so.
    void close(bool switching = false);
    bool active() const { return _panel != nullptr; }

    // Supplied by the owner once a frame, before poll(). [0] is the primary.
    void set_sources(std::vector<SourceInfo> sources);
    // The primary model's file coordinates into the pane's shared frame:
    // base placement after the model's own normalization.
    void set_world_to_shared(const spirula::Sim3& w2s) { _w2s = w2s; }

    // Where file pickers come from: the owner has the dialog. `kind` is
    // what the answer is for (Pick).
    enum class Pick { SaveProject = 0, OpenProject, Output, AddModel };
    void set_pick(std::function<void(Pick, const std::string& start,
                                     const std::string& suggested)> f) {
        _pick = std::move(f);
    }
    void picked(Pick kind, const std::string& path);
    // Asked of the owner: go to the editor with this pane.
    void set_switch_to_edit(std::function<void()> f) { _to_edit = std::move(f); }
    void set_leave(std::function<void()> f) { _on_leave = std::move(f); }
    // Asked of the owner: close this pane, a model no longer wanted; open
    // this file as another model; show (and edit) this pane's model.
    void set_remove_model(std::function<void(int pane)> f) { _remove_model = std::move(f); }
    void set_add_model(std::function<void(const std::string&)> f) { _add_model = std::move(f); }
    void set_view_model(std::function<void(int pane, bool edit)> f) { _view_model = std::move(f); }
    // The editor wrote `placement` (file coordinates) into `saved`: when the
    // primary model is read back from there, the keys are already where the
    // model went and must not be moved again.
    void note_saved(const std::string& saved, const spirula::Sim3& placement);
    void set_ffmpeg(const std::string& exe) { _ffmpeg = exe; }

    const Output& output() const { return _project.output; }
    std::vector<std::string> drain_log();

    // Once a frame: sources, preview, the export in flight, the history.
    void poll();
    void draw_panel();
    // The strip under the panes: transport, ruler, keys, shots.
    void draw_timeline();
    float timeline_height() const;
    // The camera's own view, in the pane's space or on its own.
    enum class PreviewMode { Corner = 0, Beside, Through };
    PreviewMode preview_mode() const { return _preview_mode; }
    // Side by side: the camera's share of the width, which a splitter drags.
    float& beside_share() { return _beside_share; }
    void draw_preview_pane();
    void draw_status();

    // The project on disk has changes it does not.
    bool dirty() const;
    // For the quit dialog: the project's own file, if it has one; saving
    // over it (false with none); and asking where to save it.
    const std::string& project_file() const { return _project_path; }
    // A project file, opened as the panel's Open does.
    void open_project(const std::string& path) { open_from(path); }
    bool save_in_place();
    void ask_save_as();
    // What a save or a render is offered as: the project's name, or the
    // model's, with the extension it needs.
    std::string suggested_project_name() const;
    std::string suggested_output_name() const;
    // Rendering: the owner keeps the pane's model where it is meanwhile.
    bool exporting() const { return _job.state != Job::Idle; }
    // Playing back or exporting: the window keeps drawing.
    bool animating() const;

    bool owns_left_button() const override;
    bool blocks_fly_keys() const override;
    bool owns_right_button() const override { return _xform.active(); }
    bool on_viewport_input(const ViewportInput& in) override;
    void draw_viewport_overlay(const ViewportOverlay& v) override;
    bool frame_bounds(double centre[3], double& radius) override;

private:
    // ---- the project and its history ----
    void new_project();
    void commit_history();
    void reset_history();
    void goto_step(int i);
    void undo();
    void redo();
    const spirula::i18n::Msg* change_label(const std::string& before,
                                           const std::string& after) const;
    void project_changed();
    const Trajectory& trajectory();
    void save_to(const std::string& path);
    std::string base_name() const;
    spirula::Sim3 primary_placement() const;
    // Up in the keys' frame; false when no dataset or editor turn says, and
    // `out` is then the pane's up.
    bool scene_up(double out[3]) const;
    void open_from(const std::string& path);

    // ---- keyframes ----
    // Between two keys the new one is where the camera already passes, so
    // the move does not change; past the last it is the view.
    int add_key(double time, bool select);
    int add_key_from_view(double time, bool select);
    // The lens the viewport shows through, for a first key.
    Lens view_lens() const;
    // Where to put the next key when none is asked for: after the one
    // selected, or at the playhead.
    double next_key_time() const;
    void click_select(int index, bool ctrl, bool shift);
    void set_playing(bool on);
    // When the camera passes key `i`, which constant speed moves.
    double key_visit(int i);
    // Smoothing over the selected keys, or all of them: `what` may move, 0
    // their poses, 1 their times, 2 both.
    void smooth_keys(double strength, int what);
    void smooth_pass(double strength);
    // The primary model moved in the editor: the keys go with it.
    void follow_placement();
    void follow_scene_up();
    // `index` is the project's; see _rt.
    void remove_source(int index);
    void view_source(int index, bool edit);
    void reconcile_sources();
    int rt(int i) const { return i >= 0 && i < (int)_rt.size() ? _rt[(size_t)i] : -1; }
    int project_source_of(int runtime) const;
    std::string source_name(int i) const;
    void draw_history_section(float full);
    void update_key_from_view(int index);
    void aim_ahead(Keyframe& k);
    void look_through(double time);
    // `fit`: the keys left move to keep the path as it was.
    void delete_selected(bool fit = false);
    void select_only(int index);
    bool selected(int index) const;
    int single_selected() const;
    void space_evenly(double total);
    void make_orbit();
    void follow_capture();
    // Flying a path by hand: the view is recorded while the user flies it,
    // and kept -- fitted into keys -- or thrown away.
    void start_flight();
    void stop_flight(bool keep);
    void refit_flight();
    std::string keys_json() const;
    void draw_flight_controls(float full);
    // The pane's navigation camera as a keyframe pose, world frame.
    bool view_pose(double pos[3], double rot[4], double target[3]) const;

    // ---- the viewport ----
    bool view_projection(ViewProjection& out, int W, int H) const;
    void to_shared(const double w[3], double s[3]) const;
    bool key_screen(int index, const ViewProjection& vp, float& x, float& y) const;
    int hit_key(float x, float y) const;
    // The operator's frame: pivot (the selection's middle), axes, units.
    bool xform_frame(XformFrame& f, XformKind kind);
    void begin_xform(XformKind kind, float mx, float my, bool drag, int axis = -1,
                     bool plane = false);
    void apply_xform(const spirula::Sim3& step_shared, bool scale_fov, double factor);
    // `over_list`: the pointer is on the panel or the timeline, where the
    // letters that select and delete still mean keys.
    void handle_keys(bool over_view, bool over_list);
    // A camera as the viewport draws it, `scale` times the common size.
    // `hit` collects the lines drawn, pane coordinates, four floats each.
    void draw_camera(ImDrawList* dl, const ViewProjection& vp, float ox,
                     float oy, const CameraState& c, unsigned col, float scale,
                     bool axes, std::vector<float>* hit = nullptr) const;
    const camhost::FrustumShape& frustum_shape(const Lens& l) const;
    // World units every camera is drawn at, times the panel's slider.
    double camera_size() const;

    // ---- frames ----
    FrameSpec frame_spec(double t, int W, int H, bool photo);
    bool fx_frame(Transition kind, bool camera, int source, const FrameSpec& f, LayerFx& x);
    void side_effect(FrameSpec& f, LayerSpec& l, Transition kind, double u, bool in,
                     const float param[2], const float colour[3], bool camera, bool edge);
    void preview_size(float box_w, float box_h, int& W, int& H) const;
    void request_preview();

    // ---- the export ----
    struct Job {
        enum State { Idle, Preparing, Rendering, Finishing } state = Idle;
        bool photo = false;
        bool builtin = false;            // the GPU encoder, which may hand over to ffmpeg
        int frame = 0, frames = 0;
        bool queued = false;             // `frame` already asked of the renderers
        bool unread = false;             // a finished frame waits for room in the sink
        std::unique_ptr<FrameSink> sink;
        std::thread finisher;
        std::atomic<bool> finished{false};
        bool ok = true;
        std::string path, error;
        double started = 0.0;
        void reset() {
            state = Idle;
            photo = false;
            builtin = false;
            frame = frames = 0;
            queued = false;
            unread = false;
            sink.reset();
            finished = false;
            ok = true;
            path.clear();
            error.clear();
            started = 0.0;
        }
    };
    // `confirmed`: replacing what is at the output path was agreed to.
    void start_export(bool no_builtin = false, bool confirmed = false);
    void draw_overwrite_popup();
    void cancel_export();
    void poll_export();
    Encoder pick_encoder();
    void probe_encoder();
    // Frames the output has: one a photo, a loop's last left out.
    int frame_count() const;

    // ---- panel pieces (RenderPanel.cpp) ----
    void draw_output_section(float full);
    void draw_keys_section(float full);
    void draw_lens_section(float full);
    void draw_motion_section(float full);
    void draw_effects_section(float full);
    void draw_transition_settings(Transition kind, float param[2], float colour[3],
                                  bool& camera, float full);
    void draw_project_section(float full);
    // `done`: a job finished, which the status line shows in green.
    void note(const std::string& s, bool done = false);

    ViewportPanel* _panel = nullptr;
    RenderProject _project;
    bool _have_project = false;
    // Up came from a dataset or from the user, not from the frame's +Z.
    bool _up_known = false;
    // From the view: an editor turn carries it rather than replacing it.
    bool _up_taken = false;
    // The primary model's read the keys follow, and a save that moved it.
    uint64_t _tracked_load = 0;
    std::string _baked_path;
    spirula::Sim3 _baked;
    std::string _sources_sig;
    void take_up_from_view();
    std::vector<SourceInfo> _sources;
    spirula::Sim3 _w2s;
    std::string _project_path;
    std::string _saved_json;             // what _project_path holds
    // Every state the project has been in, `_head` the one it is in: undo
    // and redo walk it, the history list jumps along it.
    struct Step {
        std::string json;
        const spirula::i18n::Msg* label = nullptr;
        std::vector<uint64_t> uids;
    };
    std::vector<Step> _hist;
    int _head = 0;
    bool _hist_scroll = false;           // the list follows a new step
    // The project's sources against the open ones (_sources, the owner's
    // order, [0] shown): the pane uid each was last matched to, and its
    // index in _sources, -1 while it is not open.
    std::vector<uint64_t> _src_uid;
    std::vector<int> _rt;
    // 1: open what the project lists and is not open; 2: close the rest too.
    int _sync_models = 0;
    std::vector<std::string> _asked_open;
    std::vector<uint64_t> _asked_close;
    uint64_t _revision = 1;
    std::unique_ptr<Trajectory> _traj;
    uint64_t _traj_rev = 0;

    std::vector<uint8_t> _sel;           // one flag per key
    int _sel_anchor = -1;                // where a Shift-click range starts
    float _smooth_strength = 0.5f;
    int _smooth_what = 0;
    // What several keys turn and scale about: 0 the origin, 1 their median,
    // 2 their mean.
    int _pivot = 1;
    float _cam_size = 1.0f;
    double _time = 0.0;                  // the playhead, seconds
    bool _playing = false;
    double _play_from = 0.0, _play_clock = 0.0;
    bool _loop = true;

    // Preview.
    FrameRenderer _frames;
    PreviewMode _preview_mode = PreviewMode::Corner;
    float _preview_scale = 0.5f;         // of the output size, capped
    float _beside_share = 0.5f;
    // The picture in the corner: its width as a share of the pane's, where
    // it was drawn, and a drag of its inner corner resizing it.
    float _pip_share = 0.32f;
    float _pip_rect[4] = {0, 0, 0, 0};
    bool _pip_resizing = false, _pip_hot = false, _pip_right = true;
    float _pip_pane_w = 0.0f;
    bool over_pip() const;
    std::string _preview_key;
    bool _preview_wanted = true;
    int _preview_w = 0, _preview_h = 0;
    float _through_drag[2] = {0, 0};

    // Viewport interaction.
    TransformTool _xform;
    XformKind _handle_mode = XformKind::Move;
    int _hot = -1;                       // key under the pointer
    int _handle_hot = -1;                // TransformTool handle under it
    std::vector<Keyframe> _xform_from;   // the keys as the operator found them
    bool _pick_target = false;
    bool _pick_waiting = false;
    int _fly_block_key = 0;
    float _mouse[2] = {0, 0};
    bool _mouse_in = false;
    // A press that selects on release: an empty spot clears the selection,
    // a key already selected becomes the only one unless it was dragged.
    float _press[2] = {0, 0};
    bool _press_empty = false;
    int _press_key = -1;
    // Where the panel was last drawn, screen coordinates: a click there or
    // on the pane stops playback before it lands.
    float _panel_rect[4] = {0, 0, 0, 0};
    // The key the lens section shows, held while playing.
    int _lens_key = 0;

    // Timeline.
    int _drag_key = -1;
    double _drag_key_from = 0.0;
    // A shot's start, its arrival's end, or its own way out's start or end
    // (0..3), in hand.
    int _drag_shot = -1;
    int _drag_shot_part = 0;
    double _drag_shot_from = 0.0;
    bool _scrubbing = false;
    bool _timeline_hovered = false;
    float _timeline_zoom = 1.0f;

    // The flight: recording since `_fly_t0`, what it recorded, the lens it
    // started with, how it is fitted, and the keys the last fit made -- its
    // settings show while those still stand.
    bool _flying = false;
    double _fly_t0 = 0.0, _fly_elapsed = 0.0;
    std::vector<FlightSample> _flight;
    Lens _flight_lens;
    FlightFit _fit;
    bool _fit_length_set = false;
    std::string _fit_keys;

    Job _job;
    std::atomic<int> _encoder_probe{0};  // 0 unknown, 1 probing, 2 built-in works, 3 not
    std::atomic<int> _encoder_codecs{0}; // bit 0 H.264, bit 1 H.265, bit 2 AV1
    int _encoder_max[3][2] = {};         // largest frame per codec
    std::atomic<int> _ffmpeg_probe{0};   // 0 unknown, 1 asking, 2 known
    std::vector<std::string> _ffmpeg_encoders;
    bool _fallback_tried = false;        // the GPU encoder failed; ffmpeg next
    bool _export_after_pick = false;     // the render button asked where to
    bool _ask_overwrite = false;
    int _overwrite_frames = 0;           // frame files in the folder, or 0 for a file
    bool builtin_encodes(Codec codec, int width, int height) const;
    std::thread _probe;
    std::string _ffmpeg = "ffmpeg";

    mutable std::map<std::string, camhost::FrustumShape> _shapes;
    mutable std::string _cam_base_key;
    mutable double _cam_base = 0.0;
    mutable double _key_size = 0.0;
    mutable uint64_t _key_size_rev = 0;
    // Per key, the lines last drawn for it, which is what a click must hit.
    std::vector<std::vector<float>> _key_lines;

    std::vector<DatasetLens> _dataset_lenses;
    std::string _dataset_lenses_key;

    std::function<void(Pick, const std::string&, const std::string&)> _pick;
    std::function<void()> _to_edit, _on_leave;
    std::function<void(int)> _remove_model;
    std::function<void(const std::string&)> _add_model;
    std::function<void(int, bool)> _view_model;
    std::vector<std::string> _log;
    std::string _status;
    bool _status_err = false, _status_done = false;
};

}  // namespace render
}  // namespace gui

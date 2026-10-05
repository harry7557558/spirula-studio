#pragma once

// What a screen embeds to make a viewport editable: the document, the active
// tool, the panel they are driven from, and the glue that turns a drag into a
// selection. One per editable pane; the owner drives poll() once a frame.
//
// It reaches the viewport through ViewportInteractor alone, so the panel
// knows nothing about editing and this knows nothing about how the panel
// renders.

#include "app/gui/Picture.h"
#include "app/gui/SfmInProcess.h"
#include "app/gui/ViewportInput.h"
#include "app/gui/edit/Attributes.h"
#include "app/gui/edit/EditDoc.h"
#include "app/gui/edit/EditTool.h"
#include "app/gui/edit/PointsDoc.h"
#include "app/gui/edit/ElementGrid.h"
#include "app/gui/edit/SelectShape.h"
#include "app/gui/edit/TransformTool.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace gui {

class ViewportPanel;

// What produced a selection. Kept on the history step it made, so a setting
// changed at that point in the history re-runs THAT selection -- however many
// undos the user walked back to reach it.
struct SelectRecipe {
    enum Kind { Stencil, Grow, Shrink, Piece, Floaters };
    Kind kind = Stencil;
    int layer = 0;
    std::vector<uint8_t> before;
    Combine combine = Combine::Replace;
    ShapeStroke shape;          // Stencil
    ViewProjection view;        // Stencil
    int64_t seed = -1;          // Piece
};

class EditSession : public ViewportInteractor {
public:
    ~EditSession() override;

    // Takes the document over and installs itself on `panel`.
    void open(std::unique_ptr<EditDoc> doc, ViewportPanel* panel);
    void close();
    bool active() const { return _doc != nullptr; }
    EditDoc* doc() { return _doc.get(); }

    // Where a file picker comes from: the owner has the dialog. `folder` asks
    // for a directory rather than a file; `suggested` is the name to open a
    // save under.
    void set_pick_save(std::function<void(int target, const std::string& ext,
                                          bool folder,
                                          const std::string& suggested)> f) {
        _pick_save = std::move(f);
    }
    // Offered as a button when set: over to the render mode on this pane.
    void set_to_render(std::function<void()> f) { _to_render = std::move(f); }
    // Offered as a button on a dataset's sparse reconstruction once it is
    // saved: train on `dataset`, a copy of `source` when a save made one.
    void set_to_trainer(std::function<void(const std::string& dataset,
                                           const std::string& source)> f) {
        _to_trainer = std::move(f);
    }
    // Told after every successful save: what the document came from, where it
    // went, and the placement the file now carries in its own coordinates.
    void set_on_saved(std::function<void(const std::string& source, const std::string& saved,
                                         const spirula::Sim3& placement)> f) {
        _on_saved = std::move(f);
    }
    // Told when a kept repair replaced the model on disk: the pane and this
    // document are stale, and the owner reopens both.
    void set_on_model_replaced(std::function<void()> f) { _on_model_replaced = std::move(f); }
    // The owner's answer, once the user has chosen.
    void save_to(int target, const std::string& path);
    // Save over what the document came from, which is what the panel's Save
    // button and the quit dialog both mean.
    void save_in_place();
    // Ask for a place to save, the way the panel's "Save a copy" does.
    void ask_save_copy();
    bool can_save_in_place() const;
    bool can_save_copy() const;

    // A setting the tools read, changed through the history so that Ctrl+Z
    // puts it back and the selection it produced with it. Public because the
    // panel's widgets are what move them.
    void set_option(bool* slot, bool value, const spirula::i18n::Msg& name);
    void set_number(float* slot, float value, const spirula::i18n::Msg& name);
    void set_number(int* slot, int value, const spirula::i18n::Msg& name);

    // Anything the session wants said in the log panel.
    std::vector<std::string> drain_log();

    // Once a frame, before the viewport draws.
    void poll();
    // Off while another panel is showing -- the render's, over this same
    // pane -- whose keys G, X and Ctrl+A are then.
    void set_keys(bool on) { _keys_on = on; }
    // Other models open beside this one; then the Transform tab offers to
    // move them along (CompareView applies it).
    void set_others_open(bool on) { _others_open = on; }
    bool sync_others() const { return _others_open && _sync_others; }
    // The editing panel: tools, options, selection, actions, history.
    void draw_panel();
    // One line under the viewport: the active tool and its keys.
    void draw_status();

    bool owns_left_button() const override {
        return _tool.owns_pointer() || _xform.active() || _pick != Pick::None;
    }
    // The letter that switched tools is usually still down on the frame the
    // switch takes effect, so Q would both select Navigate and fly the camera
    // once. The block lifts when that key comes up.
    bool blocks_fly_keys() const override {
        return _tool.owns_keys() || _xform.active() || _fly_block_key != 0;
    }
    bool owns_right_button() const override { return _xform.active(); }
    // Once the model has been moved the renderers' own grid is in the wrong
    // place, so from then on the grid is this session's to draw.
    bool draws_world_grid() const override;
    // Whether what is on disk still matches the pane that was loaded from it:
    // false once a save went over the source, which is the owner's cue to
    // read the file again when the edit ends.
    bool saved_over_source() const { return _saved_over_source; }
    bool on_viewport_input(const ViewportInput& in) override;
    void draw_viewport_overlay(const ViewportOverlay& v) override;
    bool frame_bounds(double centre[3], double& radius) override;
    // A long job is in flight; editing waits for it.
    bool busy() const {
        return _comp_busy.load() || _save_busy.load() || _attr_busy.load() ||
               _repair_busy.load();
    }
    // Give up on it. The worker checks between cells, so this is not instant.
    void cancel_work();
    // How far along, for the bar the panel draws.
    int work_done() const { return _save_done.load(); }
    int work_total() const { return _save_total.load(); }

private:
    // ---- placing the model (EditTransform.cpp) ----
    enum class Pivot { Origin = 0, Median, Mean, Selection };
    enum class Pick { None = 0, Ground, Corner, Origin };
    // Saved coordinates (the file's, placement applied) -> the shared frame.
    spirula::Sim3 saved_to_shared() const;
    spirula::Sim3 base_frame() const;
    bool xform_frame(XformFrame& f);
    bool pointer_in_view(float& x, float& y) const;
    void pivot_model(double out[3], bool selection = false);
    void begin_xform(XformKind kind);
    // A step made in the shared frame / in saved coordinates, as one history
    // entry.
    void place_shared(const spirula::Sim3& step, const spirula::i18n::Msg& name);
    // `carry` takes the view along: an alignment is the WORLD being defined
    // under the model, and a model that leaves the screen looks like a bug.
    void place_saved(const spirula::Sim3& step, const spirula::i18n::Msg& name,
                     bool carry = false);
    void set_placement(const spirula::Sim3& next, const spirula::i18n::Msg& name,
                       bool carry = false);
    void push_placement();
    // Undo and redo across an alignment take the view with them too.
    void follow_history();
    void quarter_turn(int axis, bool negative);
    void auto_align();
    void ground_from_selection();
    void pick_align(float px, float py);
    // Live layer-0 points in saved coordinates, thinned to at most `cap`.
    std::vector<double> saved_points(int64_t cap, bool selected_only,
                                     std::vector<int64_t>* index = nullptr) const;
    void enter_transform();
    void draw_transform_tab(float full);

    // ---- repairing cameras through the SfM (EditRepair.cpp) ----
    enum class RepairKind { Replace, Snap, Missing, Audit };
    bool repair_available();
    void draw_repair_tab(float full);
    void start_repair(RepairKind kind);
    void poll_repair();
    void keep_repair();
    void discard_repair();
    // The Transform tool moves the selected cameras rather than the model.
    bool camera_mode() const;
    void begin_camera_move();
    PointsDoc::Poses moved_by(const spirula::Sim3& shared_step) const;
    void show_repair_preview();
    // The selected camera's photograph, and the view from where it stands.
    void draw_camera_photo(float full);
    void look_through(int64_t camera);
    void look_from(const std::array<double, 12>& c2w, int64_t like, bool move);
    void step_camera(int dir);
    // Drives the photo loader; true once `path`'s photo is on the texture.
    bool photo_of(const std::string& path);
    void draw_photo_overlay(const ViewportOverlay& v);
    // Images the model left out: placed by hand or guessed, then registered
    // from there.
    void read_missing();
    void draw_missing_section(float full);
    void pick_missing(int k);
    void place_at_view();
    void guess_missing();
    int64_t like_of(const std::string& name) const;

    // ---- selecting by attribute and by colour (EditAttributes.cpp) ----
    void draw_attribute_section(float full);
    void draw_colour_section(float full);
    void refresh_attribute();
    // A selection that replaces the last one of the same kind rather than
    // stacking on it: dragging a range again is an adjustment, not a new step.
    void begin_adjustable(int kind);
    void preview_adjustable(const std::vector<uint8_t>& w);
    void commit_adjustable(const std::vector<uint8_t>& w, const std::string& label);
    void pick_colour(float px, float py, bool append);
    void run_colour(bool commit);

    void apply_stroke(const ShapeStroke& s, const ViewportInput& in);
    // Compute the selection a recipe describes and either record it as a step
    // or write it straight in -- a setting change is its own step, so the
    // selection it re-derives must not be a second one.
    void run_recipe(const std::shared_ptr<const SelectRecipe>& r, bool push);
    void start_recipe(std::shared_ptr<SelectRecipe> r);
    void run_select(std::vector<uint8_t> w, std::string label);
    Combine combine_now(bool shift, bool ctrl) const;
    void set_layer(int i);
    float reach();
    void ensure_grid();
    // The labels for the current layer, live set and reach, computed on a
    // worker the first time they are asked for; the same answer then serves
    // every click after it.
    bool components_ready();
    void compute_components();
    void grow_shrink(bool grow);
    void select_component_under(float px, float py, Combine how);
    void keep_largest_components();
    void select_all(bool on);
    void invert_selection();
    void handle_keys();
    void note(const std::string& s);
    bool view(ViewProjection& out) const;
    // Wrap a setting change as a history step that re-derives the selection
    // the step it stands on produced.
    void run_setting(std::function<void(bool)> write, std::string label);

    std::unique_ptr<EditDoc> _doc;
    ViewportPanel* _panel = nullptr;
    EditTool _tool;
    ElementGrid _grid;
    SelectOptions _opt;
    int _combine = 0;                 // Combine, when no modifier is held
    int _save_target = 0;
    float _radius_mul = 2.0f;
    float _spacing = 0.0f;            // measured once per layer
    int _keep_components = 1;

    SelectResult _last_result;

    // A depth buffer over the live elements, built for one camera.
    OcclusionBuffer _occ;
    bool _occ_dirty = true;
    int64_t _occ_alive = -1;
    float _occ_pose[12] = {};

    // Connected components, cached against what they were computed from.
    struct Components {
        int layer = -1;
        int64_t alive = -1;
        float radius = -1.0f;
        std::vector<int32_t> label;
        std::vector<int64_t> sizes;
    };
    Components _comp;
    std::thread _comp_worker;
    std::atomic<bool> _comp_busy{false};
    std::atomic<bool> _cancel{false};
    // The recipe waiting on that worker, and whether it is a step of its own
    // or a re-derivation inside one.
    std::shared_ptr<SelectRecipe> _pending;
    bool _pending_push = true;
    int _fly_block_key = 0;

    // Saving, which for a linked mesh is several large files.
    std::thread _save_worker;
    std::atomic<bool> _save_busy{false};
    std::atomic<int> _save_done{0}, _save_total{0};
    std::string _save_error;
    std::string _save_path;

    // ---- placement ----
    TransformTool _xform;
    XformKind _xform_mode = XformKind::Move;
    int _xform_hot = -1;
    spirula::Sim3 _xform_from;        // the placement the running operator began at
    spirula::Sim3 _seen_placement;    // as of the last frame, for follow_history
    int _seen_head = 0;
    ToolId _xform_return = ToolId::Navigate;
    int _pivot = (int)Pivot::Median;
    Pick _pick = Pick::None;
    bool _align_yaw = true, _align_centre = true, _corner_to_origin = false;
    float _align_tol = 1.0f;          // x 1% of the model's extent
    bool _levelling_was = false, _levelling_touched = false;
    bool _moved_ever = false;         // the grid is this session's from here on
    struct Centres {
        int layer = -1;
        int64_t alive = -1, selected = -1;
        uint64_t sel_rev = 0;
        double median[3] = {0, 0, 0}, mean[3] = {0, 0, 0}, sel[3] = {0, 0, 0};
    } _centres;
    float _placement_fields[7] = {0, 0, 0, 0, 0, 0, 1};   // t, euler deg, s
    bool _fields_active = false;

    // ---- attributes ----

    // One axis of the plot: which attribute, its values, and what they were
    // computed FOR -- a neighbour search is not repeated because a selection
    // changed, only when what it depends on did.
    struct AttrAxis {
        int index = 0;                // into _attrs; -1 = no second axis
        std::vector<float> values;
        AttrHistogram hist;
        int attr = -1, layer = -1;
        int64_t alive = -1;
        uint64_t placement = 0, hist_rev = 0;
        bool failed = false;
    };
    bool axis_current(const AttrAxis& a) const;
    bool axis_ready(const AttrAxis& a) const;
    void draw_density_plot(float full);
    void apply_plot_stroke(const ShapeStroke& s, float w, float h);
    void select_plot_cluster(float x, float y, float w, float h);
    std::vector<Attr> _attrs;         // what the current layer offers
    int _attrs_layer = -1;
    AttrAxis _axis[2];
    // The slow ones are computed here; the job's answer is adopted by poll().
    std::thread _attr_worker;
    std::atomic<bool> _attr_busy{false};
    int _attr_job_axis = -1;
    AttrAxis _attr_job;
    // Two attributes against each other, and the tool that draws on them: the
    // same kind as the viewport's, with a stroke of its own in progress.
    AttrDensity _density;
    std::vector<int32_t> _density_cell;
    uint64_t _density_rev = 0;
    int _density_key[4] = {-1, -1, 0, 0};
    EditTool _plot_tool;
    float _plot_size[2] = {1, 1};     // as last drawn, for a stroke closed by a key
    // The repeated part of each plot axis, as a fraction of the axis; 0 for an
    // axis that is not periodic.
    float _plot_wrap[2] = {0, 0};
    bool _hist_log_counts = true;
    bool _range_outside = false;
    double _range[2] = {0.25, 0.75};  // fractions of the histogram's axis
    bool _range_set = false;
    int _range_drag = 0;              // 0 none, 1 low edge, 2 high edge, 3 new
    double _range_anchor = 0.0;
    // Colour samples, display-referred RGB, and how close is close.
    std::vector<float> _samples;
    std::vector<float> _colours;
    uint64_t _colours_key = 0;
    float _colour_tol = 0.08f, _colour_light = 1.0f;
    // The adjustable selection in flight: what it started from, and which
    // history position it left behind when it was last committed.
    std::vector<uint8_t> _adjust_before;
    int _adjust_kind = 0, _adjust_head = -1;
    bool _adjust_live = false;
    double _preview_at = 0.0;

    // ---- repair ----
    std::thread _repair_worker;
    std::atomic<bool> _repair_busy{false}, _repair_cancel{false};
    int _repair_avail = -1;           // -1 not yet looked, 0 no, 1 yes
    std::string _repair_model;        // the model folder a Keep writes over
    std::string _repair_stage;        // scratch folder of the run in flight or shown
    std::string _repair_error;
    InProcessResult _repair_result;
    std::vector<RepairResultLine> _repair_report;
    bool _repair_shown = false;       // a finished run is waiting on Keep or Discard
    bool _repair_match = true;
    bool _cam_xform = false;          // the running operator moves cameras
    PointsDoc::Poses _cam_from;       // every hand move as the operator began
    PointsDoc::Poses _cam_start;      // the selected cameras' poses then
    std::function<void()> _on_model_replaced;
    spirula::Sim3 _repair_moved;      // the placement the staged input was written with
    // The photo panel: one load in flight, the latest request wins.
    struct Photo {
        std::thread worker;
        std::atomic<bool> busy{false};
        std::mutex mu;
        std::string want, loaded, shown;  // file paths
        Picture pic;
        unsigned tex = 0;
        int w = 0, h = 0;
    } _photo;
    bool _photo_overlay = false;      // the photo laid over the viewport
    float _photo_alpha = 0.5f;
    std::vector<std::string> _missing;  // under the image folder, sorted
    bool _missing_read = false;
    int _missing_at = -1;
    int64_t _missing_cam = -1;        // the camera selected when it was picked
    std::string _images_dir;
    PointsDoc::PlacedMap _placed_carry;  // across the reopen a Keep causes
    std::string _placed_carry_dir;
    std::mutex _repair_log_mtx;       // the worker writes, drain_log reads
    std::vector<std::string> _repair_log;

    int _tab = 0;                     // 0 select, 1 transform, 2 repair
    bool _tab_force = false;
    bool _saved_over_source = false;

    std::function<void(int, const std::string&, bool, const std::string&)>
        _pick_save;
    std::function<void()> _to_render;
    std::function<void(const std::string&, const std::string&)> _to_trainer;
    // The save in flight, or the one being asked for, ends in the trainer.
    bool _train_after_save = false;
    bool _ask_train = false;
    // The dataset the trainer button would open, "" when there is none.
    std::string trainer_dataset() const;
    int folder_target() const;
    void draw_trainer_button(float full);
    std::function<void(const std::string&, const std::string&, const spirula::Sim3&)> _on_saved;
    spirula::Sim3 _save_placement;
    std::vector<std::string> _log;
    std::string _status;              // formatted, already translated
    bool _status_err = false;
    // Set when the Save button is pressed; the panel puts the question up and
    // clears it when it is answered.
    bool _ask_overwrite = false;
    bool _keys_on = true;
    bool _others_open = false;
    bool _sync_others = true;
};

}  // namespace gui

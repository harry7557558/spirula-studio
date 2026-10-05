#pragma once

// RoiEditor -- draw the region of interest a dataset trains in over its
// reconstruction: boxes, ellipsoids, cylinders and outlines, added to or cut
// from each other (data/RoiDocument.h), saved under <dataset>/roi/ where the
// trainer finds them. Opened beside Partition on the dataset screen and from
// the training screen's region row; docs/notes/roi-editor.md.

#include "app/gui/ViewportInput.h"
#include "app/gui/ViewportPanel.h"
#include "app/gui/edit/SelectShape.h"
#include "core/Similarity.h"
#include "data/DatasetParser.h"
#include "data/RoiDocument.h"

#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace spirula { struct RegionOverlay; }

namespace gui {

class RoiEditor : public ViewportInteractor {
public:
    struct Hooks {
        std::function<void(const std::string& line)> log;
        // A region of `dataset` was saved or deleted.
        std::function<void(const std::string& dataset)> changed;
    };

    RoiEditor();
    ~RoiEditor() override;

    // `file` empty opens the first saved region, or a new one.
    void open(const std::string& dataset_dir, Hooks hooks, const std::string& file = "");
    bool is_open() const { return _open; }
    // Once per frame, over whichever screen is up.
    void draw();
    void destroy_gl();

    bool owns_left_button() const override;
    bool blocks_fly_keys() const override;
    bool owns_right_button() const override;
    bool frame_bounds(double centre[3], double& radius) override;
    bool on_viewport_input(const ViewportInput& in) override;
    void draw_viewport_overlay(const ViewportOverlay& v) override;

private:
    enum class Mode { Adjust = 0, Move, Resize, Rotate };
    // What a press took hold of: the selected shape's own handles or sides,
    // or a body. Face moves one side (Adjust); Stretch both sides of an axis
    // and Scale all of them (Resize); Wall is an outline's edge.
    enum class Grab { None, Body, Center, Axis, Face, Wall, Corner, Midpoint, Stretch, Scale, Ring };
    struct Handle {
        Grab kind = Grab::None;
        int index = 0;   // axis, corner or edge
        int sign = 1;    // which face along the axis
    };
    struct Overlays {
        std::shared_ptr<const spirula::RegionOverlay> mesh;
        uint64_t gen = 0;
    };

    void run_load();
    void poll_load();
    void close_now();
    void reset_history();
    void touch();
    void commit();
    void undo();
    void redo();
    void select(int i);

    void load_file(const std::string& path);
    void new_region();
    void save();
    void delete_file();
    void refresh_files();
    // Asks first when the region has unsaved changes.
    void guard(std::function<void()> then);
    bool dirty() const { return _editable && _doc != _saved; }

    // ---- frames: the shapes are kept in the dataset's frame, edited in the
    // shared frame the viewport navigates, where +Z is up ----
    bool view(ViewProjection& out) const;
    void update_frame();
    spirula::RoiShape to_shared(const spirula::RoiShape& s) const;
    spirula::RoiShape to_raw(const spirula::RoiShape& s) const;
    double shared_size() const;

    // ---- shapes ----
    std::string next_name(spirula::RoiShapeKind kind) const;
    void add_shape(spirula::RoiShape raw);
    void add_kind(spirula::RoiShapeKind kind);
    void start_box();
    // A cylinder or an ellipsoid where subject_start found the subject.
    void start_subject(spirula::RoiShapeKind kind);
    // Numpad . on the selection, for a shape the user did not place by hand.
    void frame_selected();
    bool subject_start(spirula::RoiShape& out) const;
    void start_outline(int replace = -1);
    void set_draw_frame(const ViewProjection& cam);
    void finish_outline();
    void end_outline();
    void fit_depth(spirula::RoiShape& shared) const;

    // ---- the view ----
    void refresh_overlays();
    void begin_drag(const Handle& h, const ViewProjection& cam, float x, float y);
    void update_drag(const ViewProjection& cam, float x, float y, bool shift, bool ctrl);
    void end_drag(bool keep);
    Handle hit_handle(const ViewProjection& cam, float x, float y) const;
    // The side of the selected shape (shared frame) under the pointer.
    Handle side_under(const spirula::RoiShape& s, const ViewProjection& cam, float x, float y) const;
    void draw_side(ImDrawList* dl, const ViewProjection& cam, const ImVec2& origin,
                   const spirula::RoiShape& s, const Handle& h) const;
    int pick_shape(const ViewProjection& cam, float x, float y, double* depth = nullptr) const;
    void handle_keys();

    // ---- panels ----
    void draw_file_row();
    void draw_start();
    void draw_shape_list();
    void draw_properties();
    void draw_stats();
    void draw_toolbar();
    void draw_confirm();

    bool _open = false;
    std::string _dataset;
    Hooks _hooks;
    std::string _want_file;

    // ---- loading ----
    std::thread _worker;
    std::atomic<bool> _busy{false};
    std::atomic<bool> _done{false};
    std::mutex _mu;
    std::string _load_error;
    ParsedDataset _ds;
    PostSplitCameras _post;
    bool _loaded = false;
    std::vector<double> _points;    // dataset frame, [N,3]
    std::vector<double> _cams;      // camera centres, dataset frame
    spirula::Aabb _bounds;          // robust, dataset frame
    spirula::Sim3 _to_view;         // dataset frame -> the preview's model frame
    bool _subject_ok = false;       // the cameras circle a subject: _subject_raw
    spirula::RoiShape _subject_raw;

    // ---- the document ----
    spirula::RoiDocument _doc;
    spirula::RoiDocument _saved;
    std::vector<spirula::RoiDocument> _history;
    int _hist_pos = 0;
    int _sel = -1;
    bool _editable = true;
    uint64_t _gen = 1;
    std::vector<std::string> _files;
    std::string _path;             // what _doc was read from or saved to
    std::string _name;
    std::string _status;           // an error
    std::string _notice;           // what the last save did

    // ---- the view ----
    ViewportPanel _view;
    bool _attached = false;
    spirula::Sim3 _S, _S_inv;      // dataset frame <-> shared frame
    Mode _mode = Mode::Adjust;
    bool _show_all = true;
    Handle _hot;
    int _hover = -1;
    // A drag: what it holds, the shape as it was (shared frame), and where.
    Handle _grab;
    bool _armed = false;           // a body press that has not moved yet
    spirula::RoiShape _start;
    float _press[2] = {0, 0};
    double _anchor[3] = {0, 0, 0};
    double _plane_n[3] = {0, 0, 1};
    double _push_dir[3] = {0, 0, 1};   // shared frame, what Wall and Scale drag along
    double _push_n[2] = {0, 0};        // a Wall's outward normal, in the outline's plane
    double _t0 = 0.0;
    double _angle = 0.0, _last_angle = 0.0;
    bool _press_empty = false;
    float _empty_xy[2] = {0, 0};
    ViewportInput _in;
    // An outline being drawn: x,y pairs on _draw_frame's plane (shared frame).
    bool _drawing = false;
    bool _draw_fixed = false;
    bool _was_ortho = false;
    int _draw_replace = -1;
    std::vector<double> _draw_pts;
    spirula::RoiShape _draw_frame;

    // ---- what the region keeps ----
    uint64_t _inside_gen = 0;
    double _inside_time = 0.0;
    std::shared_ptr<const std::vector<uint8_t>> _inside;
    int64_t _n_inside = 0, _n_cams_inside = 0;
    std::shared_ptr<const spirula::RegionOverlay> _mesh;
    uint64_t _mesh_gen = 0;
    std::future<Overlays> _mesh_job;

    enum class Confirm { None, Discard, Delete };
    Confirm _confirm = Confirm::None;
    bool _confirm_open = false;
    std::function<void()> _after_discard;
};

}  // namespace gui

#pragma once

// The sparse-reconstruction document: the seed cloud a dataset trains from
// and the cameras that produced it, open for cleaning.
//
// Two layers, because they are two different things to point a lasso at: the
// points, and the camera centres. Saving is a row filter over the files they
// were read from (data/SparseEdit.h) -- the tracks of a removed image go with
// it, and nothing else in the reconstruction is rewritten.

#include "app/gui/edit/EditDoc.h"
#include "data/DatasetParser.h"
#include "data/SparseEdit.h"

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace gui {

class PointsDoc : public EditDoc {
public:
    // `dataset_dir` is "" for a loose PLY. `show` rebuilds the GL preview from
    // the display cloud, with a selection flag and a colour (or null) per
    // displayed camera.
    using Show = std::function<void(const ParsedDataset&, const PostSplitCameras&,
                                    const uint8_t* selected, const float* rgb)>;
    PointsDoc(ParsedDataset ds, PostSplitCameras post,
              const std::string& source, const std::string& dataset_dir,
              Show show);

    Kind kind() const override { return Kind::Points; }
    std::vector<SaveTarget> save_targets() const override;
    void save(int target, const std::string& path,
              std::atomic<int>* progress) override;
    std::string default_save_path(int target) const override;
    void revert_display() override;
    bool live_centers(dsparse::CenterTable& out) const override;
    spirula::Sim3 view_frame() const override;
    bool up_hint(float up[3]) const override;
    bool colours(std::vector<float>& rgb) const override;
    bool colours_available() const override {
        return layer() == 0 && !_ds.points.rgb.empty();
    }
    std::vector<float> camera_centres() const override;
    const ParsedDataset* dataset() const override { return &_ds; }
    // Read from the model's own files the first time it is asked for, which
    // is on the attribute worker: images.bin alone can be a hundred megabytes.
    const spirula::SparseStats* sparse_stats() const override;
    bool has_sparse_stats() const override {
        return _fmt == spirula::SparseFormat::Colmap;
    }

    spirula::SparseFormat format() const { return _fmt; }
    const std::string& dataset_dir() const { return _dataset_dir; }

    // Cameras moved by hand: where Repair starts snapping them from. Save does
    // not write them. Raw file frame, OpenGL camera-to-world, row-major 3x4.
    using Poses = std::map<int64_t, std::array<double, 12>>;
    const Poses& moved_cameras() const { return _moved; }
    void set_moved_cameras(Poses p);
    std::array<double, 12> camera_pose(int64_t i) const;
    const std::string& image_file(int64_t i) const { return _ds.image_filenames[(size_t)i]; }
    // What a folder save would drop, which is also what a repair starts without.
    spirula::SparseKeep sparse_keep() const;
    std::array<double, 12> original_pose(int64_t i) const;
    const ParsedDataset& parsed() const { return _ds; }

    // A repair's answer, drawn before it is kept: not an edit, so not in the
    // history. Poses are camera-to-world like Poses.
    enum class Mark : uint8_t { Moved, Added, Failed };
    struct AddedCamera {
        std::string name;
        int64_t like = 0;  // a camera whose intrinsics it is drawn with
        std::array<double, 12> pose;
    };
    struct RepairPreview {
        Poses poses;
        std::map<int64_t, Mark> marks;
        std::vector<AddedCamera> added;
        bool empty() const { return poses.empty() && marks.empty() && added.empty(); }
    };
    void set_repair_preview(RepairPreview p);
    const RepairPreview& repair_preview() const { return _preview; }

    // Images the model left out, put where the user thinks they were taken:
    // where Register starts them from. Keyed by name under the image folder.
    struct Placed {
        int64_t like = 0;  // a camera whose intrinsics it is drawn with
        std::array<double, 12> pose;
    };
    using PlacedMap = std::map<std::string, Placed>;
    const PlacedMap& placed() const { return _placed; }
    void set_placed(PlacedMap p);
    // The one the panel is working on, drawn as selected.
    void set_placed_current(const std::string& name);

protected:
    void publish_impl(bool geometry) override;

private:
    // The layer order, which is also what the panel offers.
    enum Layer { kPoints = 0, kCameras = 1 };
    // The display copy, with dead cameras dropped and its post-split table
    // rebuilt for them. Rebuilt only when the live camera set changes.
    void rebuild_display(bool cameras_changed);

    ParsedDataset _ds;
    PostSplitCameras _post;
    ParsedDataset _display;
    PostSplitCameras _post_display;
    std::vector<uint8_t> _cam_highlight;   // per live camera
    int64_t _live_cameras = -1;            // what the display was baked for
    Poses _moved;
    bool _poses_dirty = false;
    RepairPreview _preview;
    PlacedMap _placed;
    std::string _placed_current;
    // What each camera of the display dataset is.
    enum class Row : uint8_t { Live, Ghost, Added, Placed };
    struct DisplayRow {
        Row kind;
        int64_t index;  // camera, into _preview.added, or the nth of _placed
    };
    std::vector<DisplayRow> _display_rows;
    std::vector<float> _cam_rgb;
    void append_camera(int64_t like, const std::array<double, 12>& c2w);
    std::string _dataset_dir;
    spirula::SparseFormat _fmt = spirula::SparseFormat::None;
    // The files as this session found them; every save filters these again.
    spirula::SparseBaseline _baseline;
    mutable spirula::SparseStats _stats;
    mutable bool _stats_read = false;
    Show _show;
};

// Both ends are stored, as make_placement_op does.
std::unique_ptr<EditOp> make_camera_move_op(PointsDoc& doc, PointsDoc::Poses next,
                                            std::string label);
std::unique_ptr<EditOp> make_place_missing_op(PointsDoc& doc, PointsDoc::PlacedMap next,
                                              std::string label);

}  // namespace gui

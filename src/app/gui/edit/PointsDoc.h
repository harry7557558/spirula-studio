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

#include <functional>
#include <string>

namespace gui {

class PointsDoc : public EditDoc {
public:
    // `dataset_dir` is "" for a loose PLY. `show` is called with the display
    // cloud whenever it changes, which is how the GL preview is rebuilt; its
    // last argument is one flag per live camera, for the frustum highlight.
    using Show = std::function<void(const ParsedDataset&, const PostSplitCameras&,
                                    const uint8_t* selected)>;
    PointsDoc(ParsedDataset ds, PostSplitCameras post,
              const std::string& source, const std::string& dataset_dir,
              Show show);

    Kind kind() const override { return Kind::Points; }
    std::vector<SaveTarget> save_targets() const override;
    void save(int target, const std::string& path,
              std::atomic<int>* progress) override;
    std::string default_save_path(int target) const override;
    // Copies go beside the originals, where Starting points in the trainer
    // lists them: a new model in sparse/, another PLY in dense/.
    std::string copy_dir(int target) const override;
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
    std::string _dataset_dir;
    spirula::SparseFormat _fmt = spirula::SparseFormat::None;
    // The files as this session found them; every save filters these again.
    spirula::SparseBaseline _baseline;
    mutable spirula::SparseStats _stats;
    mutable bool _stats_read = false;
    Show _show;
};

}  // namespace gui

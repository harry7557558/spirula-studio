#pragma once

// "Recompute Sparse Point Cloud" on the training screen: new points for the
// cameras a dataset already has, which stay byte for byte as they are. Runs
// `spirula sfm auto --poses` as a child and puts its points in place of the
// model's own, kept as *_original (docs/notes/fixed-poses.md).

#include "app/gui/SfmProgress.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace gui {

class RecomputePanel {
public:
    struct Source {
        std::string dataset;
        std::string image_dir, mask_dir;   // as the trainer has them, may be relative
        bool mask_flipped = false;
    };
    ~RecomputePanel();

    // Above the region row. `busy`: training owns the dataset. `start` asks
    // the app for the device to run on, "" for automatic, or false to refuse.
    void draw(const Source& src, bool busy, const std::function<bool(std::string&)>& start);
    bool running() const { return _running.load(); }
    void cancel();
    // Lines for the log since the last call: (text, detail).
    std::vector<std::pair<std::string, bool>> drain_log();
    // True once after the model's points changed, recomputed or restored.
    bool take_changed();

private:
    void run(std::vector<std::string> argv, std::string out_dir, std::string model);
    void log(const std::string& s, bool detail = false);

    bool _open = false;
    std::string _model_for, _model;   // the dataset, and the model it would keep
    std::atomic<bool> _restorable{false};   // _model holds a recompute and its originals
    int _quality = 2;   // kSfmQuality
    int _features = 0;  // kSfmFeatures
    int _max_features = 0, _max_image_size = 0;
    bool _use_masks = true;

    std::thread _worker;
    std::atomic<bool> _running{false}, _cancel{false}, _changed{false};
    std::mutex _mu;
    std::vector<std::pair<std::string, bool>> _log;
    std::string _progress_dir;
    int64_t _status_mtime = 0;
    RunStatus _status;
};

}  // namespace gui

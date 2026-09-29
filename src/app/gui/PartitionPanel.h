#pragma once

// PartitionPanel -- split a finished reconstruction into parts that train
// separately (data/ScenePartition.h), see the split before committing to it,
// queue the parts in Batch, and merge their models back into one. Opened from
// the dataset screen beside "Open in Trainer"; docs/notes/scene-partition.md.

#include "app/gui/ViewportPanel.h"
#include "data/DatasetParser.h"
#include "data/RegionMesh.h"
#include "data/ScenePartition.h"
#include "data/SparseEdit.h"
#include "i18n/Message.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace gui {

class PartitionPanel {
public:
    struct Hooks {
        // Queue one training run per part of the saved partition file.
        std::function<int(const std::string& partition_json, int num_parts)> queue_batch;
        std::function<void(const std::string& path)> open_splat;
        std::function<void()> open_batch;
        std::function<void(const std::string& line)> log;
    };

    PartitionPanel();
    ~PartitionPanel();

    void open(const std::string& dataset_dir, Hooks hooks);
    bool is_open() const { return _open; }
    void close();
    // Once per frame from the dataset screen.
    void draw();
    void destroy_gl();

private:
    enum class Job { None, Load, Compute, Merge };

    void start(Job job);
    void run_load();
    void run_compute();
    void run_merge();
    void poll();
    void refresh_view();
    void draw_controls();
    void draw_parts_table();
    void draw_merge();
    bool options_changed() const;

    bool _open = false;
    std::string _dataset;
    Hooks _hooks;

    // ---- worker ----
    std::thread _worker;
    std::atomic<bool> _busy{false};
    std::atomic<Job> _job{Job::None};
    std::atomic<bool> _done{false};
    std::atomic<bool> _cancel{false};
    bool _cancelled = false;   // under _mu: the last job was stopped
    std::mutex _mu;
    std::string _error;

    // ---- what the worker produced (read on the GUI thread once _done) ----
    ParsedDataset _ds;
    PostSplitCameras _post;
    spirula::SparseStats _tracks;
    bool _loaded = false;
    spirula::Covisibility _cov;
    spirula::PartitionOptions _cov_opt;   // the options _cov was built with
    bool _cov_valid = false;
    spirula::ScenePartition _part;
    std::vector<spirula::RegionMesh> _part_meshes;   // per part, its boundary
    spirula::PartitionOptions _part_opt;  // the options _part was built with
    spirula::PartitionOptions _compute_opt;   // the options the running compute uses
    bool _part_valid = false;
    // A finished compute's result, moved into _part by poll() on the GUI thread.
    spirula::ScenePartition _new_part;
    std::vector<spirula::RegionMesh> _new_meshes;
    std::vector<std::string> _log_lines;

    // ---- options on screen ----
    spirula::PartitionOptions _opt;
    int _mode = 1;   // 0: number of parts, 1: cameras per part
    int _source_idx = 0;

    // ---- the view ----
    ViewportPanel _view;
    bool _view_dirty = false;
    int _show_part = -1;   // -1: every part
    bool _owner_colors = true;
    bool _show_grid = true;
    bool _gl_ok = true;

    // ---- save / batch / merge ----
    std::string _save_path;
    std::string _saved_path;   // where the CURRENT partition was last written
    std::string _status;   // an error
    const spirula::i18n::Msg* _notice = nullptr;   // not one, e.g. a cancelled compute
    std::string _runs_dir;
    std::vector<std::string> _runs;
    bool _scanned = false;
    std::string _merge_out;
    std::string _merged_path;
    std::string _merge_status;
    std::string _merge_error;
};

}  // namespace gui

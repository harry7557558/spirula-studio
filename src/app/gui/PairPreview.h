#pragma once

// PairPreview -- the two images behind a cell of the match map, with the
// features found on them and the matches that survived between them.
//
// The map says whether two views matched; this says what that meant. A pair
// whose matches all sit on one wall, or on the moving thing the mask was
// supposed to remove, will register badly, and no count of inliers shows it.
//
// It draws whatever of the run's files are there: during matching, keypoints
// and no matches yet; after a run has swept its intermediates away, only the
// pictures. Loading is on its own thread and the latest request wins, so
// sweeping the cursor across the map costs one load, not one per cell.

#include "app/gui/GlLoader.h"
#include "app/gui/Picture.h"
#include "app/gui/SfmProgress.h"    // KeyPoint2D

#include "imgui.h"

#include <cstddef>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace gui {

class PairPreview {
public:
    ~PairPreview();

    // Where this run's files are. Cheap every frame; a change is what makes it
    // re-read the image list.
    // `live_matches` is read while matching runs and matches.bin is not
    // there yet; empty when the run has none.
    void configure(const std::string& image_dir, const std::string& mask_dir,
                   const std::string& features_dir,
                   const std::string& matches_path,
                   const std::string& live_matches = {});
    // Draw the pair of this block, numbered as the reconstruction numbers
    // them, that holds the most matches -- or its first pair when none does.
    void show(const PairBlock& block);
    bool empty() const;
    void clear();

    // Into the current window, `box` pixels. GL context must be current.
    void draw(const ImVec2& box);
    void destroy_gl();

private:
    // One image of the pair, ready to draw.
    struct Side {
        std::string name;
        Picture pic;
        std::vector<KeyPoint2D> pts;   // as fractions of the picture
    };
    struct Shot {
        PairBlock block;
        uint32_t a = 0, b = 0;    // the pair of the block drawn
        bool loaded = false;      // this pair was looked for ...
        bool ready = false;       // ... and at least one picture came back
        Side left, right;
        // Four floats per match: the two ends, each a fraction of its own
        // picture. Thinned; `matches` is how many there were.
        std::vector<float> lines;
        size_t matches = 0;
    };
    struct Pane {
        GLuint tex = 0;
        int w = 0, h = 0;
    };
    // One row of matches.bin's pair table, without the matches themselves: an
    // index of where each pair's array is, so hovering a cell reads that pair
    // and not the gigabyte around it. Kept in the shape sfm::MatchesIndex
    // writes it, so that this header needs nothing from src/sfm.
    struct PairEntry {
        uint32_t a = 0, b = 0, count = 0;
        uint64_t offset = 0;
    };

    void start();
    void stop();
    void worker_loop();
    void load(const PairBlock& block, Shot& out);
    // The matches file to read and _pairs indexed from it; empty when neither
    // matches.bin nor the live file is there yet.
    std::string index_pairs(const std::string& matches_path,
                            const std::string& live_matches);
    void upload(Pane& p, const Picture& pic);
    // One side's picture and dots in `rect`, plus the ends of the match lines.
    void draw_side(const Pane& p, const Side& s, const ImVec2& min,
                   const ImVec2& size);

    mutable std::mutex _mu;
    std::condition_variable _cv;
    std::thread _worker;
    bool _stop = false;

    std::string _image_dir, _mask_dir, _features_dir, _matches_path;
    std::string _live_matches;
    bool _paths_dirty = false;
    // The pair asked for, the one the worker is on, and the one that came
    // back. All three are needed because show() is called on every frame the
    // cursor is over a cell: without knowing what is already in flight it
    // would re-ask for the same pair forever and the worker would never get to
    // publish one.
    bool _has_request = false;
    PairBlock _req;
    bool _loading = false;
    PairBlock _load;
    Shot _result;
    bool _result_new = false;

    // ---- worker thread only ----
    std::vector<std::string> _stems;    // image index -> its name in the run
    std::vector<PairEntry> _pairs;      // sorted, for a binary search
    int64_t _pairs_mtime = 0;
    std::string _w_features_dir, _w_matches_path;
    // Which file _pairs was built from, so switching to matches.bin when the
    // stage finishes rebuilds the index instead of reusing live offsets.
    std::string _pairs_src;
    // Long edge one side of the pair is drawn at, from the last draw: what the
    // pictures are sized for (Picture.h).
    int _target = 640;

    // ---- UI thread only ----
    Shot _shot;
    Pane _left, _right;
};

}  // namespace gui

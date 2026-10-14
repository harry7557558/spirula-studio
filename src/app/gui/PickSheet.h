#pragma once

// PickSheet -- every frame a dataset run chose, kept or left out, with what
// the rule measured on it (app/FramePick.h), its file and a thumbnail.
//
// Filled by the run's worker thread and read by the screen, which walks the
// rule again over these to show what another threshold would keep. Thumbnails
// are held as JPEGs: a few kilobytes a pick, whatever the source.

#include "app/FramePick.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace gui {

// Long edge of a thumbnail: what a hover shows at full size.
inline constexpr int kPickThumbSide = 256;

// `rgb` (w*h*3) as a JPEG at most kPickThumbSide on its long edge; empty when
// there is nothing to encode. Cheap enough for the thread feeding a decoder.
std::vector<uint8_t> pick_thumb_jpeg(const uint8_t* rgb, int w, int h);

struct Pick : app::FramePick {
    // Bits its hash differs from the last frame kept by. Filled by whoever
    // walks the rule, since it depends on what that walk kept.
    int dist = 64;
    bool as_run = true;    // the verdict is the one the run gave it
};

class PickSheet {
public:
    // ---- producer (worker thread) ----
    // One row per input of the run, named; what an earlier run left is dropped.
    void reset(std::vector<std::string> names);
    // `path` is the frame's own file, wherever the run put it; "" for none.
    void add(size_t row, const app::FramePick& p, const uint8_t* rgb, int w, int h,
             std::string path);
    // `origin` is the photograph it was made from, which outlives a copy the
    // run deleted ("" for a video frame). `more` are the frame's other files,
    // a rig's other lenses, which change sides with it.
    void add(size_t row, const app::FramePick& p, std::vector<uint8_t>&& jpeg,
             std::string path, std::string origin = {}, std::vector<std::string> more = {});
    // Where `row`'s kept frames and its left-out ones are written. With both
    // known a frame can change sides after the run (resort).
    void set_dirs(size_t row, std::string keep_dir, std::string skip_dir);
    // `row` holds frames a finished dataset was sorted from, each with its
    // camera -- not the frames an extraction chose among.
    void set_in_place(size_t row, bool yes);
    bool in_place(size_t row) const;
    // The rule walked again over `row`, each frame's verdict replaced and its
    // file moved to the side it now belongs on. Returns the files moved.
    int resort(size_t row, app::PickRule rule);
    // `row`'s left-out frames deleted from its skip folder, and the folders
    // that empties with them. Returns the files deleted.
    int discard_skipped(size_t row);
    // The files of `row`'s left-out frames, as paths under its folders.
    std::vector<std::string> left_out(size_t row) const;
    void clear() { reset({}); }

    // ---- consumer ----
    // Changes whenever reset() runs: a cache keyed by row and index is stale.
    uint64_t generation() const;
    std::vector<std::string> names() const;
    std::vector<Pick> picks(size_t row) const;
    bool any() const;
    // Empty for a pick that came without pixels.
    std::vector<uint8_t> jpeg(size_t row, size_t index) const;
    // The pick's own file, or failing that its origin: something to look at.
    std::string path(size_t row, size_t index) const;

private:
    struct Row {
        std::vector<Pick> picks;
        std::vector<std::vector<uint8_t>> jpegs;
        std::vector<std::string> paths, origins;
        std::vector<std::vector<std::string>> more;
        std::string keep_dir, skip_dir;
        bool in_place = false;
    };
    mutable std::mutex _mu;
    std::vector<Row> _rows;
    std::vector<std::string> _names;
    uint64_t _gen = 0;
};

}  // namespace gui

#pragma once

// The reader for what a running reconstruction writes about itself.
//
// The other half of sfm/core/Progress.h: the GUI passes `--progress-dir` to its
// own child and polls the three files that appear there. Each is written whole
// and renamed, so a read either gets the previous snapshot or this one.
//
// Polling rather than watching on purpose -- two files at 2 Hz is a pair of
// stats, and an inotify/ReadDirectoryChangesW pair is a portability problem for
// nothing.

#include "data/DatasetParser.h"

#include <cstdint>
#include <string>
#include <vector>

namespace gui {

// A reconstruction in progress, in the shape ViewportPanel::attach_preview_data
// takes. `ds` carries only what the preview reads -- poses, intrinsics and
// points; there is no dataset behind this and no images to load.
struct LiveModel {
    ParsedDataset ds;
    PostSplitCameras post;
    uint32_t n_images = 0;      // in the capture
    uint32_t n_registered = 0;  // ... and in the model
    uint64_t n_points = 0;      // before subsampling
    bool empty() const { return n_registered == 0 && ds.points.num() == 0; }
};

// Inlier counts between image ranges. `bins` is `n_images` for a capture small
// enough and 512 above that, so a cell is a block of images rather than one.
struct PairMatrix {
    uint32_t n_images = 0, bins = 0;
    std::vector<uint32_t> counts;   // bins*bins, symmetric
    // Candidate pairs in the cell, and how many of them verification has
    // reached. Both empty when the source cannot say (a finished matches.bin
    // holds the pairs that survived and nothing about the rest), which is what
    // `staged()` asks before a cell is drawn as pending or as filtered out.
    std::vector<uint32_t> planned, verified;
    uint32_t peak = 0;
    bool empty() const { return bins == 0 || counts.empty(); }
    bool staged() const { return planned.size() == counts.size(); }
    uint32_t at(uint32_t r, uint32_t c) const { return cell(counts, r, c); }
    uint32_t cell(const std::vector<uint32_t>& v, uint32_t r, uint32_t c) const {
        const size_t i = (size_t)r * bins + c;
        return r < bins && c < bins && i < v.size() ? v[i] : 0;
    }
};

// Where the run is and how it ended (sfm/core/Progress.h, status.bin). What
// the dataset screen used to recover by parsing the child's translated stdout
// and its exit code, which could report only one of "partial" and "not metric".
struct RunStatus {
    // sfm::Stage: 0 extract, 1 match, 2 map, 3 merge, 4 orient, 5 finish,
    // 6 load, 7 select, 8 seed, 9 refine.
    uint32_t stage = 0;
    bool finished = false;
    bool partial = false;
    bool metric = true;
    int64_t done = 0, total = 0;
    int64_t registered = 0, images = 0, points = 0, models = 0;
    double mean_reproj = 0.0;
    // -1 when the stage cannot say, which is every stage before it starts.
    float fraction() const {
        return total > 0 ? (float)((double)done / (double)total) : -1.0f;
    }
};

bool read_status(const std::string& dir, int64_t& mtime, RunStatus& out);

// Where the mapping bar stops with every image placed: the finishing solves
// come after the last one, and a phase that places none holds it here or at
// zero and moves its label instead.
inline constexpr float kMappingBarFull = 0.94f;

// The mapping bar at `done` of `total` images placed. Not the ratio: an image
// costs what the model it joins costs to solve, so the count runs ahead of the
// clock (docs/notes/sfm-design.md D77).
float mapping_fraction(int64_t done, int64_t total);

// `dir` is the --progress-dir the child was given. All return false when the
// file is absent, unfinished or not newer than `mtime` -- which the caller
// keeps, so a poll that finds nothing new costs one stat.
bool read_live_model(const std::string& dir, int64_t& mtime, LiveModel& out);
bool read_pair_matrix(const std::string& dir, int64_t& mtime, PairMatrix& out);

// The same matrix from a finished `matches.bin`, which is what a run leaves
// behind: the live file is deleted with the rest of the intermediates.
bool read_pair_matrix_from_matches(const std::string& matches_path,
                                   int64_t& mtime, PairMatrix& out);

// A keypoint as a preview draws it: where it is, and how big the detector said
// the feature was. `r` is 0 when the detector reports no scale -- ALIKED has
// none -- and those are drawn at a fixed size.
struct KeyPoint2D {
    float x = 0, y = 0, r = 0;
};

// Keypoints of one extracted image, in its own pixels, or false when the
// feature file is not there yet. Descriptors are skipped -- they are the file,
// and nothing here looks at them. `width` / `height` of 0 asks for the
// coordinates in whatever size the extractor measured them at.
//
// `rel_stem` is the image's path under the image directory without its
// extension ("cam0/01973"): the feature files mirror the image tree, so that
// stems from different camera folders cannot collide.
bool read_keypoints(const std::string& features_dir, const std::string& rel_stem,
                    int width, int height, std::vector<KeyPoint2D>& out);
bool read_keypoints_file(const std::string& path, int width, int height,
                         std::vector<KeyPoint2D>& out);

// The image order a reconstruction numbers by: the feature files' stems,
// sorted, which is exactly how matching indexes its images. Falls back to the
// names a finished matches.bin carries, since a finished run deletes the
// feature files it was given.
std::vector<std::string> read_image_stems(const std::string& features_dir,
                                          const std::string& matches_path);

}  // namespace gui

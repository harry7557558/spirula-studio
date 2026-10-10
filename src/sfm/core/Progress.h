#pragma once

// Snapshots of a run in progress, for a front end that is watching it.
//
// Off unless `--progress-dir` was given, so an ordinary CLI run writes nothing
// and pays for nothing. The GUI passes one and polls the files; that is the
// whole of the channel, because the reconstruction is a child process and its
// stdout already belongs to the user (SfmRunner.h says why it still is one).
//
// Every file is written whole to a `.tmp` and renamed, so a reader never sees
// half of one, and every write is rate-limited by wall clock -- the mapper
// registers an image every few milliseconds on a small capture and every few
// seconds on a large one, and a screen wants the same cadence from both.

#include "sfm/core/Events.h"

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace sfm {

struct Point3D;
struct Reconstruction;

namespace progress {

// Longest side of the pair matrix, and the point budget of a model snapshot.
// Both are what a screen can show rather than what the run holds: 512^2 u32 is
// 1 MB, and 50k points is already more than a preview resolves.
inline constexpr uint32_t kMatrixBins = 512;
inline constexpr uint32_t kMaxPoints = 50000;

// Where snapshots go. Empty (the default) disables everything below.
void set_dir(const std::string& dir);
bool enabled();

// What the model's frame is worth, carried into the next model.bin so a front
// end drawing it can say whether a unit is a metre (sfm/Pipeline.h ModelGauge).
void gauge(bool oriented, bool metric);

// model.bin: "VKPM", u32 version=4, flags (1 oriented, 2 metric), images, registered,
// u64 points; per registered image { u32 id, f32 c2w[12] OpenGL, u32 w, h,
// colmap_model_id, nparams, f64 params[] }; u32 count, { f32 xyz, u8 rgb }.

// images.bin, beside model.bin: "VKPI", u32 version=1, u32 images; per image
// { u32 id, u8 placed, u32 keypoints, u32 points (seeing a 3D point), f32 mean and
// f32 max reprojection error in pixels, u32 length, name }.

// The capture's images by the ids models use, so images.bin lists the unplaced.
void images(const std::vector<std::string>& names, const std::vector<uint32_t>& keypoints);
// The same file anywhere, whether or not --progress-dir was given: the run
// keeps one in its workspace, which outlives .progress.
bool write_image_stats(const Reconstruction& rec, const std::string& path);

// The model as it stands, subsampled to kMaxPoints. Call it as often as is
// convenient; it returns immediately until the interval has passed, unless
// `force` says this is the last word on a stage.
//
// `color` fills one point's rgb, and is called only for the points a snapshot
// actually writes: the mapper colours a model when it finishes one, so without
// it every preview of a model under construction is neutral grey.
using PointColor = std::function<void(const Point3D&, uint8_t rgb[3])>;
void model(const Reconstruction& rec, bool force = false,
           const PointColor& color = {});

// pairs.bin: "VKPP", u32 version=2, u32 images, u32 bins, then three
// bins*bins u32 planes -- summed inliers, candidate pairs, verified pairs.
// The last two are what tell a cell nothing has reached it yet from a cell
// pairing was never going to try.
//
// Matching is about to verify `pairs` among `n_images` images.
void begin_matching(uint32_t n_images,
                    const std::vector<std::pair<uint32_t, uint32_t>>& pairs);
// One verified pair, `inliers` of 0 meaning it did not survive verification.
// Safe to call from the verification workers.
void pair(uint32_t image1, uint32_t image2, uint32_t inliers);

// status.bin: "VKPS", u32 version=1, u32 stage, u32 flags (1 finished,
// 2 partial, 4 metric), i64 done, total, registered, images, points, models,
// f64 mean_reproj.
//
// Where the run is and how it ended, so a front end watching a child reads the
// same facts an in-process one gets from the event stream instead of parsing
// the log. Rate-limited like the rest; a stage change or a result forces it.
void status(const Event& e);

// thumbs/<rel_stem>.jpg: the working copy the extractor has already decoded and
// downscaled, at kThumbLong on its long side.
//
// Without it a screen showing the frames as they are extracted has to decode
// the source file a second time -- a 24 MP JPEG per frame, on its own thread,
// which is what made the reel lag the stage it was drawing.
void thumbnail(const std::string& rel_stem, const uint8_t* rgb, int w, int h);
inline constexpr int kThumbLong = 640;

// live_matches.bin: a matches.bin whose pair count is `kStreamingPairs`,
// appended as verification produces each pair, so hovering the match map draws
// a verified pair instead of nothing until the stage ends.
void live_matches_begin(const std::vector<std::string>& names,
                        const std::vector<uint32_t>& num_features);
void live_pair(uint32_t a, uint32_t b, int32_t config,
               const uint32_t* idx1, const uint32_t* idx2, size_t stride,
               uint32_t count);

// Write whatever is buffered, whatever the clock says. Call at the end of a
// stage so the last state on screen is the final one.
void flush();

}  // namespace progress
}  // namespace sfm

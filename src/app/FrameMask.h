#pragma once

// FrameMask -- the part of every frame that is never scene.
//
// A fisheye border, a watermark, a timestamp and the rig holding the camera
// are all fixed in image space for a whole capture, so they are geometry
// rather than segmentation: no model, no per-frame inference, one stencil per
// camera intersected with whatever else masking produced.
//
// Shapes are normalized to the image (x, rx by width; y, ry by height) so one
// stencil serves every resolution the same frames are stored at.

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace app {

struct MaskShape {
    enum class Kind { Ellipse, Rect };
    Kind  kind = Kind::Ellipse;
    bool  remove = false;           // false = keep what is inside
    float cx = 0.5f, cy = 0.5f;     // rect: the two corners go in cx,cy / rx,ry
    float rx = 0.5f, ry = 0.5f;
};

// Shapes are applied IN ORDER, each one adding its inside to what is kept or
// taking it away, so the last shape covering a pixel decides it. That is what
// lets a crescent be cut: a box takes a band out and an ellipse over it puts
// most of the band back. The base is the whole frame, unless the first shape
// keeps -- then it is what defines the region and there is nothing before it.
struct FrameMask {
    std::vector<MaskShape> shapes;
    // An image whose dark pixels are dropped, intersected with the shapes. The
    // escape hatch for a region no ellipse or rectangle describes.
    std::string image;
    bool empty() const { return shapes.empty() && image.empty(); }
};

// "ellipse 0.5,0.5,0.49,0.49; -rect 0.2,0.9,0.8,1": ';'-separated shapes, a
// leading '-' meaning "remove what is inside this one". A rect takes its two
// corners, an ellipse its centre and its two radii. FrameMask::image has no
// spelling here -- it is a path, and carrying one through a ';'-separated
// string is a quoting problem for no gain.
bool parse_mask_shapes(const std::string& spec, std::vector<MaskShape>& out,
                       std::string& error);
std::string format_mask_shapes(const std::vector<MaskShape>& shapes);

// 255 = keep: inside every keep-shape (or everywhere, when the stencil has
// none), minus every remove-shape, minus the dark pixels of `image`.
bool rasterize_frame_mask(const FrameMask& m, int width, int height,
                          std::vector<uint8_t>& out, std::string& error);

// Any image file as a 0/255 stencil at its own size. Used for FrameMask::image
// and to intersect with masks that are already on disk.
bool load_stencil(const std::string& path, int& width, int& height,
                  std::vector<uint8_t>& out);

// The header only, without decoding.
bool image_size(const std::string& path, int& width, int& height);

// Any image file as interleaved 8-bit RGB. The GUI's preview reads its frames
// through this so that a build without the inference layer still has one.
bool load_rgb(const std::string& path, int& width, int& height,
              std::vector<uint8_t>& out);

struct BorderDetectOptions {
    int   samples = 24;         // frames read per camera
    int   dark = 16;            // luma at or below this counts as black
    float shrink = 0.01f;       // pull the boundary in, fraction of its radius
    int   rays = 360;
    float max_residual = 0.02f; // RMS radial fit error, fraction of the radius
};

// How the border was told from the scene. Activity -- where the frame never
// resolves anything -- leads; a lit flare ring outside the circle is not black.
// Dark answers the capture too still for it.
enum class BorderCue { None, Dark, Activity };

struct BorderDetect {
    bool      found = false;
    MaskShape shape;             // keep-inside ellipse, `shrink` applied
    BorderCue cue = BorderCue::None;
    float     residual = 0.0f;
    float     dark_fraction = 0.0f;
    float     kept_fraction = 1.0f;
    int       frames = 0;        // frames actually read
    int       width = 0, height = 0;
};

// Reads `samples` frames spread across `files` and fits the boundary of the
// image circle. `files` is one camera's frames, in capture order.
BorderDetect detect_fisheye_border(const std::vector<std::string>& files,
                                   const BorderDetectOptions& o);

// The same, for a caller that produces frames itself. Holding 24 frames of a
// 360 camera would be 265 MB, so they are folded in one at a time; `add`
// ignores anything whose size differs from the first.
class BorderAccumulator {
public:
    BorderAccumulator();
    ~BorderAccumulator();
    BorderAccumulator(const BorderAccumulator&) = delete;
    BorderAccumulator& operator=(const BorderAccumulator&) = delete;

    void add(const uint8_t* px, int width, int height, int channels);
    int  frames() const;
    BorderDetect finish(const BorderDetectOptions& o) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ---------------------------------------------------------------------------
// Applying a stencil to a folder of frames
// ---------------------------------------------------------------------------

// One input's decision: the shapes drawn on it, and whether the lens circle
// should also be fitted. Both, because the two answer different questions --
// a dual-fisheye file writes two cameras whose circles differ by about 1% of
// the radius, and whatever drew the shapes only ever saw one of them.
struct FrameStencil {
    FrameMask mask;
    bool  detect_border = false;
    float shrink = 0.01f;      // overrides BorderDetectOptions::shrink
    bool empty() const { return mask.empty() && !detect_border; }
};

// Image files under `dir`, grouped by the folder holding them -- a folder is a
// camera, which is what a multi-track extraction writes (cam0/, cam1/). Keys
// are relative to `dir`, "" for `dir` itself; each list is sorted.
std::map<std::string, std::vector<std::string>> group_frames_by_camera(
    const std::string& dir, const std::string& skip_dir = "");

struct FrameStencilRun {
    std::string image_dir, mask_dir;
    // Masks the stencil is intersected with, mirroring the image tree. "" is
    // mask_dir itself, which is how a segmentation pass folds its own output
    // in; naming another folder is how masks that came WITH the photos do.
    std::string merge_dir;
    // Those masks mark what to REMOVE, not what to keep. Applied on read, so
    // what this writes is in the one convention everything else reads.
    bool flip_merge = false;
    FrameStencil stencil;
    BorderDetectOptions detect;
    bool replace = false;      // ignore masks already in mask_dir
    bool dry_run = false;      // resolve the shapes and report, write nothing
    std::string skip_dir;      // a subtree of image_dir not to walk
};

struct FrameStencilSinks {
    std::function<void(const std::string& camera, int64_t frames)> camera;
    // What this camera ended up with. `border` describes the fit when the
    // stencil asked for one; `border.found` is false when it did not.
    std::function<void(const std::string& camera, const FrameMask&,
                       const BorderDetect&)> resolved;
    std::function<void(int64_t done, int64_t total)> progress;
    const std::atomic<bool>* cancel = nullptr;
};

// Writes one mask per image into `mask_dir`, mirroring the image tree.
// Returns how many were written, or -1 with `error` set.
int64_t apply_frame_stencil(const FrameStencilRun& run,
                            const FrameStencilSinks& sinks, std::string& error);

}  // namespace app

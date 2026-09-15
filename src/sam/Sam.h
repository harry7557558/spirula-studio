#pragma once
// SAM 2 / SAM 3 segmentation and tracking -- the subsystem's public header.
//
// Nothing here exposes Vulkan or the internal tensor types, so a consumer (the
// GUI, the CLI, a dataset tool) includes this and knows nothing about the
// compute layer underneath. Device selection and teardown are nn's
// (nn/Device.h); the host image type is nn::Image (nn/io/Image.h).
//
// No function throws. Failures return false / an empty result and leave a
// description in `Session::lastError()`.
//
//   sam::Session session;
//   session.loadModel({.model_path = "sam3-f16.ggml"});
//   session.encodeImage(image);                     // once per frame
//   sam::Result r = session.segmentConcept({.text = "yellow school bus"});
//
// A session holds one frame's features; several prompts may run against it.

#include "nn/io/Image.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sam {

// ---------------------------------------------------------------------------
// Device request
// ---------------------------------------------------------------------------

// Records the physical identity for `device`, resolved through the shared
// precedence: the caller's value, else SS_VK_DEVICE, else Auto. False with
// `error` on a bad selection, which the caller reports, never falls back.
bool freeze_device(const std::string& device, std::string& error,
                   bool validation = false, bool profile = false);

// ---------------------------------------------------------------------------
// Data types
// ---------------------------------------------------------------------------

struct Point {
    float x = 0.0f;
    float y = 0.0f;
};

// Axis-aligned box in pixels of the ORIGINAL image, top-left / bottom-right.
struct Box {
    float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
};

using nn::Image;

struct Mask {
    int width = 0;
    int height = 0;
    float iou_score = 0.0f;
    float obj_score = 0.0f;
    int   instance_id = -1;
    std::vector<uint8_t> data;     // width*height, 0 or 255
};

struct Detection {
    Box   box;
    float score = 0.0f;            // presence * per-query for PCS, IoU for PVS
    float iou_score = 0.0f;
    int   instance_id = -1;
    Mask  mask;
    // The SAM decoder's output token, kept so the tracker can derive an object
    // pointer from an interactive prompt. Empty for detector results.
    std::vector<float> sam_token;
};

struct Result {
    std::vector<Detection> detections;
};

// ---------------------------------------------------------------------------
// Parameters
// ---------------------------------------------------------------------------

struct ModelParams {
    std::string model_path;
    // "auto", a Vulkan ordinal, a unique name substring, or "uuid:<32 hex>",
    // resolved by core/VulkanDeviceSelection.h. Empty leaves SS_VK_DEVICE and
    // then Auto in charge; a bad value fails the load.
    std::string device;
    int         img_size = 0;        // 0 = the checkpoint's native resolution
    bool        validation = false;  // enable the Vulkan validation layers
    // Collect per-kernel GPU timestamps, for Session::printProfile(). Has to be
    // set before the device is created, which is why it lives here rather than
    // being a flag on the print call.
    bool        profile = false;
};

// Promptable Concept Segmentation: a noun phrase and/or exemplar boxes, and
// every matching instance comes back.
struct ConceptPrompt {
    std::string      text;
    std::vector<Box> pos_exemplars;
    std::vector<Box> neg_exemplars;
    float            score_threshold = 0.5f;
    float            nms_threshold = 0.1f;
    // Cap on how many queries get a mask decoded. The detector always scores
    // all 200; this bounds the segmentation head, which is the expensive part.
    int              max_detections = 100;
};

// Promptable Visual Segmentation: clicks and/or a box, one instance out.
struct VisualPrompt {
    std::vector<Point> pos_points;
    std::vector<Point> neg_points;
    Box                box;
    bool               use_box = false;
    // Return the three ambiguity-resolving masks instead of the single one.
    bool               multimask = false;
};

struct VideoParams {
    // Empty text runs the tracker in visual-only mode: instances are added by
    // addInstance() and only propagated, never re-detected.
    std::string text_prompt;
    float score_threshold = 0.5f;
    float nms_threshold = 0.1f;
    float assoc_iou_threshold = 0.1f;
    int   hotstart_delay = 15;     // frames a new masklet must survive
    int   max_keep_alive = 30;     // frames a masklet may go unseen
    int   fill_hole_area = 16;     // in pixels of the output mask
    // Run the detector every N frames instead of every frame; in between,
    // instances are carried by the memory bank alone, which is what having one
    // is for. Worth about 110 ms/frame per text phrase on SAM 3 -- little next
    // to its backbone with one phrase, 1.17x with the three a masking pipeline
    // typically uses, since the detector runs once per phrase and the backbone
    // once. The trade is latency: an object entering mid-clip is not picked up
    // until the next detection frame. 1 = detect on every frame.
    int   detect_every = 1;
    // Cap on spatial memory frames per instance, below the checkpoint's
    // num_maskmem (7). Memory cross-attention is linear in this and dominates
    // the per-frame cost once the bank is full, so halving it nearly halves
    // tracking time -- at the cost of a shorter memory. 0 = the model's own.
    int   max_memory_frames = 0;
};

// ---------------------------------------------------------------------------
// Session
// ---------------------------------------------------------------------------

class Tracker;

class Session {
public:
    Session();
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    bool loadModel(const ModelParams& params);
    bool isLoaded() const;

    // Frees the weights, the derived tables and the current frame's features,
    // and returns the session to "no model loaded". Called by the destructor,
    // so this is only needed to hand a couple of gigabytes back early -- an
    // application that segments and then does something else on the same GPU
    // (a reconstruction, a training run) wants it before that starts.
    //
    // Every Tracker built from this session must already be destroyed.
    void unload();

    // True when the checkpoint carries the text encoder and detector; a
    // visual-only checkpoint supports segmentVisual() and propagation only.
    bool supportsTextPrompts() const;

    // Runs the backbone. Every prompt below reuses the result, so encode once
    // per frame and prompt as often as you like.
    bool encodeImage(const Image& image);
    bool imageEncoded() const;

    Result segmentConcept(const ConceptPrompt& prompt);
    Result segmentVisual(const VisualPrompt& prompt);

    const std::string& lastError() const;

    // Human-readable VRAM breakdown, for diagnostics.
    std::string vramReport() const;
    // Per-kernel GPU timing, when SS_PROFILE=1 is set.
    void printProfile();

private:
    friend class Tracker;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ---------------------------------------------------------------------------
// Video tracking
// ---------------------------------------------------------------------------

class Tracker {
public:
    Tracker(Session& session, const VideoParams& params);
    ~Tracker();
    Tracker(const Tracker&) = delete;
    Tracker& operator=(const Tracker&) = delete;

    // Encode the frame, propagate every tracked instance, and -- when a text
    // prompt is set -- run the detector and associate its output with the
    // tracks. Returns the masks for this frame.
    Result trackFrame(const Image& frame);

    // Propagation only: the visual-only path, for instances added by hand.
    Result propagateFrame(const Image& frame);

    // The same two, on the frame ALREADY encoded into the session. Several
    // trackers -- one per prompt -- can then share a single backbone pass,
    // which is the whole cost of a frame:
    //     session.encodeImage(frame);
    //     for (auto& t : trackers) t.trackEncoded();
    Result trackEncoded();
    Result propagateEncoded();

    // Start tracking a new instance from clicks/a box on the CURRENT frame
    // (the one most recently encoded). Returns its id, or -1.
    //
    // `mask_out` receives this frame's mask for the new instance. It is worth
    // asking for: the propagation pass for this frame ran before the instance
    // existed, so its mask is not in that Result, and running the pass again to
    // get it would advance the frame counter a second time.
    int addInstance(const VisualPrompt& prompt, Mask* mask_out = nullptr);

    // Correct an existing instance with extra clicks on the current frame.
    bool refineInstance(int instance_id, const std::vector<Point>& pos_points,
                        const std::vector<Point>& neg_points,
                        Mask* mask_out = nullptr);

    int  frameIndex() const;
    void reset();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ---------------------------------------------------------------------------
// I/O helpers
// ---------------------------------------------------------------------------

// Reading and writing plain images is nn's (nn/io/Image.h); these two know
// about masks and detections.
bool  save_mask_png(const Mask& mask, const std::string& path);
// Writes the image with each detection's mask tinted and its box outlined.
bool  save_overlay_png(const Image& image, const Result& result,
                       const std::string& path);

}  // namespace sam

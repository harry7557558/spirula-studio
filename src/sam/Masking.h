#pragma once
// Prompt-to-mask policy, shared by the CLI, the GUI's preview and its run:
// positive phrases unioned; negative ones carved back out (a region matching
// one is KEPT); the mask says what to keep, so prompted objects come out black
// unless `keep_prompted`; a longest-side cap on what the model sees. Also the
// visual path (clicks, SeedPrompt) and the two non-SAM models, BiRefNet's
// subject and Grounding DINO's boxes (MaskOptions). src/sam/README.md.

#include "sam/Sam.h"

#include <cstdint>

#include <memory>
#include <string>
#include <vector>

namespace sam {

// One object's clicks, drawn on one frame.
//
// Two things follow from SAM's design and both are load-bearing here. Objects
// are SEPARATE: a model prompted with a click on the dog and a click on the
// bicycle returns one mask that fits neither, so each object gets its own id
// and its own tracked instance, and the masks are unioned afterwards. And a
// click belongs to the FRAME it was drawn on: pointing at (900, 500) says
// nothing about frame 400 of a moving capture, which is why this carries a
// frame rather than being applied to all of them.
//
// Several seeds sharing an `object` refine one instance as the capture goes
// on: the first is what starts the track, the rest are corrections at the
// frames where it drifted. That is exactly the SAM 2 conditioning-frame model.
struct SeedPrompt {
    sam::VisualPrompt prompt;
    int     object = 0;
    int64_t frame  = 0;
};

struct MaskOptions {
    // A SAM checkpoint, or a BiRefNet one (id or path): BiRefNet masks the
    // image's main subject and reads no prompt -- text and clicks are ignored.
    std::string model;
    // Grounding DINO (id or path). Set, it finds the text prompts' boxes and
    // `model` segments them -- lang-segment-anything, which gives a SAM 2
    // checkpoint words. Every frame is detected on its own; clicks still track.
    std::string detector;
    float detector_threshold = 0.3f;   // a box's best token probability
    // The device request, in the spelling core/VulkanDeviceSelection.h parses:
    // "auto", an ordinal, a name substring, or "uuid:<32 hex>". Empty leaves
    // SS_VK_DEVICE and then Auto in charge; a bad value fails the run.
    std::string device;
    std::string text, neg_text;
    // Clicks seeding tracked instances. The only way to prompt a SAM 2
    // checkpoint, and usable alongside text on a SAM 3 one.
    std::vector<SeedPrompt> seeds;

    bool  video = true;           // track across frames vs. segment each alone
    bool  keep_prompted = false;  // white = the prompted objects
    // Move every detection's boundary by this share of its own bounding-box
    // mean side before the union. SIGNED: positive grows the region, covering
    // the halo a tight outline leaves; negative trims inside the outline.
    float dilate_ratio = 0.05f;
    float threshold = 0.5f, nms = 0.1f;
    int   detect_every = 1;
    int   memory_frames = 0;
    // Longest side handed to the model; 0 = off. Masks still come back at the
    // source resolution. mask.py's --max_image_size, same default.
    int   max_size = 1600;
    int   img_size = 0;
    bool  validate = false;
    // Per-kernel GPU timestamps; see sam::ModelParams::profile.
    bool  profile = false;
};

// "person; car;  bicycle" -> {"person", "car", "bicycle"}. Blank entries drop.
std::vector<std::string> split_phrases(const std::string& s);

// Longest-side cap. Returns `src` unchanged when it already fits.
nn::Image downscale_to_fit(const nn::Image& src, int max_size);

// Euclidean pixels a detection of `box` moves by at `dilate_ratio`, signed as
// the ratio is. A fraction of the object's own size, so it is the same fraction
// whatever resolution the box is measured in, and a distant object moves less.
int dilate_radius_px(const Box& box, float dilate_ratio);

// ORs one detection's mask into `hit` (1 = covered), its boundary first moved
// by `radius` Euclidean pixels: outward for a positive one, stopping at the
// frame border rather than wrapping, and inward for a negative one.
void accumulate_dilated(const Mask& mask, int radius, std::vector<uint8_t>& hit);

// The union of `positive`, each detection moved by `dilate_ratio`, minus every
// pixel `negative` covers. The order is load-bearing and is why this is one
// function: a negative phrase is an explicit keep and must beat the margin.
void compose_hit(const Result& positive, const Result& negative,
                 float dilate_ratio, std::vector<uint8_t>& hit);

// Whether `model` (id or path) is a BiRefNet checkpoint: the mask is the
// subject, so a job needs neither prompt nor clicks.
bool is_subject_model(const std::string& model);

class Masker {
public:
    Masker();
    ~Masker();
    Masker(const Masker&) = delete;
    Masker& operator=(const Masker&) = delete;

    bool init(const MaskOptions& o, std::string& error);

    // Writes a mask the size of `image`: 255 where the pixel should be KEPT.
    // `overlay_out`, when given, receives the raw detections for diagnostics.
    //
    // `frame_id` says which frame of the source this is, and is what
    // SeedPrompt::frame is matched against. Callers that hand over every frame
    // in order can leave it at -1 and get a plain 0, 1, 2 counter; the video
    // extractor passes the decoded index instead, because it writes only the
    // sharpest frame of each window and the two do not line up. A seed lands on
    // the first frame at or after the one it was drawn on, so it is never lost
    // to a frame that was skipped.
    bool run(const nn::Image& image, sam::Mask& out, sam::Result* overlay_out,
             int64_t frame_id = -1);

    const std::string& lastError() const;
    sam::Session& session();
    // Whether init() chose BiRefNet: the mask is the subject, no prompt used.
    bool subjectMode() const;
    // Returns every model's weights to the device; init() loads them again.
    void unload();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace sam

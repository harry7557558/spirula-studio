#pragma once
// Prompt-to-mask policy, shared by ssam-cli and ssam-extract.
//
// This is the semantic layer reference/scripts/mask.py implements in
// Python, kept in one place so the two tools cannot drift:
//
//   * several positive phrases, semicolon separated, unioned;
//   * negative phrases carved back out -- a region matching one is KEPT even
//     when it also matches a positive phrase;
//   * the output mask says what to KEEP, so by default the prompted objects
//     come out black and everything else white. `keep_prompted` flips that,
//     for the case where the prompt names the subject rather than a distractor;
//   * a longest-side cap on what the model sees, with the mask returned at the
//     source resolution.
//
// It also covers the visual path, where there is no text at all and instances
// are seeded from clicks -- see SeedPrompt.

#include "sam/Sam.h"

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
    std::string model, device;
    std::string text, neg_text;
    // Clicks seeding tracked instances. The only way to prompt a SAM 2
    // checkpoint, and usable alongside text on a SAM 3 one.
    std::vector<SeedPrompt> seeds;

    bool  video = true;           // track across frames vs. segment each alone
    bool  keep_prompted = false;  // white = the prompted objects
    // Grow every detection by this share of its own bounding-box mean side
    // before the union: SAM's outlines hug the object and leave a halo of its
    // colour behind. It grows the detected region, either side of keep_prompted.
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

// Euclidean pixels a detection of `box` grows by at `dilate_ratio`. A fraction
// of the object's own size, so it is the same fraction whatever resolution the
// box is measured in, and a distant object gets a smaller margin than a close one.
int dilate_radius_px(const Box& box, float dilate_ratio);

// ORs one detection's mask into `hit` (1 = covered), first grown by `radius`
// Euclidean pixels. Growth stops at the frame border rather than wrapping: the
// rest of the pipeline reads a mask as a plain image, seam or no seam.
void accumulate_dilated(const Mask& mask, int radius, std::vector<uint8_t>& hit);

// The union of `positive`, each detection grown by `dilate_ratio`, minus every
// pixel `negative` covers. The order is load-bearing and is why this is one
// function: a negative phrase is an explicit keep and must beat the margin.
void compose_hit(const Result& positive, const Result& negative,
                 float dilate_ratio, std::vector<uint8_t>& hit);

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

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace sam

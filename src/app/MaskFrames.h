#pragma once
// Frames in, mask PNGs out: the one masking loop behind `spirula sam track`
// and the dataset screen. It runs one sam::Masker in this process, or one
// `sam --mask-worker` child (app/MaskWorker.h) per entry of `workers`.
// Feature phrases (the sky) go into the same mask as kMaskSegment
// (app/FrameMask.h), so SfM drops them and training keeps them.

#include "nn/io/Image.h"
#include "sam/Masking.h"
#include "sfm/core/Exif.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace app {

struct MaskFrame {
    std::filesystem::path source;
    std::filesystem::path output;
    int64_t frame = 0;   // what sam::SeedPrompt::frame matches
    bool write = true;   // false: masked only to carry a track through it
};

struct MaskFramesJob {
    sam::MaskOptions mask;
    std::string gamut;              // the frames' colour, as app/FrameLook.h reads it
    std::optional<bool> linear;
    std::string exposure;           // colorspace::parse_exposure's spelling
    std::vector<MaskFrame> frames;
    bool train = true;              // the prompted mask; false starts from all-keep
    bool features = false;          // `feature_text`, folded in as kMaskSegment
    // Canonical "uuid:" selectors, one child each; empty masks in process.
    std::vector<std::string> workers;
    // Worker staging, on the outputs' volume so publishing is a rename.
    std::filesystem::path scratch;
};

struct MaskFrameDone {
    size_t index = 0;               // into MaskFramesJob::frames
    double kept = 0;                // share of pixels above 127
    // In process only, both in the file's stored orientation.
    const nn::Image* image = nullptr;
    const sam::Mask* mask = nullptr;
};

struct MaskFramesSinks {
    const std::atomic<bool>* cancel = nullptr;
    std::function<void(const std::string&)> log;    // worker output
    std::function<void(const std::string&)> warn;   // a frame skipped
    // In the model's frame (`turn` from the stored one), before the feature
    // fold; may replace or cut into `mask`. Called from worker threads too.
    std::function<bool(size_t index, const sfm::ExifTransform& turn, sam::Mask& mask,
                       std::string& why)> compose;
    // Once per frame, serialized. With workers, after its file is published.
    std::function<void(const MaskFrameDone&)> done;
    // With workers, after `done` and outside its lock, for slow follow-up.
    std::function<void(size_t index)> settled;
    // In process only: the detections over the upright frame.
    std::function<void(size_t index, const nn::Image& image, const sam::Result& r)> overlay;
    // In process only: the masker before it is freed, for its VRAM and profile.
    std::function<void(sam::Masker&)> report;
};

struct MaskFramesStats {
    double load_ms = 0, model_ms = 0, write_ms = 0;   // in process only
};

// Whether `o` asks for a prompted mask, and for a feature one.
bool mask_has_target(const sam::MaskOptions& o);
bool mask_has_features(const sam::MaskOptions& o);

bool mask_frames(const MaskFramesJob& job, const MaskFramesSinks& sinks, std::string& error,
                 MaskFramesStats* stats = nullptr);

}  // namespace app

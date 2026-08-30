#pragma once

// DataManager — host-side training-data orchestrator.
//
// Owns the on-disk dataset layout, decodes images via stb_image, batches
// indices by image-shape group (so each engine_train_step sees uniform
// [B,H,W,C] inputs), and feeds the resulting batches directly into the engine.
//
// The two cache modes trade RAM for per-step latency:
//
//   CPU    every image is decoded up front into a host byte buffer, so
//          next_*_batch() only gathers rows. Peak RAM ~= the whole decoded
//          dataset.
//   DISK   nothing is pre-decoded. Three worker pools (four with masks) run
//          per modality -- RGB+MASK / DEPTH / NORMAL -- each with its own
//          bounded job and ready queues, keeping `prefetch_batches` batches in
//          flight. The split is per modality because depth and normal dominate
//          decode cost on dense-supervision datasets, and one shared pool lets
//          them starve the RGB stream.
//
// Formats are whatever stb_image reads: 8- and 16-bit PNG (one channel for
// depth / masks, three or four for RGB / normals), JPEG, BMP, TGA.

#include "core/Common.cuh"
#include "core/Tensor.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>


// Thrown by next_train_step() / next_val_batch() when a decode worker is
// parked on a file it could not read. Answer with resolve_data_error().
class DataDecodeError : public std::runtime_error {
public:
    explicit DataDecodeError(const std::string& what)
        : std::runtime_error(what) {}
};


// ===========================================================================
// Public configuration
// ===========================================================================

enum class CacheMode {
    CPU,    // pageable host RAM, all images pre-decoded at construction
    DISK    // async per-modality prefetch pool, nothing pre-decoded
};

struct DataManagerConfig {
    CacheMode cache_mode = CacheMode::CPU;

    bool load_masks   = true;
    bool load_depths  = true;
    bool load_normals = true;

    // Train batch size. Picked per-group at next_train_batch() time — groups
    // with fewer than batch_size images yield a smaller batch.
    int  train_batch_size = 1;

    // Eval batch size. Ignored when val_indices is empty.
    int  val_batch_size   = 1;

    // RNG seed for group sampling and shuffle. 0 lets the implementation pick
    // a nondeterministic value (steady_clock).
    uint64_t seed = 0;

    // Worker count per modality pool (DISK mode only). Defaults to a small
    // fixed number; tune up for fast disks / many CPU cores.
    int workers_rgb    = 16;
    int workers_depth  = 8;
    int workers_normal = 8;

    // How many batches to keep in flight in the prefetch pipeline (DISK only).
    // Hard upper bound on RAM consumed by ready / partially-ready batches.
    int prefetch_batches = 4;

    // Swap keep and ignore in every mask, before mask_boundary_offset. For
    // masks that mark what to EXCLUDE, the other convention in the wild.
    bool flip_mask = false;

    // Signed boundary offset applied to binarized masks at decode time,
    // expressed as a fraction of sqrt(W*H) of the decoded mask. Positive ->
    // dilate (grow) foreground; negative -> erode (shrink) foreground; zero
    // disables. Implemented via separable Felzenszwalb-Huttenlocher squared
    // Euclidean distance transform on CPU (O(N) per row+col, exact).
    float mask_boundary_offset = 0.0f;
};


// ===========================================================================
// Decoded batch
// ===========================================================================

// One run of the face axis at one size: faces [k0, k1) of every input image in
// a batch, rendered at width x height. A camera whose faces differ in size
// takes one render pass per entry (data/CameraMath.h, FaceFit).
struct WarpFacePass {
    int32_t k0 = 0, k1 = 0, width = 0, height = 0;
};


// Element-type tag, matching the engine's set_training_data inputs.
//   RGB:    UINT8 (sRGB 0..255), UINT16 (0..65535), or FLOAT32 (0..1).
//   MASK:   UINT8 (treated as bool; nonzero -> valid pixel).
//   DEPTH:  UINT16 (raw counts, no scaling) or FLOAT32.
//   NORMAL: UINT8 (x/127.5 - 1) or FLOAT32 (already in [-1, 1]).
enum class PixelDType : uint8_t { UINT8 = 1, UINT16 = 2, FLOAT32 = 4 };

inline uint32_t pixel_dtype_size(PixelDType t) { return (uint32_t)t; }


// A single ready-to-consume training batch.
//
// `*_buffer` are host buffers (std::vector for ownership + automatic growth);
// `*_view` is a non-owning TorchTensorView pointing into the corresponding
// buffer in the [B, H, W, C] (or [B, H, W, 1]) channel-last layout expected
// by the engine.
//
// Empty optional modalities (no mask / no depth / no normal) leave the
// corresponding view at all-zero (data_ptr == 0), which the engine interprets
// as "not provided".
struct DecodedBatch {
    // Per-batch metadata, POST-split: `width`, `height`, `num`, `model`,
    // `viewmats`, `intrins`, `dist_coeffs` are what set_camera_params
    // consumes. The INPUT (unwarped) shape the GT warp reads is tracked below.
    int32_t                width  = 0;     // post-split W (= input W when K=1)
    int32_t                height = 0;     // post-split H
    int32_t                num    = 0;     // post-split B (= input B * K)
    CameraModelType        model  = (CameraModelType)-1;   // PINHOLE when K>1
    CameraDistortionType   distortion = CameraDistortionType::None;  // None when K>1

    // Selected camera indices, in input batch order (length = input B).
    std::vector<int32_t>   indices;

    // Camera parameters at POST-split resolution, host-contiguous.
    // Length = 16 / 4 / 8 * num (= B*K).
    std::vector<float>     viewmats;
    std::vector<float>     intrins;
    std::vector<float>     dist_coeffs;

    // Per-camera post-split offset for bilagrid / per-image lookup. Length
    // = input B. bilagrid_cam_indices for the batch is built from this:
    // [post_offsets[j] + k for j in 0..B-1 for k in 0..K-1].
    std::vector<int32_t>   post_offsets;

    // Pixel buffers + their dtypes. `rgb_dtype` is the original on-disk dtype
    // (so 16-bit PNGs survive into the upload as uint16); the engine's GPU
    // converter handles the cast.
    std::vector<uint8_t>   rgb_buffer;
    PixelDType             rgb_dtype = PixelDType::UINT8;

    std::vector<uint8_t>   mask_buffer;    // uint8, empty if disabled / none
    std::vector<uint8_t>   depth_buffer;
    PixelDType             depth_dtype = PixelDType::UINT16;

    std::vector<uint8_t>   normal_buffer;
    PixelDType             normal_dtype = PixelDType::UINT8;

    // Per-modality (H, W). Mask / depth / normal images are allowed to be a
    // different resolution from the rendered RGB output — the engine's
    // fused per-pixel loss kernel bilinearly samples from these buffers,
    // so the data manager just exposes the raw on-disk shape and leaves the
    // resize to the loss path. Zero when the modality is disabled / absent.
    int32_t                mask_height   = 0;
    int32_t                mask_width    = 0;
    int32_t                depth_height  = 0;
    int32_t                depth_width   = 0;
    int32_t                normal_height = 0;
    int32_t                normal_width  = 0;

    // ---- Warp metadata (only meaningful when K > 1) -----------------------
    // `input_*` is the unwarped GT shape the warp reads the buffers at; `K`
    // the split factor (1 = pass-through); `input_model` picks the kernel.
    int32_t                input_width   = 0;
    int32_t                input_height  = 0;
    int32_t                input_num     = 0;   // = num / K
    int32_t                K             = 1;
    CameraModelType        input_model   = (CameraModelType)-1;
    CameraDistortionType   input_distortion = CameraDistortionType::None;

    // Each post camera's frame in its input camera's coordinates, rows (ax,
    // ay, az): the face the warp samples for. Length 9 * num; empty at K == 1.
    std::vector<float>     face_axes;

    // The face axis in runs of equal size, in order; one entry unless the
    // camera was split per face. `width` / `height` above are the first run's.
    std::vector<WarpFacePass> face_passes;

    // Per-INPUT intrins / dist_coeffs (length input_num). Populated when
    // K > 1 and the wide warp kernel (fisheye/equisolid) needs them. The
    // equirectangular path ignores these.
    std::vector<float>     input_intrins;       // 4 * input_num
    std::vector<float>     input_dist_coeffs;   // 8 * input_num

    // Per-INPUT source camera for the images that must be re-distorted.
    // Empty when the dataset has none; see ParsedDataset::redistort.
    std::vector<int32_t>   input_source_models;   // input_num
    std::vector<float>     input_source_params;   // 16 * input_num

    TorchTensorView        input_intrins_view{0, 0, {}};
    TorchTensorView        input_dist_coeffs_view{0, 0, {}};
    TorchTensorView        input_source_models_view{0, 0, {}};
    TorchTensorView        input_source_params_view{0, 0, {}};
    TorchTensorView        face_axes_view{0, 0, {}};

    // Engine-facing TorchTensorViews. Lazily filled by `build_views()` once
    // the buffers are populated. The Engine consumes these directly via
    // set_camera_params / set_training_data.
    TorchTensorView        viewmats_view{0, 0, {}};
    TorchTensorView        intrins_view{0, 0, {}};
    TorchTensorView        dist_coeffs_view{0, 0, {}};
    TorchTensorView        rgb_view{0, 0, {}};
    TorchTensorView        mask_view{0, 0, {}};
    TorchTensorView        depth_view{0, 0, {}};
    TorchTensorView        normal_view{0, 0, {}};

    // Populate the *_view fields from the *_buffer fields. Idempotent.
    void build_views();
};


// A single optimizer step's worth of training data: one or more homogeneous
// sub-batches. Each `subs[i]` is internally uniform in (W, H, model). Steps
// with a single sub-batch are the common case (a single-resolution dataset,
// or a full B-image chunk of one resolution group) and are dispatched to the
// engine exactly as before. Steps with several sub-batches arise from
// cross-group remainder packing on datasets with many small resolution groups
// (e.g. an internet image collection); the engine consumes them via its
// heterogeneous grad-accumulation path (one optimizer step accumulating over
// sub-batches of differing resolution). Cross-group packing is restricted to
// non-warp (K == 1) groups, so a multi-sub-batch step never mixes warp faces.
struct TrainStep {
    std::vector<std::shared_ptr<DecodedBatch>> subs;
};


// ===========================================================================
// Internal: per-modality prefetch worker pool (DISK mode)
//
// Forward-declared so DataManager can hold a unique_ptr<Impl>.
// ===========================================================================
class DataManagerImpl;


// ===========================================================================
// DataManager
// ===========================================================================
class DataManager {
public:

    // The constructor takes the parsed-out dataset arrays directly. The
    // dataparser port populates these — DataManager doesn't know anything
    // about transforms.json / COLMAP.
    //
    // All filename vectors are sized to the total dataset; `mask_filenames`
    // / `depth_filenames` / `normal_filenames` may be empty to indicate
    // "no modality of that kind on disk". An individual entry that is the
    // empty string indicates "this image has no mask/depth/normal" and the
    // corresponding batch row is left zero.
    //
    // `train_indices` / `val_indices` partition the dataset. They are NOT
    // required to be disjoint; that's the caller's concern.
    DataManager(
        DataManagerConfig          config,
        // Per-INPUT-camera (length N) ---------------------------------------
        std::vector<int32_t>       camera_models,
        std::vector<int32_t>       camera_distortions,
        std::vector<std::string>   image_filenames,
        std::vector<std::string>   mask_filenames,
        std::vector<std::string>   depth_filenames,
        std::vector<std::string>   normal_filenames,
        std::vector<int32_t>       widths,
        std::vector<int32_t>       heights,
        // Per-INPUT split factor (the faces bake_post_split gave camera i)
        // and its exclusive-prefix-sum offset into the POST-split arrays.
        // Pass empty vectors to disable the warp path.
        std::vector<int32_t>       K_per_camera,        // length N
        std::vector<int32_t>       post_offsets,        // length N
        // Per-POST-split-camera (length N_post = sum(K_per_camera)) -----------
        // N_post == N and the per-input params when the warp path is off;
        // otherwise bake_post_split (PostSplit.cpp) expanded them per face.
        std::vector<float>         viewmats,            // [N_post, 4, 4]
        std::vector<float>         intrins,             // [N_post, 4]
        std::vector<float>         dist_coeffs,         // [N_post, 8]
        // Face size and frame per POST camera; the K faces of one input share
        // a size. Empty when the warp path is inactive.
        std::vector<int32_t>       post_widths,         // [N_post]
        std::vector<int32_t>       post_heights,        // [N_post]
        std::vector<float>         face_axes,           // [N_post, 3, 3]
        // Per-INPUT intrins/dist_coeffs (length N). Needed by the wide warp
        // kernel (fisheye/equisolid projection). Pass empty vectors when
        // no fisheye-warp camera is in the dataset -- the kernel only reads
        // them on the fisheye path.
        std::vector<float>         input_intrins,       // [N, 4]
        std::vector<float>         input_dist_coeffs,   // [N, 8]
        // Per-INPUT source camera for images that must be re-distorted;
        // empty when the dataset has none. See PostSplitCameras.
        std::vector<int32_t>       redistort_models,    // [N]
        std::vector<float>         redistort_params,    // [N, 16]
        std::vector<int32_t>       train_indices,
        std::vector<int32_t>       val_indices);

    ~DataManager();

    DataManager(const DataManager&)            = delete;
    DataManager& operator=(const DataManager&) = delete;

    // ---- Batch fetch ------------------------------------------------------

    // Block until the next training step is ready and return a reference to
    // it. The step holds one or more homogeneous sub-batches (see TrainStep).
    // The returned reference — and the DecodedBatch host buffers it points
    // into — stay valid only until the next call to `next_train_step()`.
    const TrainStep& next_train_step();

    // Block until the next training batch is ready and return a reference to
    // it. The returned reference is valid only until the next call to
    // `next_train_batch()`. Convenience wrapper over `next_train_step()` that
    // returns the first sub-batch; only meaningful when the active schedule is
    // known to produce single-sub-batch steps.
    const DecodedBatch& next_train_batch();

    // Same for eval. Returns nullptr if no validation indices were configured.
    const DecodedBatch* next_val_batch();

    // Decode ONE input image, by dataset index, into `out`: a batch of size 1
    // carrying the mask, warp and resize a training batch over that image
    // would carry. Synchronous, and it touches neither the sampling cursors
    // nor the prefetch queues, so it may run between training steps. RGB and
    // mask only -- its caller is the GUI's ground-truth-vs-render view.
    void fetch_one(int32_t index, DecodedBatch& out);

    // ---- Decode faults ----------------------------------------------------

    // Non-empty while a decode worker is parked on a file it could not read
    // (moved, renamed, deleted, permissions). The pipeline stays paused, and
    // the batch in flight is untouched, until this is answered.
    std::string data_error() const;

    // true re-runs the parked decode -- the run continues where it stopped
    // once the file is back. false abandons the pipeline: the ready queues
    // close, so the next fetch throws instead of blocking forever.
    void resolve_data_error(bool retry);

    // ---- Stats ------------------------------------------------------------

    int64_t num_train()     const;
    int64_t num_val()       const;
    bool    has_val()       const;
    CacheMode cache_mode()  const;

    // Whether each modality is present in the dataset AND enabled.
    bool has_masks()        const;
    bool has_depths()       const;
    bool has_normals()      const;

    // Largest possible POST-split batch size (B_post) for a single training
    // step, computed as train_batch_size (clamped to train-set size) times
    // the max K_per_camera over train_indices. 1 means every train step
    // produces exactly one POST-split camera (so split_batch sub-batching
    // would be a no-op). Used by the engine to resolve the case where the
    // user has both `split_batch` and `use_fused_proj_bwd_optim` enabled.
    int64_t max_input_batch_size() const;

    // Most render passes one input image costs: 1 unless faces differ in size.
    int max_face_passes() const;

private:
    std::unique_ptr<DataManagerImpl> _impl;
};

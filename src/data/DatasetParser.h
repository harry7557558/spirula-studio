#pragma once

// DatasetParser -- dataset readers + camera bakes. No external deps: JSON via
// data/Json.h, PLY reader in NerfstudioParser.cpp, images via stb in the
// engine's DataManager.
//
// parse_colmap_dataset / parse_nerfstudio_dataset / parse_metashape_dataset
// read one format each; parse_dataset identifies the format from its marker
// files. All produce a ParsedDataset of PER-INPUT cameras in the raw
// train_frame="points" frame. bake_post_split() (PostSplit.cpp) then expands
// that to the POST-split arrays engine_setup_data_manager consumes: identity
// at K=1, or the pinhole faces camhost::plan_split_faces cuts a wide camera into.

#include <array>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <vector>


// ===========================================================================
// Raw COLMAP records (mirrors colmap_utils.py Camera / Image / Point3D)
// ===========================================================================

struct ColmapCamera {
    int32_t              camera_id = -1;
    std::string          model;       // e.g. "OPENCV", "OPENCV_FISHEYE"
    uint64_t             width  = 0;
    uint64_t             height = 0;
    std::vector<double>  params;      // model-specific, COLMAP order
};

// What a COLMAP camera record means to the renderer: which model, which
// distortion tier, and its coefficients. Exposed for a caller that only wants
// to DRAW the camera -- the live SfM preview, whose model is not on disk yet --
// so it needs neither the images nor the re-distort fit the parser runs.
struct PreviewIntrins {
    float fx = 0, fy = 0, cx = 0, cy = 0;
    int32_t model = 0;         // CameraModelType
    int32_t distortion = 0;    // CameraDistortionType
    // kCameraDistortionParams wide; the literal is here because core/Common.cuh
    // defines the same constant for the device side and the two headers cannot
    // both be included in one translation unit.
    std::array<float, 8> dist{};
};

// False for a model id this reader does not know, a parameter count that does
// not match it, or a source model no tier represents exactly -- the last of
// which the parser answers by fitting and resampling, which is more than a
// frustum needs.
bool colmap_preview_intrins(int model_id, int width, int height,
                            const std::vector<double>& params,
                            PreviewIntrins& out);

struct ColmapImage {
    int32_t                image_id  = -1;
    std::array<double, 4>  qvec{};    // (w, x, y, z), world->camera
    std::array<double, 3>  tvec{};
    int32_t                camera_id = -1;
    std::string            name;      // path relative to the image dir
};

struct ColmapPoints3D {
    std::vector<float>    xyz;        // [N, 3] flat
    std::vector<uint8_t>  rgb;        // [N, 3] flat
    int64_t num() const { return (int64_t)xyz.size() / 3; }
};

// Low-level readers. Throw std::runtime_error on missing/corrupt files.
std::map<int32_t, ColmapCamera> read_cameras_binary(const std::string& recon_dir);
std::map<int32_t, ColmapImage>  read_images_binary(const std::string& recon_dir);
ColmapPoints3D                  read_points3D_binary(const std::string& recon_dir);

// Text-format equivalents (cameras.txt / images.txt / points3D.txt).
std::map<int32_t, ColmapCamera> read_cameras_text(const std::string& recon_dir);
std::map<int32_t, ColmapImage>  read_images_text(const std::string& recon_dir);
ColmapPoints3D                  read_points3D_text(const std::string& recon_dir);

// PLY point-cloud reader (ascii + binary_little_endian; x/y/z of any float
// or double type, red/green/blue uchar or float). NerfstudioParser.cpp.
ColmapPoints3D read_ply_points(const std::string& path);


// The exotic camera a fitted camera stands in for, so the re-distort kernel
// can evaluate the true source projection rather than an approximation of it.
// `source_model` and `params` are as data/SourceCamera.h defines them; ignored
// when `source_model < 0`.
struct RedistortSource {
    int32_t source_model = -1;
    float   params[16]{};
    float   fit_max_px = 0.0f;   // reported near-minimax error of the fit
};


// ===========================================================================
// Parser config + baked dataset
// ===========================================================================

struct DatasetParserConfig {
    // COLMAP reconstruction dir relative to the dataset dir. Empty =
    // auto-detect over {sparse/0, colmap/sparse/0, sparse, colmap, .}.
    std::string recon_dir;

    std::string image_dir  = "images";
    std::string mask_dir   = "masks";
    std::string depth_dir  = "depths";
    std::string normal_dir = "normals";

    // When false, frames whose image file is missing on disk are kept (with
    // their computed path) instead of being skipped/rejected. The trainer
    // needs the pixels (default true); the standalone viewer only needs the
    // camera poses, so it parses cameras from a dataset whose images were not
    // provided (e.g. dropping just the sparse/ folder).
    bool require_image_files = true;

    // Fraction of training images held out for validation (linspace-spread,
    // matching get_train_eval_split_fraction). 0 = no validation set.
    float validation_fraction = 0.0f;

    // Train/eval split:
    //   "all"      -> every image trains (default).
    //   "fraction" -> linspace-spread ceil(N * train_split_fraction) train.
    //   "interval" -> index % eval_interval == 0 is eval, rest train.
    //   "filename" -> basename contains "train" / "eval".
    std::string eval_mode = "all";
    float       train_split_fraction = 0.9f;
    int         eval_interval = 8;

    // Which side of that split to return. "train" is what training wants;
    // "eval" returns the complement. Everything derived from the camera set
    // -- train_frame_scale, train_to_normalized, the outlier filter -- is
    // computed over ALL frames BEFORE the split, so the two parses agree
    // frame-for-frame. "all" makes both sides the full set. An empty eval
    // split is legal (and the caller's cue to skip eval); an empty train
    // split is an error.
    std::string split = "train";

    // Reject frames whose camera position is more than this many MADs from
    // the geometric median of all camera positions. inf = off (default).
    float outlier_threshold = std::numeric_limits<float>::infinity();

    // Pixel size of an image file (data/ImageProbe.h). Set: every camera trains
    // at its own image's resolution. Null: a caller with no decoders -- the
    // WebAssembly viewer, given a dataset's cameras but never its pixels.
    bool (*probe_image_size)(const char* path, int* w, int* h) = nullptr;

    // Divide the training resolution by this factor, on top of that fit. 0 or
    // 1 trains at the images' own size.
    float       train_resolution_divisor = 0.0f;
    std::string downscale_rounding_mode = "floor";   // floor | ceil | round

    // Metashape inputs (parse_metashape_dataset). Relative paths resolve
    // against the dataset dir; empty = auto-detect a unique candidate in the
    // dataset dir (.psx is optional -- used only to disambiguate camera ->
    // image filename matching).
    std::string metashape_xml;
    std::string metashape_ply;
    std::string metashape_psx;

    // Which <component> group to use when a Metashape export contains several.
    // -1 (default) keeps the historical behavior: train on the largest group.
    // >= 0 selects that component-group index (document order); the viewer uses
    // this to let the user pick a component.
    int metashape_component = -1;
};

// Everything main.cpp needs, PER-INPUT camera (length N), sorted by image
// filename.
struct ParsedDataset {
    int64_t num_cameras = 0;

    // CameraModelType / CameraDistortionType as int, per input camera. The
    // distortion tier is the cheapest one that represents the source camera
    // exactly; `redistort` says when that required resampling the images.
    std::vector<int32_t>     camera_models;
    std::vector<int32_t>     camera_distortions;

    // Absolute file paths. mask/depth/normal are all-N with "" for images
    // that have no such auxiliary file, or empty vectors when no image has
    // one (the DataManager convention).
    std::vector<std::string> image_filenames;
    std::vector<std::string> mask_filenames;
    std::vector<std::string> depth_filenames;
    std::vector<std::string> normal_filenames;

    std::vector<int32_t>     widths;
    std::vector<int32_t>     heights;

    // Camera-to-world, [N, 3, 4] flat, OpenGL/nerfstudio convention.
    std::vector<float>       c2w;

    std::vector<float>       intrins;      // [N, 4]  (fx, fy, cx, cy)
    // [N, 8]; slot meaning is per-tier, see core/CameraModel.h
    std::vector<float>       dist_coeffs;

    // Cameras whose source model no tier represents exactly (COLMAP FOV /
    // DIVISION / EUCM / RAD_TAN_THIN_PRISM_FISHEYE, Metashape affinity-skew).
    // Empty when every camera mapped exactly. Indexed like the arrays above;
    // `source_model < 0` marks a camera that needs no resampling.
    std::vector<RedistortSource> redistort;

    // Per-INPUT train/val partition (validation_fraction).
    std::vector<int32_t>     train_indices;
    std::vector<int32_t>     val_indices;

    // Seed point cloud in the training frame.
    ColmapPoints3D           points;

    // 1 / scale_factor of the would-be normalized frame. Computed over ALL
    // frames, before the eval_mode subset is dropped.
    float                    train_frame_scale = 1.0f;

    // Similarity mapping a normalized-frame point into the training frame.
    // The name is historical; the stored value is inv(T_n_from_train). The
    // viewer client navigates in the normalized frame and remaps its c2w
    // through this before rendering (RenderWorker.cpp). Row-major 4x4;
    // identity when train_frame_scale == 1.
    std::array<float, 16>    train_to_normalized{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
};

ParsedDataset parse_colmap_dataset(const std::string& dataset_dir,
                                   const DatasetParserConfig& cfg);
ParsedDataset parse_nerfstudio_dataset(const std::string& dataset_dir,
                                       const DatasetParserConfig& cfg);
ParsedDataset parse_metashape_dataset(const std::string& dataset_dir,
                                      const DatasetParserConfig& cfg);

// Nerfstudio back-end over an already-built transforms.json-shaped meta
// (NerfstudioParser.cpp; used by the Metashape front-end).
struct JsonValue;
ParsedDataset parse_nerfstudio_meta(const JsonValue& meta,
                                    const std::string& dataset_dir,
                                    const DatasetParserConfig& cfg);

// Auto-detect (format = "") or dispatch ("colmap" / "nerfstudio" /
// "metashape").
ParsedDataset parse_dataset(const std::string& dataset_dir,
                            const DatasetParserConfig& cfg,
                            const std::string& format);


// ===========================================================================
// POST-split camera bake
// ===========================================================================

// Per-input split factor K: the faces camhost::plan_split_faces returns for a
// FISHEYE / EQUISOLID camera when warp_to_pinhole, and for an EQUIRECTANGULAR
// one when warp_spherical_to_pinhole (else K=1 direct-equirect); 1 otherwise.

// One size for every face (one render pass), or each face cropped to its own
// lens (one pass per size, for VRAM). data/CameraMath.h, FaceFit.
enum class WarpFaceFit { Uniform = 0, PerFace };

struct PostSplitCameras {
    int64_t n_post = 0;
    bool any_warp        = false;   // any K > 1
    bool any_fov_mask    = false;   // a split face reaches past what its lens
                                    // holds (DataManager synthesizes FOV masks)
    bool direct_equirect = false;   // un-split equirect present

    std::vector<int32_t> K_per_camera;   // [N]  (empty vectors passed to the
    std::vector<int32_t> post_offsets;   // [N]   engine when !any_warp)

    // Engine-convention world-to-camera (R/T inverse + Y/Z flip pre-baked),
    // POST-split.
    std::vector<float>   viewmats;       // [N_post, 4, 4]
    std::vector<float>   intrins;        // [N_post, 4]
    std::vector<float>   dist_coeffs;    // [N_post, 8]
    std::vector<int32_t> post_distortions;   // [N_post] CameraDistortionType

    // Viewer (engine_viewer_init) arrays, POST-split: camera-to-world in the
    // y/z-flipped form the blit kernel expects, plus per-post W/H/model.
    std::vector<float>   c2w_flip;       // [N_post, 3, 4]
    std::vector<int32_t> post_widths;    // [N_post]
    std::vector<int32_t> post_heights;   // [N_post]
    std::vector<int32_t> post_models;    // [N_post] CameraModelType (faces = PINHOLE)

    // Each post camera's frame in its INPUT camera's coordinates, rows (ax,
    // ay, az); identity at K == 1. The GT warp samples `az + u*ax + v*ay`.
    std::vector<float>   face_axes;      // [N_post, 3, 3]
    // Which entry of camhost::fisheye_face_axes / equirect_face_axes that
    // frame is, -1 at K == 1; the GUI's unfolded-cube layout reads it.
    std::vector<int32_t> post_faces;     // [N_post]

    // Per-INPUT copies for the wide-warp kernel; empty when !any_warp.
    std::vector<float>   input_intrins;      // [N, 4]
    std::vector<float>   input_dist_coeffs;  // [N, 8]
    std::vector<int32_t> input_distortions;  // [N] CameraDistortionType

    // Flattened ParsedDataset::redistort, per INPUT camera; empty when none
    // needs resampling, -1 marks one that does not. Any of these sets
    // any_warp: the re-distort shares the warp path's staging even at K == 1.
    std::vector<int32_t> redistort_models;   // [N]
    std::vector<float>   redistort_params;   // [N, 16]
};

PostSplitCameras bake_post_split(const ParsedDataset& ds,
                                 bool warp_to_pinhole,
                                 bool warp_spherical_to_pinhole,
                                 WarpFaceFit fit = WarpFaceFit::Uniform,
                                 bool back_face = false);


// ===========================================================================
// Shared parser internals (DatasetCommon.cpp) -- used by both format
// parsers; exposed here so they stay in one place.
// ===========================================================================
namespace dsparse {

// Normalized-frame scale factor over c2w [N,3,4] (orient="up",
// center="poses", auto-scale). Only the scalar matters for
// train_frame="points". Returns 1/max_abs.
double compute_normalized_scale_factor(const std::vector<float>& c2w, int64_t n);

// Full normalized-frame similarity: writes the row-major 4x4
// T_n_from_camera = scale * [R_align | -R_align @ center], and returns
// scale_factor. The viewer remap transform is
// inv(T_n_from_camera @ applied_transform).
double compute_normalized_transform(const std::vector<float>& c2w, int64_t n,
                                    double T_out[16]);

// inv([A|b; 0 1]) for a general invertible 3x3 A (row-major 4x4 in/out).
void invert_affine4x4(const double in[16], double out[16]);

// eval_mode subset over N sorted frames, honouring cfg.split; identity for
// "all". `names` are image filenames (used by eval_mode="filename").
std::vector<int64_t> train_subset(int64_t n, const std::vector<std::string>& names,
                                  const DatasetParserConfig& cfg);

// validation_fraction partition of 0..N-1 into ds.train_indices/val_indices.
void assign_val_split(ParsedDataset& ds, float validation_fraction);

// Auxiliary mask/depth/normal discovery by filename convention.
std::string find_aux_file(const std::string& aux_dir, const std::string& rel_name,
                          const char* suffix_tag);

// Outlier-frame mask via geometric median of camera positions. Returns
// keep-flags, all-true when threshold is inf. positions = [N, 3].
std::vector<char> outlier_keep_mask(const std::vector<double>& positions,
                                    int64_t n, float threshold);

// The resolution one camera trains at: its image file's size when cfg probes
// for one, divided by train_resolution_divisor. W/H and the intrinsics come in
// as the reconstruction's and leave scaled to that; `src` (may be null) too.
void fit_camera_resolution(const DatasetParserConfig& cfg,
                           const std::string& image_path,
                           double& W, double& H,
                           double& fx, double& fy, double& cx, double& cy,
                           RedistortSource* src);

}  // namespace dsparse

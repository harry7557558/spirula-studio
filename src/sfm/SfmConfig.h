// The one option surface of the SfM pipeline: an aggregate of the stage option
// structs, plus a descriptor table that every consumer reads instead of writing
// its own list.
//
// Why a table. The pipeline has ~90 knobs spread over eight option structs, and
// three consumers need the same view of them: the CLI parser, `--help`, and
// (port plan phase 5) the GUI's options editor. Written by hand that is three
// lists that drift; here the table is the source of truth and each consumer is
// one macro expansion over it -- the same shape as the trainer's generated
// SS_CONFIG_FIELDS / ConfigUI pair.
//
// What is NOT in the table: flags that do not name one scalar field.
// `--camera-model PREFIX=MODEL` and `--focal PREFIX=F` also feed a per-group
// override list, `--no-manage` moves four fields at once, `--check` / `--audit`
// pick what the command does rather than how, and `-o/--output` is positional
// in spirit. Those stay hand-parsed in the CLI, which tries them *before* the
// table, so a hand-parsed name always wins.
//
// Layering: the sub-option structs are untouched, and the pipeline-level knobs
// here are the ones that span stages (`--max-error` is one tolerance measured
// in one frame, D47, but two struct fields; `--device`, `--quiet` and the
// camera settings likewise). finalize() is what fans them out; nothing else
// may, or the CLI and the GUI would have to agree on the fan-out separately.
#pragma once

#include <cmath>
#include <cstdio>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

#include "sfm/core/CameraSetup.h"
#include "sfm/feature/Matcher.h"
#include "sfm/feature/Extractor.h"
#include "sfm/feature/LearnedMatcher.h"
#include "sfm/feature/PairSelection.h"
#include "sfm/feature/Sift.h"
#include "sfm/geometry/TwoView.h"
#include "sfm/map/Bottomup.h"
#include "sfm/map/Mapper.h"
#include "sfm/map/Merge.h"

namespace sfm {

// Which subcommands accept a flag. A name may be reused across commands as long
// as the masks are disjoint -- `--max-error` is the verification/mapping
// tolerance for auto/match/map and the *alignment* tolerance for merge, which
// are genuinely different quantities with the same spelling.
enum CmdMask : uint32_t {
    CMD_AUTO    = 1u << 0,
    CMD_EXTRACT = 1u << 1,
    CMD_MATCH   = 1u << 2,
    CMD_MAP     = 1u << 3,
    CMD_MERGE   = 1u << 4,
    CMD_ALL     = CMD_AUTO | CMD_EXTRACT | CMD_MATCH | CMD_MAP | CMD_MERGE,
};

// Basic is what a first-time user has to think about and what the GUI shows
// unfolded; Advanced is everything else; Alias is a second spelling of a field
// already listed (parsed, but not printed twice or offered to the GUI).
enum class Tier { Basic, Advanced, Alias };

// The aggregate. Sub-option structs are held unchanged so that library callers
// -- the tests, the mapper's own defaults -- are unaffected by anything here.
struct SfmConfig {
    // ---- pipeline-level knobs ----
    // Presets (auto only). Applied before the table's overrides, so an explicit
    // flag always wins; see applyPresets().
    std::string quality = "high";
    std::string data_type = "individual";
    // "auto" resolves at run time: prefilter at 100 images or more, sequential
    // for video below that, exhaustive otherwise. `match` reads it as
    // exhaustive (it has not counted the images).
    std::string pairs = "auto";
    int overlap = 10;
    // Sequential pairing is a chain: image i with the next `overlap`. A capture
    // that walks around a subject and comes back has no link across the seam,
    // so one weak step anywhere breaks the reconstruction into pieces (a
    // 262-frame walk around a plaza came out as four models). This adds the
    // pair-selection shortlist -- the same content-based scoring `--pairs
    // prefilter` uses -- on top of the temporal window, which is what COLMAP's
    // SequentialMatching.loop_detection does with a vocabulary tree.
    // Sequential only; the other modes already consider every pair.
    bool loop_closure = true;

    // The one geometric tolerance, in extraction pixels (D47): the two-view
    // verifier's inlier radius and the mapper's reprojection cap are the same
    // quantity in the same frame. finalize() writes both.
    double max_error = 3.0;
    // 0 means "whatever the selected frontend wants" -- 3200 for SIFT, 1600
    // for a learned one, mirroring COLMAP's EffMaxImageSize(). Resolved in
    // finalize(), so a command that applies no presets (extract, match) still
    // gets the right one instead of running ALIKED at SIFT's resolution and
    // spending four times the VRAM on it.
    int max_image_size = 0;
    std::string mask_dir;
    // Swap keep and ignore in every mask, for the exporters that paint the
    // region to REMOVE (sfm/core/Mask.h).
    bool flip_mask = false;

    // The input files' colour space. Pixels convert to sRGB on decode, which
    // is what the detectors and the AI models were trained on.
    std::string image_gamut = "Rec.709";
    bool image_is_linear = false;
    // "srgb" leaves point colours there; "image" writes them back in the
    // photographs' space (trainer: convert_initial_point_cloud_color off).
    std::string point_color_space = "srgb";

    // Camera setup. The string forms are what the table and the GUI see; the
    // parsed forms live in `camera` after finalize(). `--camera-model` and
    // `--focal` also accept PREFIX=VALUE, which the CLI routes straight into
    // camera.overrides and never through these.
    std::string camera_mode = "folder";
    std::string camera_model = "opencv";
    double focal = 0;
    // Starting distortion coefficients, "k1,k2,..." in the camera model's BA
    // order. Empty starts at zero, as COLMAP does.
    std::string distortion;
    // Whether camera_mode is pinned: set by an explicit --camera-mode, and by
    // the internet preset, which chooses per-image cameras deliberately (D20).
    // Unpinned, buildCameras may still switch a Folder default to Image for a
    // set that is plainly a photo collection (D48).
    bool camera_mode_pinned = false;

    // Stage switches that are not a field of any stage's options.
    bool verify = true;                 // match: geometric verification
    bool compact_unused_features = true;   // compact stored-match feature rows
    bool final_principal_point = true;  // one PP-free global BA at the end (D51)
    bool final_extra_params = true;     // ... releasing the distortion too (D72)
    // One more, after those, with every image on its own intrinsics (D73).
    bool final_per_image_intrinsics = false;
    // Write the finished model in an upright, centred, unit-sized frame rather
    // than in whatever gauge the seed pair left it in (map/Orient.h).
    bool orient = true;
    bool merge_ba = true;               // merge: bundle-adjust across the seams
    bool in_place = false;              // merge: write back over the input

    // Inputs the `map` subcommand names by flag (auto takes them positionally).
    std::string image_dir;
    std::string feature_dir;
    std::string resume;
    bool check = false;

    // Runtime.
    int threads = 0;           // host worker pools; 0 = hardware_concurrency
    int decode_threads = 0;    // image decode pool; 0 = hardware_concurrency
    int decode_budget_mb = 0;  // 0 = ImageLoadOptions default
    int device = -1;
    bool quiet = false;

    // Which frontend runs. "sift" is the GPU SIFT that has always been here;
    // the aliked-* values are the learned one (src/aliked/), which needs the
    // inference layer compiled in. The pair is deliberately two flags and not
    // one: ALIKED descriptors can be matched brute-force, and LightGlue is a
    // matcher for them rather than a different extractor.
    std::string features = "sift";
    std::string matcher = "bruteforce";

    // ---- the stage option structs, unchanged ----
    SiftOptions sift;
    AlikedOptions aliked;
    LightGlueOptions lightglue;
    MatchOptions match;
    PairSelectionOptions prefilter;
    TwoViewOptions twoview;
    CameraSetupOptions camera;
    MapperOptions mapper;
    ManagerOptions manager;
    MergeOptions merge;
    BottomUpOptions bup;
    // The schedule both mappers run once they have models: merge levels with
    // growth and a joint solve between them, then the finishing passes
    // (sfm/map/Assemble.h). Its flags are spelled --bup-* for history; they are
    // not the bottom-up mapper's alone.
    AssembleOptions assemble;
    // How the mapper is scheduled: "flat" is one incremental reconstruction of
    // the whole capture, "bottom-up" reconstructs small atoms of the view graph
    // and merges them upwards (D57). "hierarchical" is accepted as the old name
    // for "bottom-up".
    //
    // Flat whatever the capture is, and there is deliberately no size-based
    // switch: bottom-up has not been measured on enough captures to pick it for
    // a user, and a schedule that changes under you at some image count makes
    // every comparison between two runs a question about which one ran.
    std::string mapper_mode = "flat";

    // Fan the pipeline knobs out into the structs above and validate what the
    // table's ranges cannot. Returns an empty string, or the error to print.
    // Call once, after parsing and after the presets.
    std::string finalize(uint32_t cmd);

    // What --pairs names, with "auto" resolving to exhaustive. `auto` alone
    // additionally switches to pair selection above 100 images, which it can
    // only decide once extraction has counted them -- see cmdAuto.
    PairMode pairMode() const;
};

// ---------------------------------------------------------------------------
// The descriptor table
// ---------------------------------------------------------------------------
// F(member, name, cmds, tier, group, lo, hi, choices, help)
//
//   member   path inside SfmConfig; its declared type is the field's type
//            (bool switch, integer, real, string), deduced at each expansion
//   name     the CLI flag without "--"; the GUI's id; the key applyPresets logs
//   cmds     CmdMask bits. A field an editor should offer must include
//            CMD_AUTO, since `auto` is the whole pipeline
//   tier     Tier::Basic is the short list; Tier::Alias is a second spelling
//   group    display group, in the order the rows appear here
//   lo, hi   accepted range; lo >= hi means unbounded
//   choices  "a|b|c" for string fields, "" otherwise
//   help     the NAME of a message in i18n/catalog/SfmFields.h -- a token, not
//            a string: `quality` resolves to `msg::sfmfield::quality_help`.
//            One sentence, no trailing default (the printer appends it). It
//            lives there rather than here so that `spirula sfm --help` can be
//            read in the language the rest of the program is in, and a row
//            added with no entry there is a compile error naming the flag.
//
// A bool field is a switch: `--name` sets it true and `--no-name` false, so a
// field defaulting to true reads as `--no-name` on the command line and is
// printed that way. Keep the name positive.
#define SFM_CONFIG_FIELDS(F)                                                                       \
    /* ---- pipeline ---- */                                                                       \
    F(quality, "quality", CMD_AUTO, Tier::Basic, "pipeline", 0, 0, "low|medium|high|extreme",      \
      quality)                                                                                     \
    F(data_type, "data-type", CMD_AUTO, Tier::Basic, "pipeline", 0, 0,                             \
      "individual|video|internet", data_type)                                                      \
    F(pairs, "pairs", CMD_AUTO | CMD_MATCH, Tier::Basic, "pipeline", 0, 0,                         \
      "auto|exhaustive|sequential|prefilter", pairs)                                               \
    F(overlap, "overlap", CMD_AUTO | CMD_MATCH, Tier::Advanced, "pipeline", 1, 1000000, "",        \
      overlap)                                                                                     \
    F(loop_closure, "loop-closure", CMD_AUTO | CMD_MATCH, Tier::Advanced, "pipeline", 0, 0, "",    \
      loop_closure)                                                                                \
    F(max_error, "max-error", CMD_AUTO | CMD_MATCH | CMD_MAP, Tier::Advanced, "pipeline", 0.1,     \
      100, "", max_error)                                                                          \
    F(max_image_size, "max-image-size", CMD_AUTO | CMD_EXTRACT, Tier::Advanced, "pipeline", 0,     \
      20000, "", max_image_size)                                                                   \
    F(mask_dir, "masks", CMD_AUTO | CMD_EXTRACT, Tier::Basic, "pipeline", 0, 0, "", masks)         \
    F(mask_dir, "mask-dir", CMD_AUTO | CMD_EXTRACT, Tier::Alias, "pipeline", 0, 0, "", mask_dir)   \
    F(flip_mask, "flip-mask", CMD_AUTO | CMD_EXTRACT, Tier::Advanced, "pipeline", 0, 0, "",        \
      flip_mask)                                                                                   \
    /* ---- colour ---- */                                                                         \
    F(image_gamut, "image-gamut", CMD_AUTO | CMD_EXTRACT, Tier::Advanced, "colour", 0, 0,          \
      "Rec.709|ACES2065-1|ACEScg|Rec.2020|AdobeRGB|DCI-P3", image_gamut)                           \
    F(image_is_linear, "image-linear", CMD_AUTO | CMD_EXTRACT, Tier::Advanced, "colour", 0, 0, "", \
      image_linear)                                                                                \
    F(point_color_space, "point-color", CMD_AUTO | CMD_EXTRACT, Tier::Advanced, "colour", 0, 0,    \
      "srgb|image", point_color)                                                                   \
    /* ---- camera ---- */                                                                         \
    F(camera_mode, "camera-mode", CMD_AUTO | CMD_MATCH | CMD_MAP, Tier::Basic, "camera", 0, 0,     \
      "single|folder|image", camera_mode)                                                          \
    F(camera_model, "camera-model", CMD_AUTO | CMD_MATCH | CMD_MAP, Tier::Basic, "camera", 0, 0,   \
      "simple-pinhole|pinhole|radial|opencv|full-opencv|opencv-fisheye|thin-prism-fisheye|equirectangular",\
      camera_model)                                                                                \
    F(focal, "focal", CMD_AUTO | CMD_MATCH | CMD_MAP, Tier::Basic, "camera", 0, 10000000, "",      \
      focal)                                                                                       \
    F(distortion, "distortion", CMD_AUTO | CMD_MATCH | CMD_MAP, Tier::Advanced, "camera", 0, 0,    \
      "", distortion)                                                                              \
    F(camera.exif_focal, "exif-focal", CMD_AUTO | CMD_MATCH | CMD_MAP, Tier::Advanced, "camera",   \
      0, 0, "", exif_focal)                                                                        \
    F(camera.exif_groups, "exif-groups", CMD_AUTO | CMD_MATCH | CMD_MAP, Tier::Advanced, "camera", \
      0, 0, "", exif_groups)                                                                       \
    F(camera.exif_focal_tol, "exif-focal-tol", CMD_AUTO | CMD_MATCH | CMD_MAP, Tier::Advanced,     \
      "camera", 0.001, 1.0, "", exif_focal_tol)                                                    \
    /* ---- features ---- */                                                                       \
    F(features, "features", CMD_AUTO | CMD_EXTRACT, Tier::Basic, "features", 0, 0,                 \
      "sift|aliked-n16rot|aliked-n32", features)                                                   \
    F(sift.max_num_features, "max-features", CMD_AUTO | CMD_EXTRACT, Tier::Advanced, "features",   \
      128, 1000000, "", max_features)                                                              \
    F(aliked.max_num_features, "aliked-max-features", CMD_AUTO | CMD_EXTRACT, Tier::Advanced,      \
      "features", 128, 1000000, "", aliked_max_features)                                           \
    F(aliked.min_score, "aliked-min-score", CMD_AUTO | CMD_EXTRACT, Tier::Advanced, "features", 0, \
      1, "", aliked_min_score)                                                                     \
    F(aliked.model, "aliked-model", CMD_AUTO | CMD_EXTRACT, Tier::Advanced, "features", 0, 0, "",  \
      aliked_model)                                                                                \
    F(sift.num_octaves, "octaves", CMD_AUTO | CMD_EXTRACT, Tier::Advanced, "features", 1, 8, "",   \
      octaves)                                                                                     \
    F(sift.peak_threshold, "peak-threshold", CMD_AUTO | CMD_EXTRACT, Tier::Advanced, "features",   \
      0, 1, "", peak_threshold)                                                                    \
    F(sift.edge_threshold, "edge-threshold", CMD_AUTO | CMD_EXTRACT, Tier::Advanced, "features",   \
      1, 1000, "", edge_threshold)                                                                 \
    F(sift.max_num_orientations, "max-orientations", CMD_AUTO | CMD_EXTRACT, Tier::Advanced,       \
      "features", 1, 8, "", max_orientations)                                                      \
    F(sift.profile, "profile", CMD_EXTRACT, Tier::Advanced, "features", 0, 0, "", profile)         \
    F(sift.spv_path, "spv-path", CMD_EXTRACT, Tier::Advanced, "features", 0, 0, "", spv_path)      \
    /* ---- matching ---- */                                                                       \
    F(matcher, "matcher", CMD_AUTO | CMD_MATCH, Tier::Basic, "matching", 0, 0,                     \
      "bruteforce|lightglue", matcher)                                                             \
    F(lightglue.min_score, "lightglue-min-score", CMD_AUTO | CMD_MATCH, Tier::Advanced,            \
      "matching", 0, 1, "", lightglue_min_score)                                                   \
    F(lightglue.model, "lightglue-model", CMD_AUTO | CMD_MATCH, Tier::Advanced, "matching", 0, 0,  \
      "", lightglue_model)                                                                         \
    F(match.max_ratio, "ratio", CMD_AUTO | CMD_MATCH, Tier::Advanced, "matching", 0, 1, "", ratio) \
    F(match.min_similarity, "min-similarity", CMD_AUTO | CMD_MATCH, Tier::Advanced, "matching", 0, \
      1, "", min_similarity)                                                                       \
    F(match.cross_check, "cross-check", CMD_AUTO | CMD_MATCH, Tier::Advanced, "matching", 0, 0,    \
      "", cross_check)                                                                             \
    F(match.max_num_matches, "max-matches", CMD_AUTO | CMD_MATCH, Tier::Advanced, "matching", 0,   \
      1000000000, "", max_matches)                                                                 \
    F(verify, "verify", CMD_MATCH, Tier::Advanced, "matching", 0, 0, "", verify)                   \
    F(twoview.min_num_inliers, "min-inliers", CMD_AUTO | CMD_MATCH, Tier::Advanced, "matching", 4, \
      1000000, "", min_inliers)                                                                    \
    F(prefilter.num_features, "prefilter-features", CMD_AUTO | CMD_MATCH, Tier::Advanced,          \
      "matching", 16, 100000, "", prefilter_features)                                              \
    F(prefilter.train_features, "prefilter-train", CMD_AUTO | CMD_MATCH, Tier::Advanced,           \
      "matching", 0, 1000000, "", prefilter_train)                                                 \
    F(prefilter.num_neighbors, "prefilter-neighbors", CMD_AUTO | CMD_MATCH, Tier::Advanced,        \
      "matching", 1, 100000, "", prefilter_neighbors)                                              \
    F(prefilter.min_score, "prefilter-min-score", CMD_AUTO | CMD_MATCH, Tier::Advanced,            \
      "matching", 0, 1000000, "", prefilter_min_score)                                             \
    F(prefilter.ratio, "prefilter-ratio", CMD_AUTO | CMD_MATCH, Tier::Advanced, "matching", 0, 1,  \
      "", prefilter_ratio)                                                                         \
    /* ---- mapper ---- */                                                                         \
    F(compact_unused_features, "compact-unused-features", CMD_AUTO | CMD_MAP, Tier::Advanced,     \
      "mapper", 0, 0, "", compact_unused_features)                                                 \
    F(mapper.focal_trials, "focal-trials", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper", 0, 1000,  \
      "", focal_trials)                                                                            \
    F(mapper.refine_principal_point, "refine-principal-point", CMD_AUTO | CMD_MAP, Tier::Advanced, \
      "mapper", 0, 0, "", refine_principal_point)                                                  \
    F(final_principal_point, "final-principal-point", CMD_AUTO | CMD_MAP, Tier::Advanced,          \
      "mapper", 0, 0, "", final_principal_point)                                                   \
    F(mapper.pp_min_images, "pp-min-images", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper", 2,      \
      1000000, "", pp_min_images)                                                                  \
    F(mapper.refine_extra_params, "refine-extra-params", CMD_AUTO | CMD_MAP, Tier::Advanced,       \
      "mapper", 0, 0, "", refine_extra_params)                                                     \
    F(final_extra_params, "final-extra-params", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper", 0,   \
      0, "", final_extra_params)                                                                   \
    F(final_per_image_intrinsics, "final-per-image-intrinsics", CMD_AUTO | CMD_MAP,                \
      Tier::Advanced, "mapper", 0, 0, "", final_per_image_intrinsics)                              \
    F(orient, "orient", CMD_AUTO | CMD_MAP | CMD_MERGE, Tier::Advanced, "mapper", 0, 0, "",        \
      orient)                                                                                      \
    F(mapper.min_tri_angle_deg, "min-tri-angle", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper", 0,  \
      90, "", min_tri_angle)                                                                       \
    F(mapper.init_min_tri_angle_deg, "init-min-tri-angle", CMD_AUTO | CMD_MAP, Tier::Advanced,     \
      "mapper", 0, 90, "", init_min_tri_angle)                                                     \
    F(mapper.min_num_pnp_inliers, "min-pnp-inliers", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper", \
      4, 1000000, "", min_pnp_inliers)                                                             \
    F(mapper.min_pnp_inlier_ratio, "min-pnp-ratio", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper",  \
      0, 1, "", min_pnp_ratio)                                                                     \
    F(mapper.min_image_points, "min-image-points", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper",   \
      1, 1000000, "", min_image_points)                                                            \
    F(mapper.ba_loss, "ba-loss", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper", 0, 0,               \
      "trivial|huber|cauchy", ba_loss)                                                             \
    F(mapper.ba_loss_param, "ba-loss-param", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper", 0,      \
      1000, "", ba_loss_param)                                                                     \
    F(mapper.seed_homography, "seed-homography", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper", 0,  \
      0, "", seed_homography)                                                                      \
    F(mapper.ba_real, "ba-real", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper", 0, 0,               \
      "float|double|df|cpu", ba_real)                                                              \
    F(mapper.ba_real_coarse, "ba-real-coarse", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper", 0, 0, \
      "float|double|df|cpu", ba_real_coarse)                                                       \
    F(mapper.ba_solver, "ba-solver", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper", 0, 0,           \
      "auto|dense|cg", ba_solver)                                                                  \
    F(mapper.retri_scale, "retri-scale", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper", 0, 10, "",  \
      retri_scale)                                                                                 \
    F(mapper.merge_tracks, "merge-tracks", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper", 0, 0, "", \
      merge_tracks)                                                                                \
    F(mapper.rank_by_visibility, "rank-by-visibility", CMD_AUTO | CMD_MAP, Tier::Advanced,         \
      "mapper", 0, 0, "", rank_by_visibility)                                                      \
    F(mapper.seed_blocking, "seed-blocking", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper", 0, 0,   \
      "", seed_blocking)                                                                           \
    F(mapper_mode, "mapper", CMD_AUTO | CMD_MAP, Tier::Basic, "mapper", 0, 0, "flat|bottom-up",    \
      mapper)                                                                                      \
    F(bup.partition.leaf_max_images, "bup-atom-size", CMD_AUTO | CMD_MAP, Tier::Advanced,          \
      "mapper", 8, 100000, "", bup_atom_size)                                                      \
    F(bup.partition.overlap, "bup-overlap", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper", 0,       \
      100000, "", bup_overlap)                                                                     \
    F(assemble.max_rounds, "bup-rounds", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper", 1, 100, "", \
      bup_rounds)                                                                                  \
    F(bup.atom.threads, "bup-atom-threads", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper", 0, 1024, \
      "", bup_atom_threads)                                                                        \
    F(bup.atom.ba_growth, "bup-atom-ba-growth", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper", 1,   \
      1000000, "", bup_atom_ba_growth)                                                             \
    F(assemble.joint_every, "bup-joint-every", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper", 1,    \
      100, "", bup_joint_every)                                                                    \
    F(bup.atom.tight_final_ba, "bup-atom-tight-final", CMD_AUTO | CMD_MAP, Tier::Advanced,         \
      "mapper", 0, 0, "", bup_atom_tight_final)                                                    \
    F(assemble.coarse_joint_ba, "bup-coarse-ba", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper", 0,  \
      0, "", bup_coarse_ba)                                                                        \
    F(bup.atom.init_trials, "bup-atom-init-trials", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper",  \
      1, 1000, "", bup_atom_init_trials)                                                           \
    F(bup.atom.min_model_fraction, "bup-atom-min-fraction", CMD_AUTO | CMD_MAP, Tier::Advanced,    \
      "mapper", 0, 1, "", bup_atom_min_fraction)                                                   \
    F(assemble.joint_intrinsics, "bup-joint-intrinsics", CMD_AUTO | CMD_MAP, Tier::Advanced,       \
      "mapper", 0, 0, "", bup_joint_intrinsics)                                                    \
    F(assemble.grow_every, "bup-grow-every", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper", 0, 100, \
      "", bup_grow_every)                                                                          \
    F(assemble.grow_budget_frac, "bup-grow-budget", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper",  \
      0, 1000, "", bup_grow_budget)                                                                \
    F(mapper.ba_growth_ratio, "ba-growth", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper", 1, 100,   \
      "", ba_growth)                                                                               \
    F(mapper.ba_growth_rtol, "ba-growth-rtol", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper", 0, 1, \
      "", ba_growth_rtol)                                                                          \
    F(mapper.ba_growth_patience, "ba-growth-patience", CMD_AUTO | CMD_MAP, Tier::Advanced,         \
      "mapper", 1, 1000, "", ba_growth_patience)                                                   \
    F(mapper.max_num_models, "max-models", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper", 1,        \
      100000, "", max_models)                                                                      \
    F(mapper.max_model_overlap, "model-overlap", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper", 0,  \
      1000000, "", model_overlap)                                                                  \
    F(mapper.model_overlap_ratio, "model-overlap-ratio", CMD_AUTO | CMD_MAP, Tier::Advanced,       \
      "mapper", 0, 1000, "", model_overlap_ratio)                                                  \
    F(mapper.min_model_size, "min-model-size", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper", 2,    \
      1000000, "", min_model_size)                                                                 \
    F(mapper.pnp_ratio_visible_only, "pnp-ratio-visible", CMD_AUTO | CMD_MAP, Tier::Advanced,      \
      "mapper", 0, 0, "", pnp_ratio_visible)                                                       \
    F(mapper.strong_pnp_inliers, "strong-pnp-inliers", CMD_AUTO | CMD_MAP, Tier::Advanced,         \
      "mapper", 0, 1000000, "", strong_pnp_inliers)                                                \
    F(mapper.strong_pnp_max_rival, "strong-pnp-max-rival", CMD_AUTO | CMD_MAP, Tier::Advanced,     \
      "mapper", 0, 1, "", strong_pnp_max_rival)                                                    \
    F(mapper.audit_min_evidence, "audit-evidence", CMD_AUTO | CMD_MAP, Tier::Advanced, "mapper",   \
      0, 1000000, "", audit_evidence)                                                              \
    /* ---- assembling the models ---- */                                                          \
    F(assemble.max_rounds, "rounds", CMD_AUTO | CMD_MAP, Tier::Alias, "manage", 1, 1000, "",       \
      rounds)                                                                                      \
    F(assemble.max_bridges, "max-bridges", CMD_AUTO | CMD_MAP, Tier::Advanced, "manage", 0, 1000,  \
      "", max_bridges)                                                                             \
    F(manager.do_merge, "merge", CMD_AUTO | CMD_MAP, Tier::Advanced, "manage", 0, 0, "", merge)    \
    F(manager.do_grow, "grow", CMD_AUTO | CMD_MAP, Tier::Advanced, "manage", 0, 0, "", grow)       \
    F(manager.do_reseed, "reseed", CMD_AUTO | CMD_MAP, Tier::Advanced, "manage", 0, 0, "", reseed) \
    F(manager.do_audit, "audit", CMD_AUTO | CMD_MAP, Tier::Advanced, "manage", 0, 0, "", audit)    \
    F(manager.do_split, "split", CMD_AUTO | CMD_MAP, Tier::Advanced, "manage", 0, 0, "", split)    \
    F(manager.do_duplicate_split, "fold-split", CMD_AUTO | CMD_MAP, Tier::Advanced, "manage", 0,   \
      0, "", fold_split)                                                                           \
    F(manager.duplicate.max_cut_fraction, "fold-max-cut", CMD_AUTO | CMD_MAP, Tier::Advanced,      \
      "manage", 0, 1, "", fold_max_cut)                                                            \
    F(manager.duplicate.min_fold_overlap, "fold-min-overlap", CMD_AUTO | CMD_MAP, Tier::Advanced,  \
      "manage", 0, 1, "", fold_min_overlap)                                                        \
    F(assemble.joint_intrinsics, "joint-ba", CMD_AUTO | CMD_MAP, Tier::Alias, "manage", 0, 0, "",  \
      joint_ba)                                                                                    \
    F(manager.seam_min_agreement, "seam-min-agreement", CMD_AUTO | CMD_MAP, Tier::Advanced,        \
      "manage", 0, 1, "", seam_min_agreement)                                                      \
    F(manager.seam_relative_bar, "seam-relative-bar", CMD_AUTO | CMD_MAP, Tier::Advanced,          \
      "manage", 0, 10, "", seam_relative_bar)                                                      \
    F(manager.seam_min_pairs, "seam-min-pairs", CMD_AUTO | CMD_MAP, Tier::Advanced, "manage", 1,   \
      100000, "", seam_min_pairs)                                                                  \
    F(manager.seam_rescue_frac, "seam-rescue", CMD_AUTO | CMD_MAP, Tier::Advanced, "manage", 0, 1, \
      "", seam_rescue)                                                                             \
    F(manager.seam_max_rescues, "seam-max-rescues", CMD_AUTO | CMD_MAP, Tier::Advanced, "manage",  \
      0, 100000, "", seam_max_rescues)                                                             \
    /* ---- merging ---- */                                                                        \
    F(merge.max_reproj_error, "max-error", CMD_MERGE, Tier::Advanced, "merge", 0.1, 1000, "",      \
      merge_align_max_error)                                                                       \
    F(merge.max_reproj_error, "merge-max-error", CMD_AUTO | CMD_MAP, Tier::Advanced, "merge", 0.1, \
      1000, "", merge_max_error)                                                                   \
    F(merge.min_common_images, "min-common", CMD_MERGE, Tier::Advanced, "merge", 2, 1000000, "",   \
      min_common)                                                                                  \
    F(merge.min_common_images, "merge-min-common", CMD_AUTO | CMD_MAP, Tier::Advanced, "merge", 2, \
      1000000, "", merge_min_common)                                                               \
    F(merge.splice_arbitrate_inliers, "merge-arbitrate", CMD_AUTO | CMD_MAP | CMD_MERGE,           \
      Tier::Advanced, "merge", 0, 1000000, "", merge_arbitrate)                                    \
    F(merge.min_inlier_ratio, "min-inlier-ratio", CMD_MERGE, Tier::Advanced, "merge", 0, 1, "",    \
      min_inlier_ratio)                                                                            \
    F(merge.filter_reproj_error, "filter-error", CMD_MERGE, Tier::Advanced, "merge", 0, 1000, "",  \
      filter_error)                                                                                \
    F(merge.min_tri_angle_deg, "min-tri-angle", CMD_MERGE, Tier::Advanced, "merge", 0, 90, "",     \
      merge_min_tri_angle)                                                                         \
    F(merge_ba, "ba", CMD_MERGE, Tier::Advanced, "merge", 0, 0, "", ba)                            \
    F(in_place, "in-place", CMD_MERGE, Tier::Advanced, "merge", 0, 0, "", in_place)                \
    /* ---- inputs ---- */                                                                         \
    F(image_dir, "images", CMD_MAP, Tier::Advanced, "input", 0, 0, "", images)                     \
    F(feature_dir, "features", CMD_MAP, Tier::Advanced, "input", 0, 0, "", feature_dir)            \
    F(resume, "resume", CMD_MAP, Tier::Advanced, "input", 0, 0, "", resume)                        \
    F(check, "check", CMD_MAP, Tier::Advanced, "input", 0, 0, "", check)                           \
    /* ---- runtime ---- */                                                                        \
    F(threads, "threads", CMD_AUTO | CMD_MATCH | CMD_MAP, Tier::Advanced, "runtime", 0, 4096, "",  \
      threads)                                                                                     \
    F(decode_threads, "decode-threads", CMD_AUTO | CMD_EXTRACT, Tier::Advanced, "runtime", 0,      \
      4096, "", decode_threads)                                                                    \
    F(decode_budget_mb, "decode-budget", CMD_AUTO | CMD_EXTRACT, Tier::Advanced, "runtime", 0,     \
      1048576, "", decode_budget)                                                                  \
    F(device, "device", CMD_ALL, Tier::Advanced, "runtime", -1, 64, "", device)                    \
    F(quiet, "quiet", CMD_ALL, Tier::Advanced, "runtime", 0, 0, "", quiet)

// ---------------------------------------------------------------------------
// Consumers
// ---------------------------------------------------------------------------

// Outcome of offering one command-line token to the table.
enum class FieldResult { Unknown, Ok, Error };

// Try to consume argv[i] (and its value, if the field takes one) as a table
// field of `cmd`. `arg` is the whole token, leading dashes included; `seen`
// collects the names that were set, which is what makes an explicit flag beat a
// preset and what tells `map` whether the command line said anything about
// cameras. Returns Unknown if no field of this command has that name.
FieldResult setConfigField(SfmConfig& cfg, uint32_t cmd, const std::string& arg, int argc,
                           char** argv, int& i, std::set<std::string>& seen, std::string& error);

// One field the presets moved, for the "which options a preset changed" report.
struct PresetChange {
    std::string flag, from, to;
};

// Apply --quality / --data-type. Fields in `seen` are left alone, so an
// explicit flag always wins; everything the presets do move is logged.
std::string applyPresets(SfmConfig& cfg, const std::set<std::string>& seen,
                         std::vector<PresetChange>& moved);

// The option block of `--help` for one subcommand: the table's rows for `cmd`,
// grouped, with each field's current value as the printed default.
void printConfigOptions(FILE* out, uint32_t cmd, const SfmConfig& defaults);

// One option line in that same layout, for the flags a CLI parses by hand.
// `flag` carries its own metavar ("-o, --output DIR"); `value` is the bracketed
// note after it, empty for a switch.
void printOptionLine(FILE* out, const std::string& flag, const std::string& value,
                     const std::string& help);

}  // namespace sfm

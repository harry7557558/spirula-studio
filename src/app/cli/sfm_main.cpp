// spirula-sfm: the SfM pipeline CLI. Subcommands are the stage graph
// (src/sfm/README.md), each reading and writing files on disk so any one of
// them can be replaced by COLMAP's equivalent to bisect a failure:
//
//   spirula-sfm auto    <image_dir> -o <workspace>      all of the below
//   spirula-sfm extract <image|dir> -o <features>
//   spirula-sfm match   <features>  -o <matches.bin>
//   spirula-sfm map     <matches.bin> <features> -o <sparse/>
//   spirula-sfm merge   <sparse/>   -o <merged/>
//   spirula-sfm ba      <bal_problem.txt>               solver benchmark
//
// This file is presentation and plumbing only: what a flag *means* lives in
// sfm/SfmConfig.h's descriptor table, which is also what `--help` prints and
// what the GUI will edit. Flags that do not name one scalar field are parsed
// here, before the table is offered the token, so a hand-parsed name always
// wins -- `map --audit` (run an audit pass) has to beat the table's `--audit`
// / `--no-audit` switch for the audit pass.
//
// The self-checks are separate binaries (src/sfm/tests/, one per area).
#include "app/Tools.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "core/ColorSpace.h"
#include "core/ExrImage.h"
#include "sfm/SfmConfig.h"
#include "sfm/core/Progress.h"
#include "sfm/core/CameraSetup.h"
#include "sfm/core/FeatureCompaction.h"
#include "sfm/core/Log.h"
#include "sfm/core/Features.h"
#include "sfm/core/Image.h"
#include "sfm/core/ImageLoader.h"
#include "sfm/core/Mask.h"
#include "sfm/core/Matches.h"
#include "sfm/feature/Matcher.h"
#include "sfm/feature/PairSelection.h"
#include "sfm/feature/Pairing.h"
#include "sfm/feature/Sift.h"
#include "sfm/feature/Verification.h"
#include "sfm/geometry/TwoView.h"
#include "sfm/map/Assemble.h"
#include "sfm/map/Mapper.h"
#include "sfm/map/Orient.h"
#include "sfm/map/Merge.h"

#include "i18n/catalog/Sfm.h"
#include "i18n/catalog/Cli.h"
#include "i18n/catalog/SfmHelp.h"

// `spirula-sfm ba`, in sfm_ba.cpp. It prints its own help.
int cmdBa(int argc, char** argv);
void printBaHelp(FILE* out);

// Set by the build (cmake/SsOptions.cmake reads it from pyproject.toml).
#ifndef SS_VERSION
#define SS_VERSION "dev"
#endif

namespace fs = std::filesystem;
using namespace sfm;

// Every line this tool prints goes out tagged and translated; see
// sfm/core/Log.h for the mechanism and for what stays English.
namespace L = sfm::slog;
namespace M = spirula::i18n::msg::sfm;
namespace CM = spirula::i18n::msg::cli;
namespace H = spirula::i18n::msg::sfmhelp;
using sfm::slog::Tag;

// How this tool was invoked ("spirula sfm" as dispatched); see app/Tools.h.
// The examples in the command tables below are written against the historical
// name and rewritten at print time.
static const char* kProgram = "spirula sfm";
static std::string with_program_name(const char* text) {
    return app::help_text(text, "spirula-sfm");
}

// ---------------------------------------------------------------------------
// Command help
// ---------------------------------------------------------------------------
// One record per subcommand: what the top-level list shows, the argument
// syntax, the paragraph that says what the stage does, the flags this file
// parses by hand (the table prints the rest), and worked examples. Kept
// together so the six help screens stay in one shape.
struct CommandInfo {
    const char* name;
    uint32_t mask;
    // The prose is translated (i18n/catalog/SfmHelp.h); the syntax and the
    // examples are what the reader types, so they are not.
    const spirula::i18n::Msg* summary;              // one line, for `--help`
    const char* usage;                              // argument syntax
    const spirula::i18n::Msg* description[3];       // one per paragraph, then null
    void (*own_options)(FILE*);
    const char* examples;                           // pre-wrapped, indented
    bool exit_status;                               // print `auto`'s footer
};

// A description paragraph, wrapped where the language allows rather than where
// an English hand-wrap once put a newline.
static void printParagraph(FILE* out, const spirula::i18n::Msg& m) {
    for (const std::string& line : spirula::i18n::wrap(m.get(), 94))
        std::fprintf(out, "  %s\n", line.c_str());
}

// One line of a command's own (hand-parsed) options, in the table's layout.
static void helpLine(FILE* out, const char* flag, const char* value, const char* help) {
    printOptionLine(out, flag, value, help);
}

static void ownOptionsAuto(FILE* out) {
    helpLine(out, "-o, --output DIR", H::word_required.get(),
             H::opt_auto_output.get());
    helpLine(out, "--no-masks", "", H::opt_no_masks.get());
    helpLine(out, "--no-manage", "", H::opt_no_manage_auto.get());
    helpLine(out, "--progress-dir DIR", "", H::opt_progress_dir.get());
    helpLine(out, "-h, --help", "", H::opt_help.get());
}
static void ownOptionsExtract(FILE* out) {
    helpLine(out, "-o, --output DIR|FILE", "features", H::opt_extract_output.get());
    helpLine(out, "-h, --help", "", H::opt_help.get());
}
static void ownOptionsMatch(FILE* out) {
    helpLine(out, "-o, --output FILE", "", H::opt_match_output.get());
    helpLine(out, "--progress-dir DIR", "", H::opt_progress_dir.get());
    helpLine(out, "-h, --help", "", H::opt_help.get());
}
static void ownOptionsMap(FILE* out) {
    helpLine(out, "-o, --output DIR", "", H::opt_map_output.get());
    helpLine(out, "--audit", "", H::opt_map_audit.get());
    helpLine(out, "--no-manage", "", H::opt_no_manage_map.get());
    helpLine(out, "--progress-dir DIR", "", H::opt_progress_dir.get());
    helpLine(out, "-h, --help", "", H::opt_help.get());
}
static void ownOptionsMerge(FILE* out) {
    helpLine(out, "-o, --output DIR", "", H::opt_merge_output.get());
    helpLine(out, "-h, --help", "", H::opt_help.get());
}

static const CommandInfo kCommands[] = {
    {"auto", CMD_AUTO, &H::sum_auto,
     "[IMAGE_DIR|DATASET_DIR] -o WORKSPACE [options]",
     {&H::desc_auto_1, &H::desc_auto_2, &H::desc_auto_3},
     ownOptionsAuto,
     "  spirula-sfm auto images/ -o workspace/\n"
     "  spirula-sfm auto -o ws/                       # ./images and ./masks, all defaults\n"
     "  spirula-sfm auto DATASET/ -o ws/ --data-type video --quality medium\n"
     "  spirula-sfm auto images/ -o ws/ --camera-model opencv-fisheye --focal 520\n"
     "  spirula-sfm auto images/ -o ws/ --camera-model cam0=thin-prism-fisheye",
     /*exit_status=*/true},

    {"extract", CMD_EXTRACT, &H::sum_extract,
     "<IMAGE|DIR> [-o OUT] [options]",
     {&H::desc_extract_1, &H::desc_extract_2, nullptr},
     ownOptionsExtract,
     "  spirula-sfm extract images/ -o features/\n"
     "  spirula-sfm extract images/ -o features/ --masks masks/ --max-features 4096\n"
     "  spirula-sfm extract photo.jpg -o photo.bin",
     false},

    {"match", CMD_MATCH, &H::sum_match,
     "<FEATURE_DIR> -o MATCHES.BIN [options]",
     {&H::desc_match_1, &H::desc_match_2, nullptr},
     ownOptionsMatch,
     "  spirula-sfm match features/ -o matches.bin\n"
     "  spirula-sfm match features/ -o matches.bin --pairs prefilter --threads 8\n"
     "  spirula-sfm match features/ -o matches.bin --camera-model opencv \\\n"
     "                             --camera-model cam0=thin-prism-fisheye --focal cam0=520",
     false},

    {"map", CMD_MAP, &H::sum_map,
     "<MATCHES.BIN> <FEATURE_DIR> -o SPARSE_DIR [options]",
     {&H::desc_map_1, &H::desc_map_2, nullptr},
     ownOptionsMap,
     "  spirula-sfm map matches.bin features/ -o sparse/ --images images/\n"
     "  spirula-sfm map matches.bin features/ -o sparse/ --max-models 1\n"
     "  spirula-sfm map matches.bin features/ --resume sparse/ --check\n"
     "  spirula-sfm map matches.bin features/ -o sparse/ --resume sparse/ --audit",
     false},

    {"merge", CMD_MERGE, &H::sum_merge,
     "<SPARSE_DIR|MODEL_DIR> [more...] -o DIR [options]",
     {&H::desc_merge_1, &H::desc_merge_2, nullptr},
     ownOptionsMerge,
     "  spirula-sfm merge sparse/ -o merged/\n"
     "  spirula-sfm merge sparse/ --in-place\n"
     "  spirula-sfm merge runA/sparse/0 runB/sparse/0 -o merged/ --min-common 5",
     false},
};

static const CommandInfo* findCommand(const std::string& name) {
    for (const CommandInfo& c : kCommands)
        if (name == c.name) return &c;
    return nullptr;
}

static void printCommandHelp(const CommandInfo& c) {
    FILE* out = stdout;
    std::fprintf(out, "%s %s -- %s\n\n", kProgram, c.name, c.summary->get());
    std::fprintf(out, "%s\n  %s %s %s\n\n", H::label_usage.get(), kProgram, c.name,
                 c.usage);
    std::fprintf(out, "%s\n", H::label_description.get());
    for (int i = 0; i < 3 && c.description[i]; i++) {
        if (i) std::fprintf(out, "\n");
        printParagraph(out, *c.description[i]);
    }
    std::fprintf(out, "\n%s\n", H::label_options.get());
    c.own_options(out);
    // Defaults are printed from a fresh config, which is exactly what the
    // command starts from; `auto` says separately what its presets then move.
    SfmConfig defaults;
    printConfigOptions(out, c.mask, defaults);
    std::fprintf(out, "\n%s\n%s\n", H::label_examples.get(),
                 with_program_name(c.examples).c_str());
    if (c.exit_status) {
        std::fprintf(out, "\n%s\n", H::label_exit_status.get());
        std::fprintf(out, "  0  %s\n", H::exit_0.get());
        std::fprintf(out, "  1  %s\n", H::exit_1.get());
        std::fprintf(out, "  2  %s\n", H::exit_2.get());
        std::fprintf(out, "  3  %s\n", H::exit_3.get());
    }
}

static void printTopHelp(FILE* out) {
    std::fprintf(out, "%s %s -- %s\n\n", kProgram, SS_VERSION, H::tagline.get());
    std::fprintf(out, "%s\n  %s <command> [options]\n  %s <command> --help\n\n",
                 H::label_usage.get(), kProgram, kProgram);
    std::fprintf(out, "%s\n", H::label_commands.get());
    for (const CommandInfo& c : kCommands)
        std::fprintf(out, "  %-9s %s\n", c.name, c.summary->get());
    std::fprintf(out, "  %-9s %s\n", "ba", H::sum_ba.get());
    std::fprintf(out, "\n%s\n", H::label_options.get());
    std::fprintf(out, "  %-15s %s\n", "-h, --help", H::opt_help.get());
    std::fprintf(out, "  %-15s %s\n", "-V, --version", H::opt_version.get());
    std::fprintf(out, "\n");
    for (const std::string& line : spirula::i18n::wrap(H::top_note.get(), 78))
        std::fprintf(out, "%s\n", line.c_str());
    std::fprintf(out, "\n%s\n", H::label_environment.get());
    std::fprintf(out, "  %-19s %s\n", "SS_SFM_MAP_PROF=1", H::env_map_prof.get());
}

// A usage error, in the shape every command-line tool uses: what was wrong, and
// where to look. Never exit code 2 or 3 -- `auto` spends those on the quality
// of the reconstruction, and a batch script must be able to tell them apart.
static int usageError(const char* cmd, const std::string& msg) {
    std::fprintf(stderr, "%s %s: %s\n", kProgram, cmd,
                 spirula::i18n::format(CM::error_line, {msg}).c_str());
    std::fprintf(stderr, "%s\n",
                 spirula::i18n::format(
                     CM::usage_try_help,
                     {std::string(kProgram) + " " + cmd + " --help"}).c_str());
    return 1;
}

// Offer one token to the descriptor table. Returns 1 handled, 0 not a flag of
// this command, -1 usage error (already reported).
static int tableFlag(SfmConfig& cfg, uint32_t cmd, const char* cmdname, const std::string& a,
                     int argc, char** argv, int& i, std::set<std::string>& seen) {
    std::string err;
    FieldResult r = setConfigField(cfg, cmd, a, argc, argv, i, seen, err);
    if (r == FieldResult::Ok) return 1;
    if (r == FieldResult::Error) { usageError(cmdname, err); return -1; }
    return 0;
}

// --camera-model / --focal / --distortion: a bare value sets the dataset-wide
// default (which is a table field), PREFIX=VALUE names one camera group (which
// is not).
static bool cameraOverride(SfmConfig& cfg, OverrideKind kind, const std::string& v,
                           std::set<std::string>& seen, std::string& err) {
    const char* flag = kind == OverrideKind::Focal        ? "focal"
                       : kind == OverrideKind::Distortion ? "distortion"
                                                          : "camera-model";
    const char* form = kind == OverrideKind::Focal        ? "F or PREFIX=F"
                       : kind == OverrideKind::Distortion ? "k1,k2,... or PREFIX=k1,k2,..."
                                                          : "MODEL or PREFIX=MODEL";
    if (!parseCameraOverride(v, kind, cfg.camera.overrides)) {
        err = std::string("bad --") + flag + " '" + v + "' (" + form + ")";
        return false;
    }
    if (v.find('=') != std::string::npos) return true;  // per-group only
    switch (kind) {
        case OverrideKind::Focal:
            cfg.focal = std::atof(v.c_str());
            break;
        case OverrideKind::Distortion:
            cfg.distortion = v;
            break;
        case OverrideKind::Model: {
            CamModel m;
            if (!parseCamModelName(v, m)) {
                err = "unknown --camera-model '" + v + "'";
                return false;
            }
            cfg.camera_model = v;
            break;
        }
    }
    seen.insert(flag);
    return true;
}

// The three flags above, recognized by name; false for anything else.
static bool cameraOverrideFlag(const std::string& a, OverrideKind& kind) {
    if (a == "--camera-model") kind = OverrideKind::Model;
    else if (a == "--focal") kind = OverrideKind::Focal;
    else if (a == "--distortion") kind = OverrideKind::Distortion;
    else return false;
    return true;
}

// Did the command line say anything about cameras? If not, `map` keeps the
// setup verification recorded in matches.bin rather than deriving its own (D47).
static bool sawCameraFlags(const std::set<std::string>& seen) {
    static const char* kCameraFlags[] = {"camera-mode", "camera-model", "focal", "distortion",
                                         "exif-focal", "exif-groups", "exif-focal-tol"};
    for (const char* f : kCameraFlags)
        if (seen.count(f)) return true;
    return false;
}

static bool isImageExt(const std::string& e) {
    std::string s;
    for (char c : e) s += (char)std::tolower((unsigned char)c);
    return s == ".jpg" || s == ".jpeg" || s == ".png" || s == ".bmp" || s == ".tga" ||
           s == ".ppm" || s == ".pgm" || s == ".exr";
}

// macOS writes an AppleDouble sidecar, `._<name>`, beside any file it has to
// attach metadata to on a filesystem with nowhere to put it -- an exFAT or
// NTFS drive, an SMB share. The sidecar keeps the original's extension, so it
// passes every extension test here: `._00333.jpg` enumerates as an image, and
// `._00168.bin` as a feature file, where the reader can only report that the
// magic is wrong and the whole reconstruction stops.
static bool isSidecar(const fs::path& p) {
    const std::string n = p.filename().string();
    return n.size() > 2 && n[0] == '.' && n[1] == '_';
}

// Does `root` hold any image outside `nested`? This is what decides whether
// `auto DATASET` may quietly reinterpret itself as `auto DATASET/images`.
//
// The convenience is real -- a dataset directory and its image directory
// should behave the same -- but taken unconditionally it silently DISCARDS
// input: a capture assembled from two folders, one of which happened to be
// called `images`, produces `<ws>/images/{images,images_2}`, and descending
// into the first reconstructs half the capture under names that no longer
// resolve against the dataset (the models' names become `camera1/x.png`
// rather than `images/camera1/x.png`, so the trainer cannot find one file).
// Descending is therefore only allowed when it cannot lose anything: every
// image under `root` is already under `root/images`.
static bool holdsImagesOutside(const fs::path& root, const fs::path& nested) {
    std::error_code walk, ec;
    for (auto it = fs::recursive_directory_iterator(
             root, fs::directory_options::follow_directory_symlink, walk);
         !walk && it != fs::recursive_directory_iterator(); it.increment(walk)) {
        if (it->is_directory(ec)) {
            if (fs::equivalent(it->path(), nested, ec)) it.disable_recursion_pending();
            continue;
        }
        if (it->is_regular_file(ec) && isImageExt(it->path().extension().string()) &&
            !isSidecar(it->path()))
            return true;
    }
    return false;
}

// Path of `p` relative to the directory it was enumerated from.
//
// NOT fs::relative(): that canonicalizes both operands, which resolves
// symlinks. A directory of symlinked images (the natural way to stage a
// dataset without copying it) then relativizes to the *link targets*, yielding
// "../../.." paths that escape the output directory -- `extract` would write
// features next to the source images instead of into -o. Everything we pass
// here came out of a directory_iterator over `root`, so it is literally
// prefixed by `root` and a lexical comparison is both correct and cheap.
static fs::path relativeTo(const fs::path& p, const fs::path& root) {
    fs::path r = root;
    // "dir/" iterates to "dir/x.jpg" but has a trailing empty element, which
    // lexically_relative would mismatch into "../x.jpg". Drop it.
    if (!r.empty() && r.filename().empty()) r = r.parent_path();
    return p.lexically_relative(r);
}

// Wall-clock seconds since an arbitrary epoch, for the stage timings `auto`
// reports.
static double now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// Mean/median reprojection error over a reconstruction's observations. The one
// number that says whether a model is sane; reported by both `map` and `auto`.
static void reprojStats(const Reconstruction& rec, const std::vector<FeatureSet>& feats,
                        double& mean, double& median, size_t& nobs) {
    std::vector<double> e;
    for (const auto& kv : rec.points3D)
        for (const TrackElement& t : kv.second.track) {
            auto img = rec.images.find(t.image_id);
            if (img == rec.images.end() || !img->second.registered) continue;
            auto cam = rec.cameras.find(img->second.camera_id);
            if (cam == rec.cameras.end()) continue;
            Vec3 pc = mul(img->second.pose.R, kv.second.xyz) + img->second.pose.t;
            if (pc.z <= 0) continue;
            Vec2 px = cam->second.project(pc);
            const Keypoint& k = feats[t.image_id].keypoints[t.point2D_idx];
            e.push_back(std::hypot(px.x - k.x, px.y - k.y));
        }
    nobs = e.size();
    mean = median = 0;
    if (e.empty()) return;
    double s = 0;
    for (double v : e) s += v;
    mean = s / e.size();
    std::nth_element(e.begin(), e.begin() + e.size() / 2, e.end());
    median = e[e.size() / 2];
}

// Sample an RGB color at every keypoint from the (already decoded) source image
// and store it in the feature set, so the point cloud can be colored without a
// second decode pass. No-op if the image was decoded without color.
static void sampleFeatureColors(FeatureSet& fs, const GrayImage& img) {
    if (!img.hasColor()) return;
    fs.colors.resize((size_t)fs.count() * 3);
    for (uint32_t i = 0; i < fs.count(); i++)
        sampleColor(img, fs.keypoints[i].x, fs.keypoints[i].y, &fs.colors[(size_t)i * 3]);
}

// Put a freshly extracted set into the source image's frame and attach what
// EXIF said about the camera. Called once per image, after anything that
// indexes the *decoded* image (colors, masks) is done.
static void finishFeatures(FeatureSet& fs, const GrayImage& img) {
    scaleKeypoints(fs, img.orig_width, img.orig_height);
    fs.exif_focal = exifFocalPx(img.exif, fs.width, fs.height);
    fs.exif_camera = exifCameraKey(img.exif, fs.width, fs.height);
}

// Put every finished model in the upright, centred, unit-sized frame before it
// is written (sfm/map/Orient.h explains why here rather than downstream).
// Each model is its own gauge, so each gets its own transform.
static void orientModels(std::vector<Reconstruction>& models, bool enabled, bool verbose) {
    if (!enabled) return;
    for (size_t i = 0; i < models.size(); i++) {
        const Sim3 T = orientModel(models[i]);
        if (verbose)
            L::err(Tag::Orient, M::orient_done, {(long long)i, L::num(T.scale, 4)});
    }
}

// An EXR carries its own colour space. Reading it needs no declaration -- the
// decoder falls back to the file's own -- but --point-color and the reported
// space both do, so adopt it before any stage runs.
static void adoptExrColorSpace(SfmConfig& cfg, const std::string& imagedir,
                               const std::set<std::string>& seen) {
    const bool take_gamut = !seen.count("image-gamut");
    const bool take_linear = !seen.count("image-linear");
    if (!take_gamut && !take_linear) return;
    std::error_code ec, walk;
    for (auto it = fs::recursive_directory_iterator(
             imagedir, fs::directory_options::follow_directory_symlink, walk);
         !walk && it != fs::recursive_directory_iterator(); it.increment(walk)) {
        if (!it->is_regular_file(ec)) continue;
        if (!isImageExt(it->path().extension().string()) || isSidecar(it->path()))
            continue;
        exr::Info info;
        if (!exr::declared_color_space(it->path().string(), info)) return;
        if (take_gamut) cfg.image_gamut = info.gamut;
        if (take_linear) cfg.image_is_linear = info.is_linear;
        const std::string name =
            cfg.image_gamut.empty() ? "Rec.709" : cfg.image_gamut;
        if (take_linear) L::out(Tag::Run, M::run_exr_color, {name});
        else             L::out(Tag::Run, M::run_exr_gamut_from_file, {name});
        if (take_gamut && !info.gamut_known)
            L::warn(Tag::Run, M::run_exr_gamut_unknown, {});
        return;
    }
}

static void reportFeatureCompaction(const FeatureCompactionStats& stats) {
    const double removed_pct =
        stats.original_features ? 100.0 * stats.removedFeatures() / stats.original_features : 0.0;
    L::out(Tag::Map, M::map_feature_compaction,
           {(long long)stats.original_features, (long long)stats.compact_features,
            (long long)stats.removedFeatures(), L::num(removed_pct, 2), (long long)stats.images,
            (long long)stats.zero_feature_images, (long long)stats.pairs,
            (long long)stats.correspondences});
}

// Point colours are sampled from images the loader converted to sRGB, which is
// where "srgb" leaves them. "image" puts them back in the photographs' space,
// for a trainer run with convert_initial_point_cloud_color off.
static void recolorPoints(std::vector<Reconstruction>& models, const SfmConfig& cfg) {
    if (cfg.point_color_space != "image") return;
    if (colorspace::is_identity(cfg.image_gamut, cfg.image_is_linear)) return;
    for (Reconstruction& m : models)
        for (auto& kv : m.points3D)
            colorspace::from_srgb_inplace(kv.second.rgb, 1, cfg.image_gamut,
                                          cfg.image_is_linear);
}

// Write every reconstruction the mapper produced as <dir>/0, <dir>/1, ...
// (D41). This is COLMAP's layout for a dataset that does not form one connected
// view graph -- `sparse/0` is the model with the most 3D points, and the rest
// are the components a later merge step can align onto it. A single-model
// dataset therefore still writes exactly `sparse/0`, as before.
static void writeModels(const std::vector<Reconstruction>& models, const fs::path& dir,
                        bool verbose) {
    for (size_t i = 0; i < models.size(); i++) {
        fs::path p = dir / std::to_string(i);
        fs::create_directories(p);
        models[i].writeBinary(p.string());
        if (verbose)
            L::err(Tag::Map, M::map_wrote_model,
                   {(long long)i, (long long)models[i].numRegistered(),
                    (long long)models[i].points3D.size(), p.string()});
    }
    // A re-run that produces fewer models than the last one left numbered
    // directories behind, and `--resume` would read them back as if they were
    // part of this reconstruction. Remove them.
    if (!fs::is_directory(dir)) return;
    for (const auto& e : fs::directory_iterator(dir)) {
        if (!e.is_directory()) continue;
        const std::string name = e.path().filename().string();
        if (name.find_first_not_of("0123456789") != std::string::npos) continue;
        size_t idx = std::stoull(name);
        if (idx < models.size()) continue;
        if (!fs::exists(e.path() / "images.bin")) continue;
        std::error_code ec;
        fs::remove_all(e.path(), ec);
        if (verbose && !ec)
            L::err(Tag::Map, M::map_removed_stale, {e.path().string()});
    }
}

// Distinct images covered by any model. Not the sum of the model sizes: models
// deliberately overlap by up to max_model_overlap images (D41), which is what a
// merge step aligns on, so summing double-counts the joins.
static size_t distinctRegistered(const std::vector<Reconstruction>& models) {
    std::set<uint32_t> ids;
    for (const Reconstruction& m : models)
        for (const auto& kv : m.images)
            if (kv.second.registered) ids.insert(kv.first);
    return ids.size();
}

// One line per model beyond the first, so a fragmented capture is visible in
// the summary rather than only in the directory listing.
static void printExtraModels(const std::vector<Reconstruction>& models,
                             const std::vector<FeatureSet>& feats) {
    for (size_t i = 1; i < models.size(); i++) {
        double mn = 0, md = 0;
        size_t nobs = 0;
        reprojStats(models[i], feats, mn, md, nobs);
        L::out(Tag::Run, M::map_model_line_error,
               {(long long)i, (long long)models[i].numRegistered(),
                (long long)models[i].points3D.size(), L::num(mn, 3)});
    }
}

// Registration per top-level image sub-folder.
//
// A capture assembled from several inputs has one folder per input, and the
// number that matters to whoever assembled it is not the total but whether
// every input got in: an input that contributed nothing is a dataset that
// silently describes half of what was handed over. Printed only when there is
// more than one folder, since otherwise it is the summary's own count again.
static void printFolderCoverage(const std::vector<Reconstruction>& models,
                                const MatchesDatabase& db) {
    auto group_of = [](const std::string& name) {
        size_t slash = name.find('/');
        return slash == std::string::npos ? std::string(".") : name.substr(0, slash);
    };
    std::map<std::string, std::pair<size_t, size_t>> per;  // folder -> {registered, total}
    for (const auto& im : db.images) per[group_of(im.name)].second++;
    if (per.size() < 2) return;
    std::set<uint32_t> ids;
    for (const Reconstruction& m : models)
        for (const auto& kv : m.images)
            if (kv.second.registered) ids.insert(kv.first);
    for (uint32_t id : ids)
        if (id < db.images.size()) per[group_of(db.images[id].name)].first++;
    L::out(Tag::Run, M::sum_per_folder);
    for (const auto& kv : per)
        L::out(Tag::Run, M::sum_folder_line,
               {kv.first, (long long)kv.second.first, (long long)kv.second.second});
    for (const auto& kv : per)
        if (kv.second.first == 0) L::warn(Tag::Run, M::sum_folder_empty, {kv.first});
}

// Mean/median reprojection error straight from a model, with no FeatureSets in
// hand: Image::points2D already carries the keypoint coordinates. `merge`
// starts from models on disk, where that is all there is. Observations behind
// the camera are excluded rather than counted as infinite, as reprojStats does.
static void modelReprojStats(const Reconstruction& rec, double& mean, double& median,
                             size_t& nobs) {
    std::vector<double> e;
    for (const auto& kv : rec.points3D)
        for (const TrackElement& t : kv.second.track) {
            auto img = rec.images.find(t.image_id);
            if (img == rec.images.end() || !img->second.registered) continue;
            auto cam = rec.cameras.find(img->second.camera_id);
            if (cam == rec.cameras.end() || t.point2D_idx >= img->second.points2D.size()) continue;
            double r = reprojErrorAt(cam->second, img->second.pose,
                                     img->second.points2D[t.point2D_idx], kv.second.xyz);
            if (r < 1e29) e.push_back(r);
        }
    nobs = e.size();
    mean = median = 0;
    if (e.empty()) return;
    double s = 0;
    for (double v : e) s += v;
    mean = s / e.size();
    std::nth_element(e.begin(), e.begin() + e.size() / 2, e.end());
    median = e[e.size() / 2];
}

// ---- merging (D43) ----
// Everything `merge` and `auto` share: run the session to convergence, then
// bundle-adjust the models that absorbed something. A merged model is two
// independently optimized halves glued along a seam that has never been
// optimized as one, so the BA is not cosmetic -- it is where the shared images'
// two answers become one. Models that merged nothing are moved through
// untouched, so a dataset with nothing to merge is bit-identical to before.
struct MergeSummary {
    size_t before = 0, after = 0, merges = 0, refused = 0;
    double seconds = 0, ba_seconds = 0;
};

static std::vector<Reconstruction> mergeModels(std::vector<Reconstruction> models,
                                               const MergeOptions& mo, bool refine, int device,
                                               MergeSummary& sum) {
    sum.before = sum.after = models.size();
    if (models.size() < 2) return models;
    const double t0 = now();
    MergeSession session(std::move(models), mo);
    sum.merges = session.runAuto();
    for (const MergeAttempt& a : session.log())
        if (!a.merged) sum.refused++;
    if (refine && sum.merges) {
        const double tb = now();
        VkContext ctx;  // one device + pipeline set for every model, as the mapper does
        for (size_t i : session.changed()) {
            Reconstruction& m = session.modelMut(i);
            BundleOptions bo;
            bo.device = device;
            bo.verbose = false;
            bo.shared_ctx = &ctx;
            double cost = runGlobalBA(m, bo);
            size_t robs = 0, rpts = 0;
            filterModel(m, mo.filter_reproj_error, mo.min_tri_angle_deg, robs, rpts);
            if (mo.verbose)
                L::err(Tag::Merge, M::merge_rebundled,
                       {(long long)i, cost, (long long)robs, (long long)rpts,
                        (long long)m.points3D.size()});
        }
        sum.ba_seconds = now() - tb;
    }
    std::vector<Reconstruction> out = session.take();
    sum.after = out.size();
    sum.seconds = now() - t0;
    return out;
}

// ---- reading models back off disk ----
// An input is either one model directory or a `sparse/` holding 0, 1, 2, ...
// Numbered sub-directories are read in numeric order, so index 0 stays the
// model with the most structure and therefore the natural anchor.
static bool isModelDir(const fs::path& p) {
    return fs::exists(p / "cameras.bin") && fs::exists(p / "images.bin");
}

static bool collectModelDirs(const std::string& input, std::vector<fs::path>& out) {
    fs::path p(input);
    if (!fs::is_directory(p)) {
        L::fail(Tag::Map, M::map_not_a_directory, {input});
        return false;
    }
    if (isModelDir(p)) {
        out.push_back(p);
        return true;
    }
    std::vector<std::pair<long, fs::path>> numbered;
    for (const auto& e : fs::directory_iterator(p)) {
        if (!e.is_directory() || !isModelDir(e.path())) continue;
        const std::string n = e.path().filename().string();
        if (n.empty() || n.find_first_not_of("0123456789") != std::string::npos) continue;
        numbered.emplace_back(std::stol(n), e.path());
    }
    if (numbered.empty()) {
        L::fail(Tag::Map, M::map_no_model_in, {input});
        return false;
    }
    std::sort(numbered.begin(), numbered.end());
    for (auto& kv : numbered) out.push_back(kv.second);
    return true;
}

static bool readModels(const std::string& dir, std::vector<Reconstruction>& models, bool verbose) {
    std::vector<fs::path> dirs;
    if (!collectModelDirs(dir, dirs)) return false;
    for (const fs::path& d : dirs) {
        try {
            models.push_back(Reconstruction::readBinary(d.string()));
        } catch (const std::exception& e) {
            L::fail(Tag::Map, M::map_cannot_read, {d.string(), e.what()});
            return false;
        }
        if (verbose)
            L::out(Tag::Map, M::map_read_model,
                   {d.string(), models.back().numRegistered(),
                    (long long)models.back().points3D.size()});
    }
    return true;
}

// Flat or bottom-up, per --mapper. Flat is the default for every capture: it is
// what the measurements are on, and nothing is ever cut, so nothing has to be
// glued back. Bottom-up is asked for by name.
//
// Either way the models are then assembled by the same schedule (D63,
// sfm/map/Assemble.h): merge levels with growth and a joint solve between them,
// then the finishing passes. There is no separate manage stage -- it drove the
// same operations in rounds, over models nothing had touched, and on a
// 5356-image capture cost more than the mapping it followed.
// What became of the mapper's models: one line, because a capture that comes
// back in several pieces is the case a user has to be able to reason about, and
// the counts say whether that was the view graph's doing or a refused merge.
static void printAssembly(const AssembleStats& ast, size_t models, Tag tag = Tag::Map) {
    if (!ast.models_in) return;
    const ManagerStats& f = ast.finish;
    L::out(tag, M::map_assembled,
           {L::num(ast.t_merge + ast.t_ba + ast.t_grow + ast.finishSecs(), 2),
            (long long)ast.models_in,
            (long long)models, (long long)ast.rounds, (long long)ast.merges,
            (long long)ast.merges_refused, (long long)ast.grown_images,
            (long long)f.covered_before, (long long)f.covered_after});
    L::out(tag, M::map_finishing,
           {L::num(ast.finishSecs(), 1), (long long)f.splits, (long long)f.duplicate_splits,
            (long long)f.reseeded_models, (long long)f.dropped_redundant,
            (long long)f.audited_repaired, (long long)f.audited_out});
}

static std::vector<Reconstruction> runMapper(Mapper& mapper, const MatchesDatabase& db,
                                             const std::vector<FeatureSet>& feats, SfmConfig& cfg,
                                             AssembleStats& ast) {
    const bool bup = cfg.mapper_mode == "bottom-up" || cfg.mapper_mode == "hierarchical";
    AssembleOptions ao = cfg.assemble;
    ao.verbose = !cfg.quiet;
    std::vector<Reconstruction> models;
    if (!bup) {
        ao.tag = "map";
        models = assembleModels(mapper, mapper.run(), cfg.manager, ao, ast);
    } else {
        ao.tag = "bup";
        BottomUpStats bs;
        BottomUpOptions bo = cfg.bup;
        bo.verbose = !cfg.quiet;
        models = bottomUpReconstruct(mapper, db, feats, bo, cfg.manager, ao, bs);
        ast = bs.assemble;
    }
    // The assembly passes move images between models, so the last snapshot the
    // mapper took is not what came out. Leave the largest result on screen.
    if (!models.empty()) sfm::progress::model(models.front(), /*force=*/true);
    return models;
}

// The finishing passes: one global bundle adjustment per model releasing what
// the mapper held, then optionally another with every image on its own
// intrinsics. src/sfm/README.md, "The finishing passes", has the reasoning.
static std::vector<Reconstruction> finishModels(Mapper& mapper,
                                                std::vector<Reconstruction> models,
                                                const SfmConfig& cfg, bool verbose,
                                                double& secs) {
    const double t0 = now();
    // Nothing to release is a solve that ends where it started, and a line in
    // the log saying it ran.
    if (cfg.final_principal_point ||
        (cfg.final_extra_params && !cfg.mapper.refine_extra_params)) {
        for (Reconstruction& m : models)
            if (m.numRegistered() >= 2)
                m = mapper.polish(m, cfg.final_principal_point, cfg.final_extra_params);
        if (verbose)
            L::err(Tag::Map, M::map_final_intrinsics,
                   {(long long)models.size(), L::num(now() - t0, 1)});
    }
    if (cfg.final_per_image_intrinsics) {
        const double t1 = now();
        for (Reconstruction& m : models)
            m = mapper.perImageIntrinsics(m, cfg.final_extra_params);
        if (verbose)
            L::err(Tag::Map, M::map_per_image_done,
                   {(long long)models.size(), L::num(now() - t1, 1)});
    }
    secs = now() - t0;
    return models;
}

// Feature stems carry no extension; put the real filename (relative to the
// image dir, so sub-folders survive) back into every model for COLMAP tooling.
// The directory is walked once, not once per model -- a fragmented capture can
// produce dozens (D41).
static void resolveImageNames(std::vector<Reconstruction>& models, const std::string& imagedir) {
    if (imagedir.empty()) return;
    std::map<std::string, std::string> stem2name;
    for (const auto& e : fs::recursive_directory_iterator(imagedir))
        if (e.is_regular_file() && !isSidecar(e.path())) {
            fs::path rel = relativeTo(e.path(), imagedir);
            fs::path stem = rel;
            stem.replace_extension();
            stem2name[stem.generic_string()] = rel.generic_string();
        }
    for (Reconstruction& rec : models)
        for (auto& kv : rec.images) {
            auto it = stem2name.find(kv.second.name);
            if (it != stem2name.end()) kv.second.name = it->second;
        }
}

// Report the grouping decision, one line per camera. Worth printing in full:
// a wrong --camera-mode is otherwise invisible until the intrinsics come out
// strange, and a mixed-model capture is exactly where it goes wrong.
static void printCameraSetup(Tag tag, const CameraSetup& cs,
                             const CameraSetupOptions& sopt, size_t nimages) {
    std::map<uint32_t, size_t> counts;
    for (uint32_t id : cs.ids) counts[id]++;
    L::err(tag, M::match_camera_mode,
           {cameraModeName(cs.mode_used), (long long)cs.count(), (long long)nimages});
    if (cs.mode_switched)
        L::err(tag, M::match_camera_mode_switched,
               {(long long)cs.dim_buckets, (long long)nimages});
    if (cs.exif_focal_images)
        L::err(tag, sopt.exif_focal ? M::match_exif_focals : M::match_exif_focals_ignored,
               {(long long)cs.exif_focal_images, (long long)nimages});
    // Capped: --camera-mode image on an internet collection makes one camera
    // per image, and a thousand lines of stderr helps nobody.
    const size_t kMaxLines = 8;
    size_t shown = 0;
    for (const auto& kv : cs.cameras) {
        if (shown++ >= kMaxLines) {
            L::err(tag, M::match_more_cameras, {(long long)(cs.cameras.size() - kMaxLines)});
            break;
        }
        const Camera& c = kv.second;
        L::err(tag, M::match_camera_line,
               {(long long)kv.first, (long long)counts[kv.first], (long long)c.width,
                (long long)c.height, camInfo(c.model).cli_name, c.focal(),
                (cs.focal_known.count(kv.first)   ? M::focal_prior
                 : cs.focal_given.count(kv.first) ? M::focal_given
                                                  : M::focal_guessed).get()});
    }
}

// -----------------------------------------------------------------------
// extract
// -----------------------------------------------------------------------

// Batch-extract every image in `imagedir` to `outdir`/<stem>.bin. Shared by
// `spirula-sfm extract DIR` and `spirula-sfm auto`.
struct ExtractStats {
    size_t images = 0, failed = 0, unreadable = 0;
    uint64_t features = 0;
    // Masking (D39), all zero when no mask directory was given.
    size_t masked_images = 0;     // images that found a mask file
    size_t unmasked_images = 0;   // images that did not
    size_t mask_unreadable = 0;   // found a mask file but could not decode it
    uint64_t masked_out = 0;      // keypoints dropped by masks
    std::string first_unmasked;   // an example, for the warning
    bool warned_empty = false;    // "this mask masked out everything", warned once
};

// Warn once per distinct (mask size, image size) pair when the two disagree in
// *aspect*. A size difference is expected and handled -- masks are sampled in
// uv (D39) -- but a different aspect ratio means the mask was made for a
// differently-cropped image, and uv sampling will happily stretch it over the
// wrong content. That is a dataset bug we can see and the user cannot.
static void checkMaskShape(const std::string& mask_path, const Mask& m,
                           const std::pair<int, int>& img_dims) {
    static std::set<std::array<int, 4>> warned;
    if (m.empty() || img_dims.first <= 0 || img_dims.second <= 0) return;
    const double am = (double)m.width / m.height;
    const double ai = (double)img_dims.first / img_dims.second;
    if (std::fabs(am - ai) <= 0.01 * ai) return;
    if (!warned.insert({m.width, m.height, img_dims.first, img_dims.second}).second) return;
    L::warn(Tag::Extract, M::extract_mask_aspect,
            {mask_path, (long long)m.width, (long long)m.height,
             (long long)img_dims.first, (long long)img_dims.second});
}

// A mask that drops nearly every keypoint in the dataset is far more often
// inverted than intended. Both conventions exist in the wild -- COLMAP's (and
// ours) is white = keep, but some tools export the *excluded* region as white,
// and one of the benchmark captures does exactly that: its masks are 6% white,
// covering only the ground below the horizon, so honouring them literally
// reconstructs the floor under a moving camera and nothing else.
//
// This is a warning and not an automatic flip, because a legitimately small
// mask exists too: an object-centric capture masks away everything but the
// object. The user is the one who knows which they have.
static void warnIfMasksLookInverted(const ExtractStats& st) {
    const uint64_t before = st.features + st.masked_out;
    if (!st.masked_images || before == 0) return;
    const double dropped = (double)st.masked_out / (double)before;
    if (dropped < 0.7) return;
    L::warn(Tag::Extract, M::extract_masks_look_inverted, {(long long)(100.0 * dropped)});
}

static int extractDirectory(const std::string& imagedir, const fs::path& outdir,
                            const SfmConfig& cfg, ExtractStats& stats) {
    const SiftOptions& opt = cfg.sift;
    const std::string& maskdir = cfg.mask_dir;
    // Recursive: datasets that carry per-folder intrinsics (ppisp) keep images
    // in images/<camera>/, and the folder is the grouping key (D17). Feature
    // files mirror the tree so stems from different folders cannot collide.
    //
    // A mask directory nested inside the image directory is skipped: masks are
    // themselves PNGs, and extracting features from them would silently double
    // the image count with garbage views (`spirula-sfm auto DATASET` with the images
    // one level up is exactly the layout that trips this).
    std::error_code skip_ec;
    const bool skip_masks = !maskdir.empty() && fs::is_directory(maskdir, skip_ec);
    std::vector<fs::path> found;
    for (auto it = fs::recursive_directory_iterator(imagedir, fs::directory_options::follow_directory_symlink);
         it != fs::recursive_directory_iterator(); ++it) {
        if (skip_masks && it->is_directory() && fs::equivalent(it->path(), maskdir, skip_ec)) {
            it.disable_recursion_pending();
            continue;
        }
        if (it->is_regular_file() && isImageExt(it->path().extension().string()) &&
            !isSidecar(it->path()))
            found.push_back(it->path());
    }
    if (found.empty()) {
        L::fail(Tag::Extract, M::extract_no_images, {imagedir});
        return 1;
    }
    std::sort(found.begin(), found.end());

    // Probe every header once (the old comparator re-probed O(n log n) times),
    // both to order the batch and to size the decoder's memory.
    std::vector<fs::path> imgs;
    std::vector<std::pair<int, int>> dims;
    for (const fs::path& p : found) {
        int w = 0, h = 0;
        if (!imageSize(p.string(), w, h) || w <= 0 || h <= 0) {
            L::warn(Tag::Extract, M::extract_skipping_file,
                    {p.filename().string()});
            stats.unreadable++;
            continue;
        }
        imgs.push_back(p);
        dims.emplace_back(w, h);
    }
    if (imgs.empty()) {
        L::fail(Tag::Extract, M::extract_no_decodable, {imagedir});
        return 1;
    }

    // Largest-first so the extractor allocates device buffers exactly once.
    std::vector<size_t> order(imgs.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return (int64_t)dims[a].first * dims[a].second > (int64_t)dims[b].first * dims[b].second;
    });
    std::vector<std::string> paths(order.size());
    std::vector<std::pair<int, int>> sorted_dims(order.size());
    for (size_t k = 0; k < order.size(); k++) {
        paths[k] = imgs[order[k]].string();
        sorted_dims[k] = dims[order[k]];
    }

    fs::create_directories(outdir);

    ImageLoadOptions lopt;
    lopt.max_image_size = cfg.max_image_size;
    lopt.num_threads = cfg.decode_threads;
    lopt.want_color = true;  // sample per-keypoint colors while the image is hot
    lopt.gamut = cfg.image_gamut;
    lopt.is_linear = cfg.image_is_linear;
    lopt.flip_mask = cfg.flip_mask;
    if (cfg.decode_budget_mb > 0)
        lopt.memory_budget_bytes = (size_t)cfg.decode_budget_mb << 20;

    // Resolve each image's mask (D39) up front, in the same order as `paths`,
    // so the decode pool can pick them up. Doing it here rather than inside the
    // consumer also means a mask directory that matches nothing is reported
    // before the GPU stage burns an hour on unmasked features.
    MaskIndex masks(maskdir);
    if (!maskdir.empty() && !masks.valid())
        L::warn(Tag::Extract, M::extract_mask_dir_missing, {maskdir});
    if (masks.valid()) {
        lopt.mask_paths.assign(paths.size(), std::string());
        for (size_t k = 0; k < paths.size(); k++) {
            std::string rel = relativeTo(paths[k], imagedir).generic_string();
            lopt.mask_paths[k] = masks.find(rel);
            if (lopt.mask_paths[k].empty()) {
                stats.unmasked_images++;
                if (stats.first_unmasked.empty()) stats.first_unmasked = rel;
            } else {
                stats.masked_images++;
            }
        }
        L::out(Tag::Extract, M::extract_masks_matched,
               {(long long)stats.masked_images, (long long)paths.size(), maskdir});
        if (stats.masked_images == 0) {
            L::fail(Tag::Extract, M::extract_no_mask_matches,
                    {maskdir, stats.first_unmasked});
            return 1;
        }
        if (stats.unmasked_images)
            L::warn(Tag::Extract, M::extract_some_unmasked,
                    {(long long)stats.unmasked_images, stats.first_unmasked});
    }
    ImageLoadPlan plan = planImageLoad(sorted_dims, lopt);
    if (opt.verbose)
        L::err(Tag::Extract, M::extract_plan,
               {(long long)paths.size(), (long long)plan.num_threads,
                (long long)plan.window,
                (long long)(((size_t)plan.num_threads * plan.decode_peak_bytes +
                             (size_t)plan.window * plan.held_bytes) >> 20)});

    std::unique_ptr<IFeatureExtractor> ext =
        createFeatureExtractor(cfg.features, opt, cfg.aliked);
    if (opt.verbose) L::err(Tag::Extract, M::extract_frontend, {ext->name()});
    loadImagesInOrder(
        paths, plan, lopt,
        [&](size_t k, GrayImage& img) {
            FeatureSet f = ext->extract(img);
            sampleFeatureColors(f, img);
            uint32_t dropped = 0;
            if (!lopt.mask_paths.empty() && !lopt.mask_paths[k].empty()) {
                if (img.mask.empty()) {
                    stats.mask_unreadable++;
                    L::warn(Tag::Extract, M::extract_mask_undecodable,
                            {lopt.mask_paths[k],
                             fs::path(paths[k]).filename().string()});
                } else {
                    checkMaskShape(lopt.mask_paths[k], img.mask, sorted_dims[k]);
                    const uint32_t before = f.count();
                    dropped = applyMask(f, img.mask);
                    stats.masked_out += dropped;
                    // Masking away an entire image is what an inverted mask
                    // (or one whose keep value is 0) looks like, and it is
                    // otherwise invisible until the reconstruction is short of
                    // images. Warn once; the run still continues.
                    if (before && dropped == before && !stats.warned_empty) {
                        stats.warned_empty = true;
                        L::warn(Tag::Extract, M::extract_mask_empty,
                                {lopt.mask_paths[k],
                                 fs::path(paths[k]).filename().string()});
                    }
                }
            }
            // Back to the source file's coordinates, so cameras.bin describes
            // the images the user has rather than the extractor's working copy
            // (D46). Everything that reads a keypoint against the decoded image
            // -- the color sampling above, the mask (sampled in uv) -- has
            // already run.
            finishFeatures(f, img);
            fs::path rel = relativeTo(paths[k], imagedir);
            fs::path out = outdir / rel;
            out.replace_extension(".bin");
            fs::create_directories(out.parent_path());
            writeFeatures(out.string(), f);
            stats.features += f.count();
            stats.images++;
            if (opt.verbose) {
                const std::string name = fs::path(paths[k]).filename().string();
                if (dropped)
                    L::out(Tag::Extract, M::extract_progress_masked,
                           {(long long)stats.images, (long long)paths.size(), name,
                            (long long)f.count(), (long long)dropped});
                else
                    L::out(Tag::Extract, M::extract_progress,
                           {(long long)stats.images, (long long)paths.size(), name,
                            (long long)f.count()});
            }
        },
        [&](size_t k, const std::string& err) {
            L::fail(Tag::Extract, M::extract_failed_file,
                    {fs::path(paths[k]).filename().string(), err});
            stats.failed++;
        });
    return 0;
}

static int cmdExtract(int argc, char** argv) {
    SfmConfig cfg;
    std::set<std::string> seen;
    std::string image, output;
    for (int i = 0; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") { printCommandHelp(*findCommand("extract")); return 0; }
        if (a == "--output" || a == "-o") {
            if (i + 1 >= argc) return usageError("extract", "--output: missing value");
            output = argv[++i];
            continue;
        }
        int r = tableFlag(cfg, CMD_EXTRACT, "extract", a, argc, argv, i, seen);
        if (r < 0) return 1;
        if (r > 0) continue;
        if (a[0] == '-') return usageError("extract", "unknown option " + a);
        if (!image.empty())
            return usageError("extract", "unexpected argument '" + a + "'");
        image = a;
    }
    if (std::string err = cfg.finalize(CMD_EXTRACT); !err.empty())
        return usageError("extract", err);
    if (image.empty())
        return usageError("extract", "an image or a directory of images is required");

    // ---- directory (batch) ----
    if (fs::is_directory(image)) {
        adoptExrColorSpace(cfg, image, seen);
        fs::path outdir = output.empty() ? fs::path("features") : fs::path(output);
        ExtractStats st;
        int rc = extractDirectory(image, outdir, cfg, st);
        if (rc) return rc;
        L::out(Tag::Extract, M::extract_dir_done,
               {(long long)st.images, (unsigned long long)st.features});
        if (st.masked_out)
            L::out(Tag::Extract, M::extract_dir_masked,
                   {(unsigned long long)st.masked_out,
                    (long long)st.masked_images});
        if (st.failed || st.unreadable)
            L::out(Tag::Extract, M::extract_dir_failed,
                   {(long long)st.failed, (long long)st.unreadable});
        warnIfMasksLookInverted(st);
        return (st.failed || st.unreadable) ? 1 : 0;
    }

    // ---- single image ----
    // One image has no relative path to key on, so the mask is looked up by
    // filename -- MaskIndex's basename fallback resolves it either way.
    std::string maskpath;
    if (!cfg.mask_dir.empty()) {
        maskpath = MaskIndex(cfg.mask_dir).find(fs::path(image).filename().generic_string());
        if (maskpath.empty())
            L::warn(Tag::Extract, M::match_no_mask_for,
                    {image, cfg.mask_dir});
    }
    GrayImage img = loadGrayImage(image, cfg.max_image_size, /*want_color=*/true, maskpath,
                                  cfg.image_gamut, cfg.image_is_linear, cfg.flip_mask);
    if (cfg.sift.verbose)
        L::err(Tag::Extract, M::extract_to_gray,
               {image, img.width, img.height});
    std::unique_ptr<IFeatureExtractor> ext =
        createFeatureExtractor(cfg.features, cfg.sift, cfg.aliked);
    FeatureSet fset = ext->extract(img);
    sampleFeatureColors(fset, img);
    uint32_t masked_out = applyMask(fset, img.mask);
    finishFeatures(fset, img);
    L::out(Tag::Extract, M::extract_one_done,
           {fset.count(), fset.dim, fset.width, fset.height});
    if (fset.exif_focal > 0)
        L::out(Tag::Extract, M::extract_one_exif_focal,
               {L::num(fset.exif_focal, 1)});
    if (!img.mask.empty())
        L::out(Tag::Extract, M::extract_one_mask,
               {img.mask.width, img.mask.height, masked_out});
    if (!output.empty()) {
        writeFeatures(output, fset);
        if (cfg.sift.verbose) L::err(Tag::Extract, M::wrote_file, {output});
    }
    return 0;
}

// -----------------------------------------------------------------------
// match
// -----------------------------------------------------------------------

// Match + verify a feature directory into a MatchesDatabase. Shared by
// `spirula-sfm match` and `spirula-sfm auto`.
struct MatchStats {
    size_t images = 0, pairs = 0, kept = 0, scored = 0;
    uint64_t inliers = 0, putative = 0;
    double select_seconds = 0;
};

// Calibrated verification (D45). A fisheye pair verified on raw pixels asks a
// pinhole fundamental matrix to explain rays 100 deg off axis, which it cannot
// represent at all; the correspondences that carry the wide field of view are
// thrown away as outliers. With a camera model in hand we verify on unit
// bearings instead, where the epipolar constraint is exact at any FOV.
//
// The model needs a focal length before it can produce bearings, and the
// geometric default (diag/pi) is off by ~1.7x on a 200 deg lens, which is
// enough to warp the bearings and lose most of the benefit -- so unless one is
// given we search for it on a sample of pairs first (`bootstrapFocal`).
struct VerifyCalibration {
    CameraSetupOptions setup;
    size_t sample_pairs = 150;  // pairs per group used by the focal search
    // outputs
    CameraSetup cameras;        // what the grouping decided, for the caller to reuse
    bool used_bearings = false;
};

static int matchFeatureDir(const std::string& featdir, const SfmConfig& cfg, PairMode mode,
                           bool verify, std::vector<FeatureSet>& feats, MatchesDatabase& db,
                           MatchStats& stats, VerifyCalibration* calib = nullptr) {
    const MatchOptions& opt = cfg.match;
    const PairSelectionOptions& popt = cfg.prefilter;
    const TwoViewOptions& tvopt = cfg.twoview;
    const bool verbose = !cfg.quiet;
    // Load every features.bin under the directory (recursively -- the tree
    // mirrors the image tree), sorted by name for stable indices. The image
    // name is the relative path without ".bin", which is also COLMAP's
    // convention for `images.bin` names.
    std::vector<fs::path> files;
    for (const auto& e : fs::recursive_directory_iterator(featdir))
        if (e.is_regular_file() && e.path().extension() == ".bin" && !isSidecar(e.path()))
            files.push_back(e.path());
    std::sort(files.begin(), files.end());
    if (files.size() < 2) {
        L::fail(Tag::Match, M::match_need_two, {featdir});
        return 1;
    }

    // Read in parallel: this is a gigabyte of descriptors on a large capture,
    // and it is pure per-file work with no shared state. Results land by index,
    // so the image order is the sorted file order either way.
    feats.assign(files.size(), FeatureSet());
    db.images.resize(files.size());
    {
        const unsigned hc = std::thread::hardware_concurrency();
        int nt = cfg.threads > 0 ? cfg.threads : (hc > 0 ? (int)hc : 1);
        nt = std::max(1, std::min<int>(nt, (int)files.size()));
        std::atomic<size_t> next{0};
        std::mutex err_mtx;
        std::string first_error;  // a bad file must still report itself, not terminate
        std::vector<std::thread> pool;
        for (int t = 0; t < nt; t++)
            pool.emplace_back([&] {
                for (size_t i = next++; i < files.size(); i = next++) {
                    try {
                        feats[i] = readFeatures(files[i].string());
                    } catch (const std::exception& e) {
                        std::lock_guard<std::mutex> lk(err_mtx);
                        if (first_error.empty()) first_error = e.what();
                    }
                }
            });
        for (std::thread& t : pool) t.join();
        if (!first_error.empty()) {
            L::err_raw(Tag::Match, first_error);
            return 1;
        }
    }
    for (size_t i = 0; i < files.size(); i++) {
        fs::path rel = relativeTo(files[i], featdir);
        rel.replace_extension();
        db.images[i] = {rel.generic_string(), feats[i].count()};
    }
    stats.images = files.size();

    std::vector<std::pair<uint32_t, uint32_t>> pairs;
    std::function<void(size_t, size_t)> sp;
    if (verbose)
        sp = [&](size_t done, size_t total) {
            // Redrawn in place, so it carries the tag itself rather than
            // going through L::err(), which always ends its line.
            fprintf(stderr, "\r%s%s", L::prefix(Tag::Match).c_str(),
                    spirula::i18n::format(M::match_pairs_scored,
                                 {(long long)done, (long long)total}).c_str());
        };
    if (mode == PairMode::Prefilter) {
        stats.scored = files.size() * (files.size() - 1) / 2;
        double t0 = now();
        pairs = prefilterPairs(feats, popt, sp);
        stats.select_seconds = now() - t0;
        if (verbose)
            fprintf(stderr, "\n");
            L::err(Tag::Match, M::match_prefilter_kept,
                   {(long long)pairs.size(), (long long)stats.scored,
                    popt.num_features, popt.num_neighbors,
                    L::num(stats.select_seconds, 1)});
    } else {
        pairs = generatePairs((uint32_t)files.size(), mode, cfg.overlap);
        // Loop closure. A sequential chain has no link between the start and
        // end of a walk that comes back on itself, so one weak step splits the
        // reconstruction; the pair-selection shortlist supplies the missing
        // links from image content, the way COLMAP's loop_detection does from a
        // vocabulary tree. Exhaustive already has every pair.
        if (mode == PairMode::Sequential && cfg.loop_closure && files.size() > 2) {
            const size_t seq = pairs.size();
            stats.scored = files.size() * (files.size() - 1) / 2;
            double t0 = now();
            std::vector<std::pair<uint32_t, uint32_t>> extra = prefilterPairs(feats, popt, sp);
            stats.select_seconds = now() - t0;
            pairs.insert(pairs.end(), extra.begin(), extra.end());
            std::sort(pairs.begin(), pairs.end());
            pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());
            if (verbose)
                fprintf(stderr, "\n");
                L::err(Tag::Match, M::match_loop_closure_added,
                       {(long long)(pairs.size() - seq), (long long)seq,
                        (long long)extra.size(),
                        L::num(stats.select_seconds, 1)});
        }
    }
    stats.pairs = pairs.size();
    if (verbose)
        L::err(Tag::Match, M::match_plan,
               {(long long)files.size(), (long long)pairs.size(),
                mode == PairMode::Exhaustive ? "exhaustive"
                : mode == PairMode::Sequential
                    ? (cfg.loop_closure ? "sequential + loop closure" : "sequential")
                    : "prefilter"});
    if (verbose && mode == PairMode::Prefilter)
        L::err(Tag::Match, M::match_prefilter_params,
               {popt.num_features, popt.num_neighbors});

    std::unique_ptr<IFeatureMatcher> matcher =
        createFeatureMatcher(cfg.matcher, opt, cfg.lightglue);
    if (verbose && cfg.matcher != "bruteforce")
        L::err(Tag::Match, M::match_matcher_name, {matcher->name()});
    auto matchFn = [&](size_t b, size_t e, std::vector<std::vector<FeatureMatch>>& mout) {
        matcher->matchBatch(feats, pairs, b, e, mout);
    };
    // Every 200 pairs is a line a second on a fast GPU and a line every five
    // minutes on an Apple M2 matching 8192-feature images -- which reads as a
    // hung program, in the GUI especially. Rate-limit by time instead, with
    // the count still there for anyone parsing it.
    std::function<void(size_t, size_t)> progress;
    if (verbose) {
        progress = [&, last = 0.0](size_t done, size_t total_pairs) mutable {
            const double t = now();
            if (done != total_pairs && t - last < 2.0) return;
            last = t;
            L::err(Tag::Match, M::match_progress,
                   {(long long)done, (long long)total_pairs});
        };
    }

    if (verify) {
        // Verification runs on a worker pool fed by the (serial, GPU-bound)
        // matcher -- it is the pipeline's dominant cost, see sfm/feature/Verification.h.
        VerificationOptions vopt;
        vopt.two_view = tvopt;
        vopt.num_threads = cfg.threads;
        vopt.match_batch_pairs = opt.batch_pairs;

        // Calibrated verification (D45), for fisheye captures only: see
        // VerifyCalibration. Everything else keeps the pixel path, where the
        // pinhole assumption is exact and results are long settled.
        BearingCache bc;
        if (calib) {
            CameraSetup& cs = calib->cameras;
            cs = buildCameras(db.images, feats, calib->setup);
            if (verbose) printCameraSetup(Tag::Match, cs, calib->setup, feats.size());
            // Both focal searches want the same thing: putative matches for a
            // sample of pairs, spread over the list (a prefix would sample one
            // part of the capture, since pair lists are ordered). The fisheye
            // one has to run before verification because the focal decides the
            // bearings it verifies on; the rectilinear one could run after, but
            // sharing this sample costs nothing and keeps one code path.
            const bool want_rect = cs.anyGuessedRectilinear();
            std::vector<std::pair<uint32_t, uint32_t>> sample;
            std::vector<std::vector<FeatureMatch>> sm;
            if (cs.anyWide() || want_rect) {
                const size_t want = calib->sample_pairs * std::max<size_t>(1, cs.count());
                size_t stride = std::max<size_t>(1, pairs.size() / std::max<size_t>(1, want));
                for (size_t p = 0; p < pairs.size() && sample.size() < want; p += stride)
                    sample.push_back(pairs[p]);
                std::vector<std::vector<FeatureMatch>> chunk;
                for (size_t b = 0; b < sample.size(); b += 16) {
                    size_t e = std::min(b + 16, sample.size());
                    matcher->matchBatch(feats, sample, b, e, chunk);
                    for (size_t k = b; k < e; k++) sm.push_back(std::move(chunk[k - b]));
                }
            }
            if (want_rect) {
                double t_f = now();
                bootstrapRectilinearFocals(feats, cs.ids, sample, sm, cs.cameras, cs.focal_given,
                                           cs.focal_measured, tvopt, calib->sample_pairs,
                                           cfg.threads, verbose);
                if (verbose && !cs.focal_measured.empty())
                    L::err(Tag::Match, M::focal_epipolar_search,
                           {L::num(now() - t_f, 1)});
            }
            if (cs.anyWide()) {
                bootstrapGroupFocals(feats, cs.ids, sample, sm, cs.cameras, cs.focal_given,
                                     cs.focal_measured, tvopt, calib->sample_pairs, cfg.threads,
                                     verbose);
                std::vector<Camera> percam(feats.size());
                for (size_t i = 0; i < feats.size(); i++) percam[i] = cs.cameras.at(cs.ids[i]);
                double t_b = now();
                // A mixed capture calibrates *everything*: its cross pairs have
                // a fisheye on one side, and the pixel path cannot represent
                // those at all. A wholly rectilinear capture keeps the pixel
                // path, where the pinhole assumption is exact and results are
                // long settled.
                bc = precomputeBearings(feats, percam, /*wide_only=*/!cs.mixed(), cfg.threads);
                vopt.bearings = &bc;
                calib->used_bearings = true;
                if (verbose) {
                    // The focal list is identifiers and numbers, built here and
                    // passed through the message as one argument.
                    std::string focals;
                    for (const auto& kv : cs.cameras) {
                        if (!focals.empty()) focals += ", ";
                        focals += "cam " + std::to_string(kv.first) + ": " +
                                  L::num(kv.second.focal(), 1);
                    }
                    L::err(Tag::Match, M::match_bearings,
                           {L::num(now() - t_b, 1),
                            L::num(bc.bytes() / 1048576.0, 0), focals});
                }
            }
        }
        // A focal either search measured was measured *on that group's own
        // pairs*, which is what focal_known means: the mapper's per-image sweep
        // has nothing to add to it and every reason to leave it alone. On a
        // dual-fisheye rig the sweep was seen taking a group from the 552 px
        // the two-view stage measured to 397 px on one image's 80 inliers, with
        // the joint refinement then dragging it back to 517. It is deliberately
        // *not* focal_given: the mapper's probe reconstruction still runs and
        // bundle-adjusts the value, which measured better than the raw vote.
        if (calib)
            for (uint32_t id : calib->cameras.focal_measured)
                calib->cameras.focal_known.insert(id);
        // Hand the setup on through the database (D47), so `spirula-sfm map` inherits
        // the grouping and the focals this stage measured instead of
        // re-deriving them from the inliers it is about to produce.
        if (calib) storeCameraSetup(db, calib->cameras);
        if (verbose)
            L::err(Tag::Match, M::match_verifying, {(long long)verificationThreadCount(vopt)});
        sfm::progress::begin_matching((uint32_t)feats.size(), pairs);
        db.pairs = verifyPairs(feats, pairs, matchFn, vopt, &stats.putative, progress);
        sfm::progress::flush();
        for (const TwoViewMatches& tvm : db.pairs) stats.inliers += tvm.matches.size();
    } else {
        const size_t batch = std::max(1, opt.batch_pairs);
        std::vector<std::vector<FeatureMatch>> mout;
        for (size_t b = 0; b < pairs.size(); b += batch) {
            size_t e = std::min(b + batch, pairs.size());
            matchFn(b, e, mout);
            for (size_t p = b; p < e; p++) {
                uint32_t i = pairs[p].first, j = pairs[p].second;
                std::vector<FeatureMatch>& m = mout[p - b];
                stats.putative += m.size();
                if (!m.empty()) {
                    stats.inliers += m.size();
                    db.pairs.push_back({i, j, 0, std::move(m)});
                }
                if (progress) progress(p + 1, pairs.size());
            }
        }
    }
    stats.kept = db.pairs.size();
    return 0;
}

static int cmdMatch(int argc, char** argv) {
    SfmConfig cfg;
    std::set<std::string> seen;
    std::string featdir, output;
    for (int i = 0; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") { printCommandHelp(*findCommand("match")); return 0; }
        if (a == "--output" || a == "-o") {
            if (i + 1 >= argc) return usageError("match", "--output: missing value");
            output = argv[++i];
            continue;
        }
        if (a == "--progress-dir") {
            if (i + 1 >= argc) return usageError("match", "--progress-dir: missing value");
            sfm::progress::set_dir(argv[++i]);
            continue;
        }
        if (OverrideKind kind; cameraOverrideFlag(a, kind)) {
            if (i + 1 >= argc) return usageError("match", a + ": missing value");
            std::string err;
            if (!cameraOverride(cfg, kind, argv[++i], seen, err))
                return usageError("match", err);
            continue;
        }
        int r = tableFlag(cfg, CMD_MATCH, "match", a, argc, argv, i, seen);
        if (r < 0) return 1;
        if (r > 0) continue;
        if (a[0] == '-') return usageError("match", "unknown option " + a);
        if (!featdir.empty()) return usageError("match", "unexpected argument '" + a + "'");
        featdir = a;
    }
    if (std::string err = cfg.finalize(CMD_MATCH); !err.empty())
        return usageError("match", err);
    if (featdir.empty()) return usageError("match", "a feature directory is required");

    VerifyCalibration calib;
    calib.setup = cfg.camera;
    std::vector<FeatureSet> feats;
    MatchesDatabase db;
    MatchStats stats;
    if (int rc = matchFeatureDir(featdir, cfg, cfg.pairMode(), cfg.verify, feats, db, stats,
                                 &calib))
        return rc;
    if (cfg.verify)
        L::out(Tag::Match, M::match_done_inliers,
               {(long long)stats.kept, (long long)stats.pairs,
                (unsigned long long)stats.inliers,
                (unsigned long long)stats.putative});
    else
        L::out(Tag::Match, M::match_done_raw,
               {(long long)stats.kept, (long long)stats.pairs,
                (unsigned long long)stats.inliers});
    if (!output.empty()) {
        writeMatches(output, db);
        if (!cfg.quiet) L::err(Tag::Match, M::wrote_file, {output});
    }
    return 0;
}

// -----------------------------------------------------------------------
// map
// -----------------------------------------------------------------------

static int cmdMap(int argc, char** argv) {
    SfmConfig cfg;
    std::set<std::string> seen;
    std::string matchesPath, output;
    bool audit_first = false;
    for (int i = 0; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") { printCommandHelp(*findCommand("map")); return 0; }
        if (a == "--output" || a == "-o") {
            if (i + 1 >= argc) return usageError("map", "--output: missing value");
            output = argv[++i];
            continue;
        }
        if (a == "--progress-dir") {
            if (i + 1 >= argc) return usageError("map", "--progress-dir: missing value");
            sfm::progress::set_dir(argv[++i]);
            continue;
        }
        // Before the table, which also owns "audit" -- as the assembler's
        // audit stage (--no-audit). Here the positive spelling is the one-shot
        // pass over adopted models, which is what it has always meant.
        if (a == "--audit") { audit_first = true; continue; }
        if (a == "--no-manage") {
            cfg.manager.do_merge = cfg.manager.do_grow = cfg.manager.do_reseed = false;
            cfg.manager.do_audit = cfg.manager.do_split = cfg.manager.do_duplicate_split = false;
            continue;
        }
        if (OverrideKind kind; cameraOverrideFlag(a, kind)) {
            if (i + 1 >= argc) return usageError("map", a + ": missing value");
            std::string err;
            if (!cameraOverride(cfg, kind, argv[++i], seen, err))
                return usageError("map", err);
            continue;
        }
        int r = tableFlag(cfg, CMD_MAP, "map", a, argc, argv, i, seen);
        if (r < 0) return 1;
        if (r > 0) continue;
        if (a[0] == '-') return usageError("map", "unknown option " + a);
        if (matchesPath.empty()) matchesPath = a;
        else if (cfg.feature_dir.empty()) cfg.feature_dir = a;
        else return usageError("map", "unexpected argument '" + a + "'");
    }
    if (std::string err = cfg.finalize(CMD_MAP); !err.empty()) return usageError("map", err);
    if (matchesPath.empty() || cfg.feature_dir.empty())
        return usageError("map", "a match database and a feature directory are required");

    MapperOptions& opt = cfg.mapper;
    ManagerOptions& mgopt = cfg.manager;
    const std::string& featdir = cfg.feature_dir;

    MatchesDatabase db = readMatches(matchesPath);
    std::optional<FeatureCompactionPlan> compaction;
    if (cfg.compact_unused_features) compaction.emplace(buildFeatureCompactionPlan(db));
    std::vector<FeatureSet> feats(db.images.size());
    {
        // Descriptors are skipped: matching is over, and on a 5000-image
        // capture they are several gigabytes of file that nothing downstream
        // reads. Parallel because it is pure per-file work landing by index.
        const unsigned hc = std::thread::hardware_concurrency();
        int nt = cfg.threads > 0 ? cfg.threads : (hc > 0 ? (int)hc : 1);
        nt = std::max(1, std::min<int>(nt, (int)db.images.size()));
        std::atomic<size_t> next{0};
        std::mutex err_mtx;
        std::string first_error;  // a bad file must report itself, not terminate
        std::vector<std::thread> pool;
        for (int t = 0; t < nt; t++)
            pool.emplace_back([&] {
                for (size_t i = next++; i < db.images.size(); i = next++) {
                    try {
                        FeatureSet loaded =
                            readFeatures(featdir + "/" + db.images[i].name + ".bin", false);
                        if (compaction)
                            feats[i] = compactFeatureSet(std::move(loaded),
                                                         compaction->old_to_new[i],
                                                         compaction->compact_counts[i]);
                        else
                            feats[i] = std::move(loaded);
                    } catch (const std::exception& e) {
                        std::lock_guard<std::mutex> lk(err_mtx);
                        if (first_error.empty()) first_error = e.what();
                    }
                }
            });
        for (std::thread& th : pool) th.join();
        if (!first_error.empty()) {
            L::err_raw(Tag::Map, first_error);
            return 1;
        }
    }
    if (compaction) {
        remapMatches(db, *compaction, feats);
        const FeatureCompactionStats stats = compaction->stats;
        // old_to_new is the only temporary proportional to the original row count.
        compaction.reset();
        if (opt.verbose) reportFeatureCompaction(stats);
    }

    // The camera setup, in order of authority: what the command line asked for,
    // else what verification recorded in matches.bin (D47), else derived here.
    const bool cam_args = sawCameraFlags(seen) || !cfg.camera.overrides.empty();
    CameraSetup cs;
    const bool from_db = !cam_args && loadCameraSetup(db, cs);
    if (!from_db) cs = buildCameras(db.images, feats, cfg.camera);
    if (opt.verbose) {
        printCameraSetup(Tag::Map, cs, cfg.camera, db.images.size());
        if (from_db)
            L::err(Tag::Map, M::map_camera_setup_from_db, {matchesPath});
        else if (cam_args && db.hasCameras())
            L::err(Tag::Map, M::map_camera_setup_rebuilt, {matchesPath});
    }

    // A fisheye group with no focal prior and none recorded (D45/D46/D47).
    // `spirula-sfm auto` and `spirula-sfm match` measure this before verifying, on raw putative
    // matches, and it now travels in the match database; this is the fallback
    // for a matches.bin that predates that or arrived from elsewhere. The
    // sample here is verified *inliers*, so if the verification was itself
    // calibrated the answer is pulled towards the focal it used -- which is
    // exactly why carrying the measured one is better than re-deriving it.
    if (!from_db && db.pairs.size() >= 8) {
        std::vector<std::pair<uint32_t, uint32_t>> sample;
        std::vector<std::vector<FeatureMatch>> sm;
        const size_t want = 200 * std::max<size_t>(1, cs.count());
        size_t stride = std::max<size_t>(1, db.pairs.size() / want);
        for (size_t p = 0; p < db.pairs.size() && sample.size() < want; p += stride) {
            sample.push_back({db.pairs[p].image1, db.pairs[p].image2});
            sm.push_back(db.pairs[p].matches);
        }
        bootstrapGroupFocals(feats, cs.ids, sample, sm, cs.cameras, cs.focal_given,
                             cs.focal_measured, TwoViewOptions{}, 200, 0, opt.verbose);
        for (uint32_t id : cs.focal_measured) cs.focal_known.insert(id);
    }
    opt.initial_cameras = cs.cameras;
    opt.known_focal_cameras = cs.focal_known;
    opt.given_focal_cameras = cs.focal_given;
    opt.measured_focal_cameras = cs.focal_measured;

    Mapper mapper(db, feats, opt, cs.ids);
    std::vector<Reconstruction> models;
    AssembleStats ast;
    if (cfg.resume.empty()) {
        models = runMapper(mapper, db, feats, cfg, ast);
    } else {
        // Adopt what a previous run wrote and work on it instead. The models
        // must come from this database (image ids are positions in it); adopt()
        // checks the names and says so if they do not.
        if (!readModels(cfg.resume, models, opt.verbose)) return 1;
        // point2D_idx indexes this run's feature arrays. A model written with a
        // different --compact-unused-features would index other keypoints, and
        // the mapper can only drop those observations, not recover them.
        for (const Reconstruction& m : models)
            for (const auto& kv : m.images) {
                if (!kv.second.registered || kv.first >= feats.size()) continue;
                if (kv.second.points2D.size() == feats[kv.first].count()) continue;
                L::err_raw(Tag::Map,
                           "resumed model image '" + kv.second.name + "' holds " +
                               std::to_string(kv.second.points2D.size()) +
                               " keypoints but this run's features have " +
                               std::to_string(feats[kv.first].count()) +
                               "; --compact-unused-features must match the run that wrote " +
                               cfg.resume);
                return 1;
            }
        L::out(Tag::Map, M::map_resumed,
               {(long long)models.size(),
                (long long)distinctRegistered(models),
                (long long)db.images.size()});
    }
    if (models.empty()) {
        L::fail(Tag::Map, M::map_no_model_to_work_with);
        return 1;
    }

    // Diagnostic (D45): does each model agree with the two-view geometries it
    // was built from? A model welded together at a wrong relative pose is
    // internally perfect and fails no other test, but the verified pairs that
    // span the weld do not hold, and the images fall into disconnected groups.
    if (cfg.check) {
        for (size_t i = 0; i < models.size(); i++) {
            Mapper::SplitStats ss;
            std::vector<Reconstruction> parts = mapper.splitInconsistent(
                models[i], mgopt.merge.max_reproj_error, mgopt.seam_min_pair_fraction,
                mgopt.split_min_matches, mgopt.split_min_group, &ss);
            printf("model %zu: %u images, %zu points | %zu/%zu inner pairs hold (%.0f%%)\n"
                   "    pair agreement p05/p10/p25/p50 %.2f/%.2f/%.2f/%.2f | "
                   "%zu group(s), largest %zu",
                   i, models[i].numRegistered(), models[i].points3D.size(), ss.pairs_agree,
                   ss.pairs_tested, ss.pairs_tested ? 100.0 * ss.pairs_agree / ss.pairs_tested : 0.0,
                   ss.percentile(0.05), ss.percentile(0.10), ss.percentile(0.25),
                   ss.percentile(0.50), ss.groups, ss.largest);
            if (parts.size() > 1) {
                printf(" -> would split into");
                for (const Reconstruction& p : parts) printf(" %u", p.numRegistered());
                if (ss.dropped_images) printf(" (%zu images dropped)", ss.dropped_images);
            }
            printf("\n");
            DuplicateReport dr =
                findDuplicateStructure(models[i], mgopt.duplicate, mapper.matchedPredicate());
            printf("    duplicate structure: %zu of %zu co-located pairs share no points "
                   "and never matched (%.0f%%; %zu more share none but did match)\n",
                   dr.conflicts, dr.colocated, 100.0 * dr.ratio(), dr.unmatched_but_seen);
            // The cut is reported whenever there are conflicts at all, not only
            // when the rate passes: the whole point is that the rate is not the
            // criterion (D46), so both numbers have to be visible side by side.
            if (!dr.pairs.empty()) {
                size_t dropped = 0;
                DuplicateCut cut;
                std::vector<Reconstruction> dp = splitDuplicateStructure(
                    models[i], dr, mgopt.split_min_group, &dropped, &cut,
                    mgopt.duplicate.min_fold_overlap);
                printf("    cut: %zu group(s), severs %.2f%% of co-visibility (%llu of %llu)%s",
                       cut.groups, 100.0 * cut.fraction(), (unsigned long long)cut.severed,
                       (unsigned long long)(cut.severed + cut.kept),
                       foldSplitAccepted(dr, cut, mgopt.duplicate) ? " -- folded" : " -- kept whole");
                if (dp.size() > 1) {
                    printf(" -> would split into");
                    for (const Reconstruction& p : dp) printf(" %u", p.numRegistered());
                    if (dropped) printf(" (%zu images dropped)", dropped);
                }
                printf("\n");
            }
        }
        return 0;
    }

    // Audit models that came from elsewhere before doing anything with them:
    // every image's pose is checked against the correspondence graph, and the
    // ones the model cannot support are re-registered (D44).
    if (audit_first) {
        for (size_t i = 0; i < models.size(); i++) {
            Mapper::AuditStats as;
            models[i] = mapper.audit(models[i], &as);
            printf("audit model %zu: %u images checked, %u unsupported, %u re-registered, "
                   "%u dropped -> %u images\n",
                   i, as.checked, as.unsupported, as.reregistered, as.deregistered,
                   models[i].numRegistered());
        }
    }

    {
        double t_finish = 0;
        models = finishModels(mapper, std::move(models), cfg, opt.verbose, t_finish);
    }
    printAssembly(ast, models.size());

    resolveImageNames(models, cfg.image_dir);
    // The mapper reported its own stage when run() returned; the passes that
    // assemble its models add to the same counters.
    g_map_prof.report(0, "map");

    const Reconstruction& rec = models.front();
    double mean = 0, median = 0;
    size_t nobs = 0;
    reprojStats(rec, feats, mean, median, nobs);
    L::out(Tag::Map, M::map_reconstruction_summary,
           {rec.numRegistered(), (long long)db.images.size(),
            (long long)rec.points3D.size(), L::num(mean, 3), L::num(median, 3),
            (long long)nobs});
    if (models.size() > 1) {
        L::out(Tag::Map, M::map_models_covering,
               {(long long)models.size(),
                (long long)distinctRegistered(models),
                (long long)db.images.size()});
        printExtraModels(models, feats);
    }
    orientModels(models, cfg.orient, opt.verbose);
    recolorPoints(models, cfg);
    if (!output.empty()) writeModels(models, output, opt.verbose);
    return 0;
}

// -----------------------------------------------------------------------
// merge: fold the models of a fragmented capture back together (D43)
// -----------------------------------------------------------------------

static int cmdMerge(int argc, char** argv) {
    SfmConfig cfg;
    std::set<std::string> seen;
    std::vector<std::string> inputs;
    std::string output;
    for (int i = 0; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") { printCommandHelp(*findCommand("merge")); return 0; }
        if (a == "--output" || a == "-o") {
            if (i + 1 >= argc) return usageError("merge", "--output: missing value");
            output = argv[++i];
            continue;
        }
        int r = tableFlag(cfg, CMD_MERGE, "merge", a, argc, argv, i, seen);
        if (r < 0) return 1;
        if (r > 0) continue;
        if (a[0] == '-') return usageError("merge", "unknown option " + a);
        inputs.push_back(a);
    }
    if (std::string err = cfg.finalize(CMD_MERGE); !err.empty()) return usageError("merge", err);
    if (inputs.empty()) return usageError("merge", "at least one model directory is required");
    if (output.empty() && !cfg.in_place)
        return usageError("merge", "--output is required (or --in-place)");

    const MergeOptions& mo = cfg.merge;
    std::vector<fs::path> dirs;
    for (const std::string& in : inputs)
        if (!collectModelDirs(in, dirs)) return 1;
    if (cfg.in_place) {
        if (inputs.size() != 1)
            return usageError("merge", "--in-place takes exactly one input directory");
        if (output.empty()) output = inputs[0];
    }
    // Writing merged models over their own inputs is destructive (model 0 is
    // replaced and the absorbed ones are removed), so it has to be asked for.
    if (!cfg.in_place)
        for (const fs::path& d : dirs)
            if (fs::exists(output) && fs::equivalent(fs::path(output), d.parent_path())) {
                L::fail(Tag::Merge, CM::sfm_merge_output_is_input, {output});
                return 1;
            }

    std::vector<Reconstruction> models;
    for (const fs::path& d : dirs) {
        try {
            models.push_back(Reconstruction::readBinary(d.string()));
        } catch (const std::exception& e) {
            L::fail(Tag::Merge, M::map_cannot_read, {d.string(), e.what()});
            return 1;
        }
        L::out(Tag::Merge, M::map_read_model,
               {d.string(), models.back().numRegistered(),
                (long long)models.back().points3D.size()});
    }
    if (models.size() < 2) {
        L::fail(Tag::Merge, M::merge_need_two, {(long long)models.size()});
        return 1;
    }
    const size_t covered_before = distinctRegistered(models);

    MergeSummary sum;
    models = mergeModels(std::move(models), mo, cfg.merge_ba, cfg.device, sum);

    L::out(Tag::Merge, M::merge_summary,
           {(long long)sum.before, (long long)sum.after, L::num(sum.seconds, 2),
            (long long)sum.merges, (long long)sum.refused});
    if (sum.ba_seconds > 0)
        L::out(Tag::Merge, M::merge_ba_seconds, {L::num(sum.ba_seconds, 2)});
    for (size_t i = 0; i < models.size(); i++) {
        double mean = 0, median = 0;
        size_t nobs = 0;
        modelReprojStats(models[i], mean, median, nobs);
        L::out(Tag::Merge, M::merge_model_line,
               {(long long)i, models[i].numRegistered(),
                (long long)models[i].points3D.size(), L::num(mean, 3),
                L::num(median, 3), (long long)nobs});
    }
    const size_t covered_after = distinctRegistered(models);
    if (covered_after != covered_before)
        L::out(Tag::Merge, M::merge_survived,
               {(long long)covered_after, (long long)covered_before});

    orientModels(models, cfg.orient, mo.verbose);
    recolorPoints(models, cfg);
    writeModels(models, fs::path(output), mo.verbose);
    // In place, the models that were absorbed must not stay behind as stale
    // directories claiming to be reconstructions.
    if (cfg.in_place)
        for (const fs::path& d : dirs) {
            const std::string name = d.filename().string();
            if (d.parent_path() != fs::path(output) || name.empty() ||
                name.find_first_not_of("0123456789") != std::string::npos)
                continue;
            long idx = std::stol(name);
            if (idx >= (long)models.size()) {
                fs::remove_all(d);
                L::out(Tag::Merge, M::merge_removed_absorbed, {d.string()});
            }
        }
    return 0;
}

// -----------------------------------------------------------------------
// auto: one command from an image directory to a COLMAP sparse model
// -----------------------------------------------------------------------
//
// The equivalent of COLMAP's `automatic_reconstructor`: pick sane settings from
// two knobs (quality, data type), run extract -> match -> map in one process,
// and lay the workspace out the way COLMAP does so existing tooling reads it.
// The presets themselves live in SfmConfig.cpp's applyPresets, next to the
// table they move, and report what they moved.

static int cmdAuto(int argc, char** argv) {
    SfmConfig cfg;
    std::set<std::string> seen;
    std::string imagedir, workspace;
    bool maskdir_explicit = false;
    for (int i = 0; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") { printCommandHelp(*findCommand("auto")); return 0; }
        if (a == "--output" || a == "-o") {
            if (i + 1 >= argc) return usageError("auto", "--output: missing value");
            workspace = argv[++i];
            continue;
        }
        if (a == "--progress-dir") {
            if (i + 1 >= argc) return usageError("auto", "--progress-dir: missing value");
            sfm::progress::set_dir(argv[++i]);
            continue;
        }
        if (a == "--no-masks") { cfg.mask_dir.clear(); maskdir_explicit = true; continue; }
        if (a == "--no-manage") {
            cfg.manager.do_merge = cfg.manager.do_grow = cfg.manager.do_reseed = false;
            cfg.manager.do_audit = cfg.manager.do_split = cfg.manager.do_duplicate_split = false;
            continue;
        }
        if (OverrideKind kind; cameraOverrideFlag(a, kind)) {
            if (i + 1 >= argc) return usageError("auto", a + ": missing value");
            std::string err;
            if (!cameraOverride(cfg, kind, argv[++i], seen, err))
                return usageError("auto", err);
            continue;
        }
        int r = tableFlag(cfg, CMD_AUTO, "auto", a, argc, argv, i, seen);
        if (r < 0) return 1;
        if (r > 0) continue;
        if (a[0] == '-') return usageError("auto", "unknown option " + a);
        if (!imagedir.empty()) return usageError("auto", "unexpected argument '" + a + "'");
        imagedir = a;
    }
    if (workspace.empty()) return usageError("auto", "--output WORKSPACE is required");
    maskdir_explicit = maskdir_explicit || seen.count("masks") || seen.count("mask-dir");

    // ---- presets, then the fan-out ----
    // Presets first, overrides on top: applyPresets skips anything the command
    // line already claimed, so an explicit flag always wins.
    std::vector<PresetChange> moved;
    if (std::string err = applyPresets(cfg, seen, moved); !err.empty())
        return usageError("auto", err);
    if (std::string err = cfg.finalize(CMD_AUTO); !err.empty()) return usageError("auto", err);
    const bool verbose = !cfg.quiet;

    // ---- where the images and masks are (D39/D40) ----
    // The default layout is a dataset directory holding `images/` and `masks/`,
    // which is Spirula Studio's and nerfstudio's. Pointing straight at an image
    // directory still works: `masks` is then looked for as its sibling, so
    // `spirula-sfm auto DATASET/images` and `spirula-sfm auto DATASET` behave the same.
    if (imagedir.empty()) imagedir = "images";
    if (!fs::is_directory(imagedir)) {
        L::fail(Tag::Run, M::run_not_a_directory, {imagedir});
        return 1;
    }
    {
        fs::path nested = fs::path(imagedir) / "images";
        if (fs::is_directory(nested) && !holdsImagesOutside(imagedir, nested)) {
            L::out(Tag::Run, M::run_nested_images, {imagedir, nested.string()});
            imagedir = nested.string();
        }
    }
    if (!maskdir_explicit) {
        fs::path p = fs::path(imagedir);
        if (!p.empty() && p.filename().empty()) p = p.parent_path();  // drop a trailing '/'
        fs::path sibling = p.parent_path() / "masks";
        if (fs::is_directory(sibling)) cfg.mask_dir = sibling.string();
    }
    adoptExrColorSpace(cfg, imagedir, seen);

    fs::path ws(workspace);
    fs::create_directories(ws);
    const fs::path featdir = ws / "features";
    const fs::path matchpath = ws / "matches.bin";
    // COLMAP's layout: sparse/<i> per reconstruction, sparse/0 the largest.
    const fs::path sparsedir = ws / "sparse";

    L::out(Tag::Run, M::run_header, {imagedir, workspace});
    L::out(Tag::Run, M::run_quality,
           {cfg.quality, (long long)cfg.max_image_size,
            (long long)cfg.sift.max_num_features});
    L::out(Tag::Run, M::run_data_type, {cfg.data_type});
    L::out(Tag::Run, M::run_cameras, {cfg.camera_model, cfg.camera_mode});
    if (!cfg.mask_dir.empty()) L::out(Tag::Run, M::run_masks, {cfg.mask_dir});
    // What the two knobs moved, so a surprising run is explainable from its own
    // output rather than from reading the preset table.
    for (const PresetChange& p : moved)
        L::out(Tag::Run, M::run_preset_moved, {"--" + p.flag, p.to, p.from});

    // ---- 1. extract ----
    double t0 = now();
    ExtractStats est;
    if (int rc = extractDirectory(imagedir, featdir, cfg, est)) return rc;
    double t_extract = now() - t0;
    if (est.images < 2) {
        L::fail(Tag::Run, M::run_too_few_images, {(long long)est.images});
        return 1;
    }
    warnIfMasksLookInverted(est);

    // Pairing: what --pairs says, except that "auto" only knows the image count
    // once extraction has run. Above COLMAP's exhaustive cutoff -- where COLMAP
    // switches to vocabulary-tree retrieval -- we switch to GPU pair selection
    // (sfm/feature/PairSelection.h, D35).
    //
    // The same cutoff retires the video preset's sequential pairing. A temporal
    // window is a chain, and a capture long enough to be worth this many frames
    // is long enough to come back on itself; pair selection finds those links
    // from content, at a cost that is a fraction of matching. Measured on a
    // 262-frame walk: sequential gave four models (144 / 74 / 19 / 12 images),
    // pair selection one with 254. Below the cutoff the temporal prior is still
    // the cheaper way to get the same pairs, and --loop-closure covers its blind
    // spot. `--pairs sequential` explicitly still means sequential.
    PairMode mode = cfg.pairMode();
    if ((mode == PairMode::Exhaustive || mode == PairMode::Sequential) &&
        est.images >= 100 && !seen.count("pairs")) {
        const char* was = mode == PairMode::Exhaustive ? "exhaustive" : "sequential";
        mode = PairMode::Prefilter;
        L::out(Tag::Match, M::match_switch_to_selection, {(long long)est.images, was});
    } else if (mode == PairMode::Exhaustive && est.images >= 100) {
        L::warn(Tag::Match, M::match_exhaustive_quadratic,
                {(long long)est.images, (long long)(est.images * (est.images - 1) / 2)});
    }

    // ---- 2. match + geometric verification ----
    t0 = now();
    std::vector<FeatureSet> feats;
    MatchesDatabase db;
    MatchStats mstats;
    VerifyCalibration calib;
    calib.setup = cfg.camera;
    if (int rc = matchFeatureDir(featdir.string(), cfg, mode, /*verify=*/true, feats, db, mstats,
                                 &calib))
        return rc;
    double t_match = now() - t0;
    writeMatches(matchpath.string(), db);
    // Nothing past this point reads a descriptor -- the mapper works on
    // keypoints, the correspondence graph and the per-keypoint colors -- and on
    // a large capture they are the biggest thing in the process: 8k features
    // per image at 128 bytes is a gigabyte per thousand images, held for the
    // whole of mapping for nothing.
    for (FeatureSet& fs : feats) {
        std::vector<uint8_t>().swap(fs.descriptors);
    }
    // After writeMatches, never before: the file on disk indexes the feature
    // files, which keep every row.
    if (cfg.compact_unused_features) {
        FeatureCompactionPlan plan = buildFeatureCompactionPlan(db);
        for (size_t i = 0; i < feats.size(); i++)
            feats[i] = compactFeatureSet(std::move(feats[i]), plan.old_to_new[i],
                                         plan.compact_counts[i]);
        remapMatches(db, plan, feats);
        if (verbose) reportFeatureCompaction(plan.stats);
    }

    // ---- 3. incremental mapping ----
    // The grouping and the focals the two-view stage settled on carry straight
    // into mapping: a focal it measured beats the geometric default the mapper
    // would otherwise start the group from, and every group starting from a
    // measurement is what stops small components inventing their own
    // intrinsics (D45/D46).
    MapperOptions& mapopt = cfg.mapper;
    const CameraSetup& cs = calib.cameras;
    mapopt.initial_cameras = cs.cameras;
    mapopt.known_focal_cameras = cs.focal_known;
    mapopt.given_focal_cameras = cs.focal_given;
    mapopt.measured_focal_cameras = cs.focal_measured;

    t0 = now();
    Mapper mapper(db, feats, mapopt, cs.ids);
    AssembleStats ast;
    std::vector<Reconstruction> models = runMapper(mapper, db, feats, cfg, ast);
    double t_map = now() - t0;

    {
        double t_finish = 0;
        models = finishModels(mapper, std::move(models), cfg, verbose, t_finish);
        t_map += t_finish;
    }

    resolveImageNames(models, imagedir);
    orientModels(models, cfg.orient, verbose);
    recolorPoints(models, cfg);
    writeModels(models, sparsedir, verbose);

    // The mapper reports its own breakdown when `run()` returns; the passes
    // that assemble its models accumulate into the same counters.
    g_map_prof.report(t_map, "map");

    // ---- report ----
    const Reconstruction& rec = models.front();
    double mean = 0, median = 0;
    size_t nobs = 0;
    reprojStats(rec, feats, mean, median, nobs);
    const uint32_t reg = rec.numRegistered();
    L::out(Tag::Run, M::sum_header);
    L::out(Tag::Run, M::sum_extract,
           {L::num(t_extract, 2), (long long)est.images, (long long)est.features});
    if (est.masked_images) {
        const uint64_t before = est.features + est.masked_out;
        L::out(Tag::Run, M::sum_masks,
               {(long long)est.masked_images, (long long)est.images,
                (long long)est.masked_out,
                L::num(before ? 100.0 * est.masked_out / before : 0.0, 1)});
    }
    L::out(Tag::Run, M::sum_match,
           {L::num(t_match, 2), (long long)mstats.kept, (long long)mstats.pairs,
            (long long)mstats.inliers, (long long)mstats.putative});
    L::out(Tag::Run, M::sum_map,
           {L::num(t_map, 2), (long long)reg, (long long)est.images,
            (long long)rec.points3D.size(), (long long)rec.cameras.size()});
    printAssembly(ast, models.size(), Tag::Run);
    printFolderCoverage(models, db);
    L::out(Tag::Run, M::sum_total, {L::num(t_extract + t_match + t_map, 2)});
    L::out(Tag::Run, M::sum_model_error,
           {L::num(mean, 3), L::num(median, 3), (long long)nobs});
    if (models.size() > 1) {
        // A fragmented capture: sparse/0 is the largest component, the rest are
        // separate reconstructions with no known transform between them (D41).
        L::out(Tag::Run, M::sum_components,
               {(long long)models.size(), (long long)distinctRegistered(models),
                (long long)est.images});
        printExtraModels(models, feats);
    }
    L::out(Tag::Run, M::sum_written,
           {models.size() > 1
                ? sparsedir.string() + "/{0.." + std::to_string(models.size() - 1) + "}"
                : (sparsedir / "0").string()});

    // Verdict, so a batch run can be scanned without reading every number.
    // Thresholds are deliberately loose -- this flags "obviously broken", not
    // "not as good as COLMAP".
    const double frac = est.images ? (double)reg / est.images : 0.0;
    if (reg < 2 || rec.points3D.empty()) {
        L::out(Tag::Run, M::result_failed);
        return 2;
    }
    if (frac < 0.5 || mean > 2.0) {
        L::out(Tag::Run, M::result_partial, {L::num(100 * frac, 0), L::num(mean, 2)});
        return 3;
    }
    L::out(Tag::Run, M::result_ok, {L::num(100 * frac, 0), L::num(mean, 2)});
    return 0;
}

int spirula_sfm_main(int argc, char** argv) {
    app::set_program_name(argc > 0 ? argv[0] : nullptr, "spirula sfm");
    kProgram = app::program_name().c_str();
    if (argc < 2) {
        printTopHelp(stderr);
        return 1;
    }
    // Accept `--flag=value` as well as `--flag value`, everywhere. Only tokens
    // that start with `--` are split, and only at their first `=`, so a value
    // that itself contains one (`--camera-model cam0=opencv-fisheye`) is
    // untouched. Every subcommand parses positionally, so this is the one place
    // that has to know.
    std::vector<std::string> store;
    std::vector<char*> args;
    store.reserve((size_t)argc * 2);
    args.reserve((size_t)argc * 2);
    for (int i = 0; i < argc; i++) {
        std::string a = argv[i];
        size_t eq = a.find('=');
        if (i >= 2 && a.size() > 2 && a[0] == '-' && a[1] == '-' && eq != std::string::npos &&
            eq > 2) {
            store.push_back(a.substr(0, eq));
            store.push_back(a.substr(eq + 1));
        } else {
            store.push_back(std::move(a));
        }
    }
    for (std::string& a : store) args.push_back(&a[0]);
    argc = (int)args.size();
    argv = args.data();

    std::string cmd = argv[1];
    if (cmd == "--help" || cmd == "-h" || cmd == "help") {
        // `spirula-sfm help <command>` is the same as `<command> --help`.
        if (argc > 2) {
            if (std::string(argv[2]) == "ba") { printBaHelp(stdout); return 0; }
            if (const CommandInfo* c = findCommand(argv[2])) { printCommandHelp(*c); return 0; }
            std::fprintf(stderr, "%s: error: unknown command '%s'\n", kProgram, argv[2]);
            return 1;
        }
        printTopHelp(stdout);
        return 0;
    }
    if (cmd == "--version" || cmd == "-V" || cmd == "version") {
        std::printf("%s %s\n", kProgram, SS_VERSION);
        return 0;
    }
    // One catch for every subcommand. Setup failures throw rather than return
    // -- a checkpoint that will not download, a matcher handed the wrong kind
    // of descriptor -- and those messages are written to be read by the person
    // who typed the command, not by a terminate handler.
    try {
        if (cmd == "auto") return cmdAuto(argc - 2, argv + 2);
        if (cmd == "extract") return cmdExtract(argc - 2, argv + 2);
        if (cmd == "match") return cmdMatch(argc - 2, argv + 2);
        if (cmd == "map") return cmdMap(argc - 2, argv + 2);
        if (cmd == "merge") return cmdMerge(argc - 2, argv + 2);
        if (cmd == "ba") return cmdBa(argc - 2, argv + 2);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s %s: error: %s\n", kProgram, cmd.c_str(), e.what());
        return 1;
    }
    std::fprintf(stderr, "%s: error: unknown command '%s'\n", kProgram, cmd.c_str());
    std::fprintf(stderr, "%s\n",
                 spirula::i18n::format(
                     CM::usage_try_help,
                     {std::string(kProgram) + " --help"}).c_str());
    return 1;
}

// Pipeline.cpp -- the stages of a run, and `auto`'s ordering of them.
//
// This is the half of the old sfm_main.cpp that is not argument parsing: what
// a stage does, in what order, and what it reports. It lives here so the CLI
// and an in-process front end drive one implementation rather than two
// (docs/notes/sfm-in-process-plan.md).
//
// Every stage still reads and writes the same files, so any one of them can
// still be replaced by COLMAP's equivalent to bisect a failure.

#include "sfm/Pipeline.h"

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
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "core/ColorSpace.h"
#include "core/Env.h"
#include "core/ExrImage.h"
#include "sfm/core/Cancel.h"
#include "sfm/core/Events.h"
#include "sfm/core/Progress.h"
#include "sfm/core/CameraSetup.h"
#include "sfm/core/FeatureCompaction.h"
#include "sfm/core/Log.h"
#include "sfm/core/Features.h"
#include "sfm/core/Image.h"
#include "sfm/core/ImageLoader.h"
#include "sfm/core/Manifest.h"
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
#include "sfm/map/MetricGauge.h"
#include "sfm/map/Orient.h"
#include "sfm/map/SensorGauge.h"
#include "sfm/map/Merge.h"

#include "i18n/catalog/Sfm.h"

namespace fs = std::filesystem;

namespace sfm {

namespace L = sfm::slog;
namespace M = spirula::i18n::msg::sfm;
using sfm::slog::Tag;

bool isImageExt(const std::string& e) {
    std::string s;
    for (char c : e) s += (char)std::tolower((unsigned char)c);
    return s == ".jpg" || s == ".jpeg" || s == ".png" || s == ".bmp" || s == ".tga" ||
           s == ".ppm" || s == ".pgm" || s == ".exr";
}

// macOS AppleDouble sidecars (`._<name>`, written on exFAT / NTFS / SMB) keep
// the original's extension, so `._00333.jpg` enumerates as an image and
// `._00168.bin` stops the reconstruction on a magic the reader cannot place.
bool isSidecar(const fs::path& p) {
    const std::string n = p.filename().string();
    return n.size() > 2 && n[0] == '.' && n[1] == '_';
}

// Does `root` hold an image outside `nested`? Reinterpreting `auto DATASET`
// as `auto DATASET/images` is allowed only when it cannot lose one: otherwise
// a capture of two folders loses half, under names the trainer cannot resolve.
bool holdsImagesOutside(const fs::path& root, const fs::path& nested) {
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

// Path of `p` relative to the directory it was enumerated from. NOT
// fs::relative(): it resolves symlinks, so a directory of symlinked images
// relativizes to "../.." and `extract` writes features outside -o.
fs::path relativeTo(const fs::path& p, const fs::path& root) {
    fs::path r = root;
    // "dir/" iterates to "dir/x.jpg" but has a trailing empty element, which
    // lexically_relative would mismatch into "../x.jpg". Drop it.
    if (!r.empty() && r.filename().empty()) r = r.parent_path();
    return p.lexically_relative(r);
}

// Wall-clock seconds since an arbitrary epoch, for the stage timings `auto`
// reports.
double now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// Mean/median reprojection error over a reconstruction's observations. The one
// number that says whether a model is sane; reported by both `map` and `auto`.
void reprojStats(const Reconstruction& rec, const std::vector<FeatureSet>& feats,
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
void sampleFeatureColors(FeatureSet& fs, const GrayImage& img) {
    if (!img.hasColor()) return;
    fs.colors.resize((size_t)fs.count() * 3);
    for (uint32_t i = 0; i < fs.count(); i++)
        sampleColor(img, fs.keypoints[i].x, fs.keypoints[i].y, &fs.colors[(size_t)i * 3]);
}

// Put a freshly extracted set into the source image's frame and attach what
// EXIF said about the camera. Called once per image, after anything that
// indexes the *decoded* image (colors, masks) is done.
void finishFeatures(FeatureSet& fs, const GrayImage& img) {
    scaleKeypoints(fs, img.orig_width, img.orig_height);
    fs.exif_focal = exifFocalPx(img.exif, fs.width, fs.height);
    fs.exif_camera = exifCameraKey(img.exif, fs.width, fs.height);
}

// Why a metric fit was refused, with the numbers, so one line is a complete
// bug report.
std::string metricReason(const MetricFit& f) {
    switch (f.reason) {
        case MetricFail::Pairs:
            return spirula::i18n::format(M::metric_fail_pairs, {(long long)f.n});
        case MetricFail::Spread:
            return spirula::i18n::format(M::metric_fail_spread, {L::num(f.spread, 3)});
        case MetricFail::Inliers:
            return spirula::i18n::format(M::metric_fail_inliers,
                                {L::num(f.max_error, 3), (long long)f.inliers,
                                 (long long)f.n});
        case MetricFail::Collinear:
            return spirula::i18n::format(
                M::metric_fail_collinear,
                {L::num(100.0 * f.perp_frac, 2), L::num(100.0 * kMetricMinPerpFraction, 1),
                 L::num(f.perp_frac > 0 ? 1.0 / f.perp_frac : 0.0, 0)});
        case MetricFail::None: break;
    }
    return {};
}

// A telemetry file read once per run, with the queries the gauge fit makes.
struct LoadedCapture {
    SensorCapture cap;
    SensorTimeline timeline;
};

std::vector<std::unique_ptr<LoadedCapture>> loadCaptures(const SfmConfig& cfg, bool verbose) {
    std::vector<std::unique_ptr<LoadedCapture>> out;
    if (cfg.sensor_gauge == "none") return out;
    for (const TelemetryInput& in : cfg.telemetry_inputs) {
        Telemetry t;
        std::string err;
        if (!telemetry_read(in.path, t, err) || t.empty()) {
            L::warn(Tag::Orient, M::sensor_file_bad, {in.path, err.empty() ? "-" : err});
            continue;
        }
        const TelemetryCheck c = telemetry_check(t);
        auto lc = std::make_unique<LoadedCapture>();
        if (!lc->timeline.init(t, c, err)) {
            L::warn(Tag::Orient, M::sensor_file_bad, {in.path, err});
            continue;
        }
        lc->cap.prefix = in.prefix;
        lc->cap.path = in.path;
        lc->cap.camera = t.camera;
        lc->cap.fps = in.fps > 0 ? in.fps : t.video_fps;
        lc->cap.time_offset = in.time_offset;
        lc->cap.readout = t.frame_readout;
        lc->cap.timeline = &lc->timeline;
        L::out(Tag::Orient, M::sensor_file,
               {in.path, t.camera.empty() ? "?" : t.camera, L::num(c.gyro.rate_hz, 0),
                L::num(c.accel.rate_hz, 0), L::num(c.orientation.rate_hz, 0),
                (long long)c.gps_distinct, L::num(c.gps_path_m, 0)});
        if (!(lc->cap.fps > 0)) {
            L::warn(Tag::Orient, M::sensor_file_no_fps, {in.path});
            continue;
        }
        if (verbose)
            for (const std::string& w : c.warnings) L::err_raw(Tag::Orient, w);
        out.push_back(std::move(lc));
    }
    return out;
}

const spirula::i18n::Msg& extrinsicReason(ExtrinsicFail f) {
    switch (f) {
        case ExtrinsicFail::Frames: return M::sensor_calib_fail_frames;
        case ExtrinsicFail::Pairs: return M::sensor_calib_fail_pairs;
        case ExtrinsicFail::Disagree: return M::sensor_calib_fail_disagree;
        case ExtrinsicFail::NoStream: return M::sensor_calib_fail_nostream;
        case ExtrinsicFail::Degenerate: return M::sensor_calib_fail_degenerate;
        case ExtrinsicFail::None: break;
    }
    return M::sensor_calib_fail_pairs;
}

const spirula::i18n::Msg& noScaleReason(SensorNoScale r) {
    switch (r) {
        case SensorNoScale::NotAsked: return M::sensor_no_scale_not_asked;
        case SensorNoScale::ImuWeak: return M::sensor_no_scale_imu_weak;
        case SensorNoScale::GpsRefused: return M::sensor_no_scale_gps_refused;
        case SensorNoScale::Disagree: return M::sensor_no_scale_disagree;
        case SensorNoScale::NoSource:
        case SensorNoScale::None: break;
    }
    return M::sensor_no_scale_no_source;
}

void reportSensorGauge(size_t i, const SensorGaugeResult& r,
                       const std::vector<std::unique_ptr<LoadedCapture>>& caps, bool verbose) {
    const long long model = (long long)i;
    if (r.fail == SensorFail::NoFrames || r.fail == SensorFail::NoTelemetry) {
        L::err(Tag::Orient, M::sensor_declined, {model, M::sensor_fail_frames.get()});
        return;
    }
    if (r.frames_untimed > 0)
        L::err(Tag::Orient, M::sensor_untimed,
               {model, (long long)r.frames_untimed, (long long)(r.frames_untimed + r.frames_timed)});
    for (size_t c = 0; c < r.time_offsets.size() && c < caps.size(); c++)
        if (r.time_offsets[c].found)
            L::err(Tag::Orient, M::sensor_time_offset,
                   {caps[c]->cap.path, L::num(1000.0 * r.time_offsets[c].offset, 1),
                    (long long)r.time_offsets[c].pairs});
    for (const SensorGroupReport& g : r.groups) {
        const std::string name = g.name.empty() ? std::string(".") : g.name;
        if (!g.fit.ok) {
            L::err(Tag::Orient, M::sensor_calib_failed, {name, extrinsicReason(g.fit.reason).get()});
            continue;
        }
        if (verbose) {
            L::err(Tag::Orient, M::sensor_calib,
                   {name, (long long)g.fit.frames, L::num(g.fit.sig_rot_deg, 2),
                    L::num(g.fit.sig_grav_deg, 2)});
            if (g.triples > 0)
                L::err(Tag::Orient, M::sensor_calib_scale,
                       {name, L::num(g.scale, 6), L::num(100.0 * g.scale_sigma, 2),
                        (long long)g.triples, L::num(g.g_norm, 2), L::num(g.g_angle_deg, 1)});
        }
        if (g.fit.mirrored && !g.fit.degenerate)
            L::err(Tag::Orient, M::sensor_calib_mirrored, {name});
        if (g.fit.degenerate) L::err(Tag::Orient, M::sensor_calib_yaw_free, {name});
    }
    if (r.up_from_imu)
        L::out(Tag::Orient, M::sensor_up,
               {model, (long long)r.up.votes, L::num(r.up.spread_deg, 2), (long long)r.up.outliers});
    if (r.fail == SensorFail::NoUp) {
        L::err(Tag::Orient, M::sensor_declined, {model, M::sensor_fail_noup.get()});
        return;
    }
    if (r.scale_imu_sigma > 0) {
        int triples = 0;
        double g_norm = 0, g_angle = 0;
        for (const SensorGroupReport& g : r.groups) {
            triples += g.triples;
            if (g.triples > 0 && g.g_norm > 0) { g_norm = g.g_norm; g_angle = g.g_angle_deg; }
        }
        if (r.scale_from_imu || r.disagree)
            L::out(Tag::Orient, M::sensor_scale_imu,
                   {model, L::num(r.scale_imu, 6), L::num(100.0 * r.scale_imu_sigma, 2),
                    (long long)triples, L::num(g_norm, 2), L::num(g_angle, 1)});
        else
            L::err(Tag::Orient, M::sensor_scale_imu_weak,
                   {model, L::num(100.0 * r.scale_imu_sigma, 1), (long long)triples});
    }
    if (r.gps_frames > 0) {
        if (r.gps.ok)
            L::out(Tag::Orient, M::sensor_scale_gps,
                   {model, L::num(r.scale_gps, 6), L::num(100.0 * r.scale_gps_sigma, 2),
                    (long long)r.gps_frames, L::num(r.gps.max_error, 1), (long long)r.gps.inliers,
                    (long long)r.gps.n, L::num(r.gps.rms, 2)});
        else
            L::err(Tag::Orient, M::sensor_scale_gps_failed, {model, metricReason(r.gps)});
    }
    if (r.disagree)
        L::warn(Tag::Orient, M::sensor_disagree, {model, L::num(r.scale_imu, 6), L::num(r.scale_gps, 6)});
    if (!r.applied) return;
    if (!r.metric) {
        L::out(Tag::Orient, M::sensor_up_only,
               {model, L::num(r.T.scale, 4), noScaleReason(r.no_scale).get()});
        return;
    }
    const spirula::i18n::Msg& src = r.scale_from_imu && r.scale_from_gps ? M::sensor_src_both
                                    : r.scale_from_imu                    ? M::sensor_src_imu
                                                                          : M::sensor_src_gps;
    L::out(Tag::Orient, M::sensor_done,
           {model, src.get(), L::num(r.T.scale, 6), L::num(100.0 * r.scale_sigma, 2),
            L::num(r.tilt_sigma_deg, 2)});
}

// The gauge every finished model is written in: the video's sensors when
// named and the fit holds, else a metric reference when given and fitted,
// else the orient frame. False when a metric frame was asked for and missed.
bool fixGauge(std::vector<Reconstruction>& models, const SfmConfig& cfg,
                     const std::string& imagedir, bool verbose,
                     std::vector<ModelGauge>& gauge) {
    const bool gps = cfg.metric_gps != "none";
    const bool flat = cfg.metric_gps == "horizontal";
    const bool file = !cfg.metric_positions.empty();

    // `gauge[i]` is the state, not just the record: `oriented` and `metric` say
    // what a source has already settled, and every source below reads them
    // before touching what an earlier one answered.
    gauge.assign(models.size(), ModelGauge());

    // ---- the video's own sensors -----------------------------------------
    const std::vector<std::unique_ptr<LoadedCapture>> loaded = loadCaptures(cfg, verbose);
    if (!loaded.empty()) {
        std::vector<SensorCapture> caps;
        for (const auto& lc : loaded) caps.push_back(lc->cap);
        SensorGaugeOptions opt;
        opt.mode = cfg.sensor_gauge == "up" ? SensorMode::Up : SensorMode::Auto;
        opt.gps_full = cfg.metric_gps == "full";
        opt.gps_max_error = gps ? cfg.metric_max_error : 5.0;
        opt.verbose = verbose;
        for (size_t i = 0; i < models.size(); i++) {
            const SensorGaugeResult r = fitSensorGauge(models[i], caps, opt);
            reportSensorGauge(i, r, loaded, verbose);
            if (!r.applied) continue;
            applySim3(models[i], r.T);
            gauge[i].oriented = true;
            gauge[i].up = "sensors";
            if (r.metric) {
                gauge[i].metric = true;
                gauge[i].scale = r.scale_from_imu && r.scale_from_gps ? "imu+gps"
                                 : r.scale_from_imu                   ? "imu"
                                                                     : "gps";
                gauge[i].scale_sigma = r.scale_sigma;
            }
        }
    }

    // ---- an outside metric reference --------------------------------------
    bool all = true;
    std::map<std::string, Vec3> positions;
    if (file) {
        std::string err;
        if (!readMetricPositions(cfg.metric_positions, positions, err)) {
            L::fail(Tag::Orient, M::metric_positions_bad, {cfg.metric_positions, err});
            all = false;
        }
    }
    for (size_t i = 0; i < models.size() && all && (gps || file); i++) {
        // A metric sensor frame settles the model; a second reference over it
        // could only disagree with the one already applied.
        if (gauge[i].metric) continue;
        MetricRef ref;
        if (file) {
            const MetricPairCounts pc = pairMetricRef(models[i], positions, ref);
            L::out(Tag::Orient, M::metric_matched,
                   {(long long)pc.matched, (long long)(pc.matched + pc.unmatched_model),
                    (long long)pc.unmatched_file});
        } else {
            const MetricGpsCounts gc = metricRefFromGps(models[i], imagedir, ref);
            L::out(Tag::Orient, M::metric_gps_read,
                   {(long long)gc.matched, (long long)(gc.matched + gc.no_gps),
                    (long long)gc.no_alt});
        }
        // Horizontal mode takes the tilt from the caller's up axis, so its fit
        // -- scale, heading and place -- runs in an upright frame. Where a
        // sensor already levelled the model, that frame is the one it is in.
        const Sim3 pre = flat && !gauge[i].oriented ? uprightTransform(models[i]) : Sim3{};
        for (Vec3& c : ref.centres) c = transformPoint(pre, c);
        const MetricFit fit =
            fitMetricGauge(ref, cfg.metric_max_error,
                           flat ? MetricAxes::Horizontal : MetricAxes::Full);
        // A refused fit leaves the model in the frame it came in with -- the
        // sensors', or the normalized one the fallback below writes. Applying
        // the identity it returns would still claim the metre.
        if (!fit.ok) {
            all = false;
            L::warn(Tag::Orient, M::metric_failed, {(long long)i, metricReason(fit)});
            continue;
        }
        applySim3(models[i], composeSim3(fit.T, pre));
        gauge[i].metric = true;
        gauge[i].scale = file ? "positions" : "gps";
        gauge[i].scale_sigma = fit.scale_unc;
        // Horizontal fits scale, heading and place only, so which way is up is
        // still whatever `pre` left it as: the sensors, or the cameras.
        if (!flat) {
            gauge[i].oriented = true;
            gauge[i].up = file ? "positions" : "gps";
        } else if (!gauge[i].oriented) {
            gauge[i].up = "cameras";
        }
        L::out(Tag::Orient, M::metric_done,
               {(long long)i,
                spirula::i18n::format(!gps    ? M::metric_source_positions
                                      : flat  ? M::metric_source_gps_flat
                                              : M::metric_source_gps,
                                      {}),
                L::num(fit.T.scale, 6), L::num(fit.max_error, 3), (long long)fit.inliers,
                (long long)fit.n, L::num(fit.rms, 4), L::num(fit.scale_unc, 3),
                L::num(fit.rot_unc_deg, 3)});
        if (!verbose) continue;
        // A drifting altitude offset is absorbed by the rotation as a tilt and
        // leaves the RMS looking fine; these two numbers are what show it.
        double e = 0, n = 0, u = 0;
        for (size_t k = 0; k < ref.centres.size(); k++) {
            if (!fit.inlier_mask[k]) continue;
            const Vec3 r = ref.targets[k] - transformPoint(fit.T, ref.centres[k]);
            e += r.x * r.x;
            n += r.y * r.y;
            u += r.z * r.z;
        }
        const double m = std::max(fit.inliers, 1);
        L::err(Tag::Orient, M::metric_axes,
               {L::num(std::sqrt(e / m), 4), L::num(std::sqrt(n / m), 4),
                L::num(std::sqrt(u / m), 4),
                L::num(metricUpDisagreementDeg(models[i]), 2)});
    }

    // ---- whatever no source settled ---------------------------------------
    // The only place that falls back on the cameras' mean up axis, so a model
    // something measured cannot be re-levelled by the guess it replaced.
    if (cfg.orient)
        for (size_t i = 0; i < models.size(); i++) {
            if (gauge[i].oriented || gauge[i].metric) continue;
            const Sim3 T = orientModel(models[i]);
            gauge[i].up = "cameras";
            if (verbose) L::err(Tag::Orient, M::orient_done, {(long long)i, L::num(T.scale, 4)});
        }
    return true;
}

// An EXR carries its own colour space. Reading it needs no declaration -- the
// decoder falls back to the file's own -- but --point-color and the reported
// space both do, so adopt it before any stage runs.
void adoptExrColorSpace(SfmConfig& cfg, const std::string& imagedir,
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

void reportFeatureCompaction(const FeatureCompactionStats& stats) {
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
void recolorPoints(std::vector<Reconstruction>& models, const SfmConfig& cfg) {
    if (cfg.point_color_space != "image") return;
    if (colorspace::is_identity(cfg.image_gamut, cfg.image_is_linear)) return;
    for (Reconstruction& m : models)
        for (auto& kv : m.points3D)
            colorspace::from_srgb_inplace(kv.second.rgb, 1, cfg.image_gamut,
                                          cfg.image_is_linear);
}

// A COLMAP camera *is* a frame size, and grouping buckets sizes within 2%
// (CameraSetup.h), so give each size in a group its own camera before writing.
// The parameters stay the group's: the images shared them through BA.
void splitCamerasBySize(std::vector<Reconstruction>& models,
                               const std::vector<FeatureSet>& feats) {
    uint32_t next = 0;
    for (const Reconstruction& rec : models)
        for (const auto& kv : rec.cameras) next = std::max(next, kv.first);
    // One id space over all the models: they overlap by design (D41), and a
    // merge keeps the destination's camera for an id both sides use.
    std::map<std::pair<uint32_t, uint64_t>, uint32_t> key2id;
    std::set<uint32_t> kept;  // groups that a first size took the id of
    for (Reconstruction& rec : models) {
        std::map<uint32_t, Camera> cams;
        for (auto& kv : rec.images) {
            Image& im = kv.second;
            if (!im.registered) continue;
            auto c = rec.cameras.find(im.camera_id);
            if (c == rec.cameras.end()) continue;
            int w = c->second.width, h = c->second.height;
            if (im.id < feats.size() && feats[im.id].width > 0 && feats[im.id].height > 0) {
                w = feats[im.id].width;
                h = feats[im.id].height;
            }
            const std::pair<uint32_t, uint64_t> key{
                im.camera_id, ((uint64_t)(uint32_t)w << 32) | (uint32_t)h};
            auto it = key2id.find(key);
            if (it == key2id.end())
                // The group keeps its id for the first size written with it, so
                // a capture of one frame size writes what it always did.
                it = key2id.emplace(
                    key, kept.insert(im.camera_id).second ? im.camera_id : ++next).first;
            if (!cams.count(it->second)) {
                Camera nc = c->second;
                nc.id = it->second;
                nc.width = w;
                nc.height = h;
                cams[nc.id] = nc;
            }
            im.camera_id = it->second;
        }
        if (!cams.empty()) rec.cameras = std::move(cams);
    }
}

// gauge.txt beside the model: whether +Z is up and whether a unit is a metre.
// Plain text and not hidden, because it is as much for the user reading the
// folder as for the viewer that stops guessing an up axis when it is there.
void writeGauge(const fs::path& dir, const ModelGauge& g) {
    std::ofstream f(dir / "gauge.txt", std::ios::trunc);
    if (!f) return;
    f << "# What this model's frame means, from spirula sfm.\n";
    f << "oriented " << (g.oriented ? 1 : 0) << "\n";
    f << "metric " << (g.metric ? 1 : 0) << "\n";
    f << "up " << g.up << "\n";
    f << "scale " << g.scale << "\n";
    if (g.scale_sigma > 0) f << "scale_sigma " << g.scale_sigma << "\n";
}

// Every reconstruction as <dir>/0, <dir>/1, ... (D41) -- COLMAP's layout for a
// view graph that is not connected. `sparse/0` has the most 3D points, so a
// single-model dataset still writes exactly `sparse/0`.
void writeModels(const std::vector<Reconstruction>& models, const fs::path& dir,
                 bool verbose, const std::vector<ModelGauge>& gauge) {
    for (size_t i = 0; i < models.size(); i++) {
        fs::path p = dir / std::to_string(i);
        fs::create_directories(p);
        models[i].writeBinary(p.string());
        if (i < gauge.size()) writeGauge(p, gauge[i]);
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
size_t distinctRegistered(const std::vector<Reconstruction>& models) {
    std::set<uint32_t> ids;
    for (const Reconstruction& m : models)
        for (const auto& kv : m.images)
            if (kv.second.registered) ids.insert(kv.first);
    return ids.size();
}

// One line per model beyond the first, so a fragmented capture is visible in
// the summary rather than only in the directory listing.
void printExtraModels(const std::vector<Reconstruction>& models,
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

// Registration per top-level image sub-folder: one folder per input, and an
// input that contributed nothing is a dataset silently describing half of what
// was handed over. Only printed for more than one folder.
void printFolderCoverage(const std::vector<Reconstruction>& models,
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

// Feature stems carry no extension; map every file under the images folder
// from "<relative path without extension>" to its real name.
static std::map<std::string, std::string> imageStemMap(const std::string& imagedir) {
    std::map<std::string, std::string> stem2name;
    if (imagedir.empty()) return stem2name;
    for (const auto& e : fs::recursive_directory_iterator(imagedir))
        if (e.is_regular_file() && !isSidecar(e.path())) {
            fs::path rel = relativeTo(e.path(), imagedir);
            fs::path stem = rel;
            stem.replace_extension();
            stem2name[stem.generic_string()] = rel.generic_string();
        }
    return stem2name;
}

// The unregistered list as a data file, when SS_UNREG_LOG names one: per
// folder, every image no model took. Data only -- names need no translation;
// full coverage writes nothing.
static void writeUnregisteredList(const std::vector<Reconstruction>& models,
                                  const MatchesDatabase& db,
                                  const std::string& imagedir) {
    const char* path = spirula::env("UNREG_LOG");
    if (!path || !*path) return;
    std::set<uint32_t> ids;
    for (const Reconstruction& m : models)
        for (const auto& kv : m.images)
            if (kv.second.registered) ids.insert(kv.first);
    const std::map<std::string, std::string> stem2name = imageStemMap(imagedir);
    std::map<std::string, std::vector<std::string>> missing;
    for (size_t i = 0; i < db.images.size(); i++) {
        if (ids.count(uint32_t(i))) continue;
        std::string name = db.images[i].name;
        const auto stem = stem2name.find(name);
        if (stem != stem2name.end()) name = stem->second;
        const size_t slash = name.find('/');
        missing[slash == std::string::npos ? std::string(".")
                                           : name.substr(0, slash)]
            .push_back(name);
    }
    if (missing.empty()) return;
    std::ofstream f(path, std::ios::trunc);
    if (!f) return;
    size_t unreg = 0;
    for (const auto& kv : missing) unreg += kv.second.size();
    f << "unregistered " << unreg << '/' << db.images.size() << "\n";
    for (const auto& kv : missing) {
        f << '\n' << '[' << (kv.first == "." ? "(root)" : kv.first.c_str())
          << "] " << kv.second.size() << '\n';
        for (const std::string& n : kv.second) f << n << '\n';
    }
}


// What became of the mapper's models: one line, because a capture that comes
// back in several pieces is the case a user has to be able to reason about, and
// the counts say whether that was the view graph's doing or a refused merge.
void printAssembly(const AssembleStats& ast, size_t models, Tag tag) {
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

// Flat or bottom-up, per --mapper; flat is the default and what the
// measurements are on. Either way the same schedule assembles the models (D63,
// sfm/map/Assemble.h) -- there is no separate manage stage.
std::vector<Reconstruction> runMapper(Mapper& mapper, const MatchesDatabase& db,
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
std::vector<Reconstruction> finishModels(Mapper& mapper,
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

// Feature stems carry no extension; put the real filename back into every
// model for COLMAP tooling. Walked once, not once per model -- a fragmented
// capture can produce dozens (D41).
void resolveImageNames(std::vector<Reconstruction>& models, const std::string& imagedir) {
    if (imagedir.empty()) return;
    const std::map<std::string, std::string> stem2name = imageStemMap(imagedir);
    for (Reconstruction& rec : models)
        for (auto& kv : rec.images) {
            auto it = stem2name.find(kv.second.name);
            if (it != stem2name.end()) kv.second.name = it->second;
        }
}

// Report the grouping decision, one line per camera. Worth printing in full:
// a wrong --camera-mode is otherwise invisible until the intrinsics come out
// strange, and a mixed-model capture is exactly where it goes wrong.
void printCameraSetup(Tag tag, const CameraSetup& cs,
                      const CameraSetupOptions& sopt, size_t nimages) {
    std::map<uint32_t, size_t> counts;
    for (uint32_t id : cs.ids) counts[id]++;
    L::err(tag, M::match_camera_mode,
           {cameraModeName(cs.mode_used), (long long)cs.count(), (long long)nimages});
    if (cs.mode_switched)
        L::err(tag, M::match_camera_mode_switched,
               {(long long)cs.dim_buckets, (long long)nimages});
    if (cs.size_split_groups)
        L::warn(tag, M::match_camera_size_split, {(long long)cs.size_split_groups});
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


// Warn once per distinct (mask, image) size pair that disagrees in *aspect*.
// Differing size is fine -- masks are sampled in uv (D39) -- but a differing
// aspect means a mask cut for another crop, stretched over the wrong content.
void checkMaskShape(const std::string& mask_path, const Mask& m,
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

// A mask that drops nearly every keypoint is more often inverted than meant:
// ours is white = keep, some tools export the excluded region instead. A
// warning, not a flip -- an object-centric capture masks away all but the
// object, and only the user knows which they have.
void warnIfMasksLookInverted(const ExtractStats& st) {
    const uint64_t before = st.features + st.masked_out;
    if (!st.masked_images || before == 0) return;
    const double dropped = (double)st.masked_out / (double)before;
    if (dropped < 0.7) return;
    L::warn(Tag::Extract, M::extract_masks_look_inverted, {(long long)(100.0 * dropped)});
}

int extractDirectory(const std::string& imagedir, const fs::path& outdir,
                     const SfmConfig& cfg, ExtractStats& stats) {
    const SiftOptions& opt = cfg.sift;
    const std::string& maskdir = cfg.mask_dir;
    // Recursive: per-folder intrinsics (ppisp) keep images in images/<camera>/
    // and the folder is the grouping key (D17). A mask directory nested inside
    // is skipped -- masks are PNGs too, and would double the image count with
    // garbage views.
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

    // Resolved up front (D39), in `paths` order, so the decode pool can pick
    // them up -- and so a mask directory matching nothing is reported before
    // the GPU stage burns an hour on unmasked features.
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

    events::stage_begin(Stage::Extract, (int64_t)paths.size());
    std::unique_ptr<IFeatureExtractor> ext =
        createFeatureExtractor(cfg.features, opt, cfg.aliked, cfg.loma);
    if (opt.verbose) L::err(Tag::Extract, M::extract_frontend, {ext->name()});
    loadImagesInOrder(
        paths, plan, lopt,
        [&](size_t k, GrayImage& img) {
            cancel::check();
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
                    // An inverted mask, or one whose keep value is 0, masks
                    // an image away entirely -- invisible until the model is
                    // short of images. Warn once; the run continues.
                    if (before && dropped == before && !stats.warned_empty) {
                        stats.warned_empty = true;
                        L::warn(Tag::Extract, M::extract_mask_empty,
                                {lopt.mask_paths[k],
                                 fs::path(paths[k]).filename().string()});
                    }
                }
            }
            // Back to the source file's coordinates (D46), so cameras.bin
            // describes the user's images and not the working copy. Everything
            // reading a keypoint against the decoded image has already run.
            finishFeatures(f, img);
            fs::path rel = relativeTo(paths[k], imagedir);
            fs::path out = outdir / rel;
            out.replace_extension(".bin");
            fs::create_directories(out.parent_path());
            writeFeatures(out.string(), f);
            stats.features += f.count();
            stats.images++;
            Event ev;
            ev.kind = Event::Kind::ImageExtracted;
            ev.stage = Stage::Extract;
            ev.done = stats.images;
            ev.total = (int64_t)paths.size();
            ev.name = fs::path(paths[k]).filename().string();
            ev.width = sorted_dims[k].first;
            ev.height = sorted_dims[k].second;
            ev.features = f.count();
            ev.masked = dropped;
            events::emit(ev);
            // The picture the reel draws, from the copy already in hand.
            if (progress::enabled()) {
                fs::path stem = relativeTo(paths[k], imagedir);
                stem.replace_extension();
                progress::thumbnail(stem.generic_string(), img.rgb.data(),
                                    img.width, img.height);
            }
        },
        [&](size_t k, const std::string& err) {
            L::fail(Tag::Extract, M::extract_failed_file,
                    {fs::path(paths[k]).filename().string(), err});
            stats.failed++;
        });
    events::stage_end(Stage::Extract);
    return 0;
}



int matchFeatureDir(const std::string& featdir, const SfmConfig& cfg, PairMode mode,
                    bool verify, std::vector<FeatureSet>& feats, MatchesDatabase& db,
                    MatchStats& stats, VerifyCalibration* calib) {
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
        createFeatureMatcher(cfg.matcher, opt, cfg.lightglue, cfg.loma_match);
    if (verbose && cfg.matcher != "bruteforce")
        L::err(Tag::Match, M::match_matcher_name, {matcher->name()});
    auto matchFn = [&](size_t b, size_t e, std::vector<std::vector<FeatureMatch>>& mout) {
        matcher->matchBatch(feats, pairs, b, e, mout);
    };
    // Rate-limiting the printed line lives in the CLI's event sink, which is
    // the only thing that prints it; verifyPairs emits the fraction either way.
    std::function<void(size_t, size_t)> progress;

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
        {
            std::vector<std::string> names;
            std::vector<uint32_t> nfeat;
            names.reserve(db.images.size());
            nfeat.reserve(db.images.size());
            for (const ImageEntry& im : db.images) {
                names.push_back(im.name);
                nfeat.push_back(im.num_features);
            }
            sfm::progress::live_matches_begin(names, nfeat);
        }
        events::stage_begin(Stage::Match, (int64_t)pairs.size());
        db.pairs = verifyPairs(feats, pairs, matchFn, vopt, &stats.putative, progress);
        sfm::progress::flush();
        events::stage_end(Stage::Match);
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
// ---------------------------------------------------------------------------
// `auto`
// ---------------------------------------------------------------------------

AutoResult run_auto(SfmConfig& cfg, const AutoInputs& in) {
    AutoResult r;
    std::string _imagedir = in.image_dir;
    const std::string& _workspace = in.workspace;
    const bool verbose = !cfg.quiet;

    // ---- where the images and masks are (D39/D40) ----
    // The default layout is a dataset directory holding `images/` and `masks/`,
    // which is Spirula Studio's and nerfstudio's. Pointing straight at an image
    // directory still works: `masks` is then looked for as its sibling, so
    // `spirula-sfm auto DATASET/images` and `spirula-sfm auto DATASET` behave the same.
    if (_imagedir.empty()) _imagedir = "images";
    if (!fs::is_directory(_imagedir)) {
        L::fail(Tag::Run, M::run_not_a_directory, {_imagedir});
        { r.exit_code = 1; return r; }
    }
    {
        fs::path nested = fs::path(_imagedir) / "images";
        if (fs::is_directory(nested) && !holdsImagesOutside(_imagedir, nested)) {
            L::out(Tag::Run, M::run_nested_images, {_imagedir, nested.string()});
            _imagedir = nested.string();
        }
    }
    if (!in.mask_dir_explicit) {
        fs::path p = fs::path(_imagedir);
        if (!p.empty() && p.filename().empty()) p = p.parent_path();  // drop a trailing '/'
        fs::path sibling = p.parent_path() / "masks";
        if (fs::is_directory(sibling)) cfg.mask_dir = sibling.string();
    }
    adoptExrColorSpace(cfg, _imagedir, in.explicit_flags);

    fs::path ws(_workspace);
    fs::create_directories(ws);
    const fs::path featdir = ws / "features";
    const fs::path matchpath = ws / "matches.bin";
    // COLMAP's layout: sparse/<i> per reconstruction, sparse/0 the largest.
    const fs::path sparsedir = ws / "sparse";

    L::out(Tag::Run, M::run_header, {_imagedir, _workspace});
    L::out(Tag::Run, M::run_quality,
           {cfg.quality, (long long)cfg.max_image_size,
            (long long)cfg.sift.max_num_features});
    L::out(Tag::Run, M::run_data_type, {cfg.data_type});
    L::out(Tag::Run, M::run_cameras, {cfg.camera_model, cfg.camera_mode});
    if (!cfg.mask_dir.empty()) L::out(Tag::Run, M::run_masks, {cfg.mask_dir});
    // What the two knobs moved, so a surprising run is explainable from its own
    // output rather than from reading the preset table.
    for (const PresetChange& p : in.preset_changes)
        L::out(Tag::Run, M::run_preset_moved, {"--" + p.flag, p.to, p.from});

    // ---- 1. extract ----
    double t0 = now();
    ExtractStats est;
    if (int rc = extractDirectory(_imagedir, featdir, cfg, est)) { r.exit_code = rc; return r; }
    double t_extract = now() - t0;
    if (est.images < 2) {
        L::fail(Tag::Run, M::run_too_few_images, {(long long)est.images});
        { r.exit_code = 1; return r; }
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
        est.images >= 100 && !in.explicit_flags.count("pairs")) {
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
        { r.exit_code = rc; return r; }
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
    events::stage_begin(Stage::Map, (int64_t)db.images.size());
    events::map_begin(db.images.size());
    Mapper mapper(db, feats, mapopt, cs.ids);
    AssembleStats ast;
    std::vector<Reconstruction> models = runMapper(mapper, db, feats, cfg, ast);
    double t_map = now() - t0;

    {
        double t_finish = 0;
        models = finishModels(mapper, std::move(models), cfg, verbose, t_finish);
        t_map += t_finish;
    }
    events::stage_end(Stage::Map);

    resolveImageNames(models, _imagedir);
    std::vector<ModelGauge> gauge;
    const bool auto_metric = fixGauge(models, cfg, _imagedir, verbose, gauge);
    recolorPoints(models, cfg);
    // The gauge is what a screen was missing: every snapshot before this one is
    // in the seed pair's frame, so a run watched to the end left a tilted model
    // on display until the user opened the written one.
    if (!gauge.empty()) progress::gauge(gauge[0].oriented, gauge[0].metric);
    if (!models.empty()) progress::model(models.front(), /*force=*/true);
    // Before the split: the summary reports what was estimated, and the file's
    // one camera per frame size is not that.
    const size_t n_cameras = models.empty() ? 0 : models.front().cameras.size();
    splitCamerasBySize(models, feats);
    writeModels(models, sparsedir, verbose, gauge);

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
            (long long)rec.points3D.size(), (long long)n_cameras});
    printAssembly(ast, models.size(), Tag::Run);
    printFolderCoverage(models, db);
    writeUnregisteredList(models, db, _imagedir);
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
    // Everything the exit code says, said as data. A front end reads this
    // instead of the status: 3 and 4 cannot both be reported, and this can.
    auto emitResult = [&](bool part) {
        r.registered = reg;
        r.images = est.images;
        r.points = (int64_t)rec.points3D.size();
        r.models = (int64_t)models.size();
        r.mean_reproj = mean;
        r.median_reproj = median;
        r.partial = part;
        r.metric = auto_metric;
        r.sparse_dir = sparsedir;
        Event ev;
        ev.kind = Event::Kind::Result;
        ev.stage = Stage::Finish;
        ev.registered = reg;
        ev.images = est.images;
        ev.points = (int64_t)rec.points3D.size();
        ev.models = (int64_t)models.size();
        ev.mean_reproj = mean;
        ev.partial = part;
        ev.metric = auto_metric;
        events::emit(ev);
    };
    if (reg < 2 || rec.points3D.empty()) {
        L::out(Tag::Run, M::result_failed);
        emitResult(true);
        { r.exit_code = 2; return r; }
    }
    const bool partial = frac < 0.5 || mean > 2.0;
    emitResult(partial);
    if (partial) L::out(Tag::Run, M::result_partial, {L::num(100 * frac, 0), L::num(mean, 2)});
    // A sound model in the wrong gauge is still a sound model, so the metric
    // verdict takes the exit status only when nothing about the reconstruction
    // itself claims it -- but it is always printed, and the GUI reads the line.
    if (!auto_metric) L::out(Tag::Run, M::result_not_metric);
    if (!partial && auto_metric)
        L::out(Tag::Run, M::result_ok, {L::num(100 * frac, 0), L::num(mean, 2)});
    r.exit_code = partial ? 3 : (auto_metric ? 0 : 4);
    return r;
}

// ---------------------------------------------------------------------------
// RunContext
// ---------------------------------------------------------------------------

// Restoring on destruction is what lets a front end run a second job, and what
// keeps a thrown Cancelled from leaving the sinks pointing at a dead object.
RunContext::~RunContext() {
    slog::set_sink({});
    events::set_sink({});
    cancel::set_token(nullptr);
    progress::set_dir("");
}

void RunContext::set_log(slog::Sink s) { slog::set_sink(std::move(s)); }
void RunContext::set_events(events::Sink s) { events::set_sink(std::move(s)); }
void RunContext::set_cancel(const std::atomic<bool>* flag) { cancel::set_token(flag); }
void RunContext::set_progress_dir(const std::string& dir) { progress::set_dir(dir); }

// ---------------------------------------------------------------------------
// Reading a settings list
// ---------------------------------------------------------------------------

namespace {

// --camera-model / --focal / --distortion: a bare value sets the dataset-wide
// default (which is a table field), PREFIX=VALUE names one camera group
// (which is not).
bool cameraOverrideArg(SfmConfig& cfg, OverrideKind kind, const std::string& v,
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
        case OverrideKind::Focal: cfg.focal = std::atof(v.c_str()); break;
        case OverrideKind::Distortion: cfg.distortion = v; break;
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

bool cameraOverrideName(const std::string& a, OverrideKind& kind) {
    if (a == "--camera-model") kind = OverrideKind::Model;
    else if (a == "--focal") kind = OverrideKind::Focal;
    else if (a == "--distortion") kind = OverrideKind::Distortion;
    else return false;
    return true;
}

}  // namespace

std::string parse_auto_args(const std::vector<std::string>& args, AutoRequest& out) {
    // setConfigField consumes argv the way main() gets it; this is that view.
    std::vector<char*> argv;
    argv.reserve(args.size());
    for (const std::string& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    const int argc = (int)argv.size();

    SfmConfig& cfg = out.cfg;
    std::set<std::string> seen;
    std::string imagedir, workspace, manifest_path;
    bool maskdir_explicit = false;

    for (int i = 0; i < argc; i++) {
        const std::string a = args[(size_t)i];
        if (a == "--help" || a == "-h") { out.wants_help = true; return {}; }
        if (a == "--output" || a == "-o") {
            if (i + 1 >= argc) return "--output: missing value";
            workspace = args[(size_t)++i];
            continue;
        }
        if (a == "--manifest") {
            if (i + 1 >= argc) return "--manifest: missing value";
            manifest_path = args[(size_t)++i];
            continue;
        }
        if (a == "--progress-dir") {
            if (i + 1 >= argc) return "--progress-dir: missing value";
            out.progress_dir = args[(size_t)++i];
            continue;
        }
        if (a == "--no-masks") { cfg.mask_dir.clear(); maskdir_explicit = true; continue; }
        if (a == "--no-manage") {
            cfg.manager.do_merge = cfg.manager.do_grow = cfg.manager.do_reseed = false;
            cfg.manager.do_audit = cfg.manager.do_split = cfg.manager.do_duplicate_split = false;
            continue;
        }
        if (OverrideKind kind; cameraOverrideName(a, kind)) {
            if (i + 1 >= argc) return a + ": missing value";
            std::string err;
            if (!cameraOverrideArg(cfg, kind, args[(size_t)++i], seen, err)) return err;
            continue;
        }
        std::string err;
        const FieldResult r =
            setConfigField(cfg, CMD_AUTO, a, argc, argv.data(), i, seen, err);
        if (r == FieldResult::Error) return err;
        if (r == FieldResult::Ok) continue;
        if (a[0] == '-') return "unknown option " + a;
        if (!imagedir.empty()) return "unexpected argument '" + a + "'";
        imagedir = a;
    }
    if (workspace.empty()) return "--output WORKSPACE is required";
    maskdir_explicit = maskdir_explicit || seen.count("masks") || seen.count("mask-dir");

    // Presets first, the file next, the command line over both: applyPresets
    // skips anything already claimed, and manifest_apply is told the same set.
    std::vector<PresetChange> moved;
    if (std::string err = applyPresets(cfg, seen, moved); !err.empty()) return err;
    if (!manifest_path.empty()) {
        Manifest man;
        try {
            man = manifest_read(manifest_path);
        } catch (const std::exception& e) {
            return e.what();
        }
        if (std::string err = manifest_apply(man, cfg, seen, imagedir); !err.empty())
            return manifest_path + ": " + err;
        if (!man.mask_dir.empty()) maskdir_explicit = true;
    }
    if (std::string err = cfg.finalize(CMD_AUTO); !err.empty()) return err;

    out.in.image_dir = imagedir;
    out.in.workspace = workspace;
    out.in.mask_dir_explicit = maskdir_explicit;
    out.in.explicit_flags = std::move(seen);
    out.in.preset_changes = std::move(moved);
    return {};
}

}  // namespace sfm

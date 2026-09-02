// Which images share intrinsics, and what those intrinsics start at.
//
// Three questions, deliberately kept apart because datasets answer them
// independently (COLMAP's ImageReader splits the same way):
//
//   * grouping  -- --camera-mode: one camera for everything, one per folder,
//                  one per image; plus EXIF identity and always a resolution
//                  split, because images of different sizes cannot share a
//                  principal point.
//   * model     -- --camera-model: the distortion model a group is fitted with.
//   * focal     -- --focal / EXIF: where its focal length starts.
//
// The last two are per group, not per dataset (D46). A rig that carries a
// rectilinear camera and two fisheyes is one capture with three camera models
// in it, and forcing one model on all of them makes two thirds of the rig
// unusable; a `PREFIX=VALUE` argument sets either for one group.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "i18n/catalog/Sfm.h"
#include "sfm/core/Camera.h"
#include "sfm/core/Features.h"
#include "sfm/core/Matches.h"

namespace sfm {

enum class CameraMode { Single, Folder, Image };

// The flag value plus what it means, the second half translated (the value is
// a flag spelling and is not).
inline const char* cameraModeName(CameraMode m) {
    namespace msg = spirula::i18n::msg::sfm;
    switch (m) {
        case CameraMode::Folder: return msg::camera_mode_folder.get();
        case CameraMode::Image:  return msg::camera_mode_image.get();
        default:                 return msg::camera_mode_single.get();
    }
}

inline bool parseCameraMode(const std::string& v, CameraMode& out) {
    if (v == "single" || v == "auto") out = CameraMode::Single;  // `auto` = the old name
    else if (v == "folder") out = CameraMode::Folder;
    else if (v == "image") out = CameraMode::Image;
    else return false;
    return true;
}

// A per-group setting. `prefix` matches an image name (the relative path under
// the image/feature directory) by path prefix: "cam0" matches cam0/00017 and
// cam0/sub/x, and nothing else. An empty prefix is the dataset-wide default.
struct CameraOverride {
    std::string prefix;
    bool has_model = false;
    CamModel model = CamModel::OpenCV;
    bool has_focal = false;
    double focal = 0;
    // Starting distortion coefficients, in the model's BA order
    // (sfm/core/Camera.h packIntrinsics). Empty = start at zero.
    bool has_extra = false;
    std::vector<double> extra;
};

// Which of the three per-group settings a `PREFIX=VALUE` argument carries.
enum class OverrideKind { Model, Focal, Distortion };

// "k1,k2,..." -> coefficients in the camera model's BA order. False on an empty
// list or any token that is not wholly a number.
inline bool parseDistortion(const std::string& v, std::vector<double>& out) {
    out.clear();
    if (v.empty()) return false;
    for (size_t i = 0;;) {
        size_t c = v.find(',', i);
        if (c == std::string::npos) c = v.size();
        const std::string tok = v.substr(i, c - i);
        try {
            size_t used = 0;
            out.push_back(std::stod(tok, &used));
            if (used != tok.size()) return false;
        } catch (const std::exception&) {
            return false;
        }
        if (c == v.size()) break;
        i = c + 1;
    }
    return true;
}

// Parse "VALUE" or "PREFIX=VALUE" into `out`, merging into an existing entry
// for the same prefix so --camera-model, --focal and --distortion can name the
// same group. A distortion value is a comma-separated list.
inline bool parseCameraOverride(const std::string& arg, OverrideKind kind,
                                std::vector<CameraOverride>& out) {
    std::string prefix, value = arg;
    size_t eq = arg.find('=');
    if (eq != std::string::npos) {
        prefix = arg.substr(0, eq);
        value = arg.substr(eq + 1);
        while (!prefix.empty() && (prefix.back() == '/' || prefix.back() == '\\')) prefix.pop_back();
    }
    CamModel m = CamModel::OpenCV;
    double f = 0;
    std::vector<double> extra;
    if (kind == OverrideKind::Focal) {
        try {
            f = std::stod(value);
        } catch (const std::exception&) {
            return false;
        }
        if (!(f > 0)) return false;
    } else if (kind == OverrideKind::Distortion) {
        if (!parseDistortion(value, extra)) return false;
    } else if (!parseCamModelName(value, m)) {
        return false;
    }
    CameraOverride* e = nullptr;
    for (CameraOverride& o : out)
        if (o.prefix == prefix) { e = &o; break; }
    if (!e) {
        out.push_back(CameraOverride{});
        e = &out.back();
        e->prefix = prefix;
    }
    switch (kind) {
        case OverrideKind::Focal: e->has_focal = true; e->focal = f; break;
        case OverrideKind::Distortion: e->has_extra = true; e->extra = std::move(extra); break;
        case OverrideKind::Model: e->has_model = true; e->model = m; break;
    }
    return true;
}

struct CameraSetupOptions {
    CameraMode mode = CameraMode::Folder;
    // Applied to any image no override names. `focal <= 0` means "no prior":
    // the group starts at Camera::defaultFor's geometric guess.
    CamModel model = CamModel::OpenCV;
    double focal = 0;
    // Dataset-wide starting distortion, in the model's BA order; a group an
    // override names uses that instead. Empty = start at zero, as COLMAP does.
    std::vector<double> extra;
    std::vector<CameraOverride> overrides;
    // Use the focal length EXIF recorded (features.bin v3) when no explicit
    // --focal covers the group. On by default: it is a measurement of the lens
    // that took the picture, and the alternative is a guess that is 85% long on
    // a 24 mm full-frame frame (D46).
    bool exif_focal = true;
    // Split groups by what EXIF says the camera was, on top of --camera-mode:
    // by body (make, model, frame size) and by *focal cluster* (see
    // exifFocalClusters). On by default (D48) -- with the focal clustered
    // rather than compared exactly, this is a no-op for any capture whose lens
    // stayed put, and the only thing that saves a zoom-heavy photo collection:
    // one internet collection's 83 images carry focals from 5.4k to 60k px,
    // and sharing one camera between them registered 55 of 83 at 8.5 AUC.
    bool exif_groups = true;
    // Set when --camera-mode was given, which pins `mode`. Otherwise
    // buildCameras may switch a Folder default to Image for a set that is
    // plainly a photo collection (looksLikePhotoCollection, D48).
    bool mode_explicit = false;
    // Single-linkage tolerance for that clustering, relative. Has to exceed
    // EXIF's 1 mm focal quantization, which is 4% at 24 mm (one fixed 24 mm
    // lens can record both 24 and 25 mm and must stay one group), and stay
    // well under a real zoom step.
    double exif_focal_tol = 0.10;
};

struct CameraSetup {
    std::vector<uint32_t> ids;                  // per image, 1-based camera id
    std::map<uint32_t, Camera> cameras;         // starting intrinsics per id
    // Two different things, and the difference matters:
    //   focal_given -- the focal came from somewhere other than the geometric
    //                  guess (EXIF, or any --focal), so the two-view focal
    //                  *search* has nothing to add and must not overwrite it.
    //   focal_known -- ... and it describes *this group* (EXIF, or a --focal
    //                  that named the group), so the mapper's per-camera focal
    //                  sweep should leave it alone too. A dataset-wide --focal
    //                  is given but not known: it says nothing about which
    //                  group it describes, and the sweep is how a second camera
    //                  group departs from a value measured over the first (D45).
    std::set<uint32_t> focal_given;
    std::set<uint32_t> focal_known;             // subset of focal_given
    //   focal_measured -- the two-view stage measured it, on this group's own
    //                  pairs (D53's epipolar vote, or the fisheye peripheral
    //                  inlier search). Folded into focal_known, because a
    //                  per-image registration sweep has nothing to add to a
    //                  measurement over hundreds of pairs; deliberately *not*
    //                  into focal_given, so the mapper still builds its probe
    //                  model and lets bundle adjustment refine the value.
    std::set<uint32_t> focal_measured;
    std::map<uint32_t, std::string> labels;     // id -> the key it grouped on
    size_t exif_focal_images = 0;               // images that carried an EXIF focal
    size_t exif_camera_images = 0;              // images that carried an EXIF identity
    size_t dim_buckets = 0;                     // distinct frame sizes, 2% tolerance
    // Groups --camera-mode asked for that hold more than one frame size, and
    // were therefore split further.
    size_t size_split_groups = 0;
    CameraMode mode_used = CameraMode::Folder;  // after any automatic switch
    bool mode_switched = false;                 // ... and whether there was one

    uint32_t count() const { return (uint32_t)cameras.size(); }
    // A capture with both wide-FOV (fisheye or spherical) and rectilinear
    // groups. Verification has to treat the whole dataset as calibrated when
    // this holds, because the cross pairs have a wide camera on one side (D46).
    bool anyWide() const {
        for (const auto& kv : cameras)
            if (kv.second.wideFov()) return true;
        return false;
    }
    // A rectilinear group still on the geometric focal guess: the case the
    // epipolar focal search exists for.
    bool anyGuessedRectilinear() const {
        for (const auto& kv : cameras)
            if (!kv.second.wideFov() && !focal_given.count(kv.first)) return true;
        return false;
    }
    bool mixed() const {
        bool wide = false, rect = false;
        for (const auto& kv : cameras) (kv.second.wideFov() ? wide : rect) = true;
        return wide && rect;
    }
};

namespace detail {

// Does `prefix` name `name`, as a path prefix? "" matches everything.
inline bool cameraPrefixMatches(const std::string& name, const std::string& prefix) {
    if (prefix.empty()) return true;
    if (name.size() < prefix.size()) return false;
    if (name.compare(0, prefix.size(), prefix) != 0) return false;
    return name.size() == prefix.size() || name[prefix.size()] == '/';
}

// The longest matching prefix wins, so "cam0=..." beats a bare default and
// "rig/cam0=..." beats "rig=...".
inline const CameraOverride* cameraOverrideFor(const std::string& name,
                                               const std::vector<CameraOverride>& ovr) {
    const CameraOverride* best = nullptr;
    for (const CameraOverride& o : ovr) {
        if (!cameraPrefixMatches(name, o.prefix)) continue;
        if (!best || o.prefix.size() > best->prefix.size()) best = &o;
    }
    return best;
}

// Parent path of a '/'-separated relative name ("" for a top-level file).
inline std::string parentPath(const std::string& name) {
    size_t s = name.find_last_of('/');
    return s == std::string::npos ? std::string() : name.substr(0, s);
}

// Group focal lengths (in pixels) that are the same lens setting to within
// `tol`, and label each input with its group. Single-linkage on the sorted
// values -- a cut wherever consecutive focals differ by more than tol -- rather
// than fixed buckets: bucket edges split a pair of nearly equal focals whenever
// they happen to straddle one, and "nearly equal" is the whole question here.
// Values <= 0 (no EXIF focal) get cluster -1, which keeps them together and
// apart from everything measured.
inline std::vector<int> exifFocalClusters(const std::vector<double>& focals, double tol) {
    std::vector<size_t> order;
    for (size_t i = 0; i < focals.size(); i++)
        if (focals[i] > 0) order.push_back(i);
    std::sort(order.begin(), order.end(),
              [&](size_t a, size_t b) { return focals[a] < focals[b]; });
    std::vector<int> label(focals.size(), -1);
    int next = 0;
    for (size_t k = 0; k < order.size(); k++) {
        if (k && focals[order[k]] > focals[order[k - 1]] * (1.0 + tol)) next++;
        label[order[k]] = next;
    }
    return label;
}

}  // namespace detail

// Is this a photo *collection* rather than a capture? It matters because the
// default grouping (one camera per folder and resolution) assumes one lens per
// frame size, which a collection violates outright: images of the same size come
// from different bodies at different zooms, and forcing them to share intrinsics
// fits none of them.
//
// The signal is how many distinct frame sizes the set spans relative to its own
// size, bucketed at the same 2% that the grouping uses (so a preprocessed
// capture's jittered dimensions do not count). The separation is not marginal:
//
//   captures     Mip-NeRF 360 garden 0.005, Tanks and Temples Family 0.007,
//                KITTI 0.009, botanical_garden 0.022; four more
//                handheld/video captures fall inside that range
//   collections  three internet photo collections: 0.181, 0.184, 0.322
//
// botanical_garden (https://www.kaggle.com/datasets/simonbethke/botanical-garden-america)
// is the interesting one: 550 images that a preprocessing step
// left at ~24 slightly different sizes, all from one physical camera (D36/D40).
// The 2% bucket collapses those to 4, which is why the ratio still reads as a
// capture. The floors keep the ratio from being read off too few images.
inline bool looksLikePhotoCollection(const std::vector<FeatureSet>& feats,
                                     size_t* buckets_out = nullptr,
                                     double ratio = 0.08, size_t min_images = 20,
                                     size_t min_buckets = 4) {
    std::vector<std::pair<int, int>> reps;
    for (const FeatureSet& f : feats) {
        bool found = false;
        for (const auto& r : reps)
            if (std::abs(f.width - r.first) <= 0.02 * r.first &&
                std::abs(f.height - r.second) <= 0.02 * r.second) { found = true; break; }
        if (!found) reps.push_back({f.width, f.height});
    }
    if (buckets_out) *buckets_out = reps.size();
    if (feats.size() < min_images || reps.size() < min_buckets) return false;
    return (double)reps.size() > ratio * (double)feats.size();
}

// Group the images and give each group its starting camera.
//
// Every mode splits on resolution first (D40): images that differ in size
// cannot share a principal point, so merging them produces intrinsics that fit
// neither. Resolutions are bucketed with a 2% tolerance (D36) -- pre-processed
// datasets (per-image crops, undistortion) jitter dimensions by a few pixels,
// and treating each jitter as a new camera splits one physical camera into
// dozens of weakly-constrained groups: botanical_garden's 550 images span ~24
// such "resolutions", and per-group focal searches on mid-reconstruction
// geometry is how its default run fell apart. Genuinely different cameras
// either match exactly (same sensor) or differ far more; the <=2%
// principal-point offset this introduces is refined away by BA.
//
// Groups also never span two camera *models* or two explicit focals, whatever
// --camera-mode says: those are statements about different physical cameras.
inline CameraSetup buildCameras(const std::vector<ImageEntry>& images,
                                const std::vector<FeatureSet>& feats,
                                const CameraSetupOptions& opt) {
    CameraSetup out;
    out.ids.assign(images.size(), 1);
    // A photo collection gets per-image intrinsics whether or not the caller
    // knew to ask (D20's --data-type internet, decided from the data instead).
    CameraMode mode = opt.mode;
    if (!opt.mode_explicit && mode == CameraMode::Folder &&
        looksLikePhotoCollection(feats, &out.dim_buckets)) {
        mode = CameraMode::Image;
        out.mode_switched = true;
    } else {
        looksLikePhotoCollection(feats, &out.dim_buckets);
    }
    out.mode_used = mode;
    std::map<std::string, uint32_t> key2id;
    std::vector<std::pair<int, int>> reps;  // resolution-bucket representatives
    auto dimBucket = [&](int w, int h) {
        for (size_t b = 0; b < reps.size(); b++)
            if (std::abs(w - reps[b].first) <= 0.02 * reps[b].first &&
                std::abs(h - reps[b].second) <= 0.02 * reps[b].second)
                return b;
        reps.push_back({w, h});
        return reps.size() - 1;
    };

    std::map<uint32_t, std::vector<double>> exif_focals;  // per id, for the median
    std::map<uint32_t, std::vector<double>> px_scales;    // per id, likewise
    std::map<uint32_t, size_t> first_image;               // per id, for the frame size

    // Pass 1: the key --camera-mode, resolution and the overrides imply. The
    // EXIF split needs all of a group's focals before it can say which of them
    // are the same lens setting, so it happens between the passes.
    std::vector<std::string> base_key(images.size());
    std::map<std::string, std::set<size_t>> group_sizes;  // mode key -> buckets
    for (size_t i = 0; i < images.size(); i++) {
        const std::string& name = images[i].name;
        const CameraOverride* ovr = detail::cameraOverrideFor(name, opt.overrides);
        const size_t bucket = dimBucket(feats[i].width, feats[i].height);
        char dims[64];
        snprintf(dims, sizeof dims, "r%zu", bucket);
        std::string key, group;
        switch (mode) {
            case CameraMode::Single: key = dims; break;
            case CameraMode::Image:  key = name; break;
            // The *full* relative parent path, so nested sub-folders are
            // distinct cameras (images/rig/cam0 != images/rig/cam1 !=
            // images/rig), which is how per-camera captures are laid out.
            case CameraMode::Folder: group = detail::parentPath(name);
                                     key = group + "|" + dims; break;
        }
        if (mode != CameraMode::Image) group_sizes[group].insert(bucket);
        // An override splits the group it names off from everything else even
        // when the mode would have merged them (one folder holding two lenses).
        if (ovr && !ovr->prefix.empty()) key += "|@" + ovr->prefix;
        base_key[i] = key;
    }

    for (const auto& kv : group_sizes)
        if (kv.second.size() > 1) out.size_split_groups++;

    // Cluster each base group's EXIF focals independently: the question "is
    // this the same lens setting as that?" is only meaningful among images that
    // would otherwise have shared a camera.
    std::vector<int> focal_cluster(images.size(), -1);
    if (opt.exif_groups) {
        std::map<std::string, std::vector<size_t>> by_base;
        for (size_t i = 0; i < images.size(); i++) by_base[base_key[i]].push_back(i);
        for (const auto& kv : by_base) {
            std::vector<double> f;
            f.reserve(kv.second.size());
            for (size_t i : kv.second) f.push_back(feats[i].exif_focal);
            std::vector<int> lab = detail::exifFocalClusters(f, opt.exif_focal_tol);
            for (size_t k = 0; k < kv.second.size(); k++) focal_cluster[kv.second[k]] = lab[k];
        }
    }

    for (size_t i = 0; i < images.size(); i++) {
        const std::string& name = images[i].name;
        const CameraOverride* ovr = detail::cameraOverrideFor(name, opt.overrides);
        std::string key = base_key[i];
        if (opt.exif_groups) {
            if (!feats[i].exif_camera.empty()) key += "|" + feats[i].exif_camera;
            if (focal_cluster[i] >= 0) key += "|f" + std::to_string(focal_cluster[i]);
        }

        auto it = key2id.find(key);
        if (it == key2id.end()) {
            it = key2id.emplace(key, (uint32_t)key2id.size() + 1).first;
            out.labels[it->second] = key;
            first_image[it->second] = i;
        }
        const uint32_t id = it->second;
        out.ids[i] = id;
        if (feats[i].exif_focal > 0) {
            exif_focals[id].push_back(feats[i].exif_focal);
            out.exif_focal_images++;
        }
        if (!feats[i].exif_camera.empty()) out.exif_camera_images++;
        px_scales[id].push_back(feats[i].pixelScale());
    }

    for (const auto& kv : first_image) {
        const uint32_t id = kv.first;
        const size_t i = kv.second;
        const CameraOverride* ovr = detail::cameraOverrideFor(images[i].name, opt.overrides);
        const CamModel model = ovr && ovr->has_model ? ovr->model : opt.model;
        double focal = 0;
        bool known = false, given = false;
        if (ovr && ovr->has_focal) {
            focal = ovr->focal;
            given = true;
            known = !ovr->prefix.empty();  // see CameraSetup::focal_given
        } else if (opt.exif_focal && exif_focals.count(id)) {
            // The median over the group: a zoom lens left on one setting still
            // records a couple of neighbouring focal lengths, and one odd frame
            // must not move the group's start.
            std::vector<double>& v = exif_focals[id];
            std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
            focal = v[v.size() / 2];
            given = known = true;
        } else if (opt.focal > 0) {
            focal = opt.focal;   // dataset-wide: given, not known
            given = true;
        }
        out.cameras[id] = Camera::defaultFor(id, feats[i].width, feats[i].height, focal, model);
        if (ovr && ovr->has_extra) setExtraParams(out.cameras[id], ovr->extra);
        else if (!opt.extra.empty()) setExtraParams(out.cameras[id], opt.extra);
        // A spherical camera has no focal length: the image dimensions are the
        // calibration, exactly, so it counts as known however the run was
        // invoked. That is what keeps both focal searches (the two-view one
        // here and the mapper's trial reconstructions) off it (D49).
        if (out.cameras[id].isSpherical()) given = known = true;
        // The group's measurement scale (Camera::pixel_scale). The median, for
        // the same reason as the focal: one odd frame must not move the group.
        std::vector<double>& ps = px_scales[id];
        if (!ps.empty()) {
            std::nth_element(ps.begin(), ps.begin() + ps.size() / 2, ps.end());
            out.cameras[id].pixel_scale = std::max(1.0, ps[ps.size() / 2]);
        }
        if (given) out.focal_given.insert(id);
        if (known) out.focal_known.insert(id);
    }
    return out;
}

// ---- carrying the setup through matches.bin (D47) -------------------------
//
// The two-view stage is where a fisheye focal is actually measured, on raw
// putative matches. Handing that measurement to the mapper through the match
// database means `spirula-sfm map` never has to guess it back: its only alternative is
// to re-run the search on the *verified inliers*, which were selected by
// whatever focal did the verifying, so the answer is pulled towards it.

inline void storeCameraSetup(MatchesDatabase& db, const CameraSetup& cs) {
    db.cameras.clear();
    db.focal_prior.clear();
    for (const auto& kv : cs.cameras) {
        db.cameras.push_back(kv.second);
        db.focal_prior.push_back(cs.focal_known.count(kv.first) ? 1 : 0);
    }
    db.camera_ids = cs.ids;
}

// The inverse. False (leaving `cs` untouched) when the file carries nothing.
inline bool loadCameraSetup(const MatchesDatabase& db, CameraSetup& cs) {
    if (!db.hasCameras()) return false;
    cs = CameraSetup();
    cs.ids = db.camera_ids;
    for (size_t i = 0; i < db.cameras.size(); i++) {
        const Camera& cam = db.cameras[i];
        cs.cameras[cam.id] = cam;
        const bool prior = i < db.focal_prior.size() && db.focal_prior[i];
        // A focal that differs from the geometric default was measured -- by the
        // two-view search, by EXIF, or by hand -- and the search has nothing to
        // add to it. One that does not is still a guess, and is reported as one.
        const Camera def = Camera::defaultFor(cam.id, cam.width, cam.height, 0, cam.model);
        if (prior || std::fabs(cam.focal() - def.focal()) > 1e-6) cs.focal_given.insert(cam.id);
        if (prior) cs.focal_known.insert(cam.id);
    }
    return true;
}

}  // namespace sfm

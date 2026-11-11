#pragma once

// Points for a model whose poses are known: the verified matches joined into
// tracks and triangulated with the poses as they stand -- COLMAP's
// point_triangulator. Nothing here moves a pose; a bundle adjustment started
// from them does that, so no image is ever registered against the others.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "sfm/core/Features.h"
#include "sfm/core/Matches.h"
#include "sfm/core/Model.h"
#include "sfm/core/Pose.h"
#include "sfm/geometry/Essential.h"
#include "sfm/geometry/Triangulation.h"
#include "sfm/map/GlobalTracks.h"

namespace sfm {

// A database image the model does not hold.
inline constexpr uint32_t kNotInModel = UINT32_MAX;

struct KnownPoseOptions {
    double max_error_px = 4.0;     // extraction pixels, as --max-error
    double min_angle_deg = 1.5;    // the mapper's triangulation angle
};

struct KnownPoseStats {
    size_t tracks = 0, points = 0, observations = 0;
};

namespace known_detail {

struct Ray {
    uint32_t image;
    uint32_t feature;
    Vec2 px;
};

// Least squares meeting point of the rays, then the angular refinement that
// stays valid for fisheye rays behind the image plane.
inline bool triangulateRays(const Reconstruction& rec, const std::vector<Ray>& rays, Vec3& X) {
    Mat3 A{};
    Vec3 b{0, 0, 0};
    std::vector<PointObservation> obs;
    for (const Ray& r : rays) {
        const Image& im = rec.images.at(r.image);
        const Camera& cam = rec.cameras.at(im.camera_id);
        const Vec3 bearing = cam.bearing(r.px);
        const Vec3 d = mul(transpose(im.pose.R), bearing);
        const Vec3 c = cameraCenter(im.pose);
        const double dd[3] = {d.x, d.y, d.z};
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++) A[3 * i + j] += (i == j ? 1.0 : 0.0) - dd[i] * dd[j];
        b = b + (c - d * d.dot(c));
        obs.push_back({poseToP(im.pose), bearing, std::max(1e-9, cam.errRad(1.0))});
    }
    bool ok = false;
    const Mat3 Ai = inverse3(A, &ok);
    if (!ok) return false;
    X = refineTriangulatedPoint(mul(Ai, b), obs);
    return std::isfinite(X.x) && std::isfinite(X.y) && std::isfinite(X.z);
}

inline bool reprojects(const Reconstruction& rec, const Ray& r, const Vec3& X, double max_px) {
    const Image& im = rec.images.at(r.image);
    const Camera& cam = rec.cameras.at(im.camera_id);
    const Vec3 p = mul(im.pose.R, X) + im.pose.t;
    if (!cam.wideFov() && !(p.z > 0)) return false;
    const Vec2 uv = cam.project(p);
    const double dx = uv.x - r.px.x, dy = uv.y - r.px.y;
    return std::isfinite(dx) && std::isfinite(dy) && dx * dx + dy * dy <= cam.errPx(max_px) * cam.errPx(max_px);
}

}  // namespace known_detail

// `image_of`: the model's image id of each database image, or kNotInModel.
// Fills those images' points2D from `features` and adds one point per track
// that two or more of them see within `max_error_px` at `min_angle_deg`.
inline KnownPoseStats triangulateKnownPoses(Reconstruction& rec, const MatchesDatabase& db,
                                            const std::vector<FeatureSet>& features,
                                            const std::vector<uint32_t>& image_of,
                                            const KnownPoseOptions& opt) {
    using namespace known_detail;
    KnownPoseStats st;
    std::vector<RelativeViewEdge> edges(db.pairs.size());
    for (size_t p = 0; p < db.pairs.size(); p++) {
        const TwoViewMatches& tv = db.pairs[p];
        if (tv.config == 0 || image_of[tv.image1] == kNotInModel ||
            image_of[tv.image2] == kNotInModel)
            continue;
        edges[p].status = RelativeViewStatus::Accepted;
        edges[p].inliers = (uint32_t)tv.matches.size();
        edges[p].match_mask.assign((tv.matches.size() + 63) / 64, ~0ull);
    }
    for (uint32_t i = 0; i < db.images.size(); i++) {
        if (image_of[i] == kNotInModel) continue;
        Image& im = rec.images.at(image_of[i]);
        im.points2D.clear();
        for (const Keypoint& k : features[i].keypoints) im.points2D.push_back({k.x, k.y});
        im.point3D_ids.assign(im.points2D.size(), kInvalidPoint3D);
    }
    const GlobalTracks tracks = buildGlobalTracks(db, features, edges, 2);
    st.tracks = tracks.size();
    const double min_angle = opt.min_angle_deg * 3.14159265358979323846 / 180.0;
    std::vector<Ray> rays;
    for (size_t t = 0; t < tracks.size(); t++) {
        rays.clear();
        for (size_t k = tracks.offsets[t]; k < tracks.offsets[t + 1]; k++) {
            const TrackElement& e = tracks.observations[k];
            const uint32_t id = image_of[e.image_id];
            if (id == kNotInModel) continue;
            // One observation per image: a point seen twice in one frame is two points.
            if (std::any_of(rays.begin(), rays.end(), [&](const Ray& r) { return r.image == id; }))
                continue;
            rays.push_back({id, e.point2D_idx, rec.images.at(id).points2D[e.point2D_idx]});
        }
        if (rays.size() < 2) continue;
        Vec3 X;
        if (!triangulateRays(rec, rays, X)) continue;
        // The worst ray is what a wrong match looks like; the rest are solved again without it.
        std::vector<Ray> kept;
        for (const Ray& r : rays)
            if (reprojects(rec, r, X, opt.max_error_px)) kept.push_back(r);
        if (kept.size() < 2) continue;
        if (kept.size() < rays.size()) {
            if (!triangulateRays(rec, kept, X)) continue;
            rays.clear();
            for (const Ray& r : kept)
                if (reprojects(rec, r, X, opt.max_error_px)) rays.push_back(r);
            if (rays.size() < 2) continue;
        } else {
            rays.swap(kept);
        }
        double widest = 0;
        for (size_t a = 0; a < rays.size(); a++)
            for (size_t c = a + 1; c < rays.size(); c++)
                widest = std::max(widest, triangulationAngle(
                                              X, cameraCenter(rec.images.at(rays[a].image).pose),
                                              cameraCenter(rec.images.at(rays[c].image).pose)));
        if (widest < min_angle) continue;
        std::vector<TrackElement> track;
        for (const Ray& r : rays) track.push_back({r.image, r.feature});
        const uint64_t pid = rec.addPoint3D(X, track);
        const uint32_t db_image = tracks.observations[tracks.offsets[t]].image_id;
        const FeatureSet& f = features[db_image];
        if (f.hasColors()) {
            const uint32_t idx = tracks.observations[tracks.offsets[t]].point2D_idx;
            for (int ch = 0; ch < 3; ch++) rec.points3D[pid].rgb[ch] = f.colors[3 * (size_t)idx + ch];
        }
        st.points++;
        st.observations += track.size();
    }
    rec.updatePointErrors();
    return st;
}

}  // namespace sfm

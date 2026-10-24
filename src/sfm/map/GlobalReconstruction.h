// Build initial reconstructions from globally averaged rotations and tracks.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>
#include <vector>

#include "sfm/core/PriorSource.h"
#include "sfm/map/GlobalPosition.h"
#include "sfm/map/GlobalTracks.h"
#include "sfm/map/RotationAverage.h"
#include "sfm/map/ViewGraphCalibration.h"

namespace sfm {

struct GlobalReconstructionOptions {
    RelativeViewOptions relative;
    RotationAverageOptions rotation;
    GlobalPositionOptions position;
    FocalGraphOptions focal;
    std::set<uint32_t> fixed_focals;
    const RigTable* rigs = nullptr;
    const PriorSource* rotation_priors = nullptr;
    PriorSource* position_priors = nullptr;
    std::function<GlobalPositionResult(GlobalPositionProblem&, const GlobalPositionOptions&)>
        position_solver;
    double max_initial_angle = 0.017453292519943295;
    double min_triangulation_angle = 0.02617993877991494;
};

struct GlobalReconstructionStats {
    size_t edges = 0, rotation_priors = 0, position_priors = 0, tracks = 0, conflicting = 0, models = 0,
           calibrated_focals = 0;
    bool rotation_converged = false;
};

inline void initializeMetricRigCalibrations(Reconstruction& model, const RigTable* rigs) {
    if (!rigs) return;
    model.rigs.resize(rigs->rigs.size());
    for (size_t r = 0; r < rigs->rigs.size(); r++) {
        const RigSpec& spec = rigs->rigs[r];
        RigCalib& calib = model.rigs[r];
        for (size_t m = 0; m < spec.members.size(); m++)
            if (spec.members[m].has_ext && spec.members[m].ext_fixed) {
                calib.ref = (int)m;
                break;
            }
        if (calib.ref < 0) continue;
        calib.resize(spec.members.size());
        const Pose base = invertPose(spec.members[(size_t)calib.ref].ext);
        for (size_t m = 0; m < spec.members.size(); m++) {
            const RigMemberDef& member = spec.members[m];
            if (!member.has_ext) continue;
            calib.cam_from_rig[m] = composePose(member.ext, base);
            if (member.ext_fixed) calib.established[m] = calib.fixed[m] = 1;
        }
    }
}

inline std::vector<Reconstruction> buildGlobalReconstructions(
    const MatchesDatabase& db, const std::vector<FeatureSet>& features,
    const std::vector<uint32_t>& camera_ids, const std::map<uint32_t, Camera>& camera_groups,
    const std::set<uint32_t>& known_focals, const GlobalReconstructionOptions& opt,
    GlobalReconstructionStats& stats) {
    stats = {};
    const size_t n = db.images.size();
    if (features.size() != n || camera_ids.size() != n ||
        !(opt.max_initial_angle > 0 && opt.max_initial_angle <= 1.5707963267948966) ||
        !(opt.min_triangulation_angle >= 0 && opt.min_triangulation_angle <= 3.141592653589793))
        throw std::invalid_argument("global reconstruction: invalid inputs");
    std::set<uint32_t> fixed_focals = known_focals;
    fixed_focals.insert(opt.fixed_focals.begin(), opt.fixed_focals.end());
    const auto calibration = calibrateViewGraph(db, features, camera_ids, camera_groups, fixed_focals,
                                                opt.relative.geometry, opt.relative.threads, opt.focal);
    const auto& calibrated_groups = calibration.cameras;
    stats.calibrated_focals = calibration.calibrated.size();
    std::vector<Camera> cameras(n);
    for (size_t i = 0; i < n; i++) {
        auto it = calibrated_groups.find(camera_ids[i]);
        if (it == calibrated_groups.end())
            throw std::invalid_argument("global reconstruction: missing camera group");
        cameras[i] = it->second;
        cameras[i].pixel_scale = features[i].pixelScale();
    }
    const auto edges = buildRelativeViewGraph(db, features, cameras, opt.relative);
    RotationAverageResult rotations;
    if (!opt.rotation_priors) {
        rotations = averageRigViewRotations(db, edges, opt.rigs, opt.rotation);
    } else {
        MatchesDatabase rotation_db;
        rotation_db.images.resize(n);
        rotation_db.pairs.reserve(db.pairs.size());
        std::vector<RelativeViewEdge> rotation_edges;
        rotation_edges.reserve(edges.size());
        std::set<std::pair<uint32_t, uint32_t>> visual_pairs;
        for (size_t i = 0; i < db.pairs.size(); i++) {
            TwoViewMatches pair;
            pair.image1 = db.pairs[i].image1;
            pair.image2 = db.pairs[i].image2;
            rotation_db.pairs.push_back(std::move(pair));
            rotation_edges.push_back({edges[i].pose, edges[i].inliers, {}, edges[i].status});
            if (edges[i].status == RelativeViewStatus::Accepted)
                visual_pairs.emplace(std::min(db.pairs[i].image1, db.pairs[i].image2),
                                     std::max(db.pairs[i].image1, db.pairs[i].image2));
        }
        std::set<std::pair<uint32_t, uint32_t>> added;
        for (uint32_t i = 0; i < n; i++)
            for (uint32_t j : opt.rotation_priors->neighbours(i)) {
                if (j >= n || i == j) continue;
                const std::pair<uint32_t, uint32_t> key{std::min(i, j), std::max(i, j)};
                if (visual_pairs.count(key) || !added.emplace(key).second) continue;
                Mat3 R;
                double sigma;
                if (!opt.rotation_priors->relativeRotation(key.first, key.second, R, sigma) ||
                    !(sigma > 0) || !std::isfinite(sigma))
                    continue;
                bool finite = true;
                for (double value : R) finite = finite && std::isfinite(value);
                if (!finite) continue;
                TwoViewMatches pair;
                pair.image1 = key.first;
                pair.image2 = key.second;
                rotation_db.pairs.push_back(std::move(pair));
                RelativeViewEdge edge;
                edge.pose.R = R;
                edge.inliers = 1;
                edge.status = RelativeViewStatus::Accepted;
                rotation_edges.push_back(std::move(edge));
                stats.rotation_priors++;
            }
        rotations = averageRigViewRotations(rotation_db, rotation_edges, opt.rigs, opt.rotation);
    }
    const FixedRigFrames rig_frames = fixedRigFrames(n, opt.rigs);
    stats.rotation_converged = rotations.converged;
    std::vector<uint8_t> accepted_edges(edges.size());
    for (size_t i = 0; i < edges.size(); i++) {
        accepted_edges[i] = rotations.accepted[i];
        stats.edges += accepted_edges[i];
    }
    const auto tracks = buildGlobalTracks(db, features, edges, 3, &accepted_edges);
    stats.tracks = tracks.size();
    stats.conflicting = tracks.conflicting;
    std::vector<size_t> parent(n + tracks.size());
    std::iota(parent.begin(), parent.end(), 0);
    auto root = [&](size_t x) {
        while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; }
        return x;
    };
    for (size_t t = 0; t < tracks.size(); t++)
        for (size_t j = tracks.offsets[t]; j < tracks.offsets[t + 1]; j++) {
            const size_t a = root(n + t), b = root(tracks.observations[j].image_id);
            if (a != b) parent[b] = a;
        }
    for (size_t i = 0; i < n; i++) {
        const size_t a = root(i), b = root(rig_frames.frame[i]);
        if (a != b) parent[b] = a;
    }
    std::map<size_t, std::vector<size_t>> groups;
    for (size_t t = 0; t < tracks.size(); t++) groups[root(n + t)].push_back(t);
    std::vector<Reconstruction> models;
    for (const auto& group : groups) {
        cancel::check();
        std::set<uint32_t> image_set;
        for (size_t t : group.second)
            for (size_t j = tracks.offsets[t]; j < tracks.offsets[t + 1]; j++)
                image_set.insert(tracks.observations[j].image_id);
        if (image_set.size() < 2) continue;
        std::vector<uint32_t> images(image_set.begin(), image_set.end());
        std::vector<uint32_t> local(n, UINT32_MAX);
        std::vector<Vec3> offsets(n);
        std::map<uint32_t, uint32_t> local_frames;
        for (uint32_t image : images) {
            const uint32_t frame = rig_frames.frame[image];
            auto found = local_frames.find(frame);
            if (found == local_frames.end())
                found = local_frames.emplace(frame, (uint32_t)local_frames.size()).first;
            local[image] = found->second;
            offsets[image] = mul(transpose(rotations.rotations[image]),
                                 rig_frames.sensor_from_rig[image].t) * -1.0;
        }
        GlobalPositionProblem problem;
        problem.cameras.resize(local_frames.size());
        problem.points.resize(group.second.size());
        std::vector<Vec3> first_offset(local_frames.size());
        std::vector<uint8_t> seen_offset(local_frames.size(), 0);
        for (size_t p = 0; p < group.second.size(); p++) {
            const size_t t = group.second[p];
            for (size_t j = tracks.offsets[t]; j < tracks.offsets[t + 1]; j++) {
                const TrackElement& obs = tracks.observations[j];
                const Keypoint& kp = features[obs.image_id].keypoints[obs.point2D_idx];
                const Vec3 ray = cameras[obs.image_id].bearing({kp.x, kp.y}).normalized();
                const Vec3 world = mul(transpose(rotations.rotations[obs.image_id]), ray);
                problem.observations.push_back({local[obs.image_id], (uint32_t)p, world,
                    known_focals.count(camera_ids[obs.image_id]) ? 1.0 : 0.5, offsets[obs.image_id]});
                const uint32_t frame = local[obs.image_id];
                if (!seen_offset[frame]) {
                    first_offset[frame] = offsets[obs.image_id];
                    seen_offset[frame] = 1;
                } else if ((first_offset[frame] - offsets[obs.image_id]).norm() > 1e-12) {
                    problem.metric_scale = true;
                }
            }
        }
        if (problem.cameras.size() < 2 && !problem.metric_scale) continue;
        initializeGlobalPosition(problem, opt.position.seed);
        const GlobalPositionResult solved = opt.position_solver
            ? opt.position_solver(problem, opt.position)
            : solveGlobalPosition(problem, opt.position);
        if (!std::isfinite(solved.final_cost) || (!solved.accepted && !solved.converged)) continue;
        if (opt.position_priors) {
            std::vector<PosedImage> posed;
            posed.reserve(images.size());
            for (uint32_t id : images) {
                const Vec3 center = problem.cameras[local[id]] + offsets[id];
                posed.push_back({id, camera_ids[id], {rotations.rotations[id],
                                                       mul(rotations.rotations[id], center) * -1.0}});
            }
            const PosePriors priors = opt.position_priors->factors(posed);
            GlobalPositionProblem metric = problem;
            std::set<uint32_t> prior_frames;
            for (const PriorCentre& q : priors.centres) {
                if (!q.absolute() || q.img[0] >= n || local[q.img[0]] == UINT32_MAX) continue;
                metric.position_priors.push_back(
                    {local[q.img[0]], offsets[q.img[0]], q.A[0], q.b, q.sigma, q.cauchy});
                prior_frames.insert(local[q.img[0]]);
            }
            if (!metric.position_priors.empty() &&
                (metric.metric_scale || prior_frames.size() >= 2)) {
                const size_t prior_count = metric.position_priors.size();
                metric.metric_scale = metric.metric_scale || prior_frames.size() >= 2;
                const GlobalPositionResult positioned = opt.position_solver
                    ? opt.position_solver(metric, opt.position)
                    : solveGlobalPosition(metric, opt.position);
                if (std::isfinite(positioned.final_cost) &&
                    (positioned.accepted || positioned.converged))
                    problem = std::move(metric);
                stats.position_priors += prior_count;
            }
        }
        Reconstruction model;
        if (problem.metric_scale) initializeMetricRigCalibrations(model, opt.rigs);
        for (size_t i = 0; i < images.size(); i++) {
            const uint32_t id = images[i], cid = camera_ids[id];
            model.cameras[cid] = calibrated_groups.at(cid);
            Image image;
            image.id = id;
            image.camera_id = cid;
            image.name = db.images[id].name;
            image.exif_orientation = features[id].exif_orientation;
            image.registered = true;
            image.pose.R = rotations.rotations[id];
            image.pose.t = mul(image.pose.R, problem.cameras[local[id]] + offsets[id]) * -1.0;
            image.points2D.reserve(features[id].count());
            for (const Keypoint& kp : features[id].keypoints) image.points2D.push_back({kp.x, kp.y});
            image.point3D_ids.assign(features[id].count(), kInvalidPoint3D);
            model.images.emplace(id, std::move(image));
        }
        for (size_t p = 0; p < group.second.size(); p++) {
            const size_t t = group.second[p];
            std::vector<TrackElement> keep;
            std::vector<Vec3> rays;
            for (size_t j = tracks.offsets[t]; j < tracks.offsets[t + 1]; j++) {
                const TrackElement obs = tracks.observations[j];
                const Vec3 delta = problem.points[p] - problem.cameras[local[obs.image_id]] -
                                   offsets[obs.image_id];
                if (delta.norm() < 1e-12) continue;
                const Keypoint& kp = features[obs.image_id].keypoints[obs.point2D_idx];
                const Vec3 observed = mul(transpose(rotations.rotations[obs.image_id]),
                    cameras[obs.image_id].bearing({kp.x, kp.y}).normalized());
                const Vec3 actual = delta.normalized();
                const double max_angle = opt.max_initial_angle *
                    (known_focals.count(camera_ids[obs.image_id]) ? 1.0 : 2.0);
                if (observed.dot(actual) < std::cos(max_angle)) continue;
                keep.push_back(obs);
                rays.push_back(actual);
            }
            bool parallax = false;
            const double max_cos = std::cos(opt.min_triangulation_angle);
            for (size_t a = 0; a < rays.size() && !parallax; a++)
                for (size_t b = a + 1; b < rays.size(); b++)
                    if (rays[a].dot(rays[b]) < max_cos) { parallax = true; break; }
            if (keep.size() < 2 || !parallax) continue;
            const uint64_t id = model.addPoint3D(problem.points[p], keep);
            uint64_t colors[3] = {0, 0, 0};
            size_t count = 0;
            for (const TrackElement& obs : keep) {
                const auto& f = features[obs.image_id];
                if (!f.hasColors()) continue;
                for (int c = 0; c < 3; c++) colors[c] += f.colors[3 * (size_t)obs.point2D_idx + c];
                count++;
            }
            if (count)
                for (int c = 0; c < 3; c++) model.points3D[id].rgb[c] = (uint8_t)(colors[c] / count);
        }
        if (model.points3D.size() >= 10) models.push_back(std::move(model));
    }
    stats.models = models.size();
    std::sort(models.begin(), models.end(), [](const Reconstruction& a, const Reconstruction& b) {
        return a.numRegistered() > b.numRegistered();
    });
    return models;
}

}  // namespace sfm

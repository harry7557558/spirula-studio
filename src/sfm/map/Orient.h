// The gauge a finished model is written in.
//
// A monocular reconstruction has no absolute orientation, position or scale --
// the mapper's answer is whatever the seed pair happened to be, so a capture
// of a level room comes out tilted at a random angle, centred on nothing in
// particular, and sized in units of "however far apart those first two frames
// were". Everything downstream then has to carry that: the trainer computes a
// normalizing similarity to place its viewer camera, the web viewer applies it
// again, and an exported splat.ply or mesh -- which carries no cameras and so
// no way to recover the transform -- is simply tilted forever.
//
// Fixing the gauge here, once, at the point where the model is written, gives
// every consumer an upright, centred, unit-sized scene for free.
//
// The transform is the SAME one the trainer computes from the poses it loads
// (dsparse::compute_normalized_transform, orientation_method="up" /
// center_method="poses" / auto_scale_poses):
//
//     up     = normalize(mean camera up axis)
//     R      = the rotation taking `up` to +Z
//     centre = mean camera position
//     scale  = 1 / max component of R (position - centre) over all cameras
//
// Two consequences worth knowing. The trainer's own normalization becomes a
// near-identity, so `train_frame_scale` comes out ~1 and the learning-rate and
// regularizer rescaling that hangs off it no longer has anything to undo --
// the training frame IS the normalized frame. And a fragmented capture
// normalizes each component separately, which is correct because separate
// components share no gauge to begin with; a later `merge` estimates the Sim3
// between them regardless.
//
// "Up" is a statistical claim about how the capture was held, not a
// measurement -- a camera carried upside-down through half a capture gives a
// mean up axis pointing at neither. It is the same claim the trainer has
// always made, and `--no-orient` is how to decline it.
#pragma once

#include <cmath>
#include <vector>

#include "sfm/core/Model.h"
#include "sfm/core/Pose.h"

namespace sfm {

// The cameras' mean up axis in world coordinates, unnormalized. Camera-to-
// world is [R^T | -R^T t]; its columns are the camera axes in the CV
// convention (x right, y DOWN, z forward), so up is minus the second ROW of R.
inline Vec3 meanCameraUp(const Reconstruction& rec) {
    Vec3 up{0, 0, 0};
    for (const auto& kv : rec.images)
        if (kv.second.registered)
            up = up + Vec3{-kv.second.pose.R[3], -kv.second.pose.R[4], -kv.second.pose.R[5]};
    return up;
}

// Rodrigues rotation taking the unit vector `up` onto +Z, about up x z.
inline Mat3 rotationUpToZ(const Vec3& up) {
    Vec3 axis{up.y, -up.x, 0.0};
    const double s = std::sqrt(axis.x * axis.x + axis.y * axis.y);
    const double c = up.z;
    Mat3 R = mat3Identity();
    if (s > 1e-12) {
        axis = axis * (1.0 / s);
        const double C = 1.0 - c;
        R = Mat3{c + axis.x * axis.x * C,       axis.x * axis.y * C - axis.z * s, axis.y * s,
                 axis.x * axis.y * C + axis.z * s, c + axis.y * axis.y * C,      -axis.x * s,
                 -axis.y * s,                   axis.x * s,                       c};
    } else if (c < 0.0) {
        R = Mat3{1, 0, 0, 0, -1, 0, 0, 0, -1};   // up == -z: flip
    }
    return R;
}

// The similarity taking `rec` into the frame where `up` is +Z, centred on the
// cameras and unit-sized. Identity with under two registered images.
inline Sim3 normalizingTransform(const Reconstruction& rec, Vec3 up) {
    Sim3 T;
    std::vector<Vec3> centers;
    Vec3 mid{0, 0, 0};
    for (const auto& kv : rec.images) {
        const Image& im = kv.second;
        if (!im.registered) continue;
        Vec3 c = mul(transpose(im.pose.R), im.pose.t) * -1.0;
        centers.push_back(c);
        mid = mid + c;
    }
    if (centers.size() < 2) return T;
    mid = mid * (1.0 / (double)centers.size());
    const double un = up.norm();
    if (!(un > 1e-12)) return T;
    up = up * (1.0 / un);
    const Mat3 R = rotationUpToZ(up);

    // Scale so the furthest camera coordinate lands on 1. Per component, not
    // by norm: that is what the trainer does, and the point of doing this here
    // is that the two agree.
    double max_abs = 0.0;
    for (const Vec3& p : centers) {
        const Vec3 d = mul(R, p - mid);
        max_abs = std::max(max_abs, std::abs(d.x));
        max_abs = std::max(max_abs, std::abs(d.y));
        max_abs = std::max(max_abs, std::abs(d.z));
    }
    T.scale = max_abs > 1e-12 ? 1.0 / max_abs : 1.0;
    T.R = R;
    T.t = mul(R, mid) * -T.scale;
    return T;
}

// The same, with up taken from the cameras themselves.
inline Sim3 uprightTransform(const Reconstruction& rec) {
    return normalizingTransform(rec, meanCameraUp(rec));
}

// Apply it. Poses and 3D points are the only things in a Reconstruction with
// world units in them: intrinsics are per-camera, 2D observations are pixels,
// and a reprojection error is a pixel count -- all unchanged by a change of
// world gauge, which is the whole reason this is safe to do at the end.
inline void applySim3(Reconstruction& rec, const Sim3& T) {
    for (auto& kv : rec.images)
        if (kv.second.registered) kv.second.pose = transformPose(T, kv.second.pose);
    for (auto& kv : rec.points3D) kv.second.xyz = transformPoint(T, kv.second.xyz);
    transformRigs(rec.rigs, T.scale);
}

// Returns the transform that was applied, so the caller can report it (and so
// a caller that needs to map something else into the new frame still can).
inline Sim3 orientModel(Reconstruction& rec) {
    const Sim3 T = uprightTransform(rec);
    applySim3(rec, T);
    return T;
}

}  // namespace sfm

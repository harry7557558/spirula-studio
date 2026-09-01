#pragma once
// Camera models.
//
// A camera is split into a per-image 6-DOF pose (angle-axis rotation +
// translation, shared by all models) and per-group intrinsics. Multiple images
// may reference the same intrinsics group ("camera groups"), and each group
// picks its own model. To add a model:
//   1. write a struct conforming to ICameraModel below,
//   2. add cost_*/jac_* entry points for it at the bottom of ba.slang,
//   3. register it in kModels in sfm/ba/Problem.h.
// Intrinsic parameter counts may differ per model; nothing else in the solver
// needs to change (keep kNumIntr <= kMaxCamDof - 6, see ba.slang).

#include "linalg.slang"

// Parameter order is COLMAP's with the principal point moved to the *end*
// (host mirror: packIntrinsics in sfm/core/Camera.h). Bundle adjustment refines a
// leading prefix of each group's parameters -- how many is per group, in
// u_group_info.z -- so which parameters are held constant is a count rather
// than a mask, and the hot kernel needs no per-parameter indirection. The
// Jacobian is still evaluated over all kNumIntr; only the leading columns are
// assembled into the reduced system, so a held-constant parameter contributes
// no column at all (rather than a zero column, which the multiplicative LM
// damping in damp_S would leave singular). D49/D50.
interface ICameraModel : IDifferentiable {
    static const int kNumIntr;
    static const bool kPeriodicX;
    [Differentiable] static This fromArray(Real v[kNumIntr]);
    // p is the point in camera frame; returns projected pixel coordinates.
    [Differentiable] RVec2 project(RVec3 p);
};

[Differentiable]
RVec3 angleAxisRotate(RVec3 axis, RVec3 p) {
    Real theta = length3(axis);
    RVec3 a = axis * (Real(1.0) / (theta + Real(1e-15)));
    RVec2 sc = rsincos(theta);
    Real st = sc.x, ct = sc.y;
    return p * ct + cross3(a, p) * st + a * (dot3(a, p) * (Real(1.0) - ct));
}

// pose = [angle-axis(3), translation(3)]
[Differentiable]
RVec3 applyPose(Real pose[6], RVec3 p_world) {
    RVec3 aa = rvec3(pose[0], pose[1], pose[2]);
    RVec3 t = rvec3(pose[3], pose[4], pose[5]);
    return angleAxisRotate(aa, p_world) + t;
}

// BAL / Snavely model: log-focal, two radial distortion coefficients.
// Projects along -z (BAL convention).
struct SnavelyModel : ICameraModel {
    static const int kNumIntr = 3;
    static const bool kPeriodicX = false;
    Real f_log; Real k1; Real k2;

    [Differentiable] static SnavelyModel fromArray(Real v[3]) {
        SnavelyModel m; m.f_log = v[0]; m.k1 = v[1]; m.k2 = v[2]; return m;
    }
    [Differentiable] RVec2 project(RVec3 p) {
        Real xp = -p.x / p.z;
        Real yp = -p.y / p.z;
        Real r2 = xp * xp + yp * yp;
        Real distortion = Real(1.0) + r2 * (k1 + r2 * k2);
        Real f = rexp(f_log) * distortion;
        return rvec2(f * xp, f * yp);
    }
};

// Same model with the focal length parameterized directly (as Ceres' BAL
// examples do) instead of log-focal.
struct SnavelyFModel : ICameraModel {
    static const int kNumIntr = 3;
    static const bool kPeriodicX = false;
    Real f; Real k1; Real k2;

    [Differentiable] static SnavelyFModel fromArray(Real v[3]) {
        SnavelyFModel m; m.f = v[0]; m.k1 = v[1]; m.k2 = v[2]; return m;
    }
    [Differentiable] RVec2 project(RVec3 p) {
        Real xp = -p.x / p.z;
        Real yp = -p.y / p.z;
        Real r2 = xp * xp + yp * yp;
        Real fd = f * (Real(1.0) + r2 * (k1 + r2 * k2));
        return rvec2(fd * xp, fd * yp);
    }
};

// A 5-parameter pinhole with principal point and one radial coefficient
// (+z looking, pixel = f * distorted + c). Demonstrates that models with a
// different parameter count drop in without touching the solver.
struct PinholeRadialModel : ICameraModel {
    static const int kNumIntr = 5;
    static const bool kPeriodicX = false;
    Real f; Real cx; Real cy; Real k1; Real k2;

    [Differentiable] static PinholeRadialModel fromArray(Real v[5]) {
        PinholeRadialModel m;
        m.f = v[0]; m.k1 = v[1]; m.k2 = v[2]; m.cx = v[3]; m.cy = v[4];
        return m;
    }
    [Differentiable] RVec2 project(RVec3 p) {
        Real xp = p.x / p.z;
        Real yp = p.y / p.z;
        Real r2 = xp * xp + yp * yp;
        Real fd = f * (Real(1.0) + r2 * (k1 + r2 * k2));
        return rvec2(fd * xp + cx, fd * yp + cy);
    }
};

// COLMAP SIMPLE_PINHOLE: one focal, principal point, no distortion.
struct SimplePinholeModel : ICameraModel {
    static const int kNumIntr = 3;
    static const bool kPeriodicX = false;
    Real f; Real cx; Real cy;
    [Differentiable] static SimplePinholeModel fromArray(Real v[3]) {
        SimplePinholeModel m; m.f = v[0]; m.cx = v[1]; m.cy = v[2]; return m;
    }
    [Differentiable] RVec2 project(RVec3 p) {
        return rvec2(f * (p.x / p.z) + cx, f * (p.y / p.z) + cy);
    }
};

// COLMAP PINHOLE: separate focals, principal point, no distortion.
struct PinholeModel : ICameraModel {
    static const int kNumIntr = 4;
    static const bool kPeriodicX = false;
    Real fx; Real fy; Real cx; Real cy;
    [Differentiable] static PinholeModel fromArray(Real v[4]) {
        PinholeModel m; m.fx = v[0]; m.fy = v[1]; m.cx = v[2]; m.cy = v[3]; return m;
    }
    [Differentiable] RVec2 project(RVec3 p) {
        return rvec2(fx * (p.x / p.z) + cx, fy * (p.y / p.z) + cy);
    }
};

// COLMAP OPENCV model: separate focals, principal point, two radial and two
// tangential coefficients (Brown-Conrady). Host mirror: sfm/core/Camera.h's
// CamModel::OpenCV project(). +z looking, pixel = f*distorted + c.
struct OpenCVModel : ICameraModel {
    static const int kNumIntr = 8;
    static const bool kPeriodicX = false;
    Real fx; Real fy; Real cx; Real cy; Real k1; Real k2; Real p1; Real p2;

    [Differentiable] static OpenCVModel fromArray(Real v[8]) {
        OpenCVModel m;
        m.fx = v[0]; m.fy = v[1];
        m.k1 = v[2]; m.k2 = v[3]; m.p1 = v[4]; m.p2 = v[5];
        m.cx = v[6]; m.cy = v[7];
        return m;
    }
    [Differentiable] RVec2 project(RVec3 p) {
        Real xp = p.x / p.z;
        Real yp = p.y / p.z;
        Real r2 = xp * xp + yp * yp;
        Real radial = Real(1.0) + r2 * (k1 + r2 * k2);
        Real dx = xp * radial + Real(2.0) * p1 * xp * yp + p2 * (r2 + Real(2.0) * xp * xp);
        Real dy = yp * radial + p1 * (r2 + Real(2.0) * yp * yp) + Real(2.0) * p2 * xp * yp;
        return rvec2(fx * dx + cx, fy * dy + cy);
    }
};

// COLMAP OPENCV_FISHEYE (Kannala-Brandt): equidistant fisheye with four radial
// polynomial terms on the angle theta from the optical axis. Valid for rays past
// 90 deg (p.z <= 0), which is why the reconstruction had to move onto bearings
// (D31). Host mirror: sfm/core/Camera.h CamModel::OpenCVFisheye.
struct FisheyeModel : ICameraModel {
    static const int kNumIntr = 8;
    static const bool kPeriodicX = false;
    Real fx; Real fy; Real cx; Real cy; Real k1; Real k2; Real k3; Real k4;

    [Differentiable] static FisheyeModel fromArray(Real v[8]) {
        FisheyeModel m;
        m.fx = v[0]; m.fy = v[1];
        m.k1 = v[2]; m.k2 = v[3]; m.k3 = v[4]; m.k4 = v[5];
        m.cx = v[6]; m.cy = v[7];
        return m;
    }
    [Differentiable] RVec2 project(RVec3 p) {
        // eps under the sqrt keeps r (and the thd/r derivative) finite on-axis.
        Real r = sqrt(p.x * p.x + p.y * p.y + Real(1e-24));
        Real theta = rAtan2(r, p.z);           // [0, pi], smooth at the axis
        Real t2 = theta * theta;
        Real thd = theta * (Real(1.0) + t2 * (k1 + t2 * (k2 + t2 * (k3 + t2 * k4))));
        Real scale = thd / r;
        return rvec2(fx * scale * p.x + cx, fy * scale * p.y + cy);
    }
};

// COLMAP FULL_OPENCV: pinhole Brown-Conrady with a rational radial term --
// (1 + k1 r^2 + k2 r^4 + k3 r^6) / (1 + k4 r^2 + k5 r^4 + k6 r^6) -- plus the
// two tangential terms. Host mirror: sfm/core/Camera.h CamModel::FullOpenCV.
struct FullOpenCVModel : ICameraModel {
    static const int kNumIntr = 12;
    static const bool kPeriodicX = false;
    Real fx; Real fy; Real cx; Real cy;
    Real k1; Real k2; Real p1; Real p2; Real k3; Real k4; Real k5; Real k6;

    [Differentiable] static FullOpenCVModel fromArray(Real v[12]) {
        FullOpenCVModel m;
        m.fx = v[0]; m.fy = v[1];
        m.k1 = v[2]; m.k2 = v[3]; m.p1 = v[4]; m.p2 = v[5];
        m.k3 = v[6]; m.k4 = v[7]; m.k5 = v[8]; m.k6 = v[9];
        m.cx = v[10]; m.cy = v[11];
        return m;
    }
    [Differentiable] RVec2 project(RVec3 p) {
        Real xp = p.x / p.z;
        Real yp = p.y / p.z;
        Real r2 = xp * xp + yp * yp;
        Real radial = (Real(1.0) + r2 * (k1 + r2 * (k2 + r2 * k3))) /
                      (Real(1.0) + r2 * (k4 + r2 * (k5 + r2 * k6)));
        Real dx = xp * radial + Real(2.0) * p1 * xp * yp + p2 * (r2 + Real(2.0) * xp * xp);
        Real dy = yp * radial + p1 * (r2 + Real(2.0) * yp * yp) + Real(2.0) * p2 * xp * yp;
        return rvec2(fx * dx + cx, fy * dy + cy);
    }
};

// COLMAP THIN_PRISM_FISHEYE: Kannala-Brandt fisheye (radial in theta) plus
// tangential and thin-prism terms, all in the equidistant coords (uf,vf) =
// theta*(x,y)/r. Valid past 90 deg. Host mirror: CamModel::ThinPrismFisheye.
struct ThinPrismFisheyeModel : ICameraModel {
    static const int kNumIntr = 12;
    static const bool kPeriodicX = false;
    Real fx; Real fy; Real cx; Real cy;
    Real k1; Real k2; Real p1; Real p2; Real k3; Real k4; Real sx1; Real sy1;

    [Differentiable] static ThinPrismFisheyeModel fromArray(Real v[12]) {
        ThinPrismFisheyeModel m;
        m.fx = v[0]; m.fy = v[1];
        m.k1 = v[2]; m.k2 = v[3]; m.p1 = v[4]; m.p2 = v[5];
        m.k3 = v[6]; m.k4 = v[7]; m.sx1 = v[8]; m.sy1 = v[9];
        m.cx = v[10]; m.cy = v[11];
        return m;
    }
    [Differentiable] RVec2 project(RVec3 p) {
        Real r = sqrt(p.x * p.x + p.y * p.y + Real(1e-24));
        Real theta = rAtan2(r, p.z);
        Real s = theta / r;
        Real uf = s * p.x;
        Real vf = s * p.y;
        Real rf2 = theta * theta;
        Real radial = rf2 * (k1 + rf2 * (k2 + rf2 * (k3 + rf2 * k4)));
        Real du = uf * radial + Real(2.0) * p1 * uf * vf + p2 * (rf2 + Real(2.0) * uf * uf) + sx1 * rf2;
        Real dv = vf * radial + p1 * (rf2 + Real(2.0) * vf * vf) + Real(2.0) * p2 * uf * vf + sy1 * rf2;
        return rvec2(fx * (uf + du) + cx, fy * (vf + dv) + cy);
    }
};

// COLMAP EQUIRECTANGULAR: the spherical panorama. Azimuth spans the width,
// elevation the height; every direction projects, including behind the camera.
// The two parameters are the image dimensions and are never refined (the host
// gives this group a free count of 0). Host mirror: CamModel::Equirect.
struct EquirectModel : ICameraModel {
    static const int kNumIntr = 2;
    static const bool kPeriodicX = true;
    Real w; Real h;

    [Differentiable] static EquirectModel fromArray(Real v[2]) {
        EquirectModel m; m.w = v[0]; m.h = v[1]; return m;
    }
    [Differentiable] RVec2 project(RVec3 p) {
        // theta measured from +z toward +x, phi from the equator toward -y.
        // The epsilon keeps the gradient finite on the exact poles/origin.
        Real horiz = sqrt(p.x * p.x + p.z * p.z + Real(1e-24));
        Real theta = rAtan2(p.x, p.z);
        Real phi = rAtan2(-p.y, horiz);
        const Real kInv2Pi = Real(0.15915494309189535);
        const Real kInvPi = Real(0.3183098861837907);
        return rvec2((theta * kInv2Pi + Real(0.5)) * w, (Real(0.5) - phi * kInvPi) * h);
    }
};

// Reprojection residual, generic in the camera model.
[Differentiable]
RVec2 reproject<M : ICameraModel>(
    Real pose[6], Real intr[M.kNumIntr], RVec3 X, no_diff RVec2 obs
) {
    M cam = M.fromArray(intr);
    RVec3 p = applyPose(pose, X);
    RVec2 r = cam.project(p) - obs;
    if (M.kPeriodicX) {
        Real half = Real(0.5) * intr[0];
        if (r.x > half) r.x = r.x - intr[0];
        else if (r.x < -half) r.x = r.x + intr[0];
    }
    return r;
}

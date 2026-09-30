// SplatTransform.cpp -- see SplatTransform.h.

#include "checkpoint/SplatTransform.h"

#include "checkpoint/SplatPly.h"
#include "data/SceneTransform.h"

#include <cmath>
#include <stdexcept>

namespace spirula {

namespace {
Sim3 as_sim3(const SceneTransform& T) {
    Sim3 out;
    out.s = T.scale;
    if (!(out.s > 0.0) || !std::isfinite(out.s))
        throw std::runtime_error("Snapshot: invalid scale");
    for (int i = 0; i < 9; ++i) out.R[i] = T.R[i];
    for (int i = 0; i < 3; ++i) out.t[i] = T.t[i];
    for (double v : out.t)
        if (!std::isfinite(v)) throw std::runtime_error("Snapshot: invalid translation");
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c) {
            double dot = 0.0;
            for (int k = 0; k < 3; ++k) dot += out.R[3*r+k]*out.R[3*c+k];
            if (!std::isfinite(dot) || std::abs(dot - (r == c ? 1.0 : 0.0)) > 1e-5)
                throw std::runtime_error("Snapshot: rotation must be orthonormal");
        }
    const auto& R = out.R;
    const double det = R[0]*(R[4]*R[8]-R[5]*R[7]) -
                       R[1]*(R[3]*R[8]-R[5]*R[6]) + R[2]*(R[3]*R[7]-R[4]*R[6]);
    if (det < 0.0) throw std::runtime_error("Snapshot: reflection is not a rotation");
    return out;
}

int row_degree(int coeffs) {
    const int degree = (int)std::sqrt((double)coeffs + 1) - 1;
    if (degree < 0 || degree > ShRotation::kMaxDegree ||
        (degree + 1)*(degree + 1) != coeffs + 1)
        throw std::runtime_error("Snapshot: unsupported SH degree");
    return degree;
}
}

SplatTransform::SplatTransform(const Sim3& T, int sh_degree)
    : _T(T), _log_s((float)std::log(T.s)), _identity(T.is_identity()),
      _sh(T.R, sh_degree) {
    _T.quat(_q);
}

SplatTransform::SplatTransform(const SceneTransform& T, int sh_coefficients)
    : SplatTransform(as_sim3(T), row_degree(sh_coefficients)) {
    _row_coeffs = sh_coefficients;
}

void SplatTransform::apply(float* row) const {
    float* rest = row + 9;
    float sh[3 * ((ShRotation::kMaxDegree + 1) * (ShRotation::kMaxDegree + 1) - 1)];
    for (int ch = 0; ch < 3; ++ch)
        for (int j = 0; j < _row_coeffs; ++j)
            sh[3*j + ch] = rest[ch*_row_coeffs + j];
    apply(row, row + 13 + 3*_row_coeffs, row + 10 + 3*_row_coeffs,
          _row_coeffs ? sh : nullptr, _row_coeffs);
    for (int ch = 0; ch < 3; ++ch)
        for (int j = 0; j < _row_coeffs; ++j)
            rest[ch*_row_coeffs + j] = sh[3*j + ch];
}

void SplatTransform::apply(float mean[3], float quat[4], float log_scale[3],
                           float* rest, int coeffs) const {
    if (_identity) return;
    const double p[3] = {mean[0], mean[1], mean[2]};
    double o[3];
    _T.apply(p, o);
    for (int i = 0; i < 3; i++) mean[i] = (float)o[i];

    // Hamilton product q_R * q: the Gaussian's own frame, then the turn.
    const double aw = _q[0], ax = _q[1], ay = _q[2], az = _q[3];
    const double bw = quat[0], bx = quat[1], by = quat[2], bz = quat[3];
    double r[4] = {aw*bw - ax*bx - ay*by - az*bz,
                   aw*bx + ax*bw + ay*bz - az*by,
                   aw*by - ax*bz + ay*bw + az*bx,
                   aw*bz + ax*by - ay*bx + az*bw};
    // The stored quaternion is not normalized, and its length is the file's
    // to keep: renormalizing here would be a second edit nobody asked for.
    for (int i = 0; i < 4; i++) quat[i] = (float)r[i];

    for (int i = 0; i < 3; i++) log_scale[i] += _log_s;
    if (rest && coeffs > 0) _sh.apply(rest, coeffs);
}

void transform_splats(SplatCloud& c, const Sim3& T) {
    const SplatTransform xf(T, c.sh_degree);
    if (xf.is_identity()) return;
    const int K = (int)c.dim_sh() - 1;
#pragma omp parallel for schedule(static)
    for (int64_t i = 0; i < c.num; i++)
        xf.apply(&c.means[(size_t)i * 3], &c.quats[(size_t)i * 4],
                 &c.scales[(size_t)i * 3],
                 K > 0 ? &c.features_sh[(size_t)i * K * 3] : nullptr, K);
}

}  // namespace spirula

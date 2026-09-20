#include "checkpoint/SplatTransform.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace spirula {
namespace {
constexpr double pi = 3.14159265358979323846;

// Real orthonormal SH, l*l+l+m order and Condon-Shortley phase (harmonics.slang).
void basis(const double d[3], int degree, double out[25]) {
    const double z = std::clamp(d[2], -1.0, 1.0);
    const double phi = std::atan2(d[1], d[0]);
    const double radial = std::sqrt(std::max(0.0, 1.0 - z*z));
    double pmm = 1.0;
    for (int m = 0; m <= degree; ++m) {
        if (m) pmm *= -(2*m - 1) * radial;
        double previous = 0, current = pmm;
        for (int l = m; l <= degree; ++l) {
            if (l > m) {
                const double next = ((2*l - 1)*z*current - (l+m-1)*previous) / (l-m);
                previous = current;
                current = next;
            }
            double ratio = 1;
            for (int k = l-m+1; k <= l+m; ++k) ratio /= k;
            const double value = std::sqrt((2*l+1) / (4*pi) * ratio) * current;
            const int center = l*l+l;
            if (m == 0) out[center] = value;
            else {
                out[center-m] = std::sqrt(2.0) * value * std::sin(m*phi);
                out[center+m] = std::sqrt(2.0) * value * std::cos(m*phi);
            }
        }
    }
}
}

SplatTransform::SplatTransform(const SceneTransform& transform, int coefficients)
    : _transform(transform), _coefficients(coefficients) {
    _degree = (int)std::sqrt((double)coefficients + 1) - 1;
    if (_degree < 0 || _degree > 4 || (_degree+1)*(_degree+1) != coefficients+1)
        throw std::runtime_error("Snapshot: unsupported SH degree");
    if (!(transform.scale > 0) || !std::isfinite(transform.scale))
        throw std::runtime_error("Snapshot: invalid scale");
    for (double v : transform.t)
        if (!std::isfinite(v)) throw std::runtime_error("Snapshot: invalid translation");
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c) {
            double v = 0;
            for (int k = 0; k < 3; ++k) v += transform.R[r*3+k]*transform.R[c*3+k];
            if (!std::isfinite(v) || std::abs(v - (r == c ? 1.0 : 0.0)) > 1e-5)
                throw std::runtime_error("Snapshot: rotation must be orthonormal");
            _rotate |= std::abs(transform.R[r*3+c] - (r == c ? 1.0 : 0.0)) > 1e-12;
        }
    const auto& R = transform.R;
    const double det = R[0]*(R[4]*R[8]-R[5]*R[7]) - R[1]*(R[3]*R[8]-R[5]*R[6]) + R[2]*(R[3]*R[7]-R[4]*R[6]);
    if (det < 0) throw std::runtime_error("Snapshot: reflection is not a rotation");
    rotation_to_quaternion(R, _quaternion);
    if (!_rotate || !_degree) return;

    // Product integration is exact through degree 8: 8 Gauss-Legendre nodes x 16 azimuths.
    constexpr double nodes[8] = {-0.9602898564975363, -0.7966664774136267,
        -0.5255324099163290, -0.1834346424956498, 0.1834346424956498,
        0.5255324099163290, 0.7966664774136267, 0.9602898564975363};
    constexpr double weights[8] = {0.1012285362903763, 0.2223810344533745,
        0.3137066458778873, 0.3626837833783620, 0.3626837833783620,
        0.3137066458778873, 0.2223810344533745, 0.1012285362903763};
    for (int z = 0; z < 8; ++z) for (int a = 0; a < 16; ++a) {
        const double phi = 2*pi*a/16, radial = std::sqrt(1-nodes[z]*nodes[z]);
        const double d[3] = {radial*std::cos(phi), radial*std::sin(phi), nodes[z]};
        double original[3]{};
        for (int r = 0; r < 3; ++r)
            for (int k = 0; k < 3; ++k) original[r] += R[k*3+r]*d[k];
        double y[25]{}, old[25]{};
        basis(d, _degree, y);
        basis(original, _degree, old);
        for (int l = 1; l <= _degree; ++l)
            for (int i = l*l; i < (l+1)*(l+1); ++i)
                for (int j = l*l; j < (l+1)*(l+1); ++j)
                    _sh[i*25+j] += weights[z]*(2*pi/16)*y[i]*old[j];
    }
}

void SplatTransform::apply(float* row) const {
    double p[3] = {row[0], row[1], row[2]};
    for (int r = 0; r < 3; ++r) {
        double v = _transform.t[r];
        for (int k = 0; k < 3; ++k) v += _transform.scale * _transform.R[r*3+k]*p[k];
        row[r] = (float)v;
    }
    const int scale_offset = 10 + 3*_coefficients;
    for (int i = 0; i < 3; ++i) row[scale_offset+i] += (float)std::log(_transform.scale);
    if (!_rotate) return;
    float* q = row + scale_offset + 3;
    const double w = q[0], x = q[1], y = q[2], z = q[3];
    const double a = _quaternion[0], b = _quaternion[1], c = _quaternion[2], d = _quaternion[3];
    q[0] = (float)(a*w-b*x-c*y-d*z);
    q[1] = (float)(a*x+b*w+c*z-d*y);
    q[2] = (float)(a*y-b*z+c*w+d*x);
    q[3] = (float)(a*z+b*y-c*x+d*w);
    for (int ch = 0; ch < 3; ++ch) {
        float* sh = row + 9 + ch*_coefficients;
        float old[24];
        std::copy(sh, sh + _coefficients, old);
        for (int l = 1; l <= _degree; ++l)
            for (int i = l*l; i < (l+1)*(l+1); ++i) {
                double value = 0;
                for (int j = l*l; j < (l+1)*(l+1); ++j) value += _sh[i*25+j]*old[j-1];
                sh[i-1] = (float)value;
            }
    }
}
}  // namespace spirula

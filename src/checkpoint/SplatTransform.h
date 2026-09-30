#pragma once

// Moving a trained model: what a similarity does to one Gaussian.
//
//   mean  -> s R mean + t          quat -> q_R * quat
//   scale -> scale + ln s (logs)   SH    -> rotated band by band
//
// Opacity and the DC colour do not change. The view-dependent bands do, or
// the highlights stay where they were while the object turns under them
// (core/ShRotation.h). One object per transform: the band matrices are built
// once and every row reuses them.

#include "core/ShRotation.h"
#include "core/Similarity.h"

#include <cstdint>

namespace spirula {

struct SplatCloud;
struct SceneTransform;

class SplatTransform {
public:
    SplatTransform(const Sim3& T, int sh_degree);
    SplatTransform(const SceneTransform& T, int sh_coefficients);

    bool is_identity() const { return _identity; }
    // One Gaussian in place, in the raw layout checkpoint/SplatPly.h stores:
    // log scales, a (w,x,y,z) quaternion, `rest` as [coeffs, 3].
    void apply(float mean[3], float quat[4], float log_scale[3], float* rest,
               int coeffs) const;
    void apply(float* ply_row) const;

private:
    Sim3 _T;
    double _q[4];
    float _log_s;
    bool _identity;
    ShRotation _sh;
    int _row_coeffs = 0;
};

// Every Gaussian of `c`, in place.
void transform_splats(SplatCloud& c, const Sim3& T);

}  // namespace spirula

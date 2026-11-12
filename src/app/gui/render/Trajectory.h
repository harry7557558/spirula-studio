#pragma once

// Where the camera is at a given time: the keyframes of a RenderProject
// joined into one motion. Every channel -- position, rotation, aim, zoom --
// is a cubic through the keys on one shared parameter: key time, or the
// distance along the path when the speed is held constant. The Spline curve
// is C2 (a cubic B-spline through the keys), Catmull-Rom is C1 and local.

#include "app/gui/render/RenderProject.h"

#include <cstdint>
#include <vector>

namespace gui::render {

// What one frame is rendered from.
struct CameraState {
    double pos[3] = {0, 0, 0};
    double rot[4] = {1, 0, 0, 0};       // camera-to-world, (w, x, y, z)
    Lens lens;
    // Row-major 3x4 camera-to-world, OpenGL axes: what the renderers take.
    void c2w(double out[12]) const;
};

// Precomputed once per edit of the project, then evaluated per frame.
class Trajectory {
public:
    // `measure` false skips the path's length where the timing does not
    // need it (length() is then 0).
    explicit Trajectory(const RenderProject& p, bool measure = true);
    CameraState at(double t) const;
    // The path as `n` evenly spaced positions, for drawing it.
    std::vector<double> sample_path(int n) const;
    double length() const { return _length; }
    // When the camera passes each key: its time, unless the speed is held
    // constant, which moves every key but the ends.
    const std::vector<double>& key_times() const { return _visit; }

private:
    // Curve parameter of the frame at `t`.
    double warp(double t) const;
    CameraState eval(double u) const;

    const RenderProject& _p;
    // Knots: one per key, and a loop repeats the first at its end.
    int _n = 0, _knots = 0;
    bool _closed = false;
    std::vector<double> _u;              // parameter at each knot
    std::vector<int> _src;               // the key each knot is
    std::vector<double> _t;              // output time at each knot
    // Per knot: every channel's value and slope (per unit of parameter).
    std::vector<double> _pos, _rot, _tgt, _roll, _logf, _log_ortho_distance;
    std::vector<double> _dpos, _drot, _dtgt, _droll, _dfocal, _dortho_distance;
    std::vector<double> _cum_u, _cum_len;    // arc-length table
    std::vector<double> _visit;
    std::vector<Lens> _lens;             // per key
    double _length = 0.0;
};

// Keys deleted from `before`: move the rest of `p` (poses, not lenses) so the
// camera passes where it did. A radian of turn weighs `unit` of distance.
void refit_keys(RenderProject& p, const RenderProject& before, double unit);

// How much the camera accelerates over the move, turning included (a radian
// weighs `unit`), summed over 30 samples a second: what smoothing lowers.
double motion_energy(const RenderProject& p, double unit);
// Key times part of the way to where the speed between keys -- turning
// included -- changes smoothly from gap to gap. The ends, the stops and keys
// not `movable` keep their times, and each run between them its length.
void smooth_key_speeds(RenderProject& p, const std::vector<uint8_t>& movable, double strength,
                       double unit);

// The quaternion of `a` then `b`, and a rotation of a vector, (w, x, y, z).
void quat_mul(const double a[4], const double b[4], double out[4]);
void quat_rotate(const double q[4], const double v[3], double out[3]);
void quat_to_matrix3(const double q[4], double R[9]);
void quat_from_matrix3(const double R[9], double q[4]);

}  // namespace gui::render

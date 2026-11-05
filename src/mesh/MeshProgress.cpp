// MeshProgress.cpp -- see MeshProgress.h.

#include "mesh/MeshProgress.h"

#include "backend/api/BackendRuntime.h"
#include "core/HostMemory.h"
#include "mesh/Meshing.h"

#include <algorithm>

namespace meshing {

namespace {

constexpr int kCount = (int)MeshPhase::Count;

// Seconds per unit on the RTX 3070 laptop for a 993k-splat model with 226
// cameras: 6.95M points, 6.1M cut edges, 12.2M raw faces, 1.19M final faces and
// an 8192^2 atlas, 125 s in all. Only their ratios reach the bar.
constexpr double kPointCloud = 0.065e-6, kDelaunay = 0.85e-6, kOccupancy = 0.92e-6;
constexpr double kCutEdges = 0.16e-6, kMarching = 0.16e-6, kBisection = 0.25e-6;  // per edge and iteration
constexpr double kMerge = 0.47e-6, kPass = 3.0e-6;                                // per face
constexpr double kQuality = 1.4e-6, kOrient = 0.46e-6, kColor = 3.5e-6, kWrite = 0.5e-6;
constexpr double kAtlas = 32e-6, kBakeTexel = 0.15e-6, kBakeFace = 3e-6;

// How each stage shrinks the mesh on that run (6.1M edges of 6.95M points; then
// 12.2M -> 5.0M -> 2.2M -> 1.19M faces), each raised a fifth: an estimate that
// comes in high moves the bar forward when the real size arrives, not back.
constexpr double kEdgesPerPoint = 1.0, kRawPerEdge = 2.0;
constexpr double kShrink[3] = {0.5, 0.53, 0.65};

// Bytes above what the process held on entry, from the same run's working set
// (processRamBytes): 0.28 GB, then 2.1 GB triangulating, 1.8 GB at the first merge
// and 1.15 GB baking; on the device, the occupancy renders, the culling BVH, the bake.
constexpr double kHostDelaunay = 270.0, kHostMerge = 130.0;   // per point, per raw face
constexpr double kHostTexel = 12.0, kHostFace = 50.0;
constexpr double kDeviceOccupancy = 45.0, kDeviceCull = 100.0, kDeviceTexel = 12.0;
// One run measured: a third of each predicted rise either way.
constexpr double kMemorySpread = 0.3;
constexpr int kAutoTextureMax = 8192;

double device_bytes() {
    const backend::MemoryUsage m = backend::memory_usage();
    return m.has_process ? (double)m.process_bytes : 0.0;
}

}  // namespace

MeshProgress::MeshProgress(const MeshingConfig& cfg, const MeshProgressSink& sink, double points,
                           bool vertex_color, bool texture)
    : _sink(sink), _bisection_iters(std::max(1, cfg.bisection_iters)),
      _quality_iters(std::max(0, cfg.quality_iters)), _points(points) {
    for (int i = 0; i < kCount; ++i) _on[i] = true;
    _on[(int)MeshPhase::Quality] = _quality_iters > 0;
    _on[(int)MeshPhase::Color] = vertex_color;
    _on[(int)MeshPhase::Atlas] = _on[(int)MeshPhase::Bake] = texture;
    const double side = cfg.texture_size > 0 ? cfg.texture_size : kAutoTextureMax;
    _texels = texture ? side * side : 0.0;
    if (!_sink) return;
    _host_base = (double)spirula::processRamBytes();
    _device_base = device_bytes();
    plan();
}

void MeshProgress::edges(double e) { _edges = e; _edges_known = true; plan(); }
void MeshProgress::raw_faces(double f) { _raw_faces = f; _raw_known = true; plan(); }
void MeshProgress::texels(double t) { _texels = t; plan(); }

void MeshProgress::faces(double f) {
    if (_faces_known < 3) _faces[++_faces_known] = f;
    plan();
}

void MeshProgress::sizes(double& E, double F[4]) const {
    E = _edges_known ? _edges : kEdgesPerPoint * _points;
    F[0] = _raw_known ? _raw_faces : kRawPerEdge * E;
    for (int k = 1; k <= 3; ++k) F[k] = _faces_known >= k ? _faces[k] : kShrink[k - 1] * F[k - 1];
}

void MeshProgress::plan() {
    double E, F[4];
    sizes(E, F);
    const double raw = F[0];
    auto set = [&](MeshPhase p, double c) { _cost[(int)p] = c; };
    set(MeshPhase::PointCloud, kPointCloud * _points);
    set(MeshPhase::Delaunay, kDelaunay * _points);
    set(MeshPhase::Occupancy, kOccupancy * _points);
    set(MeshPhase::CutEdges, kCutEdges * _points);
    set(MeshPhase::Bisection, kBisection * E * _bisection_iters);
    set(MeshPhase::Marching, kMarching * _points);
    set(MeshPhase::Merge, kMerge * raw);
    set(MeshPhase::Pass1, kPass * F[1]);
    set(MeshPhase::Pass2, kPass * F[2]);
    set(MeshPhase::Quality, kQuality * F[3] * _quality_iters);
    set(MeshPhase::Orient, kOrient * F[3]);
    set(MeshPhase::Color, kColor * F[3]);
    set(MeshPhase::Write, kWrite * F[3]);
    set(MeshPhase::Atlas, kAtlas * F[3]);
    set(MeshPhase::Bake, kBakeTexel * _texels + kBakeFace * F[3]);
    report();
}

void MeshProgress::enter(MeshPhase p) {
    const int next = (int)p;
    if (_cur >= 0 && _on[_cur]) _done += _cost[_cur];
    // A stage passed over without being entered did not run, and costs nothing.
    for (int i = std::max(0, _cur + 1); i < next; ++i) _on[i] = false;
    _cur = next;
    report();
}

void MeshProgress::finish() {
    if (!_sink) return;
    MeshProgressReport r;
    r.work = 1.0;
    r.host_peak = (double)spirula::processRamBytes();
    r.device_peak = device_bytes();
    _sink(r);
}

void MeshProgress::report() {
    if (!_sink) return;
    double left = 0.0, host = 0.0, device = 0.0;
    double E, F[4];
    sizes(E, F);
    for (int i = std::max(0, _cur); i < kCount; ++i) {
        if (!_on[i]) continue;
        left += _cost[i];
        switch ((MeshPhase)i) {
            case MeshPhase::Delaunay:  host = std::max(host, kHostDelaunay * _points); break;
            case MeshPhase::Occupancy: device = std::max(device, kDeviceOccupancy * _points); break;
            case MeshPhase::Merge:     host = std::max(host, kHostMerge * F[0]); break;
            case MeshPhase::Pass1:     device = std::max(device, kDeviceCull * F[1]); break;
            case MeshPhase::Bake:
                host = std::max(host, kHostTexel * _texels + kHostFace * F[3]);
                device = std::max(device, kDeviceTexel * _texels);
                break;
            default: break;
        }
    }
    MeshProgressReport r;
    r.work = _done + left > 0.0 ? _done / (_done + left) : 0.0;
    r.host_peak = std::max((double)spirula::processRamBytes(), _host_base + host);
    r.host_sigma = kMemorySpread * host;
    r.device_peak = std::max(device_bytes(), _device_base + device);
    r.device_sigma = kMemorySpread * device;
    _sink(r);
}

}  // namespace meshing

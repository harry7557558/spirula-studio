#pragma once

// A meshing run's progress as a share of its estimated time, and the memory it
// expects to peak at, both from the sizes known so far. Each stage's real size
// revises the estimates of the stages after it. docs/notes/progress-bars.md
// has the run the costs were measured on.

#include <cstdint>
#include <functional>

namespace meshing {

struct MeshingConfig;

struct MeshProgressReport {
    double work = 0.0;                          // 0..1 of the estimated time
    double host_peak = 0.0, host_sigma = 0.0;   // this process's RAM, bytes
    double device_peak = 0.0, device_sigma = 0.0;
};
using MeshProgressSink = std::function<void(const MeshProgressReport&)>;

enum class MeshPhase {
    PointCloud, Delaunay, Occupancy, CutEdges, Bisection, Marching, Merge,
    Pass1, Pass2, Quality, Orient, Color, Write, Atlas, Bake, Count
};

class MeshProgress {
public:
    // `points` the point cloud's size, which the evaluator knows before it is sampled.
    MeshProgress(const MeshingConfig& cfg, const MeshProgressSink& sink, double points,
                 bool vertex_color, bool texture);

    void edges(double e);
    void raw_faces(double f);
    // The mesh as it stands after a merge pass, which sets what every later stage costs.
    void faces(double f);
    void texels(double t);
    void enter(MeshPhase p);
    void finish();

private:
    void sizes(double& edges, double faces[4]) const;
    void plan();
    void report();

    const MeshProgressSink& _sink;
    int _bisection_iters = 3, _quality_iters = 3;
    bool _on[(int)MeshPhase::Count] = {};
    double _cost[(int)MeshPhase::Count] = {};
    int _cur = -1;
    double _done = 0.0;
    // Sizes: measured once the stage that makes them has run, estimated until then.
    double _points = 0, _edges = 0, _raw_faces = 0, _faces[4] = {}, _texels = 0;
    int _faces_known = 0;
    bool _edges_known = false, _raw_known = false;
    double _host_base = 0.0, _device_base = 0.0;
};

}  // namespace meshing

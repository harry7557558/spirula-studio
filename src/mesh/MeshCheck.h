/*
 * MeshCheck.h
 *
 * Sanity checks on a triangle mesh (`spirula mesh --check`): indexing,
 * topology, self-intersections, orientation and element quality. Every count
 * in MeshCheckReport::defects() is something the mesher should never emit;
 * the report prints in English, like the other deep diagnostics.
 */
#ifndef SPIRULA_MESH_CHECK_H
#define SPIRULA_MESH_CHECK_H

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "mesh/MeshExport.h"   // MeshData

namespace meshing {

struct MeshCheckLimits {
    float min_angle_deg = 1.0f;     // smaller angles are poor
    float max_aspect = 30.0f;       // longest edge / (2 sqrt3 inradius); 1 = equilateral
    int   max_valence = 16;         // more edges at a vertex is poor
    float fold_deg = 10.0f;         // adjacent faces closer than this fold onto each other
    int   min_part_faces = 0;       // parts with fewer faces count as floaters; <=1 off
    bool  intersections = true;     // the self-intersection test (the slow part)
    int   num_threads = 0;
};

struct MeshCheckReport {
    long vertices = 0, faces = 0, parts = 0, boundary_edges = 0, boundary_loops = 0;

    // defects
    long bad_index = 0;           // index out of range
    long repeated_index = 0;      // a face naming one vertex twice
    long nonfinite = 0;           // NaN / inf position
    long unused_vertices = 0;
    long duplicate_vertices = 0;  // extra vertices at an occupied position
    long duplicate_edges = 0;     // extra edges between two occupied positions
    long duplicate_faces = 0;     // extra faces over the same three positions
    long nonmanifold_edges = 0;   // edges with 3+ faces
    long nonmanifold_vertices = 0;  // faces around a vertex in 2+ edge-connected fans
    long misoriented_edges = 0;   // both faces run the edge the same way
    long inside_out_parts = 0;    // closed parts with negative volume
    long small_parts = 0;
    long zero_area = 0;
    long folds = 0;               // edges whose faces fold back onto each other
    long intersecting_pairs = 0;  // face pairs crossing away from shared vertices
    long intersecting_faces = 0;
    long small_angles = 0;
    long large_aspect = 0;
    long low_valence = 0;         // flat interior vertices of valence 3
    long high_valence = 0;

    // extremes, for the log
    double min_angle_deg = 180.0, max_aspect = 0.0;
    long under_5_deg = 0, under_15_deg = 0;   // faces by smallest angle
    int max_valence = 0;
    double seconds = 0.0;

    long defects() const;
};

MeshCheckReport check_mesh(const MeshData& mesh, const MeshCheckLimits& lim);

using Box3 = std::array<float, 6>;   // lo xyz, hi xyz
Box3 face_box(const MeshData& mesh, int face);
inline bool boxes_overlap(const Box3& a, const Box3& b) {
    return a[0] <= b[3] && a[3] >= b[0] && a[1] <= b[4] && a[4] >= b[1] &&
           a[2] <= b[5] && a[5] >= b[2];
}

// 1 when triangles a and b cross anywhere but at the corners they share
// (bit-identical points), 2 when they are the same three points, else 0.
int triangles_cross(const std::array<float,3>* const a[3],
                    const std::array<float,3>* const b[3]);

// Faces in Morton order under an implicit tree of boxes (heap layout, root 1,
// `leaves` leaves of kLeaf faces).
struct FaceBVH {
    static constexpr int kLeaf = 4;
    std::vector<int> face;    // tree order -> face id
    std::vector<Box3> box;    // per tree position
    std::vector<Box3> node;
    int leaves = 0;

    void build(const MeshData& mesh, const std::vector<int>& faces);
    // fn(tree position) for each face from position `from` on whose box
    // overlaps `b`; stops and returns true as soon as fn does.
    template <class Fn> bool visit(const Box3& b, int from, Fn&& fn) const {
        struct Visit { int node, first, count; };
        Visit stack[64];
        int sp = 0;
        const int n = (int)face.size();
        if (n == 0) return false;
        stack[sp++] = {1, 0, leaves};
        while (sp > 0) {
            const Visit it = stack[--sp];
            if ((it.first + it.count) * kLeaf <= from || !boxes_overlap(node[it.node], b))
                continue;
            if (it.node >= leaves) {
                const int lo = std::max(it.first * kLeaf, from);
                const int hi = std::min(it.first * kLeaf + kLeaf, n);
                for (int j = lo; j < hi; ++j)
                    if (boxes_overlap(box[j], b) && fn(j)) return true;
                continue;
            }
            const int half = it.count / 2;
            stack[sp++] = {2 * it.node + 1, it.first + half, half};
            stack[sp++] = {2 * it.node, it.first, half};
        }
        return false;
    }
};

// The report's lines, prefixed by `tag` (e.g. the pipeline stage it follows).
std::vector<std::string> format_mesh_check(const MeshCheckReport& r,
                                           const MeshCheckLimits& lim,
                                           const std::string& tag);

}  // namespace meshing

#endif

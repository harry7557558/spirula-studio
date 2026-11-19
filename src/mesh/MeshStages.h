/*
 * MeshStages.h
 *
 * The stages between a field and a written mesh, and what they share: surface
 * extraction (MeshExtract.cpp), the merge (MeshMerge.cpp), cleanup
 * (MeshCleanup.cpp, MeshHoles.cpp), local repair (MeshQuality.cpp) and the
 * last cut-and-refill (MeshRepair.cpp). MeshingHost.cpp runs them in order.
 * Internal to src/mesh.
 */
#pragma once

#include "mesh/MeshCheck.h"   // the triangle-pair test the edits share
#include "mesh/Meshing.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace meshing {

using Clock = std::chrono::steady_clock;
inline double secs_since(Clock::time_point t0) {
    return std::chrono::duration<double>(Clock::now() - t0).count();
}

// The stages work on the export container; they touch only V, F and C.
using Mesh = MeshData;

// A vertex's neighbours (or incident triangles) during the merge. Valences are
// ~6, so a linear scan over inline storage beats hashing, and it costs 40 bytes
// per vertex where std::unordered_set cost ~330 (5.8 GB of an 8.3 GB peak).
class VertexSet {
public:
    VertexSet() {}
    ~VertexSet() { if (cap_ > kInline) std::free(heap_); }
    VertexSet(const VertexSet&) = delete;
    VertexSet& operator=(const VertexSet&) = delete;
    VertexSet(VertexSet&& o) noexcept : n_(o.n_), cap_(o.cap_) {
        if (cap_ > kInline) heap_ = o.heap_;
        else std::memcpy(inl_, o.inl_, sizeof inl_);
        o.n_ = 0; o.cap_ = kInline;
    }

    const int* begin() const { return data(); }
    const int* end()   const { return data() + n_; }
    uint32_t size() const { return n_; }
    bool empty() const { return n_ == 0; }

    const int* find(int x) const {
        const int* d = data();
        for (uint32_t i = 0; i < n_; ++i) if (d[i] == x) return d + i;
        return d + n_;
    }
    void insert(int x) {
        int* d = data();
        for (uint32_t i = 0; i < n_; ++i) if (d[i] == x) return;
        if (n_ == cap_) { grow(); d = data(); }
        d[n_++] = x;
    }
    void erase(int x) {
        int* d = data();
        for (uint32_t i = 0; i < n_; ++i)
            if (d[i] == x) { d[i] = d[--n_]; return; }
    }
    void clear() {
        if (cap_ > kInline) { std::free(heap_); cap_ = kInline; }
        n_ = 0;
    }
    // Drop entries flagged dead. Every reader already skips them, so this only
    // stops a collapse chain from accumulating them without bound.
    void retain(const char* alive) {
        int* d = data();
        uint32_t k = 0;
        for (uint32_t i = 0; i < n_; ++i) if (alive[d[i]]) d[k++] = d[i];
        n_ = k;
    }

private:
    static constexpr uint32_t kInline = 8;
    int* data() { return cap_ > kInline ? heap_ : inl_; }
    const int* data() const { return cap_ > kInline ? heap_ : inl_; }
    void grow() {
        uint32_t nc = cap_ * 2;
        int* np = (int*)std::malloc((size_t)nc * sizeof(int));
        std::memcpy(np, data(), (size_t)n_ * sizeof(int));
        if (cap_ > kInline) std::free(heap_);
        heap_ = np; cap_ = nc;
    }
    uint32_t n_ = 0, cap_ = kInline;
    union { int inl_[kInline]; int* heap_; };
};

// ---------------------------------------------------------------------------
// Vertex -> incident faces, as offsets into one array.
// ---------------------------------------------------------------------------
struct VertexFaces {
    std::vector<int> off, inc;
    explicit VertexFaces(const Mesh& mesh) : off(mesh.V.size() + 1, 0) {
        for (const auto& f : mesh.F)
            for (int k = 0; k < 3; ++k) ++off[f[k] + 1];
        for (size_t v = 0; v < mesh.V.size(); ++v) off[v + 1] += off[v];
        inc.resize(off.back());
        std::vector<int> fill(off.begin(), off.end() - 1);
        for (size_t t = 0; t < mesh.F.size(); ++t)
            for (int k = 0; k < 3; ++k) inc[fill[mesh.F[t][k]]++] = (int)t;
    }
};

// Largest distance a collapse may move the surface (`tol`) and longest edge it
// may leave (`max_edge`), per vertex, in world units; with `rgb`, the merged
// colours must also stay within `color_tol` per channel.
struct MergeLimits {
    std::vector<float> tol, max_edge;
    std::vector<std::array<float,3>> rgb;
    float color_tol = 0.0f;
};

// Each vertex's position along a Morton curve over the vertices' box.
std::vector<int> morton_rank(const std::vector<std::array<float,3>>& V);

// True when every face of both lists is within 45 degrees of their mean
// normal: such a patch is a height field over that plane and cannot cross itself.
template <class Pt>
bool nearly_flat(Pt&& pt, const std::vector<std::array<int,3>>& next,
                        const std::vector<std::array<int,3>>& around,
                        float min_cos = 0.70710678f) {
    float mean[3] = {0, 0, 0};
    auto normal = [&](const std::array<int,3>& f, float* n) {
        const auto &a = pt(f[0]), &b = pt(f[1]), &c = pt(f[2]);
        const float e1[3] = {b[0]-a[0], b[1]-a[1], b[2]-a[2]};
        const float e2[3] = {c[0]-a[0], c[1]-a[1], c[2]-a[2]};
        n[0] = e1[1]*e2[2] - e1[2]*e2[1];
        n[1] = e1[2]*e2[0] - e1[0]*e2[2];
        n[2] = e1[0]*e2[1] - e1[1]*e2[0];
    };
    float n[3];
    for (const auto& f : next) {
        normal(f, n);
        for (int a = 0; a < 3; ++a) mean[a] += n[a];
    }
    const float lm = std::sqrt(mean[0]*mean[0] + mean[1]*mean[1] + mean[2]*mean[2]);
    if (!(lm > 0.0f)) return false;
    auto ok = [&](const std::array<int,3>& f) {
        normal(f, n);
        const float l = std::sqrt(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
        return n[0]*mean[0] + n[1]*mean[1] + n[2]*mean[2] > min_cos * l * lm;
    };
    for (const auto& f : next) if (!ok(f)) return false;
    for (const auto& f : around) if (!ok(f)) return false;
    return true;
}

// True when faces `next` cross one another or any face in `around`, corners
// placed by pt(); local crossings are nearly all an edit can make.
template <class Pt>
bool faces_cross(Pt&& pt, const std::vector<std::array<int,3>>& next,
                        const std::vector<std::array<int,3>>& around) {
    auto fbox = [&](const std::array<int,3>& f) {
        Box3 b = {1e30f, 1e30f, 1e30f, -1e30f, -1e30f, -1e30f};
        for (int w : f)
            for (int a = 0; a < 3; ++a) {
                b[a] = std::min(b[a], pt(w)[a]);
                b[3 + a] = std::max(b[3 + a], pt(w)[a]);
            }
        return b;
    };
    auto cross = [&](const std::array<int,3>& f, const std::array<int,3>& g) {
        const std::array<float,3>* a[3] = {&pt(f[0]), &pt(f[1]), &pt(f[2])};
        const std::array<float,3>* b[3] = {&pt(g[0]), &pt(g[1]), &pt(g[2])};
        return triangles_cross(a, b) != 0;
    };
    constexpr size_t kMax = 64;
    if (next.size() > kMax) return true;
    Box3 all = {1e30f, 1e30f, 1e30f, -1e30f, -1e30f, -1e30f};
    Box3 boxes[kMax];
    for (size_t i = 0; i < next.size(); ++i) {
        boxes[i] = fbox(next[i]);
        for (int a = 0; a < 3; ++a) {
            all[a] = std::min(all[a], boxes[i][a]);
            all[3 + a] = std::max(all[3 + a], boxes[i][3 + a]);
        }
        for (size_t j = 0; j < i; ++j)
            if (boxes_overlap(boxes[i], boxes[j]) && cross(next[i], next[j])) return true;
    }
    for (const auto& g : around) {
        const Box3 gb = fbox(g);
        if (!boxes_overlap(gb, all)) continue;
        for (size_t i = 0; i < next.size(); ++i)
            if (boxes_overlap(boxes[i], gb) && cross(next[i], g)) return true;
    }
    return false;
}

// Adjacent faces turning more than 168 deg fold onto each other (MeshCheck: 170).
constexpr float kFoldCos = 0.97814760f;

// The last repair's limits, inside MeshCheck's so a refill does not land just
// past them.
constexpr float kRepairMinSin = 0.02617695f;   // sin 1.5 deg; MeshCheck: 1
constexpr float kRepairMaxAspect = 25.0f;      // MeshCheck: 30
constexpr float kRepairFlatCos = 0.84804810f;  // cos 32 deg; MeshCheck: 30
constexpr int kRepairMaxValence = 15;          // MeshCheck: 16

// ---------------------------------------------------------------------------
// Extraction (MeshExtract.cpp)
// ---------------------------------------------------------------------------

// The opacity field's surface: Delaunay over splat samples and the cameras
// (`cam_pos`, may be null), marching tetrahedra at `cfg.iso`. False when there
// is no surface to extract.
bool extract_opacity_field(OccupancyEvaluator& ev, const MeshingConfig& cfg,
                           MeshProgress& prog, Clock::time_point t0, Mesh& mesh,
                           const float* cam_pos, int num_cameras);

// Every camera's depth fused into a TSDF over a uniform grid of
// `cfg.tsdf_voxel`, and its zero level (docs/notes/tsdf.md).
bool extract_tsdf(OccupancyEvaluator& ev, const CameraParams& cams, int num_cameras,
                  const MeshingConfig& cfg, MeshProgress& prog, Mesh& mesh);

// ---------------------------------------------------------------------------
// Merge (MeshMerge.cpp, docs/notes/mesh-merge.md)
// ---------------------------------------------------------------------------

// Collapses edges, lowest error first, while the surface moves less than both
// ends' limits; with `quadrics`, against the mesh the pass started from.
void merge_vertices(Mesh& mesh, MergeLimits lim, float max_flip_deg,
                    bool quadrics, bool verbose, int num_threads);

// ---------------------------------------------------------------------------
// Cleanup (MeshCleanup.cpp, MeshHoles.cpp)
// ---------------------------------------------------------------------------

// Winds neighbours alike, closed parts outward, open ones by most guesses.
void orient_mesh(Mesh& mesh, const float* cam_pos, int num_cameras);
// Flips the seen (1) and then the unseen (0) vertices in patches, joined by
// edges, of under `min_patch`; a part left with nothing seen stays unseen.
// Returns how many unseen vertices it kept.
long despeckle_visibility(const Mesh& mesh, std::vector<unsigned char>& visible, int min_patch);
// Keeps one of the faces on each vertex triple.
void dedup_faces(Mesh& mesh, bool verbose);
// Drops connected parts under `min_faces` faces; <= 1 disables.
void remove_floaters(Mesh& mesh, int min_faces, bool verbose);
// One copy of a bowtie vertex per fan of its faces.
void split_nonmanifold_vertices(Mesh& mesh, bool verbose);

// Fills boundary loops under `ratio` of their part's size or `max_edges` edges.
// With `was_boundary` (per vertex, before faces were cut), a loop touching no
// such vertex is a cut's hole and is filled whatever its size.
void fill_holes(Mesh& mesh, float ratio, int max_edges, bool verbose,
                const std::vector<char>* was_boundary = nullptr);

// ---------------------------------------------------------------------------
// Local repair (MeshQuality.cpp) and the last cut (MeshRepair.cpp)
// ---------------------------------------------------------------------------

// Slivers, folds and flat valence-3 vertices by collapses and flips that keep
// the mesh manifold and crossing-free. angle_deg <= 0 disables.
void fix_degenerate_faces(Mesh& mesh, float angle_deg, bool verbose, int num_threads);
// Valence-improving flips and tangential smoothing where the surface is flat.
void improve_mesh_quality(Mesh& mesh, int iters, bool verbose, int num_threads);

// Marks the faces to cut (crossing, or around a defect); returns how many.
long mark_defects(const Mesh& mesh, std::vector<char>& cut);
// Drops the marked faces and orphaned vertices; the old index of each one kept.
std::vector<int> cut_faces(Mesh& mesh, const std::vector<char>& cut);
// At each bowtie vertex, cuts every fan but the largest.
void drop_minor_fans(Mesh& mesh);
// Vertices on an edge with one face.
std::vector<char> boundary_vertices(const Mesh& mesh);

} // namespace meshing

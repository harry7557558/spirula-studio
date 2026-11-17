#include "mesh/Tsdf.h"

#include "data/CameraMath.h"
#include "mesh/MarchingTets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace meshing {

namespace {

constexpr int kB = 8;                   // voxels per block edge
constexpr int kBV = kB * kB * kB;
constexpr int64_t kBias = 1 << 19;      // grid coordinates are packed 20 bits each
constexpr int kOrthographic = 4;        // CameraModelType::ORTHOGRAPHIC

inline int64_t pack3(int64_t x, int64_t y, int64_t z) {
    return (x + kBias) | ((y + kBias) << 20) | ((z + kBias) << 40);
}
// A block origin whose block, and the voxel past its far faces, packs.
inline bool in_range(int64_t x, int64_t y, int64_t z) {
    const int64_t hi = kBias - 2 * kB;
    return x > -kBias && x < hi && y > -kBias && y < hi && z > -kBias && z < hi;
}
inline int local(int x, int y, int z) { return x + kB * (y + kB * z); }

// The six tetrahedra of a cube along its main diagonal, corners as bit sets
// (1 = +x, 2 = +y, 4 = +z): each a monotone path from corner 0 to corner 7, so
// neighbouring cubes split their shared faces the same way.
constexpr int kKuhn[6][4] = {{0, 1, 3, 7}, {0, 1, 5, 7}, {0, 2, 3, 7},
                             {0, 2, 6, 7}, {0, 4, 5, 7}, {0, 4, 6, 7}};

int thread_count() {
#ifdef _OPENMP
    return omp_get_max_threads();
#else
    return 1;
#endif
}
int thread_id() {
#ifdef _OPENMP
    return omp_get_thread_num();
#else
    return 0;
#endif
}

struct Block {
    float sdf[kBV];   // running mean of the truncated distance, in units of trunc
    float w[kBV];     // observations folded in
};

}  // namespace

void moments_to_depth(const float* moments, size_t n, float* depth) {
    const float half = std::sqrt(std::log(2.0f));   // (c0 + c1 z)^2 = ln 2
    #pragma omp parallel for schedule(static)
    for (long i = 0; i < (long)n; ++i) {
        const float m0 = moments[3 * i], c0 = moments[3 * i + 1], c1 = moments[3 * i + 2];
        float d = 0.0f;
        if (m0 >= 0.5f && c1 > 0.0f) d = (half - c0) / c1;
        depth[i] = std::isfinite(d) && d > 0.0f ? d : 0.0f;
    }
}

struct TsdfVolume::Impl {
    float voxel, trunc;
    bool have_origin = false;
    double origin[3] = {0, 0, 0};
    std::vector<Block> blocks;
    std::vector<std::array<int64_t, 3>> coord;   // block coordinates
    EdgeTable index{1024};                       // pack3(block) -> block id

    // Unit camera-space rays per pixel (orthographic: the pixel's plane offset),
    // kept while consecutive cameras share their intrinsics.
    std::vector<float> rays;
    std::vector<uint8_t> ray_ok;
    TsdfCamera rays_for{};
    bool rays_valid = false;

    bool same_lens(const TsdfCamera& a, const TsdfCamera& b) const {
        return a.width == b.width && a.height == b.height && a.model == b.model &&
               a.distortion == b.distortion && a.fx == b.fx && a.fy == b.fy &&
               a.cx == b.cx && a.cy == b.cy &&
               std::memcmp(a.dist, b.dist, sizeof a.dist) == 0;
    }

    void ensure_rays(const TsdfCamera& c) {
        if (rays_valid && same_lens(c, rays_for)) return;
        const long n = (long)c.width * c.height;
        rays.assign((size_t)n * 3, 0.0f);
        ray_ok.assign((size_t)n, 0);
        #pragma omp parallel for schedule(static)
        for (long p = 0; p < n; ++p) {
            const double u = ((double)(p % c.width) + 0.5 - c.cx) / c.fx;
            const double v = ((double)(p / c.width) + 0.5 - c.cy) / c.fy;
            double r[3];
            if (c.model == kOrthographic) {
                r[0] = u; r[1] = v; r[2] = 0.0;
            } else if (!camhost::generate_ray(u, v, c.model, c.distortion, c.dist, r)) {
                continue;
            }
            for (int k = 0; k < 3; ++k) rays[3 * p + k] = (float)r[k];
            ray_ok[p] = 1;
        }
        rays_for = c;
        rays_valid = true;
    }

    int block_id(int64_t bx, int64_t by, int64_t bz) const {
        return index.find(pack3(bx, by, bz));
    }
};

TsdfVolume::TsdfVolume(float voxel, float trunc) : impl_(new Impl) {
    if (!(voxel > 0.0f) || !(trunc > 0.0f))
        throw std::invalid_argument("TsdfVolume: voxel and truncation must be positive");
    impl_->voxel = voxel;
    impl_->trunc = trunc;
}

TsdfVolume::~TsdfVolume() = default;

size_t TsdfVolume::num_blocks() const { return impl_->blocks.size(); }
size_t TsdfVolume::bytes() const {
    return impl_->blocks.capacity() * sizeof(Block) +
           impl_->coord.capacity() * sizeof(impl_->coord[0]);
}

void TsdfVolume::integrate(const TsdfCamera& c, const float* depth, float max_depth) {
    Impl& m = *impl_;
    const bool ortho = c.model == kOrthographic;
    double R[9], t[3], o[3];
    for (int r = 0; r < 3; ++r) {
        for (int k = 0; k < 3; ++k) R[3 * r + k] = c.viewmat[4 * r + k];
        t[r] = c.viewmat[4 * r + 3];
    }
    for (int k = 0; k < 3; ++k) o[k] = -(R[k] * t[0] + R[3 + k] * t[1] + R[6 + k] * t[2]);
    if (!m.have_origin) {
        std::copy(o, o + 3, m.origin);
        m.have_origin = true;
    }
    m.ensure_rays(c);

    const long npix = (long)c.width * c.height;
    const double block_w = (double)m.voxel * kB;
    auto usable = [&](float d) { return d > 0.0f && (max_depth <= 0.0f || d <= max_depth); };

    // ---- the blocks each pixel's band [d - trunc, d + trunc] passes through ----
    std::vector<std::vector<int64_t>> found((size_t)thread_count());
    #pragma omp parallel
    {
        std::vector<int64_t>& mine = found[(size_t)thread_id()];
        EdgeTable seen(4096);   // this thread's blocks so far: neighbours mostly repeat
        #pragma omp for schedule(static)
        for (long p = 0; p < npix; ++p) {
            const float d = depth[p];
            if (!usable(d) || !m.ray_ok[p]) continue;
            const float* r = &m.rays[3 * p];
            double dir[3], base[3];
            for (int k = 0; k < 3; ++k) {
                const double cam_dir[3] = {ortho ? 0.0 : r[0], ortho ? 0.0 : r[1], ortho ? 1.0 : r[2]};
                dir[k] = R[k] * cam_dir[0] + R[3 + k] * cam_dir[1] + R[6 + k] * cam_dir[2];
                // Orthographic rays start on the image plane at the pixel's offset.
                base[k] = o[k] + (ortho ? R[k] * r[0] + R[3 + k] * r[1] : 0.0) - m.origin[k];
            }
            // Steps of trunc/2 stay under a block, so no block on the band is skipped.
            for (int s = -2; s <= 2; ++s) {
                const double along = d + 0.5 * s * m.trunc;
                if (along <= 0.0) continue;
                const int64_t bx = (int64_t)std::floor((base[0] + along * dir[0]) / block_w);
                const int64_t by = (int64_t)std::floor((base[1] + along * dir[1]) / block_w);
                const int64_t bz = (int64_t)std::floor((base[2] + along * dir[2]) / block_w);
                if (!in_range(bx * kB, by * kB, bz * kB)) continue;
                const int64_t key = pack3(bx, by, bz);
                if (seen.insert(key, 0) < 0) mine.push_back(key);
            }
        }
    }
    std::vector<int64_t> keys;
    for (auto& f : found) keys.insert(keys.end(), f.begin(), f.end());
    std::vector<std::vector<int64_t>>().swap(found);
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());

    std::vector<int> touched(keys.size());
    for (size_t i = 0; i < keys.size(); ++i) {
        const int id = (int)m.blocks.size();
        const int had = m.index.insert(keys[i], id);
        if (had >= 0) { touched[i] = had; continue; }
        touched[i] = id;
        m.blocks.emplace_back();
        Block& b = m.blocks.back();
        std::fill(b.sdf, b.sdf + kBV, 0.0f);
        std::fill(b.w, b.w + kBV, 0.0f);
        const int64_t k = keys[i];
        m.coord.push_back({(k & 0xFFFFF) - kBias, ((k >> 20) & 0xFFFFF) - kBias,
                           ((k >> 40) & 0xFFFFF) - kBias});
    }

    // ---- fold this camera's distances into every voxel of those blocks ----
    const float inv_trunc = 1.0f / m.trunc;
    #pragma omp parallel for schedule(dynamic, 16)
    for (long i = 0; i < (long)touched.size(); ++i) {
        Block& b = m.blocks[(size_t)touched[(size_t)i]];
        const auto& bc = m.coord[(size_t)touched[(size_t)i]];
        for (int z = 0; z < kB; ++z)
        for (int y = 0; y < kB; ++y)
        for (int x = 0; x < kB; ++x) {
            const double g[3] = {m.origin[0] + (double)(bc[0] * kB + x) * m.voxel,
                                 m.origin[1] + (double)(bc[1] * kB + y) * m.voxel,
                                 m.origin[2] + (double)(bc[2] * kB + z) * m.voxel};
            double pc[3];
            for (int r = 0; r < 3; ++r)
                pc[r] = R[3 * r] * g[0] + R[3 * r + 1] * g[1] + R[3 * r + 2] * g[2] + t[r];
            double uv[2], dist;
            if (ortho) {
                if (pc[2] <= 0.0) continue;
                uv[0] = pc[0]; uv[1] = pc[1];
                dist = pc[2];
            } else {
                if (!camhost::project_ray(pc, c.model, c.distortion, c.dist, uv)) continue;
                dist = std::sqrt(pc[0] * pc[0] + pc[1] * pc[1] + pc[2] * pc[2]);
            }
            const double px = uv[0] * c.fx + c.cx, py = uv[1] * c.fy + c.cy;
            if (!(px >= 0.0 && py >= 0.0 && px < c.width && py < c.height)) continue;
            const float d = depth[(long)py * c.width + (long)px];
            if (!usable(d)) continue;
            const float sdf = (float)(d - dist);
            if (sdf < -m.trunc) continue;
            const int l = local(x, y, z);
            const float v = std::min(sdf, m.trunc) * inv_trunc;
            b.sdf[l] = (b.sdf[l] * b.w[l] + v) / (b.w[l] + 1.0f);
            b.w[l] += 1.0f;
        }
    }
}

void TsdfVolume::extract(MeshData& mesh) const {
    const Impl& m = *impl_;
    const long nb = (long)m.blocks.size();
    mesh.V.clear();
    mesh.F.clear();
    if (nb == 0) return;

    // The block of a voxel up to one past a block's far faces.
    std::vector<std::array<int, 8>> nbr((size_t)nb);
    #pragma omp parallel for schedule(static)
    for (long i = 0; i < nb; ++i) {
        const auto& c = m.coord[(size_t)i];
        for (int k = 0; k < 8; ++k)
            nbr[(size_t)i][k] = k == 0 ? (int)i
                : m.block_id(c[0] + (k & 1), c[1] + ((k >> 1) & 1), c[2] + ((k >> 2) & 1));
    }
    // (sdf, weight) at block-local (x, y, z), each in 0..kB; weight 0 when absent.
    auto at = [&](long i, int x, int y, int z, float& sdf) -> float {
        const int k = (x >= kB ? 1 : 0) | (y >= kB ? 2 : 0) | (z >= kB ? 4 : 0);
        const int id = nbr[(size_t)i][k];
        if (id < 0) return 0.0f;
        const Block& b = m.blocks[(size_t)id];
        const int l = local(x & (kB - 1), y & (kB - 1), z & (kB - 1));
        sdf = b.sdf[l];
        return b.w[l];
    };
    auto grid = [&](long i, int x, int y, int z, double out[3]) {
        const auto& c = m.coord[(size_t)i];
        out[0] = m.origin[0] + (double)(c[0] * kB + x) * m.voxel;
        out[1] = m.origin[1] + (double)(c[1] * kB + y) * m.voxel;
        out[2] = m.origin[2] + (double)(c[2] * kB + z) * m.voxel;
    };
    // A cut edge belongs to its lower grid point: key = point * 8 + direction bits.
    auto edge_key_of = [&](long i, int x, int y, int z, int dir) -> int64_t {
        const auto& c = m.coord[(size_t)i];
        return pack3(c[0] * kB + x, c[1] * kB + y, c[2] * kB + z) * 8 + dir;
    };

    // ---- vertices: every edge from a grid point along the 7 positive directions ----
    struct Vert { int64_t key; float p[3]; };
    std::vector<std::vector<Vert>> vfound((size_t)thread_count());
    #pragma omp parallel
    {
        std::vector<Vert>& mine = vfound[(size_t)thread_id()];
        #pragma omp for schedule(dynamic, 16)
        for (long i = 0; i < nb; ++i)
        for (int z = 0; z < kB; ++z)
        for (int y = 0; y < kB; ++y)
        for (int x = 0; x < kB; ++x) {
            float sa = 0.0f;
            if (at(i, x, y, z, sa) <= 0.0f) continue;
            for (int dir = 1; dir < 8; ++dir) {
                const int dx = dir & 1, dy = (dir >> 1) & 1, dz = (dir >> 2) & 1;
                float sb = 0.0f;
                if (at(i, x + dx, y + dy, z + dz, sb) <= 0.0f) continue;
                if ((sa > 0.0f) == (sb > 0.0f)) continue;
                const double f = (double)sa / ((double)sa - (double)sb);
                double pa[3];
                grid(i, x, y, z, pa);
                const double off[3] = {(double)dx, (double)dy, (double)dz};
                Vert v{edge_key_of(i, x, y, z, dir), {}};
                for (int k = 0; k < 3; ++k) v.p[k] = (float)(pa[k] + f * off[k] * m.voxel);
                mine.push_back(v);
            }
        }
    }
    size_t nv = 0;
    for (const auto& f : vfound) nv += f.size();
    EdgeTable vid(nv + nv / 2 + 16);
    mesh.V.reserve(nv);
    for (auto& f : vfound) {
        for (const Vert& v : f) {
            vid.insert(v.key, (int)mesh.V.size());
            mesh.V.push_back({v.p[0], v.p[1], v.p[2]});
        }
        std::vector<Vert>().swap(f);
    }

    // ---- triangles: six tetrahedra per cube whose corners were all observed ----
    std::vector<std::vector<std::array<int, 3>>> ffound((size_t)thread_count());
    #pragma omp parallel
    {
        std::vector<std::array<int, 3>>& mine = ffound[(size_t)thread_id()];
        #pragma omp for schedule(dynamic, 16)
        for (long i = 0; i < nb; ++i)
        for (int z = 0; z < kB; ++z)
        for (int y = 0; y < kB; ++y)
        for (int x = 0; x < kB; ++x) {
            float s[8], w[8];
            bool any = false;
            for (int c = 0; c < 8; ++c) {
                s[c] = 0.0f;
                w[c] = at(i, x + (c & 1), y + ((c >> 1) & 1), z + ((c >> 2) & 1), s[c]);
                any = any || w[c] > 0.0f;
            }
            if (!any) continue;
            for (const auto& tet : kKuhn) {
                int code = 0;
                bool seen = true;
                for (int k = 0; k < 4; ++k) {
                    seen = seen && w[tet[k]] > 0.0f;
                    if (s[tet[k]] > 0.0f) code |= 1 << k;   // free side
                }
                if (!seen || code == 0 || code == 15) continue;
                // Free side minus solid side, in corner units: the outward direction.
                double out[3] = {0, 0, 0};
                int nfree = 0, nsolid = 0;
                double cfree[3] = {0, 0, 0}, csolid[3] = {0, 0, 0};
                for (int k = 0; k < 4; ++k) {
                    const int c = tet[k];
                    const double p[3] = {(double)(c & 1), (double)((c >> 1) & 1), (double)((c >> 2) & 1)};
                    double* acc = (code >> k) & 1 ? cfree : csolid;
                    for (int a = 0; a < 3; ++a) acc[a] += p[a];
                    ((code >> k) & 1 ? nfree : nsolid)++;
                }
                for (int a = 0; a < 3; ++a) out[a] = cfree[a] / nfree - csolid[a] / nsolid;

                const MTCase& mc = kMT[code];
                for (int ti = 0; ti < mc.ntri; ++ti) {
                    int v3[3];
                    bool ok = true;
                    for (int k = 0; k < 3; ++k) {
                        const int ca = tet[mc.e[ti][k][0]], cb = tet[mc.e[ti][k][1]];
                        const int lo = (ca & cb) == ca ? ca : cb, hi = lo == ca ? cb : ca;
                        v3[k] = vid.find(edge_key_of(i, x + (lo & 1), y + ((lo >> 1) & 1),
                                                     z + ((lo >> 2) & 1), lo ^ hi));
                        ok = ok && v3[k] >= 0;
                    }
                    if (!ok) continue;
                    const auto &A = mesh.V[(size_t)v3[0]], &B = mesh.V[(size_t)v3[1]],
                               &C = mesh.V[(size_t)v3[2]];
                    const double e1[3] = {B[0] - A[0], B[1] - A[1], B[2] - A[2]};
                    const double e2[3] = {C[0] - A[0], C[1] - A[1], C[2] - A[2]};
                    const double n[3] = {e1[1] * e2[2] - e1[2] * e2[1],
                                         e1[2] * e2[0] - e1[0] * e2[2],
                                         e1[0] * e2[1] - e1[1] * e2[0]};
                    if (n[0] * out[0] + n[1] * out[1] + n[2] * out[2] < 0.0) std::swap(v3[1], v3[2]);
                    if (v3[0] == v3[1] || v3[1] == v3[2] || v3[0] == v3[2]) continue;
                    mine.push_back({v3[0], v3[1], v3[2]});
                }
            }
        }
    }
    size_t nf = 0;
    for (const auto& f : ffound) nf += f.size();
    mesh.F.reserve(nf);
    for (auto& f : ffound) {
        mesh.F.insert(mesh.F.end(), f.begin(), f.end());
        std::vector<std::array<int, 3>>().swap(f);
    }
}

} // namespace meshing

/*
 * MeshUV.cpp -- see MeshUV.h. UV-atlas generation (charting + LSCM + packing)
 * and texture baking. Plain multithreaded host C++.
 */

#include "mesh/MeshUV.h"
#include "mesh/MeshLog.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace meshing {

namespace mmsg = spirula::i18n::msg::mesh;

namespace {

using Clock = std::chrono::steady_clock;
static double secs_since(Clock::time_point t0) {
    return std::chrono::duration<double>(Clock::now() - t0).count();
}

using V3 = std::array<float, 3>;
using V2 = std::array<double, 2>;

inline V3 sub(const V3& a, const V3& b) { return {a[0]-b[0], a[1]-b[1], a[2]-b[2]}; }
inline V3 cross(const V3& a, const V3& b) {
    return {a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]};
}
inline float dot3(const V3& a, const V3& b) { return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
inline float norm3(const V3& a) { return std::sqrt(dot3(a, a)); }

// ---------------------------------------------------------------------------
// Face adjacency over manifold edges (edges with != 2 faces give no adjacency)
// ---------------------------------------------------------------------------
static void build_face_adjacency(
    const std::vector<std::array<int,3>>& F, size_t nv,
    std::vector<std::array<int,3>>& adj   // [nf][3]; -1 = none; k-th entry is
                                          // across edge (f[k], f[k+1])
) {
    const size_t nf = F.size();
    adj.assign(nf, {-1, -1, -1});
    const int64_t P = (int64_t)nv;
    // edge -> (face*4 + corner) of the first two faces seen, -2 if >2 faces
    std::unordered_map<int64_t, std::array<int64_t,2>> emap;
    emap.reserve(nf * 3 / 2 + 1);
    for (size_t t = 0; t < nf; ++t)
        for (int k = 0; k < 3; ++k) {
            int a = F[t][k], b = F[t][(k+1)%3];
            int64_t lo = a < b ? a : b, hi = a < b ? b : a;
            auto& slot = emap.emplace(lo * P + hi,
                                      std::array<int64_t,2>{-1,-1}).first->second;
            int64_t enc = (int64_t)t * 4 + k;
            if (slot[0] < 0) slot[0] = enc;
            else if (slot[1] < 0) slot[1] = enc;
            else slot[0] = -2;  // non-manifold edge: no adjacency across it
        }
    for (const auto& kv : emap) {
        const auto& s = kv.second;
        if (s[0] < 0 || s[1] < 0) continue;   // boundary or non-manifold
        int t0 = (int)(s[0] >> 2), k0 = (int)(s[0] & 3);
        int t1 = (int)(s[1] >> 2), k1 = (int)(s[1] & 3);
        adj[t0][k0] = t1;
        adj[t1][k1] = t0;
    }
}

// ---------------------------------------------------------------------------
// 1. Chart segmentation
// ---------------------------------------------------------------------------

struct ChartSeg {
    std::vector<int> face_chart;            // [nf] chart id
    std::vector<std::vector<int>> charts;   // chart id -> face list
};

// Is `faces` (an edge-connected face set) flattenable as one chart? We
// require chi = V - E + F == 1 with a boundary present -- i.e. a disk up to
// boundary self-touch vertices ("bowties"), which LSCM handles gracefully
// (the pinch vertex gets one UV shared by both fans). Rejecting bowties
// outright shatters noisy meshes into tiny charts, so they are allowed.
// Closed pieces (no boundary) and genus/annulus pieces (chi != 1) must split.
static bool chart_is_disk(const std::vector<int>& faces,
                          const std::vector<std::array<int,3>>& F, size_t nv) {
    if (faces.size() <= 1) return true;
    std::unordered_map<int64_t, int> ecount;
    ecount.reserve(faces.size() * 2);
    std::unordered_map<int, int> vset;
    vset.reserve(faces.size() * 2);
    const int64_t P = (int64_t)nv;
    for (int t : faces) {
        for (int k = 0; k < 3; ++k) {
            int a = F[t][k], b = F[t][(k+1)%3];
            int64_t lo = a < b ? a : b, hi = a < b ? b : a;
            ecount[lo * P + hi] += 1;
            vset[a];  // touch
        }
    }
    long V = (long)vset.size(), E = (long)ecount.size(), Fc = (long)faces.size();
    if (V - E + Fc != 1) return false;
    bool has_bnd = false;
    for (const auto& kv : ecount) {
        if (kv.second > 2) return false;      // non-manifold edge inside chart
        if (kv.second == 1) has_bnd = true;   // boundary present -> disk-like
    }
    return has_bnd;                           // closed pieces must split
}

static ChartSeg segment_charts(
    const std::vector<V3>& /*V*/, const std::vector<std::array<int,3>>& F,
    size_t nv, const std::vector<std::array<int,3>>& adj,
    const std::vector<V3>& faceN_raw, const std::vector<float>& faceA,
    const UVAtlasConfig& cfg
) {
    const size_t nf = F.size();
    const float cos_thresh = std::cos(cfg.chart_angle_deg * 0.017453292519943295f);
    long cap = cfg.max_chart_faces > 0 ? cfg.max_chart_faces
             : std::min<long>(65536, std::max<long>(1024, (long)nf / 40));

    // Segment on SMOOTHED normals: extracted 3DGS surfaces are noisy at the
    // triangle scale (median adjacent-face normal angle of 20-40 deg), which
    // stalls cone-bounded growth immediately. A few rounds of area-weighted
    // neighbor averaging cancel the noise so charts follow the underlying
    // geometry instead of the bumps.
    std::vector<V3> faceN = faceN_raw;
    {
        std::vector<V3> tmp(nf);
        for (int round = 0; round < 3; ++round) {
            #pragma omp parallel for schedule(static)
            for (long t = 0; t < (long)nf; ++t) {
                V3 acc = {faceN[t][0] * faceA[t], faceN[t][1] * faceA[t],
                          faceN[t][2] * faceA[t]};
                for (int k = 0; k < 3; ++k) {
                    int nb = adj[t][k];
                    if (nb < 0) continue;
                    for (int a = 0; a < 3; ++a) acc[a] += faceN[nb][a] * faceA[nb];
                }
                float l = norm3(acc);
                tmp[t] = l > 1e-20f ? V3{acc[0]/l, acc[1]/l, acc[2]/l} : faceN[t];
            }
            faceN.swap(tmp);
        }
    }

    ChartSeg seg;
    seg.face_chart.assign(nf, -1);

    // ---- greedy normal-cone region growing, disk-by-construction ----
    // Priority queue keyed by deviation from the chart's running average
    // normal; the chart's shape stays compact because low-deviation faces are
    // absorbed first. Each chart's Euler characteristic is tracked
    // incrementally: an edge-adjacent face keeps the region a topological
    // disk iff it adds exactly one more edge than vertices (dE == dV + 1) --
    // this rejects pinches, handle closures and hole enclosures up front, so
    // no post-hoc chart splitting is needed. A face rejected only for
    // topology is retried a couple of times (it often becomes legal once the
    // chart grows around it); after that it is left for another seed.
    struct Cand { float cost; int face; int retries; };
    struct CandCmp { bool operator()(const Cand& a, const Cand& b) const {
        return a.cost > b.cost; } };
    std::priority_queue<Cand, std::vector<Cand>, CandCmp> heap;
    std::unordered_set<int64_t> ein;   // chart edge set (undirected key)
    std::unordered_set<int>     vin;   // chart vertex set
    const int64_t P = (int64_t)nv;
    auto ekey = [P](int a, int b) -> int64_t {
        int64_t lo = a < b ? a : b, hi = a < b ? b : a;
        return lo * P + hi;
    };

    std::vector<std::vector<int>> raw_charts;
    for (size_t seed = 0; seed < nf; ++seed) {
        if (seg.face_chart[seed] >= 0) continue;
        int cid = (int)raw_charts.size();
        raw_charts.emplace_back();
        auto& faces = raw_charts.back();
        V3 avgN = {faceN[seed][0]*faceA[seed], faceN[seed][1]*faceA[seed],
                   faceN[seed][2]*faceA[seed]};
        ein.clear();
        vin.clear();
        auto add_face = [&](int t) {
            seg.face_chart[t] = cid;
            faces.push_back(t);
            for (int a = 0; a < 3; ++a) avgN[a] += faceN[t][a] * faceA[t];
            for (int k = 0; k < 3; ++k) {
                vin.insert(F[t][k]);
                ein.insert(ekey(F[t][k], F[t][(k+1)%3]));
            }
        };
        add_face((int)seed);
        while (!heap.empty()) heap.pop();
        for (int k = 0; k < 3; ++k) {
            int nb = adj[seed][k];
            if (nb >= 0 && seg.face_chart[nb] < 0)
                heap.push({1.0f - dot3(faceN[nb], faceN[seed]), nb, 0});
        }
        while (!heap.empty() && (long)faces.size() < cap) {
            Cand c = heap.top(); heap.pop();
            if (seg.face_chart[c.face] >= 0) continue;
            float l = norm3(avgN);
            V3 cn = l > 1e-20f ? V3{avgN[0]/l, avgN[1]/l, avgN[2]/l} : faceN[c.face];
            if (dot3(faceN[c.face], cn) < cos_thresh) continue;  // stale/over-cone
            // disk test: dE == dV + 1
            const auto& f = F[c.face];
            int dV = 0, dE = 0;
            for (int k = 0; k < 3; ++k) {
                if (!vin.count(f[k])) ++dV;
                if (!ein.count(ekey(f[k], f[(k+1)%3]))) ++dE;
            }
            if (dE != dV + 1) {
                if (c.retries < 2)
                    heap.push({c.cost + 0.25f * (float)(c.retries + 1),
                               c.face, c.retries + 1});
                continue;
            }
            add_face(c.face);
            for (int k = 0; k < 3; ++k) {
                int nb = adj[c.face][k];
                if (nb >= 0 && seg.face_chart[nb] < 0)
                    heap.push({1.0f - dot3(faceN[nb], cn), nb, 0});
            }
        }
    }

    // ---- absorb tiny charts into their best neighbor ----
    // Growth stalls leave slivers of a handful of faces; each becomes its own
    // packed rectangle (seams + wasted atlas area). Merge any chart smaller
    // than kTinyChart into an adjacent chart, unconditionally on the cone --
    // a bit of distortion beats hundreds of micro-charts. A merge is only
    // taken when the interface is a single simple path (shared vertices =
    // shared edges + 1): both charts are disks, so the union then stays a
    // disk (chi = 1 + 1 - (V_shared - E_shared) = 1). Union-find resolves
    // merge chains; ascending-size order lets slivers clump first.
    const long kTinyChart = std::min<long>(32, cap);
    const size_t n_grown = raw_charts.size();
    {
        const int ncr = (int)raw_charts.size();
        std::vector<int> owner(ncr);
        for (int c = 0; c < ncr; ++c) owner[c] = c;
        std::function<int(int)> find = [&](int x) {
            while (owner[x] != x) { owner[x] = owner[owner[x]]; x = owner[x]; }
            return x;
        };
        // vertex -> incident faces (CSR), for interface vertex counting
        std::vector<int> vf_off(nv + 1, 0), vf_dat(nf * 3);
        for (size_t t = 0; t < nf; ++t)
            for (int k = 0; k < 3; ++k) vf_off[F[t][k] + 1]++;
        for (size_t i = 0; i < nv; ++i) vf_off[i + 1] += vf_off[i];
        {
            std::vector<int> cur(vf_off.begin(), vf_off.end() - 1);
            for (size_t t = 0; t < nf; ++t)
                for (int k = 0; k < 3; ++k) vf_dat[cur[F[t][k]]++] = (int)t;
        }
        std::vector<long> csize(ncr);
        std::vector<int> order(ncr);
        std::vector<std::vector<int>> members(ncr);   // root -> raw chart ids
        for (int c = 0; c < ncr; ++c) {
            csize[c] = (long)raw_charts[c].size();
            order[c] = c;
            members[c] = {c};
        }
        std::sort(order.begin(), order.end(),
                  [&](int a, int b) { return csize[a] < csize[b]; });
        std::unordered_map<int, long> shared;   // neighbor chart root -> edge count
        std::unordered_set<int> cverts;
        std::vector<int> union_faces;
        for (int c : order) {
            int rc = find(c);
            if (csize[rc] >= kTinyChart) continue;
            // interface edge counts per neighboring chart, over the whole
            // union owned by rc (tiny by the size check above)
            shared.clear();
            cverts.clear();
            union_faces.clear();
            for (int c2 : members[rc])
                union_faces.insert(union_faces.end(),
                                   raw_charts[c2].begin(), raw_charts[c2].end());
            for (int t : union_faces) {
                for (int k = 0; k < 3; ++k) {
                    cverts.insert(F[t][k]);
                    int nb = adj[t][k];
                    if (nb < 0) continue;
                    int rn = find(seg.face_chart[nb]);
                    if (rn != rc) shared[rn] += 1;
                }
            }
            // candidates by descending shared-edge count; take the first whose
            // interface is a simple path
            std::vector<std::pair<long,int>> cands;
            cands.reserve(shared.size());
            for (const auto& kv : shared) cands.push_back({kv.second, kv.first});
            std::sort(cands.rbegin(), cands.rend());
            for (const auto& [es, rn] : cands) {
                long vs = 0;
                for (int v : cverts) {
                    // v is shared iff some incident face belongs to chart rn
                    for (int o = vf_off[v]; o < vf_off[v + 1]; ++o)
                        if (find(seg.face_chart[vf_dat[o]]) == rn) { ++vs; break; }
                }
                if (vs != es + 1) continue;   // interface not a simple path
                owner[rc] = rn;
                csize[rn] += csize[rc];
                auto mv = std::move(members[rc]);
                members[rn].insert(members[rn].end(), mv.begin(), mv.end());
                break;
            }
        }
        // rebuild raw_charts under the merged ownership
        std::vector<std::vector<int>> merged(ncr);
        for (int c = 0; c < ncr; ++c) {
            int r = find(c);
            auto& dst = merged[r];
            if (dst.empty()) dst.swap(raw_charts[c]);
            else dst.insert(dst.end(), raw_charts[c].begin(), raw_charts[c].end());
        }
        raw_charts.clear();
        for (auto& m : merged)
            if (!m.empty()) {
                int cid = (int)raw_charts.size();
                for (int t : m) seg.face_chart[t] = cid;
                raw_charts.push_back(std::move(m));
            }
    }

    // ---- reduce every chart to edge-connected topological disks ----
    // Worklist: split off connected components, then BFS-halve anything that
    // is still not a disk. Terminates because sizes strictly decrease and a
    // single face is a disk.
    std::vector<std::vector<int>> work = std::move(raw_charts);
    seg.charts.clear();
    std::vector<char> in_set(nf, 0);
    std::vector<int> comp_stack;
    while (!work.empty()) {
        std::vector<int> faces = std::move(work.back());
        work.pop_back();
        if (faces.empty()) continue;

        // connected components within the set (via face adjacency)
        for (int t : faces) in_set[t] = 1;
        std::vector<std::vector<int>> comps;
        {
            std::vector<char> seen(faces.size(), 0);
            std::unordered_map<int,int> pos;
            pos.reserve(faces.size() * 2);
            for (size_t i = 0; i < faces.size(); ++i) pos[faces[i]] = (int)i;
            for (size_t i = 0; i < faces.size(); ++i) {
                if (seen[i]) continue;
                comps.emplace_back();
                auto& comp = comps.back();
                comp_stack.clear();
                comp_stack.push_back(faces[i]);
                seen[i] = 1;
                while (!comp_stack.empty()) {
                    int t = comp_stack.back(); comp_stack.pop_back();
                    comp.push_back(t);
                    for (int k = 0; k < 3; ++k) {
                        int nb = adj[t][k];
                        if (nb < 0 || !in_set[nb]) continue;
                        auto it = pos.find(nb);
                        if (it != pos.end() && !seen[it->second]) {
                            seen[it->second] = 1;
                            comp_stack.push_back(nb);
                        }
                    }
                }
            }
        }
        for (int t : faces) in_set[t] = 0;

        if (comps.size() > 1) {
            for (auto& c : comps) work.push_back(std::move(c));
            continue;
        }
        std::vector<int>& comp = comps[0];
        if (chart_is_disk(comp, F, nv)) {
            int cid = (int)seg.charts.size();
            for (int t : comp) seg.face_chart[t] = cid;
            seg.charts.push_back(std::move(comp));
            continue;
        }
        // BFS-halve (BFS prefix stays connected; the remainder may not, which
        // the next worklist round handles).
        for (int t : comp) in_set[t] = 1;
        std::vector<int> order;
        order.reserve(comp.size());
        {
            std::vector<char> vis_local(nf, 0);  // small charts: ok to reuse flags
            order.push_back(comp[0]);
            vis_local[comp[0]] = 1;
            for (size_t h = 0; h < order.size(); ++h) {
                int t = order[h];
                for (int k = 0; k < 3; ++k) {
                    int nb = adj[t][k];
                    if (nb >= 0 && in_set[nb] && !vis_local[nb]) {
                        vis_local[nb] = 1;
                        order.push_back(nb);
                    }
                }
            }
        }
        for (int t : comp) in_set[t] = 0;
        size_t half = order.size() / 2;
        if (half == 0) half = 1;
        work.emplace_back(order.begin(), order.begin() + half);
        work.emplace_back(order.begin() + half, order.end());
    }
    if (cfg.verbose)
        mlog::out(mlog::Stage::Uv, mmsg::uv_grew,
                  {(long long)n_grown, (long long)cap,
                   (long long)seg.charts.size()});
    return seg;
}

// ---------------------------------------------------------------------------
// 2. Per-chart parameterization (LSCM with planar fallback)
// ---------------------------------------------------------------------------

struct ChartParam {
    std::vector<int> verts;     // global vertex ids (local index order)
    std::vector<V2>  uv;        // per local vertex, area-normalized units
    double w = 0, h = 0;        // bbox after rotation (filled by orient step)
};

// LSCM for one chart. faces index into F; local vertex order defined here.
// target_area: desired chart UV area in texel-budget units (face count when
// no per-face weights are in use); the chart is rescaled to it on success.
// Returns false when even the fallback fails (degenerate chart) -- caller
// assigns a tiny dummy square.
static bool param_chart_lscm(
    const std::vector<int>& faces,
    const std::vector<V3>& V, const std::vector<std::array<int,3>>& F,
    double target_area,
    ChartParam& out
) {
    // ---- local indexing ----
    std::unordered_map<int, int> l2g;
    l2g.reserve(faces.size() * 2);
    out.verts.clear();
    auto local_id = [&](int g) {
        auto it = l2g.find(g);
        if (it != l2g.end()) return it->second;
        int id = (int)out.verts.size();
        l2g.emplace(g, id);
        out.verts.push_back(g);
        return id;
    };
    std::vector<std::array<int,3>> LF(faces.size());
    for (size_t i = 0; i < faces.size(); ++i)
        for (int k = 0; k < 3; ++k) LF[i][k] = local_id(F[faces[i]][k]);
    const int n = (int)out.verts.size();
    const int nt = (int)faces.size();
    out.uv.assign(n, {0.0, 0.0});
    if (n < 3) return false;

    // ---- boundary vertices (for pinning) ----
    std::unordered_map<int64_t, int> ecount;
    ecount.reserve(nt * 2);
    for (const auto& f : LF)
        for (int k = 0; k < 3; ++k) {
            int a = f[k], b = f[(k+1)%3];
            int64_t lo = a < b ? a : b, hi = a < b ? b : a;
            ecount[lo * (int64_t)n + hi] += 1;
        }
    std::vector<char> is_bnd(n, 0);
    for (const auto& kv : ecount)
        if (kv.second == 1) {
            is_bnd[(int)(kv.first / n)] = 1;
            is_bnd[(int)(kv.first % n)] = 1;
        }

    auto vpos = [&](int l) -> const V3& { return V[out.verts[l]]; };
    // farthest-pair heuristic over boundary vertices (fallback: all vertices)
    int b0 = -1;
    for (int i = 0; i < n; ++i) if (is_bnd[i]) { b0 = i; break; }
    bool use_all = b0 < 0;
    auto farthest_from = [&](int src) {
        int best = src;
        float bd = -1.0f;
        for (int i = 0; i < n; ++i) {
            if (!use_all && !is_bnd[i]) continue;
            float d = norm3(sub(vpos(i), vpos(src)));
            if (d > bd) { bd = d; best = i; }
        }
        return best;
    };
    if (use_all) b0 = 0;
    int pin1 = farthest_from(b0);
    int pin2 = farthest_from(pin1);
    if (pin1 == pin2) return false;
    double pin_d = norm3(sub(vpos(pin2), vpos(pin1)));
    if (!(pin_d > 0)) return false;

    // ---- assemble the conformal least-squares rows ----
    // For each triangle with local 2D coords p0,p1,p2 and doubled area dT,
    // the complex residual sum_r (W_r.x + i W_r.y)(u_r + i v_r) / sqrt(dT)
    // must vanish, where W_r = p_{r+2} - p_{r+1}.
    struct Row { int col[6]; double a[6]; int nc; };
    std::vector<Row> rows;
    rows.reserve((size_t)nt * 2);
    // unknown layout: [u_0..u_{n-1}, v_0..v_{n-1}], pins removed via rhs
    std::vector<double> rhs;
    rhs.reserve((size_t)nt * 2);
    // pinned values: pin1 -> (0,0), pin2 -> (pin_d, 0)
    auto pin_u = [&](int l) { return l == pin1 ? 0.0 : pin_d; };
    auto is_pin = [&](int l) { return l == pin1 || l == pin2; };

    for (int t = 0; t < nt; ++t) {
        const auto& f = LF[t];
        V3 e1 = sub(vpos(f[1]), vpos(f[0]));
        V3 e2 = sub(vpos(f[2]), vpos(f[0]));
        double l1 = norm3(e1);
        if (l1 < 1e-20) continue;
        double x2 = dot3(e2, e1) / l1;
        V3 nrm = cross(e1, e2);
        double y2 = norm3(nrm) / l1;
        double dT = l1 * y2;                 // 2 * area
        if (dT < 1e-20) continue;            // degenerate: contributes nothing
        double s = 1.0 / std::sqrt(dT);
        // local 2D coords
        double px[3] = {0.0, l1, x2};
        double py[3] = {0.0, 0.0, y2};
        double Wx[3], Wy[3];
        for (int r = 0; r < 3; ++r) {
            Wx[r] = (px[(r+2)%3] - px[(r+1)%3]) * s;
            Wy[r] = (py[(r+2)%3] - py[(r+1)%3]) * s;
        }
        // real row: sum_r Wx_r u_r - Wy_r v_r ; imag: sum_r Wy_r u_r + Wx_r v_r
        Row re{}, im{};
        re.nc = im.nc = 0;
        double rre = 0.0, rim = 0.0;
        for (int r = 0; r < 3; ++r) {
            int l = f[r];
            if (is_pin(l)) {
                // v is pinned to 0; only u contributes to the rhs
                rre -= Wx[r] * pin_u(l);
                rim -= Wy[r] * pin_u(l);
            } else {
                re.col[re.nc] = l;     re.a[re.nc++] = Wx[r];
                re.col[re.nc] = n + l; re.a[re.nc++] = -Wy[r];
                im.col[im.nc] = l;     im.a[im.nc++] = Wy[r];
                im.col[im.nc] = n + l; im.a[im.nc++] = Wx[r];
            }
        }
        rows.push_back(re); rhs.push_back(rre);
        rows.push_back(im); rhs.push_back(rim);
    }
    if (rows.empty()) return false;

    // ---- CGNR (CG on A^T A x = A^T b) with Jacobi preconditioning ----
    const int N2 = 2 * n;
    std::vector<double> diag(N2, 0.0);
    for (const auto& r : rows)
        for (int k = 0; k < r.nc; ++k) diag[r.col[k]] += r.a[k] * r.a[k];
    for (auto& d : diag) d = d > 1e-30 ? 1.0 / d : 0.0;

    std::vector<double> x(N2, 0.0), Ax(rows.size()), atr(N2), z(N2), p(N2), Ap(rows.size());
    auto apply_A = [&](const std::vector<double>& v, std::vector<double>& out_) {
        for (size_t r = 0; r < rows.size(); ++r) {
            double s2 = 0.0;
            for (int k = 0; k < rows[r].nc; ++k) s2 += rows[r].a[k] * v[rows[r].col[k]];
            out_[r] = s2;
        }
    };
    auto apply_At = [&](const std::vector<double>& v, std::vector<double>& out_) {
        std::fill(out_.begin(), out_.end(), 0.0);
        for (size_t r = 0; r < rows.size(); ++r)
            for (int k = 0; k < rows[r].nc; ++k)
                out_[rows[r].col[k]] += rows[r].a[k] * v[r];
    };

    // r0 = b - A*0 = b; atr = A^T r
    std::vector<double> res = rhs;
    apply_At(res, atr);
    double b_at_norm = 0.0;
    for (double v : atr) b_at_norm += v * v;
    if (b_at_norm < 1e-30) return false;   // rhs ~0: pins degenerate
    for (int i = 0; i < N2; ++i) z[i] = diag[i] * atr[i];
    p = z;
    double rz = 0.0;
    for (int i = 0; i < N2; ++i) rz += atr[i] * z[i];
    const int max_iters = std::min(6000, 20 * n + 100);
    const double tol2 = 1e-14 * b_at_norm;
    for (int it = 0; it < max_iters; ++it) {
        apply_A(p, Ap);
        double pAp = 0.0;
        for (double v : Ap) pAp += v * v;
        if (pAp < 1e-300) break;
        double alpha = rz / pAp;
        for (int i = 0; i < N2; ++i) x[i] += alpha * p[i];
        for (size_t r = 0; r < rows.size(); ++r) res[r] -= alpha * Ap[r];
        apply_At(res, atr);
        double at2 = 0.0;
        for (double v : atr) at2 += v * v;
        if (at2 < tol2) break;
        for (int i = 0; i < N2; ++i) z[i] = diag[i] * atr[i];
        double rz_new = 0.0;
        for (int i = 0; i < N2; ++i) rz_new += atr[i] * z[i];
        double beta = rz_new / std::max(rz, 1e-300);
        rz = rz_new;
        for (int i = 0; i < N2; ++i) p[i] = z[i] + beta * p[i];
    }

    for (int l = 0; l < n; ++l) {
        if (is_pin(l)) out.uv[l] = {pin_u(l), 0.0};
        else out.uv[l] = {x[l], x[n + l]};
    }

    // ---- validate: NaN / flipped / collapsed triangles ----
    // Flipped UV triangles OVERLAP in the atlas -- the bake then writes one
    // surface region's colors over another's, which shows up as patchwork.
    // So the tolerance is strict: a handful of (tiny, near-degenerate) flips
    // pass; anything more fails and the caller splits the chart and retries.
    // 3D face areas, for the collapse test below
    std::vector<double> a3(nt);
    double a3_sum = 0.0;
    for (int t = 0; t < nt; ++t) {
        const auto& f = LF[t];
        V3 nn = cross(sub(vpos(f[1]), vpos(f[0])), sub(vpos(f[2]), vpos(f[0])));
        a3[t] = 0.5 * (double)norm3(nn);
        a3_sum += a3[t];
    }
    auto uv_area2 = [&](int t) {   // signed doubled UV area of face t
        const auto& f = LF[t];
        double ax = out.uv[f[1]][0] - out.uv[f[0]][0];
        double ay = out.uv[f[1]][1] - out.uv[f[0]][1];
        double bx = out.uv[f[2]][0] - out.uv[f[0]][0];
        double by = out.uv[f[2]][1] - out.uv[f[0]][1];
        return ax * by - ay * bx;
    };
    auto acceptable = [&]() -> bool {
        for (const auto& uv : out.uv)
            if (!std::isfinite(uv[0]) || !std::isfinite(uv[1])) return false;
        long n_neg = 0, n_pos = 0;
        double uv_sum = 0.0;
        for (int t = 0; t < nt; ++t) {
            double a2 = uv_area2(t);
            if (a2 > 0) ++n_pos; else if (a2 < 0) ++n_neg;
            uv_sum += 0.5 * std::fabs(a2);
        }
        if (n_pos + n_neg == 0) return false;
        long flipped = std::min(n_pos, n_neg);
        if (flipped > std::max<long>(1, nt / 500)) return false;
        // collapsed faces: UV area far below the face's area-proportional
        // share means the whole face samples ~one texel -- it bakes as a flat
        // patch (folded planar projections were the main source: a fold maps
        // a face at |cos| ~ 0 of its share). Conformal maps under the 60-deg
        // chart cone keep per-face scale variation way above 2%, so a strict
        // relative floor separates the two cleanly.
        if (a3_sum > 0.0 && uv_sum > 0.0) {
            long n_collapsed = 0;
            for (int t = 0; t < nt; ++t) {
                double share = a3[t] / a3_sum;     // conformal expectation
                double got = 0.5 * std::fabs(uv_area2(t)) / uv_sum;
                if (share > 1e-9 && got < 0.02 * share) ++n_collapsed;
            }
            if (n_collapsed > std::max<long>(2, nt / 50)) return false;
        }
        return true;
    };
    if (!acceptable()) {
        // ---- fallback: project on the best-fit (area-weighted normal) plane
        //      (subject to the same acceptance test -- a folded chart fails it
        //      and is split further by the caller; single faces always pass) ----
        V3 avgN = {0, 0, 0};
        for (const auto& f : LF) {
            V3 nn = cross(sub(vpos(f[1]), vpos(f[0])), sub(vpos(f[2]), vpos(f[0])));
            for (int a = 0; a < 3; ++a) avgN[a] += nn[a];
        }
        float l = norm3(avgN);
        if (l < 1e-20f) return false;
        for (int a = 0; a < 3; ++a) avgN[a] /= l;
        V3 t0 = std::fabs(avgN[0]) < 0.9f ? V3{1, 0, 0} : V3{0, 1, 0};
        V3 tu = cross(avgN, t0);
        float lu = norm3(tu);
        for (int a = 0; a < 3; ++a) tu[a] /= lu;
        V3 tv = cross(avgN, tu);
        for (int i = 0; i < n; ++i)
            out.uv[i] = {(double)dot3(vpos(i), tu), (double)dot3(vpos(i), tv)};
        if (!acceptable()) return false;
    }

    // ---- normalize scale: UV area == target texel budget ----
    // The target follows how much detail the training views captured (the
    // caller sums per-face projected-pixel-area weights when cameras are
    // available, and falls back to the face count -- itself proportional to
    // the trained splat density -- without them). Plain world-area
    // normalization wastes most of an unbounded scene's atlas on huge distant
    // ground/wall charts.
    double area2 = 0.0;
    for (int t = 0; t < nt; ++t) area2 += 0.5 * std::fabs(uv_area2(t));
    if (!(area2 > 1e-300) || !(target_area > 0.0)) return false;
    double s = std::sqrt(target_area / area2);
    for (auto& uv : out.uv) { uv[0] *= s; uv[1] *= s; }

    // bbox sanity: a numerically collapsed ("line") parameterization can pass
    // the flip test yet be arbitrarily long once area-normalized -- one such
    // chart blows up the whole atlas scale. Reject anything whose extent is
    // wildly out of proportion to its intrinsic length scale sqrt(uv area).
    double x0 = 1e300, x1 = -1e300, y0 = 1e300, y1 = -1e300;
    for (const auto& uv : out.uv) {
        x0 = std::min(x0, uv[0]); x1 = std::max(x1, uv[0]);
        y0 = std::min(y0, uv[1]); y1 = std::max(y1, uv[1]);
    }
    double ext = std::max(x1 - x0, y1 - y0);
    if (!std::isfinite(ext) || ext > 100.0 * std::sqrt(std::max(target_area, 1e-4)))
        return false;
    return true;
}

// ---------------------------------------------------------------------------
// 3. Orientation (min-area rotation) + skyline packing
// ---------------------------------------------------------------------------

// Rotate chart UVs to the min-area bounding rectangle (rotating calipers over
// the convex hull), translate to origin, fill w/h.
static void orient_chart(ChartParam& c) {
    const size_t n = c.uv.size();
    if (n == 0) { c.w = c.h = 0; return; }
    // convex hull (Andrew monotone chain)
    std::vector<V2> pts = c.uv;
    std::sort(pts.begin(), pts.end(), [](const V2& a, const V2& b) {
        return a[0] < b[0] || (a[0] == b[0] && a[1] < b[1]);
    });
    pts.erase(std::unique(pts.begin(), pts.end()), pts.end());
    auto cross2 = [](const V2& o, const V2& a, const V2& b) {
        return (a[0]-o[0]) * (b[1]-o[1]) - (a[1]-o[1]) * (b[0]-o[0]);
    };
    std::vector<V2> hull;
    if (pts.size() >= 3) {
        hull.resize(2 * pts.size());
        size_t k = 0;
        for (size_t i = 0; i < pts.size(); ++i) {
            while (k >= 2 && cross2(hull[k-2], hull[k-1], pts[i]) <= 0) --k;
            hull[k++] = pts[i];
        }
        size_t lower = k + 1;
        for (size_t i = pts.size() - 1; i-- > 0; ) {
            while (k >= lower && cross2(hull[k-2], hull[k-1], pts[i]) <= 0) --k;
            hull[k++] = pts[i];
        }
        hull.resize(k > 1 ? k - 1 : k);
    } else hull = pts;

    double best_angle = 0.0, best_area = 1e300;
    const size_t nh = hull.size();
    auto bbox_at = [&](double ang, double& w, double& h) {
        double ca = std::cos(ang), sa = std::sin(ang);
        double x0 = 1e300, x1 = -1e300, y0 = 1e300, y1 = -1e300;
        for (const auto& p : hull) {
            double x = ca * p[0] + sa * p[1];
            double y = -sa * p[0] + ca * p[1];
            x0 = std::min(x0, x); x1 = std::max(x1, x);
            y0 = std::min(y0, y); y1 = std::max(y1, y);
        }
        w = x1 - x0; h = y1 - y0;
    };
    if (nh >= 2) {
        for (size_t i = 0; i < nh; ++i) {
            const V2& a = hull[i];
            const V2& b = hull[(i+1) % nh];
            double ang = std::atan2(b[1]-a[1], b[0]-a[0]);
            double w, h;
            bbox_at(-ang, w, h);
            if (w * h < best_area) { best_area = w * h; best_angle = -ang; }
        }
    }
    double ca = std::cos(best_angle), sa = std::sin(best_angle);
    double x0 = 1e300, y0 = 1e300, x1 = -1e300, y1 = -1e300;
    for (auto& p : c.uv) {
        double x = ca * p[0] + sa * p[1];
        double y = -sa * p[0] + ca * p[1];
        p = {x, y};
        x0 = std::min(x0, x); y0 = std::min(y0, y);
        x1 = std::max(x1, x); y1 = std::max(y1, y);
    }
    for (auto& p : c.uv) { p[0] -= x0; p[1] -= y0; }
    c.w = x1 - x0;
    c.h = y1 - y0;
    // prefer tall rects (skyline packs by height)
    if (c.w > c.h) {
        for (auto& p : c.uv) p = {c.h - p[1], p[0]};
        std::swap(c.w, c.h);
    }
}

// Skyline bottom-left packing of rects (w,h) + `pad` on every side into a
// strip of width `W`; returns used height. offsets receive the rect origin
// (content, i.e. pad included).
static double skyline_pack(std::vector<ChartParam>& charts,
                           const std::vector<int>& order, double W,
                           const std::vector<double>& pads,
                           std::vector<V2>& offsets) {
    struct Node { double x, w, y; };
    std::vector<Node> sky = {{0.0, W, 0.0}};
    double used_h = 0.0;
    offsets.resize(charts.size());
    for (int ci : order) {
        double pad = pads[ci];
        double rw = charts[ci].w + 2 * pad, rh = charts[ci].h + 2 * pad;
        rw = std::min(rw, W);
        // find placement minimizing (y, x)
        double best_y = 1e300, best_x = 0.0;
        size_t best_i = 0, best_span = 0;
        for (size_t i = 0; i < sky.size(); ++i) {
            double x = sky[i].x;
            if (x + rw > W + 1e-12) break;
            double y = 0.0, span_w = 0.0;
            size_t jj = i;
            while (jj < sky.size() && span_w < rw - 1e-12) {
                y = std::max(y, sky[jj].y);
                span_w += sky[jj].w;
                ++jj;
            }
            if (span_w < rw - 1e-12) continue;
            if (y < best_y - 1e-12 || (y < best_y + 1e-12 && x < best_x)) {
                best_y = y; best_x = x; best_i = i; best_span = jj - i;
            }
        }
        if (best_y > 1e299) {  // wider than any span (shouldn't happen): stack on top
            best_x = 0; best_y = used_h; best_i = 0; best_span = sky.size();
        }
        offsets[ci] = {best_x + pad, best_y + pad};
        used_h = std::max(used_h, best_y + rh);
        // update skyline: clip nodes against [best_x, span_end], insert the
        // new plateau, then merge equal-height neighbours
        double span_end = best_x + rw;
        std::vector<Node> nsky;
        nsky.reserve(sky.size() + 2);
        for (const Node& nd : sky) {
            double nd_end = nd.x + nd.w;
            if (nd_end <= best_x + 1e-12 || nd.x >= span_end - 1e-12) {
                nsky.push_back(nd);
                continue;
            }
            if (nd.x < best_x - 1e-12)       // left remainder
                nsky.push_back({nd.x, best_x - nd.x, nd.y});
            if (nd_end > span_end + 1e-12)   // right remainder
                nsky.push_back({span_end, nd_end - span_end, nd.y});
        }
        nsky.push_back({best_x, rw, best_y + rh});
        std::sort(nsky.begin(), nsky.end(),
                  [](const Node& a, const Node& b) { return a.x < b.x; });
        sky.clear();
        for (const Node& nd : nsky) {
            if (!sky.empty() && std::fabs(sky.back().y - nd.y) < 1e-12)
                sky.back().w = nd.x + nd.w - sky.back().x;
            else sky.push_back(nd);
        }
        (void)best_i; (void)best_span;
    }
    return used_h;
}

} // namespace


// ---------------------------------------------------------------------------
// build_uv_atlas
// ---------------------------------------------------------------------------
std::vector<int> build_uv_atlas(MeshData& mesh, UVAtlasConfig& cfg) {
    auto t0 = Clock::now();
    const size_t nf = mesh.F.size();
    const size_t nv = mesh.V.size();
    mesh.UV.clear();
    if (nf == 0) return {};

#ifdef _OPENMP
    if (cfg.num_threads > 0) omp_set_num_threads(cfg.num_threads);
#endif

    // per-chart texel budget (face count when no per-face weights given)
    const bool has_fw = cfg.face_weight.size() == nf;
    auto chart_weight = [&](const std::vector<int>& faces) -> double {
        if (!has_fw) return (double)faces.size();
        double s = 0.0;
        for (int t : faces) s += std::max(cfg.face_weight[t], 0.0f);
        return std::max(s, 1e-6);
    };

    // resolve auto texture size from the total texel budget: face weights are
    // absolute (projected pixel area in the best view), so their sum is the
    // resolution at which the atlas can hold everything the views captured
    if (cfg.texture_size <= 0) {
        double budget = 0.0;
        if (has_fw)
            for (float w : cfg.face_weight) budget += std::max(w, 0.0f);
        else
            budget = 4.0 * (double)nf;
        // ~60% effective packing; nearest power of two in [1024, 8192]
        // (log-space rounding: only double when the budget is past sqrt(2)x)
        double side = std::sqrt(std::max(budget, 1.0) / 0.6);
        int ts = 1024;
        while ((double)ts * 1.41421356 < side && ts < 8192) ts <<= 1;
        cfg.texture_size = ts;
        if (cfg.verbose)
            mlog::out(mlog::Stage::Uv, mmsg::uv_auto_size,
                      {cfg.texture_size, budget});
    }

    // face normals + areas
    std::vector<V3> faceN(nf);
    std::vector<float> faceA(nf);
    #pragma omp parallel for schedule(static)
    for (long t = 0; t < (long)nf; ++t) {
        const auto& f = mesh.F[t];
        V3 nn = cross(sub(mesh.V[f[1]], mesh.V[f[0]]), sub(mesh.V[f[2]], mesh.V[f[0]]));
        float l = norm3(nn);
        faceA[t] = 0.5f * l;
        faceN[t] = l > 1e-20f ? V3{nn[0]/l, nn[1]/l, nn[2]/l} : V3{0, 0, 1};
    }

    std::vector<std::array<int,3>> adj;
    build_face_adjacency(mesh.F, nv, adj);

    // ---- 1. charts ----
    ChartSeg seg = segment_charts(mesh.V, mesh.F, nv, adj, faceN, faceA, cfg);
    if (cfg.verbose)
        mlog::out(mlog::Stage::Uv, mmsg::uv_charts,
                  {(long long)seg.charts.size(), (long long)nf,
                   mlog::num(secs_since(t0), 2)});

    // ---- 2. parameterize (parallel, with split-and-retry) ----
    // A chart whose LSCM (and planar fallback) still has flipped/overlapping
    // UVs is split in half and re-parameterized -- overlapping UVs corrupt
    // the bake (one region's colors land on another), so they are not
    // tolerated. Halving terminates: single triangles always parameterize.
    auto t1 = Clock::now();
    std::vector<std::vector<int>> chart_faces = std::move(seg.charts);
    std::vector<ChartParam> params;
    std::vector<std::vector<int>> done_faces;
    long n_split_retry = 0, n_parked = 0;
    std::vector<int> pending(chart_faces.size());
    for (size_t i = 0; i < chart_faces.size(); ++i) pending[i] = (int)i;
    while (!pending.empty()) {
        const int np = (int)pending.size();
        std::vector<ChartParam> batch(np);
        std::vector<char> ok(np, 0);
        #pragma omp parallel for schedule(dynamic, 1)
        for (int i = 0; i < np; ++i)
            ok[i] = param_chart_lscm(chart_faces[pending[i]], mesh.V, mesh.F,
                                     chart_weight(chart_faces[pending[i]]),
                                     batch[i]) ? 1 : 0;
        std::vector<int> next;
        for (int i = 0; i < np; ++i) {
            int ci = pending[i];
            if (ok[i]) {
                params.push_back(std::move(batch[i]));
                done_faces.push_back(std::move(chart_faces[ci]));
                continue;
            }
            // moved out, not referenced: the emplace_backs below reallocate chart_faces
            std::vector<int> faces = std::move(chart_faces[ci]);
            if (faces.size() <= 1) {
                // terminally degenerate (zero-area triangle): park it on a
                // zero-size point so export stays valid
                ++n_parked;
                ChartParam p;
                std::unordered_map<int,int> l2g;
                for (int t : faces)
                    for (int k = 0; k < 3; ++k)
                        if (l2g.emplace(mesh.F[t][k], (int)p.verts.size()).second)
                            p.verts.push_back(mesh.F[t][k]);
                p.uv.assign(p.verts.size(), {0.0, 0.0});
                params.push_back(std::move(p));
                done_faces.push_back(std::move(faces));
                continue;
            }
            // split in half by BFS order (halves may be disconnected; a
            // disconnected half fails LSCM and is split again)
            ++n_split_retry;
            std::vector<char> in_set2(nf, 0);
            for (int t : faces) in_set2[t] = 1;
            std::vector<int> order2;
            order2.reserve(faces.size());
            std::vector<char> vis2(nf, 0);
            for (int t : faces) {
                if (vis2[t]) continue;
                size_t h0 = order2.size();
                order2.push_back(t);
                vis2[t] = 1;
                for (size_t h = h0; h < order2.size(); ++h)
                    for (int k = 0; k < 3; ++k) {
                        int nb = adj[order2[h]][k];
                        if (nb >= 0 && in_set2[nb] && !vis2[nb]) {
                            vis2[nb] = 1;
                            order2.push_back(nb);
                        }
                    }
            }
            size_t half = order2.size() / 2;
            int ia = (int)chart_faces.size();
            chart_faces.emplace_back(order2.begin(), order2.begin() + half);
            chart_faces.emplace_back(order2.begin() + half, order2.end());
            next.push_back(ia);
            next.push_back(ia + 1);
        }
        pending.swap(next);
    }
    chart_faces.clear();
    const size_t ncp = params.size();
    // rebuild face -> chart over the final chart list
    for (size_t c = 0; c < ncp; ++c)
        for (int t : done_faces[c]) seg.face_chart[t] = (int)c;
    #pragma omp parallel for schedule(dynamic, 16)
    for (long c = 0; c < (long)ncp; ++c) orient_chart(params[c]);
    if (cfg.verbose)
        mlog::out(mlog::Stage::Uv, mmsg::uv_param,
                  {(long long)ncp, n_split_retry, n_parked,
                   mlog::num(secs_since(t1), 2)});

    // ---- 3. pack ----
    auto t2 = Clock::now();
    double total_area = 0.0, max_dim = 0.0;
    for (const auto& p : params) {
        total_area += p.w * p.h;
        max_dim = std::max(max_dim, std::max(p.w, p.h));
    }
    if (total_area <= 0) total_area = 1e-12;
    // estimated atlas side in UV units at ~75% efficiency; pads are laid out
    // at that estimated texel size. Tiny charts get a 1px pad -- with tens of
    // thousands of few-texel charts, a full gutter on each would spend a
    // large share of the atlas on spacing.
    double side_est = std::max(std::sqrt(total_area / 0.75), max_dim);
    double texel_uv = side_est / (double)cfg.texture_size;
    std::vector<double> pads(ncp);
    double max_pad = 0.0;
    for (size_t i = 0; i < ncp; ++i) {
        bool tiny = std::min(params[i].w, params[i].h) < 4.0 * texel_uv;
        double px = tiny ? std::max(1.0, 0.25 * cfg.gutter_px)
                         : 0.5 * cfg.gutter_px;
        pads[i] = px * texel_uv;
        max_pad = std::max(max_pad, pads[i]);
    }

    std::vector<int> order(ncp);
    for (size_t i = 0; i < ncp; ++i) order[i] = (int)i;
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return params[a].h > params[b].h;
    });
    std::vector<V2> offsets;
    double W = side_est, H = skyline_pack(params, order, W, pads, offsets);
    if (H > 1.05 * W || H < 0.7 * W) {       // rebalance once toward square
        W = std::max(std::sqrt(W * H), max_dim + 2 * max_pad);
        H = skyline_pack(params, order, W, pads, offsets);
    }
    double side = std::max(W, H);
    double inv_side = 1.0 / side;
    // chart UV area == texel budget, so this is texels delivered per texel
    // asked for (the budget overcommits when it exceeds the atlas)
    double px_per_unit = (double)cfg.texture_size * inv_side;
    if (cfg.verbose)
        mlog::out(mlog::Stage::Uv, mmsg::uv_packed,
                  {(long long)ncp,
                   mlog::num(100.0 * total_area / (side * side), 0),
                   (double)(px_per_unit * px_per_unit),
                   mlog::num(secs_since(t2), 2)});

    // ---- 4. seam split + write back ----
    const bool has_n = mesh.N.size() == nv;
    const bool has_c = mesh.C.size() == nv;
    // (vertex, chart) -> new index
    std::unordered_map<int64_t, int> vc2new;
    vc2new.reserve(nv * 2);
    std::vector<std::array<float,3>> newV;
    std::vector<std::array<float,3>> newN;
    std::vector<std::array<unsigned char,3>> newC;
    std::vector<std::array<float,2>> newUV;
    newV.reserve(nv + nv / 4);
    newUV.reserve(nv + nv / 4);
    if (has_n) newN.reserve(nv + nv / 4);
    if (has_c) newC.reserve(nv + nv / 4);

    // per-chart local lookup (vertex -> local id) for UV retrieval
    std::vector<std::unordered_map<int,int>> chart_l2g(ncp);
    for (size_t c = 0; c < ncp; ++c) {
        auto& m = chart_l2g[c];
        m.reserve(params[c].verts.size() * 2);
        for (size_t l = 0; l < params[c].verts.size(); ++l)
            m.emplace(params[c].verts[l], (int)l);
    }

    std::vector<std::array<int,3>> newF(nf);
    for (size_t t = 0; t < nf; ++t) {
        int c = seg.face_chart[t];
        const auto& off = offsets[c];
        for (int k = 0; k < 3; ++k) {
            int g = mesh.F[t][k];
            int64_t key = (int64_t)g * (int64_t)(ncp + 1) + c;
            auto it = vc2new.find(key);
            int idx;
            if (it != vc2new.end()) idx = it->second;
            else {
                idx = (int)newV.size();
                vc2new.emplace(key, idx);
                newV.push_back(mesh.V[g]);
                if (has_n) newN.push_back(mesh.N[g]);
                if (has_c) newC.push_back(mesh.C[g]);
                int l = chart_l2g[c].at(g);
                double u = (params[c].uv[l][0] + off[0]) * inv_side;
                double v = (params[c].uv[l][1] + off[1]) * inv_side;
                newUV.push_back({(float)u, (float)v});
            }
            newF[t][k] = idx;
        }
    }
    size_t n_seam = newV.size() - nv;
    mesh.V.swap(newV);
    mesh.F.swap(newF);
    mesh.UV.swap(newUV);
    if (has_n) mesh.N.swap(newN); else mesh.N.clear();
    if (has_c) mesh.C.swap(newC); else mesh.C.clear();
    if (cfg.verbose)
        mlog::out(mlog::Stage::Uv, mmsg::uv_seam,
                  {(long long)n_seam, (long long)mesh.V.size(),
                   mlog::num(secs_since(t0), 2)});
    return seg.face_chart;
}


// ---------------------------------------------------------------------------
// bake_texture
// ---------------------------------------------------------------------------
void bake_texture(MeshData& mesh, const std::vector<int>& face_chart,
                  const UVAtlasConfig& cfg,
                  const std::function<void(const float*, int, float*)>& colorize) {
    auto t0 = Clock::now();
    const int W = cfg.texture_size, H = cfg.texture_size;
    mesh.tex_width = W;
    mesh.tex_height = H;
    mesh.texture.assign((size_t)W * H * 3, 0);
    if (mesh.F.empty() || mesh.UV.size() != mesh.V.size()) return;

#ifdef _OPENMP
    if (cfg.num_threads > 0) omp_set_num_threads(cfg.num_threads);
#endif

    // charts -> face lists (charts own disjoint texel regions; parallel-safe)
    int nc = 0;
    for (int c : face_chart) nc = std::max(nc, c + 1);
    std::vector<std::vector<int>> chart_faces(nc);
    for (size_t t = 0; t < mesh.F.size(); ++t)
        chart_faces[face_chart[t]].push_back((int)t);

    // ---- rasterize texel centers (with ~0.75px edge dilation) ----
    std::vector<int64_t> texel_of;         // covered texel indices
    std::vector<float>   pos_of;           // 3D position per covered texel
    {
        std::vector<char> covered((size_t)W * H, 0);
        int nthreads = 1;
#ifdef _OPENMP
        nthreads = omp_get_max_threads();
#endif
        std::vector<std::vector<int64_t>> loc_tex(nthreads);
        std::vector<std::vector<float>>   loc_pos(nthreads);

        #pragma omp parallel for schedule(dynamic, 4)
        for (long c = 0; c < (long)nc; ++c) {
#ifdef _OPENMP
            int tid = omp_get_thread_num();
#else
            int tid = 0;
#endif
            auto& Ltex = loc_tex[tid];
            auto& Lpos = loc_pos[tid];
            for (int t : chart_faces[c]) {
                const auto& f = mesh.F[t];
                // triangle in pixel space (texel (i,j) center = (j+0.5, i+0.5))
                double ux[3], uy[3];
                for (int k = 0; k < 3; ++k) {
                    ux[k] = (double)mesh.UV[f[k]][0] * W;
                    uy[k] = (double)mesh.UV[f[k]][1] * H;
                }
                double area2 = (ux[1]-ux[0]) * (uy[2]-uy[0])
                             - (uy[1]-uy[0]) * (ux[2]-ux[0]);
                double xs0 = std::min({ux[0], ux[1], ux[2]}) - 1.0;
                double xs1 = std::max({ux[0], ux[1], ux[2]}) + 1.0;
                double ys0 = std::min({uy[0], uy[1], uy[2]}) - 1.0;
                double ys1 = std::max({uy[0], uy[1], uy[2]}) + 1.0;
                int j0 = std::max(0, (int)std::floor(xs0));
                int j1 = std::min(W - 1, (int)std::ceil(xs1));
                int i0 = std::max(0, (int)std::floor(ys0));
                int i1 = std::min(H - 1, (int)std::ceil(ys1));
                const double kDilate = 0.75;   // pixels outside an edge still bake
                // edge lengths (pixel units) for the outside-distance test;
                // el[k] is the edge opposite corner k
                double el[3];
                for (int k = 0; k < 3; ++k) {
                    int a = (k + 1) % 3, b = (k + 2) % 3;
                    double dx = ux[b] - ux[a], dy = uy[b] - uy[a];
                    el[k] = std::max(std::sqrt(dx*dx + dy*dy), 1e-12);
                }
                for (int i = i0; i <= i1; ++i) {
                    double py = i + 0.5;
                    for (int j = j0; j <= j1; ++j) {
                        size_t tx = (size_t)i * W + j;
                        if (covered[tx]) continue;
                        double px = j + 0.5;
                        // signed edge functions; e[k] is the edge opposite corner k
                        double e0 = (ux[2]-ux[1]) * (py-uy[1]) - (uy[2]-uy[1]) * (px-ux[1]);
                        double e1 = (ux[0]-ux[2]) * (py-uy[2]) - (uy[0]-uy[2]) * (px-ux[2]);
                        double e2 = (ux[1]-ux[0]) * (py-uy[0]) - (uy[1]-uy[0]) * (px-ux[0]);
                        if (area2 < 0) { e0 = -e0; e1 = -e1; e2 = -e2; }
                        // pixel distance outside each edge
                        if (e0 < -kDilate * el[0]) continue;
                        if (e1 < -kDilate * el[1]) continue;
                        if (e2 < -kDilate * el[2]) continue;
                        // clamped barycentric -> 3D position
                        double s = e0 + e1 + e2;
                        double b0, b1, b2;
                        if (std::fabs(s) < 1e-300) { b0 = b1 = b2 = 1.0 / 3.0; }
                        else {
                            b0 = std::max(e0 / s, 0.0);
                            b1 = std::max(e1 / s, 0.0);
                            b2 = std::max(e2 / s, 0.0);
                            double bs = b0 + b1 + b2;
                            b0 /= bs; b1 /= bs; b2 /= bs;
                        }
                        covered[tx] = 1;
                        Ltex.push_back((int64_t)tx);
                        for (int a = 0; a < 3; ++a)
                            Lpos.push_back((float)(b0 * mesh.V[f[0]][a] +
                                                   b1 * mesh.V[f[1]][a] +
                                                   b2 * mesh.V[f[2]][a]));
                    }
                }
            }
        }
        size_t total = 0;
        for (auto& l : loc_tex) total += l.size();
        texel_of.reserve(total);
        pos_of.reserve(total * 3);
        for (int i = 0; i < nthreads; ++i) {
            texel_of.insert(texel_of.end(), loc_tex[i].begin(), loc_tex[i].end());
            pos_of.insert(pos_of.end(), loc_pos[i].begin(), loc_pos[i].end());
            loc_tex[i].clear(); loc_tex[i].shrink_to_fit();
            loc_pos[i].clear(); loc_pos[i].shrink_to_fit();
        }
    }
    const size_t n_cov = texel_of.size();
    if (cfg.verbose)
        mlog::out(mlog::Stage::Bake, mmsg::bake_covered,
                  {(long long)n_cov, W * H,
                   mlog::num(100.0 * n_cov / ((double)W * H), 1),
                   mlog::num(secs_since(t0), 2)});
    if (n_cov == 0) return;

    // ---- evaluate colors (chunked to bound VRAM; each chunk re-renders the
    //      cameras, so chunks are large) ----
    auto t1 = Clock::now();
    std::vector<float> rgb(n_cov * 3);
    const size_t kChunk = 16 << 20;
    for (size_t off = 0; off < n_cov; off += kChunk) {
        size_t n = std::min(kChunk, n_cov - off);
        colorize(pos_of.data() + off * 3, (int)n, rgb.data() + off * 3);
    }
    pos_of.clear();
    pos_of.shrink_to_fit();
    if (cfg.verbose)
        mlog::out(mlog::Stage::Bake, mmsg::bake_colorized,
                  {(long long)n_cov, mlog::num(secs_since(t1), 2)});

    // ---- write texels + average color ----
    std::vector<char> filled((size_t)W * H, 0);
    double avg[3] = {0, 0, 0};
    for (size_t i = 0; i < n_cov; ++i) {
        size_t tx = (size_t)texel_of[i];
        for (int a = 0; a < 3; ++a) {
            float v = std::min(std::max(rgb[3*i + a], 0.0f), 1.0f);
            mesh.texture[3*tx + a] = (unsigned char)std::lround(v * 255.0f);
            avg[a] += v;
        }
        filled[tx] = 1;
    }
    unsigned char avg8[3];
    for (int a = 0; a < 3; ++a)
        avg8[a] = (unsigned char)std::lround(
            std::min(std::max(avg[a] / (double)n_cov, 0.0), 1.0) * 255.0);
    rgb.clear(); rgb.shrink_to_fit();
    texel_of.clear(); texel_of.shrink_to_fit();

    // ---- gutter dilation: grow filled colors outward so bilinear / mip
    //      sampling near chart borders reads plausible values ----
    auto t2 = Clock::now();
    const int rounds = cfg.gutter_px + 2;
    std::vector<char> next(filled.size());
    for (int r = 0; r < rounds; ++r) {
        std::copy(filled.begin(), filled.end(), next.begin());
        #pragma omp parallel for schedule(dynamic, 64)
        for (long i = 0; i < (long)H; ++i) {
            for (int j = 0; j < W; ++j) {
                size_t tx = (size_t)i * W + j;
                if (filled[tx]) continue;
                int acc[3] = {0, 0, 0}, cnt = 0;
                for (int di = -1; di <= 1; ++di) {
                    int ii = (int)i + di;
                    if (ii < 0 || ii >= H) continue;
                    for (int dj = -1; dj <= 1; ++dj) {
                        int jj = j + dj;
                        if (jj < 0 || jj >= W) continue;
                        size_t sx = (size_t)ii * W + jj;
                        if (!filled[sx]) continue;
                        for (int a = 0; a < 3; ++a) acc[a] += mesh.texture[3*sx + a];
                        ++cnt;
                    }
                }
                if (cnt > 0) {
                    for (int a = 0; a < 3; ++a)
                        mesh.texture[3*tx + a] = (unsigned char)(acc[a] / cnt);
                    next[tx] = 1;
                }
            }
        }
        filled.swap(next);
    }
    // background: flat average color (keeps distant mips neutral)
    #pragma omp parallel for schedule(static)
    for (long tx = 0; tx < (long)((size_t)W * H); ++tx)
        if (!filled[tx])
            for (int a = 0; a < 3; ++a) mesh.texture[3*tx + a] = avg8[a];
    if (cfg.verbose)
        mlog::out(mlog::Stage::Bake, mmsg::bake_done,
                  {mlog::num(secs_since(t2), 2), W, H,
                   mlog::num(secs_since(t0), 2)});
}

} // namespace meshing

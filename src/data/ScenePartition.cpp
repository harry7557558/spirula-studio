// ScenePartition.cpp -- see ScenePartition.h and docs/notes/scene-partition.md.

#include "data/ScenePartition.h"

#include "data/CameraMath.h"
#include "data/Json.h"
#include "data/JsonWrite.h"
#include "data/Knn.h"
#include "external/stb_image.h"
#include "external/stb_image_write.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <unordered_map>

namespace fs = std::filesystem;

namespace spirula {

namespace {

std::string leaf_of(const std::string& path) { return fs::path(path).filename().string(); }

// Generic paths with the separators the two sides may disagree on unified.
std::string slashed(const std::string& path) {
    std::string s = path;
    for (char& c : s)
        if (c == '\\') c = '/';
    return s;
}

bool ends_with_name(const std::string& path, const std::string& name) {
    if (path.size() < name.size()) return false;
    if (path.compare(path.size() - name.size(), name.size(), name) != 0) return false;
    return path.size() == name.size() || path[path.size() - name.size() - 1] == '/';
}

// Frames by the tail of their path: a name from a model file is `a/b/c.jpg`
// relative to some folder, and the frame whose absolute path ends in it is
// the one -- a rig has the same leaf under several folders.
class FrameIndex {
public:
    explicit FrameIndex(const std::vector<std::string>& paths) : _paths(paths.size()) {
        for (size_t i = 0; i < paths.size(); i++) {
            _paths[i] = slashed(paths[i]);
            _by_leaf[leaf_of(_paths[i])].push_back((int32_t)i);
        }
    }
    int32_t find(const std::string& name) const {
        const std::string n = slashed(name);
        auto it = _by_leaf.find(leaf_of(n));
        if (it == _by_leaf.end()) return -1;
        for (int32_t i : it->second)
            if (ends_with_name(_paths[(size_t)i], n)) return i;
        return -1;
    }
    // The shortest tail of each path that names it alone among all of them.
    std::vector<std::string> unique_tails() const {
        std::vector<std::string> out(_paths.size());
        for (const auto& kv : _by_leaf) {
            for (int32_t i : kv.second) {
                const std::vector<std::string> parts = split(_paths[(size_t)i]);
                std::string tail;
                for (size_t k = parts.size(); k-- > 0;) {
                    tail = tail.empty() ? parts[k] : parts[k] + "/" + tail;
                    int hits = 0;
                    for (int32_t j : kv.second) hits += ends_with_name(_paths[(size_t)j], tail);
                    if (hits == 1) break;
                }
                out[(size_t)i] = tail;
            }
        }
        return out;
    }

private:
    static std::vector<std::string> split(const std::string& p) {
        std::vector<std::string> out;
        size_t at = 0;
        while (at <= p.size()) {
            const size_t next = p.find('/', at);
            const std::string piece = p.substr(at, next == std::string::npos ? std::string::npos : next - at);
            if (!piece.empty()) out.push_back(piece);
            if (next == std::string::npos) break;
            at = next + 1;
        }
        return out;
    }
    std::vector<std::string> _paths;
    std::unordered_map<std::string, std::vector<int32_t>> _by_leaf;
};

void say(const PartitionLog& log, const std::string& s) {
    if (log) log(s);
}

bool cancelled(const std::atomic<bool>* cancel) {
    return cancel && cancel->load(std::memory_order_relaxed);
}

void stop_if(const std::atomic<bool>* cancel) {
    if (cancelled(cancel)) throw PartitionCancelled();
}

template <typename T>
void keep_rows(std::vector<T>& v, int64_t n, int stride, const uint8_t* keep) {
    if (v.empty()) return;
    int64_t w = 0;
    for (int64_t i = 0; i < n; i++) {
        if (!keep[i]) continue;
        if (w != i)
            for (int k = 0; k < stride; k++) v[(size_t)(w * stride + k)] = std::move(v[(size_t)(i * stride + k)]);
        w++;
    }
    v.resize((size_t)(w * stride));
}

// Camera-to-world of frame i as CV world-to-camera: ray = R_cv (p - t).
struct FrameView {
    double R[9];   // rows: the CV camera axes in world
    double t[3];
    camhost::Camera cam;
    double cos_max = -2.0;   // cone pre-test for pinhole; -2 disables
};

FrameView frame_view(const ParsedDataset& ds, int64_t i) {
    FrameView f;
    const float* M = &ds.c2w[(size_t)i * 12];
    // OpenGL columns X, Y, Z (camera looks down -Z); CV is (x, -y, -z).
    for (int k = 0; k < 3; k++) {
        f.R[0 + k] = M[k * 4 + 0];
        f.R[3 + k] = -M[k * 4 + 1];
        f.R[6 + k] = -M[k * 4 + 2];
        f.t[k] = M[k * 4 + 3];
    }
    f.cam.model = ds.camera_models.empty() ? 0 : ds.camera_models[(size_t)i];
    f.cam.tier = ds.camera_distortions.empty() ? 0 : ds.camera_distortions[(size_t)i];
    f.cam.width = ds.widths[(size_t)i];
    f.cam.height = ds.heights[(size_t)i];
    f.cam.fx = ds.intrins[(size_t)i * 4 + 0];
    f.cam.fy = ds.intrins[(size_t)i * 4 + 1];
    f.cam.cx = ds.intrins[(size_t)i * 4 + 2];
    f.cam.cy = ds.intrins[(size_t)i * 4 + 3];
    if (!ds.dist_coeffs.empty())
        for (int k = 0; k < 8; k++) f.cam.dist[k] = ds.dist_coeffs[(size_t)i * 8 + k];
    if (f.cam.model == 0 && f.cam.fx > 0 && f.cam.fy > 0) {
        // The widest normalized coordinate any corner reaches, with slack for
        // lens distortion pulling the border in or out.
        double r2 = 0;
        for (int c = 0; c < 4; c++) {
            const double u = ((c & 1 ? f.cam.width : 0) - f.cam.cx) / f.cam.fx;
            const double v = ((c & 2 ? f.cam.height : 0) - f.cam.cy) / f.cam.fy;
            r2 = std::max(r2, u * u + v * v);
        }
        const double r = std::sqrt(r2) * 1.15;
        f.cos_max = 1.0 / std::sqrt(1.0 + r * r);
    }
    return f;
}

bool frame_sees(const FrameView& f, const double p[3]) {
    const double d[3] = {p[0] - f.t[0], p[1] - f.t[1], p[2] - f.t[2]};
    double ray[3];
    for (int r = 0; r < 3; r++)
        ray[r] = f.R[r * 3] * d[0] + f.R[r * 3 + 1] * d[1] + f.R[r * 3 + 2] * d[2];
    const double len = std::sqrt(ray[0] * ray[0] + ray[1] * ray[1] + ray[2] * ray[2]);
    if (!(len > 1e-12)) return false;
    if (f.cos_max > -1.5 && ray[2] / len < f.cos_max) return false;
    double px[2];
    return camhost::ray_in_frame(f.cam, ray, px);
}

// Pair weights from who sees what: every pair of frames sharing a point gets
// one. Long tracks are strided so one point seen everywhere does not cost a
// quadratic number of pairs.
graph::WeightedGraph camera_graph_of(const std::vector<int64_t>& beg,
                                     const std::vector<int32_t>& frame, size_t n_frames,
                                     const std::atomic<bool>* cancel = nullptr) {
    graph::EdgeAccumulator acc;
    acc.reserve(frame.size() * 4);
    constexpr int64_t kMaxTrack = 48;
    for (size_t i = 0; i + 1 < beg.size(); i++) {
        if (i % 65536 == 0) stop_if(cancel);
        const int64_t lo = beg[i], hi = beg[i + 1];
        const int64_t m = hi - lo;
        if (m < 2) continue;
        const int64_t step = m > kMaxTrack ? (m + kMaxTrack - 1) / kMaxTrack : 1;
        for (int64_t a = lo; a < hi; a += step)
            for (int64_t b = a + step; b < hi; b += step)
                acc.add((uint32_t)frame[(size_t)a], (uint32_t)frame[(size_t)b], 1.0);
    }
    return acc.build(n_frames);
}

int find_root(std::vector<int32_t>& up, int32_t i) {
    while (up[(size_t)i] != i) {
        up[(size_t)i] = up[(size_t)up[(size_t)i]];
        i = up[(size_t)i];
    }
    return i;
}

// Owner per point of `pick`: the nearest camera's part, diffused over a
// covisibility-weighted kNN graph, then islands dissolved. A plain vote per
// point interleaved parts wherever both saw a surface (docs/notes/scene-partition.md).
std::vector<int32_t> own_points(const ParsedDataset& ds, const Covisibility& cov,
                                const std::vector<int32_t>& frame_label,
                                const std::vector<float>& centers, int parts,
                                const std::vector<int64_t>& pick, bool tracked,
                                const std::atomic<bool>* cancel) {
    const int64_t n = (int64_t)pick.size();
    const int K = std::max(1, parts);
    std::vector<int32_t> out((size_t)n, 0);
    if (n == 0 || K == 1) return out;
    std::vector<float> xyz((size_t)n * 3);
    for (int64_t j = 0; j < n; j++)
        for (int r = 0; r < 3; r++) xyz[(size_t)j * 3 + r] = (float)ds.points.xyz[(size_t)pick[(size_t)j] * 3 + r];

    // Start from the nearest camera's part. Its nearest *observer* or the
    // summed closeness of its observers both merged worse (tracks are far
    // sparser than what each camera actually sees; the doc has the numbers).
    std::vector<float> s((size_t)n * K, 0.0f), t((size_t)n * K, 0.0f);
    {
        const knn::KdTree3 cams(centers.data(), (int64_t)centers.size() / 3);
#pragma omp parallel for schedule(static)
        for (int64_t j = 0; j < n; j++) {
            float d2;
            int32_t c = -1;
            if (cams.query(&xyz[(size_t)j * 3], -1, 1, &d2, &c) == 1)
                s[(size_t)j * K + frame_label[(size_t)c]] = 1.0f;
        }
    }
    std::vector<int32_t> frames;
    std::vector<int64_t> beg((size_t)n + 1, 0);
    if (tracked)
        for (int64_t j = 0; j < n; j++) {
            const int64_t i = pick[(size_t)j];
            frames.insert(frames.end(), cov.frame.begin() + cov.beg[(size_t)i],
                          cov.frame.begin() + cov.beg[(size_t)i + 1]);
            beg[(size_t)j + 1] = (int64_t)frames.size();
            std::sort(frames.begin() + beg[(size_t)j], frames.end());
        }
    // Two points link as strongly as the share of one's observers that are,
    // or are covisible with, the other's: tracks are too short to share frames.
    const graph::WeightedGraph& g = cov.cameras;
    std::vector<uint32_t> cam_adj = g.adj;
    for (size_t c = 0; c < g.n(); c++)
        std::sort(cam_adj.begin() + g.offs[c], cam_adj.begin() + g.offs[c + 1]);

    constexpr int kNb = 12;
    std::vector<int32_t> nb((size_t)n * kNb, -1);
    std::vector<float> w((size_t)n * kNb, 0.0f);
    {
        const knn::KdTree3 tree(xyz.data(), n);
#pragma omp parallel for schedule(dynamic, 1024)
        for (int64_t j = 0; j < n; j++) {
            if (cancelled(cancel)) continue;
            float d2[kNb];
            int32_t idx[kNb];
            const int got = tree.query(&xyz[(size_t)j * 3], (int32_t)j, kNb, d2, idx);
            for (int e = 0; e < got; e++) {
                const int32_t o = idx[e];
                float a = 1.0f;
                if (tracked) {
                    const int32_t* oi = &frames[(size_t)beg[(size_t)j]];
                    const int32_t* oi_end = &frames[0] + beg[(size_t)j + 1];
                    int64_t hits = 0;
                    for (int64_t y = beg[(size_t)o]; y < beg[(size_t)o + 1]; y++) {
                        const uint32_t b = (uint32_t)frames[(size_t)y];
                        bool linked = std::binary_search(oi, oi_end, (int32_t)b);
                        for (const int32_t* x = oi; x < oi_end && !linked; x++)
                            linked = std::binary_search(cam_adj.begin() + g.offs[(size_t)*x],
                                                        cam_adj.begin() + g.offs[(size_t)*x + 1], b);
                        hits += linked;
                    }
                    const int64_t m = beg[(size_t)o + 1] - beg[(size_t)o];
                    a = m > 0 ? (float)hits / (float)m : 0.0f;
                }
                nb[(size_t)j * kNb + e] = o;
                w[(size_t)j * kNb + e] = a;
            }
        }
    }

    for (int it = 0; it < 40; it++) {
        stop_if(cancel);
#pragma omp parallel for schedule(static)
        for (int64_t j = 0; j < n; j++) {
            float* dst = &t[(size_t)j * K];
            const float* self = &s[(size_t)j * K];
            float total = 1.0f;
            for (int k = 0; k < K; k++) dst[k] = self[k];
            for (int e = 0; e < kNb; e++) {
                const int32_t o = nb[(size_t)j * kNb + e];
                const float a = w[(size_t)j * kNb + e];
                if (o < 0 || a <= 0) continue;
                const float* src = &s[(size_t)o * K];
                for (int k = 0; k < K; k++) dst[k] += a * src[k];
                total += a;
            }
            for (int k = 0; k < K; k++) dst[k] /= total;
        }
        s.swap(t);
    }
    for (int64_t j = 0; j < n; j++)
        out[(size_t)j] = (int32_t)(std::max_element(&s[(size_t)j * K], &s[(size_t)j * K] + K) -
                                   &s[(size_t)j * K]);

    // Islands: every piece of a part smaller than a quarter of its largest
    // joins the larger piece it borders most, until nothing moves.
    for (int pass = 0; pass < 8; pass++) {
        stop_if(cancel);
        std::vector<int32_t> up((size_t)n);
        std::iota(up.begin(), up.end(), 0);
        for (int64_t j = 0; j < n; j++)
            for (int e = 0; e < kNb; e++) {
                const int32_t o = nb[(size_t)j * kNb + e];
                if (o < 0 || w[(size_t)j * kNb + e] <= 0 || out[(size_t)o] != out[(size_t)j]) continue;
                const int a = find_root(up, (int32_t)j), b = find_root(up, o);
                if (a != b) up[(size_t)std::max(a, b)] = std::min(a, b);
            }
        for (int64_t j = 0; j < n; j++) up[(size_t)j] = find_root(up, (int32_t)j);
        std::vector<int64_t> size((size_t)n, 0), largest((size_t)K, 0);
        for (int64_t j = 0; j < n; j++) size[(size_t)up[(size_t)j]]++;
        for (int64_t j = 0; j < n; j++)
            if (up[(size_t)j] == j) largest[(size_t)out[(size_t)j]] = std::max(largest[(size_t)out[(size_t)j]], size[(size_t)j]);
        std::vector<std::vector<float>> border;
        std::unordered_map<int32_t, size_t> slot;
        for (int64_t j = 0; j < n; j++) {
            const int32_t r = up[(size_t)j];
            if (4 * size[(size_t)r] >= largest[(size_t)out[(size_t)j]]) continue;
            for (int e = 0; e < kNb; e++) {
                const int32_t o = nb[(size_t)j * kNb + e];
                if (o < 0 || w[(size_t)j * kNb + e] <= 0 || out[(size_t)o] == out[(size_t)j] ||
                    size[(size_t)up[(size_t)o]] <= size[(size_t)r])
                    continue;
                auto [at, fresh] = slot.emplace(r, border.size());
                if (fresh) border.emplace_back((size_t)K, 0.0f);
                border[at->second][(size_t)out[(size_t)o]] += w[(size_t)j * kNb + e];
            }
        }
        if (slot.empty()) break;
        std::unordered_map<int32_t, int32_t> to;
        for (const auto& [r, at] : slot)
            to[r] = (int32_t)(std::max_element(border[at].begin(), border[at].end()) - border[at].begin());
        for (int64_t j = 0; j < n; j++) {
            auto it = to.find(up[(size_t)j]);
            if (it != to.end()) out[(size_t)j] = it->second;
        }
    }
    return out;
}

// The camera-point visibility graph over `pick`, cameras as rows. An
// observation weighs the image area the point stands for, 1/d^2, and each
// camera's observations sum to 1, so a label's weight is its share of the view.
struct Visibility {
    std::vector<int64_t> pbeg, cbeg;   // per point / per camera, into the lists
    std::vector<int32_t> pcam, cpt;    // observers of a point / points of a camera
    std::vector<float> pw, cw;         // the observation's weight, both orders
    std::vector<int32_t> nb;           // [n, kNb] point neighbours, -1 none
    std::vector<float> nw;             // their covisibility affinity
    static constexpr int kNb = 12;
};

Visibility visibility_of(const ParsedDataset& ds, const Covisibility& cov,
                         const std::vector<int64_t>& pick, const std::vector<float>& centers,
                         int64_t n_cam, const std::atomic<bool>* cancel) {
    Visibility v;
    const int64_t n = (int64_t)pick.size();
    v.pbeg.assign((size_t)n + 1, 0);
    for (int64_t j = 0; j < n; j++) {
        const int64_t i = pick[(size_t)j];
        v.pcam.insert(v.pcam.end(), cov.frame.begin() + cov.beg[(size_t)i],
                      cov.frame.begin() + cov.beg[(size_t)i + 1]);
        v.pbeg[(size_t)j + 1] = (int64_t)v.pcam.size();
        std::sort(v.pcam.begin() + v.pbeg[(size_t)j], v.pcam.end());
    }
    v.cbeg.assign((size_t)n_cam + 1, 0);
    for (int32_t c : v.pcam) v.cbeg[(size_t)c + 1]++;
    for (int64_t c = 0; c < n_cam; c++) v.cbeg[(size_t)c + 1] += v.cbeg[(size_t)c];
    v.cpt.resize(v.pcam.size());
    v.cw.resize(v.pcam.size());
    v.pw.resize(v.pcam.size());
    std::vector<int64_t> at(v.pbeg.size());
    {
        std::vector<int64_t> fill(v.cbeg.begin(), v.cbeg.end() - 1);
        for (int64_t j = 0; j < n; j++)
            for (int64_t k = v.pbeg[(size_t)j]; k < v.pbeg[(size_t)j + 1]; k++) {
                const int64_t slot = fill[(size_t)v.pcam[(size_t)k]]++;
                v.cpt[(size_t)slot] = (int32_t)j;
                const double* q = &ds.points.xyz[(size_t)pick[(size_t)j] * 3];
                const float* c = &centers[(size_t)v.pcam[(size_t)k] * 3];
                double d2 = 0;
                for (int r = 0; r < 3; r++) d2 += (q[r] - c[r]) * (q[r] - c[r]);
                v.cw[(size_t)slot] = (float)(1.0 / std::max(d2, 1e-12));
            }
    }
    // A camera's nearest observations are clamped to its 5th percentile
    // distance, so one point at its nose does not stand for the whole view.
#pragma omp parallel for schedule(dynamic, 16)
    for (int64_t c = 0; c < n_cam; c++) {
        float* w = &v.cw[(size_t)v.cbeg[(size_t)c]];
        const int64_t m = v.cbeg[(size_t)c + 1] - v.cbeg[(size_t)c];
        if (m == 0) continue;
        std::vector<float> sorted(w, w + m);
        std::nth_element(sorted.begin(), sorted.begin() + (m * 95) / 100, sorted.end());
        const float cap = sorted[(size_t)((m * 95) / 100)];
        double total = 0;
        for (int64_t k = 0; k < m; k++) total += (w[k] = std::min(w[k], cap));
        for (int64_t k = 0; k < m; k++) w[k] = (float)(w[k] / total);
    }
    {
        std::vector<int64_t> fill(v.cbeg.begin(), v.cbeg.end() - 1);
        for (int64_t j = 0; j < n; j++)
            for (int64_t k = v.pbeg[(size_t)j]; k < v.pbeg[(size_t)j + 1]; k++)
                v.pw[(size_t)k] = v.cw[(size_t)fill[(size_t)v.pcam[(size_t)k]]++];
    }
    // Two points link as strongly as the share of one's observers that are,
    // or are covisible with, the other's: tracks are too short to share frames.
    const graph::WeightedGraph& g = cov.cameras;
    std::vector<uint32_t> cam_adj = g.adj;
    for (size_t c = 0; c < g.n(); c++)
        std::sort(cam_adj.begin() + g.offs[c], cam_adj.begin() + g.offs[c + 1]);
    std::vector<float> xyz((size_t)n * 3);
    for (int64_t j = 0; j < n; j++)
        for (int r = 0; r < 3; r++) xyz[(size_t)j * 3 + r] = (float)ds.points.xyz[(size_t)pick[(size_t)j] * 3 + r];
    constexpr int kNb = Visibility::kNb;
    v.nb.assign((size_t)n * kNb, -1);
    v.nw.assign((size_t)n * kNb, 0.0f);
    stop_if(cancel);
    const knn::KdTree3 tree(xyz.data(), n);
#pragma omp parallel for schedule(dynamic, 1024)
    for (int64_t j = 0; j < n; j++) {
        if (cancelled(cancel)) continue;
        float d2[kNb];
        int32_t idx[kNb];
        const int got = tree.query(&xyz[(size_t)j * 3], (int32_t)j, kNb, d2, idx);
        const int32_t* oi = v.pcam.data() + v.pbeg[(size_t)j];
        const int32_t* oi_end = v.pcam.data() + v.pbeg[(size_t)j + 1];
        for (int e = 0; e < got; e++) {
            const int32_t o = idx[e];
            int64_t hits = 0;
            for (int64_t y = v.pbeg[(size_t)o]; y < v.pbeg[(size_t)o + 1]; y++) {
                const uint32_t b = (uint32_t)v.pcam[(size_t)y];
                bool linked = std::binary_search(oi, oi_end, (int32_t)b);
                for (const int32_t* x = oi; x < oi_end && !linked; x++)
                    linked = std::binary_search(cam_adj.begin() + g.offs[(size_t)*x],
                                                cam_adj.begin() + g.offs[(size_t)*x + 1], b);
                hits += linked;
            }
            const int64_t m = v.pbeg[(size_t)o + 1] - v.pbeg[(size_t)o];
            v.nb[(size_t)j * kNb + e] = o;
            v.nw[(size_t)j * kNb + e] = m > 0 ? (float)hits / (float)m : 0.0f;
        }
    }
    stop_if(cancel);
    return v;
}

// Points grouped into patches of up to kPatch linked neighbours (same
// surface, by covisibility), so the joint graph stays a few tens of
// thousands of nodes; a patch never crosses a wall.
constexpr int kPatch = 24;
std::vector<int32_t> patch_points(const Visibility& v, int32_t& n_patch) {
    constexpr int kNb = Visibility::kNb;
    const int64_t n = (int64_t)v.pbeg.size() - 1;
    std::vector<int32_t> patch((size_t)n, -1);
    std::vector<int32_t> queue;
    n_patch = 0;
    for (int64_t seed = 0; seed < n; seed++) {
        if (patch[(size_t)seed] >= 0) continue;
        queue.assign(1, (int32_t)seed);
        patch[(size_t)seed] = n_patch;
        int size = 1;
        for (size_t q = 0; q < queue.size() && size < kPatch; q++) {
            const int32_t j = queue[q];
            for (int e = 0; e < kNb && size < kPatch; e++) {
                const int32_t o = v.nb[(size_t)j * kNb + e];
                if (o < 0 || patch[(size_t)o] >= 0 || v.nw[(size_t)j * kNb + e] < 0.5f) continue;
                patch[(size_t)o] = n_patch;
                queue.push_back(o);
                size++;
            }
        }
        n_patch++;
    }
    return patch;
}

// One graph over cameras (nodes 0..n_cam-1) and patches: a camera-patch edge
// is the view weight on the patch, a patch-patch edge `lambda` times the
// affinity between them; a cut's weight is the energy of scene-partition.md.
graph::WeightedGraph joint_graph(const Visibility& v, const std::vector<int32_t>& patch, int32_t n_patch,
                                 int64_t n_cam, double lambda) {
    constexpr int kNb = Visibility::kNb;
    const int64_t n = (int64_t)v.pbeg.size() - 1;
    graph::EdgeAccumulator acc;
    acc.reserve((size_t)v.pcam.size() + (size_t)n * 4);
    for (int64_t j = 0; j < n; j++) {
        const uint32_t pn = (uint32_t)(n_cam + patch[(size_t)j]);
        for (int64_t k = v.pbeg[(size_t)j]; k < v.pbeg[(size_t)j + 1]; k++)
            acc.add((uint32_t)v.pcam[(size_t)k], pn, v.pw[(size_t)k]);
        for (int e = 0; e < kNb; e++) {
            const int32_t o = v.nb[(size_t)j * kNb + e];
            if (o < 0 || patch[(size_t)o] == patch[(size_t)j] || v.nw[(size_t)j * kNb + e] <= 0) continue;
            acc.add(pn, (uint32_t)(n_cam + patch[(size_t)o]), lambda * v.nw[(size_t)j * kNb + e]);
        }
    }
    return acc.build((size_t)(n_cam + n_patch));
}

// Per camera, its view weight on each label and its observations there.
void view_shares(const Visibility& v, const std::vector<int32_t>& own, int K, std::vector<double>& share,
                 std::vector<int64_t>& hits) {
    const int64_t n_cam = (int64_t)v.cbeg.size() - 1;
    share.assign((size_t)n_cam * K, 0.0);
    hits.assign((size_t)n_cam * K, 0);
#pragma omp parallel for schedule(dynamic, 16)
    for (int64_t c = 0; c < n_cam; c++)
        for (int64_t k = v.cbeg[(size_t)c]; k < v.cbeg[(size_t)c + 1]; k++) {
            const int32_t l = own[(size_t)v.cpt[(size_t)k]];
            if (l < 0) continue;
            share[(size_t)c * K + l] += v.cw[(size_t)k];
            hits[(size_t)c * K + l]++;
        }
}

int absorb_tiny_parts(std::vector<int32_t>& home, std::vector<int32_t>& own,
                      const std::vector<float>& centers, int64_t min_size);

// Dense labels, largest camera count first, applied to both tables.
int renumber_parts(std::vector<int32_t>& home, std::vector<int32_t>& own) {
    int K = 0;
    for (int32_t h : home) K = std::max(K, h + 1);
    for (int32_t l : own) K = std::max(K, l + 1);
    std::vector<int64_t> count((size_t)std::max(K, 1), 0);
    for (int32_t h : home)
        if (h >= 0) count[(size_t)h]++;
    std::vector<int> order((size_t)K);
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return count[(size_t)a] > count[(size_t)b]; });
    std::vector<int32_t> remap((size_t)std::max(K, 1), -1);
    int next = 0;
    for (int k : order)
        if (count[(size_t)k] > 0) remap[(size_t)k] = next++;
    for (int32_t& h : home) h = h >= 0 ? remap[(size_t)h] : -1;
    for (int32_t& l : own) l = l >= 0 ? remap[(size_t)l] : -1;
    return next;
}

// Cameras without observations take the label of the nearest one with.
void fill_unseen(std::vector<int32_t>& home, const std::vector<float>& centers) {
    const int64_t n_cam = (int64_t)home.size();
    std::vector<float> seen_xyz;
    std::vector<int32_t> seen_idx;
    for (int64_t c = 0; c < n_cam; c++)
        if (home[(size_t)c] >= 0) {
            seen_xyz.insert(seen_xyz.end(), &centers[(size_t)c * 3], &centers[(size_t)c * 3] + 3);
            seen_idx.push_back((int32_t)c);
        }
    if (seen_idx.empty()) {
        std::fill(home.begin(), home.end(), 0);
        return;
    }
    const knn::KdTree3 tree(seen_xyz.data(), (int64_t)seen_idx.size());
    for (int64_t c = 0; c < n_cam; c++) {
        if (home[(size_t)c] >= 0) continue;
        float d2;
        int32_t at = -1;
        if (tree.query(&centers[(size_t)c * 3], -1, 1, &d2, &at) == 1)
            home[(size_t)c] = home[(size_t)seen_idx[(size_t)at]];
    }
}

// Parts below `min_size` cameras dissolve: their cameras go to the nearest
// camera of a larger one, their points are unlabelled. Labels renumbered.
int absorb_tiny_parts(std::vector<int32_t>& home, std::vector<int32_t>& own,
                      const std::vector<float>& centers, int64_t min_size) {
    int K = 0;
    for (int32_t h : home) K = std::max(K, h + 1);
    std::vector<int64_t> size((size_t)std::max(K, 1), 0);
    for (int32_t h : home)
        if (h >= 0) size[(size_t)h]++;
    for (int32_t& h : home)
        if (h >= 0 && size[(size_t)h] < min_size) h = -1;
    for (int32_t& l : own)
        if (l >= 0 && (l >= K || size[(size_t)l] < min_size)) l = -1;
    fill_unseen(home, centers);
    return renumber_parts(home, own);
}

}  // namespace

// ===========================================================================
// Names
// ===========================================================================

const char* covisibility_source_name(CovisibilitySource s) {
    switch (s) {
        case CovisibilitySource::Auto: return "auto";
        case CovisibilitySource::Tracks: return "tracks";
        case CovisibilitySource::Projection: return "projection";
        case CovisibilitySource::Proximity: return "proximity";
    }
    return "auto";
}

const char* partition_method_name(PartitionMethod m) {
    return m == PartitionMethod::ViewGraph ? "viewgraph" : "graph";
}

bool partition_method_from_name(const std::string& s, PartitionMethod& out) {
    for (PartitionMethod m : {PartitionMethod::Graph, PartitionMethod::ViewGraph})
        if (s == partition_method_name(m)) { out = m; return true; }
    return false;
}

bool covisibility_source_from_name(const std::string& s, CovisibilitySource& out) {
    for (CovisibilitySource c : {CovisibilitySource::Auto, CovisibilitySource::Tracks,
                                 CovisibilitySource::Projection, CovisibilitySource::Proximity})
        if (s == covisibility_source_name(c)) { out = c; return true; }
    return false;
}

// ===========================================================================
// Covisibility
// ===========================================================================

namespace {

bool covisibility_from_tracks(const ParsedDataset& ds, const SparseStats& st, Covisibility& out,
                              const PartitionLog& log, const std::atomic<bool>* cancel) {
    if (st.empty() || (int64_t)st.track_beg.size() - 1 != ds.points.num()) return false;
    const FrameIndex index(ds.image_filenames);
    std::vector<int32_t> image_frame(st.image_names.size(), -1);
    size_t matched = 0;
    for (size_t i = 0; i < st.image_names.size(); i++) {
        image_frame[i] = index.find(st.image_names[i]);
        matched += image_frame[i] >= 0;
    }
    if (matched == 0) return false;
    out.beg.assign(1, 0);
    out.frame.clear();
    out.frame.reserve(st.track_image.size());
    for (size_t i = 0; i + 1 < st.track_beg.size(); i++) {
        if (i % 65536 == 0) stop_if(cancel);
        for (int64_t k = st.track_beg[i]; k < st.track_beg[i + 1]; k++) {
            const int32_t f = image_frame[(size_t)st.track_image[(size_t)k]];
            if (f >= 0) out.frame.push_back(f);
        }
        out.beg.push_back((int64_t)out.frame.size());
    }
    out.cameras = camera_graph_of(out.beg, out.frame, (size_t)ds.num_cameras, cancel);
    out.source = CovisibilitySource::Tracks;
    (void)log;
    return true;
}

bool covisibility_from_projection(const ParsedDataset& ds, const PartitionOptions& opt,
                                  Covisibility& out, const std::atomic<bool>* cancel) {
    const int64_t n_pts = ds.points.num();
    const int64_t n_cam = ds.num_cameras;
    if (n_pts == 0 || n_cam == 0) return false;
    const int64_t stride = std::max<int64_t>(1, n_pts / std::max(1, opt.max_projected_points));
    std::vector<int64_t> sample;
    for (int64_t i = 0; i < n_pts; i += stride) sample.push_back(i);
    std::vector<FrameView> views((size_t)n_cam);
    for (int64_t i = 0; i < n_cam; i++) views[(size_t)i] = frame_view(ds, i);

    // Per frame, which sampled points it sees; then inverted to per point.
    std::vector<std::vector<int32_t>> seen((size_t)n_cam);
#pragma omp parallel for schedule(dynamic, 4)
    for (int64_t c = 0; c < n_cam; c++) {
        if (cancelled(cancel)) continue;
        std::vector<int32_t>& s = seen[(size_t)c];
        for (size_t k = 0; k < sample.size(); k++)
            if (frame_sees(views[(size_t)c], &ds.points.xyz[(size_t)sample[k] * 3]))
                s.push_back((int32_t)k);
    }
    stop_if(cancel);
    std::vector<int64_t> count(sample.size() + 1, 0);
    for (int64_t c = 0; c < n_cam; c++)
        for (int32_t k : seen[(size_t)c]) count[(size_t)k + 1]++;
    for (size_t i = 1; i < count.size(); i++) count[i] += count[i - 1];
    std::vector<int32_t> frame((size_t)count.back());
    {
        std::vector<int64_t> fill(count.begin(), count.end() - 1);
        for (int64_t c = 0; c < n_cam; c++)
            for (int32_t k : seen[(size_t)c]) frame[(size_t)fill[(size_t)k]++] = (int32_t)c;
    }
    // Spread back over every point: the unsampled ones get their sample's
    // observers, so the tables stay one row per seed point.
    out.beg.assign(1, 0);
    out.frame.clear();
    out.frame.reserve(frame.size() * (size_t)stride);
    for (int64_t i = 0; i < n_pts; i++) {
        const size_t k = (size_t)(i / stride);
        for (int64_t j = count[k]; j < count[k + 1]; j++) out.frame.push_back(frame[(size_t)j]);
        out.beg.push_back((int64_t)out.frame.size());
    }
    // The graph from the sample alone; the copies would only scale it.
    out.cameras = camera_graph_of(count, frame, (size_t)n_cam, cancel);
    out.source = CovisibilitySource::Projection;
    return out.cameras.adj.size() > 0;
}

void covisibility_from_proximity(const ParsedDataset& ds, const PartitionOptions& opt,
                                 Covisibility& out, const std::atomic<bool>* cancel) {
    const int64_t n = ds.num_cameras;
    const int k = std::max(1, opt.proximity_neighbours);
    std::vector<double> c((size_t)n * 3), dir((size_t)n * 3);
    for (int64_t i = 0; i < n; i++) {
        const float* M = &ds.c2w[(size_t)i * 12];
        for (int r = 0; r < 3; r++) {
            c[(size_t)i * 3 + r] = M[r * 4 + 3];
            dir[(size_t)i * 3 + r] = -M[r * 4 + 2];
        }
    }
    std::vector<std::vector<std::pair<double, int32_t>>> nearest((size_t)n);
#pragma omp parallel for schedule(dynamic, 16)
    for (int64_t i = 0; i < n; i++) {
        if (cancelled(cancel)) continue;
        std::vector<std::pair<double, int32_t>>& best = nearest[(size_t)i];
        for (int64_t j = 0; j < n; j++) {
            if (j == i) continue;
            double d2 = 0;
            for (int r = 0; r < 3; r++) {
                const double d = c[(size_t)i * 3 + r] - c[(size_t)j * 3 + r];
                d2 += d * d;
            }
            if ((int)best.size() < k) {
                best.emplace_back(d2, (int32_t)j);
                std::push_heap(best.begin(), best.end());
            } else if (d2 < best.front().first) {
                std::pop_heap(best.begin(), best.end());
                best.back() = {d2, (int32_t)j};
                std::push_heap(best.begin(), best.end());
            }
        }
    }
    stop_if(cancel);
    graph::EdgeAccumulator acc;
    for (int64_t i = 0; i < n; i++)
        for (const auto& [d2, j] : nearest[(size_t)i]) {
            double cosang = 0;
            for (int r = 0; r < 3; r++) cosang += dir[(size_t)i * 3 + r] * dir[(size_t)j * 3 + r];
            // Facing the same way counts for more than standing side by side.
            acc.add((uint32_t)i, (uint32_t)j, 1.0 + std::max(0.0, cosang));
        }
    out.cameras = acc.build((size_t)n);
    out.beg.assign((size_t)ds.points.num() + 1, 0);
    out.frame.clear();
    out.source = CovisibilitySource::Proximity;
}

}  // namespace

bool tracks_usable(const ParsedDataset& ds, const SparseStats& tracks) {
    return !tracks.empty() && (int64_t)tracks.track_beg.size() - 1 == ds.points.num();
}

Covisibility build_covisibility(const ParsedDataset& ds, const SparseStats* tracks,
                                const PartitionOptions& opt, const PartitionLog& log,
                                const std::atomic<bool>* cancel) {
    Covisibility out;
    if (ds.num_cameras == 0) return out;
    const CovisibilitySource want = opt.source;
    if ((want == CovisibilitySource::Auto || want == CovisibilitySource::Tracks) && tracks &&
        covisibility_from_tracks(ds, *tracks, out, log, cancel))
        return out;
    if (want == CovisibilitySource::Tracks)
        say(log, "no usable tracks; falling back to projection");
    stop_if(cancel);
    if ((want != CovisibilitySource::Proximity) && covisibility_from_projection(ds, opt, out, cancel))
        return out;
    if (want == CovisibilitySource::Projection)
        say(log, "no seed points to project; falling back to camera proximity");
    stop_if(cancel);
    covisibility_from_proximity(ds, opt, out, cancel);
    return out;
}

// ===========================================================================
// Partition
// ===========================================================================

std::vector<int32_t> ScenePartition::frames_of(int part) const {
    std::vector<int32_t> out;
    if (part < 0 || part >= num_parts) return out;
    for (size_t i = 0; i < frame_label.size(); i++)
        if (frame_label[i] == part) out.push_back((int32_t)i);
    out.insert(out.end(), ring[(size_t)part].begin(), ring[(size_t)part].end());
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

int64_t ScenePartition::core_count(int part) const {
    int64_t n = 0;
    for (int32_t l : frame_label) n += l == part;
    return n;
}

ScenePartition partition_scene(const ParsedDataset& ds, const Covisibility& cov,
                               const PartitionOptions& opt, const PartitionLog& log,
                               const std::atomic<bool>* cancel) {
    const int64_t n_cam = ds.num_cameras;
    if (n_cam == 0) throw std::runtime_error("partition: the dataset has no cameras");
    ScenePartition p;
    p.options = opt;
    p.source = cov.source;
    p.frame_names = FrameIndex(ds.image_filenames).unique_tails();

    const int64_t n_pts = ds.points.num();
    const bool tracked = cov.has_tracks() && cov.num_points() == n_pts;
    p.frame_centers.resize((size_t)n_cam * 3);
    for (int64_t i = 0; i < n_cam; i++)
        for (int r = 0; r < 3; r++) p.frame_centers[(size_t)i * 3 + r] = ds.c2w[(size_t)i * 12 + r * 4 + 3];
    std::vector<int64_t> pick;
    {
        const int64_t stride = std::max<int64_t>(1, (n_pts + std::max(1, opt.max_seeds) - 1) /
                                                        std::max(1, opt.max_seeds));
        for (int64_t i = 0; i < n_pts; i += stride)
            if (!tracked || cov.beg[(size_t)i] < cov.beg[(size_t)i + 1]) pick.push_back(i);
    }
    std::vector<int32_t> own;
    const bool graph_cut = opt.method == PartitionMethod::Graph && tracked && n_pts > 0;

    if (graph_cut) {
        const Visibility vis = visibility_of(ds, cov, pick, p.frame_centers, n_cam, cancel);
        stop_if(cancel);
        const int64_t min_final = std::max<int64_t>(2, n_cam / 50);
        int32_t n_patch = 0;
        const std::vector<int32_t> patch = patch_points(vis, n_patch);
        // Smoothness: a point's neighbours weigh twice its observers, on
        // average (4 and 8 cost view share on every capture tried, 1 split more).
        constexpr double kSmooth = 2.0;
        double vote_mass = 0, nb_mass = 0;
        for (float w : vis.pw) vote_mass += w;
        for (float a : vis.nw) nb_mass += a;
        const double lambda = nb_mass > 0 ? kSmooth * vote_mass / nb_mass : 0.0;
        const graph::WeightedGraph joint = joint_graph(vis, patch, n_patch, n_cam, lambda);
        std::vector<double> cost((size_t)joint.n(), 1e-9);
        std::fill(cost.begin(), cost.begin() + n_cam, 1.0);

        // Cores are cut to `leaf`; when a part's ring takes it over the cap
        // the cores are cut smaller, in proportion.
        double leaf = (double)opt.max_images;
        std::vector<double> share;
        std::vector<int64_t> hits;
        for (int attempt = 0; attempt < 6; attempt++) {
            graph::LabelCutOptions co;
            co.parts = opt.parts > 0 ? (size_t)opt.parts : 0;
            co.leaf_max = leaf;
            co.min_part = 2;
            co.cost = cost.data();
            co.min_final = (size_t)min_final;
            co.cancel = cancel;
            const std::vector<int32_t> label = graph::cut_labels(joint, co);
            stop_if(cancel);
            p.frame_label.assign(label.begin(), label.begin() + n_cam);
            own.resize(pick.size());
            for (size_t j = 0; j < pick.size(); j++) own[j] = label[(size_t)(n_cam + patch[j])];
            int K = 0;
            for (int32_t l : label) K = std::max(K, l + 1);
            // Home: the part holding most of the view.
            view_shares(vis, own, K, share, hits);
            for (int64_t c = 0; c < n_cam; c++) {
                if (vis.cbeg[(size_t)c + 1] == vis.cbeg[(size_t)c]) { p.frame_label[(size_t)c] = -1; continue; }
                const double* row = &share[(size_t)c * K];
                p.frame_label[(size_t)c] = (int32_t)(std::max_element(row, row + K) - row);
            }
            std::vector<int64_t> load((size_t)K, 0);
            for (int64_t c = 0; c < n_cam; c++) {
                if (p.frame_label[(size_t)c] < 0) continue;
                for (int32_t l = 0; l < K; l++)
                    if (l == p.frame_label[(size_t)c] || (hits[(size_t)c * K + l] >= opt.ring_min_points &&
                                                          share[(size_t)c * K + l] >= opt.ring_fraction))
                        load[(size_t)l]++;
            }
            const int64_t heaviest = *std::max_element(load.begin(), load.end());
            double cut = 0, total = 0;
            for (uint32_t i = 0; i < joint.n(); i++)
                for (uint32_t k = joint.offs[i]; k < joint.offs[i + 1]; k++) {
                    total += joint.w[k];
                    cut += joint.w[k] * (label[i] != label[joint.adj[k]]);
                }
            char buf[160];
            std::snprintf(buf, sizeof buf, "visibility cut: %d parts, cores to %.0f, cut %.2f%% of the graph, heaviest part needs %lld cameras", K,
                          leaf, 100.0 * cut / std::max(total, 1e-300), (long long)heaviest);
            say(log, buf);
            if (opt.parts > 0 || heaviest <= opt.max_images || leaf < 0.25 * opt.max_images) break;
            leaf *= (double)opt.max_images / (double)heaviest;
        }
        fill_unseen(p.frame_label, p.frame_centers);
        absorb_tiny_parts(p.frame_label, own, p.frame_centers, min_final);
        p.num_parts = renumber_parts(p.frame_label, own);
    } else {
        // ---- the cut ----
        graph::LabelCutOptions co;
        co.parts = opt.parts > 0 ? (size_t)opt.parts : 0;
        co.leaf_max = (double)std::max(1, opt.max_images);
        co.min_part = 2;
        co.min_final = (size_t)std::max<int64_t>(2, n_cam / 50);
        co.cancel = cancel;
        p.frame_label = graph::cut_labels(cov.cameras, co);
        stop_if(cancel);

        // A camera that shares nothing with anyone is a part of its own after the
        // cut; it joins the part of the nearest camera that is in a real one.
        {
            int n_parts = 0;
            for (int32_t l : p.frame_label) n_parts = std::max(n_parts, l + 1);
            std::vector<int64_t> size((size_t)n_parts, 0);
            for (int32_t l : p.frame_label) size[(size_t)l]++;
            std::vector<char> tiny((size_t)n_parts, 0);
            int n_real = 0;
            for (int k = 0; k < n_parts; k++) {
                tiny[(size_t)k] = size[(size_t)k] < (int64_t)co.min_final;
                n_real += !tiny[(size_t)k];
            }
            if (n_real > 0 && n_real < n_parts) {
                for (int64_t i = 0; i < n_cam; i++) {
                    if (!tiny[(size_t)p.frame_label[(size_t)i]]) continue;
                    const float* a = &ds.c2w[(size_t)i * 12];
                    double best = 1e300;
                    int32_t pick = -1;
                    for (int64_t j = 0; j < n_cam; j++) {
                        if (tiny[(size_t)p.frame_label[(size_t)j]]) continue;
                        const float* b = &ds.c2w[(size_t)j * 12];
                        double d2 = 0;
                        for (int r = 0; r < 3; r++) {
                            const double d = (double)a[r * 4 + 3] - b[r * 4 + 3];
                            d2 += d * d;
                        }
                        if (d2 < best) { best = d2; pick = p.frame_label[(size_t)j]; }
                    }
                    if (pick >= 0) p.frame_label[(size_t)i] = pick;
                }
            }
            // Dense labels, largest part first.
            std::vector<int64_t> count((size_t)n_parts, 0);
            for (int32_t l : p.frame_label) count[(size_t)l]++;
            std::vector<int> order((size_t)n_parts);
            std::iota(order.begin(), order.end(), 0);
            std::stable_sort(order.begin(), order.end(),
                             [&](int a, int b) { return count[(size_t)a] > count[(size_t)b]; });
            std::vector<int32_t> remap((size_t)n_parts, -1);
            int next = 0;
            for (int k : order)
                if (count[(size_t)k] > 0) remap[(size_t)k] = next++;
            for (int32_t& l : p.frame_label) l = remap[(size_t)l];
            p.num_parts = next;
        }
    }
    if (p.num_parts > 254) throw std::runtime_error("partition: more than 254 parts");
    stop_if(cancel);

    p.core_pieces.assign((size_t)p.num_parts, 0);
    for (int k = 0; k < p.num_parts; k++) {
        std::vector<uint32_t> nodes;
        for (int64_t i = 0; i < n_cam; i++)
            if (p.frame_label[(size_t)i] == k) nodes.push_back((uint32_t)i);
        p.core_pieces[(size_t)k] = (int32_t)graph::connected_components(cov.cameras, nodes).size();
    }

    // ---- how much the cut severed ----
    {
        double total = 0, cut = 0;
        const graph::WeightedGraph& g = cov.cameras;
        for (uint32_t i = 0; i < g.n(); i++)
            for (uint32_t k = g.offs[i]; k < g.offs[i + 1]; k++) {
                total += g.w[k];
                if (p.frame_label[i] != p.frame_label[g.adj[k]]) cut += g.w[k];
            }
        p.cut_fraction = total > 0 ? cut / total : 0.0;
    }

    // ---- point ownership, and the field that carries it into space ----
    p.point_label.assign((size_t)n_pts, -1);
    {
        if (!graph_cut)
            own = own_points(ds, cov, p.frame_label, p.frame_centers, p.num_parts, pick, tracked, cancel);
        std::vector<float> xyz, dirs;
        std::vector<int32_t> lab;
        for (size_t j = 0; j < pick.size(); j++) {
            if (own[j] < 0) continue;
            const int64_t i = pick[j];
            const double* q = &ds.points.xyz[(size_t)i * 3];
            float d[3] = {0, 0, 0};
            if (tracked) {
                for (int64_t k = cov.beg[(size_t)i]; k < cov.beg[(size_t)i + 1]; k++) {
                    const float* c = &p.frame_centers[(size_t)cov.frame[(size_t)k] * 3];
                    float v[3] = {(float)(c[0] - q[0]), (float)(c[1] - q[1]), (float)(c[2] - q[2])};
                    const float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
                    if (len > 1e-9f)
                        for (int r = 0; r < 3; r++) d[r] += v[r] / len;
                }
            }
            for (int r = 0; r < 3; r++) xyz.push_back((float)q[r]);
            dirs.insert(dirs.end(), d, d + 3);
            lab.push_back(own[j]);
            p.point_label[(size_t)i] = own[j];
        }
        // Cameras seed the field only when no point does: a camera standing
        // in another part's owned space would cut a hole in it.
        if (lab.empty())
            for (int64_t i = 0; i < n_cam; i++) {
                for (int r = 0; r < 3; r++) xyz.push_back(p.frame_centers[(size_t)i * 3 + r]);
                dirs.insert(dirs.end(), {0.f, 0.f, 0.f});
                lab.push_back(p.frame_label[(size_t)i]);
            }
        p.field = std::make_shared<LabelField>(
            LabelField::build(xyz.data(), lab.data(), dirs.data(), (int64_t)lab.size()));
#pragma omp parallel for schedule(static)
        for (int64_t i = 0; i < n_pts; i++)
            if (p.point_label[(size_t)i] < 0)
                p.point_label[(size_t)i] = p.field->label(&ds.points.xyz[(size_t)i * 3]);
    }
    stop_if(cancel);

    // ---- rings: outside cameras that see enough of a part ----
    p.ring.assign((size_t)p.num_parts, {});
    if (cov.has_tracks() && cov.num_points() == n_pts) {
        std::vector<std::vector<int64_t>> seen_per_part((size_t)n_cam,
                                                        std::vector<int64_t>((size_t)p.num_parts, 0));
        std::vector<int64_t> seen_total((size_t)n_cam, 0);
        for (int64_t i = 0; i < n_pts; i++) {
            const int32_t owner = p.point_label[(size_t)i];
            for (int64_t k = cov.beg[(size_t)i]; k < cov.beg[(size_t)i + 1]; k++) {
                const int32_t f = cov.frame[(size_t)k];
                seen_per_part[(size_t)f][(size_t)owner]++;
                seen_total[(size_t)f]++;
            }
        }
        for (int64_t c = 0; c < n_cam; c++) {
            if (seen_total[(size_t)c] == 0) continue;
            for (int k = 0; k < p.num_parts; k++) {
                if (k == p.frame_label[(size_t)c]) continue;
                const int64_t n = seen_per_part[(size_t)c][(size_t)k];
                if (n >= opt.ring_min_points &&
                    (double)n >= opt.ring_fraction * (double)seen_total[(size_t)c])
                    p.ring[(size_t)k].push_back((int32_t)c);
            }
        }
    } else {
        // Proximity only: a camera joins the ring of every part it has an
        // edge into worth a share of its own connectivity.
        const graph::WeightedGraph& g = cov.cameras;
        for (uint32_t c = 0; c < g.n(); c++) {
            std::vector<double> into((size_t)p.num_parts, 0.0);
            double total = 0;
            for (uint32_t k = g.offs[c]; k < g.offs[c + 1]; k++) {
                into[(size_t)p.frame_label[g.adj[k]]] += g.w[k];
                total += g.w[k];
            }
            for (int k = 0; k < p.num_parts; k++)
                if (k != p.frame_label[c] && total > 0 && into[(size_t)k] >= opt.ring_fraction * total)
                    p.ring[(size_t)k].push_back((int32_t)c);
        }
    }

    // ---- what each part seeds from ----
    p.part_points.assign((size_t)p.num_parts, {});
    {
        std::vector<int32_t> part_of_frame((size_t)n_cam, -1);
        std::vector<std::vector<uint8_t>> in_part((size_t)p.num_parts,
                                                  std::vector<uint8_t>((size_t)n_cam, 0));
        for (int k = 0; k < p.num_parts; k++)
            for (int32_t f : p.frames_of(k)) in_part[(size_t)k][(size_t)f] = 1;
        for (int k = 0; k < p.num_parts; k++) {
            stop_if(cancel);
            std::vector<uint8_t> take((size_t)n_pts, 0);
            // Outside the region, a hash-stable share of what the cameras see.
            const uint32_t keep = (uint32_t)(std::clamp(opt.outside_seed_fraction, 0.0f, 1.0f) * 65536.0f);
            for (int64_t i = 0; i < n_pts; i++) {
                if (p.point_label[(size_t)i] == k) { take[(size_t)i] = 1; continue; }
                if (!tracked || ((uint32_t)((uint64_t)i * 2654435761u) >> 16) >= keep) continue;
                for (int64_t t = cov.beg[(size_t)i]; t < cov.beg[(size_t)i + 1]; t++)
                    if (in_part[(size_t)k][(size_t)cov.frame[(size_t)t]]) { take[(size_t)i] = 1; break; }
            }
            for (int64_t i = 0; i < n_pts; i++)
                if (take[(size_t)i]) p.part_points[(size_t)k].push_back(i);
        }
    }

    // ---- how much of what a part's cameras see is the part's own ----
    p.view_share.assign((size_t)p.num_parts, 0.0f);
    if (tracked) {
        std::vector<int64_t> seen((size_t)n_cam, 0), mine((size_t)n_cam * p.num_parts, 0);
        for (int64_t i = 0; i < n_pts; i++)
            for (int64_t k = cov.beg[(size_t)i]; k < cov.beg[(size_t)i + 1]; k++) {
                seen[(size_t)cov.frame[(size_t)k]]++;
                mine[(size_t)cov.frame[(size_t)k] * p.num_parts + p.point_label[(size_t)i]]++;
            }
        for (int k = 0; k < p.num_parts; k++) {
            const std::vector<int32_t> f = p.frames_of(k);
            double sum = 0;
            for (int32_t c : f)
                if (seen[(size_t)c] > 0) sum += (double)mine[(size_t)c * p.num_parts + k] / (double)seen[(size_t)c];
            p.view_share[(size_t)k] = f.empty() ? 0.0f : (float)(sum / (double)f.size());
        }
    }

    if (log) {
        char buf[256];
        std::snprintf(buf, sizeof buf, "partition: %d parts from %lld cameras (%s), cut %.1f%%",
                      p.num_parts, (long long)n_cam, covisibility_source_name(p.source),
                      100.0 * p.cut_fraction);
        log(buf);
        for (int k = 0; k < p.num_parts; k++) {
            std::snprintf(buf, sizeof buf, "  part %d: core %lld, ring %lld, points %lld, own share of view %.0f%%%s", k,
                          (long long)p.core_count(k), (long long)p.ring[(size_t)k].size(),
                          (long long)p.part_points[(size_t)k].size(), 100.0 * p.view_share[(size_t)k],
                          p.core_pieces[(size_t)k] > 1 ? " (not one piece)" : "");
            log(buf);
        }
    }
    return p;
}

// ===========================================================================
// Region masks
// ===========================================================================

std::vector<std::string> write_region_masks(const ParsedDataset& ds, const double* xyz, int64_t n_pts,
                                            const uint8_t* point_inside,
                                            const std::string& dir, bool flip_existing,
                                            double* masked_share) {
    constexpr int kScale = 4;
    const int64_t n_cam = ds.num_cameras;
    std::vector<std::string> out((size_t)n_cam);
    std::error_code ec;
    fs::create_directories(dir, ec);
    double masked = 0, total = 0;
    // Typical spacing between neighbouring points: a point covers that much
    // around it, so near surfaces hide what lies behind their sparse samples.
    double spacing = 0;
    {
        std::vector<float> sample;
        const int64_t step = std::max<int64_t>(1, n_pts / 20000);
        for (int64_t p = 0; p < n_pts; p += step)
            for (int r = 0; r < 3; r++) sample.push_back((float)xyz[(size_t)p * 3 + r]);
        const int64_t m = (int64_t)sample.size() / 3;
        if (m > 8) {
            const knn::KdTree3 tree(sample.data(), m);
            std::vector<float> nn;
            for (int64_t j = 0; j < m; j++) {
                float d2[2];
                if (tree.query(&sample[(size_t)j * 3], (int32_t)j, 1, d2) == 1) nn.push_back(std::sqrt(d2[0]));
            }
            std::nth_element(nn.begin(), nn.begin() + nn.size() / 2, nn.end());
            // The sample is `step` times sparser than the cloud.
            spacing = nn[nn.size() / 2] / std::cbrt((double)step);
        }
    }
#pragma omp parallel for schedule(dynamic, 4) reduction(+ : masked, total)
    for (int64_t i = 0; i < n_cam; i++) {
        const FrameView f = frame_view(ds, i);
        const int gw = std::max(1, (f.cam.width + kScale - 1) / kScale);
        const int gh = std::max(1, (f.cam.height + kScale - 1) / kScale);
        // Nearest point per cell over its footprint: -1 none, 0 out, 1 in.
        std::vector<float> depth((size_t)gw * gh, 3.0e38f);
        std::vector<int8_t> cell((size_t)gw * gh, -1);
        for (int64_t p = 0; p < n_pts; p++) {
            const double* q = &xyz[(size_t)p * 3];
            const double d[3] = {q[0] - f.t[0], q[1] - f.t[1], q[2] - f.t[2]};
            double ray[3];
            for (int r = 0; r < 3; r++) ray[r] = f.R[r * 3] * d[0] + f.R[r * 3 + 1] * d[1] + f.R[r * 3 + 2] * d[2];
            const double len = std::sqrt(ray[0] * ray[0] + ray[1] * ray[1] + ray[2] * ray[2]);
            if (!(len > 1e-12) || (f.cos_max > -1.5 && ray[2] / len < f.cos_max)) continue;
            double px[2];
            if (!camhost::ray_in_frame(f.cam, ray, px)) continue;
            const int cx = (int)(px[0] / kScale), cy = (int)(px[1] / kScale);
            const int rad = (int)std::clamp(2.0 * spacing * f.cam.fx / (len * kScale), 1.0, 16.0);
            for (int dy = -rad; dy <= rad; dy++)
                for (int dx = -rad; dx <= rad; dx++) {
                    const int x = cx + dx, y = cy + dy;
                    if (x < 0 || y < 0 || x >= gw || y >= gh) continue;
                    const size_t c = (size_t)y * gw + x;
                    if ((float)len < depth[c]) {
                        depth[c] = (float)len;
                        cell[c] = point_inside[p] ? 1 : 0;
                    }
                }
        }
        // Holes take a labelled neighbour's; what stays unknown is kept.
        for (int pass = 0; pass < 6; pass++) {
            std::vector<int8_t> next = cell;
            for (int y = 0; y < gh; y++)
                for (int x = 0; x < gw; x++) {
                    if (cell[(size_t)y * gw + x] >= 0) continue;
                    for (int k = 0; k < 4; k++) {
                        const int xx = x + (k == 0) - (k == 1), yy = y + (k == 2) - (k == 3);
                        if (xx >= 0 && yy >= 0 && xx < gw && yy < gh && cell[(size_t)yy * gw + xx] >= 0) {
                            next[(size_t)y * gw + x] = cell[(size_t)yy * gw + xx];
                            break;
                        }
                    }
                }
            cell.swap(next);
        }
        // Keep a margin past the region's edge in the image, so the seam is
        // still supervised from this side.
        const int margin = std::max(2, gw / 40);
        std::vector<uint8_t> keep((size_t)gw * gh, 0);
        for (int y = 0; y < gh; y++)
            for (int x = 0; x < gw; x++)
                if (cell[(size_t)y * gw + x] != 0) {
                    for (int yy = std::max(0, y - margin); yy <= std::min(gh - 1, y + margin); yy++)
                        for (int xx = std::max(0, x - margin); xx <= std::min(gw - 1, x + margin); xx++)
                            keep[(size_t)yy * gw + xx] = 255;
                }
        if (i < (int64_t)ds.mask_filenames.size() && !ds.mask_filenames[(size_t)i].empty()) {
            int w = 0, h = 0, ch = 0;
            if (stbi_uc* img = stbi_load(ds.mask_filenames[(size_t)i].c_str(), &w, &h, &ch, 1)) {
                for (int y = 0; y < gh; y++)
                    for (int x = 0; x < gw; x++) {
                        const int sx = std::min(w - 1, x * w / gw), sy = std::min(h - 1, y * h / gh);
                        const bool user = (img[(size_t)sy * w + sx] != 0) != flip_existing;
                        if (!user) keep[(size_t)y * gw + x] = 0;
                    }
                stbi_image_free(img);
            }
        }
        for (uint8_t k : keep) masked += k == 0;
        total += (double)keep.size();
        // Written in the loader's polarity: it flips every mask when asked to.
        if (flip_existing)
            for (uint8_t& k : keep) k = (uint8_t)(255 - k);
        char name[32];
        std::snprintf(name, sizeof name, "%06lld.png", (long long)i);
        out[(size_t)i] = (fs::path(dir) / name).string();
        stbi_write_png(out[(size_t)i].c_str(), gw, gh, 1, keep.data(), gw);
    }
    if (masked_share) *masked_share = total > 0 ? masked / total : 0.0;
    return out;
}

// ===========================================================================
// Files
// ===========================================================================

namespace {

constexpr char kBinMagic[4] = {'S', 'S', 'P', 'T'};

template <typename T>
void put(std::string& out, const T& v) {
    out.append(reinterpret_cast<const char*>(&v), sizeof v);
}
template <typename T>
bool get(const std::string& s, size_t& at, T& v) {
    if (at + sizeof v > s.size()) return false;
    std::memcpy(&v, s.data() + at, sizeof v);
    at += sizeof v;
    return true;
}

std::string read_file(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) throw std::runtime_error("cannot read " + path);
    std::string bytes;
    char buf[1 << 16];
    size_t got;
    while ((got = std::fread(buf, 1, sizeof buf, f)) > 0) bytes.append(buf, got);
    std::fclose(f);
    return bytes;
}

void write_file(const std::string& path, const std::string& bytes) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) throw std::runtime_error("cannot write " + path);
    std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
}

}  // namespace

void write_partition(const ScenePartition& p, const std::string& json_path,
                     const std::string& dataset) {
    const fs::path jp(json_path);
    const std::string bin_name = jp.stem().string() + ".bin";
    std::error_code ec;
    if (!jp.parent_path().empty()) fs::create_directories(jp.parent_path(), ec);

    std::string bin;
    bin.append(kBinMagic, 4);
    put(bin, (uint32_t)2);
    if (p.field) p.field->write(bin);
    else LabelField().write(bin);
    put(bin, (uint64_t)p.frame_centers.size() / 3);
    bin.append(reinterpret_cast<const char*>(p.frame_centers.data()),
               p.frame_centers.size() * sizeof(float));
    put(bin, (uint64_t)p.point_label.size());
    for (int32_t l : p.point_label) put(bin, (uint8_t)(l < 0 ? LabelField::kNone : l));
    put(bin, (uint32_t)p.num_parts);
    for (int k = 0; k < p.num_parts; k++) {
        put(bin, (uint64_t)p.part_points[(size_t)k].size());
        for (int64_t i : p.part_points[(size_t)k]) put(bin, (uint32_t)i);
    }
    write_file((jp.parent_path() / bin_name).string(), bin);

    JsonWriter w;
    w.object();
    w.field("format", "spirula-scene-partition");
    w.field("version", 1);
    w.field("dataset", dataset);
    w.field("source", covisibility_source_name(p.source));
    w.key("options").object();
    w.field("method", partition_method_name(p.options.method));
    w.field("parts", p.options.parts);
    w.field("max_images", p.options.max_images);
    w.field("ring_fraction", p.options.ring_fraction);
    w.field("ring_min_points", p.options.ring_min_points);
    w.field("outside_seed_fraction", p.options.outside_seed_fraction);
    w.field("max_seeds", p.options.max_seeds);
    w.field("source", covisibility_source_name(p.options.source));
    w.field("max_projected_points", p.options.max_projected_points);
    w.field("proximity_neighbours", p.options.proximity_neighbours);
    w.end();
    w.field("num_parts", p.num_parts);
    w.field("num_frames", (long long)p.frame_names.size());
    w.field("num_points", (long long)p.point_label.size());
    w.field("cut_fraction", p.cut_fraction);
    w.field("binary", bin_name);
    w.key("frame_names").array();
    for (const std::string& s : p.frame_names) w.value(s);
    w.end();
    w.key("frame_parts").array();
    for (int32_t l : p.frame_label) w.value(l);
    w.end();
    w.key("parts").array();
    for (int k = 0; k < p.num_parts; k++) {
        w.object();
        w.field("core", (long long)p.core_count(k));
        w.field("points", (long long)p.part_points[(size_t)k].size());
        w.key("ring").array();
        for (int32_t f : p.ring[(size_t)k]) w.value(f);
        w.end();
        w.end();
    }
    w.end();
    w.end();
    write_file(json_path, w.str());
}

ScenePartition read_partition(const std::string& json_path, std::string* dataset) {
    const JsonValue root = json_parse_file(json_path);
    if (!root.is_object() || !root.find("format") ||
        root.find("format")->as_string() != "spirula-scene-partition")
        throw std::runtime_error(json_path + " is not a partition file");
    ScenePartition p;
    if (dataset) *dataset = root.find("dataset") ? root.find("dataset")->as_string() : "";
    if (const JsonValue* s = root.find("source"))
        covisibility_source_from_name(s->as_string(), p.source);
    if (const JsonValue* o = root.find("options"); o && o->is_object()) {
        p.options.parts = (int)o->find("parts")->as_int(0);
        p.options.max_images = (int)o->get_double("max_images", p.options.max_images);
        p.options.ring_fraction = (float)o->get_double("ring_fraction", p.options.ring_fraction);
        p.options.outside_seed_fraction =
            (float)o->get_double("outside_seed_fraction", p.options.outside_seed_fraction);
        if (const JsonValue* m = o->find("method"))
            partition_method_from_name(m->as_string(), p.options.method);
        p.options.ring_min_points = (int)o->get_double("ring_min_points", p.options.ring_min_points);
        p.options.max_seeds = (int)o->get_double("max_seeds", p.options.max_seeds);
        if (const JsonValue* s = o->find("source"))
            covisibility_source_from_name(s->as_string(), p.options.source);
        p.options.max_projected_points =
            (int)o->get_double("max_projected_points", p.options.max_projected_points);
        p.options.proximity_neighbours =
            (int)o->get_double("proximity_neighbours", p.options.proximity_neighbours);
    }
    p.num_parts = (int)root.find("num_parts")->as_int(0);
    p.cut_fraction = root.get_double("cut_fraction", 0.0);
    if (const JsonValue* a = root.find("frame_names"); a && a->is_array())
        for (const JsonValue& v : a->arr) p.frame_names.push_back(v.as_string());
    if (const JsonValue* a = root.find("frame_parts"); a && a->is_array())
        for (const JsonValue& v : a->arr) p.frame_label.push_back((int32_t)v.as_int(-1));
    if (p.frame_label.size() != p.frame_names.size())
        throw std::runtime_error(json_path + ": frame_names and frame_parts differ in length");
    p.ring.assign((size_t)p.num_parts, {});
    if (const JsonValue* a = root.find("parts"); a && a->is_array()) {
        if ((int)a->arr.size() != p.num_parts)
            throw std::runtime_error(json_path + ": parts[] does not match num_parts");
        for (size_t k = 0; k < a->arr.size(); k++)
            if (const JsonValue* r = a->arr[k].find("ring"); r && r->is_array())
                for (const JsonValue& v : r->arr) p.ring[k].push_back((int32_t)v.as_int(-1));
    }

    const std::string bin_name = root.find("binary") ? root.find("binary")->as_string()
                                                     : fs::path(json_path).stem().string() + ".bin";
    const std::string bin = read_file((fs::path(json_path).parent_path() / bin_name).string());
    size_t at = 0;
    if (bin.size() < 8 || std::memcmp(bin.data(), kBinMagic, 4) != 0)
        throw std::runtime_error(bin_name + " is not a partition table");
    at = 4;
    uint32_t version = 0;
    if (!get(bin, at, version) || version != 2)
        throw std::runtime_error(bin_name + ": unsupported version; compute the partition again");
    size_t used = 0;
    p.field = std::make_shared<LabelField>();
    if (!LabelField::read(bin.data() + at, bin.size() - at, used, *p.field))
        throw std::runtime_error(bin_name + ": bad label field");
    at += used;
    uint64_t n_centers = 0;
    if (!get(bin, at, n_centers) || at + n_centers * 12 > bin.size())
        throw std::runtime_error(bin_name + ": truncated camera centres");
    p.frame_centers.resize((size_t)n_centers * 3);
    std::memcpy(p.frame_centers.data(), bin.data() + at, (size_t)n_centers * 12);
    at += n_centers * 12;
    uint64_t n_pts = 0;
    if (!get(bin, at, n_pts) || at + n_pts > bin.size())
        throw std::runtime_error(bin_name + ": truncated point labels");
    p.point_label.resize((size_t)n_pts);
    for (uint64_t i = 0; i < n_pts; i++) {
        const uint8_t l = (uint8_t)bin[at + i];
        p.point_label[(size_t)i] = l == LabelField::kNone ? -1 : l;
    }
    at += n_pts;
    uint32_t n_parts = 0;
    if (!get(bin, at, n_parts) || (int)n_parts != p.num_parts)
        throw std::runtime_error(bin_name + ": part count differs from the json");
    p.part_points.assign((size_t)n_parts, {});
    for (uint32_t k = 0; k < n_parts; k++) {
        uint64_t n = 0;
        if (!get(bin, at, n) || at + n * 4 > bin.size())
            throw std::runtime_error(bin_name + ": truncated part points");
        p.part_points[k].resize((size_t)n);
        for (uint64_t i = 0; i < n; i++) {
            uint32_t v;
            std::memcpy(&v, bin.data() + at + i * 4, 4);
            p.part_points[k][(size_t)i] = v;
        }
        at += n * 4;
    }
    return p;
}

// ===========================================================================
// Applying one part to a parsed dataset
// ===========================================================================

bool apply_partition(ParsedDataset& ds, const ScenePartition& p, int part,
                     PartitionApplied& out) {
    if (part < 0 || part >= p.num_parts) return false;
    out = PartitionApplied{};
    out.frames_before = ds.num_cameras;
    out.points_before = ds.points.num();

    const FrameIndex index(ds.image_filenames);
    const int64_t n = ds.num_cameras;
    std::vector<uint8_t> keep((size_t)n, 0);
    for (int32_t f : p.frames_of(part)) {
        const int32_t i = index.find(p.frame_names[(size_t)f]);
        if (i < 0 || keep[(size_t)i]) {
            out.missing++;
            continue;
        }
        keep[(size_t)i] = 1;
        if (p.frame_label[(size_t)f] == part) out.core++;
        else out.ring++;
    }

    keep_rows(ds.camera_models, n, 1, keep.data());
    keep_rows(ds.camera_distortions, n, 1, keep.data());
    keep_rows(ds.image_filenames, n, 1, keep.data());
    keep_rows(ds.mask_filenames, n, 1, keep.data());
    keep_rows(ds.depth_filenames, n, 1, keep.data());
    keep_rows(ds.normal_filenames, n, 1, keep.data());
    keep_rows(ds.widths, n, 1, keep.data());
    keep_rows(ds.heights, n, 1, keep.data());
    keep_rows(ds.c2w, n, 12, keep.data());
    keep_rows(ds.intrins, n, 4, keep.data());
    keep_rows(ds.dist_coeffs, n, 8, keep.data());
    keep_rows(ds.redistort, n, 1, keep.data());
    keep_rows(ds.exif_quarter_turns, n, 1, keep.data());
    std::vector<int32_t> new_index((size_t)n, -1);
    int32_t w = 0;
    for (int64_t i = 0; i < n; i++)
        if (keep[(size_t)i]) new_index[(size_t)i] = w++;
    auto remap = [&](std::vector<int32_t>& v) {
        std::vector<int32_t> o;
        for (int32_t i : v)
            if (i >= 0 && i < n && new_index[(size_t)i] >= 0) o.push_back(new_index[(size_t)i]);
        v.swap(o);
    };
    remap(ds.train_indices);
    remap(ds.val_indices);
    ds.num_cameras = w;
    out.frames_after = w;

    const int64_t n_pts = ds.points.num();
    if (n_pts > 0 && (int64_t)p.point_label.size() == n_pts) {
        std::vector<uint8_t> pk((size_t)n_pts, 0);
        for (int64_t i : p.part_points[(size_t)part])
            if (i >= 0 && i < n_pts) pk[(size_t)i] = 1;
        keep_rows(ds.points.xyz, n_pts, 3, pk.data());
        keep_rows(ds.points.rgb, n_pts, 3, pk.data());
    }
    out.points_after = ds.points.num();
    return true;
}

// ===========================================================================
// Colours
// ===========================================================================

void part_color(int part, float rgb[3]) {
    // Golden-angle hues at full saturation, alternating value so neighbours in
    // index differ in more than hue.
    const double h = std::fmod(0.11 + part * 0.6180339887498949, 1.0) * 6.0;
    const double v = (part % 3 == 1) ? 0.72 : ((part % 3 == 2) ? 0.88 : 1.0);
    const double s = (part % 2) ? 0.85 : 0.65;
    const int i = (int)std::floor(h);
    const double f = h - i;
    const double p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
    double r, g, b;
    switch (i % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    rgb[0] = (float)r;
    rgb[1] = (float)g;
    rgb[2] = (float)b;
}

}  // namespace spirula

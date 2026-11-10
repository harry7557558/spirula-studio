// pca_reg_parity -- the local-PCA regularizer's launchers (kernels/loss/
// PcaReg.cu, shaders/pca_reg.slang) against a double-precision host mirror:
// the Morton order, the tree search against brute force, the window picks,
// and every mode's gradients and loss sums, in both scale modes. The host
// gradient is itself held to finite differences, and the scale-agnostic terms
// to a similarity transform of the cloud. Self-checking, both backends.

#include <kernels/loss/PcaReg.cuh>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

using backend::MemcpyKind;

namespace {

using D3 = std::array<double, 3>;

int g_failures = 0;
void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

template <typename T>
struct Dev {
    T* p = nullptr;
    int64_t n = 0;
    explicit Dev(int64_t count, const T* host = nullptr) : n(count) {
        p = (T*)backend::device_malloc(std::max<size_t>(16, (size_t)count * sizeof(T)));
        if (host) backend::memcpy_sync(p, host, (size_t)count * sizeof(T), MemcpyKind::HostToDevice);
        else backend::memset_sync(p, 0, (size_t)count * sizeof(T));
    }
    ~Dev() { backend::device_free(p); }
    Dev(const Dev&) = delete;
    Dev& operator=(const Dev&) = delete;
    DeviceVector<T> v() const {
        return DeviceVector<T>(std::make_tuple((uint64_t)p, (uint32_t)sizeof(T),
                                               std::vector<int64_t>{n, 1}));
    }
    std::vector<T> host() const {
        std::vector<T> h((size_t)n);
        backend::memcpy_sync(h.data(), p, (size_t)n * sizeof(T), MemcpyKind::DeviceToHost);
        return h;
    }
    void zero() { backend::memset_sync(p, 0, (size_t)n * sizeof(T)); }
};

D3 sub(D3 a, D3 b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
double dot(D3 a, D3 b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

// Eigenpairs of a symmetric 3x3, ascending; Jacobi to convergence in double.
void eigen3(double a[3][3], double ev[3], D3 vec[3]) {
    double v[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    for (int sweep = 0; sweep < 60; sweep++) {
        const double off = a[0][1] * a[0][1] + a[0][2] * a[0][2] + a[1][2] * a[1][2];
        if (off < 1e-60) break;
        for (int p = 0; p < 2; p++)
            for (int q = p + 1; q < 3; q++) {
                if (a[p][q] == 0.0) continue;
                const double th = (a[q][q] - a[p][p]) / (2 * a[p][q]);
                const double t = (th >= 0 ? 1 : -1) / (std::fabs(th) + std::sqrt(th * th + 1));
                const double c = 1 / std::sqrt(t * t + 1), s = t * c;
                for (int k = 0; k < 3; k++) {
                    const double akp = a[k][p], akq = a[k][q];
                    a[k][p] = c * akp - s * akq;
                    a[k][q] = s * akp + c * akq;
                }
                for (int k = 0; k < 3; k++) {
                    const double apk = a[p][k], aqk = a[q][k];
                    a[p][k] = c * apk - s * aqk;
                    a[q][k] = s * apk + c * aqk;
                }
                for (int k = 0; k < 3; k++) {
                    const double vkp = v[k][p], vkq = v[k][q];
                    v[k][p] = c * vkp - s * vkq;
                    v[k][q] = s * vkp + c * vkq;
                }
            }
    }
    int o[3] = {0, 1, 2};
    std::sort(o, o + 3, [&](int x, int y) { return a[x][x] < a[y][y]; });
    for (int m = 0; m < 3; m++) {
        ev[m] = a[o[m]][o[m]];
        vec[m] = {v[0][o[m]], v[1][o[m]], v[2][o[m]]};
    }
}

// Column `axis` of the rotation of (w, x, y, z), normalized.
D3 quat_axis(const double q[4], int axis) {
    const double inv = 1 / std::sqrt(std::max(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3], 1e-20));
    const double w = q[0] * inv, x = q[1] * inv, y = q[2] * inv, z = q[3] * inv;
    if (axis == 0) return {1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w)};
    if (axis == 1) return {2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w)};
    return {2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y)};
}

struct Spectrum {
    double ev[3];
    D3 vec[3];
    D3 mean;
};

Spectrum spectrum(const std::vector<D3>& pts) {
    Spectrum s{};
    const double inv = 1.0 / (double)pts.size();
    s.mean = {0, 0, 0};
    for (const D3& p : pts)
        for (int c = 0; c < 3; c++) s.mean[c] += p[c] * inv;
    double a[3][3] = {};
    for (const D3& p : pts) {
        const D3 d = sub(p, s.mean);
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++) a[i][j] += d[i] * d[j] * inv;
    }
    eigen3(a, s.ev, s.vec);
    return s;
}

double pca_loss(const std::vector<D3>& pts, double gamma) {
    const Spectrum s = spectrum(pts);
    return s.ev[0] + gamma * (s.ev[2] - s.ev[1]);
}

struct Cloud {
    std::vector<float> means, quats, scales;
    int64_t n() const { return (int64_t)means.size() / 3; }
    D3 at(int64_t i) const { return {means[3 * i], means[3 * i + 1], means[3 * i + 2]}; }
};

// Splats on a few jittered anisotropic grids, so every neighbourhood has
// well-separated eigenvalues; `messy` adds floaters, clumps and duplicates.
Cloud make_cloud(int per_plane, bool messy, uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> u(0.f, 1.f);
    std::normal_distribution<float> g(0.f, 1.f);
    Cloud c;
    auto push = [&](float x, float y, float z) {
        c.means.insert(c.means.end(), {x, y, z});
    };
    const float planes[4][9] = {
        {0, 0, 0, 1, 0, 0, 0, 1, 0},
        {2, 0, 1, 0, 1, 0, 0, 0, 1},
        {-1, 2, 0, 0.6f, 0.8f, 0, 0, 0, 1},
        {0, -3, 2, 1, 0, 0, 0, 0.6f, 0.8f},
    };
    for (const auto& pl : planes) {
        const int side = (int)std::sqrt((float)per_plane / 5.0f);
        for (int i = 0; i < side; i++)
            for (int j = 0; j < 5 * side; j++) {
                const float a = (i + 0.3f * u(rng)) * 0.05f, b = (j + 0.3f * u(rng)) * 0.01f;
                const float h = 0.002f * g(rng);
                const float nx = pl[4] * pl[8] - pl[5] * pl[7];
                const float ny = pl[5] * pl[6] - pl[3] * pl[8];
                const float nz = pl[3] * pl[7] - pl[4] * pl[6];
                push(pl[0] + a * pl[3] + b * pl[6] + h * nx, pl[1] + a * pl[4] + b * pl[7] + h * ny,
                     pl[2] + a * pl[5] + b * pl[8] + h * nz);
            }
    }
    if (messy) {
        for (int i = 0; i < per_plane / 10; i++) push(8 * g(rng), 8 * g(rng), 8 * g(rng));
        for (int i = 0; i < per_plane / 10; i++)
            push(5 + 0.01f * g(rng), 5 + 0.01f * g(rng), 5 + 0.01f * g(rng));
        const int64_t base = c.n();
        for (int i = 0; i < 200; i++) {
            const int64_t j = (int64_t)(u(rng) * (float)(base - 1));
            push(c.means[3 * j], c.means[3 * j + 1], c.means[3 * j + 2]);
        }
        push(1e4f, -1e4f, 3e3f);
    }
    const int64_t n = c.n();
    for (int64_t i = 0; i < n; i++) {
        float q[4] = {g(rng), g(rng), g(rng), g(rng)};
        c.quats.insert(c.quats.end(), q, q + 4);
        c.scales.insert(c.scales.end(), {-4 + g(rng), -4 + g(rng), -4 + g(rng)});
    }
    return c;
}

struct DeviceCloud {
    Dev<float3> means;
    Dev<float4> quats;
    Dev<float3> scales;
    explicit DeviceCloud(const Cloud& c)
        : means(c.n(), (const float3*)c.means.data()), quats(c.n(), (const float4*)c.quats.data()),
          scales(c.n(), (const float3*)c.scales.data()) {}
};

struct Weights {
    float plane, tangent, normal, surfel;
};

// Screen radii with about a third zero, for the gated scatter.
std::vector<float> make_radii(int64_t n) {
    std::vector<float> r((size_t)n);
    for (int64_t i = 0; i < n; i++) r[(size_t)i] = (i * 2654435761u) % 3 == 0 ? 0.0f : 2.0f;
    return r;
}

struct Run {
    std::vector<int32_t> nbrs;
    std::vector<float> v_means, v_quats, v_scales;
    float sums[5];
};

struct Tree {
    DeviceVector<float4> nodes;
    DeviceVector<int4> meta;
    DeviceVector<int32_t> ranges;
};

Run run_mode(const DeviceCloud& d, int64_t n, PcaRegMode mode, int k, int window,
             const Dev<int32_t>& perm, const Dev<float4>& sorted, const Tree& tree,
             Dev<int32_t>& nbrs, bool store, const Weights& w, const Dev<float>* radii = nullptr,
             bool sa = false) {
    Dev<float3> vm(n);
    Dev<float4> vq(n);
    Dev<float3> vs(n);
    Dev<float> sums(5);
    pca_reg_neighborhood_tensor(n, (int)mode, k, window, d.means.v(), d.quats.v(), d.scales.v(), DeviceVector<float>(), 0,
                                perm.v(), sorted.v(), tree.nodes, tree.meta, tree.ranges,
                                nbrs.v(), store, radii ? radii->v() : DeviceVector<float>(),
                                radii != nullptr, sa, vm.v(), vq.v(), vs.v(), DeviceVector<float>(), w.plane, w.tangent, w.normal,
                                w.surfel, 0.0f, 0.0f, sums.v());
    backend::device_synchronize();
    Run r;
    r.nbrs = nbrs.host();
    const auto hm = vm.host();
    const auto hq = vq.host();
    const auto hs = vs.host();
    r.v_means.assign((const float*)hm.data(), (const float*)hm.data() + 3 * n);
    r.v_quats.assign((const float*)hq.data(), (const float*)hq.data() + 4 * n);
    r.v_scales.assign((const float*)hs.data(), (const float*)hs.data() + 3 * n);
    const auto hl = sums.host();
    for (int c = 0; c < 3; c++) r.sums[c] = hl[c];
    return r;
}

struct HostGrad {
    std::vector<double> v_means, v_quats, v_scales;
    std::vector<char> dirty;  // touched by an ill-conditioned neighbourhood
    double sums[3] = {0, 0, 0};
    double lambda3 = 0;       // the scale float error in the sums follows
};

// The regularizer from neighbour lists (row r = owner perm[r]); with radii,
// only splats of positive radius take a mean gradient. `sa`: scale-agnostic.
HostGrad host_grad(const Cloud& c, const std::vector<int32_t>& perm,
                   const std::vector<int32_t>& nbrs, int k, const Weights& w,
                   const std::vector<float>* radii = nullptr, bool sa = false) {
    const int64_t n = c.n();
    HostGrad h;
    h.v_means.assign(3 * n, 0.0);
    h.v_quats.assign(4 * n, 0.0);
    h.v_scales.assign(3 * n, 0.0);
    h.dirty.assign(n, 0);
    for (int64_t r = 0; r < n; r++) {
        const int64_t self = perm[r];
        std::vector<int64_t> ids;
        for (int m = 0; m < k; m++)
            if (nbrs[r * k + m] >= 0) ids.push_back(nbrs[r * k + m]);
        if (ids.size() < 2) continue;
        ids.push_back(self);
        std::vector<D3> pts;
        for (int64_t id : ids) pts.push_back(c.at(id));
        const Spectrum s = spectrum(pts);
        const double gap = std::min(s.ev[1] - s.ev[0], s.ev[2] - s.ev[1]);
        const bool ill = gap < 1e-2 * std::max(s.ev[2], 1e-30);
        const double spread = s.ev[0] + s.ev[1] + s.ev[2];
        const D3 at = c.at(self);
        const double inv_t = !sa ? 1.0 : spread > 1e-30 ? 1.0 / spread : 0.0;
        const double g = 2.0 / (double)pts.size() * inv_t;
        for (size_t m = 0; m < ids.size(); m++) {
            const D3 d = sub(pts[m], s.mean);
            for (int a = 0; a < 3 && (!radii || (*radii)[ids[m]] > 0.0f); a++) {
                const double v = g * (w.plane * dot(s.vec[0], d) * s.vec[0][a] +
                                      w.tangent * (dot(s.vec[2], d) * s.vec[2][a] -
                                                   dot(s.vec[1], d) * s.vec[1][a]));
                h.v_means[3 * ids[m] + a] += v;
            }
            if (ill) h.dirty[ids[m]] = 1;
        }
        const float* ls = &c.scales[3 * self];
        const int axis = ls[0] <= ls[1] ? (ls[0] <= ls[2] ? 0 : 2) : (ls[1] <= ls[2] ? 1 : 2);
        double q[4] = {c.quats[4 * self], c.quats[4 * self + 1], c.quats[4 * self + 2], c.quats[4 * self + 3]};
        const D3 normal = quat_axis(q, axis);
        const double ndot = dot(normal, s.vec[0]);
        const bool directional = s.ev[1] - s.ev[0] > 1e-6 * std::max(spread, 1e-30);
        const double align = directional && w.normal != 0 ? std::fabs(ndot) : 1.0;
        if (directional && w.normal != 0) {
            D3 u{};
            for (int m = 1; m < 3; m++) {
                const double coeff = -w.normal * (ndot > 0 ? 1 : -1) * dot(s.vec[m], normal) /
                                     (s.ev[0] - s.ev[m]);
                for (int a = 0; a < 3; a++) u[a] += coeff * s.vec[m][a];
            }
            for (size_t m = 0; m < ids.size(); m++) {
                if (radii && (*radii)[ids[m]] <= 0) continue;
                const D3 d = sub(pts[m], s.mean);
                for (int a = 0; a < 3; a++)
                    h.v_means[3 * ids[m] + a] +=
                        (u[a] * dot(s.vec[0], d) + s.vec[0][a] * dot(u, d)) / pts.size();
            }
        }
        for (int e = 0; e < 4; e++) {
            const double hstep = 1e-6;
            double qp[4] = {q[0], q[1], q[2], q[3]}, qm[4] = {q[0], q[1], q[2], q[3]};
            qp[e] += hstep;
            qm[e] -= hstep;
            const double dp = std::fabs(dot(quat_axis(qp, axis), s.vec[0]));
            const double dm = std::fabs(dot(quat_axis(qm, axis), s.vec[0]));
            h.v_quats[4 * self + e] += -(directional ? w.normal : 0.0) * (dp - dm) / (2 * hstep);
        }
        const double big = sa ? std::max({ls[0], ls[1], ls[2]}) : 0.0;
        h.v_scales[3 * self + axis] += w.surfel * std::exp((double)ls[axis] - big);
        h.sums[0] += s.ev[0] * inv_t;
        h.sums[1] += (s.ev[2] - s.ev[1]) * inv_t;
        h.sums[2] += 1 - align;
        h.lambda3 += s.ev[2];
    }
    return h;
}

// Largest |device - host| over the clean entries, relative to the largest |host|.
double rel_err(const std::vector<float>& dev, const std::vector<double>& host,
               const std::vector<char>& dirty, int stride) {
    double num = 0, den = 0;
    for (size_t i = 0; i < host.size(); i++) {
        if (dirty[i / stride]) continue;
        num = std::max(num, std::fabs((double)dev[i] - host[i]));
        den = std::max(den, std::fabs(host[i]));
    }
    return den > 0 ? num / den : num;
}

// Sorted distances from splat `self` to the ids in row r.
std::vector<double> row_d2(const Cloud& c, int64_t self, const int32_t* row, int k) {
    std::vector<double> d;
    for (int m = 0; m < k; m++)
        if (row[m] >= 0) {
            const D3 e = sub(c.at(row[m]), c.at(self));
            d.push_back(dot(e, e));
        }
    std::sort(d.begin(), d.end());
    return d;
}

bool same_d2(const std::vector<double>& a, const std::vector<double>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (std::fabs(a[i] - b[i]) > 1e-6 * std::max(1.0, b[i])) return false;
    return true;
}

void test_gradient_formula() {
    std::mt19937 rng(5);
    std::normal_distribution<double> g(0, 1);
    double worst = 0;
    for (int trial = 0; trial < 20; trial++) {
        std::vector<D3> pts;
        for (int i = 0; i < 12; i++) pts.push_back({2 * g(rng), 0.7 * g(rng), 0.1 * g(rng)});
        const double gamma = 0.3;
        const Spectrum s = spectrum(pts);
        const double gk = 2.0 / (double)pts.size();
        for (size_t j = 0; j < pts.size(); j++)
            for (int a = 0; a < 3; a++) {
                const D3 d = sub(pts[j], s.mean);
                const double an = gk * (dot(s.vec[0], d) * s.vec[0][a] +
                                        gamma * (dot(s.vec[2], d) * s.vec[2][a] -
                                                 dot(s.vec[1], d) * s.vec[1][a]));
                auto pp = pts, pm = pts;
                pp[j][a] += 1e-6;
                pm[j][a] -= 1e-6;
                const double fd = (pca_loss(pp, gamma) - pca_loss(pm, gamma)) / 2e-6;
                worst = std::max(worst, std::fabs(an - fd) / std::max(std::fabs(fd), 1e-3));
            }
    }
    check(worst < 1e-5, "d(lambda_1 + gamma (lambda_3 - lambda_2))/d mu matches finite "
                        "differences (worst " + std::to_string(worst) + ")");
}

void test_cloud(bool messy, int k) {
    const Cloud c = make_cloud(messy ? 6000 : 4000, messy, messy ? 11u : 3u);
    const int64_t n = c.n();
    const std::string tag = std::string(messy ? "messy" : "clean") + " n=" + std::to_string(n) +
                            " k=" + std::to_string(k) + ": ";
    DeviceCloud d(c);
    pool_begin_phase(PoolPhase::RasterBwd);
    Dev<int32_t> perm(n), nbrs(n * k), kept(n);
    Dev<float4> sorted(n);
    Tree tr;
    {
        auto [p, srt, box] = pca_reg_order_tensor(n, d.means.v(), kept.v());
        std::tie(tr.nodes, tr.meta, tr.ranges) = pca_reg_tree_tensor(n, srt, box);
        backend::memcpy_sync(perm.p, p.data_ptr(), n * sizeof(int32_t), MemcpyKind::DeviceToDevice);
        backend::memcpy_sync(sorted.p, srt.data_ptr(), n * sizeof(float4), MemcpyKind::DeviceToDevice);
    }
    backend::device_synchronize();
    check(kept.host() == perm.host(), tag + "the kept copy of the order matches it");
    const auto hp = perm.host();
    {
        std::vector<char> seen(n, 0);
        bool ok = true;
        for (int32_t i : hp) {
            if (i < 0 || i >= n || seen[i]) ok = false;
            else seen[i] = 1;
        }
        check(ok, tag + "the Morton order is a permutation");
        const auto hs = sorted.host();
        bool match = true;
        for (int64_t r = 0; r < n && ok; r++)
            if (hs[r].x != c.means[3 * hp[r]] || hs[r].y != c.means[3 * hp[r] + 1] ||
                hs[r].z != c.means[3 * hp[r] + 2])
                match = false;
        check(match, tag + "sorted[r] is the centre of perm[r]");
        // Consecutive splats in the order are near on average: a broken key
        // would leave them as far apart as random pairs.
        double step = 0, rnd = 0;
        for (int64_t r = 1; r < n; r++) {
            const D3 e = sub(c.at(hp[r]), c.at(hp[r - 1]));
            const D3 f = sub(c.at(hp[r]), c.at(hp[(r * 7919) % n]));
            step += std::sqrt(dot(e, e));
            rnd += std::sqrt(dot(f, f));
        }
        if (!messy)
            check(step < 0.05 * rnd, tag + "Morton neighbours are spatial neighbours (" +
                                         std::to_string(step / rnd) + " of random)");
    }

    const Weights w{1.0f / (float)n, 0.3f / (float)n, 0.5f / (float)n, 0.2f / (float)n};
    const int window = 4 * (k + 1);

    const Run tree = run_mode(d, n, PcaRegMode::Tree, k, window, perm, sorted, tr, nbrs, true, w);
    {
        int bad = 0;
        for (int64_t r = 0; r < n; r++) {
            const int64_t self = hp[r];
            std::vector<double> all;
            for (int64_t j = 0; j < n; j++) {
                if (j == self) continue;
                const D3 e = sub(c.at(j), c.at(self));
                all.push_back(dot(e, e));
            }
            std::partial_sort(all.begin(), all.begin() + k, all.end());
            all.resize(k);
            const int32_t* row = &tree.nbrs[r * k];
            bool self_free = true;
            for (int m = 0; m < k; m++) self_free = self_free && row[m] != self;
            if (!self_free || !same_d2(row_d2(c, self, row, k), all)) bad++;
        }
        check(bad == 0, tag + "tree search = brute-force kNN (" + std::to_string(bad) + " wrong)");
    }
    const Run best = run_mode(d, n, PcaRegMode::Best, k, window, perm, sorted, Tree{}, nbrs, true, w);
    {
        int bad = 0;
        const int64_t half = std::max(window, k + 1) / 2;
        for (int64_t r = 0; r < n; r++) {
            const int64_t self = hp[r];
            std::vector<double> cand;
            for (int64_t x = std::max<int64_t>(0, r - half); x < std::min(n, r + half + 1); x++) {
                if (x == r) continue;
                const D3 e = sub(c.at(hp[x]), c.at(self));
                cand.push_back(dot(e, e));
            }
            std::sort(cand.begin(), cand.end());
            cand.resize(std::min<size_t>(cand.size(), (size_t)k));
            if (!same_d2(row_d2(c, self, &best.nbrs[r * k], k), cand)) bad++;
        }
        check(bad == 0, tag + "best-of-window = nearest k of the window (" + std::to_string(bad) +
                            " wrong)");
    }
    const Run win = run_mode(d, n, PcaRegMode::Window, k, window, perm, sorted, Tree{}, nbrs, true, w);
    {
        int bad = 0;
        for (int64_t r = 0; r < n; r++) {
            int64_t first = r > k / 2 ? r - k / 2 : 0;
            if (first + k + 1 > n) first = n - (k + 1);
            std::vector<int32_t> want;
            for (int64_t x = first; x < first + k + 1; x++)
                if (x != r) want.push_back(hp[x]);
            std::vector<int32_t> got(&win.nbrs[r * k], &win.nbrs[r * k] + k);
            std::sort(want.begin(), want.end());
            std::sort(got.begin(), got.end());
            if (want != got) bad++;
        }
        check(bad == 0, tag + "window = the k + 1 consecutive splats around it (" +
                            std::to_string(bad) + " wrong)");
    }
    // List mode reads back what the tree stored.
    Dev<int32_t> tree_nbrs(n * k, tree.nbrs.data());
    const Run list = run_mode(d, n, PcaRegMode::List, k, window, perm, sorted, Tree{}, tree_nbrs,
                              false, w);

    const std::vector<float> hr = make_radii(n);
    const Dev<float> radii(n, hr.data());
    const Run gated = run_mode(d, n, PcaRegMode::List, k, window, perm, sorted, Tree{}, tree_nbrs,
                               false, w, &radii);
    const Run tree_sa = run_mode(d, n, PcaRegMode::Tree, k, window, perm, sorted, tr, nbrs, false,
                                 w, nullptr, true);
    const Run list_sa = run_mode(d, n, PcaRegMode::List, k, window, perm, sorted, Tree{},
                                 tree_nbrs, false, w, &radii, true);

    const struct {
        const char* name;
        const Run* run;
        const std::vector<int32_t>* lists;
        const std::vector<float>* radii;
        bool sa;
    } cases[] = {
        {"tree", &tree, &tree.nbrs, nullptr, false},
        {"best", &best, &best.nbrs, nullptr, false},
        {"window", &win, &win.nbrs, nullptr, false},
        {"list", &list, &tree.nbrs, nullptr, false},
        {"list, rendered means only", &gated, &tree.nbrs, &hr, false},
        {"tree, scale-agnostic", &tree_sa, &tree.nbrs, nullptr, true},
        {"list, rendered means only, scale-agnostic", &list_sa, &tree.nbrs, &hr, true},
    };
    for (const auto& cs : cases) {
        const HostGrad h = host_grad(c, hp, *cs.lists, k, w, cs.radii, cs.sa);
        int64_t clean = 0;
        for (char x : h.dirty) clean += !x;
        const double em = rel_err(cs.run->v_means, h.v_means, h.dirty, 3);
        const double eq = rel_err(cs.run->v_quats, h.v_quats, h.dirty, 4);
        const double es = rel_err(cs.run->v_scales, h.v_scales, h.dirty, 3);
        // Eigenvalues are good to float rounding of the largest one; the
        // misalignment sum follows v_1, which ill-conditioned neighbourhoods
        // (the messy cloud's floaters) do not pin down.
        double el = 0;
        for (int t = 0; t < (messy ? 2 : 3); t++)
            el = std::max(el, std::fabs(cs.run->sums[t] - h.sums[t]) /
                                  (t < 2 && !cs.sa ? h.lambda3 : (double)n));
        char buf[256];
        std::snprintf(buf, sizeof buf,
                      "%s: gradients match the host (means %.2e over %lld clean, quats %.2e, "
                      "scales %.2e, sums %.2e)",
                      cs.name, em, (long long)clean, eq, es, el);
        check(em < 2e-3 && eq < 1e-3 && es < 1e-5 && el < 1e-5 && clean > n / 2, tag + buf);
    }
    // A later step's gather opens the phase again, which retires the tree.
    {
        const auto hs = sorted.host();
        std::vector<float4> ha((size_t)n);
        pool_begin_phase(PoolPhase::RasterBwd);
        const DeviceVector<float4> again = pca_reg_gather_tensor(n, d.means.v(), perm.v());
        backend::memcpy_sync(ha.data(), again.data_ptr(), n * sizeof(float4), MemcpyKind::DeviceToHost);
        bool same = true;
        for (int64_t r = 0; r < n; r++)
            same = same && ha[r].x == hs[r].x && ha[r].y == hs[r].y && ha[r].z == hs[r].z;
        check(same, tag + "gather by a stored order reproduces it");
    }
}

// The splat ids' neighbour sets, from lists by rank.
std::vector<std::vector<int32_t>> by_splat(const std::vector<int32_t>& perm,
                                           const std::vector<int32_t>& nbrs, int k) {
    std::vector<std::vector<int32_t>> out(perm.size());
    for (size_t r = 0; r < perm.size(); r++) {
        out[perm[r]].assign(&nbrs[r * k], &nbrs[r * k] + k);
        std::sort(out[perm[r]].begin(), out[perm[r]].end());
    }
    return out;
}

// The exact lists of a cloud: its order into `perm`, the tree's lists by rank.
std::vector<int32_t> exact_lists(const DeviceCloud& d, int64_t n, int k, const Weights& w,
                                 Dev<int32_t>& perm, Dev<float4>& sorted) {
    pool_begin_phase(PoolPhase::RasterBwd);
    Dev<int32_t> nbrs(n * k);
    Tree tr;
    auto [p, srt, box] = pca_reg_order_tensor(n, d.means.v(), DeviceVector<int32_t>());
    std::tie(tr.nodes, tr.meta, tr.ranges) = pca_reg_tree_tensor(n, srt, box);
    backend::memcpy_sync(perm.p, p.data_ptr(), n * sizeof(int32_t), MemcpyKind::DeviceToDevice);
    backend::memcpy_sync(sorted.p, srt.data_ptr(), n * sizeof(float4), MemcpyKind::DeviceToDevice);
    return run_mode(d, n, PcaRegMode::Tree, k, 4 * (k + 1), perm, sorted, tr, nbrs, true, w).nbrs;
}

// x -> s R x + t (quaternions turned by r, log s added to log scales): the
// scale-agnostic terms stay and their gradients move with the splats; the
// paper's lambdas grow by s^2 and its surfel term by s.
void test_sim3(int k) {
    const Cloud c = make_cloud(4000, false, 7u);
    const int64_t n = c.n();
    const std::string tag = "similarity k=" + std::to_string(k) + ": ";
    double r[4] = {0.8, 0.2, -0.5, 0.26};
    const double rn = std::sqrt(r[0] * r[0] + r[1] * r[1] + r[2] * r[2] + r[3] * r[3]);
    for (double& x : r) x /= rn;
    const double s = 7.3;
    const D3 t = {12.0, -40.0, 3.5};
    D3 R[3];  // rows of the rotation of r
    for (int a = 0; a < 3; a++) {
        const D3 col = quat_axis(r, a);
        for (int b = 0; b < 3; b++) R[b][a] = col[b];
    }
    // r * q as L q, L orthogonal: the quaternion gradient moves by L as well.
    const double L[4][4] = {{r[0], -r[1], -r[2], -r[3]}, {r[1], r[0], -r[3], r[2]},
                            {r[2], r[3], r[0], -r[1]}, {r[3], -r[2], r[1], r[0]}};
    Cloud x = c;
    for (int64_t i = 0; i < n; i++) {
        const D3 p = c.at(i);
        for (int b = 0; b < 3; b++) x.means[3 * i + b] = (float)(s * dot(R[b], p) + t[b]);
        for (int b = 0; b < 4; b++) {
            double v = 0;
            for (int e = 0; e < 4; e++) v += L[b][e] * c.quats[4 * i + e];
            x.quats[4 * i + b] = (float)v;
        }
        for (int b = 0; b < 3; b++) x.scales[3 * i + b] = c.scales[3 * i + b] + (float)std::log(s);
    }
    const DeviceCloud dc(c), dx(x);
    const Weights w{1.0f / (float)n, 0.3f / (float)n, 0.5f / (float)n, 0.2f / (float)n};
    Dev<int32_t> perm(n), perm_x(n);
    Dev<float4> sorted(n), sorted_x(n);
    const std::vector<int32_t> lists = exact_lists(dc, n, k, w, perm, sorted);
    const std::vector<int32_t> lists_x = exact_lists(dx, n, k, w, perm_x, sorted_x);
    const std::vector<int32_t> hp = perm.host();
    {
        const auto a = by_splat(hp, lists, k), b = by_splat(perm_x.host(), lists_x, k);
        int64_t differ = 0;
        for (int64_t i = 0; i < n; i++) differ += a[i] != b[i];
        check(differ <= n / 1000, tag + "the exact neighbours move with the splats (" +
                                      std::to_string(differ) + " differ)");
    }
    // The same lists on both clouds, so only the terms are compared.
    Dev<int32_t> fixed(n * k, lists.data());
    const std::vector<char> dirty = host_grad(c, hp, lists, k, w).dirty;
    const Weights normal_w{0, 0, w.normal, 0};
    const Run normal = run_mode(dc, n, PcaRegMode::List, k, 0, perm, sorted, Tree{}, fixed, false,
                               normal_w);
    for (bool sa : {true, false}) {
        const Run a = run_mode(dc, n, PcaRegMode::List, k, 0, perm, sorted, Tree{}, fixed, false, w,
                               nullptr, sa);
        const Run b = run_mode(dx, n, PcaRegMode::List, k, 0, perm, sorted, Tree{}, fixed, false, w,
                               nullptr, sa);
        const double gl = sa ? 1.0 : s * s, gm = sa ? 1.0 / s : s, gs = sa ? 1.0 : s;
        std::vector<double> want_m(3 * n), want_q(4 * n), want_s(3 * n);
        for (int64_t i = 0; i < n; i++) {
            const D3 v = {a.v_means[3 * i], a.v_means[3 * i + 1], a.v_means[3 * i + 2]};
            const D3 vn = {normal.v_means[3*i], normal.v_means[3*i+1], normal.v_means[3*i+2]};
            for (int e = 0; e < 3; e++)
                want_m[3 * i + e] = gm * dot(R[e], sub(v, vn)) + dot(R[e], vn) / s;
            for (int e = 0; e < 4; e++) {
                want_q[4 * i + e] = 0;
                for (int f = 0; f < 4; f++) want_q[4 * i + e] += L[e][f] * a.v_quats[4 * i + f];
            }
            for (int e = 0; e < 3; e++) want_s[3 * i + e] = gs * a.v_scales[3 * i + e];
        }
        double el = 0;
        for (int e = 0; e < 3; e++)
            el = std::max(el, std::fabs(b.sums[e] - (e < 2 ? gl : 1.0) * a.sums[e]) /
                                  std::fabs((e < 2 ? gl : 1.0) * a.sums[e]));
        const double em = rel_err(b.v_means, want_m, dirty, 3);
        const double eq = rel_err(b.v_quats, want_q, dirty, 4);
        const double es = rel_err(b.v_scales, want_s, dirty, 3);
        char buf[256];
        std::snprintf(buf, sizeof buf,
                      "%s: sums x %s, mean gradients x %s R, quats x L, scales x %s (sums %.2e, "
                      "means %.2e, quats %.2e, scales %.2e)",
                      sa ? "scale-agnostic" : "paper's terms", sa ? "1" : "s^2", sa ? "1/s" : "s",
                      sa ? "1" : "s", el, em, eq, es);
        check(el < 1e-4 && em < 2e-3 && eq < 1e-3 && es < 1e-4, tag + buf);
    }
}

struct MathCloud {
    std::vector<double> means, quats, scales, opacities;
    explicit MathCloud(const Cloud& c)
        : means(c.means.begin(), c.means.end()), quats(c.quats.begin(), c.quats.end()),
          scales(c.scales.begin(), c.scales.end()), opacities(c.n(), -0.4) {}
};

struct MathLoss {
    std::array<double, 6> value{};
    double trace = 0;
    std::vector<double> max_scale;
};

MathLoss math_loss(const MathCloud& c, int covariance, bool sa, const MathLoss* frozen = nullptr) {
    const int n = (int)c.opacities.size();
    MathLoss out;
    std::vector<double> weights(n);
    D3 mean{};
    double total = 0;
    for (int j = 0; j < n; j++) {
        weights[j] = covariance == 0 ? 1 : 1 / (1 + std::exp(-c.opacities[j]));
        total += weights[j];
        for (int a = 0; a < 3; a++) mean[a] += weights[j] * c.means[3 * j + a];
    }
    for (double& x : mean) x /= total;
    double C[3][3] = {};
    for (int j = 0; j < n; j++) {
        D3 d{c.means[3 * j] - mean[0], c.means[3 * j + 1] - mean[1], c.means[3 * j + 2] - mean[2]};
        for (int a = 0; a < 3; a++)
            for (int b = 0; b < 3; b++) C[a][b] += weights[j] / total * d[a] * d[b];
        if (covariance == 2) {
            for (int axis = 0; axis < 3; axis++) {
                const auto v = quat_axis(&c.quats[4 * j], axis);
                const double s2 = std::exp(2 * c.scales[3 * j + axis]);
                for (int a = 0; a < 3; a++)
                    for (int b = 0; b < 3; b++) C[a][b] += weights[j] / total * s2 * v[a] * v[b];
            }
        }
    }
    double ev[3]; D3 vec[3];
    eigen3(C, ev, vec);
    out.trace = ev[0] + ev[1] + ev[2];
    const double inv = sa ? 1 / (frozen ? frozen->trace : out.trace) : 1;
    out.value[0] = ev[0] * inv;
    out.value[1] = (ev[2] - ev[1]) * inv;
    double entropy = 0;
    for (double l : ev) {
        const double p = (std::max(l, 0.0) / out.trace + 1e-8) / (1 + 3e-8);
        entropy -= p * std::log(p);
    }
    out.value[4] = std::pow(std::max(std::exp(entropy) - 2, 0.0), 2);
    for (int i = 0; i < n; i++) {
        int axis = 0;
        for (int a = 1; a < 3; a++) if (c.scales[3 * i + a] < c.scales[3 * i + axis]) axis = a;
        out.value[2] += (1 - std::fabs(dot(quat_axis(&c.quats[4 * i], axis), vec[0]))) / n;
        out.max_scale.push_back(std::max({c.scales[3 * i], c.scales[3 * i + 1], c.scales[3 * i + 2]}));
        const double max_scale = sa ? (frozen ? frozen->max_scale[i] : out.max_scale[i]) : 0;
        out.value[3] += std::exp(c.scales[3 * i + axis] - max_scale) / n;
        double transmittance = 1;
        for (int j = 0; j < n; j++) {
            D3 d{c.means[3 * j] - c.means[3 * i], c.means[3 * j + 1] - c.means[3 * i + 1],
                 c.means[3 * j + 2] - c.means[3 * i + 2]}, u{}, v{};
            for (int a = 0; a < 3; a++) {
                const auto ax = quat_axis(&c.quats[4 * j], a);
                u[a] = dot(ax, d) * std::exp(-c.scales[3 * j + a]);
                v[a] = dot(ax, vec[0]) * std::exp(-c.scales[3 * j + a]);
            }
            const double t = dot(u, v) / dot(v, v);
            for (int a = 0; a < 3; a++) u[a] -= t * v[a];
            const double alpha = std::min(std::exp(-0.5 * dot(u, u)) / (1 + std::exp(-c.opacities[j])), 0.999);
            transmittance *= 1 - alpha;
        }
        out.value[5] += transmittance / n;
    }
    return out;
}

double objective(const MathLoss& l, const std::array<double, 6>& w) {
    double s = 0;
    for (int a = 0; a < 6; a++) s += w[a] * l.value[a];
    return s;
}

void test_covariance_losses() {
    Cloud c;
    c.means = {0,0,0, 1.2f,0,0, -1.2f,0,0, 0,1,0, 0,-1,0, 0,0,0.8f, 0,0,-0.8f};
    for (int i = 0; i < 7; i++) {
        c.quats.insert(c.quats.end(), {0.9f, 0.1f + 0.03f * i, -0.2f, 0.15f});
        c.scales.insert(c.scales.end(), {-0.3f, -0.5f, -0.9f - 0.02f * i});
    }
    const int n = 7, k = n - 1;
    DeviceCloud d(c);
    std::vector<int32_t> ids(n), list(n * k);
    for (int i = 0; i < n; i++) {
        ids[i] = i;
        for (int j = 0, at = 0; j < n; j++) if (j != i) list[i * k + at++] = j;
    }
    Dev<int32_t> perm(n, ids.data()), nbrs(n * k, list.data());
    std::vector<float> alpha{-0.4f, 0.0f, 0.3f, -1.0f, 1.2f, -0.7f, 0.8f};
    Dev<float> opacities(n, alpha.data()), vo(n), sums(5);
    Dev<float3> vm(n), vs(n);
    Dev<float4> vq(n);
    const std::array<double, 6> weights[] = {
        {0,0,0.3,0,0,0}, {0,0,0,0,0.7,0}, {0,0,0,0,0,0.4}, {0.2,0.001,0.3,0.05,0.7,0.4}};
    MathCloud cpu(c);
    cpu.opacities.assign(alpha.begin(), alpha.end());
    for (int cov = 0; cov < 3; cov++) for (bool sa : {false, true}) for (int which = 0; which < 4; which++) {
        const auto& w = weights[which];
        vm.zero(); vq.zero(); vs.zero(); vo.zero(); sums.zero();
        pca_reg_neighborhood_tensor(n, (int)PcaRegMode::List, k, 0, d.means.v(), d.quats.v(), d.scales.v(),
            opacities.v(), cov, perm.v(), {}, {}, {}, {}, nbrs.v(), false, {}, false, sa,
            vm.v(), vq.v(), vs.v(), vo.v(), w[0]/n, w[1]/n, w[2]/n, w[3]/n, w[4]/n, w[5]/n, sums.v());
        backend::device_synchronize();
        const auto m = vm.host(); const auto q = vq.host(); const auto s = vs.host(); const auto a = vo.host();
        const std::vector<float> got[] = {
            std::vector<float>((float*)m.data(), (float*)m.data() + 3*n),
            std::vector<float>((float*)q.data(), (float*)q.data() + 4*n),
            std::vector<float>((float*)s.data(), (float*)s.data() + 3*n), a};
        const MathLoss base = math_loss(cpu, cov, sa);
        const auto loss = sums.host();
        const double expected[] = {base.value[0], base.value[1], w[2] ? base.value[2] : 0,
                                   w[4] ? base.value[4] : 0, w[5] ? base.value[5] : 0};
        bool values = true;
        for (int j = 0; j < 5; j++) values &= std::fabs(loss[j]/n - expected[j]) < 3e-5;
        check(values, "cov=" + std::to_string(cov) + " sa=" + std::to_string(sa) +
              " loss=" + std::to_string(which) + " forward matches double reference");
        std::vector<double>* channels[] = {&cpu.means, &cpu.quats, &cpu.scales, &cpu.opacities};
        for (int channel = 0; channel < 4; channel++) {
            double worst = 0, norm = 0;
            auto& v = *channels[channel];
            for (size_t j = 0; j < v.size(); j++) {
                const double prev = v[j], h = 1e-5;
                v[j] = prev + h; const double hi = objective(math_loss(cpu, cov, sa, &base), w);
                v[j] = prev - h; const double lo = objective(math_loss(cpu, cov, sa, &base), w);
                v[j] = prev;
                const double fd = (hi - lo) / (2*h);
                worst = std::max(worst, std::fabs(got[channel][j] - fd));
                norm = std::max(norm, std::fabs(fd));
            }
            check(worst < 2e-5 + 2e-3 * norm, "cov=" + std::to_string(cov) + " sa=" + std::to_string(sa) +
                  " loss=" + std::to_string(which) + " channel=" + std::to_string(channel) +
                  " finite difference error=" + std::to_string(worst));
        }
    }
}

void test_covariance_limits() {
    constexpr int n = 7, k = 6;
    Cloud c;
    c.means = {0,0,0, 1.2f,0,0, -1.2f,0,0, 0,1,0, 0,-1,0, 0,0,0.8f, 0,0,-0.8f};
    for (int i = 0; i < n; i++) {
        c.quats.insert(c.quats.end(), {1,0,0,0});
        c.scales.insert(c.scales.end(), {-0.3f,-0.5f,-0.9f});
    }
    std::vector<int32_t> ids(n), list(n*k);
    for (int i = 0; i < n; i++) {
        ids[i] = i;
        for (int j = 0, at = 0; j < n; j++) if (j != i) list[i*k + at++] = j;
    }
    Dev<int32_t> perm(n, ids.data()), nbrs(n*k, list.data());
    for (int shape = 0; shape < 4; shape++) {
        Cloud cloud = c;
        if (shape == 0) std::fill(cloud.means.begin(), cloud.means.end(), 0);
        if (shape == 1) for (int i = 0; i < n; i++) cloud.means[3*i+2] = 0;
        std::vector<float> logits(n, shape == 2 ? -100.0f : 100.0f);
        DeviceCloud d(cloud);
        Dev<float> opacity(n, logits.data()), vo(n), sums(5);
        Dev<float3> vm(n), vs(n); Dev<float4> vq(n);
        pca_reg_neighborhood_tensor(n, (int)PcaRegMode::List, k, 0, d.means.v(), d.quats.v(),
            d.scales.v(), opacity.v(), shape == 3 ? 2 : 0, perm.v(), {}, {}, {}, {}, nbrs.v(),
            false, {}, false, true, vm.v(), vq.v(), vs.v(), vo.v(), 0, 0, 0.3f/n, 0, 0.7f/n, 0.4f/n, sums.v());
        backend::device_synchronize();
        const auto m = vm.host(); const auto q = vq.host(); const auto s = vs.host();
        const auto a = vo.host(); const auto l = sums.host();
        bool finite = true;
        auto scan = [&](const float* ptr, size_t count) {
            for (size_t i = 0; i < count; i++) finite &= std::isfinite(ptr[i]);
        };
        scan((const float*)m.data(), 3*n); scan((const float*)q.data(), 4*n);
        scan((const float*)s.data(), 3*n); scan(a.data(), n); scan(l.data(), 5);
        check(finite, "degenerate/saturated covariance case " + std::to_string(shape) + " is finite");
        if (shape < 2) check(l[3] < 1e-10, "effective rank hinge is inactive on rank <= 2");
        if (shape == 0) check(l[2] == 0 && l[4] == 0, "undefined PCA normal disables directional terms");
        vm.zero(); sums.zero();
        pca_reg_neighborhood_tensor(n, (int)PcaRegMode::List, k, 0, d.means.v(), d.quats.v(),
            d.scales.v(), {}, 0, perm.v(), {}, {}, {}, {}, nbrs.v(), false, {}, false, false,
            vm.v(), {}, {}, {}, 1.0f/n, 0, 0, 0, 0, 0, sums.v());
        backend::device_synchronize();
        check(backend::last_error() == nullptr, "disabled losses allow absent gradient/opacity buffers");
    }
}

// Best's one-workgroup path against the neighbour-at-a-time path replaying the
// lists it chose: every covariance, scale mode, gate and gradient channel.
void test_slab_matches_list(bool messy, int k) {
    const Cloud c = make_cloud(1200, messy, 101 + (uint32_t)k);
    const int64_t n = c.n();
    const std::string tag = std::string(messy ? "messy" : "clean") + " slab k=" +
                            std::to_string(k) + ": ";
    DeviceCloud d(c);
    std::mt19937 rng((uint32_t)k);
    std::normal_distribution<float> g(0.f, 1.5f);
    std::vector<float> ha((size_t)n);
    for (float& a : ha) a = g(rng);
    const Dev<float> opacities(n, ha.data());
    const std::vector<float> hr = make_radii(n);
    const Dev<float> radii(n, hr.data());
    Dev<int32_t> perm(n);
    Dev<float4> sorted(n);
    {
        pool_begin_phase(PoolPhase::RasterBwd);
        auto [p, srt, box] = pca_reg_order_tensor(n, d.means.v(), DeviceVector<int32_t>());
        backend::memcpy_sync(perm.p, p.data_ptr(), n * sizeof(int32_t), MemcpyKind::DeviceToDevice);
        backend::memcpy_sync(sorted.p, srt.data_ptr(), n * sizeof(float4), MemcpyKind::DeviceToDevice);
    }
    Dev<int32_t> nbrs(n * k);
    const int window = 4 * (k + 1);
    check(pca_reg_uses_slab((int)PcaRegMode::Best, k, window, 0.0f), tag + "best takes the slab");
    const float w = 1.0f / (float)n;
    for (int cov = 0; cov < 3; cov++) for (bool sa : {false, true}) for (bool gate : {false, true}) {
        auto go = [&](PcaRegMode mode, bool store) {
            Dev<float3> vm(n), vs(n);
            Dev<float4> vq(n);
            Dev<float> vo(n), sums(5);
            pca_reg_neighborhood_tensor(n, (int)mode, k, window, d.means.v(), d.quats.v(),
                d.scales.v(), opacities.v(), cov, perm.v(), sorted.v(), {}, {}, {}, nbrs.v(), store,
                gate ? radii.v() : DeviceVector<float>(), gate, sa, vm.v(), vq.v(), vs.v(), vo.v(),
                w, 0.001f * w, 0.1f * w, 0.2f * w, 0.03f * w, 0.0f, sums.v());
            backend::device_synchronize();
            const auto m = vm.host();
            const auto q = vq.host();
            const auto s = vs.host();
            return std::array<std::vector<float>, 5>{
                std::vector<float>((const float*)m.data(), (const float*)m.data() + 3 * n),
                std::vector<float>((const float*)q.data(), (const float*)q.data() + 4 * n),
                std::vector<float>((const float*)s.data(), (const float*)s.data() + 3 * n),
                vo.host(), sums.host()};
        };
        const auto slab = go(PcaRegMode::Best, true);
        const auto list = go(PcaRegMode::List, false);
        const char* const names[] = {"means", "quats", "scales", "opacities", "sums"};
        for (int ch = 0; ch < 5; ch++) {
            double worst = 0, norm = 0;
            for (size_t i = 0; i < list[ch].size(); i++) {
                worst = std::max(worst, (double)std::fabs(slab[ch][i] - list[ch][i]));
                norm = std::max(norm, (double)std::fabs(list[ch][i]));
            }
            check(worst <= 2e-3 * norm + 1e-30,
                  tag + "cov=" + std::to_string(cov) + " sa=" + std::to_string(sa) + " gate=" +
                      std::to_string(gate) + " " + names[ch] + " match the list path (" +
                      std::to_string(worst / std::max(norm, 1e-300)) + " of max)");
        }
    }
}

}  // namespace

int main() {
    test_covariance_limits();
    test_covariance_losses();
    test_gradient_formula();
    test_cloud(false, 8);
    test_cloud(false, 31);
    test_cloud(true, 15);
    test_sim3(16);
    test_slab_matches_list(false, 31);
    test_slab_matches_list(true, 31);
    test_slab_matches_list(false, 8);
    test_slab_matches_list(true, 16);
    std::printf("%s\n", g_failures ? "FAILED" : "PASSED");
    return g_failures ? 1 : 0;
}

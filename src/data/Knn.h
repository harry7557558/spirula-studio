#pragma once

// Knn.h -- exact k-nearest-neighbor queries over 3D points via a kd-tree,
// with multi-threaded queries. Replaces the earlier uniform-hash-grid
// approximation, whose cell size was derived from the bounding-box volume:
// a single distant SfM outlier inflated the box, collapsed all points into
// one cell, and degenerated the scan to O(N^2). The kd-tree median split is
// insensitive to outliers and exact.
//
// Self-contained, no dependencies beyond the standard library.

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <future>
#include <thread>
#include <vector>

namespace knn {

// Kd-tree over an externally owned [n,3] float array (not copied; must
// outlive the tree). Implicit layout: the node for index range [lo,hi) is
// the median element at mid=(lo+hi)/2 after nth_element along the range's
// widest bounding-box axis; children are [lo,mid) and [mid+1,hi).
class KdTree3 {
public:
    KdTree3(const float* pts, int64_t n) : _pts(pts), _n(n) {
        _idx.resize(n);
        for (int64_t i = 0; i < n; i++) _idx[i] = (int32_t)i;
        _axis.assign(n, 0);
        build(0, n, 0);
    }

    // Squared distances to the (up to) k nearest neighbors of q, excluding
    // index `self` (-1 to disable), ascending into d2_out (capacity >= k) and
    // their indices into idx_out when given; returns the count found.
    int query(const float q[3], int32_t self, int k, float* d2_out,
              int32_t* idx_out = nullptr) const {
        KBest best{d2_out, idx_out, k, 0};
        search(0, _n, q, self, best);
        return best.count;
    }

private:
    const float* _pts;
    int64_t _n;
    std::vector<int32_t> _idx;
    std::vector<uint8_t> _axis;

    const float* pt(int64_t node) const {
        return _pts + (int64_t)_idx[node] * 3;
    }

    void build(int64_t lo, int64_t hi, int depth) {
        if (hi - lo <= 1) return;
        // Split along the widest extent of this range's bounding box --
        // stays balanced regardless of how outliers stretch the cloud.
        float bmin[3] = {1e30f, 1e30f, 1e30f};
        float bmax[3] = {-1e30f, -1e30f, -1e30f};
        for (int64_t i = lo; i < hi; i++) {
            const float* p = pt(i);
            for (int d = 0; d < 3; d++) {
                bmin[d] = std::min(bmin[d], p[d]);
                bmax[d] = std::max(bmax[d], p[d]);
            }
        }
        int ax = 0;
        for (int d = 1; d < 3; d++)
            if (bmax[d] - bmin[d] > bmax[ax] - bmin[ax]) ax = d;

        int64_t mid = lo + (hi - lo) / 2;
        std::nth_element(
            _idx.begin() + lo, _idx.begin() + mid, _idx.begin() + hi,
            [this, ax](int32_t a, int32_t b) {
                return _pts[(int64_t)a*3 + ax] < _pts[(int64_t)b*3 + ax];
            });
        _axis[mid] = (uint8_t)ax;

        if (depth < 4 && hi - lo > 65536) {
            auto left = std::async(std::launch::async,
                [this, lo, mid, depth] { build(lo, mid, depth + 1); });
            build(mid + 1, hi, depth + 1);
            left.get();
        } else {
            build(lo, mid, depth + 1);
            build(mid + 1, hi, depth + 1);
        }
    }

    // Fixed-capacity sorted list of the k smallest d^2 (k is tiny here).
    struct KBest {
        float* d2;
        int32_t* idx;
        int k, count;
        float worst() const { return count < k ? 1e30f : d2[k - 1]; }
        void push(float v, int32_t id) {
            int i = count < k ? count++ : k - 1;
            while (i > 0 && v < d2[i - 1]) {
                d2[i] = d2[i - 1];
                if (idx) idx[i] = idx[i - 1];
                i--;
            }
            d2[i] = v;
            if (idx) idx[i] = id;
        }
    };

    void search(int64_t lo, int64_t hi, const float q[3],
                int32_t self, KBest& best) const {
        if (hi <= lo) return;
        int64_t mid = lo + (hi - lo) / 2;
        const float* p = pt(mid);
        if (_idx[mid] != self) {
            float dx = q[0]-p[0], dy = q[1]-p[1], dz = q[2]-p[2];
            float d2 = dx*dx + dy*dy + dz*dz;
            if (d2 < best.worst()) best.push(d2, _idx[mid]);
        }
        if (hi - lo == 1) return;
        int ax = _axis[mid];
        float delta = q[ax] - p[ax];
        int64_t nlo[2] = {lo, mid + 1}, nhi[2] = {mid, hi};
        int side = delta < 0.f ? 0 : 1;
        search(nlo[side], nhi[side], q, self, best);
        if (delta * delta < best.worst())
            search(nlo[1 - side], nhi[1 - side], q, self, best);
    }
};

// sqrt(mean of squared distances) to the k nearest neighbors of each point
// (self excluded by index; exact duplicates in the cloud still count as
// zero-distance neighbors). Queries run across all hardware threads.
inline std::vector<float> mean_knn_dist(
        const std::vector<float>& xyz, int64_t n, int k) {
    std::vector<float> out(n, 1e-2f);
    if (n <= 1) return out;
    const int kk = (int)std::min<int64_t>(k, n - 1);

    KdTree3 tree(xyz.data(), n);

    int n_threads = (int)std::max(1u, std::thread::hardware_concurrency());
    n_threads = (int)std::min<int64_t>(n_threads, n);
    std::atomic<int64_t> next(0);
    const int64_t CHUNK = 1024;
    auto worker = [&] {
        std::vector<float> d2(kk);
        for (;;) {
            int64_t start = next.fetch_add(CHUNK);
            if (start >= n) return;
            int64_t end = std::min(start + CHUNK, n);
            for (int64_t i = start; i < end; i++) {
                int cnt = tree.query(&xyz[i*3], (int32_t)i, kk, d2.data());
                if (cnt <= 0) continue;
                double acc = 0.0;
                for (int j = 0; j < cnt; j++) acc += d2[j];
                out[i] = (float)std::sqrt(acc / cnt);
            }
        }
    };
    std::vector<std::thread> threads;
    for (int t = 1; t < n_threads; t++) threads.emplace_back(worker);
    worker();
    for (auto& t : threads) t.join();
    return out;
}

}  // namespace knn

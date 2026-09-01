// DataManager — see DataManager.h for the public contract.

#include "data/DataManager.h"
#include "core/ExrImage.h"
#include "data/ImageProbe.h"
#include "i18n/catalog/Data.h"

#include "external/stb_image.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <tuple>
#include <type_traits>
#include <future>
#include <numeric>
#include <random>
#include <stdexcept>
#include <set>
#include <unordered_map>


namespace dmsg = spirula::i18n::msg::data;
using spirula::i18n::format;

// ===========================================================================
// Small utilities
// ===========================================================================
namespace {

// Why a decode failed, for a message a user reads. stbi_failure_reason() is
// a global every decode thread writes, so a missing file -- the common case,
// a dataset moved mid-run -- is diagnosed from the filesystem instead.
std::string decode_failure(const std::string& path) {
    std::error_code ec;
    return format(std::filesystem::exists(path, ec) ? dmsg::file_unreadable
                                                    : dmsg::file_gone,
                  {path});
}

// File suffix (lowercased extension). Returns "" if none.
std::string lower_suffix(const std::string& path) {
    auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return {};
    std::string s = path.substr(dot);
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// Bounded MPMC queue used by the disk-mode prefetch pipeline. One mutex
// guards both ends — this is for low-rate batch coordination (jobs/sec, not
// allocations/sec), so the contention cost is negligible.
template<typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(size_t cap) : _cap(cap) {}

    // Push, blocking while full. Returns false if closed before space is
    // available.
    bool push(T v) {
        std::unique_lock<std::mutex> lk(_m);
        _cv_not_full.wait(lk, [&]{ return _closed || _q.size() < _cap; });
        if (_closed) return false;
        _q.push_back(std::move(v));
        _cv_not_empty.notify_one();
        return true;
    }

    // Pop, blocking while empty. Returns false if closed and drained.
    bool pop(T& out) {
        std::unique_lock<std::mutex> lk(_m);
        _cv_not_empty.wait(lk, [&]{ return _closed || !_q.empty(); });
        if (_q.empty()) return false;  // closed-and-drained
        out = std::move(_q.front());
        _q.pop_front();
        _cv_not_full.notify_one();
        return true;
    }

    enum class Pop { Ok, Timeout, Closed };

    // pop() with a deadline, so a consumer can check on the decode workers
    // instead of blocking forever behind one that is parked on a file error.
    Pop pop_for(T& out, std::chrono::milliseconds wait) {
        std::unique_lock<std::mutex> lk(_m);
        if (!_cv_not_empty.wait_for(lk, wait, [&]{ return _closed || !_q.empty(); }))
            return Pop::Timeout;
        if (_q.empty()) return Pop::Closed;
        out = std::move(_q.front());
        _q.pop_front();
        _cv_not_full.notify_one();
        return Pop::Ok;
    }

    // Wake everyone; subsequent pop()s drain remaining items then return false.
    void close() {
        {
            std::lock_guard<std::mutex> lk(_m);
            _closed = true;
        }
        _cv_not_empty.notify_all();
        _cv_not_full.notify_all();
    }

private:
    size_t                  _cap;
    std::deque<T>           _q;
    std::mutex              _m;
    std::condition_variable _cv_not_empty;
    std::condition_variable _cv_not_full;
    bool                    _closed = false;
};

// One element of a per-image grouping. Each group is homogeneous in (W,H)
// for RGB and uniform per modality (mask / depth / normal), so a batch
// sampled from a single group has the contiguous shape the engine expects.
//
// Per-modality dims are per-GROUP rather than dataset-wide because a single
// dataset may legitimately contain images of different resolutions (and
// hence different mask / depth / normal resolutions); only within a batch
// (= within a group) do the modality buffers need to be uniform.
struct IndexGroup {
    int32_t              width  = 0;
    int32_t              height = 0;
    CameraModelType      model  = (CameraModelType)-1;  // uniform within a group
    CameraDistortionType distortion = CameraDistortionType::None;  // likewise
    // Whether this group's images must be resampled from a fitted source
    // camera. Part of the group key: the kernels take it as a compile-time
    // axis, so a group that mixes fitted and exact cameras would project the
    // exact ones through source model -1 and blank them.
    bool                 redistort = false;
    int32_t              mask_h   = 0, mask_w   = 0;
    int32_t              depth_h  = 0, depth_w  = 0;
    int32_t              normal_h = 0, normal_w = 0;

    // Warp metadata, part of the group key: the split factor, the first
    // pass's resolution (= the input's when K == 1) and the runs of equal
    // face size the faces fall into.
    int32_t              K        = 1;
    int32_t              out_h    = 0;
    int32_t              out_w    = 0;
    std::vector<WarpFacePass> passes;

    std::vector<int32_t> indices;       // dataset-global indices, shuffled
    size_t               cursor = 0;    // next index to emit

    // Refill from `indices` end-of-list. Reshuffles for non-eval datasets.
    void rewind(std::mt19937_64& rng, bool eval) {
        if (!eval) std::shuffle(indices.begin(), indices.end(), rng);
        cursor = 0;
    }
};

// Group-size-weighted sampler — picks a group with probability proportional
// to its size.
class GroupSampler {
public:
    GroupSampler() = default;
    explicit GroupSampler(const std::vector<IndexGroup>& groups) {
        _weights.reserve(groups.size());
        for (auto& g : groups) _weights.push_back((double)g.indices.size());
        _dist = std::discrete_distribution<size_t>(_weights.begin(), _weights.end());
    }
    size_t operator()(std::mt19937_64& rng) { return _dist(rng); }
    bool   empty() const { return _weights.empty(); }
private:
    std::vector<double>                _weights;
    std::discrete_distribution<size_t> _dist;
};

// ---------------------------------------------------------------------------
// Deterministic per-epoch training schedule
// ---------------------------------------------------------------------------
// One homogeneous sub-batch of a training step: a group index plus the
// dataset-global image indices drawn from that group.
struct SubBatchSpec {
    int32_t              group = 0;   // index into the train IndexGroup vector
    std::vector<int32_t> picks;       // dataset-global image indices
};
// One optimizer step = one or more homogeneous sub-batches. Most steps hold a
// single sub-batch; cross-group remainder packing produces multi-sub-batch
// (heterogeneous) steps. See TrainStep in DataManager.h.
using StepSpec = std::vector<SubBatchSpec>;

// ===========================================================================
// Image decoders
// ===========================================================================
//
// All decoders write directly into a pre-allocated batch slot `dst` (pointer
// to the start of row 0 of this image's slot in the [B,H,W,C] buffer).
//
// `expected_h`, `expected_w` are the IndexGroup's promised dimensions. If the
// decoded image disagrees, the decoder warns (once per IndexGroup, keyed on
// the expected (W,H) pair) and bilinearly resizes the image into the slot,
// matching how mask / depth / normal already handle intra-group shape
// drift. Matches the gsplat / nerfstudio convention of accepting off-by-one
// downscale dims (e.g. Mip-NeRF 360 images_(2|4) round vs. floor).

// ---- CPU resize helpers ---------------------------------------------------
//
// Used by the decoders and the CPU-cache copy path. Mismatched intra-group
// modality shapes are resolved by upsampling smaller files to the group's
// chosen (largest) shape:
//   mask   -> nearest (boolean; bilinear would round badly at edges)
//   depth  -> bilinear (smooth scalar field)
//   normal -> bilinear per channel
// All resizers use half-pixel-center sampling, matching the device-side
// bilinear sampler in Interpolation.cuh.

template<typename T, int C>
inline void cpu_bilinear_resize(const T* src, int sh, int sw,
                                T* dst, int dh, int dw)
{
    const float sx = (float)sw / (float)dw;
    const float sy = (float)sh / (float)dh;
    for (int y = 0; y < dh; ++y) {
        float v  = ((float)y + 0.5f) * sy - 0.5f;
        int   y0 = (int)std::floor(v);
        float fy = v - (float)y0;
        int   y0c = std::max(0, std::min(sh - 1, y0));
        int   y1c = std::max(0, std::min(sh - 1, y0 + 1));
        for (int x = 0; x < dw; ++x) {
            float u  = ((float)x + 0.5f) * sx - 0.5f;
            int   x0 = (int)std::floor(u);
            float fx = u - (float)x0;
            int   x0c = std::max(0, std::min(sw - 1, x0));
            int   x1c = std::max(0, std::min(sw - 1, x0 + 1));
            float w00 = (1.0f - fx) * (1.0f - fy);
            float w10 = fx          * (1.0f - fy);
            float w01 = (1.0f - fx) * fy;
            float w11 = fx          * fy;
            const T* p00 = src + ((size_t)y0c * sw + x0c) * C;
            const T* p10 = src + ((size_t)y0c * sw + x1c) * C;
            const T* p01 = src + ((size_t)y1c * sw + x0c) * C;
            const T* p11 = src + ((size_t)y1c * sw + x1c) * C;
            T*       q   = dst + ((size_t)y   * dw + x  ) * C;
            for (int c = 0; c < C; ++c) {
                float r = w00 * (float)p00[c] + w10 * (float)p10[c]
                        + w01 * (float)p01[c] + w11 * (float)p11[c];
                if constexpr (std::is_integral_v<T>) {
                    r = std::round(r);
                    if (r < 0.0f) r = 0.0f;
                    float hi = (float)std::numeric_limits<T>::max();
                    if (r > hi) r = hi;
                }
                q[c] = (T)r;
            }
        }
    }
}

// Exact box average over the source rectangle each destination pixel covers.
template<typename T, int C>
inline void cpu_area_resize(const T* src, int sh, int sw,
                            T* dst, int dh, int dw)
{
    const double sy = (double)sh / dh, sx = (double)sw / dw;
    for (int y = 0; y < dh; ++y) {
        const double v0 = y * sy, v1 = (y + 1) * sy;
        const int y0 = (int)v0, y1 = std::min(sh, std::max((int)std::ceil(v1), y0 + 1));
        for (int x = 0; x < dw; ++x) {
            const double u0 = x * sx, u1 = (x + 1) * sx;
            const int x0 = (int)u0, x1 = std::min(sw, std::max((int)std::ceil(u1), x0 + 1));
            double acc[C] = {}, wsum = 0.0;
            for (int yy = y0; yy < y1; ++yy) {
                const double wy = std::min((double)yy + 1, v1) - std::max((double)yy, v0);
                for (int xx = x0; xx < x1; ++xx) {
                    const double wx = std::min((double)xx + 1, u1) - std::max((double)xx, u0);
                    const double w = wy * wx;
                    const T* p = src + ((size_t)yy * sw + xx) * C;
                    for (int c = 0; c < C; ++c) acc[c] += w * (double)p[c];
                    wsum += w;
                }
            }
            T* q = dst + ((size_t)y * dw + x) * C;
            for (int c = 0; c < C; ++c) {
                double r = wsum > 0.0 ? acc[c] / wsum : 0.0;
                if constexpr (std::is_integral_v<T>) {
                    r = std::round(r);
                    r = std::min(std::max(r, 0.0), (double)std::numeric_limits<T>::max());
                }
                q[c] = (T)r;
            }
        }
    }
}

// A 2-tap bilinear shrinking 4x reads one source pixel in four and aliases the
// rest away, which is exactly what train_resolution_divisor 4 and 8 ask for.
template<typename T, int C>
inline void cpu_resize(const T* src, int sh, int sw, T* dst, int dh, int dw)
{
    if (dh <= sh && dw <= sw && (dh < sh || dw < sw))
        cpu_area_resize<T, C>(src, sh, sw, dst, dh, dw);
    else
        cpu_bilinear_resize<T, C>(src, sh, sw, dst, dh, dw);
}

inline void cpu_nearest_resize_u8(const uint8_t* src, int sh, int sw,
                                  uint8_t* dst, int dh, int dw)
{
    const float sx = (float)sw / (float)dw;
    const float sy = (float)sh / (float)dh;
    for (int y = 0; y < dh; ++y) {
        float v  = ((float)y + 0.5f) * sy - 0.5f;
        int   ys = std::max(0, std::min(sh - 1, (int)std::floor(v + 0.5f)));
        for (int x = 0; x < dw; ++x) {
            float u  = ((float)x + 0.5f) * sx - 0.5f;
            int   xs = std::max(0, std::min(sw - 1, (int)std::floor(u + 0.5f)));
            dst[(size_t)y * dw + x] = src[(size_t)ys * sw + xs];
        }
    }
}


// Felzenszwalb-Huttenlocher 1D lower-envelope squared-Euclidean DT:
// d[q] = min over i of f[i] + (q-i)^2, over the i whose f[i] is finite.
// v / z are scratch of length n / n+1.
inline void dt_1d_squared(const float* f, float* d, int n,
                          int* v, double* z)
{
    const float  INF_F = std::numeric_limits<float>::infinity();
    const double INF_D = std::numeric_limits<double>::infinity();
    int first = 0;
    while (first < n && !std::isfinite(f[first])) ++first;
    if (first >= n) {
        for (int q = 0; q < n; ++q) d[q] = INF_F;
        return;
    }
    int k = 0;
    v[0] = first;
    z[0] = -INF_D;
    z[1] = +INF_D;
    for (int q = first + 1; q < n; ++q) {
        if (!std::isfinite(f[q])) continue;   // NaN too: it makes s NaN below
        double s;
        for (;;) {
            int vk = v[k];
            // Intersections in double though f is float: at 4K the operands
            // reach ~3e7 and the subtraction cancels, which is where a float
            // envelope would start picking the wrong parabola.
            double aq = (double)f[q]  + (double)q  * (double)q;
            double av = (double)f[vk] + (double)vk * (double)vk;
            s = (aq - av) / (2.0 * (double)(q - vk));
            if (k == 0 || s > z[k]) break;    // k == 0: never index v below 0
            --k;
        }
        ++k;
        v[k] = q;
        z[k] = s;
        z[k+1] = +INF_D;
    }
    const int kmax = k;
    k = 0;
    for (int q = 0; q < n; ++q) {
        while (k < kmax && z[k+1] < (double)q) ++k;
        double dq = (double)(q - v[k]);
        d[q] = (float)(dq * dq + (double)f[v[k]]);
    }
}

// One row of the 2D DT, straight off the mask bytes. Along a single row the
// Euclidean distance is just |dx| to the nearest source, so two linear sweeps
// give the same answer as the parabola envelope for half the time.
inline void dt_row_squared(const uint8_t* m, int w, uint8_t src_value, float* d)
{
    const float INF_F = std::numeric_limits<float>::infinity();
    int last = -1;
    for (int x = 0; x < w; ++x) {
        if (m[x] == src_value) last = x;
        d[x] = (last < 0) ? INF_F : (float)(x - last);
    }
    int next = -1;
    for (int x = w - 1; x >= 0; --x) {
        if (m[x] == src_value) next = x;
        const float back = (next < 0) ? INF_F : (float)(next - x);
        const float best = d[x] < back ? d[x] : back;
        d[x] = best * best;                   // INF squared stays INF
    }
}

// Columns are transformed a block at a time rather than one at a time: 16
// floats is one cache line, and the naive per-column walk strides the whole
// image twice per column, which measured 64% of the transform at 3840^2.
constexpr int kColBlock = 16;

// 2D squared-Euclidean DT via separable 1D passes (row, then col).
// `src_value` selects which mask value (0 or 1) acts as the source set.
// `cols` / `cols_out` are scratch of kColBlock * h.
inline void dt2d_squared(const uint8_t* mask, int h, int w, uint8_t src_value,
                         float* d2_out, float* cols, float* cols_out,
                         int* v_buf, double* z_buf)
{
    for (int y = 0; y < h; ++y)
        dt_row_squared(mask + (size_t)y * w, w, src_value,
                       d2_out + (size_t)y * w);

    for (int x0 = 0; x0 < w; x0 += kColBlock) {
        const int nc = std::min(kColBlock, w - x0);
        for (int y = 0; y < h; ++y) {
            const float* src = d2_out + (size_t)y * w + x0;
            for (int c = 0; c < nc; ++c) cols[(size_t)c * h + y] = src[c];
        }
        for (int c = 0; c < nc; ++c)
            dt_1d_squared(cols + (size_t)c * h, cols_out + (size_t)c * h,
                          h, v_buf, z_buf);
        for (int y = 0; y < h; ++y) {
            float* dst = d2_out + (size_t)y * w + x0;
            for (int c = 0; c < nc; ++c) dst[c] = cols_out[(size_t)c * h + y];
        }
    }
}

// Shrink (offset_px < 0) or dilate (offset_px > 0) a binary mask in place
// by abs(offset_px) Euclidean pixels. Output: 1 where signed-distance <=
// offset_px, signed-distance = (dist to foreground) - (dist to background).
inline void apply_mask_boundary_offset_in_place(uint8_t* mask, int h, int w,
                                                float offset_px)
{
    if (offset_px == 0.0f || h <= 0 || w <= 0 || (h == 1 && w == 1)) return;

    const int mx = std::max(h, w);
    // new[] over vector: the row pass writes every element, so vector's
    // value-initialising zero fill is 118 MB of pure memset at 3840^2.
    std::unique_ptr<float[]>  d2_fg(new float[(size_t)h * w]);
    std::unique_ptr<float[]>  d2_bg(new float[(size_t)h * w]);
    std::unique_ptr<float[]>  cols(new float[(size_t)kColBlock * h]);
    std::unique_ptr<float[]>  cols_out(new float[(size_t)kColBlock * h]);
    std::unique_ptr<int[]>    v_buf(new int[mx]);
    std::unique_ptr<double[]> z_buf(new double[(size_t)mx + 1]);

    dt2d_squared(mask, h, w, /*src_value=*/1, d2_fg.get(),
                 cols.get(), cols_out.get(), v_buf.get(), z_buf.get());
    dt2d_squared(mask, h, w, /*src_value=*/0, d2_bg.get(),
                 cols.get(), cols_out.get(), v_buf.get(), z_buf.get());

    for (size_t i = 0; i < (size_t)h * w; ++i) {
        float sd = std::sqrt(d2_fg[i]) - std::sqrt(d2_bg[i]);
        mask[i] = (sd <= offset_px) ? 1 : 0;
    }
}


// Modality decoders. `dst_h` / `dst_w` are the BATCH-slot shape (= group
// shape); when the file is smaller it is upsampled (nearest for mask,
// bilinear for depth / normal). The 1x1 mask case is a degenerate
// nearest-neighbor broadcast and falls out for free.

// One warning per group, however many workers decode into it concurrently.
// Only an upsample earns one: the parser fits each camera to its own image, so
// a shrink here is what train_resolution_divisor asked for.
static void _warn_rgb_dim_mismatch_once(
    const std::string& path,
    int actual_w, int actual_h,
    int expected_w, int expected_h)
{
    if (actual_w >= expected_w && actual_h >= expected_h) return;
    static std::mutex                       mu;
    static std::set<std::pair<int, int>>    seen;
    std::lock_guard<std::mutex> lk(mu);
    auto key = std::make_pair(expected_w, expected_h);
    if (seen.insert(key).second) {
        std::fprintf(stderr, "%s %s\n", dmsg::word_warning.get(),
            format(dmsg::rgb_shape_mismatch,
                   {path, actual_w, actual_h, expected_w, expected_h}).c_str());
        std::fflush(stderr);
    }
}

// `decode_threads` is what an EXR may use: 1 on the worker pool, which is
// already 16 wide, and every core for a lone image the viewer asked for.
void decode_rgb_into(const std::string& path,
                     int expected_h, int expected_w,
                     PixelDType dtype,
                     uint8_t* dst,
                     int decode_threads = 1)
{
    int w, h, ch;
    if (dtype == PixelDType::FLOAT32) {
        exr::Info info;
        exr::Options opt;
        opt.threads = decode_threads;
        std::vector<float> px;
        const std::string err = exr::decode(path, opt, info, px);
        if (!err.empty())
            throw std::runtime_error(decode_failure(path) + " (" + err + ")");
        if (info.width == expected_w && info.height == expected_h) {
            std::memcpy(dst, px.data(), px.size() * sizeof(float));
        } else {
            _warn_rgb_dim_mismatch_once(path, info.width, info.height, expected_w, expected_h);
            cpu_resize<float, 3>(px.data(), info.height, info.width,
                                 (float*)dst, expected_h, expected_w);
        }
    } else if (dtype == PixelDType::UINT16) {
        stbi_us* img = stbi_load_16(path.c_str(), &w, &h, &ch, 3);
        if (!img) throw std::runtime_error(decode_failure(path));
        if (w == expected_w && h == expected_h) {
            std::memcpy(dst, img, (size_t)w * h * 3 * sizeof(stbi_us));
        } else {
            _warn_rgb_dim_mismatch_once(path, w, h, expected_w, expected_h);
            cpu_resize<stbi_us, 3>(img, h, w, (stbi_us*)dst, expected_h, expected_w);
        }
        stbi_image_free(img);
    } else if (dtype == PixelDType::UINT8) {
        stbi_uc* img = stbi_load(path.c_str(), &w, &h, &ch, 3);
        if (!img) throw std::runtime_error(decode_failure(path));
        if (w == expected_w && h == expected_h) {
            std::memcpy(dst, img, (size_t)w * h * 3);
        } else {
            _warn_rgb_dim_mismatch_once(path, w, h, expected_w, expected_h);
            cpu_resize<stbi_uc, 3>(img, h, w, dst, expected_h, expected_w);
        }
        stbi_image_free(img);
    } else {
        throw std::runtime_error("DataManager: unsupported RGB pixel type for '" + path + "'");
    }
}

void decode_mask_into(const std::string& path,
                      int dst_h, int dst_w,
                      bool flip,
                      float boundary_offset_frac,
                      uint8_t* dst)
{
    int w, h, ch;
    stbi_uc* img = stbi_load(path.c_str(), &w, &h, &ch, 1);
    if (!img) throw std::runtime_error(decode_failure(path));

    // Binarize on-disk pixels first so the broadcast / resize always emits
    // strict 0/1 (matches the kernel's bool semantics).
    for (size_t i = 0; i < (size_t)w * h; ++i)
        img[i] = (uint8_t)(img[i] != 0);

    if (w == dst_w && h == dst_h) {
        std::memcpy(dst, img, (size_t)w * h);
    } else {
        cpu_nearest_resize_u8(img, h, w, dst, dst_h, dst_w);
    }
    stbi_image_free(img);

    // Before the offset: a flipped mask's boundary is the one to grow or shrink.
    if (flip)
        for (size_t i = 0; i < (size_t)dst_h * dst_w; i++) dst[i] = (uint8_t)!dst[i];

    // Apply signed boundary offset (dilate/erode) at the decoded resolution.
    // offset_px = fraction * sqrt(dst_W * dst_H).
    if (boundary_offset_frac != 0.0f) {
        float offset_px = boundary_offset_frac
                        * std::sqrt((float)dst_w * (float)dst_h);
        apply_mask_boundary_offset_in_place(dst, dst_h, dst_w, offset_px);
    }
}

void decode_depth_into(const std::string& path,
                       int dst_h, int dst_w,
                       PixelDType dtype,
                       uint8_t* dst)
{
    int w, h, ch;
    if (dtype != PixelDType::UINT16)
        throw std::runtime_error("DataManager: only 16-bit depth PNGs are supported in stb_image path");
    stbi_us* img = stbi_load_16(path.c_str(), &w, &h, &ch, 1);
    if (!img) throw std::runtime_error(decode_failure(path));
    if (w == dst_w && h == dst_h) {
        std::memcpy(dst, img, (size_t)w * h * sizeof(stbi_us));
    } else {
        cpu_bilinear_resize<stbi_us, 1>(img, h, w, (stbi_us*)dst, dst_h, dst_w);
    }
    stbi_image_free(img);
}

void decode_normal_into(const std::string& path,
                        int dst_h, int dst_w,
                        uint8_t* dst)
{
    int w, h, ch;
    stbi_uc* img = stbi_load(path.c_str(), &w, &h, &ch, 3);
    if (!img) throw std::runtime_error(decode_failure(path));
    if (w == dst_w && h == dst_h) {
        std::memcpy(dst, img, (size_t)w * h * 3);
    } else {
        cpu_bilinear_resize<stbi_uc, 3>(img, h, w, dst, dst_h, dst_w);
    }
    stbi_image_free(img);
}

bool probe_image_shape(const std::string& path, int& w, int& h) {
    return !path.empty() && probe_image_size(path.c_str(), &w, &h);
}

// On-disk RGB dtype. EXR is float32 whatever it stores: half carries values
// above 1, and 16-bit normalized would clip every one of them.
PixelDType probe_pixel_dtype(const std::string& path,
                             PixelDType fallback = PixelDType::UINT8)
{
    if (path.empty()) return fallback;
    if (exr::is_exr(path)) return PixelDType::FLOAT32;
    if (stbi_is_16_bit(path.c_str())) return PixelDType::UINT16;
    return PixelDType::UINT8;
}

} // anonymous namespace


// ===========================================================================
// DecodedBatch::build_views
// ===========================================================================
void DecodedBatch::build_views() {
    auto mk = [](const void* data, uint32_t es,
                 std::vector<int64_t> shape) -> TorchTensorView {
        return TorchTensorView((uint64_t)data, es, std::move(shape));
    };
    // Start from nothing. A batch is a reused object -- the prefetch pools
    // recycle their slots and the preview keeps one static batch across
    // datasets -- and downstream a non-null view IS the statement that the
    // batch carries that modality. Leaving last time's view standing is how a
    // masked dataset's mask ends up supervising an unmasked one.
    const TorchTensorView none{0, 0, {}};
    input_intrins_view = input_dist_coeffs_view = none;
    input_source_models_view = input_source_params_view = none;
    viewmats_view = intrins_view = dist_coeffs_view = none;
    rgb_view = mask_view = depth_view = normal_view = none;
    // Camera-param views use the POST-split count.
    int64_t B_post = num;
    viewmats_view    = mk(viewmats.data(),    4, {B_post, 4LL, 4LL});
    intrins_view     = mk(intrins.data(),     4, {B_post, 4LL});
    dist_coeffs_view = mk(dist_coeffs.data(), 4,
                          {B_post, (long long)kCameraDistortionParams});

    // Per-INPUT intrins / dist_coeffs views (size B_in). Empty when the
    // warp kernel doesn't need them.
    int64_t B_in = input_num;
    if (!input_intrins.empty()) {
        input_intrins_view = mk(input_intrins.data(), 4, {B_in, 4LL});
        input_dist_coeffs_view = mk(input_dist_coeffs.data(), 4,
                                   {B_in, (long long)kCameraDistortionParams});
        if (!input_source_models.empty()) {
            input_source_models_view = TorchTensorView(
                (uint64_t)input_source_models.data(), 4, {B_in});
            input_source_params_view = mk(input_source_params.data(), 4, {B_in, 16LL});
        }
    }

    if (!face_axes.empty())
        face_axes_view = mk(face_axes.data(), 4, {B_post, 3LL, 3LL});

    // Image / modality views stay at INPUT shape. The engine warps on the
    // fly when K > 1; when K == 1, input == post by construction.
    int64_t H_in = input_height, W_in = input_width;
    if (B_in > 0 && H_in > 0 && W_in > 0 && !rgb_buffer.empty()) {
        rgb_view = mk(rgb_buffer.data(), pixel_dtype_size(rgb_dtype),
                      {B_in, H_in, W_in, 3LL});
    }
    if (!mask_buffer.empty()) {
        mask_view = mk(mask_buffer.data(), 1,
                       {B_in, (int64_t)mask_height, (int64_t)mask_width, 1LL});
    }
    if (!depth_buffer.empty()) {
        depth_view = mk(depth_buffer.data(), pixel_dtype_size(depth_dtype),
                        {B_in, (int64_t)depth_height, (int64_t)depth_width, 1LL});
    }
    if (!normal_buffer.empty()) {
        normal_view = mk(normal_buffer.data(), pixel_dtype_size(normal_dtype),
                         {B_in, (int64_t)normal_height, (int64_t)normal_width, 3LL});
    }
}


// ===========================================================================
// DataManagerImpl — PIMPL
// ===========================================================================
class DataManagerImpl {
public:

    DataManagerImpl(
        DataManagerConfig config,
        std::vector<int32_t>     camera_models,       // per-camera enum int
        std::vector<int32_t>     camera_distortions,  // per-camera tier int
        std::vector<std::string> image_filenames,
        std::vector<std::string> mask_filenames,
        std::vector<std::string> depth_filenames,
        std::vector<std::string> normal_filenames,
        std::vector<int32_t>     widths,
        std::vector<int32_t>     heights,
        std::vector<int32_t>     K_per_camera,
        std::vector<int32_t>     post_offsets,
        std::vector<float>       viewmats,        // post-split
        std::vector<float>       intrins,         // post-split
        std::vector<float>       dist_coeffs,     // post-split
        std::vector<int32_t>     post_widths,     // post-split
        std::vector<int32_t>     post_heights,
        std::vector<float>       face_axes,
        std::vector<float>       input_intrins,   // per-input
        std::vector<float>       input_dist_coeffs,
        std::vector<int32_t>     redistort_models,
        std::vector<float>       redistort_params, // per-input
        std::vector<int32_t>     train_indices,
        std::vector<int32_t>     val_indices);

    ~DataManagerImpl();

    const TrainStep&    next_train_step();
    const DecodedBatch& next_train_batch();
    const DecodedBatch* next_val_batch();
    void                fetch_one(int32_t index, DecodedBatch& out);

    std::string data_error() const;
    void        resolve_data_error(bool retry);

    int64_t num_train()    const { return (int64_t)_train_indices.size(); }
    int64_t num_val()      const { return (int64_t)_val_indices.size(); }
    bool    has_val()      const { return !_val_indices.empty(); }
    CacheMode cache_mode() const { return _cfg.cache_mode; }

    int max_face_passes() const {
        int n = 1;
        for (const auto& g : _train_groups) n = std::max(n, (int)g.passes.size());
        for (const auto& g : _val_groups)   n = std::max(n, (int)g.passes.size());
        return n;
    }

    // Largest INPUT-image batch size (pre-split) for a single training step.
    // The engine's split_batch dispatcher carves a sub-batch per INPUT image
    // (the K post-split cameras of one input stay together in one sub-batch,
    // see _engine_train_step_split_warped), so this is what determines
    // whether split_batch is a no-op (==1) vs. whether sub-batching matters
    // (>1). Used by engine_train_step_managed to resolve split_batch vs FPBO
    // when the user enables both.
    int64_t max_input_batch_size() const {
        if (_train_indices.empty()) return 0;
        int64_t bs = (int64_t)std::max(1, _cfg.train_batch_size);
        return std::min(bs, (int64_t)_train_indices.size());
    }

    bool has_masks()   const {
        return (_cfg.load_masks && !_mask_filenames.empty()) || _has_synth_masks;
    }
    bool has_depths()  const { return !_depth_filenames.empty()  && _cfg.load_depths; }
    bool has_normals() const { return !_normal_filenames.empty() && _cfg.load_normals; }

private:

    // ---- Shared input data ------------------------------------------------
    DataManagerConfig         _cfg;
    // Per-camera model enum value. Length matches the dataset N. Groups are
    // partitioned by this so a mixed pinhole + fisheye dataset just yields
    // two extra groups; batches stay homogeneous.
    std::vector<int32_t>      _camera_models;
    std::vector<int32_t>      _camera_distortions;
    std::vector<int32_t>      _redistort_models;
    std::vector<float>        _redistort_params;
    // Per-input K and post-split offset. Length N. K[i] is the split factor
    // for input camera i; post_offsets[i] is the starting index in the
    // POST-split arrays (_viewmats / _intrins / _dist_coeffs / _face_axes).
    std::vector<int32_t>      _K_per_camera;
    std::vector<int32_t>      _post_offsets;
    int64_t                   _n_post = 0;     // total post-split cameras
    std::vector<std::string>  _image_filenames;
    std::vector<std::string>  _mask_filenames;
    // Per-image synthetic-mask flag: load_masks is on and no real mask file
    // is supplied. The decode paths treat these slots as an all-white mask,
    // which the warp kernel projects into a post-split FOV mask.
    std::vector<uint8_t>      _synth_white_mask;
    bool                      _has_synth_masks = false;
    std::vector<std::string>  _depth_filenames;
    std::vector<std::string>  _normal_filenames;
    std::vector<int32_t>      _widths, _heights;
    std::vector<float>        _viewmats;     // [N_post,4,4] flat
    std::vector<float>        _intrins;      // [N_post,4]
    std::vector<float>        _dist_coeffs;  // [N_post,8]
    std::vector<int32_t>      _post_widths;  // [N_post]; empty = input sizes
    std::vector<int32_t>      _post_heights;
    std::vector<float>        _face_axes;    // [N_post,3,3]; empty = identity
    // Per-INPUT intrins / dist_coeffs (length N each).  Empty when no
    // fisheye-warp camera is present.
    std::vector<float>        _input_intrins;
    std::vector<float>        _input_dist_coeffs;
    std::vector<int32_t>      _train_indices;
    std::vector<int32_t>      _val_indices;

    // Per-image detected RGB / depth / normal dtypes. Cached at construction
    // so the disk path doesn't re-probe headers on every fetch.
    std::vector<PixelDType>   _rgb_dtype;
    std::vector<PixelDType>   _depth_dtype;
    std::vector<PixelDType>   _normal_dtype;

    // Per-image on-disk modality shape (W, H). 0 when the image has no
    // file for that modality, or the modality is disabled. Probed in
    // probe_dtypes() via stb_image's header-only reader; cost is ~one
    // small open() per file.
    //
    // Uniformity is enforced PER-GROUP (see build_index_groups) rather than
    // dataset-wide: a single dataset may legitimately mix image resolutions,
    // and as long as a group is internally uniform the batch buffers work.
    std::vector<int32_t> _mask_h_per,   _mask_w_per;
    std::vector<int32_t> _depth_h_per,  _depth_w_per;
    std::vector<int32_t> _normal_h_per, _normal_w_per;

    // ---- CPU mode preloaded buffers --------------------------------------
    //
    // Per-image, the decoded payload. Empty for images that don't have the
    // modality on disk. For DISK mode these stay empty.
    std::vector<std::vector<uint8_t>> _rgb_cache;
    std::vector<std::vector<uint8_t>> _mask_cache;
    std::vector<std::vector<uint8_t>> _depth_cache;
    std::vector<std::vector<uint8_t>> _normal_cache;

    // ---- Batch sampling state --------------------------------------------
    std::mutex                _sampling_mu;
    std::mt19937_64           _rng;
    std::vector<IndexGroup>   _train_groups;
    std::vector<IndexGroup>   _val_groups;
    GroupSampler              _val_sampler;

    // Deterministic per-epoch training schedule. Every training image appears
    // exactly once per pass over `_train_schedule`; when the cursor reaches the
    // end the schedule is rebuilt (reshuffled) for the next epoch. Built by
    // build_train_schedule_locked() under _sampling_mu.
    std::vector<StepSpec>     _train_schedule;
    size_t                    _train_sched_cursor = 0;

    // The currently-returned-to-caller data. We hold one slot per kind so the
    // reference returned by next_*_batch()/next_train_step() stays valid until
    // the next call to that same getter.
    TrainStep                 _cpu_train_step;   // CPU-mode train (persistent)
    DecodedBatch              _cpu_val_batch;

    // ---- DISK mode: prefetch pipeline ------------------------------------
    struct DecodeJob {
        int64_t              batch_id   = 0;
        int32_t              slot       = 0;   // row inside batch buffer
        int32_t              ds_index   = 0;   // dataset-global image index
        std::shared_ptr<DecodedBatch>     batch;
        std::shared_ptr<std::atomic<int>> remaining;  // decrement on done
        // Exactly one of the two publish targets is set. For validation
        // (single-batch) jobs, `ready_q` receives the finished DecodedBatch.
        // For training (step) jobs, `step_q` receives the finished TrainStep
        // (and `step` keeps all its sub-batches alive). The worker that
        // decrements `remaining` to zero publishes; no separate completer /
        // scheduler-side wait is involved, so all worker pools stay saturated
        // while multiple batches/steps are in flight.
        BoundedQueue<std::shared_ptr<DecodedBatch>>* ready_q = nullptr;
        std::shared_ptr<TrainStep>                   step;
        BoundedQueue<std::shared_ptr<TrainStep>>*    step_q  = nullptr;
    };

    bool _disk_started = false;

    std::unique_ptr<BoundedQueue<DecodeJob>> _q_rgb;
    std::unique_ptr<BoundedQueue<DecodeJob>> _q_mask;
    std::unique_ptr<BoundedQueue<DecodeJob>> _q_depth;
    std::unique_ptr<BoundedQueue<DecodeJob>> _q_normal;
    std::unique_ptr<BoundedQueue<std::shared_ptr<TrainStep>>>    _q_ready_train;
    std::unique_ptr<BoundedQueue<std::shared_ptr<DecodedBatch>>> _q_ready_val;

    std::vector<std::thread> _workers;
    std::thread              _scheduler;
    std::atomic<bool>        _stop{false};
    std::atomic<int64_t>     _next_batch_id{0};

    // A decode that threw. The worker parks here rather than letting the
    // exception escape its thread (which is std::terminate), so a dataset
    // moved mid-run pauses the pipeline instead of killing the process.
    std::mutex               _fault_mu;
    std::condition_variable  _fault_cv;
    std::string              _fault_msg;
    bool                     _fault_parked  = false;
    bool                     _fault_give_up = false;

    // The objects currently held alive on behalf of the most recent
    // next_*() return value.
    std::shared_ptr<TrainStep>    _last_train_step_held;
    std::shared_ptr<DecodedBatch> _last_val_held;

    // Decrement a job's shared step/batch counter and, when it reaches zero,
    // build the views and publish to the appropriate ready queue.
    void publish_if_done(DecodeJob& job);

    // ---- Setup helpers ---------------------------------------------------
    void probe_dtypes();
    // Build (W,H)-keyed groups, enforcing per-modality uniformity WITHIN
    // each group (allowing 1x1 broadcast for mask). Throws on intra-group
    // shape mismatch. Inter-group differences are fine.
    std::vector<IndexGroup> build_index_groups_member(
        const std::vector<int32_t>& flat_indices) const;
    void preload_cpu_cache();
    void allocate_batch(DecodedBatch& b,
                        const IndexGroup& g,
                        const std::vector<int32_t>& ds_indices);
    void fill_camera_params(DecodedBatch& b);

    // CPU-mode synchronous batch fetch helpers.
    DecodedBatch& next_batch_cpu(DecodedBatch& slot,
                                 std::vector<IndexGroup>& groups,
                                 GroupSampler& sampler,
                                 int batch_size);
    void fill_batch_from_cache(DecodedBatch& b);

    // ---- Deterministic epoch schedule (training) -------------------------
    // Rebuild _train_schedule for a fresh epoch: shuffle each group, emit full
    // B-image chunks as single-sub-batch steps, and pack sub-B remainders
    // (non-warp groups only) across resolutions into <=B-image steps. Caller
    // must hold _sampling_mu.
    void build_train_schedule_locked();
    // Return the next scheduled step, rebuilding the schedule at epoch
    // boundaries. Thread-safe (locks _sampling_mu).
    StepSpec next_train_step_spec();
    // CPU-mode: materialize the next step's sub-batches from the cache.
    const TrainStep& next_step_cpu();

    // Disk-mode helpers.
    void start_disk_pipeline();
    void stop_disk_pipeline();
    void scheduler_loop();
    void worker_loop_rgb();
    void worker_loop_mask();
    void worker_loop_depth();
    void worker_loop_normal();
    // Run `decode`, parking the thread on a throw until the front end answers
    // resolve_data_error(). False means abandon the job and exit the worker.
    bool decode_or_park(const std::function<void()>& decode);
    void enqueue_batch(IndexGroup& group, int batch_size,
                       BoundedQueue<std::shared_ptr<DecodedBatch>>& ready_q);
    // Disk-mode: allocate + enqueue decode jobs for a whole training step
    // (one or more sub-batches), publishing the TrainStep when all decode.
    void enqueue_step(const StepSpec& spec,
                      BoundedQueue<std::shared_ptr<TrainStep>>& step_q);
};


// ===========================================================================
// Construction
// ===========================================================================
DataManagerImpl::DataManagerImpl(
    DataManagerConfig          config,
    std::vector<int32_t>       camera_models,
    std::vector<int32_t>       camera_distortions,
    std::vector<std::string>   image_filenames,
    std::vector<std::string>   mask_filenames,
    std::vector<std::string>   depth_filenames,
    std::vector<std::string>   normal_filenames,
    std::vector<int32_t>       widths,
    std::vector<int32_t>       heights,
    std::vector<int32_t>       K_per_camera,
    std::vector<int32_t>       post_offsets,
    std::vector<float>         viewmats,
    std::vector<float>         intrins,
    std::vector<float>         dist_coeffs,
    std::vector<int32_t>       post_widths,
    std::vector<int32_t>       post_heights,
    std::vector<float>         face_axes,
    std::vector<float>         input_intrins,
    std::vector<float>         input_dist_coeffs,
    std::vector<int32_t>       redistort_models,
    std::vector<float>         redistort_params,
    std::vector<int32_t>       train_indices,
    std::vector<int32_t>       val_indices)
    : _cfg(config),
      _camera_models(std::move(camera_models)),
      _camera_distortions(std::move(camera_distortions)),
      _redistort_models(std::move(redistort_models)),
      _redistort_params(std::move(redistort_params)),
      _K_per_camera(std::move(K_per_camera)),
      _post_offsets(std::move(post_offsets)),
      _image_filenames(std::move(image_filenames)),
      _mask_filenames(std::move(mask_filenames)),
      _depth_filenames(std::move(depth_filenames)),
      _normal_filenames(std::move(normal_filenames)),
      _widths(std::move(widths)), _heights(std::move(heights)),
      _viewmats(std::move(viewmats)),
      _intrins(std::move(intrins)),
      _dist_coeffs(std::move(dist_coeffs)),
      _post_widths(std::move(post_widths)),
      _post_heights(std::move(post_heights)),
      _face_axes(std::move(face_axes)),
      _input_intrins(std::move(input_intrins)),
      _input_dist_coeffs(std::move(input_dist_coeffs)),
      _train_indices(std::move(train_indices)),
      _val_indices(std::move(val_indices))
{
    int64_t N = (int64_t)_image_filenames.size();
    if ((int64_t)_camera_models.size() != N)
        throw std::runtime_error("DataManager: camera_models length mismatch");
    // Default K + offsets if caller passed empty vectors (legacy no-warp use).
    if (_K_per_camera.empty()) {
        _K_per_camera.assign((size_t)N, 1);
    }
    if (_post_offsets.empty()) {
        _post_offsets.assign((size_t)N, 0);
        for (int64_t i = 0; i < N; ++i) _post_offsets[i] = (int32_t)i;
    }
    if ((int64_t)_K_per_camera.size() != N ||
        (int64_t)_post_offsets.size() != N)
        throw std::runtime_error("DataManager: K_per_camera / post_offsets length mismatch");
    int64_t n_post = 0;
    for (int64_t i = 0; i < N; ++i) {
        if (_K_per_camera[i] <= 0)
            throw std::runtime_error("DataManager: K_per_camera must be >= 1");
        if (_post_offsets[i] != (int32_t)n_post)
            throw std::runtime_error("DataManager: post_offsets must be exclusive prefix-sum of K");
        n_post += _K_per_camera[i];
    }
    _n_post = n_post;
    if ((int64_t)_widths.size()  != N ||
        (int64_t)_heights.size() != N ||
        (int64_t)_viewmats.size()    != n_post * 16 ||
        (int64_t)_intrins.size()     != n_post * 4 ||
        (int64_t)_dist_coeffs.size() != n_post * kCameraDistortionParams) {
        throw std::runtime_error(
            "DataManager: per-camera array length mismatch (expected widths/heights "
            "of length N and viewmats/intrins/dist_coeffs of length N_post = sum(K))");
    }
    if (!_input_intrins.empty() &&
        ((int64_t)_input_intrins.size() != N * 4 ||
         (int64_t)_input_dist_coeffs.size() != N * kCameraDistortionParams)) {
        throw std::runtime_error(
            "DataManager: input_intrins / input_dist_coeffs length mismatch "
            "(expected length 4*N / 8*N respectively, or both empty)");
    }
    if (!_post_widths.empty() &&
        ((int64_t)_post_widths.size() != n_post ||
         (int64_t)_post_heights.size() != n_post ||
         (int64_t)_face_axes.size() != n_post * 9))
        throw std::runtime_error(
            "DataManager: post_widths / post_heights / face_axes length "
            "mismatch (expected N_post, N_post and 9*N_post, or all empty)");
    for (int64_t i = 0; i < N; ++i)
        if (_K_per_camera[i] > 1 && _post_widths.empty())
            throw std::runtime_error(
                "DataManager: a split camera needs post_widths / post_heights / face_axes");
    if (!_mask_filenames.empty()   && (int64_t)_mask_filenames.size()   != N)
        throw std::runtime_error("DataManager: mask_filenames length mismatch");

    // Synthesize a full-white mask for every image without a real mask
    // file when load_masks is enabled. Two reasons:
    //   1) warp_to_pinhole on fisheye/equisolid needs an in-FOV mask so
    //      the unseen cubemap-face regions outside the lens get masked
    //      out (warp_mask_wide projects the all-ones input -> FOV mask).
    //   2) Once (1) flips on load_masks for the run, EVERY group has a
    //      mask buffer allocated. Pinhole images in a mixed dataset
    //      that don't have a real mask file would otherwise get the
    //      default all-zero slot -- every pixel marked invalid, the
    //      per-pixel loss kernels skip them, and they're effectively
    //      never trained on (also why their thumbnail rendered as a
    //      solid gray panel). Synthesizing all-ones for them makes the
    //      "no mask file" case equivalent to "every pixel valid", which
    //      is the conventional NeRF/3DGS interpretation.
    // The equirectangular warp kernel projects an all-ones input to
    // all-ones too (it covers the full sphere), so this is also
    // correct for equirect inputs.
    _synth_white_mask.assign((size_t)N, 0);
    if (_cfg.load_masks) {
        for (int64_t i = 0; i < N; ++i) {
            bool has_real = (!_mask_filenames.empty() &&
                             !_mask_filenames[i].empty());
            if (!has_real) {
                _synth_white_mask[i] = 1;
                _has_synth_masks = true;
            }
        }
        // Ensure _mask_filenames is sized to N so per-image lookups below
        // are valid (entries for synthesized slots stay empty strings,
        // which the probe / decode paths special-case).
        if (_has_synth_masks && _mask_filenames.empty()) {
            _mask_filenames.assign((size_t)N, std::string());
        }
    }

    if (!_depth_filenames.empty()  && (int64_t)_depth_filenames.size()  != N)
        throw std::runtime_error("DataManager: depth_filenames length mismatch");
    if (!_normal_filenames.empty() && (int64_t)_normal_filenames.size() != N)
        throw std::runtime_error("DataManager: normal_filenames length mismatch");

    uint64_t seed = _cfg.seed != 0 ? _cfg.seed
        : (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count();
    _rng.seed(seed);

    probe_dtypes();

    _train_groups = build_index_groups_member(_train_indices);
    _val_groups   = build_index_groups_member(_val_indices);
    for (auto& g : _train_groups) g.rewind(_rng, /*eval=*/false);
    for (auto& g : _val_groups)   g.rewind(_rng, /*eval=*/true);
    _val_sampler   = GroupSampler(_val_groups);

    // Build the first epoch's deterministic training schedule (once-per-epoch
    // traversal + cross-group remainder packing). The DISK scheduler thread
    // started below draws from it, so build before spinning threads up.
    {
        std::lock_guard<std::mutex> lk(_sampling_mu);
        build_train_schedule_locked();
    }

    if (_cfg.cache_mode == CacheMode::CPU) {
        preload_cpu_cache();
    } else {
        start_disk_pipeline();
    }
}

DataManagerImpl::~DataManagerImpl() {
    if (_disk_started) stop_disk_pipeline();
}


// ---- Probe per-file dtypes (cheap; only reads PNG headers) -----------------
void DataManagerImpl::probe_dtypes() {
    int64_t N = (int64_t)_image_filenames.size();
    _rgb_dtype.assign((size_t)N, PixelDType::UINT8);
    for (int64_t i = 0; i < N; ++i) {
        _rgb_dtype[i] = probe_pixel_dtype(_image_filenames[i], PixelDType::UINT8);
    }
    if (has_depths()) {
        _depth_dtype.assign((size_t)N, PixelDType::UINT16);
        // depth assumed 16-bit grayscale PNG everywhere — we don't auto-detect
        // float because stb_image doesn't carry float PNG anyway.
    }
    if (has_normals()) {
        _normal_dtype.assign((size_t)N, PixelDType::UINT8);
    }

    // Probe per-image on-disk modality shape. Uniformity is enforced later
    // in build_index_groups_member, per-group rather than dataset-wide --
    // mixed-resolution datasets are OK as long as each (W,H) group is
    // internally consistent.
    auto probe_per_image = [&](const std::vector<std::string>& fns,
                               const char* name,
                               std::vector<int32_t>& out_h,
                               std::vector<int32_t>& out_w) {
        if (fns.empty()) return;
        out_h.assign((size_t)N, 0);
        out_w.assign((size_t)N, 0);
        for (int64_t i = 0; i < N; ++i) {
            if (fns[i].empty()) continue;
            int wi, hi;
            if (!probe_image_shape(fns[i], wi, hi))
                throw std::runtime_error(std::string("DataManager: failed to probe ")
                    + name + " '" + fns[i] + "'");
            out_h[i] = (int32_t)hi;
            out_w[i] = (int32_t)wi;
        }
    };
    if (has_masks())   probe_per_image(_mask_filenames,   "mask",   _mask_h_per,   _mask_w_per);
    // Synthesized white masks: match the per-input image shape exactly.
    // The wide-warp mask kernel projects post-split pixels into the input
    // (fisheye/equisolid) image with the original intrins, then does an
    // (xs >= 0 && xs < W && ys >= 0 && ys < H) bounds check against the
    // input mask shape -- so a 1x1 broadcast would fail for every pixel
    // except (uv ~= 0,0) (i.e. the single center pixel of the front
    // face). We therefore allocate a full-size all-ones mask at the
    // input image's (H, W) so the projection's bounds check uses the
    // real lens FOV (valid -> in-bounds -> 1, otherwise 0).
    if (_has_synth_masks) {
        if (_mask_h_per.empty()) _mask_h_per.assign((size_t)N, 0);
        if (_mask_w_per.empty()) _mask_w_per.assign((size_t)N, 0);
        for (int64_t i = 0; i < N; ++i) {
            if (_synth_white_mask[(size_t)i]) {
                _mask_h_per[(size_t)i] = _heights[(size_t)i];
                _mask_w_per[(size_t)i] = _widths[(size_t)i];
            }
        }
    }
    if (has_depths())  probe_per_image(_depth_filenames,  "depth",  _depth_h_per,  _depth_w_per);
    if (has_normals()) probe_per_image(_normal_filenames, "normal", _normal_h_per, _normal_w_per);
}



// Build per-image-shape groups (homogeneous (RGB W, H)) and pick each
// group's modality shape as the LARGEST (by area) on-disk shape across
// the group's images. Smaller modality files are upsampled at decode time
// (nearest for mask, bilinear for depth/normal). Prints a single warning
// per modality the first time a group requires upsampling.
std::vector<IndexGroup> DataManagerImpl::build_index_groups_member(
    const std::vector<int32_t>& flat_indices) const
{
    // Group key: (W, H, model, tier, redistort, K, out_W, out_H, size run).
    // std::map handles tuple keys natively, and group build happens once at
    // construction, so we don't need an unordered_map's hash machinery here.
    std::map<std::tuple<int32_t, int32_t, int32_t, int32_t, int32_t,
                        int32_t, int32_t, int32_t, int32_t>,
             IndexGroup> by_shape;

    // Track first per-modality mismatch for a one-shot warning. Mutable so
    // this const method can update them; semantically they're cache.
    static bool warned_mask = false, warned_depth = false, warned_normal = false;

    for (int32_t i : flat_indices) {
        const bool redistort_i =
            !_redistort_models.empty() && _redistort_models[i] >= 0;
        const int32_t K_i = _K_per_camera[i];
        const int32_t out_w = K_i > 1 ? _post_widths[_post_offsets[i]]  : _widths[i];
        const int32_t out_h = K_i > 1 ? _post_heights[_post_offsets[i]] : _heights[i];
        // The whole size sequence keys the group, not just the first face:
        // per-image intrinsics give two cameras of one shape different crops,
        // and a batch renders its faces run by run.
        int32_t sig = 0;
        for (int k = 0; K_i > 1 && k < K_i; ++k) {
            const int64_t o = _post_offsets[i] + k;
            sig = sig * 31 + _post_widths[o] * 7919 + _post_heights[o];
        }
        auto key = std::make_tuple(_widths[i], _heights[i], _camera_models[i],
                                   _camera_distortions[i],
                                   (int32_t)redistort_i, K_i, out_w, out_h, sig);
        auto& g = by_shape[key];
        if (g.indices.empty()) {
            g.width  = _widths[i];
            g.height = _heights[i];
            g.model  = (CameraModelType)_camera_models[i];
            g.distortion = (CameraDistortionType)_camera_distortions[i];
            g.redistort  = redistort_i;
            g.K      = K_i;
            g.out_w  = out_w;
            g.out_h  = out_h;
            for (int32_t k = 0; k < K_i; ++k) {
                const int64_t o = _post_offsets[i] + k;
                const int32_t w = K_i > 1 ? _post_widths[o]  : out_w;
                const int32_t h = K_i > 1 ? _post_heights[o] : out_h;
                if (!g.passes.empty() && g.passes.back().width == w &&
                    g.passes.back().height == h)
                    g.passes.back().k1 = k + 1;
                else
                    g.passes.push_back(WarpFacePass{k, k + 1, w, h});
            }
        }

        // Take the max-area shape across the group. We compare by area so
        // mismatched aspect ratios collapse to a single (w, h) instead of
        // arbitrarily picking one dim's max and stretching everything.
        auto merge_max_area = [](int32_t h_i, int32_t w_i,
                                 int32_t& g_h, int32_t& g_w,
                                 bool& did_mismatch) {
            if (h_i == 0 || w_i == 0) return;        // image lacks this modality
            if (g_h == 0) { g_h = h_i; g_w = w_i; return; }
            if (g_h == h_i && g_w == w_i) return;    // already matches
            did_mismatch = true;
            int64_t cur_area = (int64_t)g_h * g_w;
            int64_t new_area = (int64_t)h_i * w_i;
            if (new_area > cur_area) { g_h = h_i; g_w = w_i; }
        };

        bool mask_mm = false, depth_mm = false, normal_mm = false;
        if (!_mask_h_per.empty()) {
            // 1x1 mask is a known broadcast case (all-white / all-black). It
            // contributes nothing to the max-area picker, so we just skip it
            // and don't count it as a mismatch.
            int32_t h_i = _mask_h_per[i], w_i = _mask_w_per[i];
            if (!(h_i == 1 && w_i == 1)) {
                merge_max_area(h_i, w_i, g.mask_h, g.mask_w, mask_mm);
            }
        }
        if (!_depth_h_per.empty())
            merge_max_area(_depth_h_per[i],  _depth_w_per[i],  g.depth_h,  g.depth_w,  depth_mm);
        if (!_normal_h_per.empty())
            merge_max_area(_normal_h_per[i], _normal_w_per[i], g.normal_h, g.normal_w, normal_mm);

        if (mask_mm && !warned_mask) {
            std::fprintf(stderr, "%s %s\n", dmsg::word_warning.get(),
                format(dmsg::mask_shape_mismatch,
                       {g.width, g.height, g.mask_w, g.mask_h}).c_str());
            std::fflush(stderr);
            warned_mask = true;
        }
        if (depth_mm && !warned_depth) {
            std::fprintf(stderr, "%s %s\n", dmsg::word_warning.get(),
                format(dmsg::depth_shape_mismatch,
                       {g.width, g.height, g.depth_w, g.depth_h}).c_str());
            std::fflush(stderr);
            warned_depth = true;
        }
        if (normal_mm && !warned_normal) {
            std::fprintf(stderr, "%s %s\n", dmsg::word_warning.get(),
                format(dmsg::normal_shape_mismatch,
                       {g.width, g.height, g.normal_w, g.normal_h}).c_str());
            std::fflush(stderr);
            warned_normal = true;
        }

        g.indices.push_back(i);
    }

    std::vector<IndexGroup> out;
    out.reserve(by_shape.size());
    for (auto& kv : by_shape) {
        auto& g = kv.second;
        // Warp groups (K>1) sample the mask with the INPUT-image intrinsics
        // (warp_mask_wide_to_pinhole_kernel projects post-split pixels through
        // fx/fy/cx/cy into image-pixel space), then index the mask buffer at
        // its own resolution. If the mask buffer isn't at the input image
        // resolution, those image-space coords land in the wrong place -- and
        // when the mask is smaller than the image they fall out of bounds for
        // most pixels, zeroing the FOV mask and masking the lens out of the
        // loss. Pin the mask buffer to (height, width); per-image masks are
        // nearest-resized to this shape at decode / fill time. (Synth white
        // masks are already image-sized, so this is a no-op for them.)
        if (has_masks() && g.K > 1 && g.mask_h > 0) {
            g.mask_h = g.height;
            g.mask_w = g.width;
        }
        // Mask-only fallback: every mask in the group was 1x1 (or absent).
        // Settle the batch shape at 1x1 -- one byte per image, the per-pixel
        // kernel will read it as a scalar broadcast.
        if (has_masks() && g.mask_h == 0) { g.mask_h = 1; g.mask_w = 1; }
        out.emplace_back(std::move(g));
    }
    return out;
}


// ===========================================================================
// CPU mode — preload everything in parallel
// ===========================================================================
void DataManagerImpl::preload_cpu_cache() {
    int64_t N = (int64_t)_image_filenames.size();
    _rgb_cache.resize(N);
    if (has_masks())   _mask_cache.resize(N);
    if (has_depths())  _depth_cache.resize(N);
    if (has_normals()) _normal_cache.resize(N);

    // Pick a sensible worker count for the preload sweep — capped by the
    // number of images, so we don't spawn 32 threads to decode 4 images.
    int n_threads = (int)std::max(1u, std::thread::hardware_concurrency());
    n_threads = std::min(n_threads, (int)N);

    std::atomic<int64_t> next_idx{0};
    std::atomic<int64_t> done_count{0};
    std::mutex           print_mu;
    // A decode that throws must not escape its thread. Latch the first one,
    // abandon the sweep, and rethrow it from the constructor after the join.
    std::mutex           err_mu;
    std::exception_ptr   first_err;
    std::vector<std::thread> ts;
    ts.reserve(n_threads);
    for (int t = 0; t < n_threads; ++t) {
        ts.emplace_back([&]() {
          try {
            for (;;) {
                {
                    std::lock_guard<std::mutex> lk(err_mu);
                    if (first_err) return;
                }
                int64_t i = next_idx.fetch_add(1);
                if (i >= N) return;
                int W = _widths[i], H = _heights[i];

                // RGB.
                {
                    PixelDType dt = _rgb_dtype[i];
                    size_t bytes = (size_t)W * H * 3 * pixel_dtype_size(dt);
                    _rgb_cache[i].assign(bytes, 0);
                    decode_rgb_into(_image_filenames[i], H, W, dt, _rgb_cache[i].data());
                }
                // Per-image modality shape (may differ across images; group
                // uniformity was enforced at construction). Mask 1x1 stays
                // 1x1 in cache and is broadcast at batch-fill time.
                if (has_masks() && !_mask_filenames[i].empty()) {
                    int32_t mh = _mask_h_per[i], mw = _mask_w_per[i];
                    _mask_cache[i].assign((size_t)mw * mh, 0);
                    decode_mask_into(_mask_filenames[i], mh, mw,
                                     _cfg.flip_mask, _cfg.mask_boundary_offset,
                                     _mask_cache[i].data());
                } else if (has_masks() && _synth_white_mask[(size_t)i]) {
                    // Full-size all-ones mask matching the input image shape
                    // (see constructor note: 1x1 broadcast breaks the warp
                    // kernel's bounds check, leaving only the center pixel
                    // as 1 -> "front face is gray" symptom). The disk path
                    // memsets at H*W directly; here we mirror that into the
                    // per-image cache so fill_batch_from_cache's memcpy
                    // hits the equal-shape fast path.
                    int32_t mh = _mask_h_per[i], mw = _mask_w_per[i];
                    _mask_cache[i].assign((size_t)mw * mh, (uint8_t)1);
                }
                if (has_depths() && !_depth_filenames[i].empty()) {
                    PixelDType dt = _depth_dtype[i];
                    int32_t dh = _depth_h_per[i], dw = _depth_w_per[i];
                    size_t bytes = (size_t)dw * dh * pixel_dtype_size(dt);
                    _depth_cache[i].assign(bytes, 0);
                    decode_depth_into(_depth_filenames[i], dh, dw,
                                      dt, _depth_cache[i].data());
                }
                if (has_normals() && !_normal_filenames[i].empty()) {
                    int32_t nh = _normal_h_per[i], nw = _normal_w_per[i];
                    _normal_cache[i].assign((size_t)nw * nh * 3, 0);
                    decode_normal_into(_normal_filenames[i], nh, nw,
                                       _normal_cache[i].data());
                }

                int64_t d = done_count.fetch_add(1) + 1;
                {
                    std::lock_guard<std::mutex> lk(print_mu);
                    std::fprintf(stderr, "\r%s",
                                 format(dmsg::loading_images,
                                        {(long long)d, (long long)N}).c_str());
                    std::fflush(stderr);
                }
            }
          } catch (...) {
            std::lock_guard<std::mutex> lk(err_mu);
            if (!first_err) first_err = std::current_exception();
          }
        });
    }
    for (auto& th : ts) th.join();
    if (first_err) std::rethrow_exception(first_err);
    std::fprintf(stderr, "\n");
    std::fflush(stderr);
}


// ===========================================================================
// Batch allocation + camera params + cache gather
// ===========================================================================
void DataManagerImpl::allocate_batch(
    DecodedBatch& b,
    const IndexGroup& g,
    const std::vector<int32_t>& ds_indices)
{
    int B   = (int)ds_indices.size();
    int H   = g.height, W = g.width;
    int K   = g.K;
    int B_post = B * K;

    // POST-split camera setup: when K > 1, the engine sees PINHOLE sub-cameras
    // at (g.out_h, g.out_w), B*K of them. When K == 1, post == input.
    b.width  = g.out_w;
    b.height = g.out_h;
    b.num    = B_post;
    b.model  = (K > 1) ? CameraModelType::PINHOLE : g.model;
    b.distortion = (K > 1) ? CameraDistortionType::None : g.distortion;

    b.indices      = ds_indices;
    b.post_offsets.assign((size_t)B, 0);

    // Warp metadata so downstream (engine) knows how to dispatch the warp.
    b.input_width  = W;
    b.input_height = H;
    b.input_num    = B;
    b.K            = K;
    b.input_model  = g.model;
    b.input_distortion = g.distortion;
    b.face_passes  = g.passes;

    b.viewmats.assign((size_t)B_post * 16, 0.0f);
    if (K > 1) b.face_axes.assign((size_t)B_post * 9, 0.0f);
    else       b.face_axes.clear();
    b.intrins.assign((size_t)B_post * 4, 0.0f);
    b.dist_coeffs.assign((size_t)B_post * kCameraDistortionParams, 0.0f);

    // Per-INPUT intrins / dist_coeffs (needed by the wide warp kernel for
    // fisheye / equisolid). Allocate only when K > 1 and the source arrays
    // are present.
    const bool want_input = (K > 1 || g.redistort) && !_input_intrins.empty();
    if (want_input) {
        b.input_intrins.assign((size_t)B * 4, 0.0f);
        b.input_dist_coeffs.assign((size_t)B * kCameraDistortionParams, 0.0f);
    } else {
        b.input_intrins.clear();
        b.input_dist_coeffs.clear();
    }
    if (want_input && g.redistort) {
        b.input_source_models.assign((size_t)B, -1);
        b.input_source_params.assign((size_t)B * 16, 0.0f);
    } else {
        b.input_source_models.clear();
        b.input_source_params.clear();
    }

    // RGB: still allocated at INPUT shape -- the warp kernel reads the byte
    // staging buffer there. dtype follows the first image in the batch.
    PixelDType rgb_dt = _rgb_dtype.empty() ? PixelDType::UINT8 : _rgb_dtype[ds_indices[0]];
    b.rgb_dtype = rgb_dt;
    b.rgb_buffer.assign((size_t)B * H * W * 3 * pixel_dtype_size(rgb_dt), 0);

    // Modality shapes are PER-GROUP -- different (W,H) groups may have
    // different mask / depth / normal resolutions. allocate_batch reads
    // them off the IndexGroup the batch was sampled from.
    if (has_masks() && g.mask_h > 0 && g.mask_w > 0) {
        b.mask_height = g.mask_h;
        b.mask_width  = g.mask_w;
        b.mask_buffer.assign((size_t)B * g.mask_h * g.mask_w, 0);
    } else {
        b.mask_buffer.clear();
        b.mask_height = b.mask_width = 0;
    }

    if (has_depths() && g.depth_h > 0 && g.depth_w > 0) {
        PixelDType d_dt = PixelDType::UINT16;
        b.depth_dtype  = d_dt;
        b.depth_height = g.depth_h;
        b.depth_width  = g.depth_w;
        b.depth_buffer.assign((size_t)B * g.depth_h * g.depth_w * pixel_dtype_size(d_dt), 0);
    } else {
        b.depth_buffer.clear();
        b.depth_height = b.depth_width = 0;
    }

    if (has_normals() && g.normal_h > 0 && g.normal_w > 0) {
        b.normal_dtype  = PixelDType::UINT8;
        b.normal_height = g.normal_h;
        b.normal_width  = g.normal_w;
        b.normal_buffer.assign((size_t)B * g.normal_h * g.normal_w * 3, 0);
    } else {
        b.normal_buffer.clear();
        b.normal_height = b.normal_width = 0;
    }

    fill_camera_params(b);
}

void DataManagerImpl::fill_camera_params(DecodedBatch& b) {
    // b.num is the POST-split count (= input_num * K). For each input camera
    // j, copy its K post-split rows out of the impl-level arrays. Stamps the
    // batch's post_offsets so downstream can build bilagrid_cam_indices.
    int B_in = b.input_num;
    int K    = b.K;
    bool have_in = !b.input_intrins.empty();
    for (int j = 0; j < B_in; ++j) {
        int32_t i_in = b.indices[j];
        int32_t off  = _post_offsets[i_in];
        b.post_offsets[j] = off;
        std::memcpy(&b.viewmats[(size_t)j * K * 16],
                    &_viewmats[(size_t)off * 16],
                    (size_t)K * 16 * sizeof(float));
        std::memcpy(&b.intrins[(size_t)j * K * 4],
                    &_intrins[(size_t)off * 4],
                    (size_t)K * 4 * sizeof(float));
        std::memcpy(&b.dist_coeffs[(size_t)j * K * kCameraDistortionParams],
                    &_dist_coeffs[(size_t)off * kCameraDistortionParams],
                    (size_t)K * kCameraDistortionParams * sizeof(float));
        if (!b.face_axes.empty())
            std::memcpy(&b.face_axes[(size_t)j * K * 9],
                        &_face_axes[(size_t)off * 9],
                        (size_t)K * 9 * sizeof(float));
        if (have_in) {
            std::memcpy(&b.input_intrins[(size_t)j * 4],
                        &_input_intrins[(size_t)i_in * 4],
                        4 * sizeof(float));
            std::memcpy(&b.input_dist_coeffs[(size_t)j * kCameraDistortionParams],
                        &_input_dist_coeffs[(size_t)i_in * kCameraDistortionParams],
                        kCameraDistortionParams * sizeof(float));
            if (!b.input_source_models.empty()) {
                b.input_source_models[j] = _redistort_models[i_in];
                std::memcpy(&b.input_source_params[(size_t)j * 16],
                            &_redistort_params[(size_t)i_in * 16],
                            16 * sizeof(float));
            }
        }
    }
}

void DataManagerImpl::fill_batch_from_cache(DecodedBatch& b) {
    // RGB / mask buffers stay at INPUT shape -- the engine's warp kernel
    // reads them at input shape and writes warped float at post-split shape.
    int B = b.input_num, H = b.input_height, W = b.input_width;
    size_t rgb_row    = (size_t)H * W * 3 * pixel_dtype_size(b.rgb_dtype);
    size_t mask_row   = (size_t)b.mask_height   * b.mask_width;
    size_t depth_row  = (size_t)b.depth_height  * b.depth_width * pixel_dtype_size(b.depth_dtype);
    size_t normal_row = (size_t)b.normal_height * b.normal_width * 3;

    for (int j = 0; j < B; ++j) {
        int32_t i = b.indices[j];
        std::memcpy(b.rgb_buffer.data() + (size_t)j * rgb_row,
                    _rgb_cache[i].data(), rgb_row);

        // Mask: cache row at per-image on-disk shape, batch slot at group
        // shape. If equal, memcpy. If cache is 1x1, broadcast (memset).
        // Otherwise nearest-resize up to the group shape (binary preserving).
        if (!b.mask_buffer.empty() && !_mask_cache.empty() && !_mask_cache[i].empty()) {
            uint8_t* dst_row = b.mask_buffer.data() + (size_t)j * mask_row;
            int32_t  ih = _mask_h_per[i], iw = _mask_w_per[i];
            if (ih == 1 && iw == 1) {
                std::memset(dst_row, _mask_cache[i][0] ? 1 : 0, mask_row);
            } else if (ih == b.mask_height && iw == b.mask_width) {
                std::memcpy(dst_row, _mask_cache[i].data(), mask_row);
            } else {
                cpu_nearest_resize_u8(_mask_cache[i].data(), ih, iw,
                                      dst_row, b.mask_height, b.mask_width);
            }
        }

        // Depth: bilinear resize when per-image shape != group shape.
        if (!b.depth_buffer.empty() && !_depth_cache.empty() && !_depth_cache[i].empty()) {
            uint8_t* dst_row = b.depth_buffer.data() + (size_t)j * depth_row;
            int32_t  ih = _depth_h_per[i], iw = _depth_w_per[i];
            if (ih == b.depth_height && iw == b.depth_width) {
                std::memcpy(dst_row, _depth_cache[i].data(), depth_row);
            } else {
                // 16-bit, single channel.
                cpu_bilinear_resize<stbi_us, 1>(
                    (const stbi_us*)_depth_cache[i].data(), ih, iw,
                    (stbi_us*)dst_row, b.depth_height, b.depth_width);
            }
        }

        // Normal: bilinear per channel.
        if (!b.normal_buffer.empty() && !_normal_cache.empty() && !_normal_cache[i].empty()) {
            uint8_t* dst_row = b.normal_buffer.data() + (size_t)j * normal_row;
            int32_t  ih = _normal_h_per[i], iw = _normal_w_per[i];
            if (ih == b.normal_height && iw == b.normal_width) {
                std::memcpy(dst_row, _normal_cache[i].data(), normal_row);
            } else {
                cpu_bilinear_resize<stbi_uc, 3>(
                    _normal_cache[i].data(), ih, iw,
                    dst_row, b.normal_height, b.normal_width);
            }
        }
    }
}


// ===========================================================================
// CPU-mode batch fetch
// ===========================================================================
DecodedBatch& DataManagerImpl::next_batch_cpu(
    DecodedBatch& slot,
    std::vector<IndexGroup>& groups,
    GroupSampler& sampler,
    int batch_size)
{
    std::lock_guard<std::mutex> lk(_sampling_mu);
    if (sampler.empty()) {
        throw std::runtime_error("DataManager: no indices configured for this split");
    }
    size_t gi = sampler(_rng);
    auto& g = groups[gi];

    int eff_bs = std::min(batch_size, (int)g.indices.size());
    eff_bs = std::max(eff_bs, 1);
    std::vector<int32_t> picks;
    picks.reserve(eff_bs);
    for (int k = 0; k < eff_bs; ++k) {
        if (g.cursor >= g.indices.size()) g.rewind(_rng, /*eval=*/false);
        picks.push_back(g.indices[g.cursor++]);
    }

    allocate_batch(slot, g, picks);
    fill_batch_from_cache(slot);
    slot.build_views();
    return slot;
}


// ===========================================================================
// Deterministic epoch schedule (training)
// ===========================================================================
void DataManagerImpl::build_train_schedule_locked() {
    _train_schedule.clear();
    _train_sched_cursor = 0;
    if (_train_groups.empty()) return;

    // Target images per optimizer step. Derived (on the Python side) from
    // max_batch_per_epoch as round(N / max_batch_per_epoch), so a full pass
    // over the schedule is ~max_batch_per_epoch steps regardless of how the
    // images are distributed across resolution groups. B == 1 => one image
    // per step (no packing, no grad accumulator; FPBO path).
    const int B = std::max(1, _cfg.train_batch_size);

    // Fresh shuffle of every group for this epoch.
    for (auto& g : _train_groups)
        std::shuffle(g.indices.begin(), g.indices.end(), _rng);

    // Full B-image chunks become their own single-sub-batch (homogeneous)
    // steps. Sub-B remainders are collected for cross-group packing; groups
    // that need the warp path -- K > 1, and K == 1 re-distort -- are never
    // packed with other resolutions (the engine's heterogeneous accumulation
    // path is non-warp only, and would take their images unresampled), so
    // their remainder is emitted as its own step.
    std::vector<SubBatchSpec> remainders;
    for (size_t gi = 0; gi < _train_groups.size(); ++gi) {
        const auto& idx = _train_groups[gi].indices;
        const size_t n  = idx.size();
        size_t off = 0;
        for (; off + (size_t)B <= n; off += (size_t)B) {
            SubBatchSpec s;
            s.group = (int32_t)gi;
            // Indexed push_back rather than assign(iter, iter): the latter's
            // bulk memmove trips GCC 14's -Wnonnull on the zero-size edge case.
            s.picks.reserve((size_t)B);
            for (size_t t = off; t < off + (size_t)B; ++t) s.picks.push_back(idx[t]);
            _train_schedule.push_back(StepSpec{ std::move(s) });
        }
        if (off < n) {  // remainder of size n - off (in [1, B-1])
            SubBatchSpec s;
            s.group = (int32_t)gi;
            s.picks.reserve(n - off);
            for (size_t t = off; t < n; ++t) s.picks.push_back(idx[t]);
            if (_train_groups[gi].K > 1 || _train_groups[gi].redistort)
                _train_schedule.push_back(StepSpec{ std::move(s) });
            else
                remainders.push_back(std::move(s));
        }
    }

    // First-fit-decreasing bin packing of the non-warp remainders into steps
    // of at most B images. Packing only combines fragments that fit, so large
    // fragments end up alone (a homogeneous step) and only genuinely small
    // fragments share a heterogeneous step -- minimizing the number of steps
    // that need the (grad-accumulator) non-fused path.
    std::sort(remainders.begin(), remainders.end(),
              [](const SubBatchSpec& a, const SubBatchSpec& b) {
                  return a.picks.size() > b.picks.size();
              });
    std::vector<StepSpec> bins;
    std::vector<size_t>   fills;
    for (auto& frag : remainders) {
        const size_t fs = frag.picks.size();
        int placed = -1;
        for (size_t bi = 0; bi < bins.size(); ++bi) {
            if (fills[bi] + fs <= (size_t)B) { placed = (int)bi; break; }
        }
        if (placed < 0) {
            bins.emplace_back();
            fills.push_back(0);
            placed = (int)bins.size() - 1;
        }
        bins[placed].push_back(std::move(frag));
        fills[placed] += fs;
    }
    for (auto& bin : bins)
        _train_schedule.push_back(std::move(bin));

    // Interleave step order so full-chunk and mixed steps don't cluster.
    std::shuffle(_train_schedule.begin(), _train_schedule.end(), _rng);
}

StepSpec DataManagerImpl::next_train_step_spec() {
    std::lock_guard<std::mutex> lk(_sampling_mu);
    if (_train_groups.empty())
        throw std::runtime_error("DataManager: no training indices configured");
    if (_train_sched_cursor >= _train_schedule.size())
        build_train_schedule_locked();   // next epoch
    return _train_schedule[_train_sched_cursor++];
}

const TrainStep& DataManagerImpl::next_step_cpu() {
    // Draw the next scheduled step (locks internally), then materialize its
    // sub-batches from the CPU cache. Materialization touches only read-only
    // caches and the single-consumer _cpu_train_step slot, so it runs outside
    // the sampling lock.
    StepSpec spec = next_train_step_spec();
    TrainStep& st = _cpu_train_step;
    st.subs.resize(spec.size());
    for (size_t i = 0; i < spec.size(); ++i) {
        if (!st.subs[i]) st.subs[i] = std::make_shared<DecodedBatch>();
        DecodedBatch& b = *st.subs[i];
        allocate_batch(b, _train_groups[spec[i].group], spec[i].picks);
        fill_batch_from_cache(b);
        b.build_views();
    }
    return st;
}


// ===========================================================================
// DISK mode pipeline
// ===========================================================================
void DataManagerImpl::start_disk_pipeline() {
    int prefetch = std::max(1, _cfg.prefetch_batches);
    // Job queues are sized generously — each batch enqueues B jobs per
    // modality so cap = prefetch * batch_size keeps the scheduler from
    // running ahead of the consumer.
    int job_cap = std::max(1, prefetch * std::max(_cfg.train_batch_size, _cfg.val_batch_size));
    _q_rgb    = std::make_unique<BoundedQueue<DecodeJob>>((size_t)job_cap);
    _q_mask   = std::make_unique<BoundedQueue<DecodeJob>>((size_t)job_cap);
    _q_depth  = std::make_unique<BoundedQueue<DecodeJob>>((size_t)job_cap);
    _q_normal = std::make_unique<BoundedQueue<DecodeJob>>((size_t)job_cap);
    _q_ready_train = std::make_unique<BoundedQueue<std::shared_ptr<TrainStep>>>((size_t)prefetch);
    _q_ready_val   = std::make_unique<BoundedQueue<std::shared_ptr<DecodedBatch>>>((size_t)prefetch);

    int n_rgb_workers = std::max(1, _cfg.workers_rgb);
    for (int t = 0; t < n_rgb_workers; ++t)
        _workers.emplace_back(&DataManagerImpl::worker_loop_rgb, this);

    if (has_masks()) {
        int n_mask = std::max(1, n_rgb_workers / 2);
        for (int t = 0; t < n_mask; ++t)
            _workers.emplace_back(&DataManagerImpl::worker_loop_mask, this);
    }

    if (has_depths()) {
        int n = std::max(1, _cfg.workers_depth);
        for (int t = 0; t < n; ++t)
            _workers.emplace_back(&DataManagerImpl::worker_loop_depth, this);
    }
    if (has_normals()) {
        int n = std::max(1, _cfg.workers_normal);
        for (int t = 0; t < n; ++t)
            _workers.emplace_back(&DataManagerImpl::worker_loop_normal, this);
    }

    _scheduler = std::thread(&DataManagerImpl::scheduler_loop, this);
    _disk_started = true;
}

void DataManagerImpl::stop_disk_pipeline() {
    _stop.store(true);
    _fault_cv.notify_all();          // parked workers must leave before join
    _q_rgb->close();
    _q_mask->close();
    _q_depth->close();
    _q_normal->close();
    _q_ready_train->close();
    _q_ready_val->close();
    if (_scheduler.joinable()) _scheduler.join();
    for (auto& th : _workers) if (th.joinable()) th.join();
    _workers.clear();
}


// A decode threw: the first worker to arrive latches the message, the rest
// wait behind it, and everyone re-runs the same job once the front end says
// retry. Returning false means give up -- the caller must not publish.
bool DataManagerImpl::decode_or_park(const std::function<void()>& decode) {
    for (;;) {
        try {
            decode();
            return true;
        } catch (const std::exception& e) {
            std::unique_lock<std::mutex> lk(_fault_mu);
            if (_fault_give_up || _stop.load()) return false;
            if (!_fault_parked) {
                _fault_parked = true;
                _fault_msg    = e.what();
            }
            _fault_cv.wait(lk, [&]{
                return !_fault_parked || _fault_give_up || _stop.load();
            });
            if (_fault_give_up || _stop.load()) return false;
        }
    }
}

std::string DataManagerImpl::data_error() const {
    std::lock_guard<std::mutex> lk(const_cast<std::mutex&>(_fault_mu));
    return _fault_parked ? _fault_msg : std::string();
}

void DataManagerImpl::resolve_data_error(bool retry) {
    {
        std::lock_guard<std::mutex> lk(_fault_mu);
        _fault_parked = false;
        if (!retry) _fault_give_up = true;
    }
    _fault_cv.notify_all();
    // Nothing will finish the parked batches, so let the consumer's pop fail
    // instead of blocking on a pipeline that has stopped producing.
    if (!retry) {
        _q_ready_train->close();
        _q_ready_val->close();
    }
}


void DataManagerImpl::worker_loop_rgb() {
    DecodeJob job;
    while (_q_rgb->pop(job)) {
        if (_stop.load()) return;
        DecodedBatch& b = *job.batch;
        // RGB byte buffer is at INPUT shape; warp (if any) happens on engine side.
        int H = b.input_height, W = b.input_width;
        size_t row = (size_t)H * W * 3 * pixel_dtype_size(b.rgb_dtype);
        uint8_t* dst = b.rgb_buffer.data() + (size_t)job.slot * row;
        if (!decode_or_park([&]{
                decode_rgb_into(_image_filenames[job.ds_index], H, W,
                                b.rgb_dtype, dst); }))
            return;
        publish_if_done(job);
    }
}

void DataManagerImpl::worker_loop_mask() {
    DecodeJob job;
    while (_q_mask->pop(job)) {
        if (_stop.load()) return;
        DecodedBatch& b = *job.batch;
        int H = b.mask_height, W = b.mask_width;
        size_t row = (size_t)H * W;
        uint8_t* dst = b.mask_buffer.data() + (size_t)job.slot * row;
        if (_synth_white_mask.empty() ||
            !_synth_white_mask[(size_t)job.ds_index]) {
            if (!decode_or_park([&]{
                    decode_mask_into(_mask_filenames[job.ds_index], H, W,
                                     _cfg.flip_mask, _cfg.mask_boundary_offset,
                                     dst); }))
                return;
        } else {
            // Synthesized all-white: skip disk read, fill the slot directly.
            std::memset(dst, 1, (size_t)H * W);
        }
        publish_if_done(job);
    }
}

void DataManagerImpl::worker_loop_depth() {
    DecodeJob job;
    while (_q_depth->pop(job)) {
        if (_stop.load()) return;
        DecodedBatch& b = *job.batch;
        int H = b.depth_height, W = b.depth_width;
        size_t row = (size_t)H * W * pixel_dtype_size(b.depth_dtype);
        uint8_t* dst = b.depth_buffer.data() + (size_t)job.slot * row;
        if (!decode_or_park([&]{
                decode_depth_into(_depth_filenames[job.ds_index], H, W,
                                  b.depth_dtype, dst); }))
            return;
        publish_if_done(job);
    }
}

void DataManagerImpl::worker_loop_normal() {
    DecodeJob job;
    while (_q_normal->pop(job)) {
        if (_stop.load()) return;
        DecodedBatch& b = *job.batch;
        int H = b.normal_height, W = b.normal_width;
        size_t row = (size_t)H * W * 3;
        uint8_t* dst = b.normal_buffer.data() + (size_t)job.slot * row;
        if (!decode_or_park([&]{
                decode_normal_into(_normal_filenames[job.ds_index], H, W, dst); }))
            return;
        publish_if_done(job);
    }
}


// Scheduler: produces decoded-batch shells, hands jobs to per-modality
// queues, and routes the finished batch to the right ready queue.
void DataManagerImpl::scheduler_loop() {
    while (!_stop.load()) {
        // Alternate fill order: prefer training to validation, refill val
        // when ready_val is near-empty. (Simple heuristic.)
        if (!_train_groups.empty()) {
            StepSpec spec = next_train_step_spec();   // locks _sampling_mu
            enqueue_step(spec, *_q_ready_train);
        }
        if (!_val_groups.empty()) {
            std::unique_lock<std::mutex> lk(_sampling_mu);
            size_t gi = _val_sampler(_rng);
            auto& g = _val_groups[gi];
            lk.unlock();
            enqueue_batch(g, _cfg.val_batch_size, *_q_ready_val);
        }
    }
}

void DataManagerImpl::enqueue_batch(
    IndexGroup& group, int batch_size,
    BoundedQueue<std::shared_ptr<DecodedBatch>>& ready_q)
{
    int eff_bs = std::min(batch_size, (int)group.indices.size());
    eff_bs = std::max(eff_bs, 1);

    std::vector<int32_t> picks;
    picks.reserve(eff_bs);
    {
        std::lock_guard<std::mutex> lk(_sampling_mu);
        for (int k = 0; k < eff_bs; ++k) {
            if (group.cursor >= group.indices.size()) group.rewind(_rng, /*eval=*/false);
            picks.push_back(group.indices[group.cursor++]);
        }
    }

    auto batch = std::make_shared<DecodedBatch>();
    allocate_batch(*batch, group, picks);

    // Number of decode jobs per image — RGB always, plus each enabled modality.
    int per_image = 1;
    if (has_masks())   per_image += 1;
    if (has_depths())  per_image += 1;
    if (has_normals()) per_image += 1;
    auto remaining = std::make_shared<std::atomic<int>>(eff_bs * per_image);

    // Helper: a single per-job completion decrement. When the counter flips
    // to 0 (returns 1 before the subtract), publish the batch ourselves —
    // no scheduler wait, no promise/future. Backpressure comes from the
    // bounded ready queue.
    auto try_publish = [&ready_q, batch, remaining]() {
        if (remaining->fetch_sub(1) == 1) {
            batch->build_views();
            ready_q.push(batch);
        }
    };

    // Stage all jobs into local lists first, so each modality queue is
    // pushed in one tight burst (more chance for workers to pick them up
    // in parallel) and we don't interleave producer-side contention with
    // worker wakeups.
    std::vector<DecodeJob> rgb_jobs, mask_jobs, depth_jobs, normal_jobs;
    rgb_jobs.reserve(eff_bs);
    if (has_masks())   mask_jobs.reserve(eff_bs);
    if (has_depths())  depth_jobs.reserve(eff_bs);
    if (has_normals()) normal_jobs.reserve(eff_bs);

    for (int j = 0; j < eff_bs; ++j) {
        int32_t i = picks[j];

        DecodeJob jb;
        jb.slot      = j;
        jb.ds_index  = i;
        jb.batch     = batch;
        jb.remaining = remaining;
        jb.ready_q   = &ready_q;
        jb.batch_id  = _next_batch_id.fetch_add(1);

        rgb_jobs.push_back(jb);
        if (has_masks()) {
            bool has_real  = !_mask_filenames[i].empty();
            bool is_synth  = !_synth_white_mask.empty() &&
                             _synth_white_mask[(size_t)i];
            if (has_real || is_synth) mask_jobs.push_back(jb);
            else                       try_publish();  // truly absent
        }
        if (has_depths()) {
            if (!_depth_filenames[i].empty()) depth_jobs.push_back(jb);
            else                               try_publish();
        }
        if (has_normals()) {
            if (!_normal_filenames[i].empty()) normal_jobs.push_back(jb);
            else                                try_publish();
        }
    }

    // Hand off to the workers. The bounded ready queue (cap =
    // prefetch_batches) provides backpressure: once `prefetch_batches`
    // batches are decoded but unconsumed, the publishing worker blocks
    // here, while other workers keep draining their modality queues.
    // Modality job queues are also bounded (cap = prefetch_batches *
    // train_batch_size), so this scheduler call returns as soon as those
    // queues have room — and the next batch's jobs start enqueueing
    // immediately. This is what keeps all N workers busy when batch
    // size is 1.
    for (auto& jb : rgb_jobs)    if (!_q_rgb->push(std::move(jb))) return;
    for (auto& jb : mask_jobs)   if (!_q_mask->push(std::move(jb))) return;
    for (auto& jb : depth_jobs)  if (!_q_depth->push(std::move(jb))) return;
    for (auto& jb : normal_jobs) if (!_q_normal->push(std::move(jb))) return;
}


void DataManagerImpl::publish_if_done(DecodeJob& job) {
    // fetch_sub returns the value BEFORE the subtract, so == 1 means this call
    // took the counter to zero: we own publishing.
    if (job.remaining->fetch_sub(1) != 1) return;
    if (job.step) {
        for (auto& sub : job.step->subs) sub->build_views();
        job.step_q->push(job.step);
    } else {
        job.batch->build_views();
        job.ready_q->push(job.batch);
    }
}


// Disk-mode: allocate shells + enqueue decode jobs for a whole training step.
// A single shared `remaining` counter spans every sub-batch's jobs; the worker
// that drives it to zero publishes the TrainStep. Mirrors enqueue_batch's
// staging discipline (RGB is always present, so `remaining` can never reach
// zero mid-staging before the RGB jobs are pushed).
void DataManagerImpl::enqueue_step(
    const StepSpec& spec,
    BoundedQueue<std::shared_ptr<TrainStep>>& step_q)
{
    auto stepp = std::make_shared<TrainStep>();
    stepp->subs.resize(spec.size());

    int per_image = 1;
    if (has_masks())   per_image += 1;
    if (has_depths())  per_image += 1;
    if (has_normals()) per_image += 1;

    int total_jobs = 0;
    for (const auto& s : spec) total_jobs += (int)s.picks.size() * per_image;
    auto remaining = std::make_shared<std::atomic<int>>(total_jobs);

    // Allocate every sub-batch shell up front so stepp->subs is complete
    // before any job runs.
    for (size_t i = 0; i < spec.size(); ++i) {
        stepp->subs[i] = std::make_shared<DecodedBatch>();
        allocate_batch(*stepp->subs[i], _train_groups[spec[i].group], spec[i].picks);
    }

    auto decrement_absent = [&]() {
        DecodeJob j;
        j.remaining = remaining;
        j.step      = stepp;
        j.step_q    = &step_q;
        publish_if_done(j);
    };

    std::vector<DecodeJob> rgb_jobs, mask_jobs, depth_jobs, normal_jobs;
    for (size_t i = 0; i < spec.size(); ++i) {
        auto& sub = stepp->subs[i];
        const auto& picks = spec[i].picks;
        for (int j = 0; j < (int)picks.size(); ++j) {
            int32_t idx = picks[j];

            DecodeJob jb;
            jb.slot      = j;
            jb.ds_index  = idx;
            jb.batch     = sub;
            jb.remaining = remaining;
            jb.step      = stepp;
            jb.step_q    = &step_q;
            jb.batch_id  = _next_batch_id.fetch_add(1);

            rgb_jobs.push_back(jb);
            if (has_masks()) {
                bool has_real = !_mask_filenames[idx].empty();
                bool is_synth = !_synth_white_mask.empty() &&
                                _synth_white_mask[(size_t)idx];
                if (has_real || is_synth) mask_jobs.push_back(jb);
                else                       decrement_absent();
            }
            if (has_depths()) {
                if (!_depth_filenames[idx].empty()) depth_jobs.push_back(jb);
                else                                 decrement_absent();
            }
            if (has_normals()) {
                if (!_normal_filenames[idx].empty()) normal_jobs.push_back(jb);
                else                                  decrement_absent();
            }
        }
    }

    for (auto& jb : rgb_jobs)    if (!_q_rgb->push(std::move(jb))) return;
    for (auto& jb : mask_jobs)   if (!_q_mask->push(std::move(jb))) return;
    for (auto& jb : depth_jobs)  if (!_q_depth->push(std::move(jb))) return;
    for (auto& jb : normal_jobs) if (!_q_normal->push(std::move(jb))) return;
}


// ===========================================================================
// CPU vs DISK dispatch — public-facing fetch
// ===========================================================================
const TrainStep& DataManagerImpl::next_train_step() {
    if (_cfg.cache_mode == CacheMode::CPU) {
        return next_step_cpu();
    }
    std::shared_ptr<TrainStep> s;
    for (;;) {
        auto r = _q_ready_train->pop_for(s, std::chrono::milliseconds(100));
        if (r == BoundedQueue<std::shared_ptr<TrainStep>>::Pop::Ok) break;
        std::string err = data_error();
        if (!err.empty()) throw DataDecodeError(err);
        if (r == BoundedQueue<std::shared_ptr<TrainStep>>::Pop::Closed)
            throw std::runtime_error("DataManager: train prefetch queue closed");
    }
    _last_train_step_held = s;
    return *_last_train_step_held;
}

const DecodedBatch& DataManagerImpl::next_train_batch() {
    // Convenience wrapper: valid only when the schedule yields single
    // sub-batch steps (the common single-resolution / B==1 case).
    const TrainStep& s = next_train_step();
    if (s.subs.empty())
        throw std::runtime_error("DataManager: empty training step");
    return *s.subs.front();
}

const DecodedBatch* DataManagerImpl::next_val_batch() {
    if (!has_val()) return nullptr;
    if (_cfg.cache_mode == CacheMode::CPU) {
        return &next_batch_cpu(_cpu_val_batch, _val_groups,
                               _val_sampler, _cfg.val_batch_size);
    }
    std::shared_ptr<DecodedBatch> b;
    for (;;) {
        auto r = _q_ready_val->pop_for(b, std::chrono::milliseconds(100));
        if (r == BoundedQueue<std::shared_ptr<DecodedBatch>>::Pop::Ok) break;
        std::string err = data_error();
        if (!err.empty()) throw DataDecodeError(err);
        if (r == BoundedQueue<std::shared_ptr<DecodedBatch>>::Pop::Closed)
            throw std::runtime_error("DataManager: val prefetch queue closed");
    }
    _last_val_held = b;
    return _last_val_held.get();
}

void DataManagerImpl::fetch_one(int32_t index, DecodedBatch& out) {
    const int64_t N = (int64_t)_image_filenames.size();
    if (index < 0 || (int64_t)index >= N)
        throw std::runtime_error("DataManager::fetch_one: index out of range");

    // A group of one, built by the same code the training groups come from, so
    // the batch gets the shapes and the K a training batch over this image
    // would get.
    const std::vector<int32_t> one{index};
    std::vector<IndexGroup> g = build_index_groups_member(one);
    allocate_batch(out, g.front(), one);

    if (_cfg.cache_mode == CacheMode::CPU) {
        fill_batch_from_cache(out);
    } else {
        decode_rgb_into(_image_filenames[index], out.input_height,
                        out.input_width, out.rgb_dtype, out.rgb_buffer.data(),
                        /*decode_threads=*/0);
        if (!out.mask_buffer.empty()) {
            bool synth = !_synth_white_mask.empty() &&
                         _synth_white_mask[(size_t)index];
            if (synth) {
                std::memset(out.mask_buffer.data(), 1, out.mask_buffer.size());
            } else if (!_mask_filenames.empty() &&
                       !_mask_filenames[index].empty()) {
                decode_mask_into(_mask_filenames[index], out.mask_height,
                                 out.mask_width, _cfg.flip_mask,
                                 _cfg.mask_boundary_offset,
                                 out.mask_buffer.data());
            }
            // Neither: the row stays zero, which is what the training path
            // leaves for an image with no mask of its own.
        }
        if (!out.depth_buffer.empty() && !_depth_filenames[index].empty())
            decode_depth_into(_depth_filenames[index], out.depth_height,
                              out.depth_width, out.depth_dtype,
                              out.depth_buffer.data());
        if (!out.normal_buffer.empty() && !_normal_filenames[index].empty())
            decode_normal_into(_normal_filenames[index], out.normal_height,
                               out.normal_width, out.normal_buffer.data());
    }
    out.build_views();
}


// ===========================================================================
// Public DataManager thin facade
// ===========================================================================
DataManager::DataManager(
    DataManagerConfig          config,
    std::vector<int32_t>       camera_models,
    std::vector<int32_t>       camera_distortions,
    std::vector<std::string>   image_filenames,
    std::vector<std::string>   mask_filenames,
    std::vector<std::string>   depth_filenames,
    std::vector<std::string>   normal_filenames,
    std::vector<int32_t>       widths,
    std::vector<int32_t>       heights,
    std::vector<int32_t>       K_per_camera,
    std::vector<int32_t>       post_offsets,
    std::vector<float>         viewmats,
    std::vector<float>         intrins,
    std::vector<float>         dist_coeffs,
    std::vector<int32_t>       post_widths,
    std::vector<int32_t>       post_heights,
    std::vector<float>         face_axes,
    std::vector<float>         input_intrins,
    std::vector<float>         input_dist_coeffs,
    std::vector<int32_t>       redistort_models,
    std::vector<float>         redistort_params,
    std::vector<int32_t>       train_indices,
    std::vector<int32_t>       val_indices)
{
    _impl = std::make_unique<DataManagerImpl>(
        std::move(config), std::move(camera_models),
        std::move(camera_distortions),
        std::move(image_filenames), std::move(mask_filenames),
        std::move(depth_filenames), std::move(normal_filenames),
        std::move(widths), std::move(heights),
        std::move(K_per_camera), std::move(post_offsets),
        std::move(viewmats), std::move(intrins), std::move(dist_coeffs),
        std::move(post_widths), std::move(post_heights), std::move(face_axes),
        std::move(input_intrins), std::move(input_dist_coeffs),
        std::move(redistort_models), std::move(redistort_params),
        std::move(train_indices), std::move(val_indices));
}

DataManager::~DataManager() = default;

const TrainStep&    DataManager::next_train_step()         { return _impl->next_train_step(); }
const DecodedBatch& DataManager::next_train_batch()        { return _impl->next_train_batch(); }
const DecodedBatch* DataManager::next_val_batch()          { return _impl->next_val_batch(); }
void      DataManager::fetch_one(int32_t i, DecodedBatch& o) { _impl->fetch_one(i, o); }
std::string DataManager::data_error() const { return _impl->data_error(); }
void      DataManager::resolve_data_error(bool r) { _impl->resolve_data_error(r); }
int64_t   DataManager::num_train()      const              { return _impl->num_train(); }
int64_t   DataManager::num_val()        const              { return _impl->num_val(); }
bool      DataManager::has_val()        const              { return _impl->has_val(); }
CacheMode DataManager::cache_mode()     const              { return _impl->cache_mode(); }
bool      DataManager::has_masks()      const              { return _impl->has_masks(); }
bool      DataManager::has_depths()     const              { return _impl->has_depths(); }
bool      DataManager::has_normals()    const              { return _impl->has_normals(); }
int64_t   DataManager::max_input_batch_size() const         { return _impl->max_input_batch_size(); }
int       DataManager::max_face_passes() const              { return _impl->max_face_passes(); }

// Progress.cpp -- see Progress.h.

#include "sfm/core/Progress.h"

#include "sfm/core/Model.h"

#include "external/stb_image_write.h"

#include "sfm/core/Matches.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace sfm {
namespace progress {

namespace {

using Clock = std::chrono::steady_clock;

// Long enough that a fast mapper does not spend its time serializing, short
// enough that a screen looks live. Registration on a small capture is
// milliseconds apart, so without this the file would be rewritten thousands of
// times a second.
constexpr double kInterval = 1.5;

// A capture with 700k verified pairs would append 1.4 GB here, beside the
// matches.bin that is the actual output. Past this the file simply stops
// growing: a reader already treats what is not there as a pair it cannot draw.
constexpr uint64_t kLiveCap = 256ull << 20;

// A thumbnail on its way to disk: already downscaled, so a queued one is
// ~1 MB rather than the working copy it came from.
struct ThumbJob {
    fs::path dst;
    std::vector<uint8_t> rgb;
    int w = 0, h = 0;
};

// A JPEG per image cost a sixth of the extraction stage on the consumer
// thread, which is the one the GPU runs on. The bounded queue drops its oldest
// rather than stall it: without a thumbnail a reel decodes the source frame.
class ThumbWriter {
public:
    ~ThumbWriter() { stop(); }

    void push(ThumbJob j) {
        std::unique_lock<std::mutex> lk(mu_);
        if (!worker_.joinable()) {
            quit_ = false;
            worker_ = std::thread([this] { run(); });
        }
        while (q_.size() >= kQueue) q_.pop_front();
        q_.push_back(std::move(j));
        lk.unlock();
        cv_.notify_one();
    }

    // Everything pushed so far is on disk when this returns.
    void drain() {
        std::unique_lock<std::mutex> lk(mu_);
        if (!worker_.joinable()) return;
        idle_.wait(lk, [this] { return q_.empty() && !busy_; });
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (!worker_.joinable()) return;
            quit_ = true;
        }
        cv_.notify_all();
        worker_.join();
        std::lock_guard<std::mutex> lk(mu_);
        q_.clear();
    }

private:
    static constexpr size_t kQueue = 8;

    void run() {
        fs::path made;   // last directory created, which every stem but the
        std::error_code ec;   // first of a folder shares
        for (;;) {
            ThumbJob j;
            {
                std::unique_lock<std::mutex> lk(mu_);
                cv_.wait(lk, [this] { return quit_ || !q_.empty(); });
                if (q_.empty()) return;   // quit_, with nothing left
                j = std::move(q_.front());
                q_.pop_front();
                busy_ = true;
            }
            const fs::path parent = j.dst.parent_path();
            if (parent != made) {
                fs::create_directories(parent, ec);
                made = parent;
            }
            const fs::path tmp = fs::path(j.dst) += ".tmp";
            if (stbi_write_jpg(tmp.string().c_str(), j.w, j.h, 3, j.rgb.data(), 88))
                fs::rename(tmp, j.dst, ec);
            if (ec) fs::remove(tmp, ec);
            {
                std::lock_guard<std::mutex> lk(mu_);
                busy_ = false;
            }
            idle_.notify_all();
        }
    }

    std::mutex mu_;
    std::condition_variable cv_, idle_;
    std::deque<ThumbJob> q_;
    std::thread worker_;
    bool quit_ = false, busy_ = false;
};

struct State {
    std::mutex mu;
    std::string dir;
    Clock::time_point model_at{};
    Clock::time_point pairs_at{};
    bool model_started = false, pairs_started = false;

    // The pair matrix, binned down to kMatrixBins per side.
    uint32_t n_images = 0, bins = 0;
    // The capture's images by id, so images.bin also lists those no model holds.
    std::vector<std::string> names;
    std::vector<uint32_t> keypoints;
    std::vector<uint32_t> counts, planned, verified;
    bool pairs_dirty = false;

    // Appended by the verification workers, so it carries its own lock and
    // stays open for the stage rather than reopening per pair.
    std::mutex live_mu;
    std::ofstream live;
    Clock::time_point live_at{};
    bool live_started = false;
    uint64_t live_bytes = 0;

    bool gauge_oriented = false, gauge_metric = false;

    Clock::time_point status_at{};
    bool status_started = false;
    Event last;                  // what the next unforced write would say

    ThumbWriter thumbs;
};

State& state() {
    static State s;
    return s;
}

// Whole file, then rename: a reader polling the directory either sees the
// previous snapshot or this one, never a prefix of one.
void write_atomic(const std::string& name, const std::string& bytes) {
    State& s = state();
    const fs::path dst = fs::path(s.dir) / name;
    const fs::path tmp = fs::path(s.dir) / (name + ".tmp");
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return;
        f.write(bytes.data(), (std::streamsize)bytes.size());
        if (!f) return;
    }
    std::error_code ec;
    fs::rename(tmp, dst, ec);
    if (ec) fs::remove(tmp, ec);
}

void put(std::string& b, const void* p, size_t n) {
    b.append((const char*)p, n);
}
void put_u32(std::string& b, uint32_t v) { put(b, &v, 4); }
void put_u64(std::string& b, uint64_t v) { put(b, &v, 8); }
void put_f32(std::string& b, float v) { put(b, &v, 4); }
void put_i64(std::string& b, int64_t v) { put(b, &v, 8); }
void put_f64(std::string& b, double v) { put(b, &v, 8); }

// True when `last` is far enough behind now, and stamps it if so.
bool due(Clock::time_point& last, bool& started) {
    const auto now = Clock::now();
    if (started &&
        std::chrono::duration<double>(now - last).count() < kInterval)
        return false;
    started = true;
    last = now;
    return true;
}

// images.bin (Progress.h): every image of the model, placed or not.
std::string image_stats(const Reconstruction& rec) {
    const State& s = state();
    uint32_t absent = 0;
    for (uint32_t i = 0; i < s.names.size(); i++) absent += rec.images.count(i) ? 0 : 1;
    std::string b;
    b.reserve(16 + (rec.images.size() + absent) * 48);
    put(b, "VKPI", 4);
    put_u32(b, 1);
    put_u32(b, (uint32_t)rec.images.size() + absent);
    for (uint32_t i = 0; i < s.names.size(); i++) {
        if (rec.images.count(i)) continue;
        put_u32(b, i);
        const uint8_t placed = 0;
        put(b, &placed, 1);
        put_u32(b, i < s.keypoints.size() ? s.keypoints[i] : 0);
        put_u32(b, 0);
        put_f32(b, 0.0f);
        put_f32(b, 0.0f);
        put_u32(b, (uint32_t)s.names[i].size());
        put(b, s.names[i].data(), s.names[i].size());
    }
    for (const auto& kv : rec.images) {
        const Image& im = kv.second;
        uint32_t points = 0;
        double sum = 0, worst = 0;
        const auto cam = rec.cameras.find(im.camera_id);
        if (im.registered && cam != rec.cameras.end())
            for (size_t f = 0; f < im.point3D_ids.size() && f < im.points2D.size(); f++) {
                const auto pt = rec.points3D.find(im.point3D_ids[f]);
                if (pt == rec.points3D.end()) continue;
                const Vec3 pc = mul(im.pose.R, pt->second.xyz) + im.pose.t;
                if (pc.z <= 0) continue;
                const Vec2 px = cam->second.project(pc);
                const double e = std::hypot(px.x - im.points2D[f].x, px.y - im.points2D[f].y);
                sum += e;
                worst = std::max(worst, e);
                points++;
            }
        put_u32(b, im.id);
        const uint8_t placed = im.registered ? 1 : 0;
        put(b, &placed, 1);
        put_u32(b, (uint32_t)im.points2D.size());
        put_u32(b, points);
        put_f32(b, points ? (float)(sum / points) : 0.0f);
        put_f32(b, (float)worst);
        // The run's own stem: a finished model's names carry the extension.
        const std::string& name = im.id < s.names.size() ? s.names[im.id] : im.name;
        put_u32(b, (uint32_t)name.size());
        put(b, name.data(), name.size());
    }
    return b;
}

void write_pairs_locked() {
    State& s = state();
    if (!s.pairs_dirty || s.counts.empty()) return;
    std::string b;
    b.reserve(16 + s.counts.size() * 12);
    put(b, "VKPP", 4);
    put_u32(b, 2);
    put_u32(b, s.n_images);
    put_u32(b, s.bins);
    put(b, s.counts.data(), s.counts.size() * 4);
    put(b, s.planned.data(), s.planned.size() * 4);
    put(b, s.verified.data(), s.verified.size() * 4);
    write_atomic("pairs.bin", b);
    s.pairs_dirty = false;
}

// Which cell of the matrix a pair of images falls in.
size_t cell_of(const State& s, uint32_t image1, uint32_t image2, size_t& mirror) {
    const uint32_t a = (uint32_t)((uint64_t)image1 * s.bins / s.n_images);
    const uint32_t b = (uint32_t)((uint64_t)image2 * s.bins / s.n_images);
    mirror = (size_t)b * s.bins + a;
    return (size_t)a * s.bins + b;
}

}  // namespace

void set_dir(const std::string& dir) {
    State& s = state();
    // Before the directory moves: what is queued was addressed to the old one.
    s.thumbs.stop();
    {
        // Closed here, not left to the process: a second run in the same
        // process must not append to the first one's file.
        std::lock_guard<std::mutex> live(s.live_mu);
        s.live.close();
        s.live.clear();
    }
    std::lock_guard<std::mutex> lk(s.mu);
    s.dir = dir;
    s.gauge_oriented = s.gauge_metric = false;
    if (dir.empty()) return;
    std::error_code ec;
    fs::create_directories(dir, ec);
}

bool enabled() {
    State& s = state();
    std::lock_guard<std::mutex> lk(s.mu);
    return !s.dir.empty();
}

void model(const Reconstruction& rec, bool force, const PointColor& color) {
    State& s = state();
    std::lock_guard<std::mutex> lk(s.mu);
    if (s.dir.empty()) return;
    if (!due(s.model_at, s.model_started) && !force) return;

    // Registered images only: an unregistered one has no pose to draw.
    std::vector<const Image*> imgs;
    for (const auto& kv : rec.images)
        if (kv.second.registered) imgs.push_back(&kv.second);

    const uint64_t n_pts = rec.points3D.size();
    const uint64_t stride = n_pts > kMaxPoints ? (n_pts / kMaxPoints + 1) : 1;

    std::string b;
    b.reserve(64 + imgs.size() * 128 + (size_t)(n_pts / stride + 1) * 15);
    put(b, "VKPM", 4);
    put_u32(b, 4);
    put_u32(b, (s.gauge_oriented ? 1u : 0u) | (s.gauge_metric ? 2u : 0u));
    put_u32(b, (uint32_t)rec.images.size());
    put_u32(b, (uint32_t)imgs.size());
    put_u64(b, n_pts);
    for (const Image* im : imgs) {
        put_u32(b, im->id);
        // COLMAP world->camera (R, t) to nerfstudio/OpenGL camera->world:
        // R^T with columns 1 and 2 negated, translation -R^T t. Same
        // conversion ColmapParser does, done here so the reader needs none.
        const Mat3& R = im->pose.R;      // row-major, world -> camera
        const Vec3& t = im->pose.t;
        const double C[3] = {
            -(R[0] * t.x + R[3] * t.y + R[6] * t.z),
            -(R[1] * t.x + R[4] * t.y + R[7] * t.z),
            -(R[2] * t.x + R[5] * t.y + R[8] * t.z)};
        for (int r = 0; r < 3; r++) {
            put_f32(b, (float)R[r]);            // R^T row r, column 0
            put_f32(b, (float)-R[3 + r]);       // ... column 1, negated
            put_f32(b, (float)-R[6 + r]);       // ... column 2, negated
            put_f32(b, (float)C[r]);
        }
        // The camera as cameras.bin would hold it: a COLMAP model id and its
        // parameters. The reader hands them to the dataset parser's own
        // mapping rather than keeping a second copy of it, which is what makes
        // a fisheye frustum draw as a fisheye.
        const auto cam = rec.cameras.find(im->camera_id);
        const Camera c = cam == rec.cameras.end() ? Camera{} : cam->second;
        put_u32(b, (uint32_t)c.width);
        put_u32(b, (uint32_t)c.height);
        put_u32(b, (uint32_t)camColmapId(c.model));
        double ps[12] = {};
        packColmap(c, ps);
        const uint32_t np = (uint32_t)camColmapParams(c.model);
        put_u32(b, np);
        for (uint32_t k = 0; k < np; k++) put(b, &ps[k], 8);
    }

    // Count first, so the reader can size its buffers before the loop.
    uint32_t written = 0;
    for (uint64_t i = 0; i < n_pts; i += stride) written++;
    put_u32(b, written);
    uint64_t k = 0, next = 0;
    for (const auto& kv : rec.points3D) {
        if (k++ != next) continue;
        next += stride;
        put_f32(b, (float)kv.second.xyz.x);
        put_f32(b, (float)kv.second.xyz.y);
        put_f32(b, (float)kv.second.xyz.z);
        uint8_t rgb[3] = {kv.second.rgb[0], kv.second.rgb[1], kv.second.rgb[2]};
        if (color) color(kv.second, rgb);
        put(b, rgb, 3);
    }
    write_atomic("model.bin", b);
    write_atomic("images.bin", image_stats(rec));
}

bool write_image_stats(const Reconstruction& rec, const std::string& path) {
    std::string b;
    {
        std::lock_guard<std::mutex> lk(state().mu);
        b = image_stats(rec);
    }
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f.write(b.data(), (std::streamsize)b.size());
    return (bool)f;
}

void images(const std::vector<std::string>& names, const std::vector<uint32_t>& keypoints) {
    State& s = state();
    std::lock_guard<std::mutex> lk(s.mu);
    s.names = names;
    s.keypoints = keypoints;
}

void gauge(bool oriented, bool metric) {
    State& s = state();
    std::lock_guard<std::mutex> lk(s.mu);
    s.gauge_oriented = oriented;
    s.gauge_metric = metric;
}

void begin_matching(uint32_t n_images,
                    const std::vector<std::pair<uint32_t, uint32_t>>& pairs) {
    State& s = state();
    std::lock_guard<std::mutex> lk(s.mu);
    if (s.dir.empty() || n_images == 0) return;
    s.n_images = n_images;
    s.bins = n_images < kMatrixBins ? n_images : kMatrixBins;
    s.counts.assign((size_t)s.bins * s.bins, 0);
    s.planned.assign((size_t)s.bins * s.bins, 0);
    s.verified.assign((size_t)s.bins * s.bins, 0);
    for (const auto& p : pairs) {
        if (p.first >= n_images || p.second >= n_images) continue;
        size_t mirror = 0;
        const size_t c = cell_of(s, p.first, p.second, mirror);
        s.planned[c]++;
        if (mirror != c) s.planned[mirror]++;
    }
    s.pairs_started = false;
    s.pairs_dirty = true;
}

void pair(uint32_t image1, uint32_t image2, uint32_t inliers) {
    State& s = state();
    std::lock_guard<std::mutex> lk(s.mu);
    if (s.dir.empty() || s.counts.empty()) return;
    if (image1 >= s.n_images || image2 >= s.n_images) return;
    size_t mirror = 0;
    const size_t c = cell_of(s, image1, image2, mirror);
    s.counts[c] += inliers;
    s.verified[c]++;
    if (mirror != c) {
        s.counts[mirror] += inliers;
        s.verified[mirror]++;
    }
    s.pairs_dirty = true;
    if (due(s.pairs_at, s.pairs_started)) write_pairs_locked();
}

void status(const Event& e) {
    State& s = state();
    std::lock_guard<std::mutex> lk(s.mu);
    if (s.dir.empty()) return;
    using K = Event::Kind;
    // Nothing here is keyed on a pair, and matching emits one per pair from
    // its workers: taking the lock for it would serialize the stage on this.
    if (e.kind == K::PairVerified) return;
    // A stage boundary and the verdict are the two things a screen must not
    // miss; a fraction can wait for the clock.
    const bool force = e.kind == K::StageBegin || e.kind == K::StageEnd ||
                       e.kind == K::Result;
    if (e.kind == K::Progress || e.kind == K::ImageExtracted) {
        s.last.stage = e.stage;
        s.last.done = e.done;
        s.last.total = e.total;
    } else if (e.kind == K::ModelUpdated) {
        // Model size, not the bar: mapping's fraction comes from
        // events::map_placed, because this count falls back on a seed retry.
        s.last.stage = e.stage;
        s.last.registered = e.registered;
        s.last.images = e.images;
        s.last.points = e.points;
    } else if (force) {
        Event keep = s.last;
        s.last = e;
        if (e.kind == K::StageBegin) { s.last.done = 0; }
        if (e.kind == K::StageEnd) { s.last.done = keep.total; s.last.total = keep.total; }
    }
    if (!force && !due(s.status_at, s.status_started)) return;
    if (force) { s.status_at = Clock::now(); s.status_started = true; }

    const Event& v = s.last;
    uint32_t flags = 0;
    if (e.kind == K::Result) flags |= 1u;
    if (v.partial) flags |= 2u;
    if (v.metric) flags |= 4u;
    std::string b;
    b += "VKPS";
    put_u32(b, 1);
    put_u32(b, (uint32_t)v.stage);
    put_u32(b, flags);
    put_i64(b, v.done);
    put_i64(b, v.total);
    put_i64(b, v.registered);
    put_i64(b, v.images);
    put_i64(b, v.points);
    put_i64(b, v.models);
    put_f64(b, v.mean_reproj);
    write_atomic("status.bin", b);
}

void live_matches_begin(const std::vector<std::string>& names,
                        const std::vector<uint32_t>& num_features) {
    State& s = state();
    std::string dir;
    {
        std::lock_guard<std::mutex> lk(s.mu);
        dir = s.dir;
    }
    if (dir.empty()) return;
    std::lock_guard<std::mutex> lk(s.live_mu);
    s.live.close();
    s.live.clear();
    s.live.open(fs::path(dir) / "live_matches.bin",
                std::ios::binary | std::ios::trunc);
    if (!s.live) return;
    s.live_started = false;
    s.live_bytes = 0;
    const uint32_t version = 3, nimg = (uint32_t)names.size();
    s.live.write("VKMT", 4);
    s.live.write((const char*)&version, 4);
    s.live.write((const char*)&nimg, 4);
    for (uint32_t i = 0; i < nimg; i++) {
        const uint32_t len = (uint32_t)names[i].size();
        const uint32_t nf = i < num_features.size() ? num_features[i] : 0;
        s.live.write((const char*)&len, 4);
        s.live.write(names[i].data(), len);
        s.live.write((const char*)&nf, 4);
    }
    const uint32_t npairs = kStreamingPairs;
    s.live.write((const char*)&npairs, 4);
    s.live.flush();
}

// Packed before the lock and written once: this is the verification workers'
// inner loop, and a stream write per index queued them behind each other.
// Flushed on a clock -- a reader already has to stop at a torn tail.
void live_pair(uint32_t a, uint32_t b, int32_t config,
               const uint32_t* idx1, const uint32_t* idx2, size_t stride,
               uint32_t count) {
    std::string rec;
    rec.reserve(16 + (size_t)count * 8);
    put_u32(rec, a);
    put_u32(rec, b);
    put(rec, &config, 4);
    put_u32(rec, count);
    for (uint32_t i = 0; i < count; i++) {
        put(rec, (const char*)idx1 + i * stride, 4);
        put(rec, (const char*)idx2 + i * stride, 4);
    }
    State& s = state();
    std::lock_guard<std::mutex> lk(s.live_mu);
    if (!s.live || s.live_bytes > kLiveCap) return;
    s.live.write(rec.data(), (std::streamsize)rec.size());
    s.live_bytes += rec.size();
    if (due(s.live_at, s.live_started)) s.live.flush();
}

// Box-filtered, which is enough for a preview and avoids pulling a resampler
// in. The downscale happens here because it reads the caller's buffer, which
// is gone by the time the writer runs; everything after it is the writer's.
void thumbnail(const std::string& rel_stem, const uint8_t* rgb, int w, int h) {
    State& s = state();
    std::string dir;
    {
        std::lock_guard<std::mutex> lk(s.mu);
        if (s.dir.empty()) return;
        dir = s.dir;
    }
    if (!rgb || w <= 0 || h <= 0) return;
    const int longest = w > h ? w : h;
    const int step = longest > kThumbLong ? (longest + kThumbLong - 1) / kThumbLong : 1;
    ThumbJob j;
    j.w = (w + step - 1) / step;
    j.h = (h + step - 1) / step;
    j.dst = fs::path(dir) / "thumbs" / (rel_stem + ".jpg");
    j.rgb.resize((size_t)j.w * j.h * 3);
    for (int y = 0; y < j.h; y++) {
        for (int x = 0; x < j.w; x++) {
            uint32_t acc[3] = {0, 0, 0};
            uint32_t n = 0;
            for (int dy = 0; dy < step; dy++) {
                const int sy = y * step + dy;
                if (sy >= h) break;
                for (int dx = 0; dx < step; dx++) {
                    const int sx = x * step + dx;
                    if (sx >= w) break;
                    const uint8_t* p = rgb + ((size_t)sy * w + sx) * 3;
                    acc[0] += p[0]; acc[1] += p[1]; acc[2] += p[2];
                    n++;
                }
            }
            uint8_t* d = j.rgb.data() + ((size_t)y * j.w + x) * 3;
            for (int c = 0; c < 3; c++) d[c] = n ? (uint8_t)(acc[c] / n) : 0;
        }
    }
    s.thumbs.push(std::move(j));
}

void flush() {
    State& s = state();
    s.thumbs.drain();
    {
        std::lock_guard<std::mutex> lk(s.live_mu);
        if (s.live) s.live.flush();
    }
    std::lock_guard<std::mutex> lk(s.mu);
    if (s.dir.empty()) return;
    write_pairs_locked();
}

}  // namespace progress
}  // namespace sfm

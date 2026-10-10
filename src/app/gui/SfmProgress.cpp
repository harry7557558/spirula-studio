// SfmProgress.cpp -- see SfmProgress.h.

#include "app/gui/SfmProgress.h"

#include "core/CameraModel.h"

#include "sfm/core/Progress.h"
#ifdef SS_TOOL_SFM
#include "sfm/core/Features.h"
#include "sfm/core/Matches.h"
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>

namespace fs = std::filesystem;

namespace gui {

namespace {

// Snapshot metadata arrives by rename; dense point files are read separately.
std::string slurp_if_newer(const fs::path& p, int64_t& mtime) {
    std::error_code ec;
    const auto t = fs::last_write_time(p, ec);
    if (ec) return {};
    const int64_t stamp = t.time_since_epoch().count();
    if (stamp == mtime) return {};
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    std::string b((std::istreambuf_iterator<char>(f)),
                  std::istreambuf_iterator<char>());
    mtime = stamp;
    return b;
}

// A cursor that refuses to run off the end, so a truncated file yields an
// empty result instead of a crash.
struct Reader {
    const char* p;
    const char* end;
    bool ok = true;
    bool take(void* dst, size_t n) {
        if (!ok || (size_t)(end - p) < n) return ok = false;
        std::memcpy(dst, p, n);
        p += n;
        return true;
    }
    uint32_t u32() { uint32_t v = 0; take(&v, 4); return v; }
    uint64_t u64() { uint64_t v = 0; take(&v, 8); return v; }
    float f32() { float v = 0; take(&v, 4); return v; }
    std::string text() {
        const uint32_t n = u32();
        if (!ok || n > (size_t)(end - p)) { ok = false; return {}; }
        std::string s(p, n); p += n; return s;
    }
};

}  // namespace

bool read_status(const std::string& dir, int64_t& mtime, RunStatus& out) {
    if (dir.empty()) return false;
    const std::string b = slurp_if_newer(fs::path(dir) / "status.bin", mtime);
    if (b.size() < 72 || std::memcmp(b.data(), "VKPS", 4) != 0) return false;
    Reader r{b.data() + 4, b.data() + b.size()};
    if (r.u32() != 1) return false;
    RunStatus st;
    st.stage = r.u32();
    const uint32_t flags = r.u32();
    st.finished = (flags & 1u) != 0;
    st.partial = (flags & 2u) != 0;
    st.metric = (flags & 4u) != 0;
    st.done = (int64_t)r.u64();
    st.total = (int64_t)r.u64();
    st.registered = (int64_t)r.u64();
    st.images = (int64_t)r.u64();
    st.points = (int64_t)r.u64();
    st.models = (int64_t)r.u64();
    double mean = 0;
    r.take(&mean, 8);
    st.mean_reproj = mean;
    if (!r.ok) return false;
    out = st;
    return true;
}

float mapping_fraction(int64_t done, int64_t total) {
    if (total <= 0) return -1.0f;
    const double x = std::min(1.0, (double)done / (double)total);
    return (float)(kMappingBarFull * x * std::sqrt(x));
}

bool read_live_model(const std::string& dir, int64_t& mtime, LiveModel& out, uint64_t point_memory_budget) {
    int64_t stamp = mtime;
    const std::string b = slurp_if_newer(fs::path(dir) / "model.bin", stamp);
    if (b.size() < 24 || std::memcmp(b.data(), "VKPM", 4) != 0) return false;

    Reader r{b.data() + 4, b.data() + b.size()};
    const uint32_t version = r.u32();
    if (version < 2 || version > 7) return false;
    LiveModel m;
    uint32_t flags = 0;
    if (version >= 3) {
        flags = r.u32();
        m.ds.gauge_oriented = (flags & 1u) != 0;
        m.ds.gauge_metric = (flags & 2u) != 0;
        m.provisional = version >= 6 && (flags & 16u) != 0;
        m.filtered = version >= 7 && (flags & 32u) != 0;
    }
    m.n_images = r.u32();
    m.n_registered = r.u32();
    m.n_points = r.u64();
    if (!r.ok || m.n_registered > b.size() / 64) return false;

    ParsedDataset& ds = m.ds;
    ds.num_cameras = (int64_t)m.n_registered;
    ds.c2w.resize((size_t)m.n_registered * 12);
    ds.intrins.resize((size_t)m.n_registered * 4);
    ds.widths.resize(m.n_registered);
    ds.heights.resize(m.n_registered);
    ds.camera_models.assign(m.n_registered, (int32_t)CameraModelType::PINHOLE);
    ds.camera_distortions.assign(m.n_registered,
                                 (int32_t)CameraDistortionType::None);
    ds.dist_coeffs.assign((size_t)m.n_registered * kCameraDistortionParams, 0.0f);
    std::vector<double> params;
    // One camera record per image in the snapshot, and a capture usually
    // repeats one: colmap_preview_intrins solves a least-squares fit for a
    // lens no tier represents, which is ~25 ms each.
    std::map<std::string, std::optional<PreviewIntrins>> cams;
    if (version >= 4) m.ids.resize(m.n_registered);
    for (uint32_t i = 0; i < m.n_registered; i++) {
        if (version >= 4) m.ids[i] = r.u32();
        for (int k = 0; k < 12; k++) ds.c2w[(size_t)i * 12 + k] = r.f32();
        const int w = (int)r.u32(), h = (int)r.u32();
        ds.widths[i] = (int32_t)w;
        ds.heights[i] = (int32_t)h;
        if (version >= 6) {
            const uint32_t model = r.u32(), distortion = r.u32();
            if (model > (uint32_t)CameraModelType::EQUIRECTANGULAR ||
                distortion > (uint32_t)CameraDistortionType::ThinPrism) return false;
            ds.camera_models[i] = (int32_t)model;
            ds.camera_distortions[i] = (int32_t)distortion;
            for (int k = 0; k < 4; ++k) ds.intrins[(size_t)i * 4 + k] = r.f32();
            for (int k = 0; k < kCameraDistortionParams; ++k)
                ds.dist_coeffs[(size_t)i * kCameraDistortionParams + k] = r.f32();
            continue;
        }
        const int model_id = (int)r.u32();
        const uint32_t np = r.u32();
        if (!r.ok || np > 16) return false;
        params.resize(np);
        for (uint32_t k = 0; k < np; k++) r.take(&params[k], 8);
        // The parser's own mapping, so a fisheye draws as a fisheye rather
        // than as the pinhole a bare focal length would suggest.
        std::string key((const char*)&model_id, sizeof model_id);
        key.append((const char*)&w, sizeof w).append((const char*)&h, sizeof h);
        key.append((const char*)params.data(), params.size() * sizeof(double));
        auto [ent, fresh] = cams.try_emplace(key);
        if (fresh) {
            PreviewIntrins pi;
            if (colmap_preview_intrins(model_id, w, h, params, pi))
                ent->second = pi;
        }
        if (ent->second) {
            const PreviewIntrins& pi = *ent->second;
            ds.camera_models[i] = pi.model;
            ds.camera_distortions[i] = pi.distortion;
            ds.intrins[(size_t)i * 4 + 0] = pi.fx;
            ds.intrins[(size_t)i * 4 + 1] = pi.fy;
            ds.intrins[(size_t)i * 4 + 2] = pi.cx;
            ds.intrins[(size_t)i * 4 + 3] = pi.cy;
            for (int k = 0; k < kCameraDistortionParams; k++)
                ds.dist_coeffs[(size_t)i * kCameraDistortionParams + k] = pi.dist[k];
        } else if (params.size() >= 4) {
            // A record this reader cannot interpret at all; a plain frustum
            // from whatever the first four parameters are is still the right
            // place in space.
            for (int k = 0; k < 4; k++)
                ds.intrins[(size_t)i * 4 + k] = (float)params[k];
        }
    }
    const uint64_t n_pts = version >= 5 ? m.n_points : r.u32();
    if (!r.ok) return false;
    if (n_pts > ds.points.xyz.max_size() / 3 || n_pts > ds.points.rgb.max_size() / 3) return false;
    const uint64_t working_bytes = 3 * (sizeof(double) + 4 * sizeof(float) + sizeof(uint8_t));
    if (point_memory_budget && n_pts > point_memory_budget / working_bytes)
        throw std::runtime_error("full point checkpoint exceeds available host memory");
    std::ifstream points;
    if (version >= 5) {
        const std::string name = r.text(), error = r.text();
        if (!r.ok) return false;
        if (flags & 8u) throw std::runtime_error(error);
        if (name.empty() || fs::path(name).filename().string() != name || name == "." || name == "..") return false;
        if (version == 5) m.provisional = name.size() >= 20 && name.compare(name.size() - 20, 20, "-observations.points") == 0;
        const fs::path file = fs::path(dir) / name;
        std::error_code ec;
        const uint64_t bytes = fs::file_size(file, ec);
        if (ec || n_pts > bytes / 15) return false;
        points.open(file, std::ios::binary);
        if (!points) return false;
    } else if (n_pts > (uint64_t)(r.end - r.p) / 15) return false;
    ds.points.xyz.resize((size_t)n_pts * 3);
    ds.points.rgb.resize((size_t)n_pts * 3);
    if (version >= 5) {
        std::array<char, 4096 * 15> chunk;
        for (uint64_t begin = 0; begin < n_pts;) {
            const size_t count = (size_t)std::min<uint64_t>(chunk.size() / 15, n_pts - begin);
            points.read(chunk.data(), (std::streamsize)(count * 15));
            if (!points) return false;
            for (size_t i = 0; i < count; ++i) {
                float xyz[3];
                std::memcpy(xyz, chunk.data() + i * 15, sizeof xyz);
                for (int k = 0; k < 3; ++k) ds.points.xyz[(size_t)(begin + i) * 3 + k] = xyz[k];
                std::memcpy(&ds.points.rgb[(size_t)(begin + i) * 3], chunk.data() + i * 15 + 12, 3);
            }
            begin += count;
        }
    } else for (uint64_t i = 0; i < n_pts; i++) {
        for (int k = 0; k < 3; k++) ds.points.xyz[(size_t)i * 3 + k] = r.f32();
        r.take(&ds.points.rgb[(size_t)i * 3], 3);
    }
    if (!r.ok) return false;

    // The preview frames a scene by putting the camera one unit from the
    // origin, because a parsed dataset arrives centred on its poses and scaled
    // to fit. A model straight out of the mapper is in whatever frame the seed
    // pair fixed, so give it the same similarity the parsers record: centre on
    // the camera positions, scale so they fit inside the unit sphere.
    float centre[3] = {0, 0, 0};
    if (m.n_registered) {
        for (uint32_t i = 0; i < m.n_registered; i++)
            for (int k = 0; k < 3; k++)
                centre[k] += ds.c2w[(size_t)i * 12 + k * 4 + 3];
        for (float& c : centre) c /= (float)m.n_registered;
    }
    float radius = 0.0f;
    for (uint32_t i = 0; i < m.n_registered; i++) {
        float d = 0.0f;
        for (int k = 0; k < 3; k++) {
            const float v = ds.c2w[(size_t)i * 12 + k * 4 + 3] - centre[k];
            d += v * v;
        }
        radius = std::max(radius, std::sqrt(d));
    }
    // One or two cameras have no spread to measure; the points are all there
    // is to frame by until the third arrives.
    if (radius <= 1e-6f && n_pts) {
        for (uint64_t i = 0; i < n_pts; i++) {
            float d = 0.0f;
            for (int k = 0; k < 3; k++) {
                const float v = ds.points.xyz[(size_t)i * 3 + k] - centre[k];
                d += v * v;
            }
            radius = std::max(radius, std::sqrt(d));
        }
        radius *= 0.25f;   // points reach far past the cameras that saw them
    }
    if (!(radius > 1e-6f)) radius = 1.0f;
    ds.train_frame_scale = radius;
    // normalized -> train, which is what the field holds: scale then offset.
    ds.train_to_normalized = {radius, 0, 0, centre[0],
                              0, radius, 0, centre[1],
                              0, 0, radius, centre[2],
                              0, 0, 0, 1};

    // The frustum-size heuristic reads only these two, and a live model has no
    // camera bake behind it to fill the rest from.
    m.post.n_post = ds.num_cameras;
    m.post.c2w_flip = ds.c2w;

    out = std::move(m);
    mtime = stamp;
    return true;
}

bool read_image_stats(const std::string& path, int64_t& mtime, std::vector<ImageStat>& out) {
    if (path.empty()) return false;
    int64_t stamp = mtime;
    const std::string b = slurp_if_newer(fs::path(path), stamp);
    if (b.size() < 12 || std::memcmp(b.data(), "VKPI", 4) != 0) return false;
    Reader r{b.data() + 4, b.data() + b.size()};
    if (r.u32() != 1) return false;
    const uint32_t n = r.u32();
    if (!r.ok || n > b.size() / 21) return false;
    std::vector<ImageStat> v(n);
    for (ImageStat& s : v) {
        s.id = r.u32();
        uint8_t placed = 0;
        r.take(&placed, 1);
        s.placed = placed != 0;
        s.keypoints = r.u32();
        s.points = r.u32();
        s.mean_error = r.f32();
        s.max_error = r.f32();
        s.name = r.text();
    }
    if (!r.ok) return false;
    out = std::move(v);
    mtime = stamp;
    return true;
}

namespace {

// (x - centre) / radius: train frame to the preview's normalized one.
void normalized_centre(const LiveModel& m, size_t i, double out[3]) {
    const auto& A = m.ds.train_to_normalized;
    const double r = A[0] > 0 ? A[0] : 1.0;
    for (int k = 0; k < 3; k++)
        out[k] = (m.ds.c2w[i * 12 + (size_t)k * 4 + 3] - A[(size_t)k * 4 + 3]) / r;
}

void rotation_of(const LiveModel& m, size_t i, double R[9]) {
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) R[r * 3 + c] = m.ds.c2w[i * 12 + (size_t)r * 4 + c];
}

// Unit quaternion (x, y, z, w) of a rotation matrix, row-major.
void to_quat(const double m[9], double q[4]) {
    const double tr = m[0] + m[4] + m[8];
    if (tr > 0) {
        const double s = 0.5 / std::sqrt(tr + 1.0);
        q[0] = (m[7] - m[5]) * s; q[1] = (m[2] - m[6]) * s;
        q[2] = (m[3] - m[1]) * s; q[3] = 0.25 / s;
    } else if (m[0] > m[4] && m[0] > m[8]) {
        const double s = 2.0 * std::sqrt(1.0 + m[0] - m[4] - m[8]);
        q[0] = 0.25 * s; q[1] = (m[1] + m[3]) / s;
        q[2] = (m[2] + m[6]) / s; q[3] = (m[7] - m[5]) / s;
    } else if (m[4] > m[8]) {
        const double s = 2.0 * std::sqrt(1.0 + m[4] - m[0] - m[8]);
        q[0] = (m[1] + m[3]) / s; q[1] = 0.25 * s;
        q[2] = (m[5] + m[7]) / s; q[3] = (m[2] - m[6]) / s;
    } else {
        const double s = 2.0 * std::sqrt(1.0 + m[8] - m[0] - m[4]);
        q[0] = (m[2] + m[6]) / s; q[1] = (m[5] + m[7]) / s;
        q[2] = 0.25 * s; q[3] = (m[3] - m[1]) / s;
    }
}

void from_quat(const double q[4], double m[9]) {
    const double x = q[0], y = q[1], z = q[2], w = q[3];
    m[0] = 1 - 2*(y*y + z*z); m[1] = 2*(x*y - z*w);     m[2] = 2*(x*z + y*w);
    m[3] = 2*(x*y + z*w);     m[4] = 1 - 2*(x*x + z*z); m[5] = 2*(y*z - x*w);
    m[6] = 2*(x*z - y*w);     m[7] = 2*(y*z + x*w);     m[8] = 1 - 2*(x*x + y*y);
}

void note_peak(PairMatrix& m) {
    m.peak = 0;
    for (uint32_t v : m.counts) m.peak = std::max(m.peak, v);
}

}  // namespace

bool snapshot_motion(const LiveModel& from, const LiveModel& to, float S[12]) {
    std::map<uint32_t, size_t> at;
    for (size_t i = 0; i < from.ids.size(); i++) at[from.ids[i]] = i;
    std::vector<std::pair<size_t, size_t>> both;
    for (size_t j = 0; j < to.ids.size(); j++) {
        const auto it = at.find(to.ids[j]);
        if (it != at.end()) both.push_back({it->second, j});
    }
    if (both.size() < 2) return false;

    // The turn: every shared camera's c2w rotation, taken from one frame to
    // the other, averaged as quaternions on one hemisphere.
    double qsum[4] = {0, 0, 0, 0}, q0[4] = {0, 0, 0, 0};
    std::vector<std::array<double, 4>> qs;
    for (const auto& [i, j] : both) {
        double A[9], B[9], R[9], q[4];
        rotation_of(from, i, A);
        rotation_of(to, j, B);
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++) {
                R[r * 3 + c] = 0;
                for (int k = 0; k < 3; k++) R[r * 3 + c] += B[r * 3 + k] * A[c * 3 + k];
            }
        to_quat(R, q);
        if (qs.empty()) std::copy(q, q + 4, q0);
        const double sgn = q[0]*q0[0] + q[1]*q0[1] + q[2]*q0[2] + q[3]*q0[3] < 0 ? -1.0 : 1.0;
        for (int k = 0; k < 4; k++) qsum[k] += sgn * q[k];
        qs.push_back({sgn * q[0], sgn * q[1], sgn * q[2], sgn * q[3]});
    }
    const double qn = std::sqrt(qsum[0]*qsum[0] + qsum[1]*qsum[1] + qsum[2]*qsum[2] + qsum[3]*qsum[3]);
    if (!(qn > 1e-12)) return false;
    for (double& v : qsum) v /= qn;
    // Cameras that turned by different amounts moved relative to each other.
    for (const auto& q : qs)
        if (std::fabs(q[0]*qsum[0] + q[1]*qsum[1] + q[2]*qsum[2] + q[3]*qsum[3]) < 0.996)
            return false;   // more than ~10 degrees apart
    double R[9];
    from_quat(qsum, R);

    // Scale and move from the camera centres, least squares given R.
    std::vector<std::array<double, 3>> a(both.size()), b(both.size());
    double ma[3] = {0, 0, 0}, mb[3] = {0, 0, 0};
    for (size_t k = 0; k < both.size(); k++) {
        normalized_centre(from, both[k].first, a[k].data());
        normalized_centre(to, both[k].second, b[k].data());
        for (int d = 0; d < 3; d++) { ma[d] += a[k][d]; mb[d] += b[k][d]; }
    }
    for (int d = 0; d < 3; d++) { ma[d] /= (double)both.size(); mb[d] /= (double)both.size(); }
    std::vector<std::array<double, 3>> ra(both.size());
#if 0
    double num = 0, den = 0;
    for (size_t k = 0; k < both.size(); k++) {
        const double p[3] = {a[k][0] - ma[0], a[k][1] - ma[1], a[k][2] - ma[2]};
        for (int r = 0; r < 3; r++) ra[k][r] = R[r*3]*p[0] + R[r*3+1]*p[1] + R[r*3+2]*p[2];
        for (int d = 0; d < 3; d++) {
            num += (b[k][d] - mb[d]) * ra[k][d];
            den += p[d] * p[d];
        }
    }
    if (!(den > 1e-12) || !(num > 0)) return false;
    const double s = num / den;
#else
    const double s = 1.0;
#endif
    // The normalized frames are about one unit across; a camera that lands a
    // tenth of that away from where the fit puts it is not the same model.
    double err = 0;
    for (size_t k = 0; k < both.size(); k++)
        for (int d = 0; d < 3; d++) {
            const double e = b[k][d] - mb[d] - s * ra[k][d];
            err += e * e;
        }
    if (std::sqrt(err / (double)both.size()) > 0.1) return false;
    for (int r = 0; r < 3; r++) {
        double t = mb[r];
        for (int c = 0; c < 3; c++) {
            S[r*4 + c] = (float)(s * R[r*3 + c]);
            t -= s * R[r*3 + c] * ma[c];
        }
        S[r*4 + 3] = (float)t;
    }
    return true;
}

bool snapshot_up(const LiveModel& m, float up[3]) {
    double u[3] = {0, 0, 0};
    for (int64_t i = 0; i < m.ds.num_cameras; i++)
        for (int r = 0; r < 3; r++) u[r] += m.ds.c2w[(size_t)i * 12 + (size_t)r * 4 + 1];
    const double n = std::sqrt(u[0]*u[0] + u[1]*u[1] + u[2]*u[2]);
    if (!(n > 1e-9)) return false;
    for (int r = 0; r < 3; r++) up[r] = (float)(u[r] / n);
    return true;
}

bool read_pair_matrix(const std::string& dir, int64_t& mtime, PairMatrix& out) {
    const std::string b = slurp_if_newer(fs::path(dir) / "pairs.bin", mtime);
    if (b.size() < 16 || std::memcmp(b.data(), "VKPP", 4) != 0) return false;
    Reader r{b.data() + 4, b.data() + b.size()};
    if (r.u32() != 2) return false;
    PairMatrix m;
    m.n_images = r.u32();
    m.bins = r.u32();
    if (!r.ok || m.bins == 0 || m.bins > sfm::progress::kMatrixBins) return false;
    const size_t n = (size_t)m.bins * m.bins;
    m.counts.resize(n);
    m.planned.resize(n);
    m.verified.resize(n);
    if (!r.take(m.counts.data(), n * 4)) return false;
    if (!r.take(m.planned.data(), n * 4)) return false;
    if (!r.take(m.verified.data(), n * 4)) return false;
    note_peak(m);
    out = std::move(m);
    return true;
}

bool read_pair_matrix_from_matches(const std::string& matches_path,
                                   int64_t& mtime, PairMatrix& out) {
#ifndef SS_TOOL_SFM
    (void)matches_path; (void)mtime; (void)out;
    return false;
#else
    std::error_code ec;
    const auto t = fs::last_write_time(matches_path, ec);
    if (ec) return false;
    const int64_t stamp = t.time_since_epoch().count();
    if (stamp == mtime) return false;

    // The pair table alone: the match arrays behind it are most of the file
    // and this only counts them.
    sfm::MatchesIndex idx;
    if (!sfm::indexMatches(matches_path, idx)) return false;
    mtime = stamp;
    PairMatrix m;
    m.n_images = (uint32_t)idx.images.size();
    if (m.n_images == 0) return false;
    m.bins = std::min(m.n_images, sfm::progress::kMatrixBins);
    m.counts.assign((size_t)m.bins * m.bins, 0);
    // A finished file holds the pairs that survived and says nothing about the
    // ones that were tried and failed, so it cannot fill `planned` /
    // `verified`: what is left is the count, and the drawing says so.
    for (const sfm::MatchesIndex::Entry& p : idx.pairs) {
        if (p.image1 >= m.n_images || p.image2 >= m.n_images) continue;
        const uint32_t a = (uint32_t)((uint64_t)p.image1 * m.bins / m.n_images);
        const uint32_t b = (uint32_t)((uint64_t)p.image2 * m.bins / m.n_images);
        m.counts[(size_t)a * m.bins + b] += p.count;
        if (a != b) m.counts[(size_t)b * m.bins + a] += p.count;
    }
    note_peak(m);
    out = std::move(m);
    return true;
#endif
}

bool read_keypoints(const std::string& features_dir, const std::string& rel_stem,
                    int width, int height, std::vector<KeyPoint2D>& out) {
    return read_keypoints_file((fs::path(features_dir) / (rel_stem + ".bin")).string(),
                               width, height, out);
}

bool read_keypoints_file(const std::string& path, int width, int height,
                         std::vector<KeyPoint2D>& out) {
    out.clear();
#ifndef SS_TOOL_SFM
    (void)path; (void)width; (void)height;
    return false;
#else
    std::error_code ec;
    if (!fs::exists(path, ec)) return false;
    sfm::FeatureSet fs_;
    try {
        fs_ = sfm::readFeatures(path, /*with_descriptors=*/false);
    } catch (const std::exception&) {
        return false;
    }
    // The extractor may have measured on a downscaled copy; the file says which
    // size its coordinates are in, so the caller's image size is what they have
    // to be brought to.
    const float sx = fs_.width > 0 && width > 0 ? (float)width / fs_.width : 1.0f;
    const float sy = fs_.height > 0 && height > 0 ? (float)height / fs_.height : 1.0f;
    out.reserve(fs_.keypoints.size());
    for (const sfm::Keypoint& k : fs_.keypoints)
        // Radius from the detector's sigma the way OpenCV's drawKeypoints does
        // it: the circle is the region the descriptor was measured over, not
        // the pixel it sits on.
        out.push_back({k.x * sx, k.y * sy, k.scale * 3.0f * 0.5f * (sx + sy)});
    return true;
#endif
}

std::vector<std::string> read_image_stems(const std::string& features_dir,
                                          const std::string& matches_path) {
    std::vector<fs::path> files;
    std::error_code ec;
    const fs::path root(features_dir);
    for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end;
         it.increment(ec))
        if (it->is_regular_file(ec) && it->path().extension() == ".bin")
            files.push_back(it->path());
    // Sorted paths are what matching numbers its images by; a directory walk
    // arrives in whatever order the filesystem chose.
    std::sort(files.begin(), files.end());
    std::vector<std::string> stems;
    stems.reserve(files.size());
    for (const fs::path& f : files) {
        fs::path rel = f.lexically_relative(root);
        if (rel.empty() || *rel.begin() == "..") rel = f.filename();
        stems.push_back((rel.parent_path() / rel.stem()).generic_string());
    }
#ifdef SS_TOOL_SFM
    if (stems.empty() && !matches_path.empty()) {
        sfm::MatchesIndex idx;
        if (sfm::indexMatches(matches_path, idx))
            for (const sfm::ImageEntry& im : idx.images) stems.push_back(im.name);
    }
#else
    (void)matches_path;
#endif
    return stems;
}

}  // namespace gui

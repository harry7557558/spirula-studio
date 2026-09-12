// SfmProgress.cpp -- see SfmProgress.h.

#include "app/gui/SfmProgress.h"

#include "core/CameraModel.h"

#include "sfm/core/Progress.h"
#ifdef SS_TOOL_SFM
#include "sfm/core/Features.h"
#include "sfm/core/Matches.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>

namespace fs = std::filesystem;

namespace gui {

namespace {

// Whole file, or empty when it is not there. A snapshot is at most ~1 MB and
// arrives by rename, so there is no partial read to guard against.
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

bool read_live_model(const std::string& dir, int64_t& mtime, LiveModel& out) {
    const std::string b = slurp_if_newer(fs::path(dir) / "model.bin", mtime);
    if (b.size() < 24 || std::memcmp(b.data(), "VKPM", 4) != 0) return false;

    Reader r{b.data() + 4, b.data() + b.size()};
    const uint32_t version = r.u32();
    if (version != 2 && version != 3) return false;
    LiveModel m;
    if (version >= 3) {
        const uint32_t flags = r.u32();
        m.ds.gauge_oriented = (flags & 1u) != 0;
        m.ds.gauge_metric = (flags & 2u) != 0;
    }
    m.n_images = r.u32();
    m.n_registered = r.u32();
    m.n_points = r.u64();

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
    for (uint32_t i = 0; i < m.n_registered; i++) {
        for (int k = 0; k < 12; k++) ds.c2w[(size_t)i * 12 + k] = r.f32();
        const int w = (int)r.u32(), h = (int)r.u32();
        ds.widths[i] = (int32_t)w;
        ds.heights[i] = (int32_t)h;
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
    const uint32_t n_pts = r.u32();
    if (!r.ok) return false;
    ds.points.xyz.resize((size_t)n_pts * 3);
    ds.points.rgb.resize((size_t)n_pts * 3);
    for (uint32_t i = 0; i < n_pts; i++) {
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
        for (uint32_t i = 0; i < n_pts; i++) {
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
    return true;
}

namespace {

void note_peak(PairMatrix& m) {
    m.peak = 0;
    for (uint32_t v : m.counts) m.peak = std::max(m.peak, v);
}

}  // namespace

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

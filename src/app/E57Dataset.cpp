#include "app/E57Dataset.h"

#include "app/DepthPng.h"
#include "app/ScanDepth.h"
#include "core/CameraModel.h"
#include "data/JsonWrite.h"
#include "data/SparseEdit.h"
#include "external/stb_image.h"
#include "external/stb_image_write.h"
#include "i18n/Message.h"
#include "i18n/catalog/E57.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <set>
#include <stdexcept>
#include <utility>

namespace fs = std::filesystem;
namespace e57 = spirula::e57;
namespace emsg = spirula::i18n::msg::e57;
using spirula::i18n::format;

namespace app {
namespace {

constexpr double kPi = 3.14159265358979323846;

// OpenGL camera axes (columns) in an E57 image's own frame, measured by
// projecting real scans' coloured points into their images: a pinhole
// frame is OpenGL's; a panorama's centre looks along +x with +z up.
constexpr double kPinholeAxes[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
constexpr double kSphericalAxes[9] = {0, 0, -1, -1, 0, 0, 0, 1, 0};

// A uniform draw of 8x the target, within these bounds, is what gets thinned:
// memory stays bounded and the sparse far parts of a scan keep most points.
constexpr int64_t kPreThinFloor = 8000000;
constexpr int64_t kPreThinCeiling = 40000000;

// The depth maps are rendered from a draw of at most this many points, 15 bytes
// each once bucketed; fewer leave a speckle of holes in a dense 1600 px face.
constexpr int64_t kDepthCloudCap = 40000000;
// A 90-degree face at 1600 px samples 0.06 degrees, about what a terrestrial
// scan resolves at 10 m. Every face of one size shares a map size, which lets
// the trainer batch them without resampling across the holes.
constexpr int kDepthMapSide = 1600;

// The alignment check: a few images per kind, small maps, a million points.
constexpr int kCheckImages = 6;
constexpr int kCheckSide = 320;
constexpr int64_t kCheckPoints = 1000000;
// Another camera orientation replaces the assumed one only when it matches the
// photos by this much more, and well enough to believe at all.
constexpr double kCheckMargin = 0.1;
constexpr double kCheckFloor = 0.25;

uint64_t splitmix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

std::string file_stem(const e57::Image& im, size_t index, size_t count) {
    int width = 4;
    for (size_t n = count; n >= 10000; n /= 10) width++;
    char head[32];
    std::snprintf(head, sizeof head, "%0*zu", width, index);
    std::string s = head;
    const std::string& name = im.name.empty() ? im.guid : im.name;
    if (!name.empty()) s += '_';
    for (char c : name) {
        if (s.size() >= 64) break;
        s += (std::isalnum((unsigned char)c) || c == '-') ? c : '_';
    }
    return s;
}

void write_file(const fs::path& path, const std::vector<uint8_t>& bytes) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(bytes.data()), (std::streamsize)bytes.size());
    if (!f) throw std::runtime_error("cannot write " + path.string());
}

struct Frame {
    size_t index;
    E57Camera cam;
    std::string stem;
};

// The image file a frame is written as, in the dataset and in the preview frames.
std::string image_name(const e57::Image& im, const std::string& stem) {
    return stem + (im.png.empty() ? ".jpg" : ".png");
}

// The files of `dir` this run did not write: an earlier run's, of images this
// one leaves out, which masking would otherwise walk as frames.
void remove_others(const fs::path& dir, const std::set<std::string>& keep) {
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir, ec))
        if (e.is_regular_file(ec) && !keep.count(e.path().filename().string()))
            fs::remove(e.path(), ec);
}

// E57 marks a valid pixel with any non-zero value; the trainer keeps one from
// 128 up.
void write_mask(const std::vector<uint8_t>& png, const fs::path& path) {
    int w = 0, h = 0, c = 0;
    unsigned char* px = stbi_load_from_memory(png.data(), (int)png.size(), &w, &h, &c, 1);
    if (!px) throw std::runtime_error("E57: an image mask is not a readable PNG");
    for (int64_t i = 0; i < (int64_t)w * h; i++) px[i] = px[i] ? 255 : 0;
    const bool ok = stbi_write_png(path.string().c_str(), w, h, 1, px, w) != 0;
    stbi_image_free(px);
    if (!ok) throw std::runtime_error("cannot write " + path.string());
}

std::string skip_line(E57Skip why, const std::string& name, const e57::Image& im) {
    switch (why) {
        case E57Skip::NoPose:       return format(emsg::skip_no_pose, {name});
        case E57Skip::NoPixels:     return format(emsg::skip_no_pixels, {name});
        case E57Skip::Cylindrical:  return format(emsg::skip_cylindrical, {name});
        case E57Skip::Uncalibrated: return format(emsg::skip_uncalibrated, {name});
        case E57Skip::BadPinhole:   return format(emsg::skip_bad_pinhole, {name});
        case E57Skip::PartialPanorama: {
            char w[16], h[16];
            std::snprintf(w, sizeof w, "%.0f", im.pixel_width * (double)im.width * 180.0 / kPi);
            std::snprintf(h, sizeof h, "%.0f", im.pixel_height * (double)im.height * 180.0 / kPi);
            return format(emsg::skip_partial_panorama, {name, w, h});
        }
        case E57Skip::None: break;
    }
    return "";
}

// Which images become cameras, in file order; `log`, when given, hears why the
// others do not.
std::vector<Frame> plan_frames(const std::vector<e57::Image>& images, bool pinhole,
                               bool spherical,
                               const std::function<void(const std::string&)>& log) {
    std::vector<Frame> frames;
    for (size_t i = 0; i < images.size(); i++) {
        const e57::Image& im = images[i];
        E57Camera cam;
        const E57Skip why = e57_camera(im, cam);
        const std::string stem = file_stem(im, i, images.size());
        if (why != E57Skip::None) {
            if (log) log(skip_line(why, stem, im));
            continue;
        }
        if (im.projection == e57::Projection::Pinhole ? !pinhole : !spherical) continue;
        frames.push_back({i, cam, stem});
    }
    return frames;
}

std::string json_matrix(const double c2w[12]) {
    std::string s = "[";
    for (int r = 0; r < 4; r++) {
        s += r ? ", [" : "[";
        for (int c = 0; c < 4; c++) {
            const double v = r < 3 ? c2w[r * 4 + c] : (c == 3 ? 1.0 : 0.0);
            s += (c ? ", " : "") + json_number_exact(v);
        }
        s += "]";
    }
    return s + "]";
}

// A point is drawn when a hash of its serial number falls under the threshold,
// so a smaller draw is a subset of a larger one and a run is repeatable.
uint64_t draw_threshold(int64_t cap, int64_t total) {
    if (total <= cap) return std::numeric_limits<uint64_t>::max();
    return (uint64_t)std::min(std::ldexp((double)cap / (double)total, 64),
                              18446744073709549568.0);
}

// The 24 rotations that permute and flip a camera's axes, identity first.
std::vector<std::array<double, 9>> axis_rotations() {
    std::vector<std::array<double, 9>> out;
    int perm[3] = {0, 1, 2};
    do {
        for (int flip = 0; flip < 8; flip++) {
            std::array<double, 9> m{};
            for (int r = 0; r < 3; r++) m[r * 3 + perm[r]] = (flip >> r & 1) ? -1.0 : 1.0;
            const double det = m[0] * (m[4] * m[8] - m[5] * m[7]) -
                               m[1] * (m[3] * m[8] - m[5] * m[6]) +
                               m[2] * (m[3] * m[7] - m[4] * m[6]);
            if (det > 0) out.push_back(m);
        }
    } while (std::next_permutation(perm, perm + 3));
    return out;
}

// c2w <- c2w * [Q 0; 0 1], in the camera's OpenGL axes.
void turn_camera(E57Camera& c, const std::array<double, 9>& q) {
    for (int r = 0; r < 3; r++) {
        double row[3];
        for (int col = 0; col < 3; col++)
            row[col] = c.c2w[r * 4] * q[col] + c.c2w[r * 4 + 1] * q[3 + col] +
                       c.c2w[r * 4 + 2] * q[6 + col];
        for (int col = 0; col < 3; col++) c.c2w[r * 4 + col] = row[col];
    }
}

// `c` at `scale` of its size, OpenCV axes (OpenGL's with y and z negated).
MapCamera map_camera(const E57Camera& c, double scale) {
    MapCamera m;
    const bool pinhole = std::strcmp(c.model, "PINHOLE") == 0;
    m.model = (int)(pinhole ? CameraModelType::PINHOLE : CameraModelType::EQUIRECTANGULAR);
    m.width = std::max(1, (int)std::lround((double)c.width * scale));
    m.height = std::max(1, (int)std::lround((double)c.height * scale));
    const double sx = (double)m.width / (double)c.width, sy = (double)m.height / (double)c.height;
    m.fx = c.fx * sx; m.fy = c.fy * sy; m.cx = c.cx * sx; m.cy = c.cy * sy;
    for (int r = 0; r < 3; r++) {
        m.c2w[r * 4] = c.c2w[r * 4];
        m.c2w[r * 4 + 1] = -c.c2w[r * 4 + 1];
        m.c2w[r * 4 + 2] = -c.c2w[r * 4 + 2];
        m.c2w[r * 4 + 3] = c.c2w[r * 4 + 3];
    }
    return m;
}

// Rec. 709 luma of an image file, box-filtered down to `w` x `h`.
bool image_luma(const std::string& path, int w, int h, std::vector<float>& out) {
    int iw = 0, ih = 0, ic = 0;
    unsigned char* px = stbi_load(path.c_str(), &iw, &ih, &ic, 3);
    if (!px) return false;
    out.assign((size_t)w * h, 0.0f);
    std::vector<int> count((size_t)w * h, 0);
    for (int y = 0; y < ih; y++)
        for (int x = 0; x < iw; x++) {
            const unsigned char* p = px + ((size_t)y * iw + x) * 3;
            const size_t k = (size_t)(y * h / ih) * w + (size_t)(x * w / iw);
            out[k] += 0.2126f * p[0] + 0.7152f * p[1] + 0.0722f * p[2];
            count[k]++;
        }
    stbi_image_free(px);
    for (size_t k = 0; k < out.size(); k++) out[k] /= std::max(count[k], 1);
    return true;
}

// Pearson correlation of the front-most points' colour with the photo's, over
// the pixels a point lands in; NaN when too few do. Exposure-blind by design.
double color_correlation(const ScanCloud& cloud, const MapCamera& cam,
                         const std::vector<float>& luma) {
    std::vector<uint32_t> index;
    std::vector<float> depth;
    render_front(cloud, cam, cam.model != (int)CameraModelType::PINHOLE, index, depth);
    double n = 0, sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0;
    for (size_t k = 0; k < index.size(); k++) {
        if (index[k] == UINT32_MAX) continue;
        const uint8_t* c = &cloud.rgb[(size_t)index[k] * 3];
        const double x = 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2], y = luma[k];
        n++; sx += x; sy += y; sxx += x * x; syy += y * y; sxy += x * y;
    }
    if (n < 200) return std::numeric_limits<double>::quiet_NaN();
    const double vx = sxx - sx * sx / n, vy = syy - sy * sy / n;
    if (!(vx > 0 && vy > 0)) return std::numeric_limits<double>::quiet_NaN();
    return (sxy - sx * sy / n) / std::sqrt(vx * vy);
}

std::string two_places(double v) {
    char b[16];
    std::snprintf(b, sizeof b, "%.2f", v);
    return b;
}

void write_normal_png(const fs::path& path, const std::vector<float>& n, int w, int h) {
    // Black is the trainer's "no normal here"; a unit normal is byte/127.5 - 1.
    std::vector<uint8_t> px(n.size(), 0);
    for (size_t k = 0; k * 3 < n.size(); k++) {
        const float* v = &n[k * 3];
        if (v[0] * v[0] + v[1] * v[1] + v[2] * v[2] < 0.25f) continue;
        for (int c = 0; c < 3; c++)
            px[k * 3 + c] = (uint8_t)std::lround(std::clamp(127.5f + 127.5f * v[c], 0.0f, 255.0f));
    }
    if (!stbi_write_png(path.string().c_str(), w, h, 3, px.data(), w * 3))
        throw std::runtime_error("cannot write " + path.string());
}

}  // namespace


std::string default_e57_dataset_dir(const std::string& input) {
    return fs::path(input).replace_extension("").string();
}

E57Skip e57_camera(const e57::Image& im, E57Camera& out) {
    if (im.projection == e57::Projection::Cylindrical) return E57Skip::Cylindrical;
    if (im.projection == e57::Projection::Reference) return E57Skip::Uncalibrated;
    if (!im.has_pose) return E57Skip::NoPose;
    if (im.jpeg.empty() && im.png.empty()) return E57Skip::NoPixels;
    const bool pinhole = im.projection == e57::Projection::Pinhole;
    if (im.width <= 0 || im.height <= 0)
        return pinhole ? E57Skip::BadPinhole : E57Skip::PartialPanorama;
    out.width = im.width;
    out.height = im.height;
    const double* axes = kPinholeAxes;
    if (pinhole) {
        if (!(im.focal_length > 0 && im.pixel_width > 0 && im.pixel_height > 0))
            return E57Skip::BadPinhole;
        out.model = "PINHOLE";
        out.fx = im.focal_length / im.pixel_width;
        out.fy = im.focal_length / im.pixel_height;
        // E57 puts pixel centres on integers.
        out.cx = im.principal_x + 0.5;
        out.cy = im.principal_y + 0.5;
    } else {
        // The engine's panorama is the whole sphere; a band of one is not.
        const double span_w = im.pixel_width * (double)im.width / (2 * kPi);
        const double span_h = im.pixel_height * (double)im.height / kPi;
        if (std::fabs(span_w - 1) > 0.01 || std::fabs(span_h - 1) > 0.01)
            return E57Skip::PartialPanorama;
        out.model = "EQUIRECTANGULAR";
        out.fx = (double)im.width / (2 * kPi);
        out.fy = (double)im.height / kPi;
        out.cx = (double)im.width / 2;
        out.cy = (double)im.height / 2;
        axes = kSphericalAxes;
    }
    double R[9];
    im.pose.rotation(R);
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++)
            out.c2w[r * 4 + c] = R[r * 3] * axes[c] + R[r * 3 + 1] * axes[3 + c] +
                                 R[r * 3 + 2] * axes[6 + c];
        out.c2w[r * 4 + 3] = im.pose.t[r];
    }
    return E57Skip::None;
}

double thin_to_voxels(std::vector<double>& xyz, std::vector<uint8_t>& rgb,
                      int64_t target) {
    const size_t n = xyz.size() / 3;
    if (target <= 0 || (int64_t)n <= target) return 0.0;
    double lo[3], hi[3];
    for (int a = 0; a < 3; a++) {
        lo[a] = std::numeric_limits<double>::infinity();
        hi[a] = -lo[a];
    }
    for (size_t i = 0; i < n; i++)
        for (int a = 0; a < 3; a++) {
            lo[a] = std::min(lo[a], xyz[i * 3 + a]);
            hi[a] = std::max(hi[a], xyz[i * 3 + a]);
        }
    const double extent = std::max({hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2], 1e-9});

    constexpr uint64_t kCells = (uint64_t(1) << 21) - 1;   // 21 bits a key axis
    std::vector<uint64_t> keys(n);
    auto make_keys = [&](double edge) {
        const double inv = 1.0 / edge;
        for (size_t i = 0; i < n; i++) {
            uint64_t k = 0;
            for (int a = 0; a < 3; a++)
                k = (k << 21) | std::min(kCells, (uint64_t)((xyz[i * 3 + a] - lo[a]) * inv));
            keys[i] = k;
        }
    };
    auto occupied = [&](double edge) {
        make_keys(edge);
        std::sort(keys.begin(), keys.end());
        return (int64_t)(std::unique(keys.begin(), keys.end()) - keys.begin());
    };

    // The count falls as the voxel grows; bisect its edge in log space and
    // settle within 2% under the target.
    double fine = extent / (double)kCells, coarse = extent;
    if (occupied(fine) <= target) {
        coarse = fine;
    } else {
        for (int it = 0; it < 60 && coarse / fine > 1.0001; it++) {
            const double edge = std::sqrt(fine * coarse);
            const int64_t count = occupied(edge);
            if (count > target) {
                fine = edge;
            } else {
                coarse = edge;
                if ((double)count >= 0.98 * (double)target) break;
            }
        }
    }

    make_keys(coarse);
    std::vector<std::pair<uint64_t, uint32_t>> order(n);
    for (size_t i = 0; i < n; i++) order[i] = {keys[i], (uint32_t)i};
    std::sort(order.begin(), order.end());
    std::vector<double> out_xyz;
    std::vector<uint8_t> out_rgb;
    for (size_t b = 0; b < n;) {
        size_t e = b;
        double p[3] = {0, 0, 0}, c[3] = {0, 0, 0};
        for (; e < n && order[e].first == order[b].first; e++) {
            const size_t i = order[e].second;
            for (int a = 0; a < 3; a++) {
                p[a] += xyz[i * 3 + a];
                c[a] += rgb[i * 3 + a];
            }
        }
        const double inv = 1.0 / (double)(e - b);
        for (int a = 0; a < 3; a++) {
            out_xyz.push_back(p[a] * inv);
            out_rgb.push_back((uint8_t)std::lround(c[a] * inv));
        }
        b = e;
    }
    xyz.swap(out_xyz);
    rgb.swap(out_rgb);
    return coarse;
}

E57DatasetResult write_e57_dataset(const E57DatasetOptions& opt,
                                   const std::function<void(const std::string&)>& log,
                                   const std::atomic<bool>* cancel) {
    E57DatasetResult res;
    auto cancelled = [&] { return cancel && cancel->load(); };
    e57::Reader reader(opt.input);
    const std::vector<e57::Scan>& scans = reader.scans();
    const std::vector<e57::Image>& images = reader.images();

    int64_t n_pinhole = 0, n_spherical = 0;
    for (const e57::Image& im : images) {
        n_pinhole += im.projection == e57::Projection::Pinhole;
        n_spherical += im.projection == e57::Projection::Spherical;
    }
    log(format(emsg::summary, {(long long)scans.size(), (long long)reader.total_points(),
                               (long long)images.size()}));
    log(format(emsg::summary_images,
               {(long long)n_pinhole, (long long)n_spherical,
                (long long)images.size() - n_pinhole - n_spherical}));

    // Which images become cameras is settled before anything is written, so a
    // scan with nothing to train on leaves no folder behind.
    std::vector<Frame> frames = plan_frames(images, opt.pinhole, opt.spherical, log);
    if (frames.empty()) throw std::runtime_error(emsg::err_no_images.get());

    const fs::path out(opt.output);
    std::error_code ec;
    if (!opt.overwrite && fs::is_directory(out, ec) && !fs::is_empty(out, ec))
        throw std::runtime_error(format(emsg::err_not_empty, {out.string()}));
    // Masks are made of these images, by the scanner or by masking, and a new
    // run makes its own; corrections live in mask_edits/ and are re-applied.
    fs::remove_all(out / "masks", ec);
    fs::create_directories(out / "images", ec);
    if (ec) throw std::runtime_error("cannot create " + (out / "images").string());
    std::vector<std::string> image_paths(frames.size()), mask_paths(frames.size());
    std::set<std::string> written;
    for (size_t k = 0; k < frames.size(); k++) {
        if (cancelled()) { res.cancelled = true; return res; }
        log(format(emsg::progress_images, {(long long)k + 1, (long long)frames.size()}));
        const e57::Image& im = images[frames[k].index];
        const std::string name = image_name(im, frames[k].stem);
        image_paths[k] = "images/" + name;
        written.insert(name);
        write_file(out / image_paths[k], reader.read_blob(im.png.empty() ? im.jpeg : im.png));
        if (!im.mask.empty()) {
            fs::create_directories(out / "masks", ec);
            mask_paths[k] = "masks/" + frames[k].stem + ".png";
            write_mask(reader.read_blob(im.mask), out / mask_paths[k]);
        }
    }
    remove_others(out / "images", written);

    // One pass over the points feeds everything that needs them: the seed
    // cloud, the cloud the depth maps are rendered from, and the check.
    const int64_t total = reader.total_points();
    const bool seeds = opt.all_points || opt.seed_points > 0;
    const uint64_t seed_below = opt.all_points
        ? std::numeric_limits<uint64_t>::max()
        : draw_threshold(std::clamp<int64_t>(8 * opt.seed_points, kPreThinFloor,
                                             kPreThinCeiling), total);
    const uint64_t depth_below =
        draw_threshold(opt.depth_maps ? kDepthCloudCap : kCheckPoints, total);
    std::vector<double> seed_xyz, depth_xyz;
    std::vector<uint8_t> seed_rgb, depth_rgb;
    uint64_t serial = 0;
    for (size_t s = 0; s < scans.size(); s++) {
        log(format(emsg::progress_points, {(long long)s + 1, (long long)scans.size()}));
        const bool whole = reader.read_points(scans[s], [&](const e57::Point* p, size_t n) {
            for (size_t i = 0; i < n; i++) {
                const uint64_t h = splitmix64(serial++);
                if (seeds && h <= seed_below) {
                    seed_xyz.insert(seed_xyz.end(), p[i].xyz, p[i].xyz + 3);
                    seed_rgb.insert(seed_rgb.end(), p[i].rgb, p[i].rgb + 3);
                }
                if (h <= depth_below) {
                    depth_xyz.insert(depth_xyz.end(), p[i].xyz, p[i].xyz + 3);
                    depth_rgb.insert(depth_rgb.end(), p[i].rgb, p[i].rgb + 3);
                }
            }
        }, cancel);
        if (!whole) { res.cancelled = true; return res; }
    }
    const ScanCloud cloud = build_scan_cloud(depth_xyz, depth_rgb, 2.0);
    std::vector<double>().swap(depth_xyz);
    std::vector<uint8_t>().swap(depth_rgb);

    bool any_color = false;
    for (const e57::Scan& sc : scans) any_color = any_color || sc.has_color;
    if (cloud.size() == 0) {
        res.check = E57Check::NotRun;
    } else if (!any_color) {
        res.check = E57Check::NoColor;
        log(emsg::check_no_color.get());
    } else {
        log(emsg::check_running.get());
        std::vector<double> sub_xyz;
        std::vector<uint8_t> sub_rgb;
        const size_t step = std::max<size_t>(1, cloud.size() / (size_t)kCheckPoints);
        for (size_t i = 0; i < cloud.size(); i += step)
            for (int a = 0; a < 3; a++) {
                sub_xyz.push_back(cloud.origin[a] + cloud.xyz[i * 3 + a]);
                sub_rgb.push_back(cloud.rgb[i * 3 + a]);
            }
        const ScanCloud probe_cloud = build_scan_cloud(sub_xyz, sub_rgb, 2.0);
        const std::vector<std::array<double, 9>> turns = axis_rotations();
        res.check = E57Check::Agree;
        for (int kind = 0; kind < 2; kind++) {
            std::vector<size_t> members;
            for (size_t k = 0; k < frames.size(); k++)
                if ((std::strcmp(frames[k].cam.model, "PINHOLE") == 0) == (kind == 0))
                    members.push_back(k);
            if (members.empty()) continue;
            struct Probe {
                E57Camera cam;
                double scale;
                std::vector<float> luma;
            };
            std::vector<Probe> probes;
            const size_t n_probe = std::min<size_t>(kCheckImages, members.size());
            for (size_t j = 0; j < n_probe; j++) {
                const size_t k = members[j * members.size() / n_probe];
                const E57Camera& c = frames[k].cam;
                Probe pr{c, std::min(1.0, (double)kCheckSide / (double)std::max(c.width, c.height)), {}};
                const MapCamera m = map_camera(c, pr.scale);
                if (image_luma((out / image_paths[k]).string(), m.width, m.height, pr.luma))
                    probes.push_back(std::move(pr));
            }
            if (probes.empty()) continue;
            std::vector<double> score(turns.size(), -1.0);
#pragma omp parallel for schedule(dynamic, 1)
            for (int t = 0; t < (int)turns.size(); t++) {
                double sum = 0;
                int n = 0;
                for (const Probe& pr : probes) {
                    E57Camera c = pr.cam;
                    turn_camera(c, turns[(size_t)t]);
                    const double v = color_correlation(probe_cloud, map_camera(c, pr.scale), pr.luma);
                    if (v == v) { sum += v; n++; }
                }
                if (n) score[(size_t)t] = sum / n;
            }
            const size_t best = (size_t)(std::max_element(score.begin(), score.end()) - score.begin());
            if (best != 0 && score[best] >= kCheckFloor && score[best] - score[0] >= kCheckMargin) {
                for (size_t k : members) turn_camera(frames[k].cam, turns[best]);
                log(format(kind == 0 ? emsg::check_fixed_pinhole : emsg::check_fixed_panorama,
                           {two_places(score[0]), two_places(score[best])}));
                res.check = E57Check::Fixed;
            } else if (score[0] >= kCheckFloor) {
                log(format(emsg::check_ok, {two_places(score[0])}));
            } else {
                log(format(emsg::check_unsure, {two_places(score[0])}));
                if (res.check != E57Check::Fixed) res.check = E57Check::Unsure;
            }
        }
    }

    std::vector<std::string> depth_paths(frames.size()), normal_paths(frames.size());
    if (opt.depth_maps && cloud.size() > 0) {
        fs::create_directories(out / "depths", ec);
        fs::create_directories(out / "normals", ec);
        int64_t n_spherical = 0;
        for (const Frame& f : frames) n_spherical += std::strcmp(f.cam.model, "PINHOLE") != 0;
        // The trainer reads every map of a dataset one way, by majority
        // (TrainerCore.cpp resolve_ray_depth); a panorama's split needs ray depth.
        const bool ray = n_spherical * 2 > (int64_t)frames.size();
        const int n = (int)frames.size();
        std::mutex mu;
        std::exception_ptr failure;
        int64_t done = 0;
#pragma omp parallel for schedule(dynamic, 1)
        for (int k = 0; k < n; k++) {
            if (cancelled()) continue;
            {
                std::lock_guard<std::mutex> lk(mu);
                if (failure) continue;
            }
            try {
                const E57Camera& c = frames[(size_t)k].cam;
                const MapCamera m = map_camera(
                    c, std::min(1.0, (double)kDepthMapSide / (double)std::max(c.width, c.height)));
                std::vector<float> depth, normal;
                render_depth_normal(cloud, m, ray, depth, normal);
                // 16 bits of millimetres end at 65.5 m; further is left as no
                // data rather than clamped onto a wall at that range.
                std::vector<uint16_t> mm(depth.size(), 0);
                for (size_t j = 0; j < depth.size(); j++) {
                    const double v = depth[j] * 1000.0;
                    if (depth[j] > 0 && v <= 65535.0)
                        mm[j] = (uint16_t)std::max<long>(1, std::lround(v));
                }
                const std::string dp = "depths/" + frames[(size_t)k].stem + ".png";
                const std::string np = "normals/" + frames[(size_t)k].stem + ".png";
                if (!save_depth_png16((out / dp).string(), mm.data(), m.width, m.height))
                    throw std::runtime_error("cannot write " + (out / dp).string());
                write_normal_png(out / np, normal, m.width, m.height);
                std::lock_guard<std::mutex> lk(mu);
                depth_paths[(size_t)k] = dp;
                normal_paths[(size_t)k] = np;
                log(format(emsg::progress_depth, {(long long)++done, (long long)n}));
            } catch (...) {
                std::lock_guard<std::mutex> lk(mu);
                if (!failure) failure = std::current_exception();
            }
        }
        if (failure) std::rethrow_exception(failure);
        if (cancelled()) { res.cancelled = true; return res; }
        res.depth_maps = done;
        std::set<std::string> maps;
        for (const Frame& f : frames) maps.insert(f.stem + ".png");
        remove_others(out / "depths", maps);
        remove_others(out / "normals", maps);
    } else {
        fs::remove_all(out / "depths", ec);
        fs::remove_all(out / "normals", ec);
    }

    if (seeds && !seed_xyz.empty()) {
        if (!opt.all_points) {
            log(format(emsg::thinning, {(long long)serial, (long long)opt.seed_points}));
            res.voxel = thin_to_voxels(seed_xyz, seed_rgb, opt.seed_points);
        }
        res.seed_points = (int64_t)seed_xyz.size() / 3;
        // Double precision: a geo-referenced scan sits millions of metres out.
        spirula::write_ply_points((out / "sparse_pc.ply").string(), seed_xyz.data(),
                                  seed_rgb.data(), res.seed_points, nullptr, nullptr, true);
        if (res.voxel > 0) {
            char edge[32];
            std::snprintf(edge, sizeof edge, "%.3g", res.voxel);
            log(format(emsg::seed_done, {(long long)res.seed_points, edge}));
        } else {
            log(format(emsg::seed_all, {(long long)res.seed_points}));
        }
    }

    JsonWriter w;
    w.object();
    if (res.seed_points > 0) w.field("ply_file_path", "sparse_pc.ply");
    w.key("frames").array();
    for (size_t k = 0; k < frames.size(); k++) {
        const E57Camera& c = frames[k].cam;
        w.object();
        w.field("file_path", image_paths[k]);
        if (!mask_paths[k].empty()) w.field("mask_path", mask_paths[k]);
        if (!depth_paths[k].empty()) w.field("depth_file_path", depth_paths[k]);
        if (!normal_paths[k].empty()) w.field("normal_file_path", normal_paths[k]);
        w.field("camera_model", c.model);
        w.field_raw("fl_x", json_number_exact(c.fx));
        w.field_raw("fl_y", json_number_exact(c.fy));
        w.field_raw("cx", json_number_exact(c.cx));
        w.field_raw("cy", json_number_exact(c.cy));
        w.field("w", (long long)c.width);
        w.field("h", (long long)c.height);
        w.field_raw("transform_matrix", json_matrix(c.c2w));
        w.end();
    }
    w.end();
    w.end();
    {
        std::ofstream f(out / "transforms.json", std::ios::trunc);
        f << w.str();
        if (!f) throw std::runtime_error("cannot write " + (out / "transforms.json").string());
    }
    {
        // E57 is metres by definition, and scanners level their frame
        // (docs/datasets.md, "The frame").
        std::ofstream f(out / "gauge.txt", std::ios::trunc);
        f << "# What this dataset's frame means, from an E57 scan.\n"
             "oriented 1\nmetric 1\nup e57\nscale e57\n";
    }
    res.images = (int64_t)frames.size();
    log(format(emsg::done, {out.string(), (long long)res.images, (long long)res.seed_points}));
    return res;
}

bool extract_e57_images(const std::string& input, const std::string& images_dir,
                        bool pinhole, bool spherical, const std::atomic<bool>* cancel) {
    e57::Reader reader(input);
    const std::vector<Frame> frames = plan_frames(reader.images(), pinhole, spherical, {});
    std::error_code ec;
    fs::remove_all(images_dir, ec);
    fs::create_directories(images_dir, ec);
    if (ec) throw std::runtime_error("cannot create " + images_dir);
    for (const Frame& f : frames) {
        if (cancel && cancel->load()) return false;
        const e57::Image& im = reader.images()[f.index];
        write_file(fs::path(images_dir) / image_name(im, f.stem),
                   reader.read_blob(im.png.empty() ? im.jpeg : im.png));
    }
    return true;
}

E57Preview read_e57_preview(const std::string& path, int64_t max_points,
                            const std::atomic<bool>* cancel) {
    E57Preview p;
    e57::Reader reader(path);
    for (const e57::Image& im : reader.images()) {
        E57Camera c;
        if (e57_camera(im, c) == E57Skip::None) p.cameras.push_back(c);
    }
    const uint64_t below = draw_threshold(max_points, reader.total_points());
    uint64_t serial = 0;
    for (const e57::Scan& s : reader.scans()) {
        const bool whole = reader.read_points(s, [&](const e57::Point* pt, size_t n) {
            for (size_t i = 0; i < n; i++) {
                if (splitmix64(serial++) > below) continue;
                p.xyz.insert(p.xyz.end(), pt[i].xyz, pt[i].xyz + 3);
                p.rgb.insert(p.rgb.end(), pt[i].rgb, pt[i].rgb + 3);
            }
        }, cancel);
        if (!whole) return {};
    }
    return p;
}

}  // namespace app

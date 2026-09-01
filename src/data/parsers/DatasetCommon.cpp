// DatasetCommon.cpp -- shared dataset-bake helpers (see DatasetParser.h),
// used by every format parser. Also built into the WebAssembly viewer, so
// nothing here may reach the engine or the host camera math.

#include "data/DatasetParser.h"
#include "data/SourceCamera.h"
#include "i18n/catalog/Data.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <set>
#include <stdexcept>

namespace fs = std::filesystem;

constexpr double kPi = 3.14159265358979323846;   // MSVC has no M_PI by default


namespace dsparse {

// ---------------------------------------------------------------------------
// Normalized-frame scale:
//   up      = normalize(mean of c2w Y columns)
//   R_align = rotation taking `up` to +Z (Rodrigues)
//   center  = mean camera position
//   scale_factor = 1 / max |R_align @ (pos - center)|
// TODO: "pca" / "vertical" / "gsplat" orientation_method and "focus" /
// "gsplat" center_method. This implements the up/poses pair only;
// check_config() warns when the config asks for anything else. The reference
// implementation for the rest is kept in Python, on no code path:
// reference/python/camera_utils.py plus the call-site algebra in
// docs/notes/pose-normalization.md.
// ---------------------------------------------------------------------------
double compute_normalized_transform(const std::vector<float>& c2w, int64_t n,
                                     double T_out[16]) {
    std::fill(T_out, T_out + 16, 0.0);
    T_out[0] = T_out[5] = T_out[10] = T_out[15] = 1.0;
    if (n <= 0) return 1.0;
    double up[3] = {0, 0, 0}, center[3] = {0, 0, 0};
    for (int64_t i = 0; i < n; i++) {
        for (int r = 0; r < 3; r++) {
            up[r]     += c2w[i*12 + r*4 + 1];
            center[r] += c2w[i*12 + r*4 + 3];
        }
    }
    double un = std::sqrt(up[0]*up[0] + up[1]*up[1] + up[2]*up[2]);
    for (auto& u : up) u /= std::max(un, 1e-12);
    for (auto& c : center) c /= (double)n;

    double axis[3] = {up[1], -up[0], 0.0};              // up x z
    double s = std::sqrt(axis[0]*axis[0] + axis[1]*axis[1]);
    double c = up[2];
    double R[3][3] = {{1,0,0},{0,1,0},{0,0,1}};
    if (s > 1e-12) {
        for (auto& a : axis) a /= s;
        double C = 1.0 - c;
        double Rr[3][3] = {
            {c + axis[0]*axis[0]*C,         axis[0]*axis[1]*C - axis[2]*s, axis[1]*s},
            {axis[0]*axis[1]*C + axis[2]*s, c + axis[1]*axis[1]*C,        -axis[0]*s},
            {-axis[1]*s,                    axis[0]*s,                     c},
        };
        std::copy(&Rr[0][0], &Rr[0][0] + 9, &R[0][0]);
    } else if (c < 0.0) {
        R[1][1] = -1.0; R[2][2] = -1.0;                 // up == -z: flip
    }

    double max_abs = 0.0;
    for (int64_t i = 0; i < n; i++) {
        double d[3];
        for (int r = 0; r < 3; r++) d[r] = c2w[i*12 + r*4 + 3] - center[r];
        for (int r = 0; r < 3; r++)
            max_abs = std::max(max_abs,
                std::abs(R[r][0]*d[0] + R[r][1]*d[1] + R[r][2]*d[2]));
    }
    double scale_factor = 1.0 / std::max(max_abs, 1e-12);

    // T_n_from_camera = scale * [R_align | -R_align @ center]
    for (int r = 0; r < 3; r++) {
        double t = 0.0;
        for (int col = 0; col < 3; col++) {
            T_out[r*4 + col] = R[r][col] * scale_factor;
            t -= R[r][col] * center[col];
        }
        T_out[r*4 + 3] = t * scale_factor;
    }
    return scale_factor;
}

double compute_normalized_scale_factor(const std::vector<float>& c2w, int64_t n) {
    double T[16];
    return compute_normalized_transform(c2w, n, T);
}

void invert_affine4x4(const double in[16], double out[16]) {
    double a = in[0], b = in[1], c = in[2],
           d = in[4], e = in[5], g = in[6],
           h = in[8], i = in[9], j = in[10];
    double det = a*(e*j - g*i) - b*(d*j - g*h) + c*(d*i - e*h);
    if (std::abs(det) < 1e-30)
        throw std::runtime_error("invert_affine4x4: singular matrix");
    double inv = 1.0 / det;
    double Ai[3][3] = {
        {(e*j - g*i)*inv, (c*i - b*j)*inv, (b*g - c*e)*inv},
        {(g*h - d*j)*inv, (a*j - c*h)*inv, (c*d - a*g)*inv},
        {(d*i - e*h)*inv, (b*h - a*i)*inv, (a*e - b*d)*inv},
    };
    for (int r = 0; r < 3; r++) {
        for (int col = 0; col < 3; col++) out[r*4 + col] = Ai[r][col];
        out[r*4 + 3] = -(Ai[r][0]*in[3] + Ai[r][1]*in[7] + Ai[r][2]*in[11]);
    }
    out[12] = out[13] = out[14] = 0.0;
    out[15] = 1.0;
}


// ---------------------------------------------------------------------------
// eval_mode train subset
// ---------------------------------------------------------------------------
// numpy's `np.linspace(0, n-1, count, dtype=int)`, the index picker both the
// eval-mode fraction split and the validation split use: compute
// (k * (n-1)) / (count-1) in that order, TRUNCATE toward zero (not round),
// and pin the last sample to n-1 exactly. Rounding instead selects different
// frames for most (n, count) pairs -- a different train/val split.
static std::vector<int64_t> linspace_indices(int64_t n, int64_t count) {
    std::vector<int64_t> out;
    if (count <= 0 || n <= 0) return out;
    out.reserve(count);
    for (int64_t k = 0; k < count; k++) {
        if (count == 1) { out.push_back(0); continue; }
        if (k == count - 1) { out.push_back(n - 1); continue; }
        out.push_back((int64_t)(((double)k * (double)(n - 1))
                                / (double)(count - 1)));
    }
    return out;
}

std::vector<int64_t> train_subset(int64_t n, const std::vector<std::string>& names,
                                  const DatasetParserConfig& cfg) {
    std::vector<int64_t> keep;
    if (cfg.eval_mode == "all") {
        keep.resize(n);
        for (int64_t i = 0; i < n; i++) keep[i] = i;
    } else if (cfg.eval_mode == "fraction") {
        int64_t num_train = (int64_t)std::ceil((double)n * cfg.train_split_fraction);
        std::vector<char> flag(n, 0);
        for (int64_t idx : linspace_indices(n, num_train)) flag[idx] = 1;
        for (int64_t i = 0; i < n; i++) if (flag[i]) keep.push_back(i);
    } else if (cfg.eval_mode == "interval") {
        for (int64_t i = 0; i < n; i++)
            if (cfg.eval_interval <= 0 || i % cfg.eval_interval != 0)
                keep.push_back(i);
    } else if (cfg.eval_mode == "filename") {
        for (int64_t i = 0; i < n; i++) {
            std::string base = fs::path(names[i]).filename().string();
            if (base.find("train") != std::string::npos) keep.push_back(i);
            else if (base.find("eval") == std::string::npos)
                throw std::runtime_error(
                    "eval_mode=filename requires 'train'/'eval' in every image "
                    "name; got " + names[i]);
        }
    } else {
        throw std::runtime_error("unknown eval_mode " + cfg.eval_mode);
    }
    if (keep.empty())
        throw std::runtime_error("eval_mode split left no training images");

    if (cfg.split == "eval") {
        // eval_mode="all" means "all images for any split", so the eval side
        // is the full set, not the empty complement.
        if (cfg.eval_mode == "all") return keep;
        std::vector<char> is_train(n, 0);
        for (int64_t i : keep) is_train[i] = 1;
        std::vector<int64_t> other;
        for (int64_t i = 0; i < n; i++) if (!is_train[i]) other.push_back(i);
        // Legal to be empty (e.g. train_split_fraction=1.0); the caller's cue
        // to skip eval entirely.
        return other;
    }
    if (cfg.split != "train")
        throw std::runtime_error("unknown split '" + cfg.split +
                                 "' (expected 'train' or 'eval')");
    return keep;
}


void assign_val_split(ParsedDataset& ds, float validation_fraction) {
    const int64_t n = ds.num_cameras;
    std::vector<char> is_val(n, 0);
    if (validation_fraction > 0.0f && n > 1) {
        int64_t num_train = (int64_t)std::ceil((double)n * (1.0 - validation_fraction));
        std::vector<char> is_train(n, 0);
        for (int64_t idx : linspace_indices(n, num_train)) is_train[idx] = 1;
        for (int64_t i = 0; i < n; i++) is_val[i] = !is_train[i];
    }
    ds.train_indices.clear();
    ds.val_indices.clear();
    for (int64_t i = 0; i < n; i++)
        (is_val[i] ? ds.val_indices : ds.train_indices).push_back((int32_t)i);
}


// ---------------------------------------------------------------------------
// Auxiliary buffer discovery
// ---------------------------------------------------------------------------
std::string find_aux_file(const std::string& aux_dir_s, const std::string& rel_name,
                          const char* suffix_tag) {
    fs::path aux_dir(aux_dir_s);
    if (!fs::is_directory(aux_dir)) return "";
    fs::path rel(rel_name);
    std::string stem_rel = (rel.parent_path() / rel.stem()).string();
    const std::string exts[] = {".png", ".PNG", ".jpg", ".JPG", ".jpeg", ".JPEG"};
    std::vector<std::string> candidates;
    for (const auto& e : exts) {
        candidates.push_back(rel_name + e);   // image.jpg.png
        candidates.push_back(stem_rel + e);   // image.png
    }
    candidates.push_back(stem_rel + "_" + suffix_tag + ".png");   // image_mask.png
    for (const auto& cand : candidates) {
        fs::path p = aux_dir / cand;
        if (fs::exists(p)) return p.string();
    }
    return "";
}


// ---------------------------------------------------------------------------
// Outlier rejection via geometric median
// ---------------------------------------------------------------------------
namespace {
double median_of(std::vector<double> v) {
    if (v.empty()) return 0.0;
    size_t mid = v.size() / 2;
    std::nth_element(v.begin(), v.begin() + mid, v.end());
    double hi = v[mid];
    if (v.size() % 2 == 1) return hi;
    double lo = *std::max_element(v.begin(), v.begin() + mid);
    return 0.5 * (lo + hi);
}
}  // namespace

std::vector<char> outlier_keep_mask(const std::vector<double>& pos,
                                    int64_t n, float threshold) {
    std::vector<char> keep(n, 1);
    if (!(threshold < std::numeric_limits<float>::infinity()) || n == 0)
        return keep;

    // Geometric median via Weiszfeld with the zero-distance correction
    // (eps=0, maxiter=10).
    double y[3];
    for (int d = 0; d < 3; d++) {
        std::vector<double> col(n);
        for (int64_t i = 0; i < n; i++) col[i] = pos[i*3 + d];
        y[d] = median_of(col);
    }
    for (int it = 0; it < 10; it++) {
        double T[3] = {0, 0, 0}, Dinvs = 0.0;
        int64_t num_zeros = 0;
        for (int64_t i = 0; i < n; i++) {
            double dx = pos[i*3] - y[0], dy = pos[i*3+1] - y[1], dz = pos[i*3+2] - y[2];
            double D = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (D == 0.0) { num_zeros++; continue; }
            double w = 1.0 / D;
            Dinvs += w;
            for (int d = 0; d < 3; d++) T[d] += w * pos[i*3 + d];
        }
        double y1[3];
        if (num_zeros == n) break;
        if (Dinvs > 0) for (int d = 0; d < 3; d++) T[d] /= Dinvs;
        if (num_zeros == 0) {
            for (int d = 0; d < 3; d++) y1[d] = T[d];
        } else {
            double R[3], r = 0.0;
            for (int d = 0; d < 3; d++) { R[d] = (T[d] - y[d]) * Dinvs; r += R[d]*R[d]; }
            r = std::sqrt(r);
            double rinv = (r == 0.0) ? 0.0 : (double)num_zeros / r;
            double a = std::max(0.0, 1.0 - rinv), b = std::min(1.0, rinv);
            for (int d = 0; d < 3; d++) y1[d] = a * T[d] + b * y[d];
        }
        double diff = 0.0;
        for (int d = 0; d < 3; d++) {
            diff += (y[d] - y1[d]) * (y[d] - y1[d]);
            y[d] = y1[d];
        }
        if (diff == 0.0) break;
    }

    std::vector<double> dist(n);
    for (int64_t i = 0; i < n; i++) {
        double dx = pos[i*3] - y[0], dy = pos[i*3+1] - y[1], dz = pos[i*3+2] - y[2];
        dist[i] = std::sqrt(dx*dx + dy*dy + dz*dz);
    }
    double mad = median_of(dist);
    for (int64_t i = 0; i < n; i++)
        keep[i] = dist[i] <= (double)threshold * mad;
    return keep;
}

// ---------------------------------------------------------------------------
// Fitting a camera to its image
// ---------------------------------------------------------------------------

namespace {

std::string size_str(double w, double h) {
    return std::to_string((long long)w) + "x" + std::to_string((long long)h);
}

// One line per distinct (kind, image, camera) size pair: a dataset is
// thousands of frames over a handful of cameras, and the pair is what the
// reader acts on.
bool first_time(int kind, int iw, int ih, double W, double H) {
    static std::set<std::array<int, 5>> seen;
    return seen.insert({kind, iw, ih, (int)W, (int)H}).second;
}

void warn(const spirula::i18n::Msg& m, std::initializer_list<spirula::i18n::Arg> args) {
    std::printf("%s %s\n", spirula::i18n::msg::data::word_warning.get(),
                spirula::i18n::format(m, args).c_str());
}

}  // namespace

void fit_camera_resolution(const DatasetParserConfig& cfg,
                           const std::string& image_path,
                           double& W, double& H,
                           double& fx, double& fy, double& cx, double& cy,
                           RedistortSource* src)
{
    if (!(W > 0.0) || !(H > 0.0)) return;
    double tw = W, th = H;
    int iw = 0, ih = 0;
    if (cfg.probe_image_size && !image_path.empty() &&
        cfg.probe_image_size(image_path.c_str(), &iw, &ih) && iw > 0 && ih > 0) {
        tw = std::min(tw, (double)iw);
        th = std::min(th, (double)ih);
        // The height the width ratio implies, against the height on disk: one
        // pixel of slack for the rounding an honest downscaler does.
        if (std::fabs(H * ((double)iw / W) - (double)ih) > 1.0 &&
            first_time(0, iw, ih, W, H))
            warn(spirula::i18n::msg::data::camera_image_aspect,
                 {image_path, size_str(iw, ih), size_str(W, H)});
    }

    const double s = (double)cfg.train_resolution_divisor;
    if (s > 1.0) {
        auto round_dim = [&](double v) {
            if (cfg.downscale_rounding_mode == "ceil")  return std::ceil(v / s);
            if (cfg.downscale_rounding_mode == "round") return std::round(v / s);
            return std::floor(v / s);
        };
        tw = std::max(1.0, round_dim(tw));
        th = std::max(1.0, round_dim(th));
    }
    if (tw == W && th == H) return;

    if (iw > 0 && (iw != (int)W || ih != (int)H) && first_time(1, iw, ih, W, H))
        warn(spirula::i18n::msg::data::camera_fit_to_image,
             {image_path, size_str(iw, ih), size_str(W, H), size_str(tw, th)});

    const double sx = tw / W, sy = th / H;
    fx *= sx; cx *= sx;
    fy *= sy; cy *= sy;
    if (src && src->source_model >= 0)
        srccam::rescale(src->source_model, src->params, sx, sy);
    W = tw; H = th;
}

}  // namespace dsparse

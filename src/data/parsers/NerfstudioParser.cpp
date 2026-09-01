// NerfstudioParser.cpp -- transforms.json dataset reader for DatasetParser.h,
// with a self-contained PLY point-cloud reader. JSON via app/Json.h; no
// external dependencies.

#include "data/DatasetParser.h"
#include "i18n/catalog/Data.h"
#include "data/FastFloat.h"
#include "data/Json.h"

#include "core/CameraModel.h"   // camera_model_from_name (CUDA-free)
#include "data/DistortionFit.h"
#include "data/SourceCamera.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>

namespace dmsg = spirula::i18n::msg::data;

namespace fs = std::filesystem;

constexpr double kPi = 3.14159265358979323846;   // MSVC has no M_PI by default


// ===========================================================================
// PLY reader (ascii + binary_little_endian). Reads x/y/z (+ red/green/blue)
// from the "vertex" element; fixed-size properties only.
// ===========================================================================

namespace {

enum class PlyType { I8, U8, I16, U16, I32, U32, F32, F64 };

struct PlyProp { std::string name; PlyType type; };

size_t ply_type_size(PlyType t) {
    switch (t) {
        case PlyType::I8: case PlyType::U8: return 1;
        case PlyType::I16: case PlyType::U16: return 2;
        case PlyType::I32: case PlyType::U32: case PlyType::F32: return 4;
        case PlyType::F64: return 8;
    }
    return 0;
}

PlyType ply_type_from_name(const std::string& s) {
    if (s == "char"   || s == "int8")    return PlyType::I8;
    if (s == "uchar"  || s == "uint8")   return PlyType::U8;
    if (s == "short"  || s == "int16")   return PlyType::I16;
    if (s == "ushort" || s == "uint16")  return PlyType::U16;
    if (s == "int"    || s == "int32")   return PlyType::I32;
    if (s == "uint"   || s == "uint32")  return PlyType::U32;
    if (s == "float"  || s == "float32") return PlyType::F32;
    if (s == "double" || s == "float64") return PlyType::F64;
    throw std::runtime_error("PLY: unknown property type " + s);
}

double ply_read_scalar(const uint8_t* p, PlyType t) {
    switch (t) {
        case PlyType::I8:  return *(const int8_t*)p;
        case PlyType::U8:  return *(const uint8_t*)p;
        case PlyType::I16: { int16_t v; std::memcpy(&v, p, 2); return v; }
        case PlyType::U16: { uint16_t v; std::memcpy(&v, p, 2); return v; }
        case PlyType::I32: { int32_t v; std::memcpy(&v, p, 4); return v; }
        case PlyType::U32: { uint32_t v; std::memcpy(&v, p, 4); return v; }
        case PlyType::F32: { float v; std::memcpy(&v, p, 4); return v; }
        case PlyType::F64: { double v; std::memcpy(&v, p, 8); return v; }
    }
    return 0.0;
}

struct PlyElement {
    std::string name;
    int64_t count = 0;
    std::vector<PlyProp> props;
    bool has_list = false;
    size_t stride() const {
        size_t s = 0;
        for (const auto& p : props) s += ply_type_size(p.type);
        return s;
    }
    int find(const char* n) const {
        for (size_t i = 0; i < props.size(); i++)
            if (props[i].name == n) return (int)i;
        return -1;
    }
};

std::string read_header_line(FILE* f, const std::string& path) {
    std::string line;
    for (;;) {
        int c = std::fgetc(f);
        if (c == EOF) throw std::runtime_error("PLY: truncated header in " + path);
        if (c == '\n') break;
        if (c != '\r') line += (char)c;
    }
    return line;
}

std::vector<std::string> split_ws(const std::string& s) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) i++;
        size_t j = i;
        while (j < s.size() && s[j] != ' ' && s[j] != '\t') j++;
        if (j > i) out.push_back(s.substr(i, j - i));
        i = j;
    }
    return out;
}

}  // namespace


ColmapPoints3D read_ply_points(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) throw std::runtime_error("PLY: cannot open " + path);
    struct Closer { FILE* f; ~Closer() { std::fclose(f); } } closer{f};

    // ---- Header ------------------------------------------------------------
    if (read_header_line(f, path) != "ply")
        throw std::runtime_error("PLY: missing magic in " + path);
    bool binary = false;
    std::vector<PlyElement> elements;
    for (;;) {
        std::string line = read_header_line(f, path);
        auto tok = split_ws(line);
        if (tok.empty()) continue;
        if (tok[0] == "comment" || tok[0] == "obj_info") continue;
        if (tok[0] == "format") {
            if (tok.size() < 2) throw std::runtime_error("PLY: bad format line");
            if (tok[1] == "ascii") binary = false;
            else if (tok[1] == "binary_little_endian") binary = true;
            else throw std::runtime_error("PLY: unsupported format " + tok[1] +
                                          " (big-endian not supported)");
        } else if (tok[0] == "element") {
            PlyElement e;
            e.name = tok[1];
            e.count = std::stoll(tok[2]);
            elements.push_back(std::move(e));
        } else if (tok[0] == "property") {
            if (elements.empty()) throw std::runtime_error("PLY: property before element");
            if (tok[1] == "list") { elements.back().has_list = true; continue; }
            if (tok.size() < 3) throw std::runtime_error("PLY: bad property line");
            elements.back().props.push_back({tok[2], ply_type_from_name(tok[1])});
        } else if (tok[0] == "end_header") {
            break;
        }
    }

    // ---- Slurp the data section once ----------------------------------------
    // Per-row fgetc/fread through the stdio layer (worse still under
    // Emscripten's virtual FS) made large ascii clouds take tens of seconds;
    // reading the rest of the file into one buffer and pointer-walking it
    // parses the same 60+ MB in well under a second.
    long data_off = std::ftell(f);
    std::fseek(f, 0, SEEK_END);
    long fsize = std::ftell(f);
    std::fseek(f, data_off, SEEK_SET);
    std::string buf;
    buf.resize((size_t)(fsize > data_off ? fsize - data_off : 0));
    if (!buf.empty() && std::fread(&buf[0], 1, buf.size(), f) != buf.size())
        throw std::runtime_error("PLY: cannot read " + path);
    const char* p = buf.data();
    const char* end = buf.data() + buf.size();

    // ---- Locate the vertex element ------------------------------------------
    ColmapPoints3D pts;
    for (const auto& el : elements) {
        if (el.name != "vertex") {
            // Skip a preceding element; only possible with fixed-size props.
            if (el.has_list)
                throw std::runtime_error(
                    "PLY: list property before vertex element not supported in " + path);
            if (binary) {
                p += el.count * el.stride();
                if (p > end) throw std::runtime_error("PLY: truncated " + path);
            } else {
                for (int64_t i = 0; i < el.count; i++) {
                    const char* nl = (const char*)std::memchr(p, '\n', end - p);
                    if (!nl) throw std::runtime_error("PLY: truncated " + path);
                    p = nl + 1;
                }
            }
            continue;
        }
        if (el.has_list)
            throw std::runtime_error("PLY: list property in vertex element of " + path);
        int ix = el.find("x"), iy = el.find("y"), iz = el.find("z");
        int ir = el.find("red"), ig = el.find("green"), ib = el.find("blue");
        if (ix < 0 || iy < 0 || iz < 0)
            throw std::runtime_error("PLY: vertex element missing x/y/z in " + path);
        if (ir < 0 || ig < 0 || ib < 0)
            throw std::runtime_error("PLY: vertex element missing red/green/blue in " + path);

        const size_t nprops = el.props.size();
        std::vector<size_t> offsets(nprops);
        size_t off = 0;
        for (size_t i = 0; i < nprops; i++) {
            offsets[i] = off;
            off += ply_type_size(el.props[i].type);
        }
        const size_t stride = off;

        // Colors stored as float/double are 0..1; integer types pass through.
        auto to_u8 = [&](double v, PlyType t) -> uint8_t {
            if (t == PlyType::F32 || t == PlyType::F64) v *= 255.0;
            return (uint8_t)std::min(std::max(v, 0.0), 255.0);
        };

        pts.xyz.resize(el.count * 3);
        pts.rgb.resize(el.count * 3);
        if (binary) {
            if ((int64_t)(end - p) < el.count * (int64_t)stride)
                throw std::runtime_error("PLY: truncated " + path);
            for (int64_t i = 0; i < el.count; i++) {
                const uint8_t* row = (const uint8_t*)p + (size_t)i * stride;
                pts.xyz[i*3 + 0] = (float)ply_read_scalar(row + offsets[ix], el.props[ix].type);
                pts.xyz[i*3 + 1] = (float)ply_read_scalar(row + offsets[iy], el.props[iy].type);
                pts.xyz[i*3 + 2] = (float)ply_read_scalar(row + offsets[iz], el.props[iz].type);
                pts.rgb[i*3 + 0] = to_u8(ply_read_scalar(row + offsets[ir], el.props[ir].type), el.props[ir].type);
                pts.rgb[i*3 + 1] = to_u8(ply_read_scalar(row + offsets[ig], el.props[ig].type), el.props[ig].type);
                pts.rgb[i*3 + 2] = to_u8(ply_read_scalar(row + offsets[ib], el.props[ib].type), el.props[ib].type);
            }
        } else {
            // strtod-walk: no per-row line/token allocations. strtod skips
            // leading whitespace (including newlines), so rows self-delimit.
            std::vector<double> vals(nprops);
            for (int64_t i = 0; i < el.count; i++) {
                for (size_t k = 0; k < nprops; k++) {
                    char* q;
                    vals[k] = fast_strtod(p, &q);
                    if (q == p)
                        throw std::runtime_error("PLY: short ascii row in " + path);
                    p = q;
                }
                pts.xyz[i*3 + 0] = (float)vals[ix];
                pts.xyz[i*3 + 1] = (float)vals[iy];
                pts.xyz[i*3 + 2] = (float)vals[iz];
                pts.rgb[i*3 + 0] = to_u8(vals[ir], el.props[ir].type);
                pts.rgb[i*3 + 1] = to_u8(vals[ig], el.props[ig].type);
                pts.rgb[i*3 + 2] = to_u8(vals[ib], el.props[ib].type);
            }
        }
        if (pts.num() == 0)
            throw std::runtime_error("PLY: no points in " + path);
        return pts;
    }
    throw std::runtime_error("PLY: no vertex element in " + path);
}


// ===========================================================================
// parse_nerfstudio_dataset
// ===========================================================================

namespace {

// transforms.json distortion keys. k4..k6 mean different things either side of
// the fisheye divide: on a perspective camera they are OpenCV's RATIONAL
// denominator, on a fisheye they are further theta-space radial terms.
struct RawDistortion {
    double k1, k2, k3, k4, k5, k6, p1, p2, sx1, sy1, b1, b2;
};

// frame-then-meta numeric lookup; throws when required and absent in both.
double frame_or_meta(const JsonValue& frame, const JsonValue& meta,
                     const char* key, bool required, double def = 0.0) {
    if (const JsonValue* v = frame.find(key)) return v->as_double(def);
    if (const JsonValue* v = meta.find(key))  return v->as_double(def);
    if (required)
        throw std::runtime_error(std::string("NerfstudioParser: missing '") +
                                 key + "' in frame and file header");
    return def;
}

// A sensor skew (Metashape b2) is an off-diagonal pixel term, and every tier's
// pixel map is diagonal. So the camera is fitted without it and the images are
// resampled from the true skewed projection -- unless the skew turns out to be
// smaller than resampling can express, in which case it is simply dropped.
struct SkewFit {
    CameraModelType      model;
    CameraDistortionType tier;
    double fx, fy, cx, cy;
    float  coeffs[kCameraDistortionParams];
    RedistortSource source;   // source_model < 0 when the skew was negligible
};

// Keyed on everything the fit depends on: a transforms.json repeats the same
// intrinsics on every frame and each fit is a least-squares solve.
using SkewCache = std::map<std::string, SkewFit>;

const SkewFit& fit_skewed_sensor(SkewCache& cache, CameraModelType model,
                                 CameraDistortionType tier,
                                 double fx, double fy, double cx, double cy,
                                 double skew_px, const float* coeffs,
                                 double W, double H, const std::string& label) {
    RedistortSource src;
    src.source_model = srccam::kSkewed;
    src.params[0] = (float)fx; src.params[1] = (float)fy;
    src.params[2] = (float)cx; src.params[3] = (float)cy;
    src.params[4] = (float)skew_px;
    for (int k = 0; k < kCameraDistortionParams; k++)
        src.params[5 + k] = coeffs[k];
    src.params[13] = model == CameraModelType::FISHEYE   ? srccam::kSkewBaseFisheye
                   : model == CameraModelType::EQUISOLID ? srccam::kSkewBaseEquisolid
                                                         : srccam::kSkewBasePerspective;
    src.params[14] = tier == CameraDistortionType::Rational
                   ? srccam::kSkewRadialRational : srccam::kSkewRadialPolynomial;

    std::string key((const char*)src.params, sizeof(src.params));
    key += std::string((const char*)&W, sizeof(W));
    key += std::string((const char*)&H, sizeof(H));
    auto it = cache.find(key);
    if (it != cache.end())
        return it->second;

    dsfit::SourceProject project =
        [&src](double x, double y, double z, double* u, double* v) {
            return srccam::project(src.source_model, src.params, x, y, z, u, v);
        };
    // The same camera model reproduces everything but the skew, so try it
    // before letting the fitter pick one it likes better.
    dsfit::FitResult fit = dsfit::fit_camera(project, (int)W, (int)H, model);
    if (!fit.invertible || fit.samples == 0)
        fit = dsfit::fit_camera_auto(project, (int)W, (int)H);

    SkewFit f{};
    f.model = fit.target.model;
    f.tier  = fit.target.distortion;
    f.fx = fit.target.fx; f.fy = fit.target.fy;
    f.cx = fit.target.cx; f.cy = fit.target.cy;
    for (int k = 0; k < kCameraDistortionParams; k++) f.coeffs[k] = fit.target.coeffs[k];
    if (fit.max_px >= dsfit::kExactFitPx) {
        f.source = src;
        f.source.fit_max_px = (float)fit.max_px;
        std::printf("%s\n", spirula::i18n::format(dmsg::camera_sensor_skew,
            {label, skew_px, camera_model_to_string(f.model),
             camera_distortion_to_string(f.tier), fit.max_px}).c_str());
    }
    return cache.emplace(std::move(key), f).first->second;
}

// 3x3 inverse (adjugate); used for applied_transform^-1.
void invert3x3d(const double m[3][3], double out[3][3]) {
    double a = m[0][0], b = m[0][1], c = m[0][2],
           d = m[1][0], e = m[1][1], g = m[1][2],
           h = m[2][0], i = m[2][1], j = m[2][2];
    double det = a*(e*j - g*i) - b*(d*j - g*h) + c*(d*i - e*h);
    if (std::abs(det) < 1e-20)
        throw std::runtime_error("NerfstudioParser: singular applied_transform");
    double inv = 1.0 / det;
    out[0][0] = (e*j - g*i)*inv; out[0][1] = (c*i - b*j)*inv; out[0][2] = (b*g - c*e)*inv;
    out[1][0] = (g*h - d*j)*inv; out[1][1] = (a*j - c*h)*inv; out[1][2] = (c*d - a*g)*inv;
    out[2][0] = (d*i - e*h)*inv; out[2][1] = (b*h - a*i)*inv; out[2][2] = (a*e - b*d)*inv;
}

// Path of `file_path` relative to the configured image dir (for aux-buffer
// probing); falls back to the full relative path when not under image_dir.
std::string rel_to_image_dir(const std::string& file_path, const std::string& image_dir) {
    std::string prefix = image_dir;
    if (!prefix.empty() && prefix.back() != '/') prefix += '/';
    if (file_path.rfind(prefix, 0) == 0) return file_path.substr(prefix.size());
    return file_path;
}

}  // namespace


ParsedDataset parse_nerfstudio_dataset(const std::string& dataset_dir,
                                       const DatasetParserConfig& cfg) {
    fs::path transforms_path = fs::path(dataset_dir) / "transforms.json";
    if (!fs::exists(transforms_path))
        throw std::runtime_error("NerfstudioParser: " + transforms_path.string() +
                                 " does not exist");
    JsonValue meta = json_parse_file(transforms_path.string());
    return parse_nerfstudio_meta(meta, dataset_dir, cfg);
}

// Shared back-end: consumes an already-built transforms.json-shaped meta.
// The Metashape parser feeds this directly, mirroring Python's
// _parser_metashape_data -> _parse_nerfstudio_data(transforms[0]).
ParsedDataset parse_nerfstudio_meta(const JsonValue& meta,
                                    const std::string& dataset_dir,
                                    const DatasetParserConfig& cfg) {
    fs::path root(dataset_dir);
    const JsonValue* jframes = meta.find("frames");
    if (!jframes || !jframes->is_array() || jframes->arr.empty())
        throw std::runtime_error("NerfstudioParser: no frames in transforms.json");

    // ---- Collect frames; skip .webp / missing files ---------------------
    struct Frame { const JsonValue* j; std::string file_path; std::string abs; };
    std::vector<Frame> frames;
    for (const JsonValue& fr : jframes->arr) {
        const JsonValue* fp = fr.find("file_path");
        if (!fp) continue;
        std::string file_path = fp->as_string();
        if (fs::path(file_path).extension() == ".webp") {
            std::fprintf(stderr, "%s %s\n", dmsg::word_warning.get(),
                         spirula::i18n::format(dmsg::image_format_unsupported,
                                               {file_path}).c_str());
            continue;
        }
        fs::path abs = root / file_path;
        if (cfg.require_image_files && !fs::exists(abs)) {
            std::fprintf(stderr, "%s %s\n", dmsg::word_warning.get(),
                         spirula::i18n::format(dmsg::image_missing,
                                               {file_path}).c_str());
            continue;
        }
        frames.push_back({&fr, file_path, abs.string()});
    }
    if (frames.empty())
        throw std::runtime_error("NerfstudioParser: no usable frames");
    std::sort(frames.begin(), frames.end(),
              [](const Frame& a, const Frame& b) { return a.abs < b.abs; });

    // ---- All-frame c2w -------------------------------------------------------
    auto read_c2w = [](const JsonValue& fr, float* out12) {
        const JsonValue* tm = fr.find("transform_matrix");
        if (!tm || !tm->is_array() || tm->arr.size() < 3)
            throw std::runtime_error("NerfstudioParser: bad transform_matrix");
        for (int r = 0; r < 3; r++) {
            const JsonValue& row = tm->arr[r];
            if (!row.is_array() || row.arr.size() < 4)
                throw std::runtime_error("NerfstudioParser: bad transform_matrix row");
            for (int c = 0; c < 4; c++) out12[r*4 + c] = (float)row.arr[c].as_double();
        }
    };
    int64_t n_all = (int64_t)frames.size();
    std::vector<float> c2w_all(n_all * 12);
    std::vector<double> positions(n_all * 3);
    for (int64_t i = 0; i < n_all; i++) {
        read_c2w(*frames[i].j, &c2w_all[i*12]);
        for (int r = 0; r < 3; r++) positions[i*3 + r] = c2w_all[i*12 + r*4 + 3];
    }

    // ---- Outlier rejection ----------------------------------------------
    {
        std::vector<char> keep = dsparse::outlier_keep_mask(
            positions, n_all, cfg.outlier_threshold);
        std::vector<Frame> kept;
        std::vector<float> kept_c2w;
        for (int64_t i = 0; i < n_all; i++) {
            if (!keep[i]) continue;
            kept.push_back(frames[i]);
            kept_c2w.insert(kept_c2w.end(), &c2w_all[i*12], &c2w_all[i*12] + 12);
        }
        frames = std::move(kept);
        c2w_all = std::move(kept_c2w);
        n_all = (int64_t)frames.size();
    }

    // ---- train_frame_scale + normalized-frame similarity (all post-outlier
    // frames, pre-split). ------------------------------------------------------
    double T_n_from_camera[16];
    double scale_factor = dsparse::compute_normalized_transform(
        c2w_all, n_all, T_n_from_camera);

    // ---- eval_mode train subset ----------------------------------------------
    std::vector<std::string> names(n_all);
    for (int64_t i = 0; i < n_all; i++) names[i] = frames[i].file_path;
    std::vector<int64_t> subset = dsparse::train_subset(n_all, names, cfg);

    ParsedDataset ds;
    const int64_t N = (int64_t)subset.size();
    ds.num_cameras = N;
    ds.train_frame_scale = (float)(scale_factor != 0.0 ? 1.0 / scale_factor : 1.0);
    ds.c2w.resize(N * 12);
    ds.intrins.resize(N * 4);
    ds.dist_coeffs.resize(N * kCameraDistortionParams);
    ds.camera_models.reserve(N);
    ds.camera_distortions.reserve(N);
    ds.image_filenames.reserve(N);
    ds.widths.reserve(N);
    ds.heights.reserve(N);

    std::vector<std::string> mask_files(N), depth_files(N), normal_files(N);
    bool any_mask = false, any_depth = false, any_normal = false;
    const int EQUIRECT_V = (int)camera_model_from_name("EQUIRECTANGULAR");
    const int PINHOLE_V  = (int)camera_model_from_name("PINHOLE");
    SkewCache skew_cache;

    for (int64_t j = 0; j < N; j++) {
        const Frame& F = frames[subset[j]];
        const JsonValue& fr = *F.j;

        double fx = frame_or_meta(fr, meta, "fl_x", true);
        double fy = frame_or_meta(fr, meta, "fl_y", true);
        double cx = frame_or_meta(fr, meta, "cx", true);
        double cy = frame_or_meta(fr, meta, "cy", true);
        double W  = frame_or_meta(fr, meta, "w", true);
        double H  = frame_or_meta(fr, meta, "h", true);

        std::string model_name = "OPENCV";
        if (const JsonValue* v = fr.find("camera_model"))        model_name = v->as_string();
        else if (const JsonValue* v2 = meta.find("camera_model")) model_name = v2->as_string();
        // camera_model_from_name accepts exactly the COLMAP camera models.
        CameraModelType model = camera_model_from_name(model_name);
        if ((int)model < 0)
            throw std::runtime_error("NerfstudioParser: unsupported camera model " +
                                     model_name);

        RawDistortion rd{};
        auto get = [&](const char* key) {
            return frame_or_meta(fr, meta, key, false, 0.0);
        };
        rd.k1 = get("k1"); rd.k2 = get("k2"); rd.k3 = get("k3");
        rd.k4 = get("k4"); rd.k5 = get("k5"); rd.k6 = get("k6");
        rd.p1 = get("p1"); rd.p2 = get("p2");
        rd.sx1 = get("sx1"); rd.sy1 = get("sy1");
        rd.b1 = get("b1"); rd.b2 = get("b2");

        // b1/b2 arrive already converted, NOT as Metashape writes them:
        // metashape_utils.py (and MetashapeParser, which mirrors it) divides
        // both by fl_x, and swaps p1/p2 into OpenCV order on the way. So here
        // they are affinity and skew relative to fl_x. The affinity is an fx
        // correction and folds in exactly; the skew is off-diagonal, so it is
        // handled after the tier is known, below.
        const double skew_px = rd.b2 * fx;
        if (rd.b1 != 0.0) fx *= 1.0 + rd.b1;

        // An explicit "camera_distortion" resolves what k4 means; without one,
        // a perspective camera follows OpenCV (k4..k6 are the rational
        // denominator) and a fisheye follows Kannala-Brandt (k4 is radial).
        // MetashapeParser always writes the key, because its 4th radial term
        // would otherwise be read as a denominator.
        CameraDistortionType hint = (CameraDistortionType)-1;
        if (const JsonValue* v = fr.find("camera_distortion"))
            hint = camera_distortion_from_name(v->as_string());
        else if (const JsonValue* v2 = meta.find("camera_distortion"))
            hint = camera_distortion_from_name(v2->as_string());

        // b1/b2 are Metashape's affinity and skew and sx1/sy1 are thin-prism
        // terms; a rational camera has none of them, so their mere PRESENCE
        // resolves what k4 means for a file that omits "camera_distortion" --
        // which is what a transforms.json converted from Metashape looks like.
        // k5/k6 exist only in the rational model and decide on their own.
        auto has = [&](const char* key) {
            return fr.find(key) != nullptr || meta.find(key) != nullptr;
        };
        bool thin_prism_shaped =
            has("b1") || has("b2") || has("sx1") || has("sy1");

        float* dst = &ds.dist_coeffs[j*kCameraDistortionParams];
        CameraDistortionType tier;
        bool fisheye = ((int)model != PINHOLE_V);
        bool rational = (hint == CameraDistortionType::Rational) ||
                        ((int)hint < 0 && !fisheye &&
                         (rd.k5 != 0.0 || rd.k6 != 0.0 ||
                          (rd.k4 != 0.0 && !thin_prism_shaped)));
        if (rational) {
            tier = CameraDistortionType::Rational;
            dst[0] = (float)rd.k1; dst[1] = (float)rd.k2; dst[2] = (float)rd.k3;
            dst[3] = (float)rd.k4; dst[4] = (float)rd.k5; dst[5] = (float)rd.k6;
            dst[6] = (float)rd.p1; dst[7] = (float)rd.p2;
        } else {
            tier = CameraDistortionType::ThinPrism;
            dst[0] = (float)rd.k1;  dst[1] = (float)rd.k2;
            dst[2] = (float)rd.k3;  dst[3] = (float)rd.k4;
            dst[4] = (float)rd.p1;  dst[5] = (float)rd.p2;
            dst[6] = (float)rd.sx1; dst[7] = (float)rd.sy1;
        }
        if ((int)model == EQUIRECT_V) {
            // A panorama has no lens, so it has no skew either; b2 on one is
            // meaningless and ignored along with the rest of the coefficients.
            tier = CameraDistortionType::None;
            for (int k = 0; k < kCameraDistortionParams; k++) dst[k] = 0.0f;
        } else if (skew_px != 0.0) {
            const SkewFit& f = fit_skewed_sensor(skew_cache, model, tier, fx, fy,
                                                 cx, cy, skew_px, dst, W, H, F.abs);
            model = f.model;
            tier  = f.tier;
            fx = f.fx; fy = f.fy; cx = f.cx; cy = f.cy;
            std::copy(f.coeffs, f.coeffs + kCameraDistortionParams, dst);
            if (f.source.source_model >= 0) {
                if (ds.redistort.empty()) ds.redistort.resize(N);
                ds.redistort[j] = f.source;
            }
        }
        ds.camera_distortions.push_back(
            (int32_t)camera_distortion_demote(tier, dst, dst));

        dsparse::fit_camera_resolution(
            cfg, F.abs, W, H, fx, fy, cx, cy,
            ds.redistort.empty() ? nullptr : &ds.redistort[j]);

        // Equirectangular: canonical panorama intrinsics.
        if ((int)model == EQUIRECT_V) {
            fx = fy = W / (2.0 * kPi);
            cx = W / 2.0;
            cy = H / 2.0;
        }

        ds.camera_models.push_back((int32_t)model);
        ds.image_filenames.push_back(F.abs);
        ds.widths.push_back((int32_t)W);
        ds.heights.push_back((int32_t)H);
        ds.intrins[j*4 + 0] = (float)fx;
        ds.intrins[j*4 + 1] = (float)fy;
        ds.intrins[j*4 + 2] = (float)cx;
        ds.intrins[j*4 + 3] = (float)cy;
        std::copy(&c2w_all[subset[j]*12], &c2w_all[subset[j]*12] + 12, &ds.c2w[j*12]);

        // Auxiliary buffers: explicit frame paths win; directory-convention
        // probing as fallback (_add_auxiliary_buffers). Unlike the Python
        // parser (which requires all-or-none), per-image absence is allowed
        // -- the C++ DataManager's convention ("" = none for this image).
        std::string rel = rel_to_image_dir(F.file_path, cfg.image_dir);
        if (const JsonValue* v = fr.find("mask_path"))
            mask_files[j] = (root / v->as_string()).string();
        else
            mask_files[j] = dsparse::find_aux_file(
                (root / cfg.mask_dir).string(), rel, "mask");
        if (const JsonValue* v = fr.find("depth_file_path"))
            depth_files[j] = (root / v->as_string()).string();
        else
            depth_files[j] = dsparse::find_aux_file(
                (root / cfg.depth_dir).string(), rel, "depth");
        if (const JsonValue* v = fr.find("normal_file_path"))
            normal_files[j] = (root / v->as_string()).string();
        else
            normal_files[j] = dsparse::find_aux_file(
                (root / cfg.normal_dir).string(), rel, "normal");
        any_mask   |= !mask_files[j].empty();
        any_depth  |= !depth_files[j].empty();
        any_normal |= !normal_files[j].empty();
    }
    if (any_mask)   ds.mask_filenames   = std::move(mask_files);
    if (any_depth)  ds.depth_filenames  = std::move(depth_files);
    if (any_normal) ds.normal_filenames = std::move(normal_files);

    // ---- Seed points ------------------------------------------------------
    std::string ply_rel;
    if (const JsonValue* v = meta.find("ply_file_path")) ply_rel = v->as_string();
    else {
        for (const char* cand : {"sparse_pc.ply", "pointcloud.ply"})
            if (fs::exists(root / cand)) { ply_rel = cand; break; }
    }
    if (ply_rel.empty()) {
        // Lenient (viewer) mode: a transforms.json with no point cloud still
        // yields camera poses / frustums. The trainer requires the seed cloud.
        if (cfg.require_image_files)
            throw std::runtime_error(
                "NerfstudioParser: no initial point cloud found (ply_file_path / "
                "sparse_pc.ply / pointcloud.ply)");
    } else if (cfg.require_image_files || fs::exists(root / ply_rel)) {
        ds.points = read_ply_points((root / ply_rel).string());
    }

    // ---- applied_transform inverse (train_frame="points" branch): poses and
    // points go back to the ORIGINAL (pre-applied_transform) frame. Also folds
    // into the viewer remap:
    //   train_to_normalized = inv(T_n_from_camera @ applied)
    double T_n_from_train[16];
    std::copy(T_n_from_camera, T_n_from_camera + 16, T_n_from_train);
    if (const JsonValue* at = meta.find("applied_transform")) {
        double A[3][3], b[3];
        for (int r = 0; r < 3; r++) {
            const JsonValue& row = at->arr.at(r);
            for (int c = 0; c < 3; c++) A[r][c] = row.arr.at(c).as_double();
            b[r] = row.arr.at(3).as_double();
        }
        bool identity = true;
        for (int r = 0; r < 3 && identity; r++)
            for (int c = 0; c < 3; c++)
                if (A[r][c] != (r == c ? 1.0 : 0.0) || b[r] != 0.0) { identity = false; break; }
        if (!identity) {
            // T_n_from_train = T_n_from_camera @ applied (both affine, 0001 rows)
            double ap[16] = {A[0][0],A[0][1],A[0][2],b[0],
                             A[1][0],A[1][1],A[1][2],b[1],
                             A[2][0],A[2][1],A[2][2],b[2],
                             0,0,0,1};
            for (int r = 0; r < 4; r++)
                for (int c = 0; c < 4; c++) {
                    double v = 0.0;
                    for (int m = 0; m < 4; m++)
                        v += T_n_from_camera[r*4 + m] * ap[m*4 + c];
                    T_n_from_train[r*4 + c] = v;
                }
            double Ai[3][3];
            invert3x3d(A, Ai);
            double bi[3];
            for (int r = 0; r < 3; r++)
                bi[r] = -(Ai[r][0]*b[0] + Ai[r][1]*b[1] + Ai[r][2]*b[2]);
            // c2w' = inv(T) @ c2w  (c2w has implicit bottom row 0 0 0 1)
            for (int64_t j = 0; j < N; j++) {
                float* m = &ds.c2w[j*12];
                double out[3][4];
                for (int r = 0; r < 3; r++)
                    for (int c = 0; c < 4; c++)
                        out[r][c] = Ai[r][0]*m[0*4+c] + Ai[r][1]*m[1*4+c]
                                  + Ai[r][2]*m[2*4+c] + (c == 3 ? bi[r] : 0.0);
                for (int r = 0; r < 3; r++)
                    for (int c = 0; c < 4; c++) m[r*4+c] = (float)out[r][c];
            }
            for (int64_t i = 0; i < ds.points.num(); i++) {
                float* p = &ds.points.xyz[i*3];
                double x = Ai[0][0]*p[0] + Ai[0][1]*p[1] + Ai[0][2]*p[2] + bi[0];
                double y = Ai[1][0]*p[0] + Ai[1][1]*p[1] + Ai[1][2]*p[2] + bi[1];
                double z = Ai[2][0]*p[0] + Ai[2][1]*p[1] + Ai[2][2]*p[2] + bi[2];
                p[0] = (float)x; p[1] = (float)y; p[2] = (float)z;
            }
        }
    }

    double T_remap[16];
    dsparse::invert_affine4x4(T_n_from_train, T_remap);
    for (int k = 0; k < 16; k++) ds.train_to_normalized[k] = (float)T_remap[k];

    // validation_fraction holds out part of the TRAIN set; the eval split is
    // already a held-out set, so it is all "train" from the DataManager's
    // point of view.
    dsparse::assign_val_split(
        ds, cfg.split == "eval" ? 0.0f : cfg.validation_fraction);
    return ds;
}

// ColmapParser.cpp -- COLMAP-format reader for DatasetParser.h: the .bin/.txt
// readers, frame assembly and pose conventions. Shared bake helpers live in
// DatasetCommon.cpp.

#include "data/DatasetParser.h"
#include "i18n/catalog/Data.h"
#include "data/FastFloat.h"

#include "core/CameraModel.h"   // camera_model_from_name (CUDA-free)
#include "data/DistortionFit.h"
#include "data/SourceCamera.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;

constexpr double kPi = 3.14159265358979323846;   // MSVC has no M_PI by default


// ===========================================================================
// Little-endian binary readers (colmap_utils.py read_next_bytes)
// ===========================================================================

namespace {

struct BinReader {
    FILE* f = nullptr;
    std::string path;

    explicit BinReader(const std::string& p) : path(p) {
        f = std::fopen(p.c_str(), "rb");
        if (!f) throw std::runtime_error("ColmapParser: cannot open " + p);
    }
    ~BinReader() { if (f) std::fclose(f); }

    template <typename T>
    T read() {
        // COLMAP files are little-endian; so is every platform we target.
        T v;
        if (std::fread(&v, sizeof(T), 1, f) != 1)
            throw std::runtime_error("ColmapParser: truncated file " + path);
        return v;
    }
    void skip(size_t bytes) {
        if (std::fseek(f, (long)bytes, SEEK_CUR) != 0)
            throw std::runtime_error("ColmapParser: truncated file " + path);
    }
    std::string read_cstring() {
        std::string s;
        for (;;) {
            int c = std::fgetc(f);
            if (c == EOF) throw std::runtime_error("ColmapParser: truncated file " + path);
            if (c == '\0') break;
            s.push_back((char)c);
        }
        return s;
    }
};

// COLMAP model id -> (name, num_params). Matches colmap/src/colmap/sensor/models.h
// (CameraModelId + each model's params_info).
struct ColmapModelInfo { const char* name; int num_params; };
const std::map<int, ColmapModelInfo>& colmap_model_table() {
    static const std::map<int, ColmapModelInfo> t = {
        {0,  {"SIMPLE_PINHOLE", 3}},
        {1,  {"PINHOLE", 4}},
        {2,  {"SIMPLE_RADIAL", 4}},
        {3,  {"RADIAL", 5}},
        {4,  {"OPENCV", 8}},
        {5,  {"OPENCV_FISHEYE", 8}},
        {6,  {"FULL_OPENCV", 12}},
        {7,  {"FOV", 5}},
        {8,  {"SIMPLE_RADIAL_FISHEYE", 4}},
        {9,  {"RADIAL_FISHEYE", 5}},
        {10, {"THIN_PRISM_FISHEYE", 12}},
        {11, {"RAD_TAN_THIN_PRISM_FISHEYE", 16}},
        {12, {"SIMPLE_DIVISION", 4}},
        {13, {"DIVISION", 5}},
        {14, {"SIMPLE_FISHEYE", 3}},
        {15, {"FISHEYE", 4}},
        {16, {"EUCM", 6}},
        {17, {"EQUIRECTANGULAR", 2}},
    };
    return t;
}

int colmap_model_id(const std::string& name) {
    for (const auto& [id, info] : colmap_model_table())
        if (name == info.name) return id;
    return -1;
}

}  // namespace


std::map<int32_t, ColmapCamera> read_cameras_binary(const std::string& recon_dir) {
    BinReader r(recon_dir + "/cameras.bin");
    std::map<int32_t, ColmapCamera> cameras;
    uint64_t n = r.read<uint64_t>();
    for (uint64_t i = 0; i < n; i++) {
        ColmapCamera cam;
        cam.camera_id = r.read<int32_t>();
        int model_id  = r.read<int32_t>();
        cam.width     = r.read<uint64_t>();
        cam.height    = r.read<uint64_t>();
        auto it = colmap_model_table().find(model_id);
        if (it == colmap_model_table().end())
            throw std::runtime_error("ColmapParser: unknown camera model id " +
                                     std::to_string(model_id));
        cam.model = it->second.name;
        cam.params.resize(it->second.num_params);
        for (auto& p : cam.params) p = r.read<double>();
        cameras[cam.camera_id] = std::move(cam);
    }
    return cameras;
}

std::map<int32_t, ColmapImage> read_images_binary(const std::string& recon_dir) {
    BinReader r(recon_dir + "/images.bin");
    std::map<int32_t, ColmapImage> images;
    uint64_t n = r.read<uint64_t>();
    for (uint64_t i = 0; i < n; i++) {
        ColmapImage im;
        im.image_id = r.read<int32_t>();
        for (auto& q : im.qvec) q = r.read<double>();
        for (auto& t : im.tvec) t = r.read<double>();
        im.camera_id = r.read<int32_t>();
        im.name = r.read_cstring();
        // Skip the 2D feature track (x, y, point3D_id) -- not needed.
        uint64_t num_points2D = r.read<uint64_t>();
        r.skip(num_points2D * (2 * sizeof(double) + sizeof(uint64_t)));
        images[im.image_id] = std::move(im);
    }
    return images;
}

ColmapPoints3D read_points3D_binary(const std::string& recon_dir) {
    BinReader r(recon_dir + "/points3D.bin");
    ColmapPoints3D pts;
    uint64_t n = r.read<uint64_t>();
    pts.xyz.reserve(n * 3);
    pts.rgb.reserve(n * 3);
    for (uint64_t i = 0; i < n; i++) {
        r.skip(sizeof(uint64_t));                       // point3D_id
        for (int k = 0; k < 3; k++) pts.xyz.push_back((float)r.read<double>());
        for (int k = 0; k < 3; k++) pts.rgb.push_back(r.read<uint8_t>());
        r.skip(sizeof(double));                         // reprojection error
        uint64_t track_len = r.read<uint64_t>();
        r.skip(track_len * 2 * sizeof(int32_t));        // (image_id, point2D_idx)
    }
    return pts;
}


// ===========================================================================
// Text-format readers (cameras.txt / images.txt / points3D.txt), port of
// colmap_utils.py read_*_text. '#' lines are comments; fields are
// whitespace-separated (image names therefore contain no spaces, matching
// COLMAP's own text reader); each image record is followed by one raw
// POINTS2D line (possibly empty) which is skipped.
// ===========================================================================

namespace {

struct TextReader {
    std::string data;
    size_t pos = 0;

    explicit TextReader(const std::string& p) {
        FILE* f = std::fopen(p.c_str(), "rb");
        if (!f) throw std::runtime_error("ColmapParser: cannot open " + p);
        std::fseek(f, 0, SEEK_END);
        long n = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        data.resize(n > 0 ? (size_t)n : 0);
        size_t got = data.empty() ? 0 : std::fread(&data[0], 1, data.size(), f);
        std::fclose(f);
        if (got != data.size())
            throw std::runtime_error("ColmapParser: cannot read " + p);
    }
    // next non-comment, non-empty line; false at EOF
    bool next_line(const char** s, const char** e) {
        while (pos < data.size()) {
            size_t eol = data.find('\n', pos);
            if (eol == std::string::npos) eol = data.size();
            size_t b = pos, t = eol;
            pos = eol + 1;
            while (b < t && (data[b]==' '||data[b]=='\t'||data[b]=='\r')) b++;
            while (t > b && (data[t-1]=='\r'||data[t-1]==' '||data[t-1]=='\t')) t--;
            if (b >= t || data[b] == '#') continue;
            *s = data.c_str() + b;
            *e = data.c_str() + t;
            return true;
        }
        return false;
    }
    // consume exactly one raw line (an empty POINTS2D line still counts)
    void skip_raw_line() {
        size_t eol = data.find('\n', pos);
        pos = (eol == std::string::npos) ? data.size() : eol + 1;
    }
};

}  // namespace

std::map<int32_t, ColmapCamera> read_cameras_text(const std::string& recon_dir) {
    TextReader r(recon_dir + "/cameras.txt");
    std::map<int32_t, ColmapCamera> cameras;
    const char *s, *e;
    while (r.next_line(&s, &e)) {
        ColmapCamera cam;
        char* p;
        cam.camera_id = (int32_t)std::strtol(s, &p, 10);
        while (p < e && std::isspace((unsigned char)*p)) p++;
        const char* m0 = p;
        while (p < e && !std::isspace((unsigned char)*p)) p++;
        cam.model.assign(m0, (size_t)(p - m0));
        cam.width  = std::strtoull(p, &p, 10);
        cam.height = std::strtoull(p, &p, 10);
        for (;;) {
            char* q;
            double v = fast_strtod(p, &q);
            if (q == p || q > e) break;      // no more params on this line
            cam.params.push_back(v);
            p = q;
        }
        if (cam.model.empty())
            throw std::runtime_error("ColmapParser: malformed cameras.txt line");
        cameras[cam.camera_id] = std::move(cam);
    }
    return cameras;
}

std::map<int32_t, ColmapImage> read_images_text(const std::string& recon_dir) {
    TextReader r(recon_dir + "/images.txt");
    std::map<int32_t, ColmapImage> images;
    const char *s, *e;
    while (r.next_line(&s, &e)) {
        ColmapImage im;
        char* p;
        im.image_id = (int32_t)fast_strtol(s, &p);
        for (auto& q : im.qvec) q = fast_strtod(p, &p);
        for (auto& t : im.tvec) t = fast_strtod(p, &p);
        im.camera_id = (int32_t)fast_strtol(p, &p);
        while (p < e && std::isspace((unsigned char)*p)) p++;
        const char* n0 = p;
        while (p < e && !std::isspace((unsigned char)*p)) p++;
        im.name.assign(n0, (size_t)(p - n0));
        if (im.name.empty())
            throw std::runtime_error("ColmapParser: malformed images.txt line");
        r.skip_raw_line();                   // POINTS2D[] line (may be empty)
        images[im.image_id] = std::move(im);
    }
    return images;
}

ColmapPoints3D read_points3D_text(const std::string& recon_dir) {
    TextReader r(recon_dir + "/points3D.txt");
    ColmapPoints3D pts;
    const char *s, *e;
    while (r.next_line(&s, &e)) {
        char* p;
        fast_strtol(s, &p);                  // point3D_id
        for (int k = 0; k < 3; k++) pts.xyz.push_back((float)fast_strtod(p, &p));
        for (int k = 0; k < 3; k++) pts.rgb.push_back((uint8_t)fast_strtol(p, &p));
        // reprojection error + track: rest of line, skipped
    }
    return pts;
}


// ===========================================================================
// COLMAP camera model -> (CameraModelType, CameraDistortionType, coefficients)
//
// Every one of COLMAP's 18 models is accepted. Fourteen map exactly onto a
// tier; the remaining four (FOV, SIMPLE_DIVISION, DIVISION, EUCM) and
// RAD_TAN_THIN_PRISM_FISHEYE have no exact representation and are fitted, with
// the source model recorded so the images can be re-distorted to match.
//
// COLMAP's parameter ORDER is not ours: it interleaves p1,p2 between k2 and k3
// (models.h FullOpenCVCameraModel / ThinPrismFisheyeCameraModel), and its
// FULL_OPENCV k4..k6 are DENOMINATOR terms, not further radial terms.
// ===========================================================================

namespace {

struct BakedIntrins {
    float fx, fy, cx, cy;
    CameraModelType      model      = CameraModelType::PINHOLE;
    CameraDistortionType distortion = CameraDistortionType::None;
    std::array<float, kCameraDistortionParams> dist{};
    RedistortSource      source;      // source_model < 0 unless fitted
};

BakedIntrins bake_colmap_intrins(const ColmapCamera& cam) {
    const auto& p = cam.params;
    auto P = [&](size_t i) { return (float)p[i]; };
    auto need = [&](size_t n) {
        if (p.size() < n)
            throw std::runtime_error(
                "ColmapParser: camera model " + cam.model + " needs " +
                std::to_string(n) + " parameters, got " +
                std::to_string(p.size()));
    };
    BakedIntrins o;

    // ---- one focal length, principal point, no distortion ----------------
    if (cam.model == "SIMPLE_PINHOLE") {
        need(3); o.fx = o.fy = P(0); o.cx = P(1); o.cy = P(2);
    } else if (cam.model == "PINHOLE") {
        need(4); o.fx = P(0); o.fy = P(1); o.cx = P(2); o.cy = P(3);
    } else if (cam.model == "SIMPLE_FISHEYE") {
        need(3); o.fx = o.fy = P(0); o.cx = P(1); o.cy = P(2);
        o.model = CameraModelType::FISHEYE;
    } else if (cam.model == "FISHEYE") {
        need(4); o.fx = P(0); o.fy = P(1); o.cx = P(2); o.cy = P(3);
        o.model = CameraModelType::FISHEYE;

    // ---- radial-only, one focal length -> OpenCV tier with zero p1,p2 ----
    } else if (cam.model == "SIMPLE_RADIAL" || cam.model == "RADIAL" ||
               cam.model == "SIMPLE_RADIAL_FISHEYE" || cam.model == "RADIAL_FISHEYE") {
        bool two = (cam.model == "RADIAL" || cam.model == "RADIAL_FISHEYE");
        need(two ? 5 : 4);
        o.fx = o.fy = P(0); o.cx = P(1); o.cy = P(2);
        o.distortion = CameraDistortionType::OpenCV;
        o.dist[0] = P(3);                       // k1
        if (two) o.dist[1] = P(4);              // k2
        if (cam.model == "SIMPLE_RADIAL_FISHEYE" || cam.model == "RADIAL_FISHEYE")
            o.model = CameraModelType::FISHEYE;

    // ---- fx, fy, cx, cy + k1 k2 p1 p2 ------------------------------------
    } else if (cam.model == "OPENCV") {
        need(8);
        o.fx = P(0); o.fy = P(1); o.cx = P(2); o.cy = P(3);
        o.distortion = CameraDistortionType::OpenCV;
        o.dist[0] = P(4); o.dist[1] = P(5);     // k1 k2
        o.dist[2] = P(6); o.dist[3] = P(7);     // p1 p2

    // ---- the rational model ----------------------------------------------
    } else if (cam.model == "FULL_OPENCV") {
        need(12);
        o.fx = P(0); o.fy = P(1); o.cx = P(2); o.cy = P(3);
        o.distortion = CameraDistortionType::Rational;
        o.dist[0] = P(4);  o.dist[1] = P(5);    // k1 k2   numerator
        o.dist[2] = P(8);                       // k3      numerator
        o.dist[3] = P(9);  o.dist[4] = P(10); o.dist[5] = P(11);  // k4 k5 k6 denominator
        o.dist[6] = P(6);  o.dist[7] = P(7);    // p1 p2

    // ---- fisheye radial in theta-space -----------------------------------
    } else if (cam.model == "OPENCV_FISHEYE") {
        need(8);
        o.fx = P(0); o.fy = P(1); o.cx = P(2); o.cy = P(3);
        o.model = CameraModelType::FISHEYE;
        o.distortion = CameraDistortionType::ThinPrism;
        o.dist[0] = P(4); o.dist[1] = P(5); o.dist[2] = P(6); o.dist[3] = P(7);

    } else if (cam.model == "THIN_PRISM_FISHEYE") {
        need(12);
        o.fx = P(0); o.fy = P(1); o.cx = P(2); o.cy = P(3);
        o.model = CameraModelType::FISHEYE;
        o.distortion = CameraDistortionType::ThinPrism;
        o.dist[0] = P(4);  o.dist[1] = P(5);    // k1 k2
        o.dist[2] = P(8);  o.dist[3] = P(9);    // k3 k4
        o.dist[4] = P(6);  o.dist[5] = P(7);    // p1 p2
        o.dist[6] = P(10); o.dist[7] = P(11);   // sx1 sy1

    } else if (cam.model == "EQUIRECTANGULAR") {
        need(2);
        o.fx = (float)(p[0] / (2.0 * kPi));
        o.fy = (float)(p[1] / kPi);
        o.cx = (float)(p[0] / 2.0);
        o.cy = (float)(p[1] / 2.0);
        o.model = CameraModelType::EQUIRECTANGULAR;

    // ---- no exact tier: fit and re-distort --------------------------------
    } else {
        int id = colmap_model_id(cam.model);
        if (id < 0)
            throw std::runtime_error("ColmapParser: unknown camera model " + cam.model);
        need((size_t)colmap_model_table().at(id).num_params);
        o.source.source_model = id;
        for (size_t i = 0; i < p.size() && i < 16; i++)
            o.source.params[i] = (float)p[i];
        // Camera model, intrinsics and coefficients all come from the fit
        // below. Which model suits depends on the parameters, not the name:
        // a FOV lens is perspective at omega 0.3 and a full fisheye at 0.87.
        return o;
    }

    // Drop to the cheapest tier the coefficients actually need.
    o.distortion = camera_distortion_demote(o.distortion, o.dist.data(), o.dist.data());
    return o;
}

}  // namespace

bool colmap_preview_intrins(int model_id, int width, int height,
                            const std::vector<double>& params,
                            PreviewIntrins& out) {
    const auto& table = colmap_model_table();
    const auto it = table.find(model_id);
    if (it == table.end() || (int)params.size() != it->second.num_params)
        return false;
    ColmapCamera cam;
    cam.model = it->second.name;
    cam.width = (uint64_t)width;
    cam.height = (uint64_t)height;
    cam.params = params;
    BakedIntrins bi;
    try {
        bi = bake_colmap_intrins(cam);
    } catch (const std::exception&) {
        return false;
    }
    // A model with no exact tier comes back asking to be fitted, which needs
    // the images. The caller draws a plain frustum instead.
    if (bi.source.source_model >= 0) return false;
    out.fx = bi.fx;
    out.fy = bi.fy;
    out.cx = bi.cx;
    out.cy = bi.cy;
    out.model = (int32_t)bi.model;
    out.distortion = (int32_t)bi.distortion;
    static_assert(kCameraDistortionParams == 8, "PreviewIntrins::dist width");
    std::copy(bi.dist.begin(), bi.dist.end(), out.dist.begin());
    return true;
}

namespace {

// One line per distinct outcome rather than per camera record: an image
// collection run through COLMAP without --ImageReader.single_camera has one
// camera record per photo, and the same sentence a thousand times is noise.
struct FitReport {
    std::string          source;      // COLMAP model name
    CameraModelType      model;
    CameraDistortionType distortion;
    enum { Redistorted, Exact, Failed } kind;
    double max_px = 0.0;              // worst in the group
    int    count = 0;

    std::string key() const {
        return source + "/" + camera_model_to_string(model) + "/" +
               camera_distortion_to_string(distortion) + "/" +
               std::to_string((int)kind);
    }
};

// Replace an unrepresentable camera with the nearest supported one. The images
// are resampled to match later (DataManager's re-distort pass); what is chosen
// here is only the target camera.
void fit_colmap_source(const ColmapCamera& cam, BakedIntrins& bi,
                       std::map<std::string, FitReport>& reports) {
    const RedistortSource src = bi.source;
    dsfit::SourceProject project =
        [&src](double x, double y, double z, double* u, double* v) {
            return srccam::project(src.source_model, src.params, x, y, z, u, v);
        };
    dsfit::FitResult fit = dsfit::fit_camera_auto(
        project, (int)cam.width, (int)cam.height);

    bi.model = fit.target.model;
    bi.fx = fit.target.fx; bi.fy = fit.target.fy;
    bi.cx = fit.target.cx; bi.cy = fit.target.cy;
    for (int i = 0; i < kCameraDistortionParams; i++)
        bi.dist[i] = fit.target.coeffs[i];
    bi.distortion = camera_distortion_demote(
        fit.target.distortion, bi.dist.data(), bi.dist.data());
    bi.source.fit_max_px = (float)fit.max_px;

    FitReport r;
    r.source = cam.model;
    r.model = bi.model;
    r.distortion = bi.distortion;
    r.max_px = fit.max_px;
    if (!fit.ok && fit.samples == 0) {
        // Only reachable when the source projected nothing anywhere -- a
        // camera record this reader cannot interpret at all. Loading a wrong
        // camera beats refusing to open a reconstruction that took hours.
        r.kind = FitReport::Failed;
        bi.source.source_model = -1;
    } else if (fit.max_px < dsfit::kExactFitPx) {
        // The fitted camera and the source agree to well under what bilinear
        // resampling can resolve, so re-distorting would only cost VRAM and
        // blur the images.
        r.kind = FitReport::Exact;
        bi.source.source_model = -1;
    } else {
        r.kind = FitReport::Redistorted;
    }

    auto [it, fresh] = reports.emplace(r.key(), r);
    it->second.count++;
    if (!fresh) it->second.max_px = std::max(it->second.max_px, r.max_px);
}

void print_fit_reports(const std::map<std::string, FitReport>& reports) {
    namespace dm = spirula::i18n::msg::data;
    for (const auto& [key, r] : reports) {
        (void)key;
        switch (r.kind) {
            case FitReport::Failed:
                std::printf("%s %s\n", dm::word_warning.get(),
                    spirula::i18n::format(dm::camera_model_fit_failed,
                        {r.source, r.count,
                         camera_model_to_string(r.model)}).c_str());
                break;
            case FitReport::Exact:
                std::printf("%s\n", spirula::i18n::format(
                    dm::camera_model_fitted_exact,
                    {r.source, r.count, camera_model_to_string(r.model),
                     camera_distortion_to_string(r.distortion),
                     dsfit::kExactFitPx}).c_str());
                break;
            case FitReport::Redistorted:
                std::printf("%s\n", spirula::i18n::format(
                    dm::camera_model_fitted,
                    {r.source, r.count, camera_model_to_string(r.model),
                     camera_distortion_to_string(r.distortion),
                     r.max_px}).c_str());
                break;
        }
    }
}

// colmap_utils.py qvec2rotmat (world->camera rotation from (w,x,y,z)).
void qvec2rotmat(const std::array<double, 4>& q, double R[3][3]) {
    double w = q[0], x = q[1], y = q[2], z = q[3];
    R[0][0] = 1 - 2*y*y - 2*z*z; R[0][1] = 2*x*y - 2*w*z;     R[0][2] = 2*x*z + 2*w*y;
    R[1][0] = 2*x*y + 2*w*z;     R[1][1] = 1 - 2*x*x - 2*z*z; R[1][2] = 2*y*z - 2*w*x;
    R[2][0] = 2*x*z - 2*w*y;     R[2][1] = 2*y*z + 2*w*x;     R[2][2] = 1 - 2*x*x - 2*y*y;
}

// COLMAP w2c -> nerfstudio/OpenGL c2w:
//   c2w[:3,:3] = R^T with columns 1, 2 negated (OpenCV -> OpenGL axis flip)
//   c2w[:3,3]  = -R^T @ t
void colmap_to_c2w(const ColmapImage& im, float* out12) {
    double R[3][3];
    qvec2rotmat(im.qvec, R);
    static const double flip[3] = {1.0, -1.0, -1.0};
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++)
            out12[r*4 + c] = (float)(R[c][r] * flip[c]);
        double t = 0.0;
        for (int c = 0; c < 3; c++) t -= R[c][r] * im.tvec[c];
        out12[r*4 + 3] = (float)t;
    }
}

}  // namespace


// ===========================================================================
// parse_colmap_dataset
// ===========================================================================

// Which format each file of a COLMAP model dir is stored in.
enum class ColmapFmt { None, Bin, Text };
struct ColmapModelFmt { ColmapFmt cameras{}, images{}, points3D{}; };

// Registered-image count of a COLMAP model dir, without parsing the whole
// reconstruction: images.bin starts with a uint64 count; images.txt carries
// it in a header comment (else count non-comment lines / 2). -1 = no model.
static int64_t colmap_model_num_images(const fs::path& dir) {
    if (FILE* f = std::fopen((dir / "images.bin").string().c_str(), "rb")) {
        uint64_t n = 0;
        bool ok = std::fread(&n, sizeof n, 1, f) == 1;
        std::fclose(f);
        if (ok) return (int64_t)n;
    }
    std::ifstream t(dir / "images.txt");
    if (t) {
        std::string line;
        int64_t rows = 0;
        while (std::getline(t, line)) {
            if (!line.empty() && line[0] == '#') {
                size_t p = line.find("Number of images:");
                if (p != std::string::npos)
                    return std::atoll(line.c_str() + p + 17);
                continue;
            }
            if (!line.empty()) rows++;
        }
        return rows / 2;   // each image = header line + 2D-points line
    }
    return -1;
}

static std::string join_files(const std::vector<std::string>& v) {
    std::string s;
    for (size_t i = 0; i < v.size(); i++)
        s += (i ? (i + 1 == v.size() ? " and " : ", ") : "") + v[i];
    return s;
}

// The three files are resolved independently rather than as one all-.bin or
// all-.txt model: half-converted and hand-assembled reconstructions mix the
// two, and each file parses on its own. .bin wins when both are present.
static ColmapFmt colmap_file_fmt(const fs::path& dir, const char* base) {
    std::error_code ec;
    if (fs::exists(dir / (std::string(base) + ".bin"), ec)) return ColmapFmt::Bin;
    if (fs::exists(dir / (std::string(base) + ".txt"), ec)) return ColmapFmt::Text;
    return ColmapFmt::None;
}

static ColmapModelFmt colmap_model_fmt(const fs::path& dir) {
    return {colmap_file_fmt(dir, "cameras"), colmap_file_fmt(dir, "images"),
            colmap_file_fmt(dir, "points3D")};
}

// Locate the reconstruction; "" when there is none. `verbose` is off for
// parse_dataset's auto-detect probe; `near_miss` names the closest partial
// model, so the error says what is missing rather than "not a dataset".
static std::string find_colmap_recon(const std::string& dataset_dir,
                                     const DatasetParserConfig& cfg,
                                     ColmapModelFmt* fmt, bool verbose,
                                     std::string* near_miss = nullptr) {
    std::vector<std::string> probe;
    if (!cfg.recon_dir.empty()) {
        probe = {cfg.recon_dir};
    } else {
        // COLMAP writes one model per subdirectory (sparse/0, sparse/1, ...)
        // and the largest is NOT necessarily sparse/0 -- enumerate them and
        // try the one with the most registered images first.
        struct Model { int64_t n; std::string rel; };
        std::vector<Model> models;
        std::error_code ec;
        for (const char* parent : {"sparse", "colmap/sparse"}) {
            fs::path p = fs::path(dataset_dir) / parent;
            if (!fs::is_directory(p, ec)) continue;
            for (fs::directory_iterator it(p, ec), end; !ec && it != end;
                 it.increment(ec)) {
                if (!it->is_directory(ec)) continue;
                int64_t n = colmap_model_num_images(it->path());
                if (n >= 0)
                    models.push_back({n,
                        (fs::path(parent) / it->path().filename()).generic_string()});
            }
        }
        std::sort(models.begin(), models.end(), [](const Model& a, const Model& b) {
            return a.n != b.n ? a.n > b.n : a.rel < b.rel;
        });
        if (verbose && models.size() > 1)
            std::printf("%s\n",
                        spirula::i18n::format(
                            spirula::i18n::msg::data::colmap_models_found,
                            {(long long)models.size(), dataset_dir,
                             models[0].rel, (long long)models[0].n})
                            .c_str());
        for (const auto& m : models) probe.push_back(m.rel);
        for (const char* rel : {"sparse/0", "colmap/sparse/0", "sparse", "colmap", ""})
            probe.push_back(rel);
    }

    // points3D is required only in strict (trainer) mode; the lenient viewer
    // accepts a cameras-only reconstruction (poses / frustums).
    auto has_recon = [&](const ColmapModelFmt& f) {
        return f.cameras != ColmapFmt::None && f.images != ColmapFmt::None &&
               (f.points3D != ColmapFmt::None || !cfg.require_image_files);
    };
    *fmt = ColmapModelFmt{};
    for (const auto& rel : probe) {
        fs::path d = fs::path(dataset_dir) / rel;
        ColmapModelFmt f = colmap_model_fmt(d);
        if (has_recon(f)) { *fmt = f; return d.string(); }
    }

    // Nothing matched: report the first probed dir that holds *some* of the
    // three files (a recon that lost points3D, say) rather than none.
    if (near_miss) {
        for (const auto& rel : probe) {
            ColmapModelFmt f = colmap_model_fmt(fs::path(dataset_dir) / rel);
            const std::pair<const char*, ColmapFmt> files[] = {
                {"cameras", f.cameras}, {"images", f.images},
                {"points3D", f.points3D}};
            std::vector<std::string> present, missing;
            for (const auto& [base, e] : files)
                (e == ColmapFmt::None ? missing : present)
                    .push_back(std::string(base) +
                               (e == ColmapFmt::Bin  ? ".bin"
                                : e == ColmapFmt::Text ? ".txt"
                                                       : ".bin or .txt"));
            if (present.empty() || missing.empty()) continue;
            *near_miss = (rel.empty() ? std::string("the dataset dir")
                                      : "'" + rel + "'") +
                         " has " + join_files(present) + " but no " +
                         join_files(missing);
            return {};
        }
    }
    return {};
}

ParsedDataset parse_colmap_dataset(const std::string& dataset_dir,
                                   const DatasetParserConfig& cfg) {
    ColmapModelFmt fmt;
    std::string near_miss;
    std::string recon_dir = find_colmap_recon(dataset_dir, cfg, &fmt,
                                              /*verbose=*/true, &near_miss);
    if (recon_dir.empty())
        throw std::runtime_error(
            "ColmapParser: no COLMAP reconstruction (cameras/images/points3D"
            " .bin or .txt) found under " + dataset_dir +
            (near_miss.empty() ? "" : " -- " + near_miss));

    auto cameras = fmt.cameras == ColmapFmt::Text ? read_cameras_text(recon_dir)
                                                  : read_cameras_binary(recon_dir);
    auto images  = fmt.images == ColmapFmt::Text ? read_images_text(recon_dir)
                                                 : read_images_binary(recon_dir);

    // EQUIRECTANGULAR sanity, once per camera rather than once per frame.
    for (const auto& [id, cam] : cameras) {
        if (cam.model != "EQUIRECTANGULAR") continue;
        if (cam.params.size() != 2)
            throw std::runtime_error("ColmapParser: EQUIRECTANGULAR camera " +
                                     std::to_string(id) + " has " +
                                     std::to_string(cam.params.size()) +
                                     " params, expected 2 (w, h)");
        // COLMAP stores (w, h) as *metadata* params, so they are the camera's
        // own dimensions by construction. A mismatch means a corrupt or
        // hand-edited model, and the two would disagree about the projection.
        if (cam.params[0] != (double)cam.width || cam.params[1] != (double)cam.height)
            throw std::runtime_error(
                "ColmapParser: EQUIRECTANGULAR camera " + std::to_string(id) +
                " params (" + std::to_string((int64_t)cam.params[0]) + ", " +
                std::to_string((int64_t)cam.params[1]) + ") disagree with its "
                "resolution (" + std::to_string(cam.width) + ", " +
                std::to_string(cam.height) + ")");
        // The engine's canonical panorama intrinsics use fy = w/(2*pi), which
        // equals COLMAP's h/pi only on a 2:1 panorama (see bake_post_split).
        // Anything else is trained with the wrong vertical angular scale.
        if (cam.height * 2 != cam.width)
            std::printf("%s %s\n", spirula::i18n::msg::data::word_warning.get(),
                        spirula::i18n::format(
                            spirula::i18n::msg::data::equirect_not_2to1,
                            {(int)id, (unsigned long long)cam.width,
                             (unsigned long long)cam.height})
                            .c_str());
    }

    // ---- Assemble frames sorted by image filename ----------------------
    std::vector<const ColmapImage*> frames;
    frames.reserve(images.size());
    for (const auto& [id, im] : images) frames.push_back(&im);
    std::sort(frames.begin(), frames.end(),
              [](const ColmapImage* a, const ColmapImage* b) { return a->name < b->name; });

    // ---- All-frame c2w (needed for outlier filter + train_frame_scale) ----
    int64_t n_all = (int64_t)frames.size();
    std::vector<float> c2w_all(n_all * 12);
    std::vector<double> positions(n_all * 3);
    for (int64_t i = 0; i < n_all; i++) {
        colmap_to_c2w(*frames[i], &c2w_all[i*12]);
        for (int r = 0; r < 3; r++) positions[i*3 + r] = c2w_all[i*12 + r*4 + 3];
    }

    // ---- Outlier rejection ---------------------------------------------
    {
        std::vector<char> keep = dsparse::outlier_keep_mask(
            positions, n_all, cfg.outlier_threshold);
        std::vector<const ColmapImage*> kept;
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

    // ---- train_frame_scale + viewer remap transform over ALL post-outlier
    // frames (train + eval, matching the Python dataparser, which splits
    // after normalization). No applied_transform on the COLMAP path, so
    // train_to_normalized = inv(T_n_from_camera). -----------------------------
    double T_n[16], T_inv[16];
    double scale_factor = dsparse::compute_normalized_transform(c2w_all, n_all, T_n);
    dsparse::invert_affine4x4(T_n, T_inv);
    float train_frame_scale = (float)(scale_factor != 0.0 ? 1.0 / scale_factor : 1.0);

    // ---- eval_mode train subset --------------------------------------------
    std::vector<std::string> names(n_all);
    for (int64_t i = 0; i < n_all; i++) names[i] = frames[i]->name;
    std::vector<int64_t> subset = dsparse::train_subset(n_all, names, cfg);

    fs::path image_dir = fs::path(dataset_dir) / cfg.image_dir;

    ParsedDataset ds;
    const int64_t N = (int64_t)subset.size();
    ds.num_cameras = N;
    ds.train_frame_scale = train_frame_scale;
    for (int k = 0; k < 16; k++) ds.train_to_normalized[k] = (float)T_inv[k];
    ds.camera_models.reserve(N);
    ds.camera_distortions.reserve(N);
    ds.image_filenames.reserve(N);
    ds.widths.reserve(N);
    ds.heights.reserve(N);
    ds.c2w.resize(N * 12);
    ds.intrins.resize(N * 4);
    ds.dist_coeffs.resize(N * kCameraDistortionParams);

    // One bake per COLMAP camera record, not per frame: a record is one
    // physical camera, and a fitted one costs a least-squares solve.
    std::map<int32_t, BakedIntrins> baked;
    std::map<std::string, FitReport> fit_reports;
    bool any_redistort = false;
    for (const auto& [id, cam] : cameras) {
        BakedIntrins bi = bake_colmap_intrins(cam);
        if (bi.source.source_model >= 0) {
            // May clear source_model again when the fit turned out exact.
            fit_colmap_source(cam, bi, fit_reports);
            any_redistort |= (bi.source.source_model >= 0);
        }
        baked.emplace(id, bi);
    }
    print_fit_reports(fit_reports);
    if (any_redistort) ds.redistort.resize(N);

    std::vector<std::string> mask_files(N), depth_files(N), normal_files(N);
    bool any_mask = false, any_depth = false, any_normal = false;

    for (int64_t j = 0; j < N; j++) {
        const int64_t i = subset[j];
        const ColmapImage& im = *frames[i];
        auto cam_it = cameras.find(im.camera_id);
        if (cam_it == cameras.end())
            throw std::runtime_error("ColmapParser: image " + im.name +
                                     " references missing camera id " +
                                     std::to_string(im.camera_id));
        const ColmapCamera& cam = cam_it->second;

        fs::path img_path = image_dir / im.name;
        if (cfg.require_image_files && !fs::exists(img_path))
            throw std::runtime_error("ColmapParser: " + img_path.string() +
                                     " does not exist (set --image-dir if needed)");
        ds.image_filenames.push_back(img_path.string());

        BakedIntrins bi = baked.at(im.camera_id);
        double W = (double)cam.width, H = (double)cam.height;
        double fx = bi.fx, fy = bi.fy, cx = bi.cx, cy = bi.cy;
        dsparse::fit_camera_resolution(cfg, ds.image_filenames.back(),
                                       W, H, fx, fy, cx, cy, &bi.source);
        ds.widths.push_back((int32_t)W);
        ds.heights.push_back((int32_t)H);
        ds.intrins[j*4 + 0] = (float)fx;
        ds.intrins[j*4 + 1] = (float)fy;
        ds.intrins[j*4 + 2] = (float)cx;
        ds.intrins[j*4 + 3] = (float)cy;
        std::copy(bi.dist.begin(), bi.dist.end(),
                  ds.dist_coeffs.begin() + j*kCameraDistortionParams);

        ds.camera_models.push_back((int32_t)bi.model);
        ds.camera_distortions.push_back((int32_t)bi.distortion);
        if (any_redistort) ds.redistort[j] = bi.source;

        std::copy(&c2w_all[i*12], &c2w_all[i*12] + 12, &ds.c2w[j*12]);

        // Auxiliary supervision buffers, discovered by filename convention.
        mask_files[j]   = dsparse::find_aux_file(
            (fs::path(dataset_dir) / cfg.mask_dir).string(),   im.name, "mask");
        depth_files[j]  = dsparse::find_aux_file(
            (fs::path(dataset_dir) / cfg.depth_dir).string(),  im.name, "depth");
        normal_files[j] = dsparse::find_aux_file(
            (fs::path(dataset_dir) / cfg.normal_dir).string(), im.name, "normal");
        any_mask   |= !mask_files[j].empty();
        any_depth  |= !depth_files[j].empty();
        any_normal |= !normal_files[j].empty();
    }
    if (any_mask)   ds.mask_filenames   = std::move(mask_files);
    if (any_depth)  ds.depth_filenames  = std::move(depth_files);
    if (any_normal) ds.normal_filenames = std::move(normal_files);

    // In lenient (viewer) mode, tolerate a missing points3D file so a
    // cameras-only reconstruction still yields camera poses / frustums.
    if (fmt.points3D == ColmapFmt::Bin)
        ds.points = read_points3D_binary(recon_dir);
    else if (fmt.points3D == ColmapFmt::Text)
        ds.points = read_points3D_text(recon_dir);
    // validation_fraction holds out part of the TRAIN set; the eval split is
    // already a held-out set, so it is all "train" from the DataManager's
    // point of view.
    dsparse::assign_val_split(
        ds, cfg.split == "eval" ? 0.0f : cfg.validation_fraction);
    return ds;
}


// ===========================================================================
// Format dispatch / auto-detect. Probe order: nerfstudio first, then COLMAP,
// then Metashape.
//
// Auto-detect *identifies* the format from its marker files before parsing,
// rather than trying each parser and reporting whichever failed last -- a
// directory that is not a dataset at all used to surface as a Metashape
// complaint, which told the user nothing about what was actually wrong.
// ===========================================================================

// A Metashape camera export is <document ...><chunk ...><sensors>...; sniff
// the head of the file so an unrelated .xml that happens to sit in the
// directory is not mistaken for one (a plain "*.xml is present" test makes
// any random folder look like a Metashape dataset).
static bool looks_like_metashape_xml(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    char buf[8192];
    f.read(buf, sizeof buf);
    std::string head(buf, (size_t)f.gcount());
    return head.find("<document") != std::string::npos &&
           head.find("<chunk") != std::string::npos;
}

// Does the dataset dir hold a Metashape camera export? A .xml is the only
// mandatory input (MetashapeParser.cpp resolve_input); an explicitly
// configured one counts unconditionally -- the user named it, so any problem
// with it belongs in the Metashape parser's own error message.
static bool has_metashape_xml(const std::string& dataset_dir,
                              const DatasetParserConfig& cfg) {
    if (!cfg.metashape_xml.empty()) return true;
    std::error_code ec;
    for (fs::directory_iterator it(dataset_dir, ec), end; !ec && it != end;
         it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        std::string ext = it->path().extension().string();
        for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
        if (ext == ".xml" && looks_like_metashape_xml(it->path())) return true;
    }
    return false;
}

ParsedDataset parse_dataset(const std::string& dataset_dir,
                            const DatasetParserConfig& cfg,
                            const std::string& format) {
    std::error_code ec;
    if (!fs::exists(fs::path(dataset_dir), ec))
        throw std::runtime_error("dataset path does not exist: " + dataset_dir);
    if (!fs::is_directory(fs::path(dataset_dir), ec))
        throw std::runtime_error("dataset path is not a directory: " + dataset_dir +
                                 " (--data must point at the dataset folder)");

    if (format == "colmap")     return parse_colmap_dataset(dataset_dir, cfg);
    if (format == "nerfstudio") return parse_nerfstudio_dataset(dataset_dir, cfg);
    if (format == "metashape")  return parse_metashape_dataset(dataset_dir, cfg);
    if (!format.empty())
        throw std::runtime_error("unsupported data format: '" + format +
                                 "' (expected colmap, nerfstudio or metashape)");

    // ---- Auto-detect: probe for each format's marker files ----------------
    if (fs::exists(fs::path(dataset_dir) / "transforms.json"))
        return parse_nerfstudio_dataset(dataset_dir, cfg);

    ColmapModelFmt fmt;
    std::string colmap_near_miss;
    bool has_colmap = !find_colmap_recon(dataset_dir, cfg, &fmt,
                                         /*verbose=*/false,
                                         &colmap_near_miss).empty();
    bool has_metashape = has_metashape_xml(dataset_dir, cfg);

    if (has_colmap) {
        if (!has_metashape) return parse_colmap_dataset(dataset_dir, cfg);
        // Both markers present: COLMAP still wins, but a failure there is
        // worth retrying as Metashape rather than aborting the run.
        try {
            return parse_colmap_dataset(dataset_dir, cfg);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "%s\n",
                         spirula::i18n::format(
                             spirula::i18n::msg::data::colmap_parse_failed,
                             {e.what()})
                             .c_str());
            std::fprintf(stderr, "%s\n",
                         spirula::i18n::msg::data::trying_metashape.get());
        }
    }
    if (has_metashape) return parse_metashape_dataset(dataset_dir, cfg);

    throw std::runtime_error(
        dataset_dir + " does not look like a supported dataset.\n"
        "Looked for:\n"
        "  nerfstudio  transforms.json in the dataset dir\n"
        "  COLMAP      cameras/images/points3D (.bin or .txt) under sparse/0,\n"
        "              colmap/sparse/0, sparse, colmap or the dataset dir itself\n"
        "  Metashape   a camera-export .xml in the dataset dir\n" +
        (colmap_near_miss.empty() ? std::string()
                                  : "Closest match: " + colmap_near_miss + ".\n") +
        "Point --data at the dataset folder that contains one of these, or use\n"
        "--data-format and the per-format path options to name the inputs.");
}

// Keypoints + descriptors and the flat-file interchange format.
//
// The layout is deliberately dtype/dim-agnostic (docs/notes/sfm-design.md D1): SIFT is
// 128-D uint8 today, a learned frontend later may be F32 and a different width,
// and nothing downstream may assume otherwise. `features.bin` is our own format
// (D4); a converter to/from COLMAP's SQLite database is a separate host tool.
#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace sfm {

// Similarity-covariant keypoint in *original*-image pixel coordinates (top-left
// origin, +x right, +y down), matching COLMAP's FeatureKeypoint(x,y,scale,orn).
// "Original" is the source file's resolution, not the possibly-downscaled one
// SIFT ran on: the extractor scales its output back (see scaleKeypoints), so a
// camera built from a FeatureSet describes the images on disk (D46).
struct Keypoint {
    float x = 0, y = 0;         // subpixel location
    float scale = 0;            // sigma in original-image pixels
    float orientation = 0;      // radians, CCW from +x
    // SIFT: |DoG| at the refined extremum. A learned detector: its detection
    // score. Persisted from v5 -- ALIKED has neither scale nor orientation, so
    // this is the only ranking signal its keypoints carry, and anything that
    // used to rank by scale has to fall back to it (Pairing / PairSelection).
    float response = 0;
};

enum class DType : uint32_t { U8 = 0, F32 = 1 };

inline uint32_t dtypeSize(DType t) { return t == DType::U8 ? 1u : 4u; }

// One image's features. Descriptors are row-major: feature i occupies
// data[i*dim .. i*dim+dim) as `dtype` elements.
struct FeatureSet {
    int width = 0, height = 0;          // source image size features refer to
    // The size SIFT actually ran at, which is smaller than (width,height)
    // whenever the loader downscaled to --max-image-size. 0 means "the same",
    // which is what a file written before this field says. Keypoint coordinates
    // are in the *source* frame (see scaleKeypoints), but their localization
    // noise is a property of *this* frame, so it is what pixel thresholds are
    // measured in (D47).
    int extract_width = 0, extract_height = 0;
    // The focal length EXIF claims for this image, in pixels of (width,height),
    // and the camera identity EXIF gives it (sfm/core/Exif.h). 0 / empty when the
    // file had no usable EXIF. Recorded at extraction because that is the last
    // stage that sees the image file; matching and mapping only see features.
    double exif_focal = 0;
    std::string exif_camera;
    // The Orientation tag, 1..8, of the pixels as the mapper will see them --
    // so 1 once `--exif-orientation apply` has turned them (sfm/core/Exif.h).
    // The gauge fix reads it: a portrait capture's up is not the image's.
    uint8_t exif_orientation = 1;
    uint32_t dim = 128;
    DType dtype = DType::U8;
    std::vector<Keypoint> keypoints;
    std::vector<uint8_t> descriptors;   // count*dim*dtypeSize bytes
    // Optional per-keypoint RGB sampled from the source image (count*3 bytes, or
    // empty). Written by `spirula-sfm extract`; the mapper averages them over each 3D
    // point's track to color the point cloud for 3DGS init. Empty for a feature
    // set produced without an image (e.g. selftest) or read from a v1 file.
    std::vector<uint8_t> colors;

    uint32_t count() const { return (uint32_t)keypoints.size(); }
    bool hasColors() const { return colors.size() == (size_t)count() * 3; }
    // Whether any keypoint carries a detection score worth persisting. SIFT
    // leaves response at 0; a learned detector fills it in.
    bool hasScores() const {
        for (const Keypoint& k : keypoints)
            if (k.response != 0) return true;
        return false;
    }
    // What a subset selection should rank by. Scale for SIFT (D16: the largest
    // scales are the most repeatable), the detection score for a detector that
    // has no scale. Never both -- an extractor fills in one of them.
    float rank(uint32_t i) const {
        const Keypoint& k = keypoints[i];
        return k.scale > 0 ? k.scale : k.response;
    }
    // How many source pixels one extraction pixel is worth: >= 1, and exactly 1
    // when nothing was downscaled. The two axes agree up to the rounding in
    // the size clamp, so their mean is the isotropic answer.
    double pixelScale() const {
        if (extract_width <= 0 || extract_height <= 0 || width <= 0 || height <= 0) return 1.0;
        return 0.5 * ((double)width / extract_width + (double)height / extract_height);
    }
};

// Scale keypoints from the coordinates of a (dw,dh) image to those of a (w,h)
// one, and record the new size. The extractor runs SIFT on a downscaled copy
// when the source is larger than --max-image-size; everything downstream --
// cameras.bin above all -- should still be in the coordinates of the file the
// user has (COLMAP's ScaleKeypoints does the same, feature/utils.cc).
//
// The mapping is the exact inverse of the bilinear resample in sfm/core/Image.h: a
// destination pixel center (x+0.5) came from source (x+0.5)*sx. Sigma scales by
// the geometric mean, the only isotropic choice when the two axes round
// differently.
inline void scaleKeypoints(FeatureSet& fs, int w, int h) {
    if (w <= 0 || h <= 0 || fs.width <= 0 || fs.height <= 0) return;
    fs.extract_width = fs.width;    // where the keypoints were actually measured
    fs.extract_height = fs.height;
    if (w == fs.width && h == fs.height) return;
    const float sx = (float)w / fs.width, sy = (float)h / fs.height;
    const float ss = std::sqrt(sx * sy);
    for (Keypoint& k : fs.keypoints) {
        k.x = (k.x + 0.5f) * sx - 0.5f;
        k.y = (k.y + 0.5f) * sy - 0.5f;
        k.scale *= ss;
    }
    fs.width = w;
    fs.height = h;
}

// ---- features.bin -------------------------------------------------------
// The layout and every version's appended section: src/sfm/README.md. Each one
// is read back as its own absence, so a stale cache is reused, not rejected.

// Written to a sibling and renamed over the destination, so a run killed mid
// stage leaves a file that is either whole or absent -- which is what lets the
// next run reuse it without reading the descriptor block back to check (D76).
inline void writeFeatures(const std::string& path, const FeatureSet& fs) {
    const std::string tmp = path + ".part";
    {
    std::ofstream f(tmp, std::ios::binary);
    if (!f) throw std::runtime_error("cannot write " + path);
    uint32_t version = 6, count = fs.count(), dtype = (uint32_t)fs.dtype;
    f.write("VKFT", 4);
    f.write((const char*)&version, 4);
    f.write((const char*)&fs.width, 4);
    f.write((const char*)&fs.height, 4);
    f.write((const char*)&count, 4);
    f.write((const char*)&fs.dim, 4);
    f.write((const char*)&dtype, 4);
    for (const Keypoint& k : fs.keypoints) {
        float v[4] = {k.x, k.y, k.scale, k.orientation};
        f.write((const char*)v, sizeof v);
    }
    f.write((const char*)fs.descriptors.data(), (std::streamsize)fs.descriptors.size());
    uint8_t has_colors = fs.hasColors() ? 1 : 0;
    f.write((const char*)&has_colors, 1);
    if (has_colors) f.write((const char*)fs.colors.data(), (std::streamsize)fs.colors.size());
    uint32_t cam_len = (uint32_t)fs.exif_camera.size();
    f.write((const char*)&fs.exif_focal, 8);
    f.write((const char*)&cam_len, 4);
    f.write(fs.exif_camera.data(), (std::streamsize)cam_len);
    f.write((const char*)&fs.extract_width, 4);
    f.write((const char*)&fs.extract_height, 4);
    uint8_t has_scores = fs.hasScores() ? 1 : 0;
    f.write((const char*)&has_scores, 1);
    if (has_scores)
        for (const Keypoint& k : fs.keypoints) f.write((const char*)&k.response, 4);
    f.write((const char*)&fs.exif_orientation, 1);
    if (!f) throw std::runtime_error("cannot write " + path);
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        std::filesystem::remove(tmp, ec);
        throw std::runtime_error("cannot write " + path);
    }
}

// Is there a whole feature file at `path`, and how many keypoints does it hold?
// The keypoints and descriptors are all but the whole file, so the size against
// the header catches a truncated one without reading a gigabyte to find out.
inline bool peekFeatures(const std::string& path, uint32_t& count) {
    std::error_code ec;
    const uint64_t bytes = (uint64_t)std::filesystem::file_size(path, ec);
    if (ec) return false;
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    char magic[4];
    uint32_t version = 0, n = 0, dim = 0, dtype = 0;
    int w = 0, h = 0;
    f.read(magic, 4);
    f.read((char*)&version, 4);
    f.read((char*)&w, 4);
    f.read((char*)&h, 4);
    f.read((char*)&n, 4);
    f.read((char*)&dim, 4);
    f.read((char*)&dtype, 4);
    if (!f || std::memcmp(magic, "VKFT", 4) != 0 || dtype > 1) return false;
    const uint64_t need = 28 + (uint64_t)n * 16 + (uint64_t)n * dim * dtypeSize((DType)dtype);
    if (bytes < need) return false;
    count = n;
    return true;
}

// `with_descriptors == false` seeks past the descriptor block instead of
// reading it. Everything downstream of matching -- the mapper, and so the
// whole `map` subcommand -- wants only keypoints and colors, and the
// descriptors are the file: a gigabyte per thousand images, read and then
// never touched.
inline FeatureSet readFeatures(const std::string& path, bool with_descriptors = true) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot read " + path);
    char magic[4];
    f.read(magic, 4);
    if (std::memcmp(magic, "VKFT", 4) != 0) throw std::runtime_error("bad magic in " + path);
    uint32_t version, count, dim, dtype;
    f.read((char*)&version, 4);
    FeatureSet fs;
    f.read((char*)&fs.width, 4);
    f.read((char*)&fs.height, 4);
    f.read((char*)&count, 4);
    f.read((char*)&dim, 4);
    f.read((char*)&dtype, 4);
    fs.dim = dim;
    fs.dtype = (DType)dtype;
    fs.keypoints.resize(count);
    {   // one read, not one per keypoint: a 1200-image capture has ten million
        // of them and the per-call stream overhead dominated the actual copy.
        std::vector<float> raw((size_t)count * 4);
        f.read((char*)raw.data(), (std::streamsize)(raw.size() * sizeof(float)));
        for (uint32_t i = 0; i < count; i++) {
            const float* v = &raw[(size_t)i * 4];
            fs.keypoints[i] = {v[0], v[1], v[2], v[3], 0.0f};
        }
    }
    const std::streamsize desc_bytes = (std::streamsize)((size_t)count * dim *
                                                         dtypeSize(fs.dtype));
    if (with_descriptors) {
        fs.descriptors.resize((size_t)desc_bytes);
        f.read((char*)fs.descriptors.data(), desc_bytes);
    } else {
        f.seekg(desc_bytes, std::ios::cur);
    }
    if (version >= 2) {
        uint8_t has_colors = 0;
        f.read((char*)&has_colors, 1);
        if (has_colors) {
            fs.colors.resize((size_t)count * 3);
            f.read((char*)fs.colors.data(), (std::streamsize)fs.colors.size());
        }
    }
    if (version >= 3) {
        // Read into locals and only publish a complete section: a file
        // truncated mid-EXIF must read back as "no EXIF", not as a garbage
        // focal length that would then be trusted as a prior.
        double focal = 0;
        uint32_t cam_len = 0;
        f.read((char*)&focal, 8);
        f.read((char*)&cam_len, 4);
        if (f.gcount() == 4) {
            fs.exif_focal = focal;
            if (cam_len > 0 && cam_len < (1u << 16)) {
                std::string cam(cam_len, '\0');
                f.read(&cam[0], (std::streamsize)cam_len);
                if (f.gcount() == (std::streamsize)cam_len) fs.exif_camera = std::move(cam);
            }
        }
    }
    if (version >= 4) {
        int ew = 0, eh = 0;
        f.read((char*)&ew, 4);
        f.read((char*)&eh, 4);
        if (f.gcount() == 4 && ew > 0 && eh > 0) {
            fs.extract_width = ew;
            fs.extract_height = eh;
        }
    }
    if (version >= 5) {
        uint8_t has_scores = 0;
        f.read((char*)&has_scores, 1);
        if (f.gcount() == 1 && has_scores) {
            std::vector<float> raw(count);
            f.read((char*)raw.data(), (std::streamsize)(raw.size() * sizeof(float)));
            for (uint32_t i = 0; i < count; i++) fs.keypoints[i].response = raw[i];
        }
    }
    if (version >= 6) {
        uint8_t o = 0;
        f.read((char*)&o, 1);
        if (f.gcount() == 1 && o >= 1 && o <= 8) fs.exif_orientation = o;
    }
    return fs;
}

}  // namespace sfm

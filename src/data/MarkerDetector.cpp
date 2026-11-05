// MarkerDetector.cpp -- wrapper around the AprilTag detector.
#include "data/MarkerDetector.h"

#include "apriltag.h"
#include "tag36h10.h"
#include "tag36h11.h"
#include "tagAruco4x4_1000.h"
#include "tagAruco5x5_1000.h"
#include "tagAruco6x6_1000.h"
#include "tagAruco7x7_1000.h"
#include "external/stb_image.h"

#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace spirula {

const char* marker_family_name(MarkerFamily family) {
    switch (family) {
    case MarkerFamily::Auto: return "auto";
    case MarkerFamily::Tag36h10: return "tag36h10";
    case MarkerFamily::Tag36h11: return "tag36h11";
    case MarkerFamily::Aruco4x4: return "aruco4x4_1000";
    case MarkerFamily::Aruco5x5: return "aruco5x5_1000";
    case MarkerFamily::Aruco6x6: return "aruco6x6_1000";
    case MarkerFamily::Aruco7x7: return "aruco7x7_1000";
    case MarkerFamily::ArucoOriginal: return "aruco_original";
    }
    return "unknown";
}

bool parse_marker_family(const std::string& name, MarkerFamily& family) {
    for (int i = -1; i <= (int)MarkerFamily::ArucoOriginal; ++i) {
        const auto f = (MarkerFamily)i;
        if (name == marker_family_name(f) ||
            (f == MarkerFamily::Tag36h10 && name == "36h10") ||
            (f == MarkerFamily::Tag36h11 && name == "36h11")) {
            family = f;
            return true;
        }
    }
    return false;
}

static apriltag_family_t* original_family() {
    auto* tf = tagAruco5x5_1000_create();
    free(tf->name);
    tf->name = (char*)malloc(sizeof("aruco_original"));
    std::memcpy(tf->name, "aruco_original", sizeof("aruco_original"));
    tf->ncodes = 1024;
    tf->h = 3;
    tf->codes = (uint64_t*)calloc(tf->ncodes, sizeof(uint64_t));
    // ArUco Original encodes two ID bits per row, most significant row first.
    constexpr int rows[4] = {0x10, 0x17, 0x09, 0x0e};
    for (unsigned id = 0; id < tf->ncodes; ++id) {
        uint64_t code = 0;
        for (unsigned k = 0; k < tf->nbits; ++k) {
            const int y = tf->bit_y[k] - 1, x = tf->bit_x[k] - 1;
            code = (code << 1) | ((rows[(id >> (2 * (4 - y))) & 3] >> (4 - x)) & 1);
        }
        tf->codes[id] = code;
    }
    return tf;
}

struct MarkerDetector::Impl {
    std::vector<std::pair<MarkerFamily, apriltag_family_t*>> families;
    apriltag_detector_t* td = nullptr;

    Impl(MarkerFamily fam, int nthreads, double decimate) {
        if (!std::isfinite(decimate) || decimate < 1.0 || decimate > 8.0)
            throw std::invalid_argument("marker decimation must be between 1 and 8");
        td = apriltag_detector_create();
        for (int i = 0; i <= (int)MarkerFamily::ArucoOriginal; ++i) {
            const auto family = (MarkerFamily)i;
            if (fam != MarkerFamily::Auto && fam != family) continue;
            apriltag_family_t* tf = nullptr;
            switch (family) {
            case MarkerFamily::Tag36h10: tf = tag36h10_create(); break;
            case MarkerFamily::Tag36h11: tf = tag36h11_create(); break;
            case MarkerFamily::Aruco4x4: tf = tagAruco4x4_1000_create(); break;
            case MarkerFamily::Aruco5x5: tf = tagAruco5x5_1000_create(); break;
            case MarkerFamily::Aruco6x6: tf = tagAruco6x6_1000_create(); break;
            case MarkerFamily::Aruco7x7: tf = tagAruco7x7_1000_create(); break;
            case MarkerFamily::ArucoOriginal: tf = original_family(); break;
            default: break;
            }
            if (tf) {
                families.emplace_back(family, tf);
                // Dense ArUco dictionaries need exact decoding to avoid ID collisions.
                const int bits = family <= MarkerFamily::Tag36h11 ? 2 : 0;
                apriltag_detector_add_family_bits(td, tf, bits);
            }
        }
        td->nthreads = nthreads > 0 ? nthreads : 1;
        td->quad_decimate = (float)std::max(1.0, decimate);
        td->refine_edges = 1;
    }

    ~Impl() {
        if (td) apriltag_detector_destroy(td);
        for (const auto& f : families) {
            auto* tf = f.second;
            if (f.first == MarkerFamily::ArucoOriginal) free(tf->codes);
            free(tf->bit_x);
            free(tf->bit_y);
            free(tf->name);
            free(tf);
        }
    }
};

MarkerDetector::MarkerDetector(MarkerFamily family, int nthreads, double decimate)
    : _impl(std::make_unique<Impl>(family, nthreads, decimate)) {}

MarkerDetector::~MarkerDetector() = default;
MarkerDetector::MarkerDetector(MarkerDetector&&) noexcept = default;
MarkerDetector& MarkerDetector::operator=(MarkerDetector&&) noexcept = default;

std::vector<MarkerDetection> MarkerDetector::detect_grayscale(const uint8_t* gray, int w, int h, int stride) {
    if (!gray || w <= 0 || h <= 0 || !_impl || !_impl->td) return {};
    if (stride <= 0) stride = w;
    if (stride < w) return {};

    image_u8_t im = {
        w,
        h,
        stride,
        const_cast<uint8_t*>(gray)
    };

    zarray_t* detections = apriltag_detector_detect(_impl->td, &im);
    if (!detections) return {};

    const int n = zarray_size(detections);
    std::vector<MarkerDetection> out;
    out.reserve((size_t)n);

    for (int i = 0; i < n; i++) {
        apriltag_detection_t* det = nullptr;
        zarray_get(detections, i, &det);
        if (!det) continue;

        MarkerDetection m;
        for (const auto& f : _impl->families)
            if (f.second == det->family) m.family = f.first;
        m.id = det->id;
        m.center[0] = det->c[0];
        m.center[1] = det->c[1];
        for (int k = 0; k < 4; k++) {
            m.corners[k][0] = det->p[k][0];
            m.corners[k][1] = det->p[k][1];
        }
        m.hamming = (double)det->hamming;
        m.margin = (double)det->decision_margin;
        out.push_back(m);
    }

    apriltag_detections_destroy(detections);
    return out;
}

std::vector<MarkerDetection> MarkerDetector::detect_rgb(const uint8_t* rgb, int w, int h, int channels) {
    if (!rgb || w <= 0 || h <= 0 || channels < 1) return {};
    if (channels == 1) return detect_grayscale(rgb, w, h, w);

    std::vector<uint8_t> gray((size_t)w * h);
    const size_t step = (size_t)channels;
    for (int y = 0; y < h; y++) {
        const uint8_t* src = rgb + (size_t)y * w * step;
        uint8_t* dst = gray.data() + (size_t)y * w;
        for (int x = 0; x < w; x++) {
            const int r = src[0], g = channels > 2 ? src[1] : r, b = channels > 2 ? src[2] : r;
            dst[x] = (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);
            src += step;
        }
    }
    return detect_grayscale(gray.data(), w, h, w);
}

std::vector<MarkerDetection> MarkerDetector::detect_file(const std::string& path) {
    int w = 0, h = 0, comp = 0;
    stbi_uc* img = stbi_load(path.c_str(), &w, &h, &comp, 1);
    if (!img) return {};
    auto res = detect_grayscale(img, w, h, w);
    stbi_image_free(img);
    return res;
}

}  // namespace spirula


// MarkerDetector.cpp -- wrapper around the AprilTag detector.
#include "data/MarkerDetector.h"

#include "apriltag.h"
#include "tag36h10.h"
#include "tag36h11.h"
#include "external/stb_image.h"

#include <cmath>
#include <utility>

namespace spirula {

struct MarkerDetector::Impl {
    MarkerFamily family;
    apriltag_family_t* tf = nullptr;
    apriltag_detector_t* td = nullptr;

    Impl(MarkerFamily fam, int nthreads, double decimate) : family(fam) {
        td = apriltag_detector_create();
        if (family == MarkerFamily::Tag36h10) {
            tf = tag36h10_create();
        } else {
            tf = tag36h11_create();
        }
        apriltag_detector_add_family(td, tf);
        td->nthreads = nthreads > 0 ? nthreads : 1;
        td->quad_decimate = (float)std::max(1.0, decimate);
        td->refine_edges = 1;
    }

    ~Impl() {
        if (td) apriltag_detector_destroy(td);
        if (tf) {
            if (family == MarkerFamily::Tag36h10) tag36h10_destroy(tf);
            else tag36h11_destroy(tf);
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
            const int r = src[0], g = src[1], b = src[2];
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


// MarkerDetector.h -- AprilTag and ArUco detection on host images.
#pragma once

#include "data/MarkerTypes.h"

#include <memory>
#include <string>
#include <vector>

namespace spirula {

class MarkerDetector {
public:
    explicit MarkerDetector(MarkerFamily family = MarkerFamily::Tag36h10,
                            int nthreads = 4, double decimate = 1.0);
    ~MarkerDetector();

    MarkerDetector(const MarkerDetector&) = delete;
    MarkerDetector& operator=(const MarkerDetector&) = delete;
    MarkerDetector(MarkerDetector&&) noexcept;
    MarkerDetector& operator=(MarkerDetector&&) noexcept;

    std::vector<MarkerDetection> detect_grayscale(const uint8_t* gray, int w, int h, int stride = 0);
    std::vector<MarkerDetection> detect_rgb(const uint8_t* rgb, int w, int h, int channels = 3);
    std::vector<MarkerDetection> detect_file(const std::string& path);

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

}  // namespace spirula


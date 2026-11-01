#pragma once

#include "data/MarkerTriangulate.h"
#include "core/Similarity.h"

#include <atomic>
#include <functional>

namespace spirula {

struct MarkerScanResult {
    std::vector<Marker3D> markers;
    MarkerFamily family = MarkerFamily::Auto;
    int images = 0, unreadable = 0, observations = 0;
    bool cancelled = false;
};

MarkerScanResult scan_markers(const ParsedDataset& ds, MarkerFamily family,
                              double decimate, int threads,
                              const std::atomic<bool>* cancel = nullptr,
                              const std::function<void(int, int)>& progress = {});

std::string marker_dataset_dir(const std::string& path);
std::string find_markers_file(const std::string& path);

struct MarkerDataset {
    std::string directory;
    DatasetParserConfig config;
    Sim3 file_from_dataset;
};

MarkerDataset resolve_marker_dataset(const std::string& path);

}  // namespace spirula

#pragma once

// A laser scan's points, whichever file they came in -- E57, LAS or PLY --
// streamed in batches in the file's own frame, so a 100M-point cloud never has
// to fit in memory. docs/notes/lidar-alignment.md.

#include "data/E57Reader.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace spirula::cloud {

enum class Format { E57, Las, Ply };

// Where a scanner stood, when the file says so: a structured E57 scan's pose,
// or the `camera` element a PCL / ETH3D PLY carries. Row-major R, metres.
struct Station {
    double origin[3] = {0, 0, 0};
    double R[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
};

struct Info {
    Format format = Format::Ply;
    int64_t points = 0;              // as declared; invalid ones are dropped on read
    bool has_color = false;          // otherwise the points are intensity grey, or 200
    std::vector<Station> stations;
};

class Reader {
public:
    explicit Reader(const std::string& path);   // throws std::runtime_error
    ~Reader();

    const Info& info() const { return _info; }
    // The E57 file behind this reader, for its images; null for LAS and PLY.
    e57::Reader* e57() { return _e57.get(); }

    // Every point, in batches; false when `cancel` stopped it.
    bool read(const std::function<void(const e57::Point*, size_t)>& sink,
              const std::atomic<bool>* cancel = nullptr);

private:
    struct Las;
    struct Ply;
    std::string _path;
    Info _info;
    std::unique_ptr<e57::Reader> _e57;
    std::unique_ptr<Las> _las;
    std::unique_ptr<Ply> _ply;
};

// By extension: .e57, .las, .ply (any case). LAZ is recognised so the caller
// can say why it is refused.
bool is_cloud_path(const std::string& path);
bool is_laz_path(const std::string& path);

}  // namespace spirula::cloud

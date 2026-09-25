#pragma once

// E57Reader -- the ASTM E2807 laser-scan container: its scans (point clouds,
// each with a pose) and the images registered to them. Read-only, no external
// dependencies. The format notes, and the camera conventions measured on real
// files, are docs/datasets.md "E57 laser scans".

#include <atomic>
#include <cstdint>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

namespace spirula::e57 {

// p_file = R(q) p_local + t, q = (w, x, y, z), metres.
struct Pose {
    double q[4] = {1, 0, 0, 0};
    double t[3] = {0, 0, 0};
    void rotation(double R[9]) const;   // row-major
    void apply(const double in[3], double out[3]) const;
};

// `offset` is the PHYSICAL file offset of the blob's section.
struct Blob {
    uint64_t offset = 0;
    uint64_t length = 0;
    bool empty() const { return length == 0; }
};

enum class Projection { Pinhole, Spherical, Cylindrical, Reference };

struct Image {
    std::string name, guid, scan_guid;
    bool has_pose = false;
    Pose pose;
    Projection projection = Projection::Reference;
    int64_t width = 0, height = 0;
    Blob jpeg, png, mask;
    // Pinhole: focal length and pixel pitch in metres, principal point in
    // pixels with pixel centres on integers. Spherical: pitch in radians.
    double focal_length = 0, pixel_width = 0, pixel_height = 0;
    double principal_x = 0, principal_y = 0;
};

struct Field {
    enum class Kind { Float, Integer, ScaledInteger };
    std::string name;
    Kind kind = Kind::Float;
    int bits = 64;                   // bit-pack width; a Float is 32 or 64
    int64_t minimum = 0;             // Integer / ScaledInteger raw range
    double scale = 1, offset = 0;
    double lo = 0, hi = 0;           // declared value range, for normalising
    bool ranged = false;
};

struct Scan {
    std::string name, guid;
    Pose pose;
    uint64_t section = 0;            // physical offset of the points section
    int64_t count = 0;
    std::vector<Field> fields;
    bool has_color = false, has_intensity = false;
    double color_lo[3] = {0, 0, 0}, color_hi[3] = {255, 255, 255};
    double intensity_lo = 0, intensity_hi = 1;
};

// In the FILE frame. Colourless scans carry their intensity as grey, or 200.
struct Point {
    double xyz[3];
    uint8_t rgb[3];
};

class Reader {
public:
    explicit Reader(const std::string& path);   // throws std::runtime_error

    const std::vector<Scan>& scans() const { return _scans; }
    const std::vector<Image>& images() const { return _images; }
    int64_t total_points() const;

    std::vector<uint8_t> read_blob(const Blob& b);

    // Every valid point of `scan`, in batches. False when `cancel` stopped it.
    bool read_points(const Scan& scan,
                     const std::function<void(const Point*, size_t)>& sink,
                     const std::atomic<bool>* cancel = nullptr);

private:
    void read_logical(uint64_t physical, void* dst, size_t n);
    uint64_t to_logical(uint64_t physical) const;
    uint64_t to_physical(uint64_t logical) const;

    std::ifstream _file;
    std::string _path;
    uint64_t _page = 1024;
    uint64_t _length = 0;
    std::vector<uint8_t> _scratch;
    std::vector<Scan> _scans;
    std::vector<Image> _images;
};

}  // namespace spirula::e57

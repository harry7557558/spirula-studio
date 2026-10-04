#include "data/PointCloudFile.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace spirula::cloud {
namespace {

constexpr size_t kBatch = 1 << 18;

[[noreturn]] void fail(const std::string& path, const std::string& why) {
    throw std::runtime_error(path + ": " + why);
}

std::string lower_ext(const std::string& path) {
    const size_t dot = path.find_last_of('.');
    const size_t slash = path.find_last_of("/\\");
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return "";
    std::string e = path.substr(dot);
    for (char& c : e) c = (char)std::tolower((unsigned char)c);
    return e;
}

template <class T> T rd(const uint8_t* p) { T v; std::memcpy(&v, p, sizeof v); return v; }

}  // namespace

bool is_cloud_path(const std::string& path) {
    const std::string e = lower_ext(path);
    return e == ".e57" || e == ".las" || e == ".ply";
}

bool is_laz_path(const std::string& path) { return lower_ext(path) == ".laz"; }

// ================
// LAS
// ================

// ASPRS LAS 1.0-1.4, point formats 0-10, uncompressed. Colour is 16-bit by the
// standard, but writers disagree on whether they scaled 8-bit values up, so
// the range is taken from a sample of the file.
struct Reader::Las {
    std::ifstream f;
    uint64_t offset = 0, count = 0;
    uint32_t record = 0;
    int format = 0;
    double scale[3] = {1, 1, 1}, shift[3] = {0, 0, 0};
    int rgb_at = -1;
    int intensity_shift = 8;   // 16-bit -> byte
    int rgb_shift = 8;
};

namespace {

int las_rgb_offset(int format) {
    switch (format) {
        case 2: return 20;
        case 3: case 5: return 28;
        case 7: case 8: case 10: return 30;
        default: return -1;
    }
}

int las_min_record(int format) {
    static const int kSize[11] = {20, 28, 26, 34, 57, 63, 30, 36, 38, 59, 67};
    return format >= 0 && format <= 10 ? kSize[format] : 0;
}

// The smallest right shift that brings `max` into a byte.
int shift_for(uint32_t max) {
    int s = 0;
    while ((max >> s) > 255) s++;
    return s;
}

}  // namespace

// ================
// PLY
// ================

struct Reader::Ply {
    struct Prop {
        std::string name, type, count_type;   // count_type set for a list
        int size = 0;
    };
    struct Element {
        std::string name;
        int64_t count = 0;
        std::vector<Prop> props;
        int stride = -1;                      // bytes a record, -1 with a list
    };
    std::ifstream f;
    enum class Enc { Ascii, Little, Big } enc = Enc::Little;
    uint64_t body = 0;                        // byte offset after end_header
    std::vector<Element> elements;
    int vertex = -1;
};

namespace {

int ply_size(const std::string& t) {
    if (t == "char" || t == "uchar" || t == "int8" || t == "uint8") return 1;
    if (t == "short" || t == "ushort" || t == "int16" || t == "uint16") return 2;
    if (t == "int" || t == "uint" || t == "int32" || t == "uint32" || t == "float" ||
        t == "float32") return 4;
    if (t == "double" || t == "float64") return 8;
    return 0;
}

double ply_value(const uint8_t* p, const std::string& t, bool big) {
    uint8_t b[8];
    const int n = ply_size(t);
    for (int i = 0; i < n; i++) b[i] = big ? p[n - 1 - i] : p[i];
    if (t == "char" || t == "int8") return (double)rd<int8_t>(b);
    if (t == "uchar" || t == "uint8") return (double)b[0];
    if (t == "short" || t == "int16") return (double)rd<int16_t>(b);
    if (t == "ushort" || t == "uint16") return (double)rd<uint16_t>(b);
    if (t == "int" || t == "int32") return (double)rd<int32_t>(b);
    if (t == "uint" || t == "uint32") return (double)rd<uint32_t>(b);
    if (t == "float" || t == "float32") return (double)rd<float>(b);
    return rd<double>(b);
}

// A colour channel's value as a byte, by its type's range.
uint8_t ply_color(double v, const std::string& t) {
    double s = 1.0;
    if (t == "float" || t == "float32" || t == "double" || t == "float64") s = 255.0;
    else if (t == "ushort" || t == "uint16") s = 255.0 / 65535.0;
    return (uint8_t)std::clamp(std::lround(v * s), 0L, 255L);
}

}  // namespace

Reader::Reader(const std::string& path) : _path(path) {
    if (is_laz_path(path))
        fail(path, "LAZ is compressed LAS; decompress it to .las first (laszip, PDAL)");
    const std::string ext = lower_ext(path);
    if (ext == ".e57") {
        _info.format = Format::E57;
        _e57 = std::make_unique<e57::Reader>(path);
        _info.points = _e57->total_points();
        for (const e57::Scan& s : _e57->scans()) _info.has_color = _info.has_color || s.has_color;
        // A tripod project's scans each stand somewhere (its reference scan
        // at the identity); a mobile scan's one identity pose says nothing.
        const bool several = _e57->scans().size() > 1;
        for (const e57::Scan& s : _e57->scans()) {
            Station st;
            s.pose.rotation(st.R);
            for (int a = 0; a < 3; a++) st.origin[a] = s.pose.t[a];
            const bool identity = st.origin[0] == 0 && st.origin[1] == 0 &&
                                  st.origin[2] == 0 && s.pose.q[0] == 1;
            if ((several || !identity) && s.count > 0) _info.stations.push_back(st);
        }
        return;
    }
    if (ext == ".las") {
        _info.format = Format::Las;
        _las = std::make_unique<Las>();
        Las& L = *_las;
        L.f.open(path, std::ios::binary);
        if (!L.f) fail(path, "cannot open");
        uint8_t h[375] = {};
        L.f.read((char*)h, sizeof h);
        if (L.f.gcount() < 227 || std::memcmp(h, "LASF", 4) != 0) fail(path, "not a LAS file");
        const int minor = h[25];
        L.offset = rd<uint32_t>(h + 96);
        if (h[104] & 0xC0) fail(path, "the points are LAZ-compressed; decompress them first");
        L.format = h[104] & 0x3F;
        L.record = rd<uint16_t>(h + 105);
        L.count = rd<uint32_t>(h + 107);
        if (minor >= 4 && L.f.gcount() >= 255) {
            const uint64_t n64 = rd<uint64_t>(h + 247);
            if (n64) L.count = n64;
        }
        if (L.format > 10 || (int)L.record < las_min_record(L.format))
            fail(path, "point format " + std::to_string(L.format) + " is not one LAS defines");
        for (int a = 0; a < 3; a++) {
            L.scale[a] = rd<double>(h + 131 + 8 * a);
            L.shift[a] = rd<double>(h + 155 + 8 * a);
        }
        L.rgb_at = las_rgb_offset(L.format);
        _info.points = (int64_t)L.count;
        _info.has_color = L.rgb_at >= 0;
        // Ranges from up to 64 spans of 1024 records across the file.
        uint32_t max_rgb = 0, max_i = 0;
        std::vector<uint8_t> buf((size_t)L.record * 1024);
        const uint64_t spans = std::min<uint64_t>(64, (L.count + 1023) / 1024);
        for (uint64_t s = 0; s < spans; s++) {
            const uint64_t first = L.count * s / std::max<uint64_t>(spans, 1);
            const uint64_t n = std::min<uint64_t>(1024, L.count - first);
            L.f.clear();
            L.f.seekg((std::streamoff)(L.offset + first * L.record));
            L.f.read((char*)buf.data(), (std::streamsize)(n * L.record));
            const uint64_t got = (uint64_t)L.f.gcount() / L.record;
            for (uint64_t i = 0; i < got; i++) {
                const uint8_t* p = &buf[i * L.record];
                max_i = std::max<uint32_t>(max_i, rd<uint16_t>(p + 12));
                if (L.rgb_at >= 0)
                    for (int c = 0; c < 3; c++)
                        max_rgb = std::max<uint32_t>(max_rgb, rd<uint16_t>(p + L.rgb_at + 2 * c));
            }
        }
        L.rgb_shift = shift_for(max_rgb);
        L.intensity_shift = shift_for(max_i);
        // A file whose colour fields are all zero carries no colour.
        if (L.rgb_at >= 0 && max_rgb == 0) _info.has_color = false;
        return;
    }
    if (ext == ".ply") {
        _info.format = Format::Ply;
        _ply = std::make_unique<Ply>();
        Ply& P = *_ply;
        P.f.open(path, std::ios::binary);
        if (!P.f) fail(path, "cannot open");
        std::string line;
        if (!std::getline(P.f, line) || line.rfind("ply", 0) != 0) fail(path, "not a PLY file");
        while (std::getline(P.f, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            std::istringstream ss(line);
            std::string key;
            ss >> key;
            if (key == "format") {
                std::string enc;
                ss >> enc;
                P.enc = enc == "ascii" ? Ply::Enc::Ascii
                      : enc == "binary_big_endian" ? Ply::Enc::Big : Ply::Enc::Little;
            } else if (key == "element") {
                Ply::Element e;
                ss >> e.name >> e.count;
                P.elements.push_back(e);
            } else if (key == "property" && !P.elements.empty()) {
                Ply::Prop pr;
                std::string t;
                ss >> t;
                if (t == "list") {
                    ss >> pr.count_type >> pr.type >> pr.name;
                } else {
                    pr.type = t;
                    ss >> pr.name;
                    pr.size = ply_size(t);
                    if (!pr.size) fail(path, "unknown PLY property type '" + t + "'");
                }
                P.elements.back().props.push_back(pr);
            } else if (key == "end_header") {
                break;
            }
        }
        if (!P.f) fail(path, "the PLY header has no end_header");
        P.body = (uint64_t)P.f.tellg();
        for (size_t i = 0; i < P.elements.size(); i++) {
            Ply::Element& e = P.elements[i];
            int stride = 0;
            for (const Ply::Prop& pr : e.props) {
                if (!pr.count_type.empty()) { stride = -1; break; }
                stride += pr.size;
            }
            e.stride = stride;
            if (e.name == "vertex") P.vertex = (int)i;
        }
        if (P.vertex < 0) fail(path, "the PLY file has no vertex element");
        const Ply::Element& v = P.elements[(size_t)P.vertex];
        bool x = false, y = false, z = false, r = false;
        for (const Ply::Prop& pr : v.props) {
            x |= pr.name == "x"; y |= pr.name == "y"; z |= pr.name == "z";
            r |= pr.name == "red" || pr.name == "r" || pr.name == "diffuse_red";
        }
        if (!(x && y && z)) fail(path, "the PLY vertices have no x, y, z");
        _info.points = v.count;
        _info.has_color = r;
        // The scanner's viewpoint a PCL-written PLY carries after its points;
        // reachable without reading them only when every element before it is
        // fixed-size binary.
        bool seekable = P.enc != Ply::Enc::Ascii;
        uint64_t at = P.body;
        for (const Ply::Element& e : P.elements) {
            if (e.name == "camera" && seekable && e.count > 0 && e.stride > 0) {
                std::vector<uint8_t> rec((size_t)e.stride);
                P.f.clear();
                P.f.seekg((std::streamoff)at);
                for (int64_t ci = 0; ci < e.count && P.f.read((char*)rec.data(), (std::streamsize)rec.size()); ci++) {
                    double val[12] = {};
                    static const char* kNames[12] = {
                        "view_px", "view_py", "view_pz", "x_axisx", "x_axisy", "x_axisz",
                        "y_axisx", "y_axisy", "y_axisz", "z_axisx", "z_axisy", "z_axisz"};
                    int found = 0, off = 0;
                    for (const Ply::Prop& pr : e.props) {
                        for (int k = 0; k < 12; k++)
                            if (pr.name == kNames[k]) {
                                val[k] = ply_value(&rec[(size_t)off], pr.type,
                                                   P.enc == Ply::Enc::Big);
                                found++;
                            }
                        off += pr.size;
                    }
                    if (found != 12) break;
                    Station st;
                    for (int a = 0; a < 3; a++) {
                        st.origin[a] = val[a];
                        for (int c = 0; c < 3; c++) st.R[a * 3 + c] = val[3 + 3 * c + a];
                    }
                    _info.stations.push_back(st);
                }
            }
            if (e.stride < 0) seekable = false;
            at += (uint64_t)std::max(e.stride, 0) * (uint64_t)e.count;
        }
        return;
    }
    fail(path, "not a point cloud this reads (.e57, .las or .ply)");
}

Reader::~Reader() = default;

bool Reader::read(const std::function<void(const e57::Point*, size_t)>& sink,
                  const std::atomic<bool>* cancel) {
    if (_e57) {
        for (const e57::Scan& s : _e57->scans())
            if (!_e57->read_points(s, sink, cancel)) return false;
        return true;
    }
    std::vector<e57::Point> batch;
    batch.reserve(kBatch);
    if (_las) {
        Las& L = *_las;
        L.f.clear();
        L.f.seekg((std::streamoff)L.offset);
        std::vector<uint8_t> buf((size_t)L.record * kBatch);
        uint64_t left = L.count;
        while (left > 0) {
            if (cancel && cancel->load()) return false;
            const uint64_t n = std::min<uint64_t>(left, kBatch);
            L.f.read((char*)buf.data(), (std::streamsize)(n * L.record));
            const uint64_t got = (uint64_t)L.f.gcount() / L.record;
            if (got == 0) fail(_path, "the file ends before its last point");
            batch.clear();
            for (uint64_t i = 0; i < got; i++) {
                const uint8_t* p = &buf[i * L.record];
                e57::Point& o = batch.emplace_back();
                for (int a = 0; a < 3; a++)
                    o.xyz[a] = (double)rd<int32_t>(p + 4 * a) * L.scale[a] + L.shift[a];
                if (_info.has_color) {
                    for (int c = 0; c < 3; c++)
                        o.rgb[c] = (uint8_t)std::min<uint32_t>(
                            255, rd<uint16_t>(p + L.rgb_at + 2 * c) >> L.rgb_shift);
                } else {
                    const uint8_t g = (uint8_t)std::min<uint32_t>(
                        255, rd<uint16_t>(p + 12) >> L.intensity_shift);
                    o.rgb[0] = o.rgb[1] = o.rgb[2] = g;
                }
            }
            sink(batch.data(), batch.size());
            left -= got;
        }
        return true;
    }
    Ply& P = *_ply;
    P.f.clear();
    P.f.seekg((std::streamoff)P.body);
    const bool big = P.enc == Ply::Enc::Big;
    for (size_t ei = 0; ei < P.elements.size(); ei++) {
        const Ply::Element& e = P.elements[ei];
        const bool is_vertex = (int)ei == P.vertex;
        if (!is_vertex) {
            if (P.enc != Ply::Enc::Ascii && e.stride >= 0) {
                P.f.seekg((std::streamoff)(e.stride * e.count), std::ios::cur);
                continue;
            }
            // A list (or ASCII) element ahead of the vertices: walk it.
            for (int64_t i = 0; i < e.count; i++) {
                if (P.enc == Ply::Enc::Ascii) {
                    std::string skip;
                    std::getline(P.f, skip);
                    continue;
                }
                for (const Ply::Prop& pr : e.props) {
                    if (pr.count_type.empty()) {
                        P.f.seekg(pr.size, std::ios::cur);
                        continue;
                    }
                    uint8_t c[8];
                    const int cs = ply_size(pr.count_type);
                    P.f.read((char*)c, cs);
                    const int64_t n = (int64_t)ply_value(c, pr.count_type, big);
                    P.f.seekg((std::streamoff)(n * ply_size(pr.type)), std::ios::cur);
                }
            }
            continue;
        }
        int px = -1, py = -1, pz = -1, pr = -1, pg = -1, pb = -1;
        std::vector<int> offs(e.props.size());
        int off = 0;
        for (size_t k = 0; k < e.props.size(); k++) {
            const std::string& n = e.props[k].name;
            offs[k] = off;
            off += e.props[k].size;
            if (n == "x") px = (int)k;
            else if (n == "y") py = (int)k;
            else if (n == "z") pz = (int)k;
            else if (n == "red" || n == "r" || n == "diffuse_red") pr = (int)k;
            else if (n == "green" || n == "g" || n == "diffuse_green") pg = (int)k;
            else if (n == "blue" || n == "b" || n == "diffuse_blue") pb = (int)k;
        }
        const bool color = pr >= 0 && pg >= 0 && pb >= 0;
        if (P.enc != Ply::Enc::Ascii && e.stride < 0)
            fail(_path, "PLY vertices with list properties are not read");
        std::vector<uint8_t> buf(P.enc == Ply::Enc::Ascii ? 0 : (size_t)e.stride * kBatch);
        int64_t left = e.count;
        while (left > 0) {
            if (cancel && cancel->load()) return false;
            const int64_t n = std::min<int64_t>(left, (int64_t)kBatch);
            batch.clear();
            if (P.enc == Ply::Enc::Ascii) {
                std::string line;
                std::vector<double> v(e.props.size());
                for (int64_t i = 0; i < n; i++) {
                    if (!std::getline(P.f, line)) fail(_path, "the file ends before its last point");
                    std::istringstream ss(line);
                    for (double& x : v) ss >> x;
                    e57::Point& o = batch.emplace_back();
                    o.xyz[0] = v[(size_t)px]; o.xyz[1] = v[(size_t)py]; o.xyz[2] = v[(size_t)pz];
                    if (color) {
                        const int ch[3] = {pr, pg, pb};
                        for (int c = 0; c < 3; c++)
                            o.rgb[c] = ply_color(v[(size_t)ch[c]], e.props[(size_t)ch[c]].type);
                    } else {
                        o.rgb[0] = o.rgb[1] = o.rgb[2] = 200;
                    }
                }
            } else {
                P.f.read((char*)buf.data(), (std::streamsize)(n * e.stride));
                if (P.f.gcount() != (std::streamsize)(n * e.stride))
                    fail(_path, "the file ends before its last point");
                for (int64_t i = 0; i < n; i++) {
                    const uint8_t* rec = &buf[(size_t)(i * e.stride)];
                    auto val = [&](int k) {
                        return ply_value(rec + offs[(size_t)k], e.props[(size_t)k].type, big);
                    };
                    e57::Point& o = batch.emplace_back();
                    o.xyz[0] = val(px); o.xyz[1] = val(py); o.xyz[2] = val(pz);
                    if (color) {
                        const int ch[3] = {pr, pg, pb};
                        for (int c = 0; c < 3; c++)
                            o.rgb[c] = ply_color(val(ch[c]), e.props[(size_t)ch[c]].type);
                    } else {
                        o.rgb[0] = o.rgb[1] = o.rgb[2] = 200;
                    }
                }
            }
            // Non-finite and origin-only records are what writers use for
            // empty cells.
            size_t w = 0;
            for (size_t i = 0; i < batch.size(); i++) {
                const double* q = batch[i].xyz;
                if (!std::isfinite(q[0]) || !std::isfinite(q[1]) || !std::isfinite(q[2])) continue;
                if (q[0] == 0 && q[1] == 0 && q[2] == 0) continue;
                batch[w++] = batch[i];
            }
            batch.resize(w);
            if (!batch.empty()) sink(batch.data(), batch.size());
            left -= n;
        }
        return true;
    }
    return true;
}

}  // namespace spirula::cloud

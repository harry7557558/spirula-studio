#include "data/E57Reader.h"

#include "data/Xml.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace spirula::e57 {
namespace {

constexpr uint64_t kCrcBytes = 4;
constexpr uint8_t kBlobSection = 0;
constexpr uint8_t kPointsSection = 1;
constexpr uint8_t kIndexPacket = 0, kDataPacket = 1, kEmptyPacket = 2;

[[noreturn]] void fail(const std::string& why) {
    throw std::runtime_error("E57: " + why);
}

uint64_t le64(const uint8_t* p) { uint64_t v; std::memcpy(&v, p, 8); return v; }
uint32_t le32(const uint8_t* p) { uint32_t v; std::memcpy(&v, p, 4); return v; }
uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

std::string trimmed(const std::string& s) {
    const size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
}

std::string text_of(const XmlNode* n) { return n ? trimmed(n->text) : ""; }

// E57 writes a zero as an empty element, so only a MISSING one falls back.
double number(const XmlNode* n, double fallback = 0.0) {
    if (!n) return fallback;
    const std::string s = trimmed(n->text);
    if (s.empty()) return 0.0;
    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    if (end == s.c_str()) fail("<" + n->tag + "> is not a number: '" + s + "'");
    return v;
}

bool attr_int(const XmlNode& n, const char* key, int64_t& out) {
    const std::string* a = n.attr(key);
    if (!a) return false;
    // A float spelling ("2.55e+02") is legal for an integer-valued attribute.
    char* end = nullptr;
    const long long v = std::strtoll(a->c_str(), &end, 10);
    if (end && *end == '\0') { out = v; return true; }
    out = (int64_t)std::llround(std::strtod(a->c_str(), nullptr));
    return true;
}

bool attr_double(const XmlNode& n, const char* key, double& out) {
    const std::string* a = n.attr(key);
    if (!a) return false;
    out = std::strtod(a->c_str(), nullptr);
    return true;
}

Pose read_pose(const XmlNode* p) {
    Pose out;
    if (!p) return out;
    if (const XmlNode* r = p->find("rotation")) {
        double q[4] = {number(r->find("w"), 1.0), number(r->find("x")),
                       number(r->find("y")), number(r->find("z"))};
        const double len = std::sqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
        if (!(len > 1e-12)) fail("a pose has a zero rotation quaternion");
        for (int i = 0; i < 4; i++) out.q[i] = q[i] / len;
    }
    if (const XmlNode* t = p->find("translation")) {
        out.t[0] = number(t->find("x"));
        out.t[1] = number(t->find("y"));
        out.t[2] = number(t->find("z"));
    }
    return out;
}

Blob read_blob_ref(const XmlNode* n) {
    Blob b;
    if (!n) return b;
    int64_t off = 0, len = 0;
    if (!attr_int(*n, "fileOffset", off) || !attr_int(*n, "length", len) ||
        off < 0 || len < 0)
        fail("<" + n->tag + "> has no valid fileOffset/length");
    b.offset = (uint64_t)off;
    b.length = (uint64_t)len;
    return b;
}

Field read_field(const XmlNode& n) {
    Field f;
    f.name = n.tag;
    const std::string* type = n.attr("type");
    if (!type) fail("point field '" + n.tag + "' has no type");
    if (*type == "Float") {
        const std::string* prec = n.attr("precision");
        f.kind = Field::Kind::Float;
        f.bits = (prec && *prec == "single") ? 32 : 64;
        const bool has_lo = attr_double(n, "minimum", f.lo);
        const bool has_hi = attr_double(n, "maximum", f.hi);
        f.ranged = has_lo && has_hi;
        return f;
    }
    if (*type != "Integer" && *type != "ScaledInteger")
        fail("point field '" + n.tag + "' has unsupported type " + *type);
    f.kind = *type == "Integer" ? Field::Kind::Integer : Field::Kind::ScaledInteger;
    int64_t mn = std::numeric_limits<int64_t>::min();
    int64_t mx = std::numeric_limits<int64_t>::max();
    const bool has_lo = attr_int(n, "minimum", mn);
    const bool has_hi = attr_int(n, "maximum", mx);
    f.ranged = has_lo && has_hi;
    if (mx < mn) fail("point field '" + n.tag + "' has maximum < minimum");
    uint64_t span = (uint64_t)mx - (uint64_t)mn;
    f.bits = 0;
    while (span) { f.bits++; span >>= 1; }
    f.minimum = mn;
    if (f.kind == Field::Kind::ScaledInteger) {
        attr_double(n, "scale", f.scale);
        attr_double(n, "offset", f.offset);
    }
    f.lo = (double)mn * f.scale + f.offset;
    f.hi = (double)mx * f.scale + f.offset;
    return f;
}

int field_index(const Scan& s, const char* name) {
    for (size_t i = 0; i < s.fields.size(); i++)
        if (s.fields[i].name == name) return (int)i;
    return -1;
}

// A value range: the scan's declared limits, else the field's own, else what
// an unranged field of that kind conventionally holds.
void value_range(const XmlNode* limits, const char* lo_key, const char* hi_key,
                 const Field& f, double integer_hi, double& lo, double& hi) {
    if (limits && limits->find(lo_key) && limits->find(hi_key)) {
        lo = number(limits->find(lo_key));
        hi = number(limits->find(hi_key));
    } else if (f.ranged) {
        lo = f.lo;
        hi = f.hi;
    } else {
        lo = 0;
        hi = f.kind == Field::Kind::Float ? 1.0 : integer_hi;
    }
    if (!(hi > lo)) hi = lo + 1;
}

Scan read_scan(const XmlNode& n) {
    Scan s;
    s.name = text_of(n.find("name"));
    s.guid = text_of(n.find("guid"));
    s.pose = read_pose(n.find("pose"));
    const XmlNode* pts = n.find("points");
    if (!pts) fail("scan '" + s.name + "' has no points");
    int64_t off = 0;
    if (!attr_int(*pts, "fileOffset", off) || !attr_int(*pts, "recordCount", s.count) ||
        off < 0 || s.count < 0)
        fail("scan '" + s.name + "' has no valid fileOffset/recordCount");
    s.section = (uint64_t)off;
    const XmlNode* proto = pts->find("prototype");
    if (!proto) fail("scan '" + s.name + "' has no prototype");
    for (const XmlNode& c : proto->children) s.fields.push_back(read_field(c));
    // bitPackCodec is the only codec the standard defines, and the default.
    if (const XmlNode* codecs = pts->find("codecs"))
        for (const XmlNode& c : codecs->children)
            for (const XmlNode& k : c.children)
                if (k.tag != "inputs" && k.tag != "bitPackCodec")
                    fail("scan '" + s.name + "' uses unsupported codec " + k.tag);

    const int r = field_index(s, "colorRed"), g = field_index(s, "colorGreen"),
              b = field_index(s, "colorBlue");
    s.has_color = r >= 0 && g >= 0 && b >= 0;
    if (s.has_color) {
        const XmlNode* lim = n.find("colorLimits");
        value_range(lim, "colorRedMinimum", "colorRedMaximum", s.fields[r], 255,
                    s.color_lo[0], s.color_hi[0]);
        value_range(lim, "colorGreenMinimum", "colorGreenMaximum", s.fields[g], 255,
                    s.color_lo[1], s.color_hi[1]);
        value_range(lim, "colorBlueMinimum", "colorBlueMaximum", s.fields[b], 255,
                    s.color_lo[2], s.color_hi[2]);
    }
    const int in = field_index(s, "intensity");
    s.has_intensity = in >= 0;
    if (s.has_intensity)
        value_range(n.find("intensityLimits"), "intensityMinimum", "intensityMaximum",
                    s.fields[in], 1, s.intensity_lo, s.intensity_hi);
    return s;
}

Image read_image(const XmlNode& n) {
    Image im;
    im.name = text_of(n.find("name"));
    im.guid = text_of(n.find("guid"));
    im.scan_guid = text_of(n.find("associatedData3DGuid"));
    const XmlNode* pose = n.find("pose");
    im.has_pose = pose != nullptr;
    im.pose = read_pose(pose);
    static const std::pair<const char*, Projection> kReps[] = {
        {"pinholeRepresentation", Projection::Pinhole},
        {"sphericalRepresentation", Projection::Spherical},
        {"cylindricalRepresentation", Projection::Cylindrical},
        {"visualReferenceRepresentation", Projection::Reference},
    };
    for (const auto& [tag, proj] : kReps) {
        const XmlNode* r = n.find(tag);
        if (!r) continue;
        im.projection = proj;
        im.jpeg = read_blob_ref(r->find("jpegImage"));
        im.png = read_blob_ref(r->find("pngImage"));
        im.mask = read_blob_ref(r->find("imageMask"));
        im.width = (int64_t)number(r->find("imageWidth"));
        im.height = (int64_t)number(r->find("imageHeight"));
        im.focal_length = number(r->find("focalLength"));
        im.pixel_width = number(r->find("pixelWidth"));
        im.pixel_height = number(r->find("pixelHeight"));
        im.principal_x = number(r->find("principalPointX"));
        im.principal_y = number(r->find("principalPointY"));
        break;
    }
    return im;
}

// One field's bytestream. Values are bit-packed LSB first and run on from one
// data packet into the next, so each packet's chunk is appended to the rest.
struct BitStream {
    std::vector<uint8_t> buf;
    size_t end = 0;
    uint64_t bit = 0;

    void append(const uint8_t* p, size_t n) {
        const size_t drop = (size_t)(bit >> 3);
        if (drop) {
            std::memmove(buf.data(), buf.data() + drop, end - drop);
            end -= drop;
            bit -= (uint64_t)drop * 8;
        }
        // Zero tail: take() loads nine bytes whatever the width.
        buf.resize(end + n + 16);
        std::memcpy(buf.data() + end, p, n);
        end += n;
        std::memset(buf.data() + end, 0, 16);
    }
    uint64_t available(int bits) const {
        if (bits == 0) return std::numeric_limits<uint64_t>::max();
        return ((uint64_t)end * 8 - bit) / (uint64_t)bits;
    }
    uint64_t take(int bits) {
        const uint8_t* p = buf.data() + (bit >> 3);
        const int shift = (int)(bit & 7);
        uint64_t v = le64(p) >> shift;
        if (shift + bits > 64) v |= (uint64_t)p[8] << (64 - shift);
        if (bits < 64) v &= (uint64_t(1) << bits) - 1;
        bit += (uint64_t)bits;
        return v;
    }
};

double decode(const Field& f, BitStream& s) {
    if (f.kind == Field::Kind::Float) {
        if (f.bits == 32) {
            const uint32_t u = (uint32_t)s.take(32);
            float x;
            std::memcpy(&x, &u, 4);
            return x;
        }
        const uint64_t u = s.take(64);
        double x;
        std::memcpy(&x, &u, 8);
        return x;
    }
    const int64_t v = (int64_t)(s.take(f.bits) + (uint64_t)f.minimum);
    return f.kind == Field::Kind::ScaledInteger ? (double)v * f.scale + f.offset
                                                : (double)v;
}

uint8_t to_byte(double v, double lo, double hi) {
    const double t = (v - lo) / (hi - lo) * 255.0;
    if (!(t > 0.0)) return 0;
    return t >= 255.0 ? (uint8_t)255 : (uint8_t)(t + 0.5);
}

}  // namespace


void Pose::rotation(double R[9]) const {
    const double w = q[0], x = q[1], y = q[2], z = q[3];
    R[0] = 1 - 2*(y*y + z*z); R[1] = 2*(x*y - w*z);     R[2] = 2*(x*z + w*y);
    R[3] = 2*(x*y + w*z);     R[4] = 1 - 2*(x*x + z*z); R[5] = 2*(y*z - w*x);
    R[6] = 2*(x*z - w*y);     R[7] = 2*(y*z + w*x);     R[8] = 1 - 2*(x*x + y*y);
}

void Pose::apply(const double in[3], double out[3]) const {
    double R[9];
    rotation(R);
    for (int r = 0; r < 3; r++)
        out[r] = R[r*3]*in[0] + R[r*3 + 1]*in[1] + R[r*3 + 2]*in[2] + t[r];
}


Reader::Reader(const std::string& path) : _path(path) {
    _file.open(path, std::ios::binary);
    if (!_file) fail("cannot open " + path);
    uint8_t h[48];
    if (!_file.read(reinterpret_cast<char*>(h), sizeof h))
        fail(path + " is too short to be an E57 file");
    if (std::memcmp(h, "ASTM-E57", 8) != 0) fail(path + " is not an E57 file");
    if (le32(h + 8) != 1)
        fail(path + ": unsupported format version " + std::to_string(le32(h + 8)));
    _length = le64(h + 16);
    const uint64_t xml_offset = le64(h + 24), xml_length = le64(h + 32);
    _page = le64(h + 40);
    if (_page <= kCrcBytes || _page > (uint64_t(1) << 24))
        fail(path + ": bad page size " + std::to_string(_page));
    _file.seekg(0, std::ios::end);
    const uint64_t actual = (uint64_t)_file.tellg();
    if (actual < _length)
        fail(path + " is truncated: " + std::to_string(actual) + " of " +
             std::to_string(_length) + " bytes");

    std::string xml(xml_length, '\0');
    read_logical(xml_offset, xml.data(), xml.size());
    const XmlNode root = xml_parse(xml);
    if (root.tag != "e57Root") fail(path + ": the XML section has no e57Root");
    if (const XmlNode* d = root.find("data3D"))
        for (const XmlNode& c : d->children) _scans.push_back(read_scan(c));
    if (const XmlNode* d = root.find("images2D"))
        for (const XmlNode& c : d->children) _images.push_back(read_image(c));
}

int64_t Reader::total_points() const {
    int64_t n = 0;
    for (const Scan& s : _scans) n += s.count;
    return n;
}

uint64_t Reader::to_logical(uint64_t physical) const {
    return physical / _page * (_page - kCrcBytes) + physical % _page;
}

uint64_t Reader::to_physical(uint64_t logical) const {
    const uint64_t payload = _page - kCrcBytes;
    return logical / payload * _page + logical % payload;
}

void Reader::read_logical(uint64_t physical, void* dst, size_t n) {
    const uint64_t payload = _page - kCrcBytes;
    if (physical % _page >= payload) fail("an offset points into a page checksum");
    if (n == 0) return;
    // One physical read for the whole span; the checksums are skipped in memory.
    const uint64_t last = to_physical(to_logical(physical) + n - 1);
    if (last >= _length) fail("a section runs past the end of " + _path);
    _scratch.resize(last - physical + 1);
    _file.clear();
    _file.seekg((std::streamoff)physical);
    if (!_file.read(reinterpret_cast<char*>(_scratch.data()),
                    (std::streamsize)_scratch.size()))
        fail("read failed in " + _path);
    uint8_t* out = static_cast<uint8_t*>(dst);
    size_t got = 0, at = 0;
    uint64_t in_page = physical % _page;
    while (got < n) {
        const size_t take = (size_t)std::min<uint64_t>(payload - in_page, n - got);
        std::memcpy(out + got, _scratch.data() + at, take);
        got += take;
        at += take + kCrcBytes;
        in_page = 0;
    }
}

std::vector<uint8_t> Reader::read_blob(const Blob& b) {
    uint8_t h[16];
    read_logical(b.offset, h, sizeof h);
    if (h[0] != kBlobSection) fail("a blob does not point at a blob section");
    if (b.length + sizeof h > le64(h + 8)) fail("a blob is longer than its section");
    std::vector<uint8_t> out((size_t)b.length);
    read_logical(to_physical(to_logical(b.offset) + sizeof h), out.data(), out.size());
    return out;
}

bool Reader::read_points(const Scan& scan,
                         const std::function<void(const Point*, size_t)>& sink,
                         const std::atomic<bool>* cancel) {
    enum Role { X, Y, Z, Range, Azimuth, Elevation, Red, Green, Blue, Intensity,
                CartInvalid, SphInvalid, ColorInvalid, kRoles };
    static const char* const kNames[kRoles] = {
        "cartesianX", "cartesianY", "cartesianZ", "sphericalRange",
        "sphericalAzimuth", "sphericalElevation", "colorRed", "colorGreen",
        "colorBlue", "intensity", "cartesianInvalidState",
        "sphericalInvalidState", "isColorInvalid"};
    int idx[kRoles];
    for (int r = 0; r < kRoles; r++) idx[r] = field_index(scan, kNames[r]);
    const bool cartesian = idx[X] >= 0 && idx[Y] >= 0 && idx[Z] >= 0;
    if (!cartesian && (idx[Range] < 0 || idx[Azimuth] < 0 || idx[Elevation] < 0))
        fail("scan '" + scan.name + "' has neither cartesian nor spherical coordinates");
    const int invalid = cartesian ? idx[CartInvalid] : idx[SphInvalid];

    uint8_t h[32];
    read_logical(scan.section, h, sizeof h);
    if (h[0] != kPointsSection)
        fail("scan '" + scan.name + "' does not point at a compressed-vector section");
    const uint64_t end = to_logical(scan.section) + le64(h + 8);
    uint64_t pos = to_logical(le64(h + 16));

    double R[9];
    scan.pose.rotation(R);
    const size_t nf = scan.fields.size();
    std::vector<BitStream> streams(nf);
    std::vector<uint8_t> packet;
    std::vector<double> v(nf);
    std::vector<Point> batch;
    int64_t left = scan.count;
    while (left > 0 && pos < end) {
        if (cancel && cancel->load()) return false;
        uint8_t ph[4];
        read_logical(to_physical(pos), ph, sizeof ph);
        const uint64_t plen = (uint64_t)le16(ph + 2) + 1;
        if (ph[0] == kDataPacket) {
            packet.resize((size_t)plen);
            read_logical(to_physical(pos), packet.data(), packet.size());
            if (plen < 6 || le16(&packet[4]) != nf)
                fail("scan '" + scan.name + "': a data packet does not match the prototype");
            size_t at = 6 + 2 * nf;
            for (size_t k = 0; k < nf; k++) {
                const size_t len = le16(&packet[6 + 2 * k]);
                if (at + len > plen) fail("scan '" + scan.name + "': a data packet overruns");
                streams[k].append(packet.data() + at, len);
                at += len;
            }
            uint64_t n = (uint64_t)left;
            for (size_t k = 0; k < nf; k++)
                n = std::min(n, streams[k].available(scan.fields[k].bits));
            batch.clear();
            for (uint64_t i = 0; i < n; i++) {
                for (size_t k = 0; k < nf; k++) v[k] = decode(scan.fields[k], streams[k]);
                if (invalid >= 0 && v[invalid] != 0) continue;
                double p[3];
                if (cartesian) {
                    p[0] = v[idx[X]]; p[1] = v[idx[Y]]; p[2] = v[idx[Z]];
                } else {
                    const double r = v[idx[Range]], az = v[idx[Azimuth]];
                    const double el = v[idx[Elevation]];
                    p[0] = r * std::cos(el) * std::cos(az);
                    p[1] = r * std::cos(el) * std::sin(az);
                    p[2] = r * std::sin(el);
                }
                // The scanner's own origin: what writers put in unflagged empty cells.
                if (p[0] == 0 && p[1] == 0 && p[2] == 0) continue;
                Point& o = batch.emplace_back();
                for (int r = 0; r < 3; r++)
                    o.xyz[r] = R[r*3]*p[0] + R[r*3 + 1]*p[1] + R[r*3 + 2]*p[2] + scan.pose.t[r];
                const bool color_ok = idx[ColorInvalid] < 0 || v[idx[ColorInvalid]] == 0;
                if (scan.has_color && color_ok) {
                    for (int c = 0; c < 3; c++)
                        o.rgb[c] = to_byte(v[idx[Red + c]], scan.color_lo[c], scan.color_hi[c]);
                } else {
                    const uint8_t g = scan.has_intensity
                        ? to_byte(v[idx[Intensity]], scan.intensity_lo, scan.intensity_hi)
                        : (uint8_t)200;
                    o.rgb[0] = o.rgb[1] = o.rgb[2] = g;
                }
            }
            left -= (int64_t)n;
            if (!batch.empty()) sink(batch.data(), batch.size());
        } else if (ph[0] != kIndexPacket && ph[0] != kEmptyPacket) {
            fail("scan '" + scan.name + "': unknown packet type " + std::to_string(ph[0]));
        }
        pos += plen;
    }
    if (left > 0)
        fail("scan '" + scan.name + "' ended after " + std::to_string(scan.count - left) +
             " of " + std::to_string(scan.count) + " points");
    return true;
}

}  // namespace spirula::e57

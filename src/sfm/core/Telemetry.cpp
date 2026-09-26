// Telemetry.cpp -- see Telemetry.h.
//
// The ISO-BMFF walk here overlaps src/video/Mp4Demuxer.cpp on purpose: that
// one is compiled only with SS_ENABLE_PATENTED and hands out coded pictures,
// this one must exist in every build and never touches a picture.

#include "sfm/core/Telemetry.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>

namespace sfm {

const char* telemetry_carrier_name(TelemetryCarrier c) {
    switch (c) {
        case TelemetryCarrier::Gpmf:     return "GoPro GPMF track";
        case TelemetryCarrier::Insta360: return "Insta360 trailer";
        case TelemetryCarrier::DjiDvtm:  return "DJI djmd track";
        case TelemetryCarrier::Camm:     return "CAMM track";
        default:                         return "none";
    }
}

namespace {

constexpr double kG = 9.80665;
constexpr double kPi = 3.14159265358979323846;

// ================
// Byte sources
// ================

struct Source {
    virtual ~Source() = default;
    virtual uint64_t size() const = 0;
    virtual bool read(uint64_t off, void* dst, size_t n) const = 0;
    bool readVec(uint64_t off, size_t n, std::vector<uint8_t>& out) const {
        out.resize(n);
        return n == 0 || read(off, out.data(), n);
    }
};

struct FileSource : Source {
    mutable std::ifstream f;
    uint64_t n = 0;
    bool open(const std::string& path) {
        f.open(path, std::ios::binary);
        if (!f) return false;
        f.seekg(0, std::ios::end);
        n = (uint64_t)f.tellg();
        return true;
    }
    uint64_t size() const override { return n; }
    bool read(uint64_t off, void* dst, size_t len) const override {
        if (off + len > n) return false;
        f.clear();
        f.seekg((std::streamoff)off, std::ios::beg);
        f.read((char*)dst, (std::streamsize)len);
        return (size_t)f.gcount() == len;
    }
};

struct MemorySource : Source {
    const uint8_t* p = nullptr;
    size_t n = 0;
    uint64_t size() const override { return n; }
    bool read(uint64_t off, void* dst, size_t len) const override {
        if (off + len > n) return false;
        std::memcpy(dst, p + off, len);
        return true;
    }
};

struct CallbackSource : Source {
    const TelemetryRead* fn = nullptr;
    uint64_t n = 0;
    uint64_t size() const override { return n; }
    bool read(uint64_t off, void* dst, size_t len) const override {
        if (off + len > n) return false;
        return len == 0 || (*fn)(off, dst, len);
    }
};

inline uint16_t be16(const uint8_t* p) { return (uint16_t)((p[0] << 8) | p[1]); }
inline uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
inline uint64_t be64(const uint8_t* p) { return ((uint64_t)be32(p) << 32) | be32(p + 4); }
inline uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
inline uint32_t le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
inline uint64_t le64(const uint8_t* p) { return (uint64_t)le32(p) | ((uint64_t)le32(p + 4) << 32); }
inline float bef32(const uint8_t* p) { uint32_t u = be32(p); float f; std::memcpy(&f, &u, 4); return f; }
inline double bef64(const uint8_t* p) { uint64_t u = be64(p); double d; std::memcpy(&d, &u, 8); return d; }
inline float lef32(const uint8_t* p) { uint32_t u = le32(p); float f; std::memcpy(&f, &u, 4); return f; }
inline double lef64(const uint8_t* p) { uint64_t u = le64(p); double d; std::memcpy(&d, &u, 8); return d; }

constexpr uint32_t fourcc(const char s[5]) {
    return ((uint32_t)(uint8_t)s[0] << 24) | ((uint32_t)(uint8_t)s[1] << 16) |
           ((uint32_t)(uint8_t)s[2] << 8) | (uint32_t)(uint8_t)s[3];
}

std::string fourcc_str(uint32_t v) {
    char s[5] = {(char)(v >> 24), (char)(v >> 16), (char)(v >> 8), (char)v, 0};
    for (char& c : s) if (c && (c < 32 || c > 126)) c = '?';
    return s;
}

// ================
// ISO-BMFF
// ================

struct Box {
    uint32_t type = 0;
    const uint8_t* payload = nullptr;
    size_t payload_size = 0;
    size_t total_size = 0;
};

bool read_box(const uint8_t* data, size_t avail, Box& out) {
    if (avail < 8) return false;
    uint64_t size = be32(data);
    out.type = be32(data + 4);
    size_t header = 8;
    if (size == 1) {
        if (avail < 16) return false;
        size = be64(data + 8);
        header = 16;
    } else if (size == 0) {
        size = avail;
    }
    if (size < header || size > avail) return false;
    out.payload = data + header;
    out.payload_size = (size_t)size - header;
    out.total_size = (size_t)size;
    return true;
}

bool find_box(const uint8_t* data, size_t size, uint32_t type, Box& out) {
    size_t off = 0;
    while (off + 8 <= size) {
        Box b;
        if (!read_box(data + off, size - off, b)) return false;
        if (b.type == type) { out = b; return true; }
        off += b.total_size;
    }
    return false;
}

void each_box(const uint8_t* data, size_t size, const std::function<void(const Box&)>& fn) {
    size_t off = 0;
    while (off + 8 <= size) {
        Box b;
        if (!read_box(data + off, size - off, b)) return;
        fn(b);
        off += b.total_size;
    }
}

struct Sample {
    uint64_t offset = 0;
    uint32_t size = 0;
    double t = 0;   // seconds, decode order
};

struct Track {
    uint32_t sample_type = 0;   // stsd entry fourcc
    uint32_t handler = 0;
    std::string handler_name;
    uint32_t timescale = 0;
    uint64_t duration = 0;
    int width = 0, height = 0;
    std::vector<Sample> samples;
    double fps() const {
        return (samples.size() > 1 && duration > 0 && timescale > 0)
                   ? (double)samples.size() / ((double)duration / timescale) : 0.0;
    }
};

struct Movie {
    uint32_t timescale = 0;
    uint64_t duration = 0;
    uint64_t creation_1904 = 0;
    VideoProjection projection;
    std::vector<Track> tracks;
    double durationSec() const { return timescale ? (double)duration / timescale : 0.0; }
    // 1904-01-01 to 1970-01-01.
    double unixStart() const { return creation_1904 ? (double)creation_1904 - 2082844800.0 : 0.0; }
};

bool parse_trak(const uint8_t* data, size_t size, Track& tk) {
    Box mdia, minf, stbl, b;
    if (!find_box(data, size, fourcc("mdia"), mdia)) return false;
    if (find_box(data, size, fourcc("tkhd"), b) && b.payload_size >= 84) {
        const size_t o = b.payload[0] == 1 ? 96 : 84;
        if (b.payload_size >= o + 8) {
            tk.width = (int)(be32(b.payload + o - 8) >> 16);
            tk.height = (int)(be32(b.payload + o - 4) >> 16);
        }
    }
    if (find_box(mdia.payload, mdia.payload_size, fourcc("hdlr"), b) && b.payload_size >= 24) {
        tk.handler = be32(b.payload + 8);
        const char* s = (const char*)b.payload + 24;
        size_t n = b.payload_size - 24;
        // QuickTime writes a Pascal string, ISO a NUL-terminated one.
        if (n > 0 && (uint8_t)s[0] == n - 1) { s++; n--; }
        while (n > 0 && s[n - 1] == 0) n--;
        tk.handler_name.assign(s, n);
    }
    if (find_box(mdia.payload, mdia.payload_size, fourcc("mdhd"), b) && b.payload_size >= 20) {
        if (b.payload[0] == 1 && b.payload_size >= 32) {
            tk.timescale = be32(b.payload + 20);
            tk.duration = be64(b.payload + 24);
        } else {
            tk.timescale = be32(b.payload + 12);
            tk.duration = be32(b.payload + 16);
        }
    }
    if (!find_box(mdia.payload, mdia.payload_size, fourcc("minf"), minf)) return false;
    if (!find_box(minf.payload, minf.payload_size, fourcc("stbl"), stbl)) return false;

    if (find_box(stbl.payload, stbl.payload_size, fourcc("stsd"), b) && b.payload_size > 16)
        tk.sample_type = be32(b.payload + 12);

    std::vector<uint32_t> sizes;
    uint32_t const_size = 0;
    if (find_box(stbl.payload, stbl.payload_size, fourcc("stsz"), b) && b.payload_size >= 12) {
        const_size = be32(b.payload + 4);
        const uint32_t count = be32(b.payload + 8);
        if (const_size == 0) {
            if (b.payload_size < 12 + 4ull * count) return false;
            sizes.resize(count);
            for (uint32_t i = 0; i < count; i++) sizes[i] = be32(b.payload + 12 + 4 * i);
        } else {
            sizes.assign(count, const_size);
        }
    }
    if (sizes.empty()) return false;

    std::vector<uint64_t> chunks;
    if (find_box(stbl.payload, stbl.payload_size, fourcc("stco"), b) && b.payload_size >= 8) {
        const uint32_t count = be32(b.payload + 4);
        if (b.payload_size < 8 + 4ull * count) return false;
        for (uint32_t i = 0; i < count; i++) chunks.push_back(be32(b.payload + 8 + 4 * i));
    } else if (find_box(stbl.payload, stbl.payload_size, fourcc("co64"), b) && b.payload_size >= 8) {
        const uint32_t count = be32(b.payload + 4);
        if (b.payload_size < 8 + 8ull * count) return false;
        for (uint32_t i = 0; i < count; i++) chunks.push_back(be64(b.payload + 8 + 8 * i));
    }
    if (chunks.empty()) return false;

    struct StscEntry { uint32_t first_chunk, per_chunk; };
    std::vector<StscEntry> stsc;
    if (find_box(stbl.payload, stbl.payload_size, fourcc("stsc"), b) && b.payload_size >= 8) {
        const uint32_t count = be32(b.payload + 4);
        if (b.payload_size < 8 + 12ull * count) return false;
        for (uint32_t i = 0; i < count; i++)
            stsc.push_back({be32(b.payload + 8 + 12 * i), be32(b.payload + 12 + 12 * i)});
    }
    if (stsc.empty()) stsc.push_back({1, 1});

    tk.samples.resize(sizes.size());
    size_t si = 0;
    for (size_t ci = 0; ci < chunks.size() && si < sizes.size(); ci++) {
        uint32_t per = stsc[0].per_chunk;
        for (const StscEntry& e : stsc)
            if (e.first_chunk <= ci + 1) per = e.per_chunk;
        uint64_t off = chunks[ci];
        for (uint32_t k = 0; k < per && si < sizes.size(); k++, si++) {
            tk.samples[si].offset = off;
            tk.samples[si].size = sizes[si];
            off += sizes[si];
        }
    }
    tk.samples.resize(si);

    if (find_box(stbl.payload, stbl.payload_size, fourcc("stts"), b) && b.payload_size >= 8) {
        const uint32_t count = be32(b.payload + 4);
        if (b.payload_size < 8 + 8ull * count) return false;
        uint64_t dts = 0;
        size_t k = 0;
        for (uint32_t i = 0; i < count && k < tk.samples.size(); i++) {
            const uint32_t n = be32(b.payload + 8 + 8 * i);
            const uint32_t delta = be32(b.payload + 12 + 8 * i);
            for (uint32_t j = 0; j < n && k < tk.samples.size(); j++, k++) {
                tk.samples[k].t = tk.timescale ? (double)dts / tk.timescale : 0.0;
                dts += delta;
            }
        }
    }
    return true;
}

// The two `udta` keys this needs: what a GoPro calls the projection, and that
// projection's own numbers. Nested rather than scanned for, so four bytes
// spelling PRJT inside somebody's payload cannot answer.
void gpmf_projection(const uint8_t* p, size_t n, int depth, VideoProjection& out) {
    for (size_t off = 0; off + 8 <= n;) {
        const uint32_t key = be32(p + off);
        const uint8_t type = p[off + 4];
        const size_t len = (size_t)p[off + 5] * be16(p + off + 6);
        if (off + 8 + len > n) break;
        if (key == fourcc("PRJT") && type == 'F' && len >= 4)
            out.name.assign((const char*)p + off + 8, 4);
        else if (key == fourcc("PMOD") && type == 'L' && out.mode.empty())
            for (size_t i = 0; i + 4 <= len && i < 64; i += 4)
                out.mode.push_back(be32(p + off + 8 + i));
        else if (type == 0 && depth < 4)
            gpmf_projection(p + off + 8, len, depth + 1, out);
        off += 8 + ((len + 3) & ~(size_t)3);
    }
}

// Scans the top-level boxes for moov, which may follow mdat, and stops at the
// first thing that is not a box (an Insta360 trailer, say).
bool read_movie(const Source& src, Movie& mv, bool& is_mp4, std::string& error) {
    is_mp4 = false;
    uint8_t hdr[16];
    uint64_t off = 0;
    std::vector<uint8_t> moov;
    while (off + 8 <= src.size()) {
        if (!src.read(off, hdr, 8)) break;
        uint64_t size = be32(hdr);
        const uint32_t type = be32(hdr + 4);
        uint64_t h = 8;
        if (size == 1) {
            if (!src.read(off + 8, hdr + 8, 8)) break;
            size = be64(hdr + 8);
            h = 16;
        } else if (size == 0) {
            size = src.size() - off;
        }
        if (size < h || off + size > src.size()) break;
        if (type == fourcc("ftyp")) is_mp4 = true;
        if (type == fourcc("moov")) {
            if (size - h > (1ull << 30)) { error = "moov box over 1 GiB"; return false; }
            if (!src.readVec(off + h, (size_t)(size - h), moov)) { error = "truncated moov"; return false; }
            is_mp4 = true;
        }
        off += size;
    }
    if (moov.empty()) return false;

    each_box(moov.data(), moov.size(), [&](const Box& b) {
        if (b.type == fourcc("mvhd") && b.payload_size >= 20) {
            if (b.payload[0] == 1 && b.payload_size >= 32) {
                mv.creation_1904 = be64(b.payload + 4);
                mv.timescale = be32(b.payload + 20);
                mv.duration = be64(b.payload + 24);
            } else {
                mv.creation_1904 = be32(b.payload + 4);
                mv.timescale = be32(b.payload + 12);
                mv.duration = be32(b.payload + 16);
            }
        } else if (b.type == fourcc("trak")) {
            Track tk;
            if (parse_trak(b.payload, b.payload_size, tk)) mv.tracks.push_back(std::move(tk));
        } else if (b.type == fourcc("udta")) {
            Box g;
            if (find_box(b.payload, b.payload_size, fourcc("GPMF"), g))
                gpmf_projection(g.payload, g.payload_size, 0, mv.projection);
        }
    });
    return true;
}

// ================
// Time helpers
// ================

// Spreads the `count` readings of payload `i` from its own start to the
// next payload's, the way gpmf-parser does; the last payload reuses the
// previous spacing.
void spread_times(const std::vector<double>& starts, const std::vector<size_t>& counts,
                  std::vector<double>& out) {
    out.clear();
    double prev_step = 0;
    for (size_t i = 0; i < starts.size(); i++) {
        double step = prev_step;
        if (i + 1 < starts.size() && counts[i] > 0)
            step = (starts[i + 1] - starts[i]) / (double)counts[i];
        if (!(step > 0)) step = prev_step;
        for (size_t j = 0; j < counts[i]; j++) out.push_back(starts[i] + step * (double)j);
        if (step > 0) prev_step = step;
    }
}

// ================
// GPMF (GoPro)
// ================

struct Klv {
    uint32_t key = 0;
    uint8_t type = 0;
    uint8_t size = 0;
    uint16_t repeat = 0;
    const uint8_t* data = nullptr;
    size_t len() const { return (size_t)size * repeat; }
    size_t padded() const { return (len() + 3) & ~(size_t)3; }
};

bool klv_read(const uint8_t* p, size_t avail, Klv& k) {
    if (avail < 8) return false;
    k.key = be32(p);
    k.type = p[4];
    k.size = p[5];
    k.repeat = be16(p + 6);
    k.data = p + 8;
    return 8 + k.len() <= avail;
}

// One STRM: the numbers a data key needs to be decoded.
struct GpmfStream {
    bool has_stmp = false;
    uint64_t stmp = 0;
    std::vector<double> scal;
    std::string orin, type_def, unit;
    uint32_t gpsf = 0;
    bool has_gpsf = false;
    double gpsp = 0;
    double gpsu_unix = 0;
    Klv data;
    bool has_data = false;
};

// Element width of one GPMF type letter; 0 for anything not a number.
size_t gpmf_width(char c) {
    switch (c) {
        case 'b': case 'B': return 1;
        case 's': case 'S': return 2;
        case 'l': case 'L': case 'f': return 4;
        case 'd': case 'j': case 'J': return 8;
        default: return 0;
    }
}

double gpmf_typed(char c, const uint8_t* p) {
    switch (c) {
        case 'b': return (double)(int8_t)p[0];
        case 'B': return (double)p[0];
        case 's': return (double)(int16_t)be16(p);
        case 'S': return (double)be16(p);
        case 'l': return (double)(int32_t)be32(p);
        case 'L': return (double)be32(p);
        case 'f': return (double)bef32(p);
        case 'd': return bef64(p);
        case 'j': return (double)(int64_t)be64(p);
        case 'J': return (double)be64(p);
        default:  return 0.0;
    }
}

// "yymmddhhmmss.sss" -> unix seconds; 0 when malformed.
double gpsu_to_unix(const uint8_t* s, size_t n) {
    if (n < 12) return 0;
    auto two = [&](size_t o) { return (s[o] - '0') * 10 + (s[o + 1] - '0'); };
    for (size_t i = 0; i < 12; i++) if (s[i] < '0' || s[i] > '9') return 0;
    int y = 2000 + two(0), m = two(2), d = two(4);
    int hh = two(6), mm = two(8), ss = two(10);
    double frac = 0;
    if (n >= 16 && s[12] == '.') frac = (two(13) * 10 + (s[15] - '0')) / 1000.0;
    // Days since the epoch, civil-from-days (Howard Hinnant).
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (unsigned)(m + (m > 2 ? -3 : 9)) + 2) / 5 + (unsigned)d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const long long days = (long long)era * 146097 + (long long)doe - 719468;
    return (double)days * 86400.0 + hh * 3600.0 + mm * 60.0 + ss + frac;
}

struct GpmfAccum {
    // Per data key: payload start times (us), counts, and the decoded rows.
    std::vector<double> starts;
    std::vector<size_t> counts;
    std::vector<std::vector<double>> rows;
    std::vector<uint8_t> fix;   // GPS only
    std::vector<double> dop;
    std::vector<double> unix_time;
    std::string orin, unit;
    double first_sample_t = 0;   // sample-table time of the first payload
};

void gpmf_walk(const uint8_t* p, size_t n, GpmfStream* strm,
               const std::function<void(const GpmfStream&)>& on_stream,
               std::string& device_name) {
    size_t off = 0;
    while (off + 8 <= n) {
        Klv k;
        if (!klv_read(p + off, n - off, k)) return;
        const size_t adv = 8 + k.padded();
        if (k.type == 0) {
            if (k.key == fourcc("STRM")) {
                GpmfStream s;
                gpmf_walk(k.data, k.len(), &s, on_stream, device_name);
                if (s.has_data) on_stream(s);
            } else {
                gpmf_walk(k.data, k.len(), strm, on_stream, device_name);
            }
        } else if (k.key == fourcc("DVNM") && k.type == 'c') {
            device_name.assign((const char*)k.data, k.len());
            while (!device_name.empty() && device_name.back() == 0) device_name.pop_back();
        } else if (strm) {
            GpmfStream& s = *strm;
            if (k.key == fourcc("STMP") && k.type == 'J' && k.len() >= 8) {
                s.stmp = be64(k.data);
                s.has_stmp = true;
            } else if (k.key == fourcc("SCAL")) {
                s.scal.clear();
                const size_t w = gpmf_width((char)k.type);
                if (w) for (size_t i = 0; i < k.len() / w; i++) s.scal.push_back(gpmf_typed((char)k.type, k.data + i * w));
            } else if (k.key == fourcc("ORIN") && k.type == 'c') {
                s.orin.assign((const char*)k.data, k.len());
            } else if (k.key == fourcc("TYPE") && k.type == 'c') {
                s.type_def.assign((const char*)k.data, k.len());
                while (!s.type_def.empty() && s.type_def.back() == 0) s.type_def.pop_back();
            } else if ((k.key == fourcc("SIUN") || k.key == fourcc("UNIT")) && k.type == 'c') {
                s.unit.assign((const char*)k.data, std::min<size_t>(k.len(), 16));
            } else if (k.key == fourcc("GPSF") && k.type == 'L' && k.len() >= 4) {
                s.gpsf = be32(k.data);
                s.has_gpsf = true;
            } else if (k.key == fourcc("GPSP") && k.type == 'S' && k.len() >= 2) {
                s.gpsp = be16(k.data) / 100.0;
            } else if (k.key == fourcc("GPSU") && k.type == 'U') {
                s.gpsu_unix = gpsu_to_unix(k.data, k.len());
            } else if (k.key == fourcc("ACCL") || k.key == fourcc("GYRO") || k.key == fourcc("MAGN") ||
                       k.key == fourcc("GRAV") || k.key == fourcc("CORI") || k.key == fourcc("GPS5") ||
                       k.key == fourcc("GPS9")) {
                s.data = k;
                s.has_data = true;
            }
        }
        off += adv;
    }
}

// Decodes one STRM's data key into rows of doubles, scaled.
bool gpmf_rows(const GpmfStream& s, std::vector<std::vector<double>>& rows) {
    const Klv& k = s.data;
    rows.clear();
    std::vector<char> layout;
    if (k.type == '?') {
        for (char c : s.type_def) if (gpmf_width(c)) layout.push_back(c);
    } else {
        const size_t w = gpmf_width((char)k.type);
        if (!w || k.size % w) return false;
        layout.assign(k.size / w, (char)k.type);
    }
    size_t row_bytes = 0;
    for (char c : layout) row_bytes += gpmf_width(c);
    if (layout.empty() || row_bytes != k.size) return false;
    for (size_t r = 0; r < k.repeat; r++) {
        std::vector<double> row(layout.size());
        const uint8_t* p = k.data + r * k.size;
        for (size_t c = 0; c < layout.size(); c++) {
            double v = gpmf_typed(layout[c], p);
            p += gpmf_width(layout[c]);
            if (!s.scal.empty()) {
                const double sc = s.scal.size() == layout.size() ? s.scal[c] : s.scal[0];
                if (sc != 0) v /= sc;
            }
            row[c] = v;
        }
        rows.push_back(std::move(row));
    }
    return true;
}

// ORIN names each column's axis, lower case negated: "XzY" is x, -z, y.
// Missing, it is the (z, x, y) the early GPMF spec documents for HERO5.
void orin_apply(const std::string& orin, const std::vector<double>& row, TelemetryVec& v) {
    std::string o = orin.size() >= 3 ? orin.substr(0, 3) : "ZXY";
    double out[3] = {0, 0, 0};
    for (int c = 0; c < 3 && c < (int)row.size(); c++) {
        const char ch = o[c];
        const int axis = std::tolower(ch) - 'x';
        if (axis < 0 || axis > 2) continue;
        out[axis] = std::isupper(ch) ? row[c] : -row[c];
    }
    v.x = out[0]; v.y = out[1]; v.z = out[2];
}

bool read_gpmf(const Source& src, const Track& tk, Telemetry& out, std::string& error) {
    std::map<uint32_t, GpmfAccum> acc;
    std::string device_name;
    std::vector<uint8_t> buf;
    bool any = false;
    for (size_t i = 0; i < tk.samples.size(); i++) {
        const Sample& sm = tk.samples[i];
        if (sm.size < 8 || sm.size > (64u << 20)) continue;
        if (!src.readVec(sm.offset, sm.size, buf)) { error = "truncated gpmd sample"; return false; }
        auto on_stream = [&](const GpmfStream& s) {
            std::vector<std::vector<double>> rows;
            if (!gpmf_rows(s, rows) || rows.empty()) return;
            GpmfAccum& a = acc[s.data.key];
            if (a.starts.empty()) a.first_sample_t = sm.t;
            // The payload's place on the video clock comes from the sample
            // table; STMP only spaces the readings within it.
            a.starts.push_back(s.has_stmp ? (double)s.stmp * 1e-6 : sm.t);
            a.counts.push_back(rows.size());
            if (a.orin.empty()) a.orin = s.orin;
            if (a.unit.empty()) a.unit = s.unit;
            if (s.data.key == fourcc("GPS5")) {
                a.fix.push_back((uint8_t)(s.has_gpsf ? s.gpsf : 3));
                a.dop.push_back(s.gpsp);
                a.unix_time.push_back(s.gpsu_unix);
            }
            for (auto& r : rows) a.rows.push_back(std::move(r));
            any = true;
        };
        gpmf_walk(buf.data(), buf.size(), nullptr, on_stream, device_name);
    }
    if (!any) return true;

    out.carrier = TelemetryCarrier::Gpmf;
    if (!device_name.empty()) out.camera = device_name;

    // STMP runs on the camera clock, whose zero is not the first frame: shift
    // each stream so its first payload lands where the sample table put it.
    for (auto& kv : acc) {
        GpmfAccum& a = kv.second;
        if (a.starts.empty()) continue;
        const double shift = a.first_sample_t - a.starts[0];
        for (double& s : a.starts) s += shift;
    }

    auto vec_stream = [&](uint32_t key, std::vector<TelemetryVec>& dst, double unit_scale,
                          bool apply_orin) {
        auto it = acc.find(key);
        if (it == acc.end()) return;
        GpmfAccum& a = it->second;
        std::vector<double> times;
        spread_times(a.starts, a.counts, times);
        dst.reserve(a.rows.size());
        for (size_t i = 0; i < a.rows.size() && i < times.size(); i++) {
            TelemetryVec v;
            v.t = times[i];
            if (apply_orin) orin_apply(a.orin, a.rows[i], v);
            else if (a.rows[i].size() >= 3) { v.x = a.rows[i][0]; v.y = a.rows[i][1]; v.z = a.rows[i][2]; }
            v.x *= unit_scale; v.y *= unit_scale; v.z *= unit_scale;
            dst.push_back(v);
        }
        if (apply_orin && a.orin.empty())
            out.notes.push_back(fourcc_str(key) + ": no ORIN, columns taken as (z, x, y)");
    };
    vec_stream(fourcc("ACCL"), out.accel, 1.0, true);
    vec_stream(fourcc("GYRO"), out.gyro, 1.0, true);
    vec_stream(fourcc("MAGN"), out.magnet, 1.0, true);
    vec_stream(fourcc("GRAV"), out.gravity, 1.0, false);
    // GRAV is written as (X, Z, Y) of the ORIN frame: 3-4 deg from the
    // averaged accelerometer on three MAX files once swapped, 60-97 deg before.
    for (TelemetryVec& g : out.gravity) std::swap(g.y, g.z);
    if (!out.gravity.empty()) out.notes.push_back("GRAV columns swapped from (X, Z, Y) into the ORIN frame");
    if (auto it = acc.find(fourcc("ACCL")); it != acc.end() && !it->second.orin.empty())
        out.notes.push_back("ACCL/GYRO ORIN " + it->second.orin.substr(0, 3) +
                            " applied: axes are GoPro's documented camera frame");
    if (auto it = acc.find(fourcc("GYRO")); it != acc.end() && it->second.unit.rfind("deg", 0) == 0) {
        for (TelemetryVec& v : out.gyro) { v.x *= kPi / 180; v.y *= kPi / 180; v.z *= kPi / 180; }
        out.notes.push_back("GYRO unit was deg/s; converted");
    }

    if (auto it = acc.find(fourcc("CORI")); it != acc.end()) {
        GpmfAccum& a = it->second;
        std::vector<double> times;
        spread_times(a.starts, a.counts, times);
        for (size_t i = 0; i < a.rows.size() && i < times.size(); i++) {
            if (a.rows[i].size() < 4) continue;
            TelemetryQuat q;
            q.t = times[i];
            q.w = a.rows[i][0]; q.x = a.rows[i][1]; q.y = a.rows[i][2]; q.z = a.rows[i][3];
            out.orientation.push_back(q);
        }
        out.orientation_axes = "XZY";
        out.notes.push_back("orientation is CORI, one per frame; its conjugate takes (X, Z, Y) sensor vectors "
                            "to a fixed world vector (2-8 deg over three MAX files)");
    }

    for (uint32_t key : {fourcc("GPS5"), fourcc("GPS9")}) {
        auto it = acc.find(key);
        if (it == acc.end()) continue;
        GpmfAccum& a = it->second;
        std::vector<double> times;
        spread_times(a.starts, a.counts, times);
        size_t payload = 0, in_payload = 0;
        for (size_t i = 0; i < a.rows.size() && i < times.size(); i++) {
            while (payload < a.counts.size() && in_payload >= a.counts[payload]) { payload++; in_payload = 0; }
            const std::vector<double>& r = a.rows[i];
            TelemetryGps g;
            g.t = times[i];
            if (key == fourcc("GPS5") && r.size() >= 5) {
                g.lat = r[0]; g.lon = r[1]; g.alt = r[2]; g.has_alt = true;
                g.speed = r[3];
                if (payload < a.fix.size()) g.fix = a.fix[payload] >= 2;
                if (payload < a.dop.size()) g.dop = a.dop[payload];
                if (payload < a.unix_time.size() && a.unix_time[payload] > 0)
                    g.unix_time = a.unix_time[payload] + (times[i] - a.starts[payload]);
            } else if (key == fourcc("GPS9") && r.size() >= 9) {
                g.lat = r[0]; g.lon = r[1]; g.alt = r[2]; g.has_alt = true;
                g.speed = r[3];
                g.unix_time = 946684800.0 + r[5] * 86400.0 + r[6];
                g.dop = r[7];
                g.fix = r[8] >= 2;
            } else {
                in_payload++;
                continue;
            }
            out.gps.push_back(g);
            in_payload++;
        }
        out.notes.push_back(std::string(key == fourcc("GPS5") ? "GPS5" : "GPS9") +
                            ": one fix flag per payload; unix time from GPSU");
    }
    return true;
}

// ================
// CAMM (Google camera motion metadata)
// ================

bool read_camm(const Source& src, const Track& tk, Telemetry& out, std::string& error) {
    std::vector<uint8_t> buf;
    bool any = false;
    for (const Sample& sm : tk.samples) {
        if (sm.size < 4 || sm.size > 4096) continue;
        if (!src.readVec(sm.offset, sm.size, buf)) { error = "truncated camm sample"; return false; }
        const uint16_t type = le16(buf.data() + 2);
        const uint8_t* p = buf.data() + 4;
        const size_t n = buf.size() - 4;
        auto vec3 = [&](std::vector<TelemetryVec>& dst) {
            if (n < 12) return;
            dst.push_back({sm.t, (double)lef32(p), (double)lef32(p + 4), (double)lef32(p + 8)});
            any = true;
        };
        switch (type) {
            case 0: {
                if (n < 12) break;
                const double ax = lef32(p), ay = lef32(p + 4), az = lef32(p + 8);
                const double a = std::sqrt(ax * ax + ay * ay + az * az);
                TelemetryQuat q;
                q.t = sm.t;
                if (a > 1e-12) {
                    const double s = std::sin(a / 2) / a;
                    q.w = std::cos(a / 2); q.x = ax * s; q.y = ay * s; q.z = az * s;
                }
                out.orientation.push_back(q);
                any = true;
                break;
            }
            case 1:
                if (n >= 8) out.frame_readout = (double)(int32_t)le32(p + 4) * 1e-9;
                break;
            case 2: vec3(out.gyro); break;
            case 3: vec3(out.accel); break;
            case 5:
                if (n >= 24) {
                    TelemetryGps g;
                    g.t = sm.t;
                    g.lat = lef64(p); g.lon = lef64(p + 8); g.alt = lef64(p + 16); g.has_alt = true;
                    g.fix = true;
                    out.gps.push_back(g);
                    any = true;
                }
                break;
            case 6:
                if (n >= 56) {
                    TelemetryGps g;
                    g.t = sm.t;
                    g.unix_time = lef64(p);
                    g.fix = (int32_t)le32(p + 8) >= 2;
                    g.lat = lef64(p + 12); g.lon = lef64(p + 20);
                    g.alt = lef32(p + 28); g.has_alt = true;
                    g.dop = lef32(p + 32);
                    const double ve = lef32(p + 40), vn = lef32(p + 44);
                    g.speed = std::sqrt(ve * ve + vn * vn);
                    g.track = std::fmod(std::atan2(ve, vn) * 180 / kPi + 360.0, 360.0);
                    out.gps.push_back(g);
                    any = true;
                }
                break;
            case 7: vec3(out.magnet); break;
            default: break;
        }
    }
    if (!any) return true;
    out.carrier = TelemetryCarrier::Camm;
    out.notes.push_back("CAMM: type 6 'dop' is the horizontal accuracy in metres; orientation is the "
                        "angle-axis record as written");
    return true;
}

// ================
// Protobuf wire format (DJI and Insta360 metadata)
// ================

struct PbField {
    uint32_t number = 0;
    uint32_t wire = 0;
    uint64_t varint = 0;
    const uint8_t* data = nullptr;  // wire 2
    size_t len = 0;
    double f32() const { return wire == 5 && len >= 4 ? (double)lef32(data) : 0.0; }
    double f64() const { return wire == 1 && len >= 8 ? lef64(data) : 0.0; }
};

bool pb_varint(const uint8_t*& p, const uint8_t* end, uint64_t& v) {
    v = 0;
    for (int shift = 0; shift < 64 && p < end; shift += 7) {
        const uint8_t b = *p++;
        v |= (uint64_t)(b & 0x7f) << shift;
        if (!(b & 0x80)) return true;
    }
    return false;
}

// Calls `fn` for each field; false when the bytes are not a message.
bool pb_each(const uint8_t* p, size_t n, const std::function<void(const PbField&)>& fn) {
    const uint8_t* end = p + n;
    while (p < end) {
        uint64_t tag;
        if (!pb_varint(p, end, tag)) return false;
        PbField f;
        f.number = (uint32_t)(tag >> 3);
        f.wire = (uint32_t)(tag & 7);
        if (f.number == 0) return false;
        switch (f.wire) {
            case 0: if (!pb_varint(p, end, f.varint)) return false; break;
            case 1: if (end - p < 8) return false; f.data = p; f.len = 8; p += 8; break;
            case 5: if (end - p < 4) return false; f.data = p; f.len = 4; p += 4; break;
            case 2: {
                uint64_t len;
                if (!pb_varint(p, end, len) || len > (uint64_t)(end - p)) return false;
                f.data = p; f.len = (size_t)len; p += len;
                break;
            }
            default: return false;
        }
        fn(f);
    }
    return true;
}

const PbField* pb_find(const std::vector<PbField>& fs, uint32_t number) {
    for (const PbField& f : fs) if (f.number == number) return &f;
    return nullptr;
}

std::vector<PbField> pb_fields(const uint8_t* p, size_t n) {
    std::vector<PbField> out;
    if (!pb_each(p, n, [&](const PbField& f) { out.push_back(f); })) out.clear();
    return out;
}

std::vector<PbField> pb_sub(const PbField* f) {
    return f && f->wire == 2 ? pb_fields(f->data, f->len) : std::vector<PbField>{};
}

std::string pb_string(const PbField* f) {
    return f && f->wire == 2 ? std::string((const char*)f->data, f->len) : std::string();
}

// ================
// DJI djmd (dvtm protobuf)
// ================

struct DjiClip {
    std::string proto, product, serial, firmware;
    double sensor_fps = 0, imu_rate = 0, readout = 0, focal = 0;
};

// A PbField points into the buffer it was parsed from, so a sub-message's
// field list has to outlive any pointer taken into it.
void dji_clip(const std::vector<PbField>& clip, DjiClip& c) {
    const auto hdr = pb_sub(pb_find(clip, 1));
    c.proto = pb_string(pb_find(hdr, 1));
    c.serial = pb_string(pb_find(hdr, 5));
    c.firmware = pb_string(pb_find(hdr, 6));
    c.product = pb_string(pb_find(hdr, 10));
    const auto readout = pb_sub(pb_find(clip, 4));
    if (const PbField* f = pb_find(readout, 1)) c.readout = (double)f->varint * 1e-9;
    const auto focal = pb_sub(pb_find(clip, 8));
    if (const PbField* f = pb_find(focal, 1)) c.focal = f->f32();
    const auto rate = pb_sub(pb_find(clip, 10));
    if (const PbField* f = pb_find(rate, 1)) c.imu_rate = (double)f->varint;
    const auto fps = pb_sub(pb_find(clip, 11));
    if (const PbField* f = pb_find(fps, 1)) c.sensor_fps = f->f32();
}

bool read_dji(const Source& src, const Track& tk, Telemetry& out, std::string& error) {
    std::vector<uint8_t> buf;
    DjiClip clip;
    bool have_clip = false, any = false, first = true;
    double t0 = 0;
    size_t attitude_frames = 0;
    for (const Sample& sm : tk.samples) {
        if (sm.size < 2 || sm.size > (16u << 20)) continue;
        if (!src.readVec(sm.offset, sm.size, buf)) { error = "truncated djmd sample"; return false; }
        const auto top = pb_fields(buf.data(), buf.size());
        if (top.empty()) continue;
        if (!have_clip) {
            if (const PbField* c = pb_find(top, 1)) {
                dji_clip(pb_sub(c), clip);
                have_clip = !clip.proto.empty();
            }
        }
        const auto frame = pb_sub(pb_find(top, 3));
        if (frame.empty()) continue;
        const auto fh = pb_sub(pb_find(frame, 1));
        const PbField* ts = pb_find(fh, 2);
        if (!ts) continue;
        const double t_abs = (double)ts->varint * 1e-6;
        if (first) { t0 = t_abs; first = false; }
        const double t = t_abs - t0;

        const auto cam = pb_sub(pb_find(frame, 2));
        if (const PbField* acc = pb_find(cam, 10)) {
            const auto a = pb_sub(acc);
            const PbField *x = pb_find(a, 2), *y = pb_find(a, 3), *z = pb_find(a, 4);
            if (x && y && z) {
                out.accel.push_back({t, x->f32() * kG, y->f32() * kG, z->f32() * kG});
                any = true;
            }
        }

        // oq101 (Osmo 360) and wa530 wrap the attitude in a cur/prev/next
        // message; wm169 stores it bare. The bare form has field 1 as a
        // varint timestamp, the wrapper has field 1 as a message.
        const auto imu = pb_sub(pb_find(frame, 3));
        const PbField* att = pb_find(imu, 2);
        auto attitude = pb_sub(att);
        if (!attitude.empty() && attitude[0].number == 1 && attitude[0].wire == 2)
            attitude = pb_sub(pb_find(attitude, 1));
        if (!attitude.empty()) {
            std::vector<TelemetryQuat> qs;
            double offset = 0;
            for (const PbField& f : attitude) {
                if (f.number == 3 && f.wire == 2) {
                    const auto q = pb_sub(&f);
                    const PbField *w = pb_find(q, 1), *x = pb_find(q, 2), *y = pb_find(q, 3), *z = pb_find(q, 4);
                    if (w && x && y && z) qs.push_back({0, w->f32(), x->f32(), y->f32(), z->f32()});
                } else if (f.number == 4) {
                    offset = f.f32();
                }
            }
            const double frame_dt = clip.sensor_fps > 0 ? 1.0 / clip.sensor_fps : 0.0;
            for (size_t i = 0; i < qs.size(); i++) {
                qs[i].t = t + frame_dt * ((double)i - offset) / (double)qs.size();
                out.orientation.push_back(qs[i]);
            }
            if (!qs.empty()) { attitude_frames++; any = true; }
        }

        const auto gimbal = pb_sub(pb_find(frame, 4));
        if (const PbField* gb = pb_find(gimbal, 2)) {
            const auto g = pb_sub(gb);
            const auto coord = pb_sub(pb_find(g, 1));
            const PbField *lat = pb_find(coord, 2), *lon = pb_find(coord, 3);
            if (lat && lon) {
                const PbField* unit = pb_find(coord, 1);
                const bool rad = !unit || unit->varint == 0;
                TelemetryGps gp;
                gp.t = t;
                gp.lat = lat->f64() * (rad ? 180 / kPi : 1.0);
                gp.lon = lon->f64() * (rad ? 180 / kPi : 1.0);
                if (const PbField* alt = pb_find(g, 2)) { gp.alt = (double)(int32_t)alt->varint * 1e-3; gp.has_alt = true; }
                const PbField* st = pb_find(g, 3);
                gp.fix = !st || st->varint != 1;
                out.gps.push_back(gp);
                any = true;
            }
        }
    }
    if (!any) return true;
    out.carrier = TelemetryCarrier::DjiDvtm;
    out.camera = clip.product.empty() ? "DJI" : (clip.product.rfind("DJI", 0) == 0 ? clip.product : "DJI " + clip.product);
    out.serial = clip.serial;
    out.firmware = clip.firmware;
    if (clip.readout > 0) out.frame_readout = clip.readout;
    out.notes.push_back("dvtm proto " + clip.proto + (clip.proto == "dvtm_oq101.proto" ? "" : " (layout not verified on a sample)"));
    if (clip.imu_rate > 0) {
        char s[96];
        std::snprintf(s, sizeof s, "IMU fusion rate %.0f Hz, sensor %.3f fps, focal %.1f px", clip.imu_rate, clip.sensor_fps, clip.focal);
        out.notes.push_back(s);
    }
    out.notes.push_back("accel is one reading per frame in g, converted; no raw gyro is written");
    if (attitude_frames) out.notes.push_back("orientation is IMU_attitude_after_fusion: sense not documented, see the check");
    return true;
}

// Only the first samples are read: a clip header is in sample 0 on every
// djmd file seen, and an .OSV runs to thousands of samples.
VideoColor color_of(const Source& src) {
    Movie mv;
    bool is_mp4 = false;
    std::string error;
    if (!read_movie(src, mv, is_mp4, error)) return {};
    std::vector<uint8_t> buf;
    VideoColor out;
    for (const Track& tk : mv.tracks) {
        if (tk.sample_type != fourcc("djmd")) continue;
        for (size_t i = 0; i < tk.samples.size() && i < 8; i++) {
            const Sample& sm = tk.samples[i];
            if (sm.size < 2 || sm.size > (16u << 20) || !src.readVec(sm.offset, sm.size, buf))
                continue;
            const VideoColor c = djmd_color(buf.data(), buf.size());
            if (c.mode != VideoColorMode::NotRecorded) return c;
        }
        // DJI metadata whose header could not be read says nothing about the
        // profile, which is not the same as saying there is none.
        out.mode = VideoColorMode::Unknown;
        out.issue = VideoColorIssue::NoClipHeader;
    }
    return out;
}

// ================
// Insta360 trailer
// ================

constexpr const char* kInstaMagic = "8db42d694ccc418790edff439fe026bf";
constexpr size_t kInstaHeader = 32 + 4 + 4 + 32;

struct InstaMeta {
    std::string camera, serial, firmware;
    int64_t first_frame_ts = 0;
    double readout = 0;
    double gyro_ts = 0;
    bool has_gyro_ts = false;
    int64_t first_gps_ts = 0;
    bool raw_gyro = false;
    double acc_range = 16, gyro_range = 2000;
    int fps = 0;
    std::string offset_v3;
};

void insta_meta(const uint8_t* p, size_t n, InstaMeta& m) {
    pb_each(p, n, [&](const PbField& f) {
        switch (f.number) {
            case 1: m.serial = pb_string(&f); break;
            case 2: m.camera = pb_string(&f); break;
            case 3: m.firmware = pb_string(&f); break;
            case 20: m.fps = (int)f.varint; break;
            case 24: m.first_frame_ts = (int64_t)f.varint; break;
            case 25: m.readout = f.f64(); break;
            case 28: m.gyro_ts = f.f64(); break;
            case 29: m.has_gyro_ts = f.varint != 0; break;
            case 36: m.first_gps_ts = (int64_t)f.varint; break;
            case 54: m.offset_v3 = pb_string(&f); break;
            case 62: m.raw_gyro = f.varint != 0; break;
            case 65: {
                const auto cfg = pb_sub(&f);
                if (const PbField* a = pb_find(cfg, 1)) m.acc_range = (double)a->varint;
                if (const PbField* g = pb_find(cfg, 2)) m.gyro_range = (double)g->varint;
                break;
            }
            default: break;
        }
    });
}

bool insta_detect(const Source& src) {
    if (src.size() < kInstaHeader) return false;
    char magic[32];
    return src.read(src.size() - 32, magic, 32) && std::memcmp(magic, kInstaMagic, 32) == 0;
}

bool read_insta360(const Source& src, const Movie* mv, Telemetry& out, std::string& error) {
    uint8_t hdr[kInstaHeader];
    if (!src.read(src.size() - kInstaHeader, hdr, kInstaHeader)) { error = "short Insta360 trailer"; return false; }
    const uint64_t extra_size = le32(hdr + 32);
    if (extra_size > src.size()) { error = "Insta360 trailer size exceeds the file"; return false; }

    // Each record ends in [format u8][id u8][size u32]; the one nearest the
    // header may be a table of (id, format, size, offset). Records past the
    // thumbnail are padded apart, so a backwards walk alone lands in zeros.
    struct Rec { uint8_t id, format; uint64_t offset, size; };
    std::vector<Rec> recs;
    const uint64_t extra_start = src.size() - extra_size;
    uint64_t back = kInstaHeader + 6;
    std::vector<uint8_t> buf;
    {
        uint8_t r[6];
        if (src.read(src.size() - back, r, 6) && r[1] == 0) {
            const uint64_t size = le32(r + 2);
            if (size <= src.size() - back && src.readVec(src.size() - back - size, (size_t)size, buf)) {
                for (size_t o = 0; o + 10 <= buf.size(); o += 10) {
                    const uint8_t* e = buf.data() + o;
                    const uint64_t rsize = le32(e + 2), roff = le32(e + 6);
                    if (e[0] == 0 || rsize == 0 || extra_start + roff + rsize + 6 > src.size()) continue;
                    uint8_t tail[6];
                    if (!src.read(extra_start + roff + rsize, tail, 6) || tail[1] != e[0] || le32(tail + 2) != rsize) continue;
                    recs.push_back({e[0], e[1], extra_start + roff, rsize});
                }
            }
        }
    }
    const bool walk = recs.empty();
    while (walk && back <= extra_size) {
        uint8_t r[6];
        if (!src.read(src.size() - back, r, 6)) break;
        const uint64_t size = le32(r + 2);
        if (size > src.size() - back) break;
        recs.push_back({r[1], r[0], src.size() - back - size, size});
        back += size + 6;
    }

    InstaMeta meta;
    for (const Rec& r : recs) {
        if (r.id != 1 || r.size > (16u << 20)) continue;
        if (!src.readVec(r.offset, (size_t)r.size, buf)) continue;
        insta_meta(buf.data(), buf.size(), meta);
    }

    // Timestamps are relative to the first frame, as telemetry-parser (the
    // reference for this layout) computes them; the raw-gyro variant stamps
    // in microseconds and the float one in milliseconds.
    const double fft = (double)meta.first_frame_ts / 1000.0;
    const double gyro_shift = meta.has_gyro_ts ? meta.gyro_ts / 1000.0 : 0.0;
    auto fix_time = [&](double raw_ms) {
        double t = raw_ms / 1000.0 - fft;
        if (meta.raw_gyro) t /= 1000.0;
        return t - gyro_shift;
    };

    bool any = false;
    for (const Rec& r : recs) {
        if (r.id == 3) {
            if (r.size > (1ull << 31) || !src.readVec(r.offset, (size_t)r.size, buf)) { error = "unreadable Insta360 gyro record"; return false; }
            const size_t item = meta.raw_gyro ? 8 + 6 * 2 : 8 + 6 * 8;
            const size_t n = buf.size() / item;
            out.accel.reserve(out.accel.size() + n);
            out.gyro.reserve(out.gyro.size() + n);
            const double acc_scale = meta.raw_gyro ? meta.acc_range / 32768.0 : 1.0;
            const double gyro_scale = meta.raw_gyro ? meta.gyro_range / 32768.0 * kPi / 180.0 : 1.0;
            for (size_t i = 0; i < n; i++) {
                const uint8_t* p = buf.data() + i * item;
                const double t = fix_time((double)le64(p));
                double a[3], g[3];
                if (meta.raw_gyro) {
                    for (int k = 0; k < 3; k++) a[k] = ((double)le16(p + 8 + 2 * k) - 32768.0) * acc_scale;
                    for (int k = 0; k < 3; k++) g[k] = ((double)le16(p + 14 + 2 * k) - 32768.0) * gyro_scale;
                } else {
                    for (int k = 0; k < 3; k++) a[k] = lef64(p + 8 + 8 * k);
                    for (int k = 0; k < 3; k++) g[k] = lef64(p + 32 + 8 * k);
                }
                out.accel.push_back({t, a[0] * kG, a[1] * kG, a[2] * kG});
                out.gyro.push_back({t, g[0], g[1], g[2]});
            }
            any = any || n > 0;
        } else if (r.id == 7) {
            if (r.size > (256u << 20) || !src.readVec(r.offset, (size_t)r.size, buf)) { error = "unreadable Insta360 GPS record"; return false; }
            const size_t item = 53;
            const size_t n = buf.size() / item;
            for (size_t i = 0; i < n; i++) {
                const uint8_t* p = buf.data() + i * item;
                TelemetryGps g;
                g.unix_time = (double)le64(p) + le16(p + 8) / 1000.0;
                if (g.unix_time <= 0 || (p[10] != 'A' && p[10] != 'V')) continue;   // padding
                g.fix = p[10] == 'A';
                g.lat = std::fabs(lef64(p + 11)) * (p[19] == 'S' ? -1 : 1);
                g.lon = std::fabs(lef64(p + 20)) * (p[28] == 'W' ? -1 : 1);
                g.speed = lef64(p + 29);
                g.track = lef64(p + 37);
                g.alt = lef64(p + 45);
                g.has_alt = true;
                out.gps.push_back(g);
            }
            any = any || n > 0;
        }
    }
    if (!any) return true;

    out.carrier = TelemetryCarrier::Insta360;
    out.camera = meta.camera;
    out.serial = meta.serial;
    out.firmware = meta.firmware;
    if (meta.readout > 0) out.frame_readout = meta.readout / 1000.0;

    // GPS carries wall-clock time only. The metadata's creation_time is a
    // local-time YYYYMMDDHHMMSS number, so the movie header (UTC) is what
    // places the video on that clock; on an X5 it matched the first fix.
    double start = mv ? mv->unixStart() : 0.0;
    if (start > 0 && !out.gps.empty()) {
        out.video_unix_start = start;
        for (TelemetryGps& g : out.gps) g.t = g.unix_time - start;
    }
    out.notes.push_back(std::string("gyro record ") + (meta.raw_gyro ? "raw int16" : "float64") +
                        "; accel converted from g; axes are the IMU's own, not the lens frame");
    if (!meta.offset_v3.empty()) out.notes.push_back("offset_v3 lens calibration present (" +
                                                     std::to_string(meta.offset_v3.size()) + " chars)");
    if (!out.gps.empty() && !(start > 0)) out.notes.push_back("no movie creation time: GPS t left at 0");
    return true;
}

// ================
// Driver
// ================

bool read_any(const Source& src, Telemetry& out, std::string& error) {
    out = Telemetry();
    Movie mv;
    bool is_mp4 = false;
    const bool have_movie = read_movie(src, mv, is_mp4, error);
    if (!error.empty()) return false;
    const bool insta = insta_detect(src);
    if (!have_movie && !insta) {
        error = is_mp4 ? "no moov box" : "not an ISO-BMFF file and no Insta360 trailer";
        return false;
    }
    if (have_movie) {
        out.video_duration = mv.durationSec();
        out.video_unix_start = mv.unixStart();
        out.projection = mv.projection;
        for (const Track& tk : mv.tracks)
            if (tk.handler == fourcc("vide") && tk.fps() > 0) { out.video_fps = tk.fps(); break; }
    }
    if (insta) return read_insta360(src, have_movie ? &mv : nullptr, out, error);
    for (const Track& tk : mv.tracks) {
        if (tk.sample_type == fourcc("gpmd")) { if (!read_gpmf(src, tk, out, error)) return false; }
        else if (tk.sample_type == fourcc("camm")) { if (!read_camm(src, tk, out, error)) return false; }
        else if (tk.sample_type == fourcc("djmd")) { if (!read_dji(src, tk, out, error)) return false; }
        if (out.carrier != TelemetryCarrier::None) break;
    }
    return true;
}

// ================
// Checks
// ================

template <class T>
TelemetryStreamCheck stream_check(const std::vector<T>& v, const std::function<bool(const T&)>& finite) {
    TelemetryStreamCheck c;
    c.count = v.size();
    if (v.empty()) return c;
    c.t_first = v.front().t;
    c.t_last = v.back().t;
    std::vector<double> dts;
    dts.reserve(v.size());
    for (size_t i = 0; i < v.size(); i++) {
        if (!finite(v[i]) || !std::isfinite(v[i].t)) c.non_finite++;
        if (i == 0) continue;
        const double dt = v[i].t - v[i - 1].t;
        if (dt < 0) c.non_monotonic++;
        else dts.push_back(dt);
        c.max_gap = std::max(c.max_gap, dt);
    }
    if (!dts.empty()) {
        std::nth_element(dts.begin(), dts.begin() + dts.size() / 2, dts.end());
        const double med = dts[dts.size() / 2];
        if (med > 0) c.rate_hz = 1.0 / med;
    }
    return c;
}

double median_of(std::vector<double> v) {
    if (v.empty()) return 0;
    std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
    return v[v.size() / 2];
}

struct Q { double w, x, y, z; };
Q qconj(const Q& q) { return {q.w, -q.x, -q.y, -q.z}; }
void qrotate(const Q& q, const double v[3], double out[3]) {
    const double w = q.w, x = q.x, y = q.y, z = q.z;
    out[0] = (1 - 2 * (y * y + z * z)) * v[0] + 2 * (x * y - w * z) * v[1] + 2 * (x * z + w * y) * v[2];
    out[1] = 2 * (x * y + w * z) * v[0] + (1 - 2 * (x * x + z * z)) * v[1] + 2 * (y * z - w * x) * v[2];
    out[2] = 2 * (x * z - w * y) * v[0] + 2 * (y * z + w * x) * v[1] + (1 - 2 * (x * x + y * y)) * v[2];
}

double angle_deg(const double a[3], const double b[3]) {
    const double na = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
    const double nb = std::sqrt(b[0] * b[0] + b[1] * b[1] + b[2] * b[2]);
    if (!(na > 0) || !(nb > 0)) return 180.0;
    const double c = std::max(-1.0, std::min(1.0, (a[0] * b[0] + a[1] * b[1] + a[2] * b[2]) / (na * nb)));
    return std::acos(c) * 180.0 / kPi;
}

// Box average over +-half_window seconds: the accelerometer's low-frequency
// content is gravity, the rest is motion and vibration.
std::vector<TelemetryVec> smooth_accel(const std::vector<TelemetryVec>& a, double half_window) {
    std::vector<TelemetryVec> out(a.size());
    size_t lo = 0, hi = 0;
    double sx = 0, sy = 0, sz = 0;
    for (size_t i = 0; i < a.size(); i++) {
        while (hi < a.size() && a[hi].t <= a[i].t + half_window) { sx += a[hi].x; sy += a[hi].y; sz += a[hi].z; hi++; }
        while (lo < hi && a[lo].t < a[i].t - half_window) { sx -= a[lo].x; sy -= a[lo].y; sz -= a[lo].z; lo++; }
        const double n = (double)(hi - lo);
        out[i] = {a[i].t, sx / n, sy / n, sz / n};
    }
    return out;
}

// Median angle between the accelerometer rotated into the world by the
// attitude and the mean of those vectors: near zero when the quaternion sense
// is right and the camera moved gently.
double world_gravity_spread(const std::vector<TelemetryVec>& accel,
                            const std::vector<TelemetryQuat>& att, bool conj) {
    std::vector<std::array<double, 3>> w;
    size_t j = 0;
    const size_t step = std::max<size_t>(1, accel.size() / 2000);
    for (size_t i = 0; i < accel.size(); i += step) {
        while (j + 1 < att.size() && att[j + 1].t <= accel[i].t) j++;
        if (std::fabs(att[j].t - accel[i].t) > 0.05) continue;
        Q q{att[j].w, att[j].x, att[j].y, att[j].z};
        if (conj) q = qconj(q);
        const double v[3] = {accel[i].x, accel[i].y, accel[i].z};
        std::array<double, 3> r;
        qrotate(q, v, r.data());
        w.push_back(r);
    }
    if (w.size() < 10) return -1;
    double m[3] = {0, 0, 0};
    for (auto& r : w) for (int k = 0; k < 3; k++) m[k] += r[k];
    std::vector<double> ang;
    for (auto& r : w) ang.push_back(angle_deg(r.data(), m));
    return median_of(ang);
}

double haversine_m(double lat1, double lon1, double lat2, double lon2) {
    const double r = 6371000.0, d = kPi / 180.0;
    const double dlat = (lat2 - lat1) * d, dlon = (lon2 - lon1) * d;
    const double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
                     std::cos(lat1 * d) * std::cos(lat2 * d) * std::sin(dlon / 2) * std::sin(dlon / 2);
    return 2 * r * std::asin(std::min(1.0, std::sqrt(a)));
}

std::string fmt(const char* f, double a, double b = 0, double c = 0) {
    char s[256];
    std::snprintf(s, sizeof s, f, a, b, c);
    return s;
}

}  // namespace

// ================
// Public entry points
// ================

bool telemetry_read(const std::string& path, Telemetry& out, std::string& error) {
    FileSource src;
    if (!src.open(path)) { error = "cannot open '" + path + "'"; return false; }
    return read_any(src, out, error);
}

VideoProjection video_projection(const std::string& path) {
    FileSource src;
    if (!src.open(path)) return VideoProjection();
    Movie mv;
    bool is_mp4 = false;
    std::string error;
    if (!read_movie(src, mv, is_mp4, error)) return VideoProjection();
    return mv.projection;
}

VideoColor djmd_color(const uint8_t* sample, size_t n) {
    VideoColor out;
    const auto top = pb_fields(sample, n);
    const auto hdr = pb_sub(pb_find(pb_sub(pb_find(top, 1)), 1));
    out.proto = pb_string(pb_find(hdr, 1));
    if (out.proto.empty()) return out;
    out.mode = VideoColorMode::Unknown;
    // The Avata 360 keeps fov_type at the Osmo's 2.4, empty on its samples,
    // which the Osmo reading would take as Normal.
    const bool osmo = out.proto == "dvtm_oq101.proto";
    const bool avata = out.proto == "dvtm_AVATA360.proto";
    if (!osmo && !avata) {
        out.issue = VideoColorIssue::UnknownLayout;
        return out;
    }
    const auto stream = pb_sub(pb_find(top, 2));
    const auto avata_meta = pb_sub(pb_find(stream, 2));
    const PbField* wrapper = pb_find(osmo ? stream : avata_meta, 4);
    if (!wrapper) {
        out.issue = VideoColorIssue::NoColorField;
        return out;
    }
    // Only an empty wrapper is proto3's unwritten 0; any other shape is a
    // layout this reader was not written against, never Normal.
    out.issue = VideoColorIssue::MalformedColorField;
    if (wrapper->wire != 2) return out;
    const auto color = pb_sub(wrapper);
    const PbField* v = pb_find(color, 1);
    if (wrapper->len > 0 && (!v || v->wire != 0)) return out;
    out.issue = VideoColorIssue::None;
    out.code = v ? (int)v->varint : 0;
    // Avata 360 samples have shown only 19 (D-Log M) and an empty wrapper (Normal).
    if (avata && v && out.code != 19) {
        out.issue = VideoColorIssue::UnverifiedColorCode;
        return out;
    }
    out.mode = out.code == 19 ? VideoColorMode::DlogM
             : out.code == 0  ? VideoColorMode::Normal
                              : VideoColorMode::OtherLog;
    return out;
}

VideoColor video_color(const std::string& path) {
    FileSource src;
    if (!src.open(path)) return {};
    return color_of(src);
}

VideoColor video_color(const uint8_t* data, size_t size) {
    MemorySource src;
    src.p = data;
    src.n = size;
    return color_of(src);
}

bool telemetry_read(const uint8_t* data, size_t size, Telemetry& out, std::string& error) {
    MemorySource src;
    src.p = data;
    src.n = size;
    return read_any(src, out, error);
}

bool telemetry_read(uint64_t size, const TelemetryRead& read, Telemetry& out, std::string& error) {
    CallbackSource src;
    src.fn = &read;
    src.n = size;
    return read_any(src, out, error);
}

bool gps_valid(const TelemetryGps& g) {
    return g.fix && std::fabs(g.lat) <= 90 && std::fabs(g.lon) <= 180 && (g.lat != 0 || g.lon != 0);
}

size_t telemetry_gps_filter(const Telemetry& t, std::vector<TelemetryGps>& kept) {
    // Speed is measured from where a position was FIRST reported, since a
    // 1 Hz receiver logged at 10 Hz repeats each fix nine times.
    kept.clear();
    size_t outliers = 0;
    double seen_t = 0;
    for (const TelemetryGps& g : t.gps) {
        if (!gps_valid(g)) continue;
        if (g.dop > 10) { outliers++; continue; }
        if (kept.empty()) {
            seen_t = g.t;
        } else if (const TelemetryGps& p = kept.back(); g.lat != p.lat || g.lon != p.lon) {
            const double dt = g.t - seen_t;
            const double d = haversine_m(p.lat, p.lon, g.lat, g.lon);
            if (dt > 0 && d / dt > 50.0) { outliers++; continue; }
            seen_t = g.t;
        }
        kept.push_back(g);
    }
    return outliers;
}

double telemetry_haversine_m(double lat1, double lon1, double lat2, double lon2) {
    return haversine_m(lat1, lon1, lat2, lon2);
}

TelemetryCheck telemetry_check(const Telemetry& t) {
    TelemetryCheck c;
    auto vfinite = [](const TelemetryVec& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); };
    auto qfinite = [](const TelemetryQuat& q) { return std::isfinite(q.w) && std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z); };
    auto gfinite = [](const TelemetryGps& g) { return std::isfinite(g.lat) && std::isfinite(g.lon); };
    c.gyro = stream_check<TelemetryVec>(t.gyro, vfinite);
    c.accel = stream_check<TelemetryVec>(t.accel, vfinite);
    c.gravity = stream_check<TelemetryVec>(t.gravity, vfinite);
    c.orientation = stream_check<TelemetryQuat>(t.orientation, qfinite);
    c.gps = stream_check<TelemetryGps>(t.gps, gfinite);

    const double dur = t.video_duration;
    auto covers = [&](const TelemetryStreamCheck& s, const char* name) {
        if (!s.count) return;
        if (dur > 0 && (s.t_first > 1.0 || s.t_last < dur - 1.0))
            c.warnings.push_back(std::string(name) +
                                 fmt(" covers %.1f..%.1f s of a %.1f s video", s.t_first, s.t_last, dur));
        if (s.non_monotonic) c.warnings.push_back(std::string(name) + ": " + std::to_string(s.non_monotonic) + " timestamps run backwards");
        if (s.non_finite) c.warnings.push_back(std::string(name) + ": " + std::to_string(s.non_finite) + " non-finite readings");
        if (s.rate_hz > 0 && s.max_gap > 5.0 / s.rate_hz && s.max_gap > 0.05)
            c.warnings.push_back(std::string(name) + fmt(": a %.2f s gap at %.0f Hz", s.max_gap, s.rate_hz));
    };
    covers(c.gyro, "gyro");
    covers(c.accel, "accel");
    covers(c.orientation, "orientation");
    covers(c.gravity, "gravity");

    // Half a second of averaging leaves gravity and removes a bike's
    // vibration; on the one MAX ride measured the raw norm median was 12.6.
    const std::vector<TelemetryVec> accel_lp = smooth_accel(t.accel, 0.25);
    if (!t.accel.empty()) {
        std::vector<double> norms, dev;
        norms.reserve(t.accel.size());
        dev.reserve(t.accel.size());
        for (size_t i = 0; i < t.accel.size(); i++) {
            const TelemetryVec& v = accel_lp[i];
            norms.push_back(std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z));
            const double dx = t.accel[i].x - v.x, dy = t.accel[i].y - v.y, dz = t.accel[i].z - v.z;
            dev.push_back(std::sqrt(dx * dx + dy * dy + dz * dz));
        }
        c.accel_norm_median = median_of(norms);
        c.accel_norm_spread = median_of(dev);
        if (std::fabs(c.accel_norm_median - 1.0) < 0.2)
            c.warnings.push_back("accel norm is ~1: readings are in g, not m/s^2");
        else if (std::fabs(c.accel_norm_median - kG) > 0.2 * kG)
            c.warnings.push_back(fmt("accel norm median %.2f m/s^2 is far from gravity", c.accel_norm_median));
    }
    if (!t.gyro.empty()) {
        std::vector<double> norms;
        norms.reserve(t.gyro.size());
        for (const TelemetryVec& v : t.gyro) norms.push_back(std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z));
        c.gyro_norm_median = median_of(norms);
        if (c.gyro_norm_median > 10.0)
            c.warnings.push_back(fmt("gyro norm median %.1f rad/s: a handheld camera does not spin that fast, deg/s?", c.gyro_norm_median));
    }
    if (!t.orientation.empty()) {
        double prev[4] = {0, 0, 0, 0};
        for (size_t i = 0; i < t.orientation.size(); i++) {
            const TelemetryQuat& q = t.orientation[i];
            const double n = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
            c.orientation_norm_err = std::max(c.orientation_norm_err, std::fabs(n - 1.0));
            if (i > 0 && n > 0) {
                const double dot = std::fabs(prev[0] * q.w + prev[1] * q.x + prev[2] * q.y + prev[3] * q.z) / n;
                const double step = 2 * std::acos(std::min(1.0, dot)) * 180 / kPi;
                c.orientation_max_step_deg = std::max(c.orientation_max_step_deg, step);
            }
            prev[0] = q.w / (n > 0 ? n : 1); prev[1] = q.x / (n > 0 ? n : 1);
            prev[2] = q.y / (n > 0 ? n : 1); prev[3] = q.z / (n > 0 ? n : 1);
        }
        if (c.orientation_norm_err > 0.01) c.warnings.push_back(fmt("orientation quaternions off unit norm by up to %.3f", c.orientation_norm_err));
        if (!t.accel.empty()) {
            std::vector<TelemetryVec> permuted = accel_lp;
            if (t.orientation_axes.size() == 3 && t.orientation_axes != "XYZ") {
                for (size_t i = 0; i < permuted.size(); i++) {
                    const double c[3] = {accel_lp[i].x, accel_lp[i].y, accel_lp[i].z};
                    double o[3] = {0, 0, 0};
                    for (int k = 0; k < 3; k++) {
                        const int axis = std::tolower(t.orientation_axes[k]) - 'x';
                        if (axis >= 0 && axis < 3) o[k] = c[axis];
                    }
                    permuted[i].x = o[0]; permuted[i].y = o[1]; permuted[i].z = o[2];
                }
            }
            const double a = world_gravity_spread(permuted, t.orientation, false);
            const double b = world_gravity_spread(permuted, t.orientation, true);
            if (a >= 0 && b >= 0) {
                c.attitude_is_sensor_to_world = a <= b;
                c.accel_in_world_spread_deg = std::min(a, b);
                if (c.accel_in_world_spread_deg > 10.0)
                    c.warnings.push_back(fmt("accel rotated by the attitude is not a fixed world vector (%.1f deg spread either sense)", c.accel_in_world_spread_deg));
            }
        }
    }
    if (!t.gravity.empty() && !t.accel.empty()) {
        std::vector<double> ang;
        size_t j = 0;
        for (const TelemetryVec& g : t.gravity) {
            while (j + 1 < accel_lp.size() && accel_lp[j + 1].t <= g.t) j++;
            if (std::fabs(accel_lp[j].t - g.t) > 0.05) continue;
            const double a[3] = {accel_lp[j].x, accel_lp[j].y, accel_lp[j].z};
            const double b[3] = {g.x, g.y, g.z};
            ang.push_back(angle_deg(a, b));
        }
        if (!ang.empty()) {
            c.grav_vs_accel_deg = median_of(ang);
            if (c.grav_vs_accel_deg > 15.0)
                c.warnings.push_back(fmt("GRAV and the accelerometer disagree by %.0f deg (median): different frames?", c.grav_vs_accel_deg));
        }
    }

    if (!t.gps.empty()) {
        size_t fixes = 0, frozen = 0, run = 0, longest_run = 0;
        double hold_start = t.gps[0].t;
        for (size_t i = 0; i < t.gps.size(); i++) {
            const TelemetryGps& g = t.gps[i];
            if (gps_valid(g)) fixes++;
            const bool same = i > 0 && g.lat == t.gps[i - 1].lat && g.lon == t.gps[i - 1].lon;
            if (same) { frozen++; run++; }
            else { c.gps_distinct++; hold_start = g.t; run = 0; }
            longest_run = std::max(longest_run, run);
            c.gps_longest_hold = std::max(c.gps_longest_hold, g.t - hold_start);
            if (g.speed > c.gps_speed_max) c.gps_speed_max = g.speed;
        }
        // A stale X5 fix repeats its timestamp too, so the hold is also
        // measured in log samples at the log's mean spacing.
        if (dur > 0) c.gps_longest_hold = std::max(c.gps_longest_hold, (double)longest_run * dur / (double)t.gps.size());
        c.gps_fix_fraction = (double)fixes / (double)t.gps.size();
        c.gps_frozen_fraction = t.gps.size() > 1 ? (double)frozen / (double)(t.gps.size() - 1) : 0.0;

        std::vector<TelemetryGps> kept_v;
        c.gps_outliers = telemetry_gps_filter(t, kept_v);
        std::vector<const TelemetryGps*> kept;
        for (const TelemetryGps& g : kept_v) kept.push_back(&g);
        if (!kept.empty()) {
            double mlat = 0, mlon = 0;
            for (const TelemetryGps* g : kept) { mlat += g->lat; mlon += g->lon; }
            mlat /= (double)kept.size(); mlon /= (double)kept.size();
            double var = 0;
            for (const TelemetryGps* g : kept) {
                const double d = haversine_m(mlat, mlon, g->lat, g->lon);
                var += d * d;
            }
            c.gps_spread_m = std::sqrt(var / (double)kept.size());
            for (size_t i = 1; i < kept.size(); i++)
                c.gps_path_m += haversine_m(kept[i - 1]->lat, kept[i - 1]->lon, kept[i]->lat, kept[i]->lon);
        }
        if (c.gps_fix_fraction < 0.5) c.warnings.push_back(fmt("GPS: only %.0f%% of samples are fixes", 100 * c.gps_fix_fraction));
        if (c.gps_distinct < 5) c.warnings.push_back("GPS: only " + std::to_string(c.gps_distinct) + " distinct positions (stale fix)");
        else if (c.gps_longest_hold > std::max(30.0, 0.2 * dur)) c.warnings.push_back(fmt("GPS: one position held for %.0f s (stale fix)", c.gps_longest_hold));
        if (c.gps_outliers) c.warnings.push_back("GPS: " + std::to_string(c.gps_outliers) + " fixes dropped as outliers (DOP over 10, or a jump over 50 m/s)");
        if (c.gps_speed_max > 60) c.warnings.push_back(fmt("GPS: reported speed up to %.0f m/s", c.gps_speed_max));
        if (dur > 0 && c.gps.count > 1 && (c.gps.t_first < -5 || c.gps.t_last > dur + 5 || c.gps.t_last < 0))
            c.warnings.push_back(fmt("GPS times %.1f..%.1f s do not sit inside the video", c.gps.t_first, c.gps.t_last));
        for (const TelemetryGps& g : t.gps)
            if (g.has_alt && (g.alt < -500 || g.alt > 10000)) { c.warnings.push_back("GPS: an altitude outside -500..10000 m"); break; }
    }

    c.imu_usable = (c.gyro.count > 0 || c.orientation.count > 0) && c.accel.count > 0 &&
                   std::fabs(c.accel_norm_median - kG) <= 0.2 * kG &&
                   c.accel.non_monotonic == 0 && c.accel.non_finite == 0 &&
                   (dur <= 0 || (c.accel.t_first <= 1.0 && c.accel.t_last >= dur - 1.0));
    // One outage in a long walk is not a stale log: 74 s of no update inside
    // an 18-minute campus walk still left 1.6 km of usable path.
    const double hold_limit = std::max(30.0, 0.2 * dur);
    c.gps_usable = c.gps.count >= 3 && c.gps_fix_fraction >= 0.5 && c.gps_distinct >= 5 &&
                   c.gps_longest_hold <= hold_limit && c.gps_spread_m > 5.0;
    return c;
}

std::string telemetry_report(const Telemetry& t, const TelemetryCheck& c) {
    std::ostringstream o;
    o << "carrier      : " << telemetry_carrier_name(t.carrier) << "\n";
    if (!t.camera.empty()) {
        o << "camera       : " << t.camera;
        if (!t.firmware.empty()) o << "  fw " << t.firmware;
        if (!t.serial.empty()) o << "  sn " << t.serial;
        o << "\n";
    }
    if (!t.projection.name.empty()) {
        o << "projection   : " << t.projection.name;
        for (uint32_t v : t.projection.mode) o << " " << v;
        o << "\n";
    }
    o << "video        : " << fmt("%.1f s", t.video_duration);
    if (t.video_fps > 0) o << fmt(", %.2f fps", t.video_fps);
    if (t.frame_readout > 0) o << fmt(", readout %.1f ms", t.frame_readout * 1e3);
    o << "\n";
    auto stream = [&](const char* name, const TelemetryStreamCheck& s) {
        if (!s.count) return;
        o << name << ": " << s.count << " samples";
        if (s.rate_hz > 0) o << fmt(", %.1f Hz", s.rate_hz);
        o << fmt(", %.2f..%.2f s", s.t_first, s.t_last);
        if (s.count > 1) o << fmt(", max gap %.1f ms", s.max_gap * 1e3);
        if (s.non_monotonic) o << ", " << s.non_monotonic << " backwards";
        if (s.non_finite) o << ", " << s.non_finite << " non-finite";
    };
    if (c.gyro.count) { stream("gyro         ", c.gyro); o << fmt(", |w| median %.3f rad/s\n", c.gyro_norm_median); }
    if (c.accel.count) { stream("accel        ", c.accel); o << fmt(", |a| median %.2f m/s^2 over 0.5 s, vibration %.2f\n", c.accel_norm_median, c.accel_norm_spread); }
    if (c.gravity.count) {
        stream("gravity      ", c.gravity);
        if (c.grav_vs_accel_deg >= 0) o << fmt(", %.1f deg from accel", c.grav_vs_accel_deg);
        o << "\n";
    }
    if (c.orientation.count) {
        stream("orientation  ", c.orientation);
        o << fmt(", norm err %.4f, max step %.2f deg", c.orientation_norm_err, c.orientation_max_step_deg);
        if (c.accel_in_world_spread_deg >= 0)
            o << fmt(", gravity in world spread %.1f deg", c.accel_in_world_spread_deg)
              << " (" << (c.attitude_is_sensor_to_world ? "q" : "conj(q)") << " maps sensor->world"
              << (t.orientation_axes == "XYZ" ? "" : ", accel as " + t.orientation_axes) << ")";
        o << "\n";
    }
    if (c.gps.count) {
        stream("gps          ", c.gps);
        o << fmt(", fix %.0f%%, %.0f%% repeats", 100 * c.gps_fix_fraction, 100 * c.gps_frozen_fraction)
          << " (" << c.gps_distinct << fmt(" distinct, longest hold %.1f s)", c.gps_longest_hold)
          << fmt(", spread %.1f m, path %.1f m", c.gps_spread_m, c.gps_path_m);
        if (c.gps_outliers) o << ", " << c.gps_outliers << " outliers dropped";
        if (c.gps_speed_max > 0) o << fmt(", speed max %.1f m/s", c.gps_speed_max);
        const TelemetryGps& g = t.gps.front();
        o << fmt("\n               first fix %.6f, %.6f", g.lat, g.lon);
        if (g.has_alt) o << fmt(", alt %.1f m", g.alt);
        if (g.unix_time > 0) o << fmt(", unix %.3f", g.unix_time);
        o << "\n";
    }
    if (t.magnet.size()) o << "magnetometer : " << t.magnet.size() << " samples\n";
    for (const std::string& n : t.notes) o << "note         : " << n << "\n";
    for (const std::string& w : c.warnings) o << "WARNING      : " << w << "\n";
    o << "verdict      : IMU " << (c.imu_usable ? "usable" : (c.accel.count ? "suspect" : "absent"))
      << ", GPS " << (c.gps_usable ? "usable" : (c.gps.count ? "not usable" : "absent")) << "\n";
    return o.str();
}

}  // namespace sfm

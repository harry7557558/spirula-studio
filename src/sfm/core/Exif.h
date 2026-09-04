// EXIF metadata: the focal-length prior and the camera identity, read straight
// from the file header.
//
// Why this exists at all: without a prior, a new camera starts at COLMAP's
// geometric guess (1.2*max(w,h) for a pinhole). On a 24 mm full-frame capture
// that guess is 6739 px against a true 3637 -- 85% long, and
// focal is the parameter the incremental mapper is least able to recover on its
// own, because a too-long focal is absorbed by distortion and a shallow
// baseline. EXIF turns that guess into a measurement for every camera that
// records one, which is most of them (D46).
//
// We parse the TIFF block ourselves rather than teaching the vendored stb_image
// about EXIF: stb hands back pixels and we only want ~six tags, the format is
// small and well specified, and keeping the vendored decoder pristine means it
// can still be updated by dropping in a new file (D5). Only the file header is
// read -- a few KB, not the image.
//
// Deliberately *not* ported: COLMAP's sensor-width database (specs.h, a few
// thousand hand-collected make/model -> sensor width rows). It is data, not an
// algorithm, and it only matters for cameras that record FocalLength but
// neither FocalLengthIn35mmFilm nor FocalPlaneXResolution. Those two cover
// every camera in our datasets; a miss just falls back to the geometric guess,
// exactly as before.
#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace sfm {

struct ExifData {
    bool valid = false;         // an EXIF block was found and parsed
    std::string make, model;
    double focal_mm = 0;        // Exif:FocalLength
    double focal_35mm = 0;      // Exif:FocalLengthIn35mmFilm
    double focal_plane_x_res = 0;   // Exif:FocalPlaneXResolution
    int focal_plane_unit = 0;       // Exif:FocalPlaneResolutionUnit (2=in, 3=cm, 4=mm, 5=um)
    int pixel_width = 0, pixel_height = 0;  // Exif:PixelXDimension/PixelYDimension

    double exposure_time = 0;   // Exif:ExposureTime, seconds
    double f_number = 0;        // Exif:FNumber
    double iso = 0;             // Exif:PhotographicSensitivity (+ SOS/REI/ISOSpeed fallbacks)
    // APEX forms, NaN when absent (Tv=0 means a 1 s exposure, so 0 cannot
    // mark "missing"): t = 2^-Tv, N = 2^(Av/2).
    double shutter_apex  = std::numeric_limits<double>::quiet_NaN();  // Exif:ShutterSpeedValue
    double aperture_apex = std::numeric_limits<double>::quiet_NaN();  // Exif:ApertureValue

    bool hasFocal() const { return focal_35mm > 0 || focal_mm > 0; }
};

namespace detail {

// Bounds-checked little/big-endian reader over the TIFF block. Every accessor
// returns 0 out of range, so a truncated or hostile file yields empty fields
// instead of a read past the buffer.
struct TiffReader {
    const uint8_t* p = nullptr;
    size_t n = 0;
    bool le = true;

    uint16_t u16(size_t o) const {
        if (o + 2 > n) return 0;
        return le ? (uint16_t)(p[o] | (p[o + 1] << 8)) : (uint16_t)((p[o] << 8) | p[o + 1]);
    }
    uint32_t u32(size_t o) const {
        if (o + 4 > n) return 0;
        return le ? (uint32_t)(p[o] | (p[o + 1] << 8) | (p[o + 2] << 16) | ((uint32_t)p[o + 3] << 24))
                  : (uint32_t)(((uint32_t)p[o] << 24) | (p[o + 1] << 16) | (p[o + 2] << 8) | p[o + 3]);
    }
};

inline size_t tiffTypeSize(uint16_t t) {
    switch (t) {
        case 1: case 2: case 6: case 7: return 1;   // BYTE, ASCII, SBYTE, UNDEFINED
        case 3: case 8: return 2;                   // SHORT, SSHORT
        case 4: case 9: case 11: return 4;          // LONG, SLONG, FLOAT
        case 5: case 10: case 12: return 8;         // RATIONAL, SRATIONAL, DOUBLE
        default: return 0;
    }
}

// One IFD entry's value as a double (first component only, which is all any tag
// we read is). Returns false for types we do not decode.
inline bool tiffValue(const TiffReader& r, size_t entry, double& out) {
    uint16_t type = r.u16(entry + 2);
    uint32_t count = r.u32(entry + 4);
    if (count == 0) return false;
    size_t esz = tiffTypeSize(type);
    if (esz == 0) return false;
    size_t vo = (esz * count <= 4) ? entry + 8 : r.u32(entry + 8);
    switch (type) {
        case 1: case 7: out = vo < r.n ? r.p[vo] : 0; return vo < r.n;
        case 3: out = r.u16(vo); return true;
        case 4: out = r.u32(vo); return true;
        case 9: out = (int32_t)r.u32(vo); return true;
        case 5: {
            uint32_t num = r.u32(vo), den = r.u32(vo + 4);
            if (den == 0) return false;
            out = (double)num / den;
            return true;
        }
        case 10: {
            int32_t num = (int32_t)r.u32(vo), den = (int32_t)r.u32(vo + 4);
            if (den == 0) return false;
            out = (double)num / den;
            return true;
        }
        default: return false;
    }
}

inline std::string tiffString(const TiffReader& r, size_t entry) {
    uint32_t count = r.u32(entry + 4);
    if (count == 0) return {};
    size_t vo = (count <= 4) ? entry + 8 : r.u32(entry + 8);
    if (vo >= r.n) return {};
    size_t len = std::min((size_t)count, r.n - vo);
    while (len > 0 && r.p[vo + len - 1] == '\0') len--;
    std::string s((const char*)r.p + vo, len);
    // Trailing spaces are common ("NIKON CORPORATION   ").
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
    return s;
}

// Walk one IFD, filling `out`. `depth` guards against a file whose Exif-IFD
// pointer loops back on itself.
inline void parseIfd(const TiffReader& r, size_t off, ExifData& out, int depth) {
    if (depth > 3 || off + 2 > r.n) return;
    uint16_t count = r.u16(off);
    if ((size_t)count * 12 + off + 2 > r.n) count = (uint16_t)((r.n - off - 2) / 12);
    for (uint16_t i = 0; i < count; i++) {
        size_t e = off + 2 + (size_t)i * 12;
        uint16_t tag = r.u16(e);
        double v = 0;
        switch (tag) {
            case 0x010F: out.make = tiffString(r, e); break;
            case 0x0110: out.model = tiffString(r, e); break;
            case 0x8769:  // Exif sub-IFD, where the lens tags live
                if (tiffValue(r, e, v) && v > 0) parseIfd(r, (size_t)v, out, depth + 1);
                break;
            case 0x829A: if (tiffValue(r, e, v) && v > 0) out.exposure_time = v; break;
            case 0x829D: if (tiffValue(r, e, v) && v > 0) out.f_number = v; break;
            case 0x8827: if (tiffValue(r, e, v) && v > 0) out.iso = v; break;
            case 0x8831: case 0x8832: case 0x8833:  // SOS / REI / ISOSpeed
                if (out.iso == 0 && tiffValue(r, e, v) && v > 0) out.iso = v;
                break;
            case 0x9201: if (tiffValue(r, e, v)) out.shutter_apex = v; break;
            case 0x9202: if (tiffValue(r, e, v)) out.aperture_apex = v; break;
            case 0x920A: if (tiffValue(r, e, v)) out.focal_mm = v; break;
            case 0xA405: if (tiffValue(r, e, v)) out.focal_35mm = v; break;
            case 0xA20E: if (tiffValue(r, e, v)) out.focal_plane_x_res = v; break;
            case 0xA210: if (tiffValue(r, e, v)) out.focal_plane_unit = (int)v; break;
            case 0xA002: if (tiffValue(r, e, v)) out.pixel_width = (int)v; break;
            case 0xA003: if (tiffValue(r, e, v)) out.pixel_height = (int)v; break;
            default: break;
        }
    }
}

}  // namespace detail

// Parse a TIFF block (starting at the "II"/"MM" byte-order mark).
inline ExifData parseExifTiff(const uint8_t* data, size_t size) {
    ExifData out;
    if (size < 8) return out;
    detail::TiffReader r;
    r.p = data;
    r.n = size;
    if (data[0] == 'I' && data[1] == 'I') r.le = true;
    else if (data[0] == 'M' && data[1] == 'M') r.le = false;
    else return out;
    if (r.u16(2) != 42) return out;
    uint32_t ifd0 = r.u32(4);
    if (ifd0 == 0 || ifd0 >= size) return out;
    detail::parseIfd(r, ifd0, out, 0);
    out.valid = true;
    return out;
}

// Read EXIF from an image file. Only JPEG carries it in the layouts we handle
// (PNG/TIFF captures in our datasets do not have lens metadata); anything else
// returns an invalid ExifData, which every caller treats as "no prior".
inline ExifData readExif(const std::string& path) {
    ExifData out;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return out;
    // The APP1 segment is at the very front of a JPEG, but some writers put
    // APP0/JFIF and thumbnails first; 512 KB covers every real file and still
    // costs one read.
    std::vector<uint8_t> buf(512 * 1024);
    size_t n = fread(buf.data(), 1, buf.size(), f);
    fclose(f);
    if (n < 4) return out;
    if (buf[0] != 0xFF || buf[1] != 0xD8) return out;  // not a JPEG
    size_t o = 2;
    while (o + 4 <= n) {
        if (buf[o] != 0xFF) break;
        uint8_t marker = buf[o + 1];
        if (marker == 0xD8 || marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) {
            o += 2;
            continue;
        }
        if (marker == 0xDA || marker == 0xD9) break;  // scan data / end: no more metadata
        size_t seg = (size_t)(buf[o + 2] << 8 | buf[o + 3]);
        if (seg < 2 || o + 2 + seg > n) break;
        if (marker == 0xE1 && seg >= 8 && std::memcmp(&buf[o + 4], "Exif\0\0", 6) == 0)
            return parseExifTiff(&buf[o + 10], seg - 8);
        o += 2 + seg;
    }
    return out;
}

// The focal length in pixels of the *stored* image, or 0 if EXIF cannot say.
// COLMAP's rules (sensor/bitmap.cc ExifFocalLength), in the same order:
//
//   1. FocalLengthIn35mmFilm: by the CIPA definition this is the focal a 35 mm
//      frame (43.27 mm diagonal) would need for the same angle of view, so
//      f_px = f35 / 43.27 * image_diagonal.
//   2. FocalLength with FocalPlaneXResolution: the resolution gives sensor
//      pixels per mm directly, so f_px = f_mm * px_per_mm.
//
// One addition over COLMAP: rule 2's resolution describes the sensor readout,
// and a file that was resized after capture keeps its EXIF while its pixel
// dimensions change. When EXIF records the capture dimensions and they differ
// from the actual ones, px_per_mm is rescaled by the ratio. A no-op whenever
// they agree, which is the common case.
inline double exifFocalPx(const ExifData& e, int width, int height) {
    if (!e.valid || width <= 0 || height <= 0) return 0;
    if (e.focal_35mm > 0) {
        const double diag = std::sqrt((double)width * width + (double)height * height);
        return e.focal_35mm / 43.27 * diag;
    }
    if (e.focal_mm > 0 && e.focal_plane_x_res > 0 && e.focal_plane_unit >= 2 &&
        e.focal_plane_unit <= 5) {
        double px_per_mm = 0;
        switch (e.focal_plane_unit) {
            case 2: px_per_mm = e.focal_plane_x_res / 25.4; break;   // inches
            case 3: px_per_mm = e.focal_plane_x_res / 10.0; break;   // cm
            case 4: px_per_mm = e.focal_plane_x_res; break;          // mm
            case 5: px_per_mm = e.focal_plane_x_res * 1000.0; break; // um
            default: return 0;
        }
        if (e.pixel_width > 0 && e.pixel_width != width)
            px_per_mm *= (double)width / e.pixel_width;
        double f = e.focal_mm * px_per_mm;
        // A sanity floor/ceiling: a focal outside [0.1, 100] x the long edge is
        // a misparsed tag, not a lens, and feeding it to the mapper is worse
        // than having no prior at all.
        if (f > 0.1 * std::max(width, height) && f < 100.0 * std::max(width, height)) return f;
    }
    return 0;
}

// Relative capture exposure in EV stops, log2(t / N^2 * ISO), preferring the
// direct tags over their APEX forms. A missing component counts as 1; false
// only when the file records none of the three.
inline bool exifExposureEv(const ExifData& e, double& ev) {
    if (!e.valid) return false;
    double t = e.exposure_time > 0 ? e.exposure_time
             : std::isfinite(e.shutter_apex) ? std::exp2(-e.shutter_apex) : 0;
    double N = e.f_number > 0 ? e.f_number
             : std::isfinite(e.aperture_apex) ? std::exp2(e.aperture_apex / 2) : 0;
    double s = e.iso;
    if (t <= 0 && N <= 0 && s <= 0) return false;
    double rel = (t > 0 ? t : 1) / ((N > 0 ? N : 1) * (N > 0 ? N : 1)) * (s > 0 ? s : 1);
    if (!(rel > 0) || !std::isfinite(rel)) return false;
    ev = std::log2(rel);
    return true;
}

// The identity two images must share to be assumed the same physical camera:
// make, model and frame size. Empty when EXIF cannot identify the camera, which
// means "do not group by it".
//
// COLMAP's ExifCameraModel also puts the focal length in this string, so a zoom
// lens at 24.0 and at 25.0 mm is two cameras. That is the right instinct and the
// wrong test: EXIF quantizes the focal to whole millimetres, which is a 4% step
// at the wide end, so string equality splits one fixed lens in two while
// treating a 1.02x zoom as decisive. The focal is compared separately, with a
// tolerance (detail::exifFocalClusters, D48).
inline std::string exifCameraKey(const ExifData& e, int width, int height) {
    if (!e.valid || e.make.empty() || e.model.empty() || !e.hasFocal()) return {};
    char buf[32];
    snprintf(buf, sizeof buf, "-%dx%d", width, height);
    return e.make + "-" + e.model + buf;
}

}  // namespace sfm

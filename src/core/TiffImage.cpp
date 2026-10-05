// TIFF decode -- see core/TiffImage.h.
//
// Strips and tiles are independent, so the worker pool is over them, as in
// core/ExrImage.cpp. Each is decompressed, unpredicted and byte-swapped in a
// per-worker buffer and scattered into an image of the kept samples; channel
// layout, WhiteIsZero and premultiplied alpha are one pass after that.

#include "core/TiffImage.h"

#include "core/ColorSpace.h"
#include "core/HalfFloat.h"
#include "core/IccProfile.h"
#include "core/MappedFile.h"
#include "external/miniz.h"

#include <algorithm>
#include <atomic>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <mutex>
#include <thread>

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#  error "core/TiffImage.cpp swaps samples into host order assuming a little-endian host"
#endif

namespace tiff {
namespace {

// ===========================================================================
// Directory
// ===========================================================================

enum : uint16_t {
    kImageWidth = 256, kImageLength = 257, kBitsPerSample = 258,
    kCompression = 259, kPhotometric = 262, kFillOrder = 266,
    kStripOffsets = 273, kSamplesPerPixel = 277, kRowsPerStrip = 278,
    kStripByteCounts = 279, kPlanarConfig = 284, kPredictor = 317,
    kTileWidth = 322, kTileLength = 323, kTileOffsets = 324,
    kTileByteCounts = 325, kExtraSamples = 338, kSampleFormat = 339,
    kIccProfile = 34675, kDngVersion = 50706,
};

enum : uint16_t {
    kNone = 1, kLzw = 5, kDeflate = 8, kPackBits = 32773, kAdobeDeflate = 32946,
};

std::string compression_name(uint64_t c) {
    switch (c) {
        case kNone: return "none";
        case 2: return "CCITT RLE";
        case 3: return "CCITT Group 3";
        case 4: return "CCITT Group 4";
        case kLzw: return "LZW";
        case 6: return "old-style JPEG";
        case 7: return "JPEG";
        case kDeflate: case kAdobeDeflate: return "Deflate";
        case kPackBits: return "PackBits";
        case 34712: return "JPEG 2000";
        case 34925: return "LZMA";
        case 50000: return "Zstandard";
        case 50001: return "WebP";
        case 50002: return "JPEG XL";
        default: return "compression " + std::to_string(c);
    }
}

// Byte-order-aware reads over the mapped file. Out of range reads 0 and
// clears `ok`, so a truncated or hostile file fails rather than overreads.
struct Reader {
    const uint8_t* p = nullptr;
    size_t n = 0;
    bool le = true;
    bool big = false;      // BigTIFF: 8-byte counts and offsets
    bool ok = true;

    uint64_t uint(uint64_t o, int bytes) {
        if (o > n || (uint64_t)bytes > n - o) { ok = false; return 0; }
        uint64_t v = 0;
        for (int i = 0; i < bytes; i++)
            v |= (uint64_t)p[o + (size_t)(le ? i : bytes - 1 - i)] << (8 * i);
        return v;
    }
};

int type_size(uint64_t t) {
    switch (t) {
        case 1: case 6: case 7: return 1;           // BYTE, SBYTE, UNDEFINED
        case 3: case 8: return 2;                   // SHORT, SSHORT
        case 4: case 9: case 13: return 4;          // LONG, SLONG, IFD
        case 16: case 17: case 18: return 8;        // LONG8, SLONG8, IFD8
        default: return 0;                          // not an integer type
    }
}

// An entry's integer values; false for a non-integer type or a bad offset.
bool entry_values(Reader& r, uint64_t e, std::vector<uint64_t>& out) {
    const int field = r.big ? 8 : 4;
    const int sz = type_size(r.uint(e + 2, 2));
    const uint64_t count = r.uint(e + 4, field);
    if (!r.ok || sz == 0 || count == 0 || count > r.n / (uint64_t)sz) return false;
    uint64_t at = e + 4 + (uint64_t)field;
    if (count * (uint64_t)sz > (uint64_t)field) at = r.uint(at, field);
    out.resize((size_t)count);
    for (uint64_t i = 0; i < count; i++) out[(size_t)i] = r.uint(at + i * (uint64_t)sz, sz);
    return r.ok;
}

struct Directory {
    uint64_t width = 0, height = 0, spp = 1;
    std::vector<uint64_t> bits{1}, sample_format{1}, extra;
    uint64_t compression = kNone, photometric = UINT64_MAX, fill_order = 1;
    uint64_t planar = 1, predictor = 1, rows_per_strip = UINT32_MAX;
    uint64_t tile_w = 0, tile_h = 0;
    std::vector<uint64_t> strip_offsets, strip_counts, tile_offsets, tile_counts;
    uint64_t icc_offset = 0, icc_size = 0;
    bool dng = false;
};

std::string read_directory(Reader& r, Directory& d) {
    if (r.n < 8 || !((r.p[0] == 'I' && r.p[1] == 'I') || (r.p[0] == 'M' && r.p[1] == 'M')))
        return "not a TIFF file";
    r.le = r.p[0] == 'I';
    uint64_t ifd = 0;
    const uint64_t magic = r.uint(2, 2);
    if (magic == 42) {
        ifd = r.uint(4, 4);
    } else if (magic == 43) {
        r.big = true;
        if (r.uint(4, 2) != 8 || r.uint(6, 2) != 0) return "the BigTIFF header is malformed";
        ifd = r.uint(8, 8);
    } else {
        return "not a TIFF file";
    }
    const uint64_t entry_size = r.big ? 20 : 12;
    const uint64_t count = r.uint(ifd, r.big ? 8 : 2);
    const uint64_t first = ifd + (r.big ? 8 : 2);
    if (!r.ok || ifd < 8 || count == 0 || first > r.n || count > (r.n - first) / entry_size)
        return "the image directory is missing or truncated";

    std::vector<uint64_t> v;
    for (uint64_t i = 0; i < count; i++) {
        const uint64_t e = first + i * entry_size;
        const uint64_t tag = r.uint(e, 2);
        if (tag == kDngVersion) d.dng = true;
        // Bytes, not integers, and a profile never fits inline.
        if (tag == kIccProfile) {
            const int field = r.big ? 8 : 4;
            const uint64_t type = r.uint(e + 2, 2);
            const uint64_t size = r.uint(e + 4, field);
            const uint64_t at = r.uint(e + 4 + (uint64_t)field, field);
            if (r.ok && (type == 1 || type == 7) && size > (uint64_t)field) {
                d.icc_offset = at;
                d.icc_size = size;
            }
            r.ok = true;
            continue;
        }
        // A tag this reader has no use for may be any type; one it needs and
        // cannot read surfaces below as missing.
        if (!entry_values(r, e, v)) { r.ok = true; continue; }
        switch (tag) {
            case kImageWidth:       d.width = v[0]; break;
            case kImageLength:      d.height = v[0]; break;
            case kBitsPerSample:    d.bits = v; break;
            case kCompression:      d.compression = v[0]; break;
            case kPhotometric:      d.photometric = v[0]; break;
            case kFillOrder:        d.fill_order = v[0]; break;
            case kStripOffsets:     d.strip_offsets = v; break;
            case kSamplesPerPixel:  d.spp = v[0]; break;
            case kRowsPerStrip:     d.rows_per_strip = v[0]; break;
            case kStripByteCounts:  d.strip_counts = v; break;
            case kPlanarConfig:     d.planar = v[0]; break;
            case kPredictor:        d.predictor = v[0]; break;
            case kTileWidth:        d.tile_w = v[0]; break;
            case kTileLength:       d.tile_h = v[0]; break;
            case kTileOffsets:      d.tile_offsets = v; break;
            case kTileByteCounts:   d.tile_counts = v; break;
            case kExtraSamples:     d.extra = v; break;
            case kSampleFormat:     d.sample_format = v; break;
            default: break;
        }
    }
    return "";
}

// ===========================================================================
// Layout
// ===========================================================================

enum class Source { U8, U16, F16, F32, F64 };

struct Layout {
    int width = 0, height = 0;
    int spp = 0;            // samples per pixel in the file
    int colour = 0;         // 1 or 3
    int keep = 0;           // colour, plus one when the first extra is alpha
    bool premultiplied = false;
    bool white_is_zero = false;
    Source src = Source::U8;
    Sample sample = Sample::U8;
    int bytes = 1;          // per stored sample
    int compression = kNone;
    int predictor = 1;
    bool big_endian = false;
    bool planar = false;
    bool tiled = false;
    uint64_t chunk_w = 0, chunk_h = 0;   // a tile, or the full width x RowsPerStrip
    uint64_t across = 0, down = 0;       // chunks per plane, in x and y
    std::vector<uint64_t> offsets, counts;   // counts empty = not recorded

    uint64_t per_plane() const { return across * down; }
};

std::string make_layout(const Directory& d, size_t file_size, Layout& L) {
    if (d.dng || d.photometric == 32803 || d.photometric == 34892)
        return "this is camera raw (DNG) data; develop it to a TIFF or JPEG first";
    if (d.width == 0 || d.height == 0) return "the image has no size";
    if (d.width > (uint64_t)INT_MAX || d.height > (uint64_t)INT_MAX)
        return "the image is too large";
    if (d.spp == 0 || d.spp > 64) return "the samples-per-pixel count is invalid";
    L.width = (int)d.width;
    L.height = (int)d.height;
    L.spp = (int)d.spp;

    for (size_t i = 1; i < d.bits.size() && i < d.spp; i++)
        if (d.bits[i] != d.bits[0]) return "the samples have different bit depths";
    for (size_t i = 1; i < d.sample_format.size() && i < d.spp; i++)
        if (d.sample_format[i] != d.sample_format[0])
            return "the samples have different number formats";
    const uint64_t bits = d.bits[0], format = d.sample_format[0];
    if (format == 1 || format == 4) {
        if (bits == 8)       { L.src = Source::U8;  L.sample = Sample::U8; }
        else if (bits == 16) { L.src = Source::U16; L.sample = Sample::U16; }
        else return std::to_string(bits) + "-bit integer samples are not supported";
    } else if (format == 3) {
        L.sample = Sample::F32;
        if (bits == 16)      L.src = Source::F16;
        else if (bits == 32) L.src = Source::F32;
        else if (bits == 64) L.src = Source::F64;
        else return std::to_string(bits) + "-bit float samples are not supported";
    } else if (format == 2) {
        return "signed integer samples are not supported";
    } else {
        return "sample format " + std::to_string(format) + " is not supported";
    }
    L.bytes = (int)(bits / 8);
    if (d.fill_order != 1) return "bit-reversed data (FillOrder 2) is not supported";

    switch (d.compression) {
        case kNone: case kLzw: case kDeflate: case kAdobeDeflate: case kPackBits: break;
        default:
            return compression_name(d.compression) +
                   " compression is not supported; re-save with LZW or ZIP";
    }
    L.compression = (int)d.compression;
    if (d.predictor < 1 || d.predictor > 3)
        return "predictor " + std::to_string(d.predictor) + " is not supported";
    if (d.predictor == 3 && format != 3)
        return "the floating-point predictor is set on integer samples";
    L.predictor = (int)d.predictor;

    const uint64_t pm = d.photometric != UINT64_MAX ? d.photometric : (d.spp >= 3 ? 2 : 1);
    switch (pm) {
        case 0: case 1: L.colour = 1; L.white_is_zero = pm == 0; break;
        case 2: L.colour = 3; break;
        case 3: return "palette-colour images are not supported; re-save as RGB";
        case 5: return "CMYK images are not supported; convert to RGB";
        case 6: return "YCbCr images are not supported; re-save as RGB";
        case 8: case 9: case 10: return "Lab images are not supported; convert to RGB";
        default:
            return "photometric interpretation " + std::to_string(pm) + " is not supported";
    }
    if (L.spp < L.colour) return "an RGB image needs three samples per pixel";
    // An extra sample of type 0 is an arbitrary channel, not transparency.
    L.keep = L.colour;
    if (L.spp > L.colour && !d.extra.empty() && (d.extra[0] == 1 || d.extra[0] == 2)) {
        L.keep++;
        L.premultiplied = d.extra[0] == 1;
    }

    L.planar = d.planar == 2 && L.spp > 1;
    L.tiled = !d.tile_offsets.empty();
    if (L.tiled) {
        if (d.tile_w == 0 || d.tile_h == 0) return "a tiled image has no tile size";
        L.chunk_w = d.tile_w;
        L.chunk_h = d.tile_h;
        L.offsets = d.tile_offsets;
        L.counts = d.tile_counts;
    } else {
        if (d.strip_offsets.empty()) return "the image has neither strips nor tiles";
        L.chunk_w = d.width;
        L.chunk_h = d.rows_per_strip == 0 ? d.height : std::min(d.rows_per_strip, d.height);
        L.offsets = d.strip_offsets;
        L.counts = d.strip_counts;
    }
    L.across = (d.width + L.chunk_w - 1) / L.chunk_w;
    L.down = (d.height + L.chunk_h - 1) / L.chunk_h;
    const uint64_t chunks = L.per_plane() * (L.planar ? d.spp : 1);
    if (L.offsets.size() < chunks)
        return L.tiled ? "the tile table is incomplete" : "the strip table is incomplete";
    if (L.counts.size() < chunks) L.counts.clear();
    for (uint64_t k = 0; k < chunks; k++)
        if (L.offsets[(size_t)k] >= file_size) return "the file is truncated";
    // Checked before the image is allocated, so a corrupt size fails here
    // rather than in the allocator. Deflate tops out near 1032:1, LZW near 1300:1.
    const double cap = (double)file_size * (L.compression == kNone ? 1.0 : 2048.0);
    const double chunk = (double)L.chunk_w * (double)L.chunk_h * (L.planar ? 1 : L.spp) * L.bytes;
    if ((double)L.width * L.height * L.keep * L.bytes > cap || chunk > cap)
        return "the image is larger than the file could hold; it is corrupt or truncated";
    return "";
}

// ===========================================================================
// Decompressors -- each fills exactly `out_n` bytes or names what went wrong
// ===========================================================================

std::string lzw_decode(const uint8_t* in, size_t in_n, uint8_t* out, size_t out_n) {
    if (in_n >= 2 && in[0] == 0 && (in[1] & 1))
        return "old-style (pre-1991) LZW is not supported; re-save the file";
    struct Entry { uint16_t prefix, len; uint8_t suffix, first; };
    std::vector<Entry> tab(4096);
    for (int i = 0; i < 256; i++) tab[(size_t)i] = {0, 1, (uint8_t)i, (uint8_t)i};

    size_t o = 0, ip = 0;
    uint64_t acc = 0;
    int have = 0, bits = 9, next = 258, prev = -1;
    auto emit = [&](int code) {
        const size_t len = tab[(size_t)code].len;
        int k = code;
        for (size_t i = len; i-- > 0;) {
            if (o + i < out_n) out[o + i] = tab[(size_t)k].suffix;
            k = tab[(size_t)k].prefix;
        }
        o = std::min(o + len, out_n);
    };
    while (o < out_n) {
        while (have < bits && ip < in_n) {
            acc = (acc << 8) | in[ip++];
            have += 8;
        }
        if (have < bits) break;
        have -= bits;
        const int code = (int)((acc >> have) & ((1u << bits) - 1));
        if (code == 257) break;
        if (code == 256) {
            bits = 9;
            next = 258;
            prev = -1;
            continue;
        }
        if (prev < 0) {
            if (code > 255) return "an LZW stream is corrupt";
            emit(code);
            prev = code;
            continue;
        }
        if (code < next) {
            emit(code);
            if (next < 4096)
                tab[(size_t)next++] = {(uint16_t)prev, (uint16_t)(tab[(size_t)prev].len + 1),
                                       tab[(size_t)code].first, tab[(size_t)prev].first};
        } else if (code == next && next < 4096) {
            tab[(size_t)next++] = {(uint16_t)prev, (uint16_t)(tab[(size_t)prev].len + 1),
                                   tab[(size_t)prev].first, tab[(size_t)prev].first};
            emit(code);
        } else {
            return "an LZW stream is corrupt";
        }
        prev = code;
        // TIFF's LZW widens one code early: at 511, 1023 and 2047.
        if (next >= (1 << bits) - 1 && bits < 12) bits++;
    }
    return o == out_n ? "" : "an LZW block ends early";
}

std::string packbits_decode(const uint8_t* in, size_t in_n, uint8_t* out, size_t out_n) {
    size_t i = 0, o = 0;
    while (o < out_n && i < in_n) {
        const int n = (int)(int8_t)in[i++];
        if (n >= 0) {
            const size_t c = std::min({(size_t)n + 1, in_n - i, out_n - o});
            std::memcpy(out + o, in + i, c);
            i += (size_t)n + 1;
            o += c;
        } else if (n != -128) {
            if (i >= in_n) break;
            const size_t c = std::min((size_t)(1 - n), out_n - o);
            std::memset(out + o, in[i++], c);
            o += c;
        }
    }
    return o == out_n ? "" : "a PackBits block ends early";
}

// Writers pad the last strip to full height, so more output than fits is fine.
std::string deflate_decode(const uint8_t* in, size_t in_n, uint8_t* out, size_t out_n) {
    tinfl_decompressor inf;
    tinfl_init(&inf);
    size_t in_sz = in_n, out_sz = out_n;
    const tinfl_status st =
        tinfl_decompress(&inf, in, &in_sz, out, out, &out_sz,
                         TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    if (st < TINFL_STATUS_DONE) return "a Deflate block is corrupt";
    return out_sz == out_n ? "" : "a Deflate block ends early";
}

// ===========================================================================
// Predictors and byte order
// ===========================================================================

template <typename T>
void horizontal_acc(uint8_t* row, size_t samples, int stride) {
    T* v = (T*)row;
    for (size_t i = (size_t)stride; i < samples; i++) v[i] = (T)(v[i] + v[i - (size_t)stride]);
}

void byte_swap(uint8_t* p, size_t n, int bytes) {
    for (size_t i = 0; i + (size_t)bytes <= n; i += (size_t)bytes)
        std::reverse(p + i, p + i + bytes);
}

// Predictor 3 (Adobe's floating-point predictor): bytes differenced across
// the row, stored as byte planes MSB first. Undoing it yields host order.
void float_acc(uint8_t* row, size_t row_bytes, int bytes, int stride,
               std::vector<uint8_t>& tmp) {
    for (size_t i = (size_t)stride; i < row_bytes; i++)
        row[i] = (uint8_t)(row[i] + row[i - (size_t)stride]);
    tmp.assign(row, row + row_bytes);
    const size_t wc = row_bytes / (size_t)bytes;
    for (size_t j = 0; j < wc; j++)
        for (int b = 0; b < bytes; b++)
            row[j * (size_t)bytes + (size_t)b] = tmp[(size_t)(bytes - 1 - b) * wc + j];
}

// ===========================================================================
// Decoder
// ===========================================================================

struct Decoder {
    spirula::MappedFile map;
    Layout L;
    Info info;

    std::string open(const std::string& path) {
        if (const std::string e = map.open(path); !e.empty()) return e;
        Reader r{map.data(), map.size()};
        Directory d;
        if (const std::string e = read_directory(r, d); !e.empty()) return e;
        if (const std::string e = make_layout(d, map.size(), L); !e.empty()) return e;
        L.big_endian = !r.le;
        info.width = L.width;
        info.height = L.height;
        info.channels = L.keep;
        info.sample = L.sample;
        info.compression = compression_name((uint64_t)L.compression);
        icc::ColorSpace cs;
        if (d.icc_size && d.icc_offset < map.size() && d.icc_size <= map.size() - d.icc_offset &&
            icc::read(map.data() + d.icc_offset, (size_t)d.icc_size, cs) &&
            cs.grey == (L.colour == 1)) {
            info.icc = true;
            info.gamut = cs.gamut;
            info.is_linear = cs.is_linear;
            info.gamut_known = cs.gamut_known;
        }
        return "";
    }

    // Kept samples, interleaved, in `info.sample`'s type: `L.keep` per pixel.
    std::string run(int threads, uint8_t* img);
    std::string decode_chunk(size_t k, std::vector<uint8_t>& raw,
                             std::vector<uint8_t>& tmp, uint8_t* img) const;
};

template <typename Dst, typename Load>
void scatter(const Layout& L, const uint8_t* buf, uint64_t x0, uint64_t y0,
             uint64_t rows, int spc, int plane, Dst* img, Load load) {
    const uint64_t x1 = std::min<uint64_t>(x0 + L.chunk_w, (uint64_t)L.width);
    const uint64_t y1 = std::min<uint64_t>(y0 + rows, (uint64_t)L.height);
    const size_t step = (size_t)spc * (size_t)L.bytes;
    const int first = plane < 0 ? 0 : plane;
    const int last = plane < 0 ? L.keep : plane + 1;
    for (uint64_t y = y0; y < y1; y++) {
        const uint8_t* s = buf + (size_t)(y - y0) * (size_t)L.chunk_w * step;
        Dst* d = img + ((size_t)y * (size_t)L.width + (size_t)x0) * (size_t)L.keep;
        for (uint64_t x = x0; x < x1; x++, s += step, d += L.keep)
            for (int c = first; c < last; c++)
                d[c] = load(s + (size_t)(plane < 0 ? c : 0) * (size_t)L.bytes);
    }
}

std::string Decoder::decode_chunk(size_t k, std::vector<uint8_t>& raw,
                                  std::vector<uint8_t>& tmp, uint8_t* img) const {
    const uint64_t per_plane = L.per_plane();
    const int plane = L.planar ? (int)(k / per_plane) : -1;
    if (plane >= L.keep) return "";
    const uint64_t idx = k % per_plane;
    const uint64_t x0 = (idx % L.across) * L.chunk_w;
    const uint64_t y0 = (idx / L.across) * L.chunk_h;
    const int spc = L.planar ? 1 : L.spp;
    const size_t row_bytes = (size_t)L.chunk_w * (size_t)spc * (size_t)L.bytes;
    const uint64_t rows = L.tiled ? L.chunk_h : std::min(L.chunk_h, (uint64_t)L.height - y0);
    const size_t want = (size_t)rows * row_bytes;

    const uint64_t off = L.offsets[k];
    uint64_t len = map.size() - off;
    if (!L.counts.empty()) len = std::min(len, L.counts[k]);
    const uint8_t* in = map.data() + off;
    raw.resize(want);
    std::string err;
    switch (L.compression) {
        case kNone:
            if (len < want) return "the file is truncated";
            std::memcpy(raw.data(), in, want);
            break;
        case kLzw:      err = lzw_decode(in, (size_t)len, raw.data(), want); break;
        case kPackBits: err = packbits_decode(in, (size_t)len, raw.data(), want); break;
        default:        err = deflate_decode(in, (size_t)len, raw.data(), want); break;
    }
    if (!err.empty()) return err;

    for (uint64_t y = 0; y < rows; y++) {
        uint8_t* row = raw.data() + (size_t)y * row_bytes;
        if (L.predictor == 3) {
            float_acc(row, row_bytes, L.bytes, spc, tmp);
            continue;
        }
        if (L.big_endian && L.bytes > 1) byte_swap(row, row_bytes, L.bytes);
        if (L.predictor != 2) continue;
        const size_t samples = row_bytes / (size_t)L.bytes;
        switch (L.bytes) {
            case 1: horizontal_acc<uint8_t>(row, samples, spc); break;
            case 2: horizontal_acc<uint16_t>(row, samples, spc); break;
            case 4: horizontal_acc<uint32_t>(row, samples, spc); break;
            default: horizontal_acc<uint64_t>(row, samples, spc); break;
        }
    }

    const uint8_t* b = raw.data();
    switch (L.src) {
        case Source::U8:
            scatter(L, b, x0, y0, rows, spc, plane, img,
                    [](const uint8_t* p) { return *p; });
            break;
        case Source::U16:
            scatter(L, b, x0, y0, rows, spc, plane, (uint16_t*)img, [](const uint8_t* p) {
                uint16_t v;
                std::memcpy(&v, p, 2);
                return v;
            });
            break;
        case Source::F16: {
            const float* halves = spirula::half_to_float_table();
            scatter(L, b, x0, y0, rows, spc, plane, (float*)img, [halves](const uint8_t* p) {
                uint16_t v;
                std::memcpy(&v, p, 2);
                return halves[v];
            });
            break;
        }
        case Source::F32:
            scatter(L, b, x0, y0, rows, spc, plane, (float*)img, [](const uint8_t* p) {
                float v;
                std::memcpy(&v, p, 4);
                return v;
            });
            break;
        case Source::F64:
            scatter(L, b, x0, y0, rows, spc, plane, (float*)img, [](const uint8_t* p) {
                double v;
                std::memcpy(&v, p, 8);
                return (float)v;
            });
            break;
    }
    return "";
}

size_t worker_count(int threads, size_t n) {
    const unsigned hc = std::thread::hardware_concurrency();
    const size_t want = threads > 0 ? (size_t)threads : (hc > 0 ? (size_t)hc : 1);
    return std::max<size_t>(1, std::min(want, n));
}

// `fn(i, worker)` for i in [0, n) on worker_count(threads, n) workers; the
// first error stops the rest.
std::string parallel(size_t n, int threads,
                     const std::function<std::string(size_t, size_t)>& fn) {
    const size_t want = worker_count(threads, n);
    if (want == 1) {
        for (size_t i = 0; i < n; i++)
            if (const std::string e = fn(i, 0); !e.empty()) return e;
        return "";
    }
    std::atomic<size_t> next{0};
    std::mutex mu;
    std::string first;
    std::vector<std::thread> pool;
    pool.reserve(want);
    for (size_t t = 0; t < want; t++) {
        pool.emplace_back([&, t] {
            for (;;) {
                const size_t i = next.fetch_add(1);
                if (i >= n) return;
                const std::string e = fn(i, t);
                if (e.empty()) continue;
                std::lock_guard<std::mutex> lk(mu);
                if (first.empty()) first = e;
                next.store(n);
                return;
            }
        });
    }
    for (std::thread& t : pool) t.join();
    return first;
}

std::string Decoder::run(int threads, uint8_t* img) {
    const size_t chunks = (size_t)(L.per_plane() * (L.planar ? (uint64_t)L.spp : 1));
    const size_t workers = worker_count(threads, chunks);
    std::vector<std::vector<uint8_t>> raw(workers), tmp(workers);
    return parallel(chunks, threads, [&](size_t k, size_t t) {
        return decode_chunk(k, raw[t], tmp[t], img);
    });
}

size_t sample_size(Sample s) { return s == Sample::U8 ? 1 : s == Sample::U16 ? 2 : 4; }

// `img` (L.keep per pixel) to `out` (nc per pixel): WhiteIsZero undone,
// premultiplied alpha divided out, grey widened or RGB narrowed to luma.
template <typename T>
void finish(const Layout& L, const T* img, T* out, int nc, int threads) {
    const bool is_float = L.sample == Sample::F32;
    const float top = is_float ? 1.0f : (float)(sizeof(T) == 1 ? 255 : 65535);
    const bool alpha = L.keep > L.colour;
    const size_t w = (size_t)L.width;
    auto store = [is_float, top](float v) -> T {
        if (is_float) return (T)v;
        return (T)std::lround(std::min(std::max(v, 0.0f), top));
    };
    parallel((size_t)L.height, threads, [&](size_t y, size_t) {
        const T* s = img + y * w * (size_t)L.keep;
        T* d = out + y * w * (size_t)nc;
        for (size_t x = 0; x < w; x++, s += L.keep, d += nc) {
            float v[4] = {0, 0, 0, top};
            for (int c = 0; c < L.keep; c++) v[c] = (float)s[c];
            const float a = alpha ? v[L.keep - 1] : top;
            for (int c = 0; c < L.colour; c++) {
                if (L.white_is_zero) v[c] = top - v[c];
                if (L.premultiplied) v[c] = a > 0 ? v[c] * top / a : 0.0f;
            }
            if (L.colour == 1) v[1] = v[2] = v[0];
            if (nc == 1) {
                d[0] = store(L.colour == 1 ? v[0]
                                           : 0.2126f * v[0] + 0.7152f * v[1] + 0.0722f * v[2]);
                continue;
            }
            for (int c = 0; c < 3; c++) d[c] = store(v[c]);
            if (nc == 4) d[3] = store(a);
        }
        return std::string();
    });
}

}  // namespace


// ===========================================================================
// Public API
// ===========================================================================

bool is_tiff(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    uint8_t m[4] = {0, 0, 0, 0};
    const size_t got = std::fread(m, 1, 4, f);
    std::fclose(f);
    if (got != 4) return false;
    if (m[0] == 'I' && m[1] == 'I') return (m[2] == 42 || m[2] == 43) && m[3] == 0;
    if (m[0] == 'M' && m[1] == 'M') return m[2] == 0 && (m[3] == 42 || m[3] == 43);
    return false;
}

std::string probe(const std::string& path, Info& info) {
    Decoder d;
    if (const std::string e = d.open(path); !e.empty()) return e;
    info = d.info;
    return "";
}

std::string decode(const std::string& path, const Options& opt, Info& info,
                   std::vector<uint8_t>& out) {
    if (opt.channels != 1 && opt.channels != 3 && opt.channels != 4)
        return "a TIFF can be decoded to 1, 3 or 4 channels";
    Decoder d;
    if (const std::string e = d.open(path); !e.empty()) return e;
    info = d.info;
    const Layout& L = d.L;
    const size_t px = (size_t)L.width * (size_t)L.height;
    const size_t ss = sample_size(L.sample);
    const bool direct = L.keep == opt.channels && !L.white_is_zero && !L.premultiplied;
    std::vector<uint8_t> img;
    std::vector<uint8_t>& dst = direct ? out : img;
    dst.resize(px * (size_t)L.keep * ss);
    if (const std::string e = d.run(opt.threads, dst.data()); !e.empty()) return e;
    if (direct) return "";
    out.resize(px * (size_t)opt.channels * ss);
    switch (L.sample) {
        case Sample::U8:  finish(L, img.data(), out.data(), opt.channels, opt.threads); break;
        case Sample::U16: finish(L, (const uint16_t*)img.data(), (uint16_t*)out.data(),
                                 opt.channels, opt.threads); break;
        case Sample::F32: finish(L, (const float*)img.data(), (float*)out.data(),
                                 opt.channels, opt.threads); break;
    }
    return "";
}

bool declared_color_space(const std::string& path, Info& info) {
    return is_tiff(path) && probe(path, info).empty() && info.icc;
}

std::string decode_srgb8(const std::string& path, const Options& opt, Info& info,
                         std::vector<uint8_t>& out, const std::string& gamut,
                         std::optional<bool> is_linear, std::vector<uint8_t>* unexposed) {
    std::vector<uint8_t> px;
    if (const std::string e = decode(path, opt, info, px); !e.empty()) return e;
    const int nc = opt.channels;
    const size_t w = (size_t)info.width, h = (size_t)info.height;
    const std::string& space = gamut.empty() ? info.gamut : gamut;
    const bool linear = is_linear.value_or(info.is_linear);
    const Sample sample = info.sample;
    const float scale = sample == Sample::U8 ? 1.0f / 255.0f
                      : sample == Sample::U16 ? 1.0f / 65535.0f : 1.0f;
    auto value = [&](size_t i) -> float {
        if (sample == Sample::U8)  return px[i] * scale;
        if (sample == Sample::U16) return ((const uint16_t*)px.data())[i] * scale;
        return ((const float*)px.data())[i];
    };

    const colorspace::Srgb8Encoder plain(space, linear);
    std::vector<float> luma;
    if (opt.exposure.automatic) {
        const size_t step = colorspace::exposure_sample_step(w, h);
        for (size_t y = 0; y < h; y += step)
            for (size_t x = 0; x < w; x += step) {
                float s[4], v[3];
                for (int c = 0; c < nc; c++) s[c] = value((y * w + x) * (size_t)nc + (size_t)c);
                plain.to_linear(s, nc, v);
                luma.push_back(colorspace::luma709(v));
            }
    }
    info.gain = colorspace::exposure_gain(opt.exposure, luma);
    const colorspace::Srgb8Encoder enc(space, linear, info.gain);
    const bool both = unexposed && info.gain != 1.0f;
    if (unexposed) unexposed->assign(both ? w * h * (size_t)nc : 0, 0);

    out.resize(w * h * (size_t)nc);
    const size_t workers = worker_count(opt.threads, h);
    std::vector<std::vector<float>> rows(workers);
    std::vector<float> peaks(workers, 0.0f);
    parallel(h, opt.threads, [&](size_t y, size_t t) {
        const size_t at = y * w * (size_t)nc;
        std::vector<float>& row = rows[t];
        row.resize(w * (size_t)nc);
        float peak = peaks[t];
        for (size_t i = 0; i < row.size(); i++) {
            row[i] = value(at + i);
            if (nc != 4 || i % 4 != 3) peak = std::max(peak, row[i]);
        }
        peaks[t] = peak;
        enc(row.data(), out.data() + at, w, nc);
        if (both) plain(row.data(), unexposed->data() + at, w, nc);
        return std::string();
    });
    info.peak = *std::max_element(peaks.begin(), peaks.end());
    return "";
}

}  // namespace tiff

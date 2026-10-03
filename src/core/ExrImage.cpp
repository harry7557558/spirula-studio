// OpenEXR decode -- see core/ExrImage.h.
//
// Two facts shape the file. Chunks (scanline blocks or tiles) are entirely
// independent, so the worker pool is over chunks and one image scales across
// cores. And PIZ's Huffman tables are ~800 KB, so every per-block buffer
// lives in a per-worker Scratch allocated once -- tens of MB faulted in per
// call would serialize the workers on the kernel's mmap_lock.

#include "core/ExrImage.h"

#include "core/ColorSpace.h"
#include "core/HalfFloat.h"
#include "core/MappedFile.h"
#include "external/miniz.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <mutex>
#include <optional>
#include <thread>

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#  error "core/ExrImage.cpp reads EXR words in host order; the host must be little-endian"
#endif

namespace exr {
namespace {

// ===========================================================================
// Format primitives
// ===========================================================================

enum PixType { kUint = 0, kHalf = 1, kFloat = 2 };

enum CompressionId {
    kNone = 0, kRle = 1, kZips = 2, kZip = 3, kPiz = 4,
    kPxr24 = 5, kB44 = 6, kB44a = 7, kDwaa = 8, kDwab = 9,
};

const char* compression_name(int c) {
    static const char* const kNames[] = {"NONE", "RLE", "ZIPS", "ZIP", "PIZ",
                                         "PXR24", "B44", "B44A", "DWAA", "DWAB"};
    return c >= 0 && c < 10 ? kNames[c] : "unknown";
}

int lines_per_block(int c) {
    switch (c) {
        case kZip: case kPxr24: return 16;
        case kPiz: case kB44: case kB44a: case kDwaa: return 32;
        case kDwab: return 256;
        default: return 1;
    }
}

int type_size(int t) { return t == kHalf ? 2 : 4; }

inline int divp(int x, int y) {
    return x >= 0 ? x / y : -((-x + y - 1) / y);
}
inline int modp(int x, int y) { return y == 1 ? 0 : x - y * divp(x, y); }
inline int num_samples(int s, int a, int b) {
    return s == 1 ? b - a + 1 : divp(b, s) - divp(a, s) + 1;
}

inline float sample_to_float(int type, const uint8_t* p, const float* halves) {
    if (type == kHalf) {
        uint16_t h;
        std::memcpy(&h, p, 2);
        return halves[h];
    }
    if (type == kFloat) {
        float f;
        std::memcpy(&f, p, 4);
        return f;
    }
    uint32_t u;
    std::memcpy(&u, p, 4);
    return (float)u;
}

// ===========================================================================
// Header
// ===========================================================================

struct Channel {
    std::string name;
    int type = kHalf;
    int xs = 1, ys = 1;
    bool p_linear = false;
};

struct Part {
    std::vector<Channel> channels;   // chlist order, which the spec sorts by name
    int compression = kZip;
    int dx0 = 0, dy0 = 0, dx1 = -1, dy1 = -1;
    int px0 = 0, py0 = 0, px1 = -1, py1 = -1;
    bool tiled = false;
    uint32_t tile_w = 0, tile_h = 0;
    int level_mode = 0, round_mode = 0;
    bool has_chroma = false;
    float chroma[8] = {0};
    int64_t chunk_count = -1;
    std::string type;

    int data_w() const { return dx1 - dx0 + 1; }
    int data_h() const { return dy1 - dy0 + 1; }
};

struct Reader {
    const uint8_t* p = nullptr;
    size_t n = 0, at = 0;
    bool ok = true;

    bool take(size_t k) {
        if (!ok || at + k > n) { ok = false; return false; }
        at += k;
        return true;
    }
    uint8_t u8() {
        const size_t o = at;
        return take(1) ? p[o] : 0;
    }
    uint32_t u32() {
        const size_t o = at;
        if (!take(4)) return 0;
        uint32_t v;
        std::memcpy(&v, p + o, 4);
        return v;
    }
    int32_t i32() { return (int32_t)u32(); }
    uint64_t u64() {
        const size_t o = at;
        if (!take(8)) return 0;
        uint64_t v;
        std::memcpy(&v, p + o, 8);
        return v;
    }
    float f32() {
        const uint32_t u = u32();
        float f;
        std::memcpy(&f, &u, 4);
        return f;
    }
    std::string str() {
        std::string s;
        while (ok) {
            const uint8_t c = u8();
            if (!ok || c == 0) break;
            s += (char)c;
            if (s.size() > 255) { ok = false; break; }
        }
        return s;
    }
};

std::string parse_channels(Reader& r, size_t end, std::vector<Channel>& out) {
    while (r.ok && r.at < end) {
        if (r.p[r.at] == 0) { r.at++; break; }
        Channel c;
        c.name = r.str();
        c.type = r.i32();
        c.p_linear = r.u8() != 0;
        r.take(3);
        c.xs = r.i32();
        c.ys = r.i32();
        if (c.type < kUint || c.type > kFloat)
            return "a channel has an unknown pixel type";
        if (c.xs < 1 || c.ys < 1) return "a channel has an invalid sampling rate";
        out.push_back(c);
    }
    return r.ok ? "" : "the channel list is truncated";
}

std::string parse_part(Reader& r, Part& part) {
    for (;;) {
        const std::string name = r.str();
        if (!r.ok) return "the header is truncated";
        if (name.empty()) return "";
        r.str();                              // attribute type name
        const uint32_t size = r.u32();
        const size_t start = r.at;
        if (!r.take(size)) return "the header is truncated";
        Reader a{r.p, start + size, start, true};
        if (name == "channels") {
            if (const std::string e = parse_channels(a, start + size, part.channels);
                !e.empty())
                return e;
        } else if (name == "compression") {
            part.compression = a.u8();
        } else if (name == "dataWindow") {
            part.dx0 = a.i32(); part.dy0 = a.i32();
            part.dx1 = a.i32(); part.dy1 = a.i32();
        } else if (name == "displayWindow") {
            part.px0 = a.i32(); part.py0 = a.i32();
            part.px1 = a.i32(); part.py1 = a.i32();
        } else if (name == "tiles") {
            part.tile_w = a.u32();
            part.tile_h = a.u32();
            const uint8_t mode = a.u8();
            part.level_mode = mode & 0xf;
            part.round_mode = mode >> 4;
        } else if (name == "chromaticities") {
            for (int i = 0; i < 8; i++) part.chroma[i] = a.f32();
            part.has_chroma = a.ok;
        } else if (name == "chunkCount") {
            part.chunk_count = a.i32();
        } else if (name == "type") {
            // A string attribute is `size` raw bytes, with no terminator.
            part.type.assign((const char*)r.p + start, size);
        }
        if (!a.ok) return "an attribute in the header is truncated";
    }
}

// ===========================================================================
// Colour space
// ===========================================================================

struct GamutEntry {
    const char* name;
    float xy[8];   // Rx Ry Gx Gy Bx By Wx Wy
};

// The white point is part of the match: core/ColorSpace.h's "DCI-P3" is the
// theatrical white, and passing P3-D65 off as it is a visible green shift, so
// that one is reported as unknown rather than as nearly right.
const GamutEntry kGamutTable[] = {
    {"Rec.709",    {0.640f, 0.330f, 0.300f, 0.600f, 0.150f, 0.060f, 0.3127f, 0.3290f}},
    {"ACES2065-1", {0.7347f, 0.2653f, 0.0f, 1.0f, 0.0001f, -0.0770f, 0.32168f, 0.33767f}},
    {"ACEScg",     {0.713f, 0.293f, 0.165f, 0.830f, 0.128f, 0.044f, 0.32168f, 0.33767f}},
    {"Rec.2020",   {0.708f, 0.292f, 0.170f, 0.797f, 0.131f, 0.046f, 0.3127f, 0.3290f}},
    {"AdobeRGB",   {0.640f, 0.330f, 0.210f, 0.710f, 0.150f, 0.060f, 0.3127f, 0.3290f}},
    {"DCI-P3",     {0.680f, 0.320f, 0.265f, 0.690f, 0.150f, 0.060f, 0.314f, 0.351f}},
};

void resolve_gamut(const Part& part, Info& info) {
    info.chromaticities = part.has_chroma;
    if (!part.has_chroma) return;
    for (const GamutEntry& g : kGamutTable) {
        bool same = true;
        for (int i = 0; i < 8 && same; i++)
            same = std::fabs(part.chroma[i] - g.xy[i]) <= 0.002f;
        if (same) {
            info.gamut = std::strcmp(g.name, "Rec.709") == 0 ? "" : g.name;
            return;
        }
    }
    info.gamut_known = false;
}

// Luminance weights of the file's primaries, for the Y/RY/BY channel layout.
void luminance_weights(const Part& part, float w[3]) {
    w[0] = 0.2126f; w[1] = 0.7152f; w[2] = 0.0722f;
    if (!part.has_chroma) return;
    const float* c = part.chroma;
    double m[3][3];
    for (int i = 0; i < 3; i++) {
        const double x = c[2 * i], y = c[2 * i + 1];
        if (std::fabs(y) < 1e-9) return;
        m[0][i] = x / y;
        m[1][i] = 1.0;
        m[2][i] = (1.0 - x - y) / y;
    }
    const double wx = c[6], wy = c[7];
    if (std::fabs(wy) < 1e-9) return;
    const double wxyz[3] = {wx / wy, 1.0, (1.0 - wx - wy) / wy};
    const double det =
        m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
        m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
        m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    if (std::fabs(det) < 1e-12) return;
    double s[3];
    for (int k = 0; k < 3; k++) {
        double a[3][3];
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++) a[i][j] = j == k ? wxyz[i] : m[i][j];
        s[k] = (a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) -
                a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0]) +
                a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0])) / det;
    }
    const double sum = s[0] + s[1] + s[2];
    if (sum <= 0.0) return;
    for (int i = 0; i < 3; i++) w[i] = (float)(s[i] / sum);
}

// ===========================================================================
// Channel selection
// ===========================================================================

struct Selection {
    int idx[4] = {-1, -1, -1, -1};   // R G B A, or Y RY BY A
    bool luminance_chroma = false;
    bool gray = false;
    int count = 0;                   // what the file offers: 1, 3 or 4
};

int find_channel(const std::vector<Channel>& ch, const std::string& name) {
    for (size_t i = 0; i < ch.size(); i++)
        if (ch[i].name == name) return (int)i;
    return -1;
}

std::string select_channels(const std::vector<Channel>& ch, Selection& sel) {
    std::string layer;
    if (find_channel(ch, "R") < 0 && find_channel(ch, "Y") < 0) {
        std::vector<std::string> layers;
        for (const Channel& c : ch) {
            const size_t dot = c.name.rfind('.');
            if (dot == std::string::npos) continue;
            const std::string l = c.name.substr(0, dot);
            if (std::find(layers.begin(), layers.end(), l) == layers.end())
                layers.push_back(l);
        }
        if (layers.size() == 1) {
            layer = layers[0] + ".";
        } else if (layers.size() > 1) {
            std::string names;
            for (const std::string& l : layers) names += (names.empty() ? "" : ", ") + l;
            return "the file has several layers (" + names +
                   ") and no plain R/G/B channels; write one layer to its own file first";
        }
    }
    sel.idx[0] = find_channel(ch, layer + "R");
    sel.idx[1] = find_channel(ch, layer + "G");
    sel.idx[2] = find_channel(ch, layer + "B");
    sel.idx[3] = find_channel(ch, layer + "A");
    if (sel.idx[0] >= 0 && sel.idx[1] >= 0 && sel.idx[2] >= 0) {
        sel.count = sel.idx[3] >= 0 ? 4 : 3;
        return "";
    }
    const int y = find_channel(ch, layer + "Y");
    if (y >= 0) {
        sel.idx[0] = y;
        sel.idx[1] = find_channel(ch, layer + "RY");
        sel.idx[2] = find_channel(ch, layer + "BY");
        sel.idx[3] = find_channel(ch, layer + "A");
        sel.luminance_chroma = sel.idx[1] >= 0 && sel.idx[2] >= 0;
        sel.gray = !sel.luminance_chroma;
        sel.count = sel.gray ? 1 : (sel.idx[3] >= 0 ? 4 : 3);
        return "";
    }
    if (ch.size() == 1) {
        sel.idx[0] = 0;
        sel.gray = true;
        sel.count = 1;
        return "";
    }
    std::string names;
    for (const Channel& c : ch) names += (names.empty() ? "" : ", ") + c.name;
    return "the file has no R/G/B or Y channels (it has: " + names + ")";
}

// ===========================================================================
// Huffman, for PIZ
// ===========================================================================

constexpr int kHufEncSize = (1 << 16) + 1;
constexpr int kHufDecBits = 14;
constexpr int kHufDecSize = 1 << kHufDecBits;
constexpr int kHufDecMask = kHufDecSize - 1;
constexpr int kShortZeroRun = 59;
constexpr int kLongZeroRun = 63;
constexpr int kShortestLongRun = 2 + kLongZeroRun - kShortZeroRun;

struct HufDec {
    int len = 0;
    int lit = 0;
    int off = 0;   // into Scratch::huf_long, for codes longer than kHufDecBits
    int num = 0;
};

inline int64_t huf_length(int64_t c) { return c & 63; }
inline int64_t huf_code(int64_t c) { return c >> 6; }

struct BitReader {
    const uint8_t* in = nullptr;
    const uint8_t* end = nullptr;
    int64_t c = 0;
    int lc = 0;

    int64_t bits(int n) {
        while (lc < n) {
            c = (c << 8) | (in < end ? *in++ : 0);
            lc += 8;
        }
        lc -= n;
        return (c >> lc) & ((1 << n) - 1);
    }
};

// ===========================================================================
// Per-worker scratch
// ===========================================================================

struct Scratch {
    std::vector<uint8_t> raw;      // uncompressed block bytes
    std::vector<uint8_t> tmp;      // predictor / inflate staging
    std::vector<float> row;        // one gathered output row
    std::vector<float> chroma;     // last RY/BY row, for the odd rows below it
    std::vector<uint16_t> words;   // PIZ / B44 word planes
    std::vector<uint16_t> lut;
    std::vector<uint8_t> bitmap;
    std::vector<int64_t> hcode;
    std::vector<HufDec> hdec;
    std::vector<int> huf_long;
    std::vector<int> huf_count;
    std::vector<uint16_t> dwa_ac, dwa_dc, dwa_blocks;
    std::vector<uint8_t> dwa_unknown, dwa_rle;
};

// ===========================================================================
// Decompressors
// ===========================================================================

// The delta predictor and byte de-interleave that ZIP, ZIPS and RLE share.
void unpredict(uint8_t* buf, size_t n, uint8_t* out) {
    for (size_t i = 1; i < n; i++)
        buf[i] = (uint8_t)((int)buf[i - 1] + (int)buf[i] - 128);
    const uint8_t* t1 = buf;
    const uint8_t* t2 = buf + (n + 1) / 2;
    for (size_t i = 0; i < n; i += 2) {
        out[i] = *t1++;
        if (i + 1 < n) out[i + 1] = *t2++;
    }
}

std::string inflate_into(const uint8_t* in, size_t in_n, uint8_t* out, size_t out_n) {
    mz_ulong got = (mz_ulong)out_n;
    if (mz_uncompress(out, &got, in, (mz_ulong)in_n) != MZ_OK || got != out_n)
        return "a deflate block is corrupt";
    return "";
}

std::string rle_uncompress(const uint8_t* in, size_t in_n, uint8_t* out, size_t out_n) {
    size_t o = 0, i = 0;
    while (i < in_n) {
        const int n = (int)(int8_t)in[i++];
        if (n < 0) {
            const size_t count = (size_t)(-n);
            if (i + count > in_n || o + count > out_n) return "an RLE block is corrupt";
            std::memcpy(out + o, in + i, count);
            o += count;
            i += count;
        } else {
            const size_t count = (size_t)n + 1;
            if (i >= in_n || o + count > out_n) return "an RLE block is corrupt";
            std::memset(out + o, (int)in[i++], count);
            o += count;
        }
    }
    return o == out_n ? "" : "an RLE block is the wrong length";
}

void huf_canonical(int64_t* hcode) {
    int64_t n[59] = {0};
    for (int i = 0; i < kHufEncSize; i++) n[hcode[i] & 63]++;
    int64_t c = 0;
    for (int i = 58; i > 0; i--) {
        const int64_t nc = (c + n[i]) >> 1;
        n[i] = c;
        c = nc;
    }
    for (int i = 0; i < kHufEncSize; i++) {
        const int l = (int)hcode[i];
        if (l > 0) hcode[i] = l | (n[l]++ << 6);
    }
}

std::string huf_unpack_table(BitReader& br, int im, int iM, int64_t* hcode) {
    for (; im <= iM; im++) {
        const int64_t l = hcode[im] = br.bits(6);
        if (l == kLongZeroRun) {
            int zerun = (int)br.bits(8) + kShortestLongRun;
            if (im + zerun > iM + 1) return "the PIZ code table is too long";
            while (zerun--) hcode[im++] = 0;
            im--;
        } else if (l >= kShortZeroRun) {
            int zerun = (int)(l - kShortZeroRun + 2);
            if (im + zerun > iM + 1) return "the PIZ code table is too long";
            while (zerun--) hcode[im++] = 0;
            im--;
        }
    }
    huf_canonical(hcode);
    return "";
}

std::string huf_build_dec(const int64_t* hcode, int im, int iM, Scratch& s) {
    s.hdec.assign(kHufDecSize, HufDec());
    s.huf_count.assign(kHufDecSize, 0);
    int total = 0;
    for (int i = im; i <= iM; i++) {
        const int l = (int)huf_length(hcode[i]);
        if (l <= kHufDecBits) continue;
        const int64_t c = huf_code(hcode[i]);
        if (c >> l) return "the PIZ code table is corrupt";
        s.huf_count[(size_t)(c >> (l - kHufDecBits))]++;
        total++;
    }
    int acc = 0;
    for (int i = 0; i < kHufDecSize; i++) {
        s.hdec[(size_t)i].off = acc;
        acc += s.huf_count[(size_t)i];
    }
    s.huf_long.assign((size_t)std::max(total, 1), 0);
    for (int i = im; i <= iM; i++) {
        const int64_t c = huf_code(hcode[i]);
        const int l = (int)huf_length(hcode[i]);
        if (l > kHufDecBits) {
            HufDec& pl = s.hdec[(size_t)(c >> (l - kHufDecBits))];
            if (pl.len) return "the PIZ code table is corrupt";
            s.huf_long[(size_t)(pl.off + pl.num++)] = i;
        } else if (l) {
            const int64_t base = c << (kHufDecBits - l);
            const int64_t span = 1LL << (kHufDecBits - l);
            if (base + span > kHufDecSize) return "the PIZ code table is corrupt";
            HufDec* pl = &s.hdec[(size_t)base];
            for (int64_t k = span; k > 0; k--, pl++) {
                if (pl->len || pl->num) return "the PIZ code table is corrupt";
                pl->len = l;
                pl->lit = i;
            }
        }
    }
    return "";
}

std::string huf_decode(const int64_t* hcode, const Scratch& s, const uint8_t* in,
                       int ni, int rlc, size_t no, uint16_t* out) {
    const uint8_t* const ie = in + (ni + 7) / 8;
    uint16_t* const ob = out;
    uint16_t* const oe = out + no;
    int64_t c = 0;
    int lc = 0;

    auto emit = [&](int po) -> const char* {
        if (po == rlc) {
            if (lc < 8) {
                if (in >= ie) return "a PIZ block ends inside a run";
                c = (c << 8) | *in++;
                lc += 8;
            }
            lc -= 8;
            unsigned cs = (unsigned)((c >> lc) & 0xff);
            if (out + cs > oe) return "a PIZ block decodes to too much data";
            if (out == ob) return "a PIZ block starts with a run";
            const uint16_t v = out[-1];
            while (cs-- > 0) *out++ = v;
        } else if (out < oe) {
            *out++ = (uint16_t)po;
        } else {
            return "a PIZ block decodes to too much data";
        }
        return nullptr;
    };

    while (in < ie) {
        c = (c << 8) | *in++;
        lc += 8;
        while (lc >= kHufDecBits) {
            const HufDec& pl = s.hdec[(size_t)((c >> (lc - kHufDecBits)) & kHufDecMask)];
            if (pl.len) {
                lc -= pl.len;
                if (const char* e = emit(pl.lit)) return e;
                continue;
            }
            if (!pl.num) return "a PIZ block contains an invalid code";
            int j = 0;
            for (; j < pl.num; j++) {
                const int sym = s.huf_long[(size_t)(pl.off + j)];
                const int l = (int)huf_length(hcode[sym]);
                while (lc < l && in < ie) { c = (c << 8) | *in++; lc += 8; }
                if (lc >= l &&
                    huf_code(hcode[sym]) == ((c >> (lc - l)) & ((1LL << l) - 1))) {
                    lc -= l;
                    if (const char* e = emit(sym)) return e;
                    break;
                }
            }
            if (j == pl.num) return "a PIZ block contains an invalid code";
        }
    }

    const int drop = (8 - ni) & 7;
    c >>= drop;
    lc -= drop;
    while (lc > 0) {
        const HufDec& pl = s.hdec[(size_t)((c << (kHufDecBits - lc)) & kHufDecMask)];
        if (!pl.len) return "a PIZ block contains an invalid code";
        lc -= pl.len;
        if (const char* e = emit(pl.lit)) return e;
    }
    return "";
}

std::string huf_uncompress(Scratch& s, const uint8_t* in, size_t n,
                           uint16_t* out, size_t no) {
    if (n == 0) return no == 0 ? "" : "a PIZ block is empty";
    if (n < 20) return "a PIZ block is truncated";
    uint32_t v[5];
    std::memcpy(v, in, 20);
    const int im = (int)v[0], iM = (int)v[1], nbits = (int)v[3];
    if (im < 0 || im >= kHufEncSize || iM < im || iM >= kHufEncSize)
        return "a PIZ block declares an invalid symbol range";
    if (nbits < 0 || (size_t)nbits > 8 * (n - 20))
        return "a PIZ block declares more bits than it carries";

    s.hcode.assign(kHufEncSize, 0);
    BitReader br{in + 20, in + n, 0, 0};
    if (const std::string e = huf_unpack_table(br, im, iM, s.hcode.data()); !e.empty())
        return e;
    if (const std::string e = huf_build_dec(s.hcode.data(), im, iM, s); !e.empty())
        return e;
    return huf_decode(s.hcode.data(), s, br.in, nbits, iM, no, out);
}

// ===========================================================================
// PIZ wavelet
// ===========================================================================

inline void wdec14(uint16_t l, uint16_t h, uint16_t& a, uint16_t& b) {
    const int hi = (int16_t)h;
    const int ai = (int)(int16_t)l + (hi & 1) + (hi >> 1);
    a = (uint16_t)(int16_t)ai;
    b = (uint16_t)(int16_t)(ai - hi);
}

inline void wdec16(uint16_t l, uint16_t h, uint16_t& a, uint16_t& b) {
    const int bb = ((int)l - ((int)h >> 1)) & 0xffff;
    const int aa = ((int)h + bb - 32768) & 0xffff;
    a = (uint16_t)aa;
    b = (uint16_t)bb;
}

void wav2_decode(uint16_t* in, int nx, int ox, int ny, int oy, uint16_t mx) {
    const bool w14 = mx < (1 << 14);
    const int n = nx > ny ? ny : nx;
    int p = 1;
    while (p <= n) p <<= 1;
    p >>= 1;
    int p2 = p;
    p >>= 1;

    while (p >= 1) {
        uint16_t* py = in;
        uint16_t* const ey = in + (ptrdiff_t)oy * (ny - p2);
        const int oy1 = oy * p, oy2 = oy * p2, ox1 = ox * p, ox2 = ox * p2;
        uint16_t i00, i01, i10, i11;
        for (; py <= ey; py += oy2) {
            uint16_t* px = py;
            uint16_t* const ex = py + (ptrdiff_t)ox * (nx - p2);
            for (; px <= ex; px += ox2) {
                uint16_t* p01 = px + ox1;
                uint16_t* p10 = px + oy1;
                uint16_t* p11 = p10 + ox1;
                if (w14) {
                    wdec14(*px, *p10, i00, i10);
                    wdec14(*p01, *p11, i01, i11);
                    wdec14(i00, i01, *px, *p01);
                    wdec14(i10, i11, *p10, *p11);
                } else {
                    wdec16(*px, *p10, i00, i10);
                    wdec16(*p01, *p11, i01, i11);
                    wdec16(i00, i01, *px, *p01);
                    wdec16(i10, i11, *p10, *p11);
                }
            }
            if (nx & p) {
                uint16_t* p10 = px + oy1;
                if (w14) wdec14(*px, *p10, i00, *p10);
                else     wdec16(*px, *p10, i00, *p10);
                *px = i00;
            }
        }
        if (ny & p) {
            uint16_t* px = py;
            uint16_t* const ex = py + (ptrdiff_t)ox * (nx - p2);
            for (; px <= ex; px += ox2) {
                uint16_t* p01 = px + ox1;
                uint16_t a;
                if (w14) wdec14(*px, *p01, a, *p01);
                else     wdec16(*px, *p01, a, *p01);
                *px = a;
            }
        }
        p2 = p;
        p >>= 1;
    }
}

// ===========================================================================
// B44 block unpacking
// ===========================================================================

void unpack14(const uint8_t* b, uint16_t s[16]) {
    s[0] = (uint16_t)((b[0] << 8) | b[1]);
    const uint16_t shift = (uint16_t)(b[2] >> 2);
    const uint16_t bias = (uint16_t)(0x20 << shift);
    s[4]  = (uint16_t)(s[0]  + ((((b[2] << 4) | (b[3] >> 4)) & 0x3f) << shift) - bias);
    s[8]  = (uint16_t)(s[4]  + ((((b[3] << 2) | (b[4] >> 6)) & 0x3f) << shift) - bias);
    s[12] = (uint16_t)(s[8]  + ((b[4] & 0x3f) << shift) - bias);
    s[1]  = (uint16_t)(s[0]  + ((b[5] >> 2) << shift) - bias);
    s[5]  = (uint16_t)(s[4]  + ((((b[5] << 4) | (b[6] >> 4)) & 0x3f) << shift) - bias);
    s[9]  = (uint16_t)(s[8]  + ((((b[6] << 2) | (b[7] >> 6)) & 0x3f) << shift) - bias);
    s[13] = (uint16_t)(s[12] + ((b[7] & 0x3f) << shift) - bias);
    s[2]  = (uint16_t)(s[1]  + ((b[8] >> 2) << shift) - bias);
    s[6]  = (uint16_t)(s[5]  + ((((b[8] << 4) | (b[9] >> 4)) & 0x3f) << shift) - bias);
    s[10] = (uint16_t)(s[9]  + ((((b[9] << 2) | (b[10] >> 6)) & 0x3f) << shift) - bias);
    s[14] = (uint16_t)(s[13] + ((b[10] & 0x3f) << shift) - bias);
    s[3]  = (uint16_t)(s[2]  + ((b[11] >> 2) << shift) - bias);
    s[7]  = (uint16_t)(s[6]  + ((((b[11] << 4) | (b[12] >> 4)) & 0x3f) << shift) - bias);
    s[11] = (uint16_t)(s[10] + ((((b[12] << 2) | (b[13] >> 6)) & 0x3f) << shift) - bias);
    s[15] = (uint16_t)(s[14] + ((b[13] & 0x3f) << shift) - bias);
    for (int i = 0; i < 16; i++) {
        if (s[i] & 0x8000) s[i] &= 0x7fff;
        else               s[i] = (uint16_t)~s[i];
    }
}

void unpack3(const uint8_t* b, uint16_t s[16]) {
    s[0] = (uint16_t)((b[0] << 8) | b[1]);
    if (s[0] & 0x8000) s[0] &= 0x7fff;
    else               s[0] = (uint16_t)~s[0];
    for (int i = 1; i < 16; i++) s[i] = s[0];
}

// ===========================================================================
// Block decode
// ===========================================================================

struct Rect {
    int x0 = 0, x1 = 0, y0 = 0, y1 = 0;
};

size_t block_bytes(const Part& part, const Rect& r) {
    size_t n = 0;
    for (int y = r.y0; y <= r.y1; y++)
        for (const Channel& c : part.channels)
            if (modp(y, c.ys) == 0)
                n += (size_t)num_samples(c.xs, r.x0, r.x1) * type_size(c.type);
    return n;
}

// Word planes, as PIZ and B44 lay a block out before it is interleaved.
struct Plane {
    size_t start = 0;   // in uint16 units within Scratch::words
    int nx = 0, ny = 0, ys = 1, size = 1;
};

size_t build_planes(const Part& part, const Rect& r, std::vector<Plane>& out) {
    out.resize(part.channels.size());
    size_t at = 0;
    for (size_t i = 0; i < part.channels.size(); i++) {
        const Channel& c = part.channels[i];
        Plane& p = out[i];
        p.start = at;
        p.nx = num_samples(c.xs, r.x0, r.x1);
        p.ny = num_samples(c.ys, r.y0, r.y1);
        p.ys = c.ys;
        p.size = type_size(c.type) / 2;
        at += (size_t)p.nx * p.ny * p.size;
    }
    return at;
}

void interleave_planes(const Part& part, const Rect& r, const std::vector<Plane>& planes,
                       const uint16_t* words, uint8_t* out) {
    std::vector<size_t> at(planes.size());
    for (size_t i = 0; i < planes.size(); i++) at[i] = planes[i].start;
    for (int y = r.y0; y <= r.y1; y++) {
        for (size_t i = 0; i < planes.size(); i++) {
            if (modp(y, part.channels[i].ys) != 0) continue;
            const size_t n = (size_t)planes[i].nx * planes[i].size;
            std::memcpy(out, words + at[i], n * 2);
            out += n * 2;
            at[i] += n;
        }
    }
}

std::string piz_uncompress(const Part& part, const Rect& r, const uint8_t* in,
                           size_t in_n, Scratch& s, uint8_t* out) {
    if (in_n < 4) return "a PIZ block is truncated";
    std::vector<Plane> planes;
    const size_t words = build_planes(part, r, planes);
    if (s.words.size() < words) s.words.resize(words);
    s.bitmap.assign(8192, 0);
    s.lut.resize(65536);

    const uint8_t* p = in;
    uint16_t lo, hi;
    std::memcpy(&lo, p, 2);
    std::memcpy(&hi, p + 2, 2);
    p += 4;
    if (hi >= 8192) return "a PIZ block has an invalid bitmap range";
    if (lo <= hi) {
        const size_t n = (size_t)(hi - lo) + 1;
        if ((size_t)(p - in) + n > in_n) return "a PIZ block is truncated";
        std::memcpy(s.bitmap.data() + lo, p, n);
        p += n;
    }
    int k = 0;
    for (int i = 0; i < 65536; i++)
        if (i == 0 || (s.bitmap[(size_t)(i >> 3)] & (1 << (i & 7))))
            s.lut[(size_t)k++] = (uint16_t)i;
    const uint16_t max_value = (uint16_t)(k - 1);
    while (k < 65536) s.lut[(size_t)k++] = 0;

    if ((size_t)(p - in) + 4 > in_n) return "a PIZ block is truncated";
    uint32_t len;
    std::memcpy(&len, p, 4);
    p += 4;
    if ((size_t)(p - in) + len > in_n) return "a PIZ block is truncated";
    if (const std::string e = huf_uncompress(s, p, len, s.words.data(), words); !e.empty())
        return e;

    for (const Plane& pl : planes)
        for (int j = 0; j < pl.size; j++)
            wav2_decode(s.words.data() + pl.start + j, pl.nx, pl.size, pl.ny,
                        pl.nx * pl.size, max_value);
    for (size_t i = 0; i < words; i++) s.words[i] = s.lut[s.words[i]];
    interleave_planes(part, r, planes, s.words.data(), out);
    return "";
}

std::string pxr24_uncompress(const Part& part, const Rect& r, const uint8_t* in,
                             size_t in_n, Scratch& s, uint8_t* out) {
    size_t packed = 0;
    for (int y = r.y0; y <= r.y1; y++)
        for (const Channel& c : part.channels)
            if (modp(y, c.ys) == 0)
                packed += (size_t)num_samples(c.xs, r.x0, r.x1) *
                          (c.type == kFloat ? 3 : type_size(c.type));
    if (s.tmp.size() < packed) s.tmp.resize(packed);
    if (const std::string e = inflate_into(in, in_n, s.tmp.data(), packed); !e.empty())
        return e;

    const uint8_t* t = s.tmp.data();
    for (int y = r.y0; y <= r.y1; y++) {
        for (const Channel& c : part.channels) {
            if (modp(y, c.ys) != 0) continue;
            const int n = num_samples(c.xs, r.x0, r.x1);
            uint32_t pixel = 0;
            if (c.type == kHalf) {
                const uint8_t* a = t;
                const uint8_t* b = t + n;
                t += (size_t)n * 2;
                for (int i = 0; i < n; i++) {
                    pixel += ((uint32_t)a[i] << 8) | b[i];
                    const uint16_t h = (uint16_t)pixel;
                    std::memcpy(out, &h, 2);
                    out += 2;
                }
            } else if (c.type == kFloat) {
                const uint8_t* a = t;
                const uint8_t* b = t + n;
                const uint8_t* d = t + (size_t)n * 2;
                t += (size_t)n * 3;
                for (int i = 0; i < n; i++) {
                    pixel += ((uint32_t)a[i] << 24) | ((uint32_t)b[i] << 16) |
                             ((uint32_t)d[i] << 8);
                    std::memcpy(out, &pixel, 4);
                    out += 4;
                }
            } else {
                const uint8_t* a = t;
                const uint8_t* b = t + n;
                const uint8_t* d = t + (size_t)n * 2;
                const uint8_t* e = t + (size_t)n * 3;
                t += (size_t)n * 4;
                for (int i = 0; i < n; i++) {
                    pixel += ((uint32_t)a[i] << 24) | ((uint32_t)b[i] << 16) |
                             ((uint32_t)d[i] << 8) | e[i];
                    std::memcpy(out, &pixel, 4);
                    out += 4;
                }
            }
        }
    }
    return "";
}

std::string b44_uncompress(const Part& part, const Rect& r, const uint8_t* in,
                           size_t in_n, Scratch& s, uint8_t* out) {
    std::vector<Plane> planes;
    const size_t words = build_planes(part, r, planes);
    if (s.words.size() < words) s.words.resize(words);

    size_t at = 0;
    for (size_t ci = 0; ci < part.channels.size(); ci++) {
        const Plane& pl = planes[ci];
        uint16_t* dst = s.words.data() + pl.start;
        if (part.channels[ci].type != kHalf) {
            const size_t n = (size_t)pl.nx * pl.ny * pl.size * 2;
            if (at + n > in_n) return "a B44 block is truncated";
            std::memcpy(dst, in + at, n);
            at += n;
            continue;
        }
        for (int y = 0; y < pl.ny; y += 4) {
            for (int x = 0; x < pl.nx; x += 4) {
                uint16_t block[16];
                if (at + 3 > in_n) return "a B44 block is truncated";
                if (in[at + 2] == 0xfc) {
                    unpack3(in + at, block);
                    at += 3;
                } else {
                    if (at + 14 > in_n) return "a B44 block is truncated";
                    unpack14(in + at, block);
                    at += 14;
                }
                const int nx = std::min(4, pl.nx - x);
                const int ny = std::min(4, pl.ny - y);
                for (int j = 0; j < ny; j++)
                    std::memcpy(dst + (size_t)(y + j) * pl.nx + x, &block[j * 4],
                                (size_t)nx * 2);
            }
        }
    }
    interleave_planes(part, r, planes, s.words.data(), out);
    return "";
}

// ===========================================================================
// DWAA / DWAB
// ===========================================================================

// OpenEXR's lossy DCT codec as its SSE2 path decodes it (OpenEXRCore/
// internal_dwa_*.h, BSD-3-Clause, Copyright Contributors to the OpenEXR
// Project). Why that path, and the bits it fixes: docs/notes/exr.md.
#if defined(__clang__)
#  pragma STDC FP_CONTRACT OFF
#elif defined(__GNUC__)
#  pragma GCC optimize("fp-contract=off")
#elif defined(_MSC_VER)
#  pragma fp_contract(off)
#endif

enum DwaScheme { kDwaUnknown = 0, kDwaDct = 1, kDwaRle = 2 };

struct DwaRule {
    std::string suffix;
    int scheme = kDwaUnknown;
    int type = kHalf;
    int csc = -1;   // which of R, G, B it is in a colour-converted triple
    bool nocase = false;
};

// What a version 0 or 1 block classifies by; version 2 carries its own list.
const std::vector<DwaRule>& dwa_legacy_rules() {
    static const std::vector<DwaRule> rules = [] {
        std::vector<DwaRule> v;
        auto add = [&](const char* suffix, int scheme, int csc,
                       std::initializer_list<int> types) {
            for (int t : types) v.push_back({suffix, scheme, t, csc, true});
        };
        for (const char* s : {"r", "red"}) add(s, kDwaDct, 0, {kHalf, kFloat});
        for (const char* s : {"g", "grn", "green"}) add(s, kDwaDct, 1, {kHalf, kFloat});
        for (const char* s : {"b", "blu", "blue"}) add(s, kDwaDct, 2, {kHalf, kFloat});
        for (const char* s : {"y", "by", "ry"}) add(s, kDwaDct, -1, {kHalf, kFloat});
        add("a", kDwaRle, -1, {kUint, kHalf, kFloat});
        return v;
    }();
    return rules;
}

bool dwa_match(const DwaRule& r, const std::string& suffix, int type) {
    if (r.type != type || r.suffix.size() != suffix.size()) return false;
    for (size_t i = 0; i < suffix.size(); i++) {
        char a = suffix[i], b = r.suffix[i];
        if (r.nocase) {
            a = (char)std::tolower((unsigned char)a);
            b = (char)std::tolower((unsigned char)b);
        }
        if (a != b) return false;
    }
    return true;
}

std::string dwa_read_rules(const uint8_t*& p, size_t& left, std::vector<DwaRule>& rules) {
    const char* const bad = "a DWA block's channel rules are corrupt";
    if (left <= 2) return bad;
    uint16_t size;
    std::memcpy(&size, p, 2);
    if (size < 2 || size > left) return bad;
    const uint8_t* q = p + 2;
    size_t n = (size_t)size - 2;
    p += size;
    left -= size;
    while (n > 0) {
        if (n <= 3) return bad;
        size_t len = 0;
        for (; len < 129; len++) {
            if (len > n - 3) return bad;
            if (q[len] == 0) break;
        }
        if (len == 129) return bad;
        DwaRule r;
        r.suffix.assign((const char*)q, len);
        len++;
        if (n < len + 2) return bad;
        const uint8_t value = q[len], type = q[len + 1];
        q += len + 2;
        n -= len + 2;
        r.csc = (value >> 4) - 1;
        r.scheme = (value >> 2) & 3;
        r.nocase = (value & 1) != 0;
        r.type = type;
        if (r.csc >= 3 || r.scheme > kDwaRle || type > kFloat) return bad;
        rules.push_back(r);
    }
    return "";
}

uint16_t float_to_half(float f) {
    uint32_t bits;
    std::memcpy(&bits, &f, 4);
    const uint32_t ui = bits & 0x7fffffffu;
    uint16_t ret = (uint16_t)((bits >> 16) & 0x8000u);
    if (ui >= 0x38800000u) {
        if (ui >= 0x7f800000u) {
            ret |= 0x7c00;
            if (ui == 0x7f800000u) return ret;
            const uint32_t m = (ui & 0x7fffffu) >> 13;
            return (uint16_t)(ret | m | (m == 0));
        }
        if (ui > 0x477fefffu) return (uint16_t)(ret | 0x7c00);
        const uint32_t u = ui - 0x38000000u;
        return (uint16_t)(ret | ((u + 0x00000fffu + ((u >> 13) & 1)) >> 13));
    }
    if (ui < 0x33000001u) return ret;
    const uint32_t e = ui >> 23, shift = 0x7e - e;
    const uint32_t m = 0x800000u | (ui & 0x7fffffu), r = m << (32 - shift);
    ret = (uint16_t)(ret | (m >> shift));
    if (r > 0x80000000u || (r == 0x80000000u && (ret & 1) != 0)) ++ret;
    return ret;
}

// Stored values are 2.2-gamma below 1 and logarithmic above it.
const uint16_t* dwa_to_linear() {
    static const std::vector<uint16_t> lut = [] {
        const float* halves = spirula::half_to_float_table();
        std::vector<uint16_t> t(65536, 0);
        for (uint32_t x = 1; x < 65536; x++) {
            if ((x & 0x7c00u) == 0x7c00u) continue;
            float f = halves[x];
            const float sign = f < 0.0f ? -1.0f : 1.0f;
            f = std::fabs(f);
            const float px = f <= 1.0f ? f : 9.02501329156f;
            const float py = f <= 1.0f ? 2.2f : f - 1.0f;
            t[x] = float_to_half(sign * std::pow(px, py));
        }
        return t;
    }();
    return lut.data();
}

// In place, rows then columns, in the operation order of OpenEXR's SSE2 path:
// the bits depend on it.
void dct_inverse_8x8(float* data) {
    constexpr float a = 3.535536e-01f, b = 4.903927e-01f, c = 4.619398e-01f,
                    d = 4.157349e-01f, e = 2.777855e-01f, f = 1.913422e-01f,
                    g = 9.754573e-02f;
    // Row pass: column i of the basis scaled by input i, summed evens and odds.
    constexpr float m[8][4] = {{a, a, a, a},   {b, d, e, g},  {c, f, -f, -c},
                               {d, -g, -b, -e}, {a, -a, -a, a}, {e, -b, g, d},
                               {f, -c, c, -f},  {g, -e, d, -b}};
    for (int row = 0; row < 8; row++) {
        float* r = data + row * 8;
        float even[4], odd[4];
        for (int j = 0; j < 4; j++) {
            even[j] = 0.0f;
            odd[j] = 0.0f;
            for (int i = 0; i < 8; i += 2) {
                even[j] = even[j] + r[i] * m[i][j];
                odd[j] = odd[j] + r[i + 1] * m[i + 1][j];
            }
        }
        for (int j = 0; j < 4; j++) {
            r[j] = even[j] + odd[j];
            r[7 - j] = even[j] - odd[j];
        }
    }
    for (int col = 0; col < 8; col++) {
        float* v = data + col;
        const float alpha[4] = {c * v[16], f * v[16], c * v[48], f * v[48]};
        const float beta[4] = {
            (v[8] * b + v[24] * d) + (v[40] * e + v[56] * g),
            (v[8] * d - v[24] * g) - (v[40] * b + v[56] * e),
            (v[8] * e - v[24] * b) + (v[40] * g + v[56] * d),
            (v[8] * g - v[24] * e) + (v[40] * d - v[56] * b)};
        const float theta[4] = {a * (v[0] + v[32]), alpha[0] + alpha[3],
                                alpha[1] - alpha[2], a * (v[0] - v[32])};
        const float gamma[4] = {theta[0] + theta[1], theta[3] + theta[2],
                                theta[3] - theta[2], theta[0] - theta[1]};
        for (int j = 0; j < 4; j++) {
            v[8 * j] = gamma[j] + beta[j];
            v[8 * (7 - j)] = gamma[j] - beta[j];
        }
    }
}

// Y'CbCr back to R'G'B' with the 709 matrix, no offsets.
inline void csc709_inverse(float& c0, float& c1, float& c2) {
    const float y = c0, cb = c1, cr = c2;
    c0 = y + 1.5747f * cr;
    c1 = y - 0.1873f * cb - 0.4682f * cr;
    c2 = y + 1.8556f * cb;
}

constexpr uint8_t kZigZag[64] = {
    0,  1,  5,  6,  14, 15, 27, 28, 2,  4,  7,  13, 16, 26, 29, 42,
    3,  8,  12, 17, 25, 30, 41, 43, 9,  11, 18, 24, 31, 40, 44, 53,
    10, 19, 23, 32, 39, 45, 52, 54, 20, 22, 33, 38, 46, 51, 55, 60,
    21, 34, 37, 47, 50, 56, 59, 61, 35, 36, 48, 49, 57, 58, 62, 63};

struct DwaChannel {
    int scheme = kDwaUnknown;
    int nx = 0, ny = 0, bytes = 2, type = kHalf;
    bool p_linear = false;
    std::vector<uint8_t*> rows;
    size_t planar = 0;   // offset into the UNKNOWN or RLE buffer
};

struct DwaStream {
    const uint16_t* ac;
    const uint16_t* ac_end;
    const uint16_t* dc;
    size_t dc_left;
};

// One channel, or an R, G, B triple coded together as Y'CbCr. `lin` turns the
// stored nonlinear values back to linear; null for a pLinear channel.
std::string dwa_dct_decode(DwaChannel* const* ch, int ncomp, const uint16_t* lin,
                           DwaStream& st, Scratch& s) {
    const char* const bad = "a DWA block's DCT data is corrupt";
    const float* halves = spirula::half_to_float_table();
    const int w = ch[0]->nx, h = ch[0]->ny;
    const int bx = (w + 7) / 8, by = (h + 7) / 8;
    const int left_x = w - (bx - 1) * 8, left_y = h - (by - 1) * 8, full_x = w / 8;
    const size_t blocks = (size_t)bx * by;
    if (st.dc_left < (size_t)ncomp * blocks) return bad;
    s.dwa_blocks.resize((size_t)ncomp * bx * 64);
    uint16_t* row_block[3];
    const uint16_t* dc[3];
    for (int k = 0; k < ncomp; k++) {
        row_block[k] = s.dwa_blocks.data() + (size_t)k * bx * 64;
        dc[k] = st.dc + (size_t)k * blocks;
    }
    alignas(16) float dct[3][64];
    uint16_t zig[64];

    for (int yb = 0; yb < by; yb++) {
        const int max_y = yb == by - 1 ? left_y : 8;
        for (int xb = 0; xb < bx; xb++) {
            bool constant = true;
            for (int k = 0; k < ncomp; k++) {
                std::memset(zig, 0, sizeof zig);
                zig[0] = *dc[k]++;
                int comp = 1, last = 0;
                while (comp < 64) {
                    if (st.ac >= st.ac_end) return bad;
                    const uint16_t v = *st.ac++;
                    if ((v & 0xff00) == 0xff00) {
                        const int run = v & 0xff;
                        comp += run == 0 ? 64 : run;
                    } else {
                        last = comp;
                        zig[comp++] = v;
                    }
                }
                float* out = dct[k];
                if (last == 0) {
                    const float v = halves[zig[0]] * 3.535536e-01f * 3.535536e-01f;
                    for (int i = 0; i < 64; i++) out[i] = v;
                } else {
                    constant = false;
                    for (int i = 0; i < 64; i++) out[i] = halves[zig[kZigZag[i]]];
                    dct_inverse_8x8(out);
                }
            }
            const int n = constant ? 1 : 64;
            if (ncomp == 3)
                for (int i = 0; i < n; i++) csc709_inverse(dct[0][i], dct[1][i], dct[2][i]);
            for (int k = 0; k < ncomp; k++) {
                uint16_t* dst = row_block[k] + (size_t)xb * 64;
                if (constant) {
                    std::fill(dst, dst + 64, float_to_half(dct[k][0]));
                } else {
                    for (int i = 0; i < 64; i++) dst[i] = float_to_half(dct[k][i]);
                }
            }
        }
        for (int k = 0; k < ncomp; k++) {
            for (int y = 8 * yb; y < 8 * yb + max_y; y++) {
                uint8_t* dst = ch[k]->rows[(size_t)y];
                for (int xb = 0; xb < bx; xb++) {
                    const uint16_t* src = row_block[k] + (size_t)xb * 64 + (size_t)(y & 7) * 8;
                    const int m = xb < full_x ? 8 : left_x;
                    for (int x = 0; x < m; x++) {
                        const uint16_t v = lin ? lin[src[x]] : src[x];
                        std::memcpy(dst + (size_t)(xb * 8 + x) * 2, &v, 2);
                    }
                }
            }
        }
    }
    st.dc += (size_t)ncomp * blocks;
    st.dc_left -= (size_t)ncomp * blocks;

    // A FLOAT channel was decoded to half in the front of its row.
    for (int k = 0; k < ncomp; k++) {
        if (ch[k]->type != kFloat) continue;
        for (uint8_t* row : ch[k]->rows)
            for (int x = w - 1; x >= 0; x--) {
                uint16_t hv;
                std::memcpy(&hv, row + (size_t)x * 2, 2);
                std::memcpy(row + (size_t)x * 4, &halves[hv], 4);
            }
    }
    return "";
}

std::string dwa_uncompress(const Part& part, const Rect& r, const uint8_t* in,
                           size_t in_n, Scratch& s, uint8_t* out) {
    const char* const bad = "a DWA block is corrupt";
    enum { kVersion, kUnknownRaw, kUnknownPacked, kAcPacked, kDcPacked, kRlePacked,
           kRleUnpacked, kRleRaw, kAcCount, kDcCount, kAcCompression, kCounters };
    if (in_n < kCounters * 8) return "a DWA block is truncated";
    uint64_t n[kCounters];
    std::memcpy(n, in, sizeof n);
    for (uint64_t v : n)
        if ((int64_t)v < 0) return bad;
    if (n[kVersion] > 2) return "a DWA block is a version newer than this reader";
    const uint64_t packed = n[kUnknownPacked] + n[kAcPacked] + n[kDcPacked] + n[kRlePacked];
    if (n[kUnknownPacked] > in_n || n[kAcPacked] > in_n || n[kDcPacked] > in_n ||
        n[kRlePacked] > in_n || packed > in_n - sizeof n)
        return "a DWA block is truncated";

    const uint8_t* p = in + sizeof n;
    size_t left = in_n - sizeof n;
    std::vector<DwaRule> own;
    if (n[kVersion] >= 2)
        if (const std::string e = dwa_read_rules(p, left, own); !e.empty()) return e;
    const std::vector<DwaRule>& rules = n[kVersion] >= 2 ? own : dwa_legacy_rules();
    if (packed > left) return "a DWA block is truncated";

    // Each channel's scheme, and the R/G/B triples sharing a name prefix.
    const size_t nch = part.channels.size();
    std::vector<DwaChannel> ch(nch);
    std::vector<std::string> prefixes;
    std::vector<std::array<int, 3>> triples;
    for (size_t c = 0; c < nch; c++) {
        const Channel& pc = part.channels[c];
        const size_t dot = pc.name.rfind('.');
        const std::string prefix = dot == std::string::npos ? "" : pc.name.substr(0, dot + 1);
        const std::string suffix = pc.name.substr(prefix.size());
        size_t pi = 0;
        while (pi < prefixes.size() && prefixes[pi] != prefix) pi++;
        if (pi == prefixes.size()) {
            prefixes.push_back(prefix);
            triples.push_back({-1, -1, -1});
        }
        for (const DwaRule& rule : rules)
            if (dwa_match(rule, suffix, pc.type)) {
                ch[c].scheme = rule.scheme;
                if (rule.csc >= 0) triples[pi][(size_t)rule.csc] = (int)c;
            }
        ch[c].nx = num_samples(pc.xs, r.x0, r.x1);
        ch[c].ny = num_samples(pc.ys, r.y0, r.y1);
        ch[c].bytes = type_size(pc.type);
        ch[c].type = pc.type;
        ch[c].p_linear = pc.p_linear;
    }

    // Where each UNKNOWN or RLE channel sits in its planar buffer.
    size_t need[3] = {0, 0, 0};
    for (DwaChannel& c : ch) {
        const size_t size = (size_t)c.nx * c.ny * c.bytes;
        if (c.scheme == kDwaDct) continue;
        c.planar = need[c.scheme];
        need[c.scheme] += size;
    }
    auto sizes_ok = [&](int scheme, uint64_t packed_n, uint64_t raw_n, uint64_t extra) {
        if (need[scheme] > 0) return raw_n >= need[scheme] && packed_n != 0;
        return raw_n == 0 && packed_n == 0 && extra == 0;
    };
    if (!sizes_ok(kDwaUnknown, n[kUnknownPacked], n[kUnknownRaw], 0) ||
        !sizes_ok(kDwaRle, n[kRlePacked], n[kRleRaw], n[kRleUnpacked]))
        return bad;
    // The counts a block of this size can need, before anything is allocated by them.
    const uint64_t raw = block_bytes(part, r);
    const uint64_t blocks = (uint64_t)((r.y1 - r.y0 + 8) / 8) * (uint64_t)((r.x1 - r.x0 + 8) / 8);
    uint64_t dct_channels = 0;
    for (const DwaChannel& c : ch) dct_channels += c.scheme == kDwaDct;
    if (n[kUnknownRaw] > raw || n[kRleRaw] > raw || n[kUnknownRaw] + n[kRleRaw] > raw ||
        n[kRleUnpacked] > 2 * raw + 64 || n[kAcCount] > dct_channels * blocks * 63 ||
        n[kDcCount] > dct_channels * blocks)
        return bad;

    const uint8_t* unknown_in = p;
    const uint8_t* ac_in = unknown_in + n[kUnknownPacked];
    const uint8_t* dc_in = ac_in + n[kAcPacked];
    const uint8_t* rle_in = dc_in + n[kDcPacked];

    if (n[kUnknownPacked] > 0) {
        s.dwa_unknown.resize((size_t)n[kUnknownRaw]);
        if (const std::string e = inflate_into(unknown_in, (size_t)n[kUnknownPacked],
                                               s.dwa_unknown.data(), s.dwa_unknown.size());
            !e.empty())
            return e;
    }

    s.dwa_ac.assign((size_t)n[kAcCount], 0);
    if (n[kAcPacked] > 0) {
        std::string e;
        if (n[kAcCompression] == 0)
            e = huf_uncompress(s, ac_in, (size_t)n[kAcPacked], s.dwa_ac.data(), s.dwa_ac.size());
        else if (n[kAcCompression] == 1)
            e = inflate_into(ac_in, (size_t)n[kAcPacked], (uint8_t*)s.dwa_ac.data(),
                             s.dwa_ac.size() * 2);
        else
            e = bad;
        if (!e.empty()) return e;
    }

    s.dwa_dc.assign((size_t)n[kDcCount], 0);
    if (n[kDcPacked] > 0) {
        const size_t bytes = s.dwa_dc.size() * 2;
        if (s.tmp.size() < bytes) s.tmp.resize(bytes);
        if (const std::string e = inflate_into(dc_in, (size_t)n[kDcPacked], s.tmp.data(), bytes);
            !e.empty())
            return e;
        unpredict(s.tmp.data(), bytes, (uint8_t*)s.dwa_dc.data());
    } else if (n[kDcCount] != 0) {
        return bad;
    }

    if (n[kRleRaw] > 0) {
        if (s.tmp.size() < n[kRleUnpacked]) s.tmp.resize((size_t)n[kRleUnpacked]);
        if (const std::string e = inflate_into(rle_in, (size_t)n[kRlePacked], s.tmp.data(),
                                               (size_t)n[kRleUnpacked]);
            !e.empty())
            return e;
        s.dwa_rle.resize((size_t)n[kRleRaw]);
        if (const std::string e = rle_uncompress(s.tmp.data(), (size_t)n[kRleUnpacked],
                                                 s.dwa_rle.data(), s.dwa_rle.size());
            !e.empty())
            return e;
    }

    // Rows in the block's own layout: by line, then by channel.
    uint8_t* at = out;
    for (int y = r.y0; y <= r.y1; y++)
        for (size_t c = 0; c < nch; c++) {
            if (modp(y, part.channels[c].ys) != 0) continue;
            ch[c].rows.push_back(at);
            at += (size_t)ch[c].nx * ch[c].bytes;
        }

    DwaStream st{s.dwa_ac.data(), s.dwa_ac.data() + s.dwa_ac.size(), s.dwa_dc.data(),
                 s.dwa_dc.size()};
    std::vector<char> done(nch, 0);
    for (size_t pi = 0; pi < triples.size(); pi++) {
        const std::array<int, 3>& t = triples[pi];
        if (t[0] < 0 || t[1] < 0 || t[2] < 0) continue;
        const Channel& c0 = part.channels[(size_t)t[0]];
        const Channel& c1 = part.channels[(size_t)t[1]];
        const Channel& c2 = part.channels[(size_t)t[2]];
        if (c0.xs != c1.xs || c0.xs != c2.xs || c0.ys != c1.ys || c0.ys != c2.ys) continue;
        DwaChannel* set[3] = {&ch[(size_t)t[0]], &ch[(size_t)t[1]], &ch[(size_t)t[2]]};
        for (DwaChannel* c : set)
            if (c->scheme != kDwaDct) return bad;
        if (const std::string e = dwa_dct_decode(set, 3, dwa_to_linear(), st, s); !e.empty())
            return e;
        for (int k : t) done[(size_t)k] = 1;
    }

    for (size_t ci = 0; ci < nch; ci++) {
        DwaChannel& c = ch[ci];
        if (done[ci] || c.nx == 0 || c.ny == 0) continue;
        const size_t row_bytes = (size_t)c.nx * c.bytes;
        if (c.scheme == kDwaDct) {
            DwaChannel* one[1] = {&c};
            const uint16_t* lin = c.p_linear ? nullptr : dwa_to_linear();
            if (const std::string e = dwa_dct_decode(one, 1, lin, st, s); !e.empty()) return e;
        } else if (c.scheme == kDwaRle) {
            const size_t plane = (size_t)c.nx * c.ny;
            size_t i = 0;
            for (uint8_t* row : c.rows)
                for (int x = 0; x < c.nx; x++, i++)
                    for (int b = 0; b < c.bytes; b++)
                        row[(size_t)x * c.bytes + b] = s.dwa_rle[c.planar + b * plane + i];
        } else {
            size_t src = c.planar;
            for (uint8_t* row : c.rows) {
                if (src + row_bytes > s.dwa_unknown.size()) return bad;
                std::memcpy(row, s.dwa_unknown.data() + src, row_bytes);
                src += row_bytes;
            }
        }
    }
    return "";
}

// ===========================================================================
// Decoder
// ===========================================================================

int level_count(int size, int round_up) {
    int n = size, i = 0;
    while (n > 1) { n = round_up ? (n + 1) >> 1 : (n >> 1); i++; }
    return i + 1;
}

int level_size(int full, int l, int round_up) {
    const int b = 1 << l;
    int s = full / b;
    if (round_up && s * b < full) s++;
    return std::max(s, 1);
}

size_t tile_table_size(const Part& part) {
    const int w = part.data_w(), h = part.data_h();
    const int tw = (int)part.tile_w, th = (int)part.tile_h;
    auto tiles = [&](int lx, int ly) {
        const int lw = level_size(w, lx, part.round_mode);
        const int lh = level_size(h, ly, part.round_mode);
        return (size_t)((lw + tw - 1) / tw) * (size_t)((lh + th - 1) / th);
    };
    if (part.level_mode == 0) return tiles(0, 0);
    if (part.level_mode == 1) {
        size_t n = 0;
        const int levels = level_count(std::max(w, h), part.round_mode);
        for (int l = 0; l < levels; l++) n += tiles(l, l);
        return n;
    }
    size_t n = 0;
    const int nx = level_count(w, part.round_mode);
    const int ny = level_count(h, part.round_mode);
    for (int ly = 0; ly < ny; ly++)
        for (int lx = 0; lx < nx; lx++) n += tiles(lx, ly);
    return n;
}

using RowSink = std::function<void(int y, int x0, int n, const float* px)>;

struct Decoder {
    spirula::MappedFile map;
    Part part;
    Selection sel;
    Info info;
    std::vector<uint64_t> offsets;
    std::vector<int64_t> part_chunks;   // multi-part: each part has its own table
    int part_index = 0;
    bool multipart = false;
    int out_channels = 3;
    float yw[3] = {0.2126f, 0.7152f, 0.0722f};
    RowSink sink;

    bool covers_display() const {
        return part.dx0 <= part.px0 && part.dy0 <= part.py0 &&
               part.dx1 >= part.px1 && part.dy1 >= part.py1;
    }

    std::string open(const std::string& path);
    std::string read_offsets(Reader& r);
    std::string run(int threads);
    std::string decode_chunk(size_t i, Scratch& s);
    void emit_rows(const Rect& r, const uint8_t* blk, Scratch& s);
};

std::string Decoder::open(const std::string& path) {
    if (const std::string e = map.open(path); !e.empty()) return e;
    Reader r{map.data(), map.size(), 0, true};
    if (r.u32() != 20000630u) return "not an OpenEXR file";
    const uint32_t version = r.u32();
    if (!r.ok) return "the file is too short to be an EXR";
    if ((version & 0xffu) > 2)
        return "EXR version " + std::to_string(version & 0xffu) + " is newer than this reader";
    const bool tiled_flag = (version & 0x200u) != 0;
    multipart = (version & 0x1000u) != 0;
    if (version & 0x800u) return "deep EXR files are not supported; flatten it first";

    std::vector<Part> parts;
    for (;;) {
        Part p;
        p.tiled = tiled_flag;
        if (const std::string e = parse_part(r, p); !e.empty()) return e;
        parts.push_back(p);
        if (!multipart) break;
        if (!r.ok) return "the header is truncated";
        if (r.at < r.n && r.p[r.at] == 0) { r.at++; break; }
    }
    info.parts = (int)parts.size();

    // Colour first, then anything readable: a multi-part render puts depth or
    // an ID pass beside the beauty, and a one-channel part is not the image.
    int chosen = -1;
    std::string why;
    for (int pass = 0; pass < 2 && chosen < 0; pass++) {
        for (size_t i = 0; i < parts.size(); i++) {
            if (parts[i].type == "deepscanline" || parts[i].type == "deeptile") continue;
            Selection s;
            const std::string e = select_channels(parts[i].channels, s);
            if (!e.empty()) {
                if (why.empty()) why = e;
                continue;
            }
            if (pass == 0 && s.gray) continue;
            chosen = (int)i;
            sel = s;
            break;
        }
    }
    if (chosen < 0)
        return why.empty() ? "the file has no flat image part with colour channels" : why;
    part = parts[(size_t)chosen];
    part_index = chosen;
    for (const Part& p : parts) part_chunks.push_back(p.chunk_count);
    if (multipart) part.tiled = part.type == "tiledimage";

    if (part.data_w() <= 0 || part.data_h() <= 0) return "the data window is empty";
    if (part.px1 < part.px0 || part.py1 < part.py0) {
        part.px0 = part.dx0; part.py0 = part.dy0;
        part.px1 = part.dx1; part.py1 = part.dy1;
    }
    if (part.compression < kNone || part.compression > kDwab)
        return "EXR compression code " + std::to_string(part.compression) +
               " is not one this reader knows";
    if (part.tiled && (part.tile_w == 0 || part.tile_h == 0))
        return "a tiled part has no tile size";
    for (const Channel& c : part.channels) {
        if ((part.compression == kB44 || part.compression == kB44a) &&
            c.type == kHalf && c.p_linear)
            return "B44 channels marked pLinear are not supported; re-save it as ZIP";
        if (part.compression == kPiz && type_size(c.type) % 2 != 0)
            return "a PIZ channel has a pixel type this reader cannot unpack";
    }

    info.width = part.px1 - part.px0 + 1;
    info.height = part.py1 - part.py0 + 1;
    info.channels = sel.count;
    info.compression = compression_name(part.compression);
    info.tiled = part.tiled;
    resolve_gamut(part, info);
    luminance_weights(part, yw);
    return read_offsets(r);
}

// Reconstructed by walking the chunks when the table is all zeros, which is
// what an interrupted write leaves behind.
std::string Decoder::read_offsets(Reader& r) {
    size_t count;
    if (part.tiled) {
        count = tile_table_size(part);
    } else {
        const int lpb = lines_per_block(part.compression);
        count = (size_t)((part.data_h() + lpb - 1) / lpb);
    }
    if (multipart) {
        for (int i = 0; i < (int)part_chunks.size(); i++) {
            if (part_chunks[(size_t)i] <= 0)
                return "a multi-part file is missing its chunk count";
            if (i < part_index) r.take((size_t)part_chunks[(size_t)i] * 8);
        }
    }
    if (part.chunk_count > 0) count = (size_t)part.chunk_count;

    offsets.resize(count);
    bool empty = true;
    for (size_t i = 0; i < count; i++) {
        offsets[i] = r.u64();
        if (offsets[i] != 0) empty = false;
    }
    if (!r.ok) return "the chunk offset table is truncated";
    if (!empty) return "";
    if (multipart) return "the chunk offset table is empty";

    const size_t head = (multipart ? 4u : 0u) + (part.tiled ? 16u : 4u);
    size_t at = r.at;
    for (size_t i = 0; i < count; i++) {
        if (at + head + 4 > r.n) return "the file ends before its last chunk";
        uint32_t size;
        std::memcpy(&size, r.p + at + head, 4);
        offsets[i] = at;
        at += head + 4 + size;
    }
    return "";
}

void Decoder::emit_rows(const Rect& rect, const uint8_t* blk, Scratch& s) {
    const float* halves = spirula::half_to_float_table();
    const int w = info.width;
    const int x0 = std::max(rect.x0, part.px0);
    const int x1 = std::min(rect.x1, part.px1);
    if (x1 < x0) return;
    const int n = x1 - x0 + 1;
    if (s.row.size() < (size_t)n * out_channels) s.row.resize((size_t)n * out_channels);
    if (sel.luminance_chroma && s.chroma.size() < (size_t)w * 2)
        s.chroma.assign((size_t)w * 2, 0.0f);

    // Byte offset of each source channel within one row of the block.
    const size_t stride[4] = {
        sel.idx[0] >= 0 ? (size_t)type_size(part.channels[(size_t)sel.idx[0]].type) : 0,
        sel.idx[1] >= 0 ? (size_t)type_size(part.channels[(size_t)sel.idx[1]].type) : 0,
        sel.idx[2] >= 0 ? (size_t)type_size(part.channels[(size_t)sel.idx[2]].type) : 0,
        sel.idx[3] >= 0 ? (size_t)type_size(part.channels[(size_t)sel.idx[3]].type) : 0};

    size_t at = 0;
    for (int y = rect.y0; y <= rect.y1; y++) {
        const uint8_t* base[4] = {nullptr, nullptr, nullptr, nullptr};
        for (size_t ci = 0; ci < part.channels.size(); ci++) {
            const Channel& c = part.channels[ci];
            if (modp(y, c.ys) != 0) continue;
            for (int k = 0; k < 4; k++)
                if (sel.idx[k] == (int)ci) base[k] = blk + at;
            at += (size_t)num_samples(c.xs, rect.x0, rect.x1) * type_size(c.type);
        }
        if (y < part.py0 || y > part.py1) continue;

        const auto fetch = [&](int k, int x) -> float {
            if (!base[k]) return 0.0f;
            const Channel& c = part.channels[(size_t)sel.idx[k]];
            const int i = c.xs == 1 ? x - rect.x0 : divp(x, c.xs) - divp(rect.x0, c.xs);
            return sample_to_float(c.type, base[k] + (size_t)i * stride[k], halves);
        };

        float* dst = s.row.data();
        for (int x = x0; x <= x1; x++) {
            float rgba[4] = {0.0f, 0.0f, 0.0f, 1.0f};
            if (sel.gray) {
                rgba[0] = rgba[1] = rgba[2] = fetch(0, x);
                if (sel.idx[3] >= 0) rgba[3] = fetch(3, x);
            } else if (sel.luminance_chroma) {
                const float lum = fetch(0, x);
                float* cache = s.chroma.data() + (size_t)(x - part.px0) * 2;
                if (base[1] && base[2]) {
                    cache[0] = fetch(1, x);
                    cache[1] = fetch(2, x);
                }
                const float rr = (cache[0] + 1.0f) * lum;
                const float bb = (cache[1] + 1.0f) * lum;
                rgba[0] = rr;
                rgba[2] = bb;
                rgba[1] = (lum - rr * yw[0] - bb * yw[2]) / yw[1];
                if (sel.idx[3] >= 0) rgba[3] = fetch(3, x);
            } else {
                for (int k = 0; k < 3; k++) rgba[k] = fetch(k, x);
                if (sel.idx[3] >= 0) rgba[3] = fetch(3, x);
            }
            if (out_channels == 1) {
                *dst++ = sel.gray ? rgba[0]
                                  : 0.299f * rgba[0] + 0.587f * rgba[1] + 0.114f * rgba[2];
            } else {
                dst[0] = rgba[0];
                dst[1] = rgba[1];
                dst[2] = rgba[2];
                if (out_channels == 4) dst[3] = rgba[3];
                dst += out_channels;
            }
        }
        sink(y - part.py0, x0 - part.px0, n, s.row.data());
    }
}

std::string Decoder::decode_chunk(size_t i, Scratch& s) {
    // Zero is never a chunk offset -- the header is there. A mipmapped file
    // whose upper levels were never written leaves those entries at zero.
    if (offsets[i] == 0) return "";
    const uint8_t* p = map.data() + offsets[i];
    const uint8_t* const end = map.data() + map.size();
    const size_t head = (multipart ? 4u : 0u) + (part.tiled ? 16u : 4u);
    if (offsets[i] >= map.size() || (size_t)(end - p) < head + 4)
        return "a chunk points past the end of the file";
    if (multipart) p += 4;

    Rect rect;
    if (part.tiled) {
        int32_t v[4];
        std::memcpy(v, p, 16);
        p += 16;
        if (v[2] != 0 || v[3] != 0) return "";   // mip / rip levels below the full one
        rect.x0 = part.dx0 + v[0] * (int)part.tile_w;
        rect.y0 = part.dy0 + v[1] * (int)part.tile_h;
        rect.x1 = std::min(rect.x0 + (int)part.tile_w - 1, part.dx1);
        rect.y1 = std::min(rect.y0 + (int)part.tile_h - 1, part.dy1);
    } else {
        int32_t y;
        std::memcpy(&y, p, 4);
        p += 4;
        rect.x0 = part.dx0;
        rect.x1 = part.dx1;
        rect.y0 = y;
        rect.y1 = std::min(y + lines_per_block(part.compression) - 1, part.dy1);
    }
    if (rect.y0 < part.dy0 || rect.y1 > part.dy1 || rect.y1 < rect.y0 ||
        rect.x0 < part.dx0 || rect.x1 > part.dx1 || rect.x1 < rect.x0)
        return "a chunk covers pixels outside the data window";

    uint32_t size;
    std::memcpy(&size, p, 4);
    p += 4;
    if ((size_t)(end - p) < size) return "a chunk runs past the end of the file";

    const size_t want = block_bytes(part, rect);
    const uint8_t* blk = p;
    if (part.compression != kNone && size < want) {
        if (s.raw.size() < want) s.raw.resize(want);
        blk = s.raw.data();
        std::string e;
        switch (part.compression) {
            case kZip:
            case kZips:
                if (s.tmp.size() < want) s.tmp.resize(want);
                e = inflate_into(p, size, s.tmp.data(), want);
                if (e.empty()) unpredict(s.tmp.data(), want, s.raw.data());
                break;
            case kRle:
                if (s.tmp.size() < want) s.tmp.resize(want);
                e = rle_uncompress(p, size, s.tmp.data(), want);
                if (e.empty()) unpredict(s.tmp.data(), want, s.raw.data());
                break;
            case kPiz:   e = piz_uncompress(part, rect, p, size, s, s.raw.data()); break;
            case kPxr24: e = pxr24_uncompress(part, rect, p, size, s, s.raw.data()); break;
            case kB44:
            case kB44a:  e = b44_uncompress(part, rect, p, size, s, s.raw.data()); break;
            case kDwaa:
            case kDwab:  e = dwa_uncompress(part, rect, p, size, s, s.raw.data()); break;
            default:     e = "an unsupported compression reached the block decoder";
        }
        if (!e.empty()) return e;
    } else if (size < want) {
        return "a stored block is shorter than the pixels it must hold";
    }
    emit_rows(rect, blk, s);
    return "";
}

std::string Decoder::run(int threads) {
    const size_t n = offsets.size();
    if (n == 0) return "the file has no image chunks";
    unsigned hc = std::thread::hardware_concurrency();
    int want = threads > 0 ? threads : (hc > 0 ? (int)hc : 1);
    want = std::max(1, std::min<int>(want, (int)n));
    // Y/RY/BY reconstructs an odd row from the chroma of the row above it, and
    // that row is in the same chunk only when the chunk is not split up.
    if (sel.luminance_chroma) want = 1;

    if (want == 1) {
        Scratch s;
        for (size_t i = 0; i < n; i++)
            if (const std::string e = decode_chunk(i, s); !e.empty()) return e;
        return "";
    }

    std::atomic<size_t> next{0};
    std::mutex mu;
    std::string first;
    std::vector<std::thread> pool;
    pool.reserve((size_t)want);
    for (int t = 0; t < want; t++) {
        pool.emplace_back([&] {
            Scratch s;
            for (;;) {
                const size_t i = next.fetch_add(1);
                if (i >= n) return;
                const std::string e = decode_chunk(i, s);
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

}  // namespace


// ===========================================================================
// Public API
// ===========================================================================

bool is_exr(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    uint8_t magic[4] = {0, 0, 0, 0};
    const size_t got = std::fread(magic, 1, 4, f);
    std::fclose(f);
    return got == 4 && magic[0] == 0x76 && magic[1] == 0x2f && magic[2] == 0x31 &&
           magic[3] == 0x01;
}

std::string probe(const std::string& path, Info& info) {
    Decoder d;
    const std::string e = d.open(path);
    if (!e.empty()) return e;
    info = d.info;
    return "";
}

bool declared_color_space(const std::string& path, Info& info) {
    return is_exr(path) && probe(path, info).empty();
}

std::string decode(const std::string& path, const Options& opt, Info& info,
                   std::vector<float>& out) {
    if (opt.channels != 1 && opt.channels != 3 && opt.channels != 4)
        return "an EXR can be decoded to 1, 3 or 4 channels";
    Decoder d;
    if (const std::string e = d.open(path); !e.empty()) return e;
    d.out_channels = opt.channels;
    info = d.info;

    // Only clear when the data window leaves pixels nobody will write: on a
    // 24 MPix frame that memset is 288 MB of pure waste.
    const size_t w = (size_t)d.info.width, h = (size_t)d.info.height;
    out.resize(w * h * (size_t)opt.channels);
    if (!d.covers_display()) std::fill(out.begin(), out.end(), 0.0f);
    const int nc = opt.channels;
    float* dst = out.data();
    d.sink = [dst, w, nc](int y, int x0, int n, const float* px) {
        std::memcpy(dst + ((size_t)y * w + (size_t)x0) * (size_t)nc, px,
                    (size_t)n * (size_t)nc * sizeof(float));
    };
    return d.run(opt.threads);
}

std::string decode_srgb8(const std::string& path, const Options& opt, Info& info,
                         std::vector<uint8_t>& out, const std::string& gamut,
                         std::optional<bool> is_linear) {
    if (opt.channels != 1 && opt.channels != 3 && opt.channels != 4)
        return "an EXR can be decoded to 1, 3 or 4 channels";
    Decoder d;
    if (const std::string e = d.open(path); !e.empty()) return e;
    d.out_channels = opt.channels;
    info = d.info;

    const colorspace::Srgb8Encoder enc(gamut.empty() ? d.info.gamut : gamut,
                                       is_linear.value_or(d.info.is_linear));
    const size_t w = (size_t)d.info.width, h = (size_t)d.info.height;
    out.resize(w * h * (size_t)opt.channels);
    if (!d.covers_display()) std::fill(out.begin(), out.end(), (uint8_t)0);
    const int nc = opt.channels;
    uint8_t* dst = out.data();
    d.sink = [&enc, dst, w, nc](int y, int x0, int n, const float* px) {
        enc(px, dst + ((size_t)y * w + (size_t)x0) * (size_t)nc, (size_t)n, nc);
    };
    return d.run(opt.threads);
}

}  // namespace exr

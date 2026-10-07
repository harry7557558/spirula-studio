// GifWriter.cpp -- see GifWriter.h.

#include "app/gui/render/GifWriter.h"

#include <cstddef>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace gui::render {

namespace {

// 5 bits a channel: the histogram the palette is cut from.
constexpr int kBits = 5, kSide = 1 << kBits, kBins = kSide * kSide * kSide;

int bin_of(int r, int g, int b) {
    return ((r >> (8 - kBits)) << (2 * kBits)) | ((g >> (8 - kBits)) << kBits) | (b >> (8 - kBits));
}

struct Box {
    int lo[3], hi[3];
    uint64_t count = 0;
};

// Median cut over the histogram: the fullest box that can still split is
// split across its longest side at its median, until there are `n` boxes.
std::vector<uint8_t> median_cut(const std::vector<uint32_t>& count,
                                const std::vector<uint64_t>& sum, int n) {
    auto at = [](int r, int g, int b) { return (r << (2 * kBits)) | (g << kBits) | b; };
    auto shrink = [&](Box& x) {
        int lo[3] = {kSide, kSide, kSide}, hi[3] = {-1, -1, -1};
        x.count = 0;
        for (int r = x.lo[0]; r <= x.hi[0]; r++)
            for (int g = x.lo[1]; g <= x.hi[1]; g++)
                for (int b = x.lo[2]; b <= x.hi[2]; b++) {
                    const uint32_t c = count[(size_t)at(r, g, b)];
                    if (!c) continue;
                    x.count += c;
                    const int v[3] = {r, g, b};
                    for (int k = 0; k < 3; k++) { lo[k] = std::min(lo[k], v[k]); hi[k] = std::max(hi[k], v[k]); }
                }
        if (x.count)
            for (int k = 0; k < 3; k++) { x.lo[k] = lo[k]; x.hi[k] = hi[k]; }
    };
    std::vector<Box> boxes(1);
    for (int k = 0; k < 3; k++) { boxes[0].lo[k] = 0; boxes[0].hi[k] = kSide - 1; }
    shrink(boxes[0]);
    if (!boxes[0].count) return std::vector<uint8_t>(3, 0);
    while ((int)boxes.size() < n) {
        int best = -1;
        double score = 0.0;
        for (int i = 0; i < (int)boxes.size(); i++) {
            const Box& x = boxes[(size_t)i];
            const int side = std::max({x.hi[0] - x.lo[0], x.hi[1] - x.lo[1], x.hi[2] - x.lo[2]});
            if (side <= 0) continue;
            const double s = (double)x.count * side;
            if (s > score) { score = s; best = i; }
        }
        if (best < 0) break;
        Box& x = boxes[(size_t)best];
        int axis = 0;
        for (int k = 1; k < 3; k++)
            if (x.hi[k] - x.lo[k] > x.hi[axis] - x.lo[axis]) axis = k;
        // Counts along the axis, to find where half of them are.
        std::vector<uint64_t> along((size_t)kSide, 0);
        for (int r = x.lo[0]; r <= x.hi[0]; r++)
            for (int g = x.lo[1]; g <= x.hi[1]; g++)
                for (int b = x.lo[2]; b <= x.hi[2]; b++) {
                    const int v[3] = {r, g, b};
                    along[(size_t)v[axis]] += count[(size_t)at(r, g, b)];
                }
        uint64_t acc = 0;
        int cut = x.lo[axis];
        for (int v = x.lo[axis]; v < x.hi[axis]; v++) {
            acc += along[(size_t)v];
            cut = v;
            if (acc * 2 >= x.count) break;
        }
        Box y = x;
        x.hi[axis] = cut;
        y.lo[axis] = cut + 1;
        shrink(x);
        shrink(y);
        if (!y.count) continue;
        if (!x.count) { x = y; continue; }
        boxes.push_back(y);
    }
    std::vector<uint8_t> pal;
    for (const Box& x : boxes) {
        uint64_t s[3] = {0, 0, 0}, c = 0;
        for (int r = x.lo[0]; r <= x.hi[0]; r++)
            for (int g = x.lo[1]; g <= x.hi[1]; g++)
                for (int b = x.lo[2]; b <= x.hi[2]; b++) {
                    const int i = at(r, g, b);
                    c += count[(size_t)i];
                    for (int k = 0; k < 3; k++) s[k] += sum[(size_t)i * 3 + k];
                }
        for (int k = 0; k < 3; k++) pal.push_back((uint8_t)(c ? (s[k] + c / 2) / c : 0));
    }
    return pal;
}

// Variable-width LZW as GIF has it: codes least significant bit first, the
// width growing with the table, a clear code when the table fills.
class Lzw {
public:
    explicit Lzw(int min_code) : _min(min_code) {}
    std::vector<uint8_t> run(const uint8_t* px, size_t n) {
        const int clear = 1 << _min, eoi = clear + 1;
        reset();
        put(clear);
        int cur = -1;
        for (size_t i = 0; i < n; i++) {
            const int c = px[i];
            if (cur < 0) { cur = c; continue; }
            const uint32_t key = ((uint32_t)cur << 8) | (uint32_t)c;
            size_t h = (key * 2654435761u) >> 18;
            while (_keys[h] != UINT32_MAX && _keys[h] != key) h = (h + 1) & (kTable - 1);
            if (_keys[h] == key) { cur = _codes[h]; continue; }
            put(cur);
            _keys[h] = key;
            _codes[h] = ++_max;
            if (_max >= (1 << _width) && _width < 12) _width++;
            if (_max == 4095) {
                put(clear);
                reset();
            }
            cur = c;
        }
        if (cur >= 0) {
            put(cur);
            // The decoder adds an entry on reading that code, which may widen
            // the one after it.
            if (_max + 1 >= (1 << _width) && _width < 12) _width++;
        }
        put(eoi);
        if (_nbits) _out.push_back((uint8_t)_acc);
        return std::move(_out);
    }

private:
    static constexpr size_t kTable = 1 << 14;
    void reset() {
        _keys.assign(kTable, UINT32_MAX);
        _codes.assign(kTable, 0);
        _width = _min + 1;
        _max = (1 << _min) + 1;
    }
    void put(int code) {
        _acc |= (uint32_t)code << _nbits;
        _nbits += _width;
        while (_nbits >= 8) {
            _out.push_back((uint8_t)(_acc & 0xFF));
            _acc >>= 8;
            _nbits -= 8;
        }
    }
    int _min, _width = 0, _max = 0;
    std::vector<uint32_t> _keys;
    std::vector<int> _codes;
    std::vector<uint8_t> _out;
    uint32_t _acc = 0;
    int _nbits = 0;
};

void put16(std::vector<uint8_t>& o, int v) {
    o.push_back((uint8_t)(v & 0xFF));
    o.push_back((uint8_t)((v >> 8) & 0xFF));
}

}  // namespace


std::vector<uint8_t> gif_encode_frame(const uint8_t* rgb, int w, int h, int colours,
                                      int delay_cs) {
    colours = std::clamp(colours, 2, 256);
    // The histogram, from every other pixel each way on a large frame.
    std::vector<uint32_t> count(kBins, 0);
    std::vector<uint64_t> sum((size_t)kBins * 3, 0);
    const int step = (int64_t)w * h > 1000000 ? 2 : 1;
    for (int y = 0; y < h; y += step)
        for (int x = 0; x < w; x += step) {
            const uint8_t* p = rgb + ((size_t)y * w + x) * 3;
            const int i = bin_of(p[0], p[1], p[2]);
            count[(size_t)i]++;
            for (int k = 0; k < 3; k++) sum[(size_t)i * 3 + k] += p[k];
        }
    std::vector<uint8_t> pal = median_cut(count, sum, colours);
    const int used = (int)pal.size() / 3;
    int bits = 1;
    while ((1 << bits) < used) bits++;
    pal.resize((size_t)(1 << bits) * 3, 0);

    // Nearest palette entry for every bin.
    std::vector<uint8_t> lut(kBins);
#pragma omp parallel for schedule(static)
    for (int i = 0; i < kBins; i++) {
        const int c[3] = {((i >> (2 * kBits)) << (8 - kBits)) + 4,
                          (((i >> kBits) & (kSide - 1)) << (8 - kBits)) + 4,
                          ((i & (kSide - 1)) << (8 - kBits)) + 4};
        int best = 0, best_d = 1 << 30;
        for (int j = 0; j < used; j++) {
            const int dr = c[0] - pal[(size_t)j * 3], dg = c[1] - pal[(size_t)j * 3 + 1],
                      db = c[2] - pal[(size_t)j * 3 + 2];
            const int d = 2 * dr * dr + 4 * dg * dg + db * db;
            if (d < best_d) { best_d = d; best = j; }
        }
        lut[(size_t)i] = (uint8_t)best;
    }

    // Ordered dithering by an 8x8 Bayer matrix, about one histogram step.
    static const uint8_t kBayer[64] = {
        0, 48, 12, 60, 3, 51, 15, 63, 32, 16, 44, 28, 35, 19, 47, 31,
        8, 56, 4, 52, 11, 59, 7, 55, 40, 24, 36, 20, 43, 27, 39, 23,
        2, 50, 14, 62, 1, 49, 13, 61, 34, 18, 46, 30, 33, 17, 45, 29,
        10, 58, 6, 54, 9, 57, 5, 53, 42, 26, 38, 22, 41, 25, 37, 21};
    std::vector<uint8_t> idx((size_t)w * h);
#pragma omp parallel for schedule(static)
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const uint8_t* p = rgb + ((size_t)y * w + x) * 3;
            const int o = (int)kBayer[(y & 7) * 8 + (x & 7)] / 4 - 8;
            const int r = std::clamp(p[0] + o, 0, 255), g = std::clamp(p[1] + o, 0, 255),
                      b = std::clamp(p[2] + o, 0, 255);
            idx[(size_t)y * w + x] = lut[(size_t)bin_of(r, g, b)];
        }

    std::vector<uint8_t> out;
    // Graphic control: the delay, and nothing left of the frame before.
    out.insert(out.end(), {0x21, 0xF9, 0x04, 0x04});
    put16(out, std::max(delay_cs, 1));
    out.insert(out.end(), {0x00, 0x00});
    // Image descriptor with a palette of its own.
    out.push_back(0x2C);
    put16(out, 0);
    put16(out, 0);
    put16(out, w);
    put16(out, h);
    out.push_back((uint8_t)(0x80 | (bits - 1)));
    out.insert(out.end(), pal.begin(), pal.end());
    const int min_code = std::max(2, bits);
    out.push_back((uint8_t)min_code);
    Lzw lzw(min_code);
    const std::vector<uint8_t> data = lzw.run(idx.data(), idx.size());
    for (size_t at = 0; at < data.size(); at += 255) {
        const size_t n = std::min<size_t>(255, data.size() - at);
        out.push_back((uint8_t)n);
        out.insert(out.end(), data.begin() + (ptrdiff_t)at, data.begin() + (ptrdiff_t)(at + n));
    }
    out.push_back(0x00);
    return out;
}

int gif_delay_cs(int frame, int frames, double fps) {
    fps = std::max(fps, 1.0);
    auto cs = [&](int k) { return (int)std::lround(100.0 * k / fps); };
    // The frames kept: the first, then each one at least 2/100 s on.
    auto kept_after = [&](int k) {
        int j = k + 1;
        while (j < frames && cs(j) - cs(k) < 2) j++;
        return j;
    };
    int k = 0;
    while (k < frame) k = kept_after(k);
    if (k != frame) return 0;
    return cs(kept_after(frame)) - cs(frame);
}

GifWriter::~GifWriter() {
    if (_f) std::fclose(_f);
}

bool GifWriter::open(const std::string& path, int w, int h) {
    _f = std::fopen(path.c_str(), "wb");
    if (!_f) return false;
    std::vector<uint8_t> head = {'G', 'I', 'F', '8', '9', 'a'};
    put16(head, w);
    put16(head, h);
    head.insert(head.end(), {0x70, 0x00, 0x00});
    // NETSCAPE2.0: loop forever.
    head.insert(head.end(), {0x21, 0xFF, 0x0B, 'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E',
                             '2', '.', '0', 0x03, 0x01, 0x00, 0x00, 0x00});
    _ok = std::fwrite(head.data(), 1, head.size(), _f) == head.size();
    return _ok;
}

bool GifWriter::write(const std::vector<uint8_t>& frame) {
    if (!_f || !_ok) return false;
    _ok = std::fwrite(frame.data(), 1, frame.size(), _f) == frame.size();
    return _ok;
}

bool GifWriter::close() {
    if (!_f) return false;
    const uint8_t trailer = 0x3B;
    _ok = std::fwrite(&trailer, 1, 1, _f) == 1 && _ok;
    _ok = std::fclose(_f) == 0 && _ok;
    _f = nullptr;
    return _ok;
}

}  // namespace gui::render

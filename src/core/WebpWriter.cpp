#include "core/WebpWriter.h"

#include <algorithm>
#include <array>
#include <functional>
#include <queue>
#include <utility>

namespace spirula {
namespace {

// VP8L: https://developers.google.com/speed/webp/docs/webp_lossless_bitstream_specification
class Bits {
public:
    std::vector<uint8_t> bytes;

    void put(uint32_t value, int count) {
        pending_ |= uint64_t(value) << used_;
        used_ += count;
        while (used_ >= 8) {
            bytes.push_back(uint8_t(pending_));
            pending_ >>= 8;
            used_ -= 8;
        }
    }
    void finish() {
        if (used_) put(0, 8 - used_);
    }

private:
    uint64_t pending_ = 0;
    int used_ = 0;
};

uint32_t reverse_bits(uint32_t value, int count) {
    uint32_t result = 0;
    for (int i = 0; i < count; ++i) {
        result = (result << 1) | (value & 1);
        value >>= 1;
    }
    return result;
}

struct Huffman {
    std::vector<int> lengths;
    std::vector<uint32_t> codes;
    int symbols = 0;

    explicit Huffman(const std::vector<uint32_t>& counts, int limit = 15)
        : lengths(counts.size()), codes(counts.size()) {
        std::vector<int> active;
        for (int i = 0; i < int(counts.size()); ++i)
            if (counts[i]) active.push_back(i);
        if (active.empty()) active.push_back(0);
        symbols = int(active.size());
        if (symbols == 1) {
            lengths[active[0]] = 1;
            return;
        }

        // Raising the frequency floor bounds depth without truncating a code tree.
        for (uint64_t floor = 1;; floor *= 2) {
            using Node = std::pair<uint64_t, int>;
            std::priority_queue<Node, std::vector<Node>, std::greater<Node>> heap;
            std::vector<int> parent(counts.size(), -1);
            for (int i : active) heap.push({std::max<uint64_t>(counts[i], floor), i});
            while (heap.size() > 1) {
                auto a = heap.top(); heap.pop();
                auto b = heap.top(); heap.pop();
                int id = int(parent.size());
                parent[a.second] = parent[b.second] = id;
                parent.push_back(-1);
                heap.push({a.first + b.first, id});
            }
            int deepest = 0;
            for (int i : active) {
                int depth = 0;
                for (int p = i; parent[p] != -1; p = parent[p]) ++depth;
                lengths[i] = depth;
                deepest = std::max(deepest, depth);
            }
            if (deepest <= limit) break;
        }
        std::array<uint32_t, 16> count{}, next{};
        for (int n : lengths) if (n) ++count[n];
        for (int n = 1; n <= 15; ++n) next[n] = (next[n - 1] + count[n - 1]) << 1;
        for (int i : active) codes[i] = reverse_bits(next[lengths[i]]++, lengths[i]);
    }

    void emit(Bits& out, int symbol) const {
        if (symbols > 1) out.put(codes[symbol], lengths[symbol]);
    }

    void write(Bits& out) const {
        std::vector<int> active;
        for (int i = 0; i < int(lengths.size()); ++i)
            if (lengths[i]) active.push_back(i);
        if (symbols <= 2 && active.back() < 256) {
            out.put(1, 1);
            out.put(symbols - 1, 1);
            out.put(active[0] > 1, 1);
            out.put(active[0], active[0] > 1 ? 8 : 1);
            if (symbols == 2) out.put(active[1], 8);
            return;
        }

        std::vector<uint32_t> counts(19);
        for (int n : lengths) ++counts[n];
        Huffman tree(counts, 7);
        constexpr int order[] = {17, 18, 0, 1, 2, 3, 4, 5, 16, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
        int n = 19;
        while (n > 4 && tree.lengths[order[n - 1]] == 0) --n;
        out.put(0, 1);
        out.put(n - 4, 4);
        for (int i = 0; i < n; ++i) out.put(tree.lengths[order[i]], 3);
        out.put(0, 1);  // All alphabet entries, including unused symbols.
        for (int length : lengths) tree.emit(out, length);
    }
};

struct Prefix {
    int symbol = 0;
    int bits = 0;
    uint32_t extra = 0;
};

Prefix prefix(uint32_t value) {
    if (value <= 4) return {int(value - 1), 0, 0};
    uint32_t v = value - 1;
    int top = 0;
    for (uint32_t t = v; t >>= 1;) ++top;
    int bits = top - 1;
    int second = int((v >> bits) & 1);
    return {2 * top + second, bits, v - (uint32_t(2 + second) << bits)};
}

struct Token {
    uint32_t pixel;
    uint16_t length;
    uint8_t distance;
};

void write_image(Bits& out, const std::vector<uint32_t>& pixels, int width, bool main_image) {
    std::vector<Token> tokens;
    std::vector<uint32_t> green(280), red(256), blue(256), alpha(256), distance(40);
    for (size_t i = 0; i < pixels.size();) {
        size_t best = 0;
        int best_code = 0;
        // Distances 1 and width cover flat runs and repeated residual rows.
        for (int d : {1, width}) {
            if (i < size_t(d)) continue;
            size_t n = 0, end = std::min<size_t>(4096, pixels.size() - i);
            while (n < end && pixels[i + n] == pixels[i + n - d]) ++n;
            if (n > best) { best = n; best_code = d == 1 ? 2 : 1; }
        }
        if (best >= 3) {
            tokens.push_back({0, uint16_t(best), uint8_t(best_code)});
            ++green[256 + prefix(uint32_t(best)).symbol];
            ++distance[best_code - 1];
            i += best;
        } else {
            uint32_t p = pixels[i++];
            tokens.push_back({p, 0, 0});
            ++green[(p >> 8) & 255];
            ++red[(p >> 16) & 255];
            ++blue[p & 255];
            ++alpha[p >> 24];
        }
    }
    Huffman g(green), r(red), b(blue), a(alpha), d(distance);
    out.put(0, 1);  // No color cache.
    if (main_image) out.put(0, 1);  // One entropy group.
    g.write(out); r.write(out); b.write(out); a.write(out); d.write(out);
    for (const Token& token : tokens) {
        if (token.length) {
            Prefix p = prefix(token.length);
            g.emit(out, 256 + p.symbol);
            out.put(p.extra, p.bits);
            d.emit(out, token.distance - 1);
        } else {
            g.emit(out, (token.pixel >> 8) & 255);
            r.emit(out, (token.pixel >> 16) & 255);
            b.emit(out, token.pixel & 255);
            a.emit(out, token.pixel >> 24);
        }
    }
}

uint32_t predict(uint32_t left, uint32_t top, uint32_t top_left, int mode) {
    if (mode == 1) return left;
    if (mode == 2) return top;
    uint32_t result = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        int l = (left >> shift) & 255, t = (top >> shift) & 255;
        int tl = (top_left >> shift) & 255;
        int v = mode == 7 ? (l + t) / 2 : std::clamp(l + t - tl, 0, 255);
        result |= uint32_t(v) << shift;
    }
    return result;
}

uint32_t residual(const std::vector<uint32_t>& pixels, int width, size_t i, int mode) {
    uint32_t prediction = 0xff000000u;
    if (i && i < size_t(width)) prediction = pixels[i - 1];
    else if (i % width == 0 && i) prediction = pixels[i - width];
    else if (i >= size_t(width))
        prediction = predict(pixels[i - 1], pixels[i - width], pixels[i - width - 1], mode);
    uint32_t result = 0;
    for (int shift = 0; shift < 32; shift += 8)
        result |= uint32_t(uint8_t((pixels[i] >> shift) - (prediction >> shift))) << shift;
    return result;
}

std::vector<uint32_t> transform(std::vector<uint32_t>& pixels, int width, int height) {
    constexpr int block = 32;
    int bw = (width + block - 1) / block, bh = (height + block - 1) / block;
    std::vector<uint32_t> modes(size_t(bw) * bh);
    constexpr int candidates[] = {1, 2, 7, 12};
    for (int by = 0; by < bh; ++by) {
        for (int bx = 0; bx < bw; ++bx) {
            std::array<uint64_t, 4> scores{};
            for (int y = by * block; y < std::min(height, (by + 1) * block); ++y)
                for (int x = bx * block; x < std::min(width, (bx + 1) * block); ++x)
                    for (int c = 0; c < 4; ++c) {
                        uint32_t p = residual(pixels, width, size_t(y) * width + x, candidates[c]);
                        for (int shift = 0; shift < 32; shift += 8) {
                            unsigned v = (p >> shift) & 255;
                            scores[c] += std::min(v, 256 - v);
                        }
                    }
            int best = int(std::min_element(scores.begin(), scores.end()) - scores.begin());
            modes[size_t(by) * bw + bx] = 0xff000000u | (uint32_t(candidates[best]) << 8);
        }
    }
    // Reverse traversal keeps predictor neighbors in their original color space.
    for (size_t i = pixels.size(); i-- > 0;) {
        int x = int(i % width), y = int(i / width);
        int mode = (modes[size_t(y / block) * bw + x / block] >> 8) & 255;
        uint32_t p = residual(pixels, width, i, mode);
        uint8_t green = uint8_t(p >> 8);
        uint8_t red = uint8_t((p >> 16) - green), blue = uint8_t(p - green);
        pixels[i] = (p & 0xff00ff00u) | (uint32_t(red) << 16) | blue;
    }
    return modes;
}

void append_u32(std::vector<uint8_t>& output, uint32_t value) {
    for (int i = 0; i < 4; ++i) output.push_back(uint8_t(value >> (8 * i)));
}

}  // namespace

bool encode_webp_lossless(const uint8_t* pixels, size_t size, int width, int height,
                          int channels, std::vector<uint8_t>& output) {
    output.clear();
    if (!pixels || width < 1 || height < 1 || width > 16384 || height > 16384 ||
        (channels != 3 && channels != 4) || size < size_t(width) * height * channels)
        return false;
    std::vector<uint32_t> argb(size_t(width) * height);
    bool has_alpha = false;
    for (size_t i = 0; i < argb.size(); ++i) {
        const uint8_t* p = pixels + i * channels;
        uint32_t alpha = channels == 4 ? p[3] : 255;
        has_alpha |= alpha != 255;
        argb[i] = (alpha << 24) | (uint32_t(p[0]) << 16) | (uint32_t(p[1]) << 8) | p[2];
    }
    auto modes = transform(argb, width, height);
    Bits out;
    out.put(0x2f, 8);
    out.put(width - 1, 14); out.put(height - 1, 14);
    out.put(has_alpha, 1); out.put(0, 3);
    out.put(1, 1); out.put(0, 2);  // Predictor transform.
    out.put(3, 3);  // 32-pixel predictor blocks.
    write_image(out, modes, (width + 31) / 32, false);
    out.put(1, 1); out.put(2, 2);  // Subtract-green transform.
    out.put(0, 1);
    write_image(out, argb, width, true);
    out.finish();
    uint32_t chunk_size = uint32_t(out.bytes.size());
    output.insert(output.end(), {'R', 'I', 'F', 'F'});
    append_u32(output, 12 + chunk_size + (chunk_size & 1));
    output.insert(output.end(), {'W', 'E', 'B', 'P', 'V', 'P', '8', 'L'});
    append_u32(output, chunk_size);
    output.insert(output.end(), out.bytes.begin(), out.bytes.end());
    if (chunk_size & 1) output.push_back(0);
    return true;
}

}  // namespace spirula

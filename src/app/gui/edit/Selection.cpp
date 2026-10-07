// Selection.cpp -- see Selection.h.

#include "app/gui/edit/Selection.h"

#include <cstddef>
#include <algorithm>

namespace gui {

void Selection::resize(int64_t n) {
    _w.resize((size_t)std::max<int64_t>(n, 0), 0);
    recount();
}

void Selection::recount() {
    int64_t c = 0;
    for (uint8_t v : _w) c += v ? 1 : 0;
    _count = c;
}

void Selection::combine(const uint8_t* in, Combine mode, const uint8_t* alive) {
    const size_t n = _w.size();
    int64_t c = 0;
    for (size_t i = 0; i < n; i++) {
        const uint8_t a = alive && !alive[i] ? 0 : 255;
        const uint8_t s = _w[i], t = (uint8_t)std::min<int>(in[i], a);
        uint8_t r;
        switch (mode) {
            case Combine::Add:       r = std::max(s, t); break;
            case Combine::Subtract:  r = t ? (uint8_t)0 : s; break;
            case Combine::Intersect: r = std::min(s, t); break;
            default:                 r = t; break;
        }
        _w[i] = r;
        c += r ? 1 : 0;
    }
    _count = c;
}

void Selection::set_all(uint8_t w, const uint8_t* alive) {
    int64_t c = 0;
    for (size_t i = 0; i < _w.size(); i++) {
        const uint8_t r = (alive && !alive[i]) ? 0 : w;
        _w[i] = r;
        c += r ? 1 : 0;
    }
    _count = c;
}

void Selection::invert(const uint8_t* alive) {
    int64_t c = 0;
    for (size_t i = 0; i < _w.size(); i++) {
        const uint8_t r = (alive && !alive[i]) ? 0 : (uint8_t)(255 - _w[i]);
        _w[i] = r;
        c += r ? 1 : 0;
    }
    _count = c;
}

void Selection::assign(const std::vector<uint8_t>& w) {
    _w = w;
    recount();
}

void Selection::clear() {
    std::fill(_w.begin(), _w.end(), (uint8_t)0);
    _count = 0;
}


// A run is one value byte followed by a varint length. Selections are mostly
// long runs of 0 and 255, so this is what keeps a history of them affordable.
std::vector<uint8_t> rle_encode(const std::vector<uint8_t>& w) {
    std::vector<uint8_t> out;
    out.reserve(w.size() / 16 + 8);
    size_t i = 0;
    while (i < w.size()) {
        const uint8_t v = w[i];
        size_t j = i + 1;
        while (j < w.size() && w[j] == v) j++;
        size_t run = j - i;
        out.push_back(v);
        while (true) {
            uint8_t b = (uint8_t)(run & 0x7f);
            run >>= 7;
            out.push_back(run ? (uint8_t)(b | 0x80) : b);
            if (!run) break;
        }
        i = j;
    }
    return out;
}

void rle_decode(const std::vector<uint8_t>& rle, std::vector<uint8_t>& out) {
    size_t w = 0;
    size_t p = 0;
    while (p < rle.size() && w < out.size()) {
        const uint8_t v = rle[p++];
        size_t run = 0;
        int shift = 0;
        while (p < rle.size()) {
            const uint8_t b = rle[p++];
            run |= (size_t)(b & 0x7f) << shift;
            shift += 7;
            if (!(b & 0x80)) break;
        }
        run = std::min(run, out.size() - w);
        std::fill(out.begin() + (ptrdiff_t)w, out.begin() + (ptrdiff_t)(w + run), v);
        w += run;
    }
}

}  // namespace gui

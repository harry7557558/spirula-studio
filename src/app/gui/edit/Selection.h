#pragma once

// A set of elements, owned by the document and not by any tool.
//
// A uint8 weight rather than a bit: a soft edge costs the same memory as a
// hard one, and every tool writes here through a combine mode, which is what
// lets "box, minus a brush stroke, intersected with a depth range" work
// without the tools knowing about each other.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace gui {

// How a tool's answer meets what is already selected. Bound to the usual
// modifiers: Shift adds, Ctrl subtracts, both intersect.
enum class Combine { Replace = 0, Add, Subtract, Intersect };
inline constexpr int kNumCombine = 4;

class Selection {
public:
    // Keeps the weights of elements that survive the resize.
    void resize(int64_t n);
    int64_t size() const { return (int64_t)_w.size(); }

    uint8_t weight(int64_t i) const { return _w[(size_t)i]; }
    const uint8_t* data() const { return _w.data(); }
    bool selected(int64_t i) const { return _w[(size_t)i] != 0; }

    // `in` is one weight per element -- a tool's own answer over the whole
    // document. Elements with a zero `alive` flag never end up selected.
    void combine(const uint8_t* in, Combine mode, const uint8_t* alive);
    void set_all(uint8_t w, const uint8_t* alive);
    void invert(const uint8_t* alive);
    void assign(const std::vector<uint8_t>& w);
    void clear();

    // Maintained by the mutators: counting 15M bytes per frame to label a
    // button is not free.
    int64_t count() const { return _count; }
    bool empty() const { return _count == 0; }

    const std::vector<uint8_t>& weights() const { return _w; }

private:
    void recount();

    std::vector<uint8_t> _w;
    int64_t _count = 0;
};

// Run-length encoding of a weight array, for the undo history: a selection is
// one byte per element and a session holds dozens of them.
std::vector<uint8_t> rle_encode(const std::vector<uint8_t>& w);
void rle_decode(const std::vector<uint8_t>& rle, std::vector<uint8_t>& out);

}  // namespace gui

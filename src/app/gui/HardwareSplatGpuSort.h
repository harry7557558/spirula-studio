#pragma once

#include <cstdint>

namespace gui {

// The CPU sort's stable order (HardwareSplatSort.h) from GL 4.3 compute: an LSD
// radix sort of the same 32-bit depth keys, 8 bits a pass, written into the
// caller's draw-order buffer. Positions come from the viewer's RGBA32F array texture.
class HardwareSplatGpuSort {
public:
    // Compute shaders, and storage blocks that hold `count` keys, on the current context.
    static bool supported(uint32_t count);
    // Device bytes beyond the order buffer: two key buffers, a value buffer and digit counts.
    static uint64_t device_bytes(uint32_t count);
    // False, holding nothing, when the buffers or programs cannot be created.
    bool allocate(uint32_t count);
    void sort(unsigned positions, const float view[16], int camera, unsigned order);
    bool allocated() const { return _count != 0; }
    void destroy();
private:
    unsigned _programs[6]{};  // keys + count, count, scan, scatter first / middle / last
    unsigned _keys[2]{}, _values = 0, _counts = 0;
    uint32_t _count = 0, _blocks = 0;
};

}

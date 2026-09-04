// Packed-SH cell addressing for the Vulkan backend, mirroring
// core/Tensor.h's ShQuantAddr and harmonics.slang's sh_cell_at.
// Layouts: docs/notes/sh-quant-layout.md.

#include "int64_compat.slang"

struct ShAddrW {
    gindex_t bounds_stride;
    gindex_t pair_pitch;   // 0 = per-cell-block AoS
    uint cells_per_splat;
};

// `arg_stride` is the launcher's raw sh_bounds_stride; 0 selects FPBO.
[ForceInline]
ShAddrW sh_quant_addr_w(uint num_sh_buffer, gindex_t arg_stride) {
    uint R = 3u * num_sh_buffer;
    ShAddrW a;
    a.cells_per_splat = R;
    if (arg_stride > 0) {
        a.bounds_stride = arg_stride;
        a.pair_pitch = 0;
    } else {
        a.bounds_stride = (gindex_t)max(((R + 1u) >> 1) * 512u, 1u);
        a.pair_pitch = 512;
    }
    return a;
}

// Widened deliberately: 100M splats at SH3 is 4.6e9 cells, so the block
// term overflows if it is formed in u32 first.
[ForceInline]
gindex_t sh_addr_base(ShAddrW a, uint gid) {
    const uint block_cells = ((a.cells_per_splat + 1u) >> 1) * 512u;
    return a.pair_pitch > 0
        ? (gindex_t)(gid >> 8) * (gindex_t)block_cells
          + (gindex_t)((gid & 255u) * 2u)
        : (gindex_t)gid * (gindex_t)a.cells_per_splat;
}

[ForceInline]
gindex_t sh_cell_at_w(gindex_t base, int c, gindex_t pair_pitch) {
    // Branch-free (per cell): AoS folds to (c >> 0) * 1 + (c & 0).
    int pair = int(min(pair_pitch, gindex_t(1)));
    gindex_t pitch = pair_pitch + gindex_t(1 - pair);
    return base + (gindex_t)(c >> pair) * pitch + (gindex_t)(c & pair);
}

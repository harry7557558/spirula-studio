#pragma once

#include <cstdint>

namespace backend { struct MemoryUsage; }

namespace spirula {

// Bytes available for additional preview allocations after reserving the meshing job's growth.
struct MeshPreviewBudget {
    uint64_t host, device;

    bool fits_surface(uint64_t vertices, uint64_t faces) const;
    int splat_limit(int64_t count, int64_t color_coefficients) const;
    int render_pixels(unsigned bytes_per_pixel) const;
};

uint64_t mesh_preview_device_free(const backend::MemoryUsage& memory);
MeshPreviewBudget mesh_preview_budget(uint64_t device_free, double host_growth = 0, double device_growth = 0);

}  // namespace spirula

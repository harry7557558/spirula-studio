#include "app/MeshPreviewBudget.h"

#include "backend/api/BackendRuntime.h"
#include "core/HostMemory.h"

#include <algorithm>
#include <climits>

namespace spirula {

uint64_t mesh_preview_device_free(const backend::MemoryUsage& m) {
    if (!m.has_total || !m.has_used) return 0;
    return m.total_bytes > m.used_bytes ? m.total_bytes - m.used_bytes : 1;
}

MeshPreviewBudget mesh_preview_budget(uint64_t device_free, double host_growth, double device_growth) {
    const auto mem = hostMemory();
    auto spare = [](uint64_t bytes, double growth, uint64_t fallback) {
        // Leave headroom for driver backing, forecast error and other applications.
        const double free = double(bytes ? bytes : fallback) - std::max(0.0, growth);
        return uint64_t(std::max(0.0, free * 0.75 - (16u << 20)));
    };
    const uint64_t host_free = mem.available && mem.commit ? std::min(mem.available, mem.commit)
                                                          : std::max(mem.available, mem.commit);
    const uint64_t host = spare(host_free, host_growth, 512u << 20);
    return {host, std::min(host, spare(device_free, device_growth, 256u << 20))};
}

bool MeshPreviewBudget::fits_surface(uint64_t v, uint64_t f) const {
    // CPU geometry, normals, packed upload and driver copy. Reserve one quarter for framebuffer storage.
    return v <= INT_MAX && f <= INT_MAX / 3 && 128*v + 48*f <= host - host/4 &&
           48*v + 12*f <= device - device/4;
}

int MeshPreviewBudget::splat_limit(int64_t count, int64_t coeffs) const {
    // Includes the retained cloud, upload scratch, GL attributes, sort buffers and driver backing.
    const uint64_t per_host = 256 + 32 * coeffs, per_device = 64 + 8 * coeffs;
    return int(std::min<uint64_t>({uint64_t(count), uint64_t(INT_MAX / std::max<int64_t>(1, coeffs)),
                                  (host - host/4) / per_host, (device - device/4) / per_device}));
}

int MeshPreviewBudget::render_pixels(unsigned bytes_per_pixel) const {
    // The hardware renderer can have colour, depth, resolve and multisample targets live together.
    return int(std::min<uint64_t>(INT_MAX, std::min(host, device) / 4 / bytes_per_pixel));
}

}  // namespace spirula

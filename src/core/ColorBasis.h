#pragma once

// What one row of features_sh holds: the SH bands past the DC, then
// spherical-beta lobes, each two float3 -- (r, g, b) raw amplitude, then
// (theta, phi, beta). The model, its PLY layout and rotation are in
// docs/notes/spherical-beta.md.

#include <cstdint>
#include <stdexcept>
#include <string>

struct ColorBasis {
    // A launch evaluates `sh_degree` bands of the stored ones, which can be
    // more during the SH warmup; the lobes always sit at the row's end.
    int sh_degree = 0;
    int sb_lobes = 0;

    // Bound by the Vulkan kernels' register arrays (proj_bwd_common.slang).
    static constexpr int kMaxShDegree = 4;
    static constexpr int kMaxSbLobes = 8;
    static constexpr int kSbCoeffsPerLobe = 2;

    static constexpr int sh_coeffs_of(int degree) { return degree * (degree + 2); }
    constexpr int sh_coeffs() const { return sh_coeffs_of(sh_degree); }
    constexpr int sb_coeffs() const { return kSbCoeffsPerLobe * sb_lobes; }
    constexpr int coeffs() const { return sh_coeffs() + sb_coeffs(); }

    constexpr bool operator==(const ColorBasis& o) const {
        return sh_degree == o.sh_degree && sb_lobes == o.sb_lobes;
    }
    constexpr bool operator!=(const ColorBasis& o) const { return !(*this == o); }
};

// The lobes of a features_sh row for the per-cell optimizers: every row's
// cells from `first_cell` on step at `lr`, and take the regularizer on their
// amplitude only. first_cell < 0: the rows have none.
struct SbOptimCells {
    int32_t first_cell = -1;
    float lr = 0.0f;
};

// The CUDA backward and optimizer kernels carry no lobes; their launchers
// refuse a request for any (kernels/projection/ProjectionLobes.cu renders them).
inline int sh_only_degree(const ColorBasis& c, const char* what) {
    if (c.sb_lobes != 0)
        throw std::runtime_error(std::string(what) +
                                 ": spherical-beta lobes need the Vulkan backend");
    return c.sh_degree;
}

inline void sh_only_cells(const SbOptimCells& sb, const char* what) {
    if (sb.first_cell >= 0)
        throw std::runtime_error(std::string(what) +
                                 ": spherical-beta lobes need the Vulkan backend");
}

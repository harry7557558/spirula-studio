#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace spirula::sky::sampling {

enum class Border { LastPixel, LastCell };

struct Axis {
    int first, second;
    float weight;
};

inline Axis bracket(float position, int length, Border border) {
    const int limit = std::max(length - (border == Border::LastCell ? 2 : 1), 0);
    const int first = std::clamp((int)std::floor(position), 0, limit);
    return {first, std::min(first + 1, length - 1),
            std::clamp(position - (float)first, 0.0f, 1.0f)};
}

inline std::array<float, 3> rgb(const uint8_t* pixels, int width, Axis x, Axis y) {
    const auto* upper = pixels + ((std::size_t)y.first * width + x.first) * 3;
    const auto* lower = pixels + ((std::size_t)y.second * width + x.first) * 3;
    const int next = (x.second - x.first) * 3;
    std::array<float, 3> value;
    for (int channel = 0; channel != 3; ++channel) {
        const float top = (float)upper[channel] * (1.0f - x.weight) +
                          (float)upper[channel + next] * x.weight;
        const float bottom = (float)lower[channel] * (1.0f - x.weight) +
                             (float)lower[channel + next] * x.weight;
        value[channel] = top * (1.0f - y.weight) + bottom * y.weight;
    }
    return value;
}

}  // namespace spirula::sky::sampling

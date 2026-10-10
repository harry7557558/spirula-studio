#pragma once

#include <cstdint>

namespace spirula {

// Byte ABI shared with shaders/per_pixel_losses.slang; binary masks stay 0/1.
enum class TrainingMask : uint8_t { Segment = 0, Keep = 1, Ignore = 2 };

constexpr uint8_t decode_training_mask(uint8_t value, bool mixed, bool flip) {
    if (!mixed) return (value != 0) != flip;
    const auto kind = value > 250 ? TrainingMask::Keep
                    : value >= 128 ? TrainingMask::Segment : TrainingMask::Ignore;
    if (!flip || kind == TrainingMask::Segment) return (uint8_t)kind;
    return (uint8_t)(kind == TrainingMask::Keep ? TrainingMask::Ignore : TrainingMask::Keep);
}

constexpr uint8_t intersect_training_masks(uint8_t a, uint8_t b) {
    if (a == (uint8_t)TrainingMask::Ignore || b == (uint8_t)TrainingMask::Ignore)
        return (uint8_t)TrainingMask::Ignore;
    return a & b;
}

} // namespace spirula

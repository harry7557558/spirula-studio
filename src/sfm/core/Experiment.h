#pragma once

#include <cerrno>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "core/Env.h"

namespace sfm {

inline int experimentInt(const char* suffix, int fallback, int maximum) {
    const char* value = spirula::env(suffix);
    if (!value) return fallback;
    char* end = nullptr;
    errno = 0;
    const long n = std::strtol(value, &end, 10);
    if (errno || end == value || *end || n < 0 || n > maximum)
        throw std::invalid_argument(std::string("invalid SS_") + suffix);
    return static_cast<int>(n);
}

} // namespace sfm

#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace sfm {

struct Fingerprint {
    uint64_t value = 14695981039346656037ull;

    template <class T>
    void add(const T& v) {
        static_assert(std::is_arithmetic_v<T> || std::is_enum_v<T>);
        const auto* bytes = reinterpret_cast<const unsigned char*>(&v);
        for (size_t i = 0; i < sizeof(v); i++) {
            value ^= bytes[i];
            value *= 1099511628211ull;
        }
    }
};

}  // namespace sfm

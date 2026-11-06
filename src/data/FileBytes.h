// FileBytes.h -- a whole file read into memory.

#pragma once

#include "i18n/catalog/Data.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <new>
#include <stdexcept>
#include <string>
#include <system_error>

namespace spirula {

inline std::runtime_error file_out_of_memory(const std::string& path, std::uintmax_t bytes) {
    char gb[32];
    std::snprintf(gb, sizeof gb, "%.1f", (double)bytes / 1e9);
    return std::runtime_error(i18n::format(i18n::msg::data::file_out_of_memory, {path, gb}));
}

// `who` prefixes the errors. Not sized with ftell: long is 32 bits on MSVC, so
// past 2 GB it returns -1.
inline std::string read_file_bytes(const std::string& path, const char* who) {
    const auto error = [&](const char* what) {
        return std::runtime_error(std::string(who) + ": " + what + " " + path);
    };
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) throw error("cannot open");
    struct Closer { FILE* f; ~Closer() { std::fclose(f); } } closer{f};

    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(path, ec);
    if (ec) throw error("cannot read");
    std::string bytes;
    if (size > bytes.max_size()) throw error("too large for this build to read");   // wasm32
    try {
        bytes.resize((size_t)size);
    } catch (const std::bad_alloc&) {
        throw file_out_of_memory(path, size);
    }
    if (!bytes.empty() && std::fread(&bytes[0], 1, bytes.size(), f) != bytes.size())
        throw error("cannot read");
    return bytes;
}

}  // namespace spirula

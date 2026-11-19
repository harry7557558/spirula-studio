#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace spirula {

inline std::filesystem::path reserve_file_directory(const std::filesystem::path& output) {
    static std::atomic<uint64_t> sequence{0};
    for (int attempt = 0; attempt < 100; attempt++) {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        const std::filesystem::path path = output.string() + ".partial-" + std::to_string(stamp) + "-" +
                                          std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
        std::error_code ec;
        if (std::filesystem::create_directory(path, ec)) return path;
        if (ec && ec != std::errc::file_exists)
            throw std::runtime_error("cannot reserve temporary directory: " + path.string() + ": " + ec.message());
    }
    throw std::runtime_error("cannot reserve temporary directory: " + output.string());
}

inline void replace_file(const std::filesystem::path& temporary, const std::filesystem::path& destination) {
#ifdef _WIN32
    // Windows refuses to replace a file another process has open, and a reader
    // polling it (the GUI, every frame) holds it for microseconds: retried.
    for (int attempt = 0;; ++attempt) {
        if (MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            return;
        const DWORD error = GetLastError();
        if (attempt >= 50 || (error != ERROR_ACCESS_DENIED && error != ERROR_SHARING_VIOLATION))
            throw std::runtime_error("cannot publish " + destination.string() + ": Windows error " + std::to_string(error));
        Sleep(10);
    }
#else
    std::error_code error;
    std::filesystem::rename(temporary, destination, error);
    if (error) throw std::runtime_error("cannot publish " + destination.string() + ": " + error.message());
#endif
}

}  // namespace spirula

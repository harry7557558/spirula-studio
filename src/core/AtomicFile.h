#pragma once

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

#pragma once

#include "core/BufferedFile.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
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
            throw std::filesystem::filesystem_error("cannot publish", destination,
                error == ERROR_NOT_SAME_DEVICE ? std::make_error_code(std::errc::cross_device_link)
                    : std::error_code(static_cast<int>(error), std::system_category()));
        Sleep(10);
    }
#else
    // Atomic replacement does not promise crash durability.
    std::error_code error;
    std::filesystem::rename(temporary, destination, error);
    if (error) throw std::filesystem::filesystem_error("cannot publish", destination, error);
#endif
}

inline std::filesystem::path resolve_output_symlink(const std::filesystem::path& path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const auto status = fs::symlink_status(path, ec);
    if (ec && ec != std::errc::no_such_file_or_directory)
        throw fs::filesystem_error("cannot inspect output", path, ec);
    if (fs::is_symlink(status)) {
        auto target = fs::canonical(path);
        if (!fs::is_regular_file(target))
            throw std::runtime_error("output is not a regular file: " + path.string());
        return target;
    }
    if (fs::exists(status) && !fs::is_regular_file(status))
        throw std::runtime_error("output is not a regular file: " + path.string());
    return path;
}

class AtomicOutputFile {
    struct StagedPath {
        std::filesystem::path path;

        ~StagedPath() {
            std::error_code ec;
            if (!path.empty()) std::filesystem::remove(path, ec);
        }

        void reserve(const std::filesystem::path& base) {
            static std::atomic<uint64_t> counter{0};
#ifdef _WIN32
            const auto pid = GetCurrentProcessId();
#else
            const auto pid = getpid();
#endif
            for (;;) {
                auto candidate = base;
                candidate += "." + std::to_string(pid) + "." +
                    std::to_string(counter.fetch_add(1, std::memory_order_relaxed)) + ".tmp";
#ifdef _WIN32
                const HANDLE file = CreateFileW(candidate.c_str(), GENERIC_WRITE, 0, nullptr,
                                                CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (file != INVALID_HANDLE_VALUE) {
                    path = std::move(candidate);
                    if (CloseHandle(file)) return;
                }
                const DWORD error = GetLastError();
                if (file == INVALID_HANDLE_VALUE && error == ERROR_FILE_EXISTS) continue;
                const std::error_code ec(static_cast<int>(error), std::system_category());
#else
                const int fd = ::open(candidate.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0666);
                if (fd >= 0) {
                    path = std::move(candidate);
                    if (::close(fd) == 0) return;
                }
                const int error = errno;
                if (fd < 0 && error == EEXIST) continue;
                const std::error_code ec(error, std::generic_category());
#endif
                throw std::filesystem::filesystem_error("cannot reserve output staging",
                    path.empty() ? candidate : path, ec);
            }
        }
    };

public:
    explicit AtomicOutputFile(const std::filesystem::path& destination,
                              size_t buffer_bytes = kFileBufferBytes,
                              const std::filesystem::path& staging_parent = {}) {
        namespace fs = std::filesystem;
        const auto absolute = fs::absolute(destination);
        const auto parent = absolute.parent_path();
        fs::create_directories(parent);
        destination_ = fs::canonical(parent) / absolute.filename();
        staging_.reserve((staging_parent.empty() ? destination_.parent_path()
                                                 : fs::canonical(staging_parent)) /
                         destination_.filename());
        stream_.open(staging_.path, std::ios::binary, buffer_bytes);
        if (!stream_) throw std::runtime_error("cannot open output staging: " + staging_.path.string());
    }

    AtomicOutputFile(const AtomicOutputFile&) = delete;
    AtomicOutputFile& operator=(const AtomicOutputFile&) = delete;

    BufferedOutputFile& stream() { return stream_; }

    void close_checked() {
        if (closed_ || !stream_.is_open())
            throw std::logic_error("cannot close output that is not open");
        stream_.flush();
        stream_.close();
        if (!stream_) throw std::runtime_error("write, flush or close failed: " + destination_.string());
        closed_ = true;
    }

    void commit() {
        if (!closed_ || staging_.path.empty() || stream_.is_open())
            throw std::logic_error("cannot publish output without a successful checked close");
        replace_file(staging_.path, destination_);
        staging_.path.clear();
    }

private:
    std::filesystem::path destination_;
    StagedPath staging_;  // Removed after stream destruction, even on Windows.
    BufferedOutputFile stream_;
    bool closed_ = false;
};

inline bool write_file_atomic(const std::filesystem::path& destination, const void* data, size_t size,
                              const std::filesystem::path& staging_parent = {}) noexcept {
    try {
        if (size > static_cast<size_t>((std::numeric_limits<std::streamsize>::max)())) return false;
        AtomicOutputFile file(destination, 0, staging_parent);
        if (size) file.stream().write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
        file.close_checked();
        file.commit();
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace spirula

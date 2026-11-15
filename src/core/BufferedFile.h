#pragma once

#include <cstddef>
#include <fstream>
#include <memory>
#include <type_traits>
#include <utility>

namespace spirula {

inline constexpr size_t kFileBufferBytes = 16u << 20;

namespace detail {
struct FileBufferStorage {
    std::unique_ptr<char[]> bytes;
    size_t capacity = 0;
};
}

// Storage must outlive the stream's filebuf, including its final flush.
template<class Stream>
class BufferedFileStream : private detail::FileBufferStorage, public Stream {
    static constexpr auto default_mode = std::is_same_v<Stream, std::ifstream>
        ? std::ios::in : std::ios::out;
public:
    BufferedFileStream() = default;

    template<class Path>
    explicit BufferedFileStream(const Path& path, std::ios::openmode mode = default_mode,
                                size_t buffer_bytes = kFileBufferBytes) {
        open(path, mode, buffer_bytes);
    }

    BufferedFileStream(BufferedFileStream&& other)
        : detail::FileBufferStorage(std::move(other)), Stream(std::move(other)) {}

    BufferedFileStream& operator=(BufferedFileStream&& other) {
        if (this != &other) {
            Stream::operator=(std::move(other));
            detail::FileBufferStorage::operator=(std::move(other));
        }
        return *this;
    }

    template<class Path>
    void open(const Path& path, std::ios::openmode mode = default_mode,
              size_t buffer_bytes = kFileBufferBytes) {
        const bool already_open = this->is_open();
        if (!already_open && !this->bytes && buffer_bytes) {
            this->bytes.reset(new char[buffer_bytes]);
            this->capacity = buffer_bytes;
        }
        // libstdc++ accepts a buffer before open; MSVC needs the open FILE first.
#ifndef _MSVC_STL_VERSION
        if (!already_open && this->bytes)
            this->rdbuf()->pubsetbuf(this->bytes.get(), (std::streamsize)this->capacity);
#endif
        Stream::open(path, mode);
#ifdef _MSVC_STL_VERSION
        if (!already_open && this->is_open() && this->bytes &&
            !this->rdbuf()->pubsetbuf(this->bytes.get(), (std::streamsize)this->capacity))
            this->setstate(std::ios::badbit);
#endif
    }

    void swap(BufferedFileStream& other) {
        Stream::swap(other);
        this->bytes.swap(other.bytes);
        std::swap(this->capacity, other.capacity);
    }
};

using BufferedInputFile = BufferedFileStream<std::ifstream>;
using BufferedOutputFile = BufferedFileStream<std::ofstream>;

}  // namespace spirula

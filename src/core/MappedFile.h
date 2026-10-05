#pragma once

// A whole file, read-only, memory-mapped where the platform allows and read
// into memory where it does not. A header probe touches only the pages it
// reads, and decode workers share one view.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace spirula {

class MappedFile {
public:
    MappedFile() = default;
    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;
    ~MappedFile() { close(); }

    // "" on success, else one sentence naming the problem. An empty file fails.
    std::string open(const std::string& path);

    const uint8_t* data() const { return _data; }
    size_t size() const { return _size; }

private:
    void close();

    const uint8_t* _data = nullptr;
    size_t _size = 0;
    std::vector<uint8_t> _fallback;
#if defined(_WIN32)
    void* _file = nullptr;
    void* _mapping = nullptr;
#else
    int _fd = -1;
#endif
};

}  // namespace spirula

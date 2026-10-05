#include "core/MappedFile.h"

#include <cstdio>

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <fcntl.h>
#  include <sys/mman.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

namespace spirula {

std::string MappedFile::open(const std::string& path) {
    close();
#if defined(_WIN32)
    HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return "cannot open the file";
    _file = file;
    LARGE_INTEGER sz;
    if (!GetFileSizeEx(file, &sz) || sz.QuadPart <= 0) {
        close();
        return "the file is empty";
    }
    _size = (size_t)sz.QuadPart;
    _mapping = CreateFileMappingA(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (_mapping)
        _data = (const uint8_t*)MapViewOfFile((HANDLE)_mapping, FILE_MAP_READ, 0, 0, 0);
#else
    _fd = ::open(path.c_str(), O_RDONLY);
    if (_fd < 0) return "cannot open the file";
    struct stat st;
    if (fstat(_fd, &st) != 0 || st.st_size <= 0) {
        close();
        return "the file is empty";
    }
    _size = (size_t)st.st_size;
    void* p = mmap(nullptr, _size, PROT_READ, MAP_PRIVATE, _fd, 0);
    if (p != MAP_FAILED) _data = (const uint8_t*)p;
#endif
    if (!_data) {
        _fallback.resize(_size);
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) { close(); return "cannot open the file"; }
        const size_t got = std::fread(_fallback.data(), 1, _size, f);
        std::fclose(f);
        if (got != _size) { close(); return "the file was truncated while reading"; }
        _data = _fallback.data();
    }
    return "";
}

void MappedFile::close() {
    const bool mapped = _data && _fallback.empty();
#if defined(_WIN32)
    if (mapped) UnmapViewOfFile((LPCVOID)_data);
    if (_mapping) CloseHandle((HANDLE)_mapping);
    if (_file) CloseHandle((HANDLE)_file);
    _mapping = nullptr;
    _file = nullptr;
#else
    if (mapped) munmap((void*)_data, _size);
    if (_fd >= 0) ::close(_fd);
    _fd = -1;
#endif
    _data = nullptr;
    _size = 0;
    _fallback.clear();
}

}  // namespace spirula

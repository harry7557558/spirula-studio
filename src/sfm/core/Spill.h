// Read-only arrays moved out of the heap: written to a file in the workspace
// and mapped back, so the OS keeps their pages while memory allows and reads
// them again when it does not, where heap would have to fit. The mapper's pair
// lists and correspondence graph go this way (src/sfm/README.md, "Memory on a
// large capture"). The file is removed as soon as it is mapped where the
// platform allows (POSIX), otherwise when the last holder lets go.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/BufferedFile.h"
#include "core/MappedFile.h"

#if defined(__GLIBC__)
#include <malloc.h>
#endif

namespace sfm {

// Hand freed heap back to the OS. glibc keeps the pages of small chunks freed
// mid-heap: a 12580-image run mapped in 10.8 GB of heap without this, 5.1 with.
inline void releaseFreedHeap() {
#if defined(__GLIBC__)
    malloc_trim(0);
#endif
}

class SpillFile {
public:
    SpillFile(const SpillFile&) = delete;
    SpillFile& operator=(const SpillFile&) = delete;
    ~SpillFile() {
        map_.reset();
        std::error_code ec;
        if (!path_.empty()) std::filesystem::remove(path_, ec);
    }

    // `chunks` back to back; null on any failure, leaving no file behind.
    static std::shared_ptr<const SpillFile> write(
        const std::string& path, const std::vector<std::pair<const void*, size_t>>& chunks) {
        size_t total = 0;
        for (const auto& c : chunks) total += c.second;
        if (total == 0) return nullptr;
        std::shared_ptr<SpillFile> s(new SpillFile);
        s->path_ = path;
        {
            spirula::BufferedOutputFile f(path, std::ios::binary | std::ios::trunc);
            if (!f) return nullptr;
            for (const auto& c : chunks)
                if (!f.write(static_cast<const char*>(c.first), (std::streamsize)c.second))
                    return nullptr;
            f.close();
            if (!f) return nullptr;
        }
        s->map_ = std::make_unique<spirula::MappedFile>();
        if (!s->map_->open(path).empty() || s->map_->size() != total) return nullptr;
#if !defined(_WIN32)
        std::error_code ec;
        if (std::filesystem::remove(path, ec)) s->path_.clear();
#endif
        return s;
    }

    const uint8_t* data() const { return map_->data(); }
    size_t size() const { return map_->size(); }

private:
    SpillFile() = default;
    std::unique_ptr<spirula::MappedFile> map_;
    std::string path_;
};

}  // namespace sfm

#pragma once
// Writes a safetensors file whose layout is planned before any data exists,
// so tensors can be filled in any order -- a converter streaming a remote
// checkpoint writes each as it arrives, and resumes into the same layout.
// Every tensor starts 256-byte aligned; filler entries named "__pad<k>" keep
// the data section hole-free, as the format requires.

#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nn {

class SafetensorsWriter {
public:
    ~SafetensorsWriter();

    void add(const std::string& name, const std::string& dtype, std::vector<int64_t> shape);
    void metadata(const std::string& key, const std::string& value);

    // The header for everything added; stable for a given plan.
    const std::string& header();
    uint64_t fileBytes();
    uint64_t offset(const std::string& name);   // absolute
    uint64_t bytes(const std::string& name);

    // `fresh` writes the header and sizes the file; otherwise an existing file
    // with this very header is reopened to continue it.
    void open(const std::string& path, bool fresh);
    void write(const std::string& name, const void* data, uint64_t n, uint64_t at = 0);
    // Back what an earlier write -- possibly an earlier process -- put there.
    void read(const std::string& name, void* data, uint64_t n, uint64_t at = 0);
    void flush();
    void close();

private:
    struct Planned {
        std::string name, dtype;
        std::vector<int64_t> shape;
        uint64_t begin = 0, end = 0;
    };
    void plan();

    std::vector<Planned> entries_;
    std::vector<std::pair<std::string, std::string>> meta_;
    std::unordered_map<std::string, size_t> index_;
    std::string header_;
    uint64_t data_offset_ = 0;
    bool planned_ = false;
    std::FILE* f_ = nullptr;
    std::string path_;
};

}  // namespace nn

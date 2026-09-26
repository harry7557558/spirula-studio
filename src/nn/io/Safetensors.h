#pragma once
// Just enough of the safetensors format to read a checkpoint: an 8-byte header
// length, a JSON table of {name: {dtype, shape, data_offsets}}, then the raw
// little-endian payloads. Tensors are read one at a time, so a caller that
// uploads and drops each never holds the whole file.

#include "nn/io/Onnx.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace nn {

class SafetensorsFile {
public:
    struct Entry {
        std::string          dtype;   // "F32", "F16", "BF16", "I64", ...
        std::vector<int64_t> shape;
        uint64_t             begin = 0, end = 0;   // relative to the data section
    };

    // Throws nn::Error naming `path` on anything malformed.
    explicit SafetensorsFile(const std::string& path);

    const std::string& path() const { return path_; }
    bool has(const std::string& name) const { return entries_.count(name) != 0; }
    const Entry& entry(const std::string& name) const;
    std::vector<std::string> names() const;

    // Converted to f32 on the host; `was_f16` set for an F16 payload. Throws
    // for a missing name or a dtype that is not a float.
    OnnxTensor read(const std::string& name) const;

private:
    std::string path_;
    uint64_t    data_offset_ = 0;
    uint64_t    file_size_ = 0;
    std::unordered_map<std::string, Entry> entries_;
};

}  // namespace nn

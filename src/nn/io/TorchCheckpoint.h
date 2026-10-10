#pragma once
// Data-only torch.save ZIP checkpoints. Pickle globals are interpreted from a
// fixed tensor/container whitelist; no Python object or callable is executed.

#include "nn/io/Onnx.h"

#include <memory>
#include <string>
#include <vector>

namespace nn {

class TorchCheckpoint {
public:
    struct Entry {
        std::string storage, dtype;
        std::vector<int64_t> shape, stride;
        uint64_t offset = 0, storage_elements = 0;
    };

    explicit TorchCheckpoint(const std::string& path);
    ~TorchCheckpoint();
    TorchCheckpoint(TorchCheckpoint&&) noexcept;
    TorchCheckpoint& operator=(TorchCheckpoint&&) noexcept;
    TorchCheckpoint(const TorchCheckpoint&) = delete;
    TorchCheckpoint& operator=(const TorchCheckpoint&) = delete;

    const std::string& path() const;
    bool has(const std::string& name) const;
    const Entry& entry(const std::string& name) const;
    std::vector<std::string> names() const;
    // A number or list stored beside the weights, by dotted path, e.g.
    // "model_config.refiner.model_channels"; None list entries are NaN. Empty
    // when absent.
    std::vector<double> config(const std::string& key) const;
    OnnxTensor read(const std::string& name) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace nn

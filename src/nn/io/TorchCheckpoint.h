#pragma once
// Data-only torch.save checkpoints: the ZIP format, and the legacy one torch
// wrote before 1.6 (torchvision's vgg16-397923af.pth). Pickle globals are
// interpreted from a fixed tensor/container whitelist; no Python object or
// callable is executed. A checkpoint that is one bare tensor holds it under
// the name "".

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
    // Opens the archive for this one read; a thread reading many tensors keeps a Reader.
    OnnxTensor read(const std::string& name) const;

    // One open archive, for one thread at a time; separate Readers may read concurrently.
    class Reader {
    public:
        explicit Reader(const TorchCheckpoint& checkpoint);
        ~Reader();
        Reader(const Reader&) = delete;
        Reader& operator=(const Reader&) = delete;
        OnnxTensor read(const std::string& name);

    private:
        const TorchCheckpoint& checkpoint_;
        struct Archive;
        std::unique_ptr<Archive> archive_;
    };

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace nn

#include "nn/io/StageDump.h"

#include "core/Env.h"
#include "external/npy.hpp"

#include <filesystem>

namespace nn {

StageDump::StageDump(const char* env_suffix) {
    const char* d = spirula::env(env_suffix);
    if (!d || !d[0]) return;
    std::error_code ec;
    std::filesystem::create_directories(d, ec);
    dir_ = d;
}

void write_npy_f32(const std::string& path, const float* data,
                   const std::vector<int64_t>& shape) {
    npy::shape_t s;
    for (int64_t d : shape) s.push_back((unsigned long)d);
    npy::npy_data_ptr<float> d{data, s, false};
    npy::write_npy(path, d);
}

void StageDump::host(const char* name, const float* data,
                     const std::vector<int64_t>& shape) const {
    if (dir_.empty()) return;
    write_npy_f32(dir_ + "/" + name + ".npy", data, shape);
}

void StageDump::tensor(const char* name, const Tensor& t,
                       const std::vector<int64_t>& shape) const {
    if (dir_.empty()) return;
    std::vector<float> v((size_t)t.numel());
    tensor_to_host(t, v.data(), t.numel());
    host(name, v.data(), shape);
}

bool read_npy_f32(const std::string& path, std::vector<float>& data,
                  std::vector<int64_t>& shape) {
    try {
        const npy::npy_data<float> d = npy::read_npy<float>(path);
        data = d.data;
        shape.assign(d.shape.begin(), d.shape.end());
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

}  // namespace nn

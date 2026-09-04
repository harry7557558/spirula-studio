#include "loma/model/Dump.h"

#include "core/Env.h"
#include "external/npy.hpp"

#include <filesystem>
#include <string>

namespace loma {
namespace {

// Resolved and created once: every dump after the first would otherwise race
// to make the same directory, and npy::write_npy throws rather than creating
// one for itself.
const char* dump_dir() {
    static const char* dir = [] {
        const char* d = spirula::env("LOMA_DUMP");
        if (d) {
            std::error_code ec;
            std::filesystem::create_directories(d, ec);
        }
        return d;
    }();
    return dir;
}

}  // namespace

bool dump_enabled() { return dump_dir() != nullptr; }

void dump_host(const char* name, const float* data, const std::vector<int64_t>& shape) {
    if (!dump_dir()) return;
    npy::shape_t s;
    for (int64_t d : shape) s.push_back((unsigned long)d);
    npy::npy_data_ptr<float> d{data, s, false};
    npy::write_npy(std::string(dump_dir()) + "/" + name + ".npy", d);
}

void dump_tensor(const char* name, const nn::Tensor& t, const std::vector<int64_t>& shape) {
    if (!dump_dir()) return;
    std::vector<float> host((size_t)t.numel());
    nn::tensor_to_host(t, host.data(), t.numel());
    dump_host(name, host.data(), shape);
}

}  // namespace loma

#include "app/GeometryModel.h"
#include "nn/io/Onnx.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
std::string varint(uint64_t value) {
    std::string out;
    do {
        out += static_cast<char>((value & 127) | (value > 127 ? 128 : 0));
        value >>= 7;
    } while (value);
    return out;
}
std::string bytes(int field, const std::string& value) {
    return varint((field << 3) | 2) + varint(value.size()) + value;
}
void save(const std::filesystem::path& path, const std::string& value) {
    std::ofstream out(path, std::ios::binary);
    require(static_cast<bool>(out.write(value.data(), value.size())), "Cannot write model fixture");
}
struct Scratch {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("spirula-model-budget-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Scratch() { require(std::filesystem::create_directory(path), "Cannot create exclusive fixture"); }
    ~Scratch() { std::error_code error; std::filesystem::remove_all(path, error); }
};
}

int main() {
    try {
        Scratch scratch;
        const auto model = scratch.path / "custom.onnx";
        const auto payload = scratch.path / "weights" / "tensor.bin";
        std::filesystem::create_directory(payload.parent_path());
        save(payload, std::string(4, '\0'));
        save(scratch.path / "unrelated.bin", std::string(4096, 'x'));
        const std::string node = bytes(1, bytes(3, "/points_head/test"));
        const std::string tensor = varint(8) + varint(1) + varint(16) + varint(1) + bytes(8, "weight");
        const std::string external = tensor +
            bytes(13, bytes(1, "location") + bytes(2, "weights/tensor.bin")) +
            bytes(13, bytes(1, "length") + bytes(2, "4")) + varint(112) + varint(1);
        save(model, bytes(7, node + bytes(5, external) + bytes(5, external)));
        uint64_t reserved = 0;
        require(app::geometry_model_preflight(model.string(), true, true, reserved) == 1,
                "Custom MoGe graph not identified");
        require(reserved == 16ull * 1024 * 1024 + 2 * (std::filesystem::file_size(model) + 4),
                "Admission counted unrelated files or repeated external references");
        size_t loaded = 0;
        nn::read_onnx_initializers(model.string(), [&](nn::OnnxTensor&& t) {
            require(t.data.size() == 1 && t.data[0] == 0, "Referenced external tensor was not loaded");
            ++loaded;
        });
        require(loaded == 2, "External fixture did not exercise both references");
        std::filesystem::remove(payload);
        bool rejected = false;
        try { app::geometry_model_preflight(model.string(), true, true, reserved); }
        catch (const std::exception&) { rejected = true; }
        require(rejected, "Missing external payload bypassed admission");
        save(model, bytes(7, node + bytes(5, tensor + bytes(9, std::string(4, '\0')))));
        app::geometry_model_preflight(model.string(), true, true, reserved);
        require(reserved == 16ull * 1024 * 1024 + 2 * std::filesystem::file_size(model),
                "Self-contained model counted sibling files");
        std::printf("geometry model admission invariants passed\n");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

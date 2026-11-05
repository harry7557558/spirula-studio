#include "moge/model/Fetch.h"

#include "nn/core/Error.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace moge {
namespace {

// SHA-256 is the LFS object id each file is published under, so a mismatch
// means the download broke rather than that upstream moved.
const ModelSource kSources[] = {
    {"moge2-vits",
     {"moge2-vits-normal.onnx",
      "https://huggingface.co/Ruicheng/moge-2-vits-normal-onnx/resolve/main/model.onnx",
      "24eacb5dc7a2c54c7bc98f7de085ffbed79ad006ea5b664c2c2cdc02ff3a52f0", 140852051ull}},
    {"moge2-vitb",
     {"moge2-vitb-normal.onnx",
      "https://huggingface.co/Ruicheng/moge-2-vitb-normal-onnx/resolve/main/model.onnx",
      "bbf14e07a30f11e69d36ab861590123f5598ababcbc8946a063eb4a966f35a21", 419411850ull}},
    {"moge2-vitl",
     {"moge2-vitl-normal.onnx",
      "https://huggingface.co/Ruicheng/moge-2-vitl-normal-onnx/resolve/main/model.onnx",
      "afbc4ccc3450298f3afb35b90f015f4c4f552dea21dc6470d5f7b78b77e2d751", 1324265014ull}},
    {"moge3-vitl",
     {"moge3-vitl.pt", "https://huggingface.co/Ruicheng/moge-3-vitl/resolve/main/model.pt",
      "9b41b7b9f65ad80aab7ad686f5e9cc0d1fd33f1964022618dfbcd52fc1fb7925", 1481333394ull}},
};

}  // namespace

const ModelSource* find_model_source(const std::string& id) {
    for (const ModelSource& s : kSources)
        if (id == s.id) return &s;
    return nullptr;
}

std::string model_id_list() {
    std::string s;
    for (const ModelSource& m : kSources) {
        if (!s.empty()) s += ", ";
        s += m.id;
    }
    return s;
}

std::string ensure_model(const ModelSource& src) { return nn::ensure_file(src.checkpoint, "moge"); }

std::string resolve_model(const std::string& id_or_path) {
    if (const ModelSource* src = find_model_source(id_or_path)) return ensure_model(*src);

    std::error_code ec;
    NN_CHECK(fs::exists(id_or_path, ec),
             "'%s' is neither a known model id (%s) nor a file that exists",
             id_or_path.c_str(), model_id_list().c_str());
    return id_or_path;
}

}  // namespace moge

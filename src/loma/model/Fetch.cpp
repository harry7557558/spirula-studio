#include "loma/model/Fetch.h"

#include "nn/core/Error.h"
#include "nn/io/Fetch.h"

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace loma {
namespace {

// COLMAP's triples (src/colmap/feature/resources.h); identical on purpose, so
// a parity run compares implementations rather than checkpoints. No bf16
// export: those exist for onnxruntime's provider, and we pick our own dtype.
const ModelSource kSources[] = {
    {"loma-dad",
     {"loma_detector.onnx",
      "https://github.com/davnords/storage/releases/download/loma/loma_detector.onnx",
      "b6af99c5e730034ac9b675d1ebe05d0679af4569a3c26f10a6a50f91e02dc512", 26187830ull}},
    {"loma-dedode-b",
     {"loma_descriptor_dedode_b.onnx",
      "https://github.com/davnords/storage/releases/download/loma/loma_descriptor_dedode_b.onnx",
      "82660a364299013618fe649092ebc4f617559f6a77e1ab5a3412be62a47ddc2d", 53782693ull}},
    {"loma-dedode-g",
     {"loma_descriptor_dedode_g.onnx",
      "https://github.com/davnords/storage/releases/download/loma/loma_descriptor_dedode_g.onnx",
      "5a7b9eaf7425d4513c5d7feae86080bae7ed3aceae7fb1b9f059d0752e2ad564", 1294905349ull}},
    {"loma-b",
     {"loma_matcher_B.onnx",
      "https://github.com/davnords/storage/releases/download/loma/loma_matcher_B.onnx",
      "ba5a2773b29cace19f1240e14e5a080cca3eaf9f69a7adb829a1d470557001c7", 48361254ull}},
    {"loma-b128",
     {"loma_matcher_B128.onnx",
      "https://github.com/davnords/storage/releases/download/loma/loma_matcher_B128.onnx",
      "e71ad490d13713374433a7ef99a7b4f4877d09338e40f347b7e64cc90150ee16", 48430237ull}},
    {"loma-r",
     {"loma_matcher_R.onnx",
      "https://github.com/davnords/storage/releases/download/loma/loma_matcher_R.onnx",
      "6c55247068568861d983b6005d9c401a4c62b8f8ea75d1a6925f13b0211407b5", 48361254ull}},
    {"loma-l",
     {"loma_matcher_L.onnx",
      "https://github.com/davnords/storage/releases/download/loma/loma_matcher_L.onnx",
      "918da5427f37c3d9e05264a58a4d05729f87d459c3d5d69add7ce72eeacea66d", 184354635ull}},
    {"loma-g",
     {"loma_matcher_G.onnx",
      "https://github.com/davnords/storage/releases/download/loma/loma_matcher_G.onnx",
      "62134cde779ed1031982c13c2c1952ed14856d715e97072364821a91f6276cf9", 726335554ull}},
};

}  // namespace

const ModelSource* find_model_source(const std::string& id) {
    for (const ModelSource& s : kSources)
        if (id == s.id) return &s;
    return nullptr;
}

std::string model_cache_path(const ModelSource& src) { return nn::cached_path(src.onnx); }

std::string ensure_model(const ModelSource& src) { return nn::ensure_file(src.onnx, "loma"); }

std::string resolve_model(const std::string& id_or_path, const char* what) {
    if (const ModelSource* src = find_model_source(id_or_path)) return ensure_model(*src);

    std::error_code ec;
    NN_CHECK(fs::exists(id_or_path, ec),
             "LoMa %s: '%s' is neither a known model id nor a file that exists", what,
             id_or_path.c_str());
    return id_or_path;
}

}  // namespace loma

#include "marigold/model/Fetch.h"

#include "marigold/model/Layout.h"
#include "nn/io/Fetch.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace marigold {
namespace {

#define QWEN_HF "https://huggingface.co/Qwen/Qwen-Image-Edit-2509/resolve/main/"
#define QWEN_MS "https://modelscope.cn/models/Qwen/Qwen-Image-Edit-2509/resolve/master/"
#define MV2_HF "https://huggingface.co/huawei-bayerlab/marigold-v2-0/resolve/main/"
#define MV2_MS "https://modelscope.cn/models/huawei-bayerlab/marigold-v2-0/resolve/master/"
#define SHARD(i, sha, n)                                                                  \
    {"transformer/diffusion_pytorch_model-0000" #i "-of-00005.safetensors",               \
     QWEN_HF "transformer/diffusion_pytorch_model-0000" #i "-of-00005.safetensors",       \
     QWEN_MS "transformer/diffusion_pytorch_model-0000" #i "-of-00005.safetensors", sha, n}
#define MV2(path, sha, n) {path, MV2_HF path, MV2_MS path, sha, n}

const std::vector<RemoteFile> kShards = {
    SHARD(1, "784a8efc4dcea09ba20b534bbe9e5fae3367d929525031a4bd26c5ddb7eb18e5", 9973578592ull),
    SHARD(2, "86ce21e5e3bdecfbeaa31aa587c03d0d1af6ab05d79c127149e8217f81e0247f", 9987326072ull),
    SHARD(3, "c6a046daa991f3b32c3ae39ff95e6e7220455265fcbbb643e9f556bea8641d02", 9987307440ull),
    SHARD(4, "c28bf871d5db0a6397843eca3987e58ba2c3013d287d5d4a1271d68e158ce7e8", 9930685712ull),
    SHARD(5, "dd2844e5cafb03b16bed9556aa29c15396a787e17c38a0eda7f5a1281497c76a", 982130472ull),
};

const RemoteFile kVae = {
    "vae/diffusion_pytorch_model.safetensors", QWEN_HF "vae/diffusion_pytorch_model.safetensors",
    QWEN_MS "vae/diffusion_pytorch_model.safetensors",
    "0c8bc8b758c649abef9ea407b95408389a3b2f610d0d10fcb054fe171d0a8344", 253806966ull};

// The default depth model is the paper's (Log-stage2): affine-invariant LOG
// depth. Its prompt context was embedded from real 512 px images.
const RemoteFile kTrainables[2] = {
    MV2("depth/Log-stage2/trainables.safetensors",
        "3edec69490ba8e8fdc8d63e130f93cb93f514360d415a22701250b2535056892", 1853909694ull),
    MV2("normals/trainables.safetensors",
        "b98dfcafa8cf86cf042d8a8e615b8b1a7d7f17c939f1716954c1b57b1b4af15a", 1853909694ull),
};
const RemoteFile kEmbeds[2] = {
    MV2("qwen_text_embeddings/qwen_edit_2509_qwen_depth_realimg512_prompt_embeds.pt",
        "ba752b582571d990c75ad6aac2a2eeb0cf04c2fe01b233b1675028eabf342a6d", 19384278ull),
    MV2("qwen_text_embeddings/qwen_edit_2509_qwen_normals_dummy512_prompt_embeds.pt",
        "411c502723f2aded4265bd9f3f7032bd9f861a317922628d2aaa82424f436217", 2538995ull),
};
const RemoteFile kMasks[2] = {
    MV2("qwen_text_embeddings/qwen_edit_2509_qwen_depth_realimg512_prompt_mask.pt",
        "80973a6521f21a5a6faf6402cae185dafef840d91e654efbeb1097914e634798", 23624ull),
    MV2("qwen_text_embeddings/qwen_edit_2509_qwen_normals_dummy512_prompt_mask.pt",
        "202dcf62ae9963730237226d8df893b0678338ece99a69ef0c9afa894524cd57", 4329ull),
};

}  // namespace

const char* task_name(Task t) { return t == Task::Depth ? "depth" : "normals"; }

const std::vector<RemoteFile>& transformer_shards() { return kShards; }
const RemoteFile& vae_file() { return kVae; }
const RemoteFile& trainables(Task t) { return kTrainables[(int)t]; }
const RemoteFile& prompt_embeds(Task t) { return kEmbeds[(int)t]; }
const RemoteFile& prompt_mask(Task t) { return kMasks[(int)t]; }

std::string install_dir() { return (fs::path(nn::model_cache_dir()) / "marigold-v2").string(); }
std::string base_path() { return (fs::path(install_dir()) / "base.safetensors").string(); }
std::string task_path(Task t) {
    return (fs::path(install_dir()) / (std::string(task_name(t)) + ".safetensors")).string();
}

uint64_t base_install_bytes() {
    uint64_t q4 = 0;
    for (const BlockLinear& l : kBlockLinears) q4 += l.N * l.K;
    // 4 bits a weight plus a bf16 scale per 64, and the encoder.
    return (uint64_t)((double)q4 * kLayers * (0.5 + 2.0 / 64.0)) + (80ull << 20);
}

uint64_t task_install_bytes() {
    uint64_t lora = 0;
    for (const BlockLinear& l : kBlockLinears) lora += (uint64_t)(l.N + l.parts * l.K) * kRank * 2;
    return lora * kLayers + (80ull << 20);
}

}  // namespace marigold

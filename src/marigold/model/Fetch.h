#pragma once
// Where Marigold v2's weights come from, and where the installed form lives.
//
// Nothing here is fetched as a file. The 41 GB of bf16 Qwen-Image-Edit-2509
// shards stream through the converter (model/Install.h), and only the 4-bit
// form ever reaches the disk. Every source is published twice -- Hugging Face,
// and the same bytes on ModelScope -- and the SHA-256 holds both to them.

#include <cstdint>
#include <string>
#include <vector>

namespace marigold {

struct RemoteFile {
    const char* name;      // for messages
    const char* url;       // Hugging Face
    const char* mirror;    // ModelScope
    const char* sha256;    // the LFS object id it is published under
    uint64_t    bytes;
};

enum class Task { Depth, Normals };
const char* task_name(Task t);

const std::vector<RemoteFile>& transformer_shards();
const RemoteFile& vae_file();
const RemoteFile& trainables(Task t);
const RemoteFile& prompt_embeds(Task t);
const RemoteFile& prompt_mask(Task t);

// <cache>/models/marigold-v2/{base,depth,normals}.safetensors
std::string install_dir();
std::string base_path();
std::string task_path(Task t);

// What a finished install occupies, for the free-space check before one.
uint64_t base_install_bytes();
uint64_t task_install_bytes();

// The id `spirula geometry --model` spells.
constexpr const char* kModelId = "marigold-v2";

}  // namespace marigold

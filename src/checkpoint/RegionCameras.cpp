#include "checkpoint/RegionCameras.h"

#include "core/AtomicFile.h"
#include "core/BufferedFile.h"
#include "core/Sha256.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <stdexcept>

namespace spirula {
namespace {
namespace fs = std::filesystem;
constexpr size_t kMetadataLimit = 64u << 20;

uint32_t bits(float value) {
    uint32_t result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

void validate(const RegionCameras& cameras) {
    const auto bad = [] { throw std::runtime_error("invalid partition camera metadata"); };
    const SceneTransform& transform = cameras.train_from_dataset;
    if (!std::isfinite(transform.scale) || transform.scale <= 0) bad();
    for (double value : transform.t) if (!std::isfinite(value)) bad();
    for (double value : transform.R) if (!std::isfinite(value)) bad();
    for (int a = 0; a < 3; a++)
        for (int b = 0; b < 3; b++) {
            double dot = 0;
            for (int k = 0; k < 3; k++) dot += transform.R[a * 3 + k] * transform.R[b * 3 + k];
            if (std::abs(dot - (a == b ? 1.0 : 0.0)) > 1e-5) bad();
        }
    const double* r = transform.R;
    const double determinant = r[0] * (r[4] * r[8] - r[5] * r[7]) -
                               r[1] * (r[3] * r[8] - r[5] * r[6]) +
                               r[2] * (r[3] * r[7] - r[4] * r[6]);
    if (determinant <= 0) bad();
    const auto& field = cameras.field;
    if (field.seeds.size() > kMetadataLimit / sizeof(float) ||
        field.nodes.size() > kMetadataLimit / sizeof(float) - field.seeds.size()) bad();
    if (field.seeds.empty() || field.nodes.empty() || field.seeds.size() % 4 || field.nodes.size() % 8) bad();
    std::vector<uint8_t> nodes((size_t)field.num_nodes()), seeds((size_t)field.num_seeds());
    std::vector<std::pair<size_t, int>> pending = {{0, 0}};
    while (!pending.empty()) {
        const auto [index, depth] = pending.back();
        pending.pop_back();
        if (depth >= 47 || index >= nodes.size() || nodes[index]++) bad();
        const float* node = field.nodes.data() + index * 8;
        for (int k = 0; k < 3; k++)
            if (!std::isfinite(node[k]) || !std::isfinite(node[k + 4]) || node[k] > node[k + 4]) bad();
        const size_t first = bits(node[3]), count = bits(node[7]);
        if (count) {
            if (first >= seeds.size() || count > seeds.size() - first) bad();
            for (size_t i = first; i < first + count; i++) {
                if (seeds[i]++) bad();
                const float* seed = field.seeds.data() + i * 4;
                if (bits(seed[3]) != LabelField::pack(0, nullptr)) bad();
                for (int k = 0; k < 3; k++)
                    if (!std::isfinite(seed[k]) || seed[k] < node[k] || seed[k] > node[k + 4]) bad();
            }
        } else {
            if (first >= nodes.size() || nodes.size() - first < 2) bad();
            for (size_t child : {first, first + 1}) {
                const float* bounds = field.nodes.data() + child * 8;
                for (int k = 0; k < 3; k++)
                    if (bounds[k] < node[k] || bounds[k + 4] > node[k + 4]) bad();
                pending.emplace_back(child, depth + 1);
            }
        }
    }
    if (std::find(nodes.begin(), nodes.end(), 0) != nodes.end() ||
        std::find(seeds.begin(), seeds.end(), 0) != seeds.end()) bad();
}

std::string hash_bytes(const std::string& bytes) {
    Sha256 hash;
    hash.update(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
    return hash.hex();
}

std::string read_blob(const fs::path& path) {
    std::error_code error;
    const uintmax_t size = fs::file_size(path, error);
    if (error) throw std::runtime_error("cannot read partition camera metadata; keep its companion blob beside the PLY: " + path.string());
    if (size > kMetadataLimit) throw std::runtime_error("partition camera metadata exceeds 64 MiB: " + path.string());
    BufferedInputFile file(path, std::ios::binary);
    std::string bytes((size_t)size, '\0');
    if (!file.read(bytes.data(), (std::streamsize)bytes.size()) || file.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("cannot read partition camera metadata: " + path.string());
    return bytes;
}

fs::path blob_path(const fs::path& directory, const std::string& hash) {
    return directory / ("roi-cameras-" + hash + ".bin");
}
}  // namespace

bool valid_region_camera_hash(const std::string& hash) {
    return hash.size() == 64 && std::all_of(hash.begin(), hash.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

std::string save_region_cameras(const std::string& directory, const RegionCameras& cameras) {
    validate(cameras);
    std::string bytes = "SSRC";
    const uint32_t version = 1;
    auto append = [&](const auto& value) { bytes.append(reinterpret_cast<const char*>(&value), sizeof(value)); };
    append(version);
    append(cameras.train_from_dataset.scale);
    for (double value : cameras.train_from_dataset.R) append(value);
    for (double value : cameras.train_from_dataset.t) append(value);
    cameras.field.write(bytes);
    if (bytes.size() > kMetadataLimit) throw std::runtime_error("partition camera metadata exceeds 64 MiB");
    const std::string hash = hash_bytes(bytes);
    const auto destination = blob_path(directory, hash);
    if (fs::exists(destination)) {
        if (read_blob(destination) != bytes) throw std::runtime_error("partition camera metadata hash mismatch: " + destination.string());
        return hash;
    }
    const auto temporary = reserve_file_directory(destination);
    try {
        const auto file_path = temporary / "metadata.bin";
        BufferedOutputFile file(file_path, std::ios::binary);
        file.write(bytes.data(), (std::streamsize)bytes.size());
        file.close();
        if (!file) throw std::runtime_error("cannot write partition camera metadata: " + file_path.string());
        replace_file(file_path, destination);
        fs::remove(temporary);
    } catch (...) {
        std::error_code ignored;
        fs::remove_all(temporary, ignored);
        throw;
    }
    return hash;
}

RegionCameras read_region_cameras(const std::string& ply, const std::string& hash) {
    if (!valid_region_camera_hash(hash)) throw std::runtime_error("invalid partition camera metadata reference in " + ply);
    const auto path = blob_path(fs::path(ply).parent_path(), hash);
    const std::string bytes = read_blob(path);
    if (hash_bytes(bytes) != hash) throw std::runtime_error("partition camera metadata hash mismatch: " + path.string());
    if (bytes.size() < 112 || bytes.compare(0, 4, "SSRC") != 0)
        throw std::runtime_error("invalid partition camera metadata: " + path.string());
    size_t at = 4;
    auto take = [&](auto& value) {
        std::memcpy(&value, bytes.data() + at, sizeof(value));
        at += sizeof(value);
    };
    uint32_t version;
    take(version);
    if (version != 1) throw std::runtime_error("unsupported partition camera metadata version");
    RegionCameras result;
    take(result.train_from_dataset.scale);
    for (double& value : result.train_from_dataset.R) take(value);
    for (double& value : result.train_from_dataset.t) take(value);
    size_t consumed = 0;
    if (!LabelField::read(bytes.data() + at, bytes.size() - at, consumed, result.field) || consumed != bytes.size() - at)
        throw std::runtime_error("invalid camera BVH in " + path.string());
    validate(result);
    return result;
}

}  // namespace spirula

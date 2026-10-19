#include "app/FocusMaskMerge.h"

#include "core/AtomicFile.h"
#include "core/Sha256.h"
#include "data/DatasetParser.h"
#include "data/Json.h"
#include "data/JsonWrite.h"
#include "external/stb_image.h"
#include "nn/io/Image.h"

#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace {

fs::path under(const fs::path& root, const fs::path& relative) {
    if (relative.is_absolute() || relative.empty()) throw std::runtime_error("invalid focus mask path");
    const fs::path base = fs::weakly_canonical(root);
    const fs::path result = fs::weakly_canonical(base / relative);
    auto b = base.begin(), r = result.begin();
    for (; b != base.end(); ++b, ++r)
        if (r == result.end() || *r != *b) throw std::runtime_error("focus mask path leaves dataset");
    return result;
}

std::string hash_if_present(const fs::path& file) {
    if (!fs::exists(file)) return {};
    const std::string hash = spirula::sha256_file(file.string());
    if (hash.empty()) throw std::runtime_error("cannot read mask: " + file.string());
    return hash;
}

void save_record(const app::FocusMaskTarget& target) {
    JsonWriter writer;
    writer.object().field("version", 1).field("output", fs::relative(target.output, target.root).generic_string())
        .field("original", target.original.empty() ? std::string() : fs::relative(target.original, target.root).generic_string())
        .field("current_hash", target.current_hash)
        .field("pending_hash", target.pending_hash).end();
    fs::create_directories(target.record.parent_path());
    fs::path temporary = target.record;
    temporary += ".part";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        stream << writer.str();
        stream.close();
        if (!stream) throw std::runtime_error("cannot save mask backup record");
    }
    spirula::replace_file(temporary, target.record);
}

}  // namespace

namespace app {

FocusMaskTarget prepare_focus_mask(const fs::path& root, const fs::path& masks,
                                   const std::string& relative) {
    FocusMaskTarget target;
    target.root = fs::weakly_canonical(root);
    under(target.root, fs::relative(masks, target.root));
    const fs::path state = root / ".spirula-focus";
    target.record = under(state / "records", relative + ".json");
    const std::string found = dsparse::find_aux_file(masks.string(), relative, "mask");
    fs::path default_name(relative);
    default_name.replace_extension(".png");
    target.output = found.empty() ? under(masks, default_name) : fs::weakly_canonical(found);
    target.current_hash = hash_if_present(target.output);
    if (fs::exists(target.record)) {
        const JsonValue record = json_parse_file(target.record.string());
        auto text = [&](const char* key) {
            const auto* value = record.find(key);
            if (!value || value->type != JsonValue::Type::String)
                throw std::runtime_error("invalid focus backup record: " + target.record.string());
            return value->str;
        };
        const fs::path previous_output = under(target.root, text("output"));
        const std::string previous_hash = text("current_hash"), pending = text("pending_hash");
        if (target.output == previous_output &&
            (target.current_hash == previous_hash || (!pending.empty() && target.current_hash == pending))) {
            const std::string original = text("original");
            if (!original.empty()) {
                target.original = under(target.root, original);
                const auto rel = fs::relative(target.original, state / "original_masks");
                target.original = under(state / "original_masks", rel);
                if (!fs::is_regular_file(target.original)) throw std::runtime_error("original mask backup is missing");
                if (spirula::sha256_file(target.original.string()) != rel.begin()->string())
                    throw std::runtime_error("original mask backup was modified");
            }
            return target;
        }
    }
    if (!target.current_hash.empty()) {
        const fs::path rel = fs::relative(target.output, masks);
        target.original = under(state / "original_masks" / target.current_hash, rel);
        fs::create_directories(target.original.parent_path());
        if (!fs::exists(target.original)) fs::copy_file(target.output, target.original);
        if (hash_if_present(target.original) != target.current_hash)
            throw std::runtime_error("original mask backup verification failed");
    }
    save_record(target);
    return target;
}

void write_focus_mask(FocusMaskTarget& target, std::vector<uint8_t>& weight, int width, int height) {
    if (weight.size() != (size_t)width * height) throw std::runtime_error("focus mask size mismatch");
    if (!target.original.empty()) {
        int w = 0, h = 0, channels = 0;
        if (stbi_is_16_bit(target.original.string().c_str())) {
            auto* original = stbi_load_16(target.original.string().c_str(), &w, &h, &channels, 1);
            if (!original) throw std::runtime_error("cannot decode original mask");
            if (w != width || h != height) {
                stbi_image_free(original);
                throw std::runtime_error("original mask and raw image sizes differ");
            }
            for (size_t k = 0; k < weight.size(); ++k)
                weight[k] = (uint8_t)(((uint32_t)weight[k] * original[k] + 32767) / 65535);
            stbi_image_free(original);
        } else {
            auto* original = stbi_load(target.original.string().c_str(), &w, &h, &channels, 1);
            if (!original) throw std::runtime_error("cannot decode original mask");
            if (w != width || h != height) {
                stbi_image_free(original);
                throw std::runtime_error("original mask and raw image sizes differ");
            }
            for (size_t k = 0; k < weight.size(); ++k)
                weight[k] = (uint8_t)(((uint32_t)weight[k] * original[k] + 127) / 255);
            stbi_image_free(original);
        }
    }
    fs::create_directories(target.output.parent_path());
    fs::path temporary = target.output;
    temporary += ".focus-part.png";
    if (!nn::save_gray_png(weight.data(), width, height, temporary.string()))
        throw std::runtime_error("cannot write focus mask");
    if (hash_if_present(target.output) != target.current_hash)
        throw std::runtime_error("mask changed during processing; refusing to overwrite: " + target.output.string());
    target.pending_hash = hash_if_present(temporary);
    save_record(target);
    spirula::replace_file(temporary, target.output);
    target.current_hash = target.pending_hash;
    target.pending_hash.clear();
    save_record(target);
}

}  // namespace app

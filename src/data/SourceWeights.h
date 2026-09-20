#pragma once

#include "data/Json.h"
#include "data/JsonWrite.h"
#include "data/DatasetParser.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace spirula { namespace source_weights {

struct Source {
    std::string key;
    int32_t camera_id = 0;
    std::string model;
    uint64_t width = 0, height = 0;
    std::set<std::string> legacy_folders;
    int train_count = 0;
    int image_count = 0;
};

struct Groups {
    std::vector<Source> sources;
    std::vector<int32_t> image_group;
};

inline Groups group_images(const std::string& dataset, const ParsedDataset& ds) {
    namespace fs = std::filesystem;
    const fs::path root = fs::absolute(fs::u8path(dataset)).lexically_normal();
    if (ds.source_camera_ids.size() != ds.image_filenames.size())
        throw std::runtime_error("source_weights: missing source camera ids");
    Groups out;
    std::map<int32_t, int> ids;
    for (int32_t camera : ds.source_camera_ids) ids.emplace(camera, 0);
    for (auto& [camera, id] : ids) {
        const auto& info = ds.source_cameras.at(camera);
        id = (int)out.sources.size();
        Source source;
        source.key = "camera:" + std::to_string(camera);
        source.camera_id = camera;
        source.model = info.model;
        source.width = info.width;
        source.height = info.height;
        out.sources.push_back(std::move(source));
    }
    for (size_t i = 0; i < ds.image_filenames.size(); ++i) {
        const int id = ids.at(ds.source_camera_ids[i]);
        out.image_group.push_back(id);
        ++out.sources[id].image_count;
        const auto parent = fs::absolute(fs::u8path(ds.image_filenames[i])).lexically_normal().parent_path();
        auto relative = parent.lexically_relative(root);
        out.sources[id].legacy_folders.insert((relative.empty() ? parent : relative).generic_u8string());
    }
    for (int32_t i : ds.train_indices) ++out.sources.at(out.image_group.at(i)).train_count;
    return out;
}

struct Stage {
    int until = 100;
    std::map<std::string, float> weights;

    float weight(const std::string& path) const {
        auto it = weights.find(path);
        return it == weights.end() ? 1.0f : it->second;
    }
};

using Schedule = std::vector<Stage>;

inline Schedule parse(const std::string& text) {
    if (text.empty()) return {{70, {}}, {100, {}}};
    const JsonValue root = json_parse(text);
    if (!root.is_array() || root.arr.empty() || root.arr.size() > 100)
        throw std::runtime_error("source_weights: expected 1..100 stages");
    Schedule out;
    int previous = 0;
    for (const auto& value : root.arr) {
        const auto* until = value.find("until");
        const auto* weights = value.find("weights");
        if (!until || until->type != JsonValue::Type::Number ||
            !std::isfinite(until->num) || until->num <= previous ||
            until->num > 100 || std::floor(until->num) != until->num ||
            !weights || !weights->is_object())
            throw std::runtime_error("source_weights: stages need increasing integer until percentages and a weights object");
        Stage stage;
        stage.until = (int)until->num;
        previous = stage.until;
        for (const auto& [folder, weight] : weights->obj) {
            const float w = (float)weight.as_double(-1.0);
            if (folder.empty() || !std::isfinite(w) || w < 0.0f ||
                !stage.weights.emplace(folder, w).second)
                throw std::runtime_error("source_weights: source weights must be unique, finite and nonnegative");
        }
        out.push_back(std::move(stage));
    }
    if (out.back().until != 100)
        throw std::runtime_error("source_weights: the final stage must end at 100 percent");
    return out;
}

inline std::string serialize(const Schedule& stages) {
    JsonWriter out;
    out.array();
    for (const auto& stage : stages) {
        out.object().field("until", stage.until).key("weights").object();
        for (const auto& [path, weight] : stage.weights) out.field(path.c_str(), weight);
        out.end().end();
    }
    out.end();
    return out.str();
}

inline Schedule resolve(const std::string& text, const Groups& groups) {
    Schedule stages = parse(text);
    for (auto& stage : stages) {
        const auto old = stage.weights;
        stage.weights.clear();
        for (const auto& source : groups.sources) {
            auto saved = old.find(source.key);
            if (saved != old.end()) {
                stage.weights[source.key] = saved->second;
                continue;
            }
            float weight = 1.0f;
            bool first = true;
            for (const auto& folder : source.legacy_folders) {
                auto it = old.find(folder);
                const float w = it == old.end() ? 1.0f : it->second;
                if (!first && w != weight)
                    throw std::runtime_error("source_weights: this camera spans folders with different saved weights; reset the schedule and set camera weights");
                weight = w;
                first = false;
            }
            stage.weights[source.key] = weight;
        }
    }
    return stages;
}

inline void validate(const Schedule& stages, const Groups& groups) {
    for (const auto& stage : stages) {
        bool active = false;
        for (const auto& source : groups.sources)
            active |= source.train_count > 0 && stage.weight(source.key) > 0.0f;
        if (!active)
            throw std::runtime_error("source_weights: each stage needs a positive weight on a camera group with training images");
    }
}

}}  // namespace spirula::source_weights

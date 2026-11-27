#pragma once

#include "data/JsonWrite.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>

namespace spirula {

struct ImageColor {
    std::string gamut;
    std::optional<bool> linear;
};

using ImageColors = std::map<std::string, ImageColor>;

// Keys are folders relative to the image root. A deeper folder wins.
inline const ImageColor* image_color_for(const ImageColors& colors,
                                         const std::filesystem::path& relative) {
    if (relative.empty() || relative.has_root_path() || *relative.begin() == "..") return nullptr;
    for (auto dir = relative.parent_path(); !dir.empty() && dir != "."; dir = dir.parent_path()) {
        if (const auto it = colors.find(dir.generic_string()); it != colors.end())
            return &it->second;
    }
    const auto it = colors.find(".");
    return it == colors.end() ? nullptr : &it->second;
}

inline void write_image_color(JsonWriter& w, const ImageColor& color) {
    w.object().field("gamut", color.gamut);
    if (color.linear) w.field("linear", *color.linear);
    w.end();
}

inline ImageColor read_image_color(const JsonValue& value) {
    if (!value.is_object()) throw std::runtime_error("expected an image color object");
    ImageColor color;
    if (const auto* gamut = value.find("gamut")) {
        if (gamut->type != JsonValue::Type::String)
            throw std::runtime_error("image gamut must be text");
        color.gamut = gamut->str;
    }
    if (const auto* linear = value.find("linear")) {
        if (linear->type != JsonValue::Type::Bool)
            throw std::runtime_error("image linear must be true or false");
        color.linear = linear->b;
    }
    return color;
}

inline std::string write_image_colors(const ImageColors& colors) {
    JsonWriter w;
    w.object();
    for (const auto& [folder, color] : colors) {
        w.key(folder.c_str());
        write_image_color(w, color);
    }
    w.end();
    return w.str();
}

inline ImageColors read_image_colors(const JsonValue& value) {
    if (!value.is_object()) throw std::runtime_error("expected image colors by folder");
    ImageColors colors;
    for (const auto& [folder, color] : value.obj) {
        auto dir = std::filesystem::path(folder.empty() ? "." : folder).lexically_normal();
        if (dir.filename().empty()) dir = dir.parent_path();
        if (dir.has_root_path() || *dir.begin() == "..")
            throw std::runtime_error("image color folders must be relative to the image root");
        colors[dir.generic_string()] = read_image_color(color);
    }
    return colors;
}

}  // namespace spirula

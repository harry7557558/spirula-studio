#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace spirula {

inline std::optional<bool> mip_from_ply_comment(std::string_view line) {
    constexpr std::string_view whitespace = " \t\r";
    const auto start = line.find_first_not_of(whitespace);
    if (start == std::string_view::npos) return std::nullopt;
    line.remove_prefix(start);
    const auto key_end = line.find_first_of(whitespace);
    if (key_end == std::string_view::npos || line.substr(0, key_end) != "comment")
        return std::nullopt;
    const auto value_start = line.find_first_not_of(whitespace, key_end);
    if (value_start == std::string_view::npos) return std::nullopt;
    std::string value(line.substr(value_start, line.find_last_not_of(whitespace) - value_start + 1));
    for (char& ch : value)
        if (ch >= 'A' && ch <= 'Z') ch = char(ch - 'A' + 'a');

    const auto setting = [&](std::string_view prefix) -> std::string_view {
        const std::string_view text(value);
        if (text.substr(0, prefix.size()) != prefix) return {};
        const auto pos = text.find_first_not_of(whitespace, prefix.size());
        return pos == std::string_view::npos ? std::string_view{} : text.substr(pos);
    };
    const auto brush = setting("splatrendermode:");
    const auto postshot = setting("postshot.anti_aliasing=");
    if (brush == "mip" || postshot == "1" || value == "antialiased 1") return true;
    if (brush == "default" || brush == "2dgs" || postshot == "0" || value == "antialiased 0")
        return false;
    return std::nullopt;
}

}  // namespace spirula

#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace spirula {

inline std::filesystem::path seed_count_file(std::filesystem::path path) {
    std::error_code ec;
    if (!std::filesystem::is_directory(path, ec)) return path;
    return path / (std::filesystem::is_regular_file(path / "points3D.bin", ec)
        ? "points3D.bin" : "points3D.txt");
}

inline int64_t seed_point_count(const std::filesystem::path& seed) {
    const auto path = seed_count_file(seed);
    std::ifstream in(path, std::ios::binary);
    if (!in) return -1;
    if (path.filename() == "points3D.bin") {
        uint64_t count = 0;
        return in.read(reinterpret_cast<char*>(&count), sizeof count) && count <= INT64_MAX
            ? static_cast<int64_t>(count) : -1;
    }
    if (path.filename() == "points3D.txt") {
        int64_t count = 0;
        for (std::string line; std::getline(in, line);) {
            const auto first = line.find_first_not_of(" \t\r");
            if (first != std::string::npos && line[first] != '#') ++count;
        }
        return count;
    }
    std::string line;
    if (!std::getline(in, line) || (line != "ply" && line != "ply\r")) return -1;
    int64_t count = -1;
    while (std::getline(in, line)) {
        if (line == "end_header" || line == "end_header\r") return count;
        std::istringstream fields(line);
        std::string element, vertex;
        if (fields >> element >> vertex; element == "element" && vertex == "vertex") {
            if (!(fields >> count) || count < 0) return -1;
        }
    }
    return -1;
}

}  // namespace spirula

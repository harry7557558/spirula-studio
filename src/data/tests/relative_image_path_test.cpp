// relative_image_path_test -- image names carrying "./" or "../" (images.txt,
// images.bin, transforms.json file_path) must resolve to the same image, and
// to that image's mask, as the plain name does.

#include "data/DatasetParser.h"

#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

template <typename T>
void put(std::string& o, T v) {
    o.append(reinterpret_cast<const char*>(&v), sizeof v);
}

void touch(const fs::path& p) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << "x";
}

void write_colmap(const fs::path& model, const std::vector<std::string>& names,
                  bool text) {
    fs::create_directories(model);
    if (text) {
        std::ofstream(model / "cameras.txt") << "1 PINHOLE 640 480 500 500 320 240\n";
        std::ofstream im(model / "images.txt");
        for (size_t i = 0; i < names.size(); i++)
            im << i + 1 << " 1 0 0 0 " << i << " 0 5 1 " << names[i] << "\n\n";
        return;
    }
    std::string c;
    put<uint64_t>(c, 1);
    put<int32_t>(c, 1); put<int32_t>(c, 1);
    put<uint64_t>(c, 640); put<uint64_t>(c, 480);
    for (double v : {500.0, 500.0, 320.0, 240.0}) put<double>(c, v);
    std::ofstream(model / "cameras.bin", std::ios::binary) << c;
    std::string im;
    put<uint64_t>(im, names.size());
    for (size_t i = 0; i < names.size(); i++) {
        put<int32_t>(im, (int32_t)i + 1);
        for (double v : {1.0, 0.0, 0.0, 0.0, (double)i, 0.0, 5.0}) put<double>(im, v);
        put<int32_t>(im, 1);
        im += names[i];
        im.push_back('\0');
        put<uint64_t>(im, 0);
    }
    std::ofstream(model / "images.bin", std::ios::binary) << im;
}

void write_nerfstudio(const fs::path& dir, const std::vector<std::string>& names) {
    std::ofstream j(dir / "transforms.json");
    j << "{\"camera_model\": \"PINHOLE\", \"w\": 640, \"h\": 480,"
         " \"fl_x\": 500, \"fl_y\": 500, \"cx\": 320, \"cy\": 240, \"frames\": [";
    for (size_t i = 0; i < names.size(); i++) {
        std::string escaped;
        for (char ch : names[i]) {
            if (ch == '\\') escaped += '\\';
            escaped += ch;
        }
        j << (i ? "," : "") << "{\"file_path\": \"" << escaped
          << "\", \"transform_matrix\": [[1,0,0," << i
          << "],[0,1,0,0],[0,0,1,0],[0,0,0,1]]}";
    }
    j << "]}";
}

// Every frame must land on images/<stem>.png with masks/<stem>.png beside it.
void expect(const ParsedDataset& ds, const fs::path& root,
            const std::vector<std::string>& stems, const std::string& label) {
    check(ds.num_cameras == (int64_t)stems.size(), label + ": frame count");
    if (ds.num_cameras != (int64_t)stems.size()) return;
    check(ds.mask_filenames.size() == stems.size(), label + ": masks found");
    for (size_t i = 0; i < stems.size(); i++) {
        const fs::path img = root / "images" / (stems[i] + ".png");
        const fs::path mask = root / "masks" / (stems[i] + ".png");
        check(fs::path(ds.image_filenames[i]) == img,
              label + ": image " + ds.image_filenames[i]);
        if (i < ds.mask_filenames.size())
            check(fs::path(ds.mask_filenames[i]) == mask,
                  label + ": mask " + ds.mask_filenames[i]);
    }
}

}  // namespace

int main() {
    const fs::path base = fs::temp_directory_path() / "ss_relative_image_path_test";
    std::error_code ec;
    fs::remove_all(base, ec);

    const std::vector<std::string> stems = {"a", "b", "c", "d"};
    // Relative to images/, the way COLMAP writes names, then relative to the
    // dataset root, the way some exporters do.
    std::vector<std::vector<std::string>> colmap_names = {
        {"a.png", "./b.png", "../images/c.png", "x/../d.png"},
        {"images/a.png", "./images/b.png", "images/./c.png", "images/x/../d.png"},
    };
    std::vector<std::vector<std::string>> nerfstudio_names = {
        {"images/a.png", "./images/b.png", "../nerfstudio0/images/c.png",
         "images/x/../d.png"},
    };
#ifdef _WIN32
    // A separator there, and only there; elsewhere it is part of the name.
    colmap_names.push_back({"a.png", ".\\b.png", "..\\images\\c.png", "x\\..\\d.png"});
    colmap_names.push_back({"images\\a.png", ".\\images\\b.png", "images\\.\\c.png",
                            "images\\x\\..\\d.png"});
    nerfstudio_names.push_back({"images\\a.png", ".\\images\\b.png",
                                "..\\nerfstudio1\\images\\c.png",
                                "images\\x\\..\\d.png"});
#endif

    DatasetParserConfig cfg;
    int run = 0;
    for (bool text : {true, false}) {
        for (const auto& names : colmap_names) {
            const fs::path root = base / ("colmap" + std::to_string(run++));
            for (const auto& s : stems) {
                touch(root / "images" / (s + ".png"));
                touch(root / "masks" / (s + ".png"));
            }
            write_colmap(root / "sparse" / "0", names, text);
            const std::string label =
                std::string(text ? "images.txt " : "images.bin ") + names[1];
            try {
                expect(parse_colmap_dataset(root.string(), cfg), root,
                       stems, label);
            } catch (const std::exception& e) {
                check(false, label + ": " + e.what());
            }
        }
    }

    for (size_t k = 0; k < nerfstudio_names.size(); k++) {
        const fs::path root = base / ("nerfstudio" + std::to_string(k));
        for (const auto& s : stems) {
            touch(root / "images" / (s + ".png"));
            touch(root / "masks" / (s + ".png"));
        }
        write_nerfstudio(root, nerfstudio_names[k]);
        const std::string label = "transforms.json " + nerfstudio_names[k][1];
        try {
            expect(parse_nerfstudio_dataset(root.string(), cfg), root,
                   stems, label);
        } catch (const std::exception& e) {
            check(false, label + ": " + e.what());
        }
    }

    fs::remove_all(base, ec);
    std::printf("%s\n", g_failures ? "FAILED" : "all passed");
    return g_failures ? 1 : 0;
}

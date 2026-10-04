// recompute_files_test -- the files "Recompute Sparse Point Cloud" swaps in a
// model folder: originals kept once, a later export told apart from a recompute,
// and restore putting back exactly what was there. Scratch folder, no SfM run.

#include "app/gui/RecomputeFiles.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;
using namespace gui;

namespace {

int g_failures = 0;

void expect(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) g_failures++;
}

void put(const fs::path& p, const std::string& text) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary | std::ios::trunc) << text;
}

std::string read(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return "<none>";
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// What a --poses run leaves in its sparse/0.
fs::path made(const fs::path& root, const std::string& tag) {
    const fs::path d = root / ("made_" + tag);
    put(d / "images.bin", "images " + tag);
    put(d / "points3D.bin", "points " + tag);
    return d;
}

}  // namespace

int main() {
    const fs::path root = fs::temp_directory_path() / "spirula_recompute_files_test";
    remove_dir_tree(root);
    const fs::path model = root / "sparse";
    put(model / "cameras.bin", "cameras");
    put(model / "images.bin", "images export");
    put(model / "points3D.bin", "points export");

    expect(!holds_recompute(model), "an export holds no recompute");
    expect(install_recomputed(model, made(root, "1")).empty(), "first recompute installs");
    expect(read(model / "points3D.bin") == "points 1" && read(model / "images.bin") == "images 1",
           "... its points and images are in place");
    expect(read(model / "points3D.bin_original") == "points export" &&
               read(model / "images.bin_original") == "images export",
           "... and the export's are kept as *_original");
    expect(read(model / "cameras.bin") == "cameras", "... cameras.bin untouched");
    expect(holds_recompute(model), "... which restore can undo");

    expect(install_recomputed(model, made(root, "2")).empty(), "a second recompute installs");
    expect(read(model / "points3D.bin") == "points 2", "... its points are in place");
    expect(read(model / "points3D.bin_original") == "points export",
           "... and the first originals stay");

    put(model / "images.bin", "images export, again");
    put(model / "points3D.bin", "points export, again");
    expect(!holds_recompute(model), "an export over a recompute is not one");
    expect(install_recomputed(model, made(root, "3")).empty(), "recomputing it installs");
    expect(read(model / "points3D.bin_original") == "points export, again",
           "... and the new export becomes the original");

    expect(restore_original(model).empty(), "restore");
    expect(read(model / "points3D.bin") == "points export, again" &&
               read(model / "images.bin") == "images export, again",
           "... puts the originals back");
    expect(!fs::exists(model / "points3D.bin_original") && !holds_recompute(model),
           "... and leaves no originals or recompute behind");

    // An export with cameras and poses only.
    const fs::path bare = root / "bare";
    put(bare / "cameras.bin", "cameras");
    put(bare / "images.bin", "images bare");
    expect(install_recomputed(bare, made(root, "4")).empty() &&
               install_recomputed(bare, made(root, "5")).empty(),
           "a model with no points recomputes twice");
    expect(read(bare / "points3D.bin") == "points 5" && !fs::exists(bare / "points3D.bin_original"),
           "... the recomputed points are never taken for originals");
    expect(restore_original(bare).empty() && !fs::exists(bare / "points3D.bin") &&
               read(bare / "images.bin") == "images bare",
           "... and restore leaves it without points again");

    remove_dir_tree(root);
    expect(!fs::exists(root), "remove_dir_tree removes the lot");

    std::printf(g_failures ? "\nFAILED: %d\n" : "\nall passed\n", g_failures);
    return g_failures ? 1 : 0;
}

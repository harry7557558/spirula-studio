// StencilPreset.cpp -- see StencilPreset.h.

#include "app/gui/StencilPreset.h"

#include "app/FrameMaskSvg.h"
#include "app/gui/PresetFile.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <set>

namespace fs = std::filesystem;

namespace gui {

namespace {

// A name as a file stem: preset_file_name's spelling without its extension.
std::string stem_for(const std::string& name) {
    return fs::path(preset_file_name(name)).stem().string();
}

// Every set in `dir`: a file naming no camera on its own, and the files
// naming one grouped by title.
std::vector<StencilPreset> list_sets(const fs::path& dir) {
    std::vector<StencilPreset> out;
    std::map<std::string, size_t> by_title;
    std::vector<fs::path> files;
    std::error_code ec;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
        if (it->is_regular_file(ec) && it->path().extension() == ".svg") files.push_back(it->path());
    std::sort(files.begin(), files.end());
    for (const fs::path& f : files) {
        std::vector<app::MaskShape> shapes;
        std::string title, camera, err;
        if (!app::load_mask_svg(f.string(), shapes, title, err, &camera)) continue;
        const std::string name = title.empty() ? f.stem().string() : title;
        if (camera.empty()) {
            out.push_back({name, f.string(), {}});
            continue;
        }
        const auto [it, fresh] = by_title.emplace(name, out.size());
        if (fresh) out.push_back({name, f.string(), {}});
        out[it->second].cameras.push_back(camera);
    }
    for (StencilPreset& p : out) std::sort(p.cameras.begin(), p.cameras.end());
    std::sort(out.begin(), out.end(), [](const StencilPreset& a, const StencilPreset& b) {
        return a.name != b.name ? a.name < b.name : a.cameras.size() < b.cameras.size();
    });
    return out;
}

// `set` into `dir` as <stem>.svg, or <stem>-<camera>.svg per camera with
// shapes, all titled `title`. Appends the files written.
bool write_set(const fs::path& dir, const std::string& stem, const app::MaskSet& set,
               const std::string& title, std::vector<std::string>& written, std::string& error) {
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (!set.per_camera()) {
        const fs::path f = dir / (stem + ".svg");
        if (!app::save_mask_svg(f.string(), set.shapes, title, error)) return false;
        written.push_back(f.string());
        return true;
    }
    for (const auto& [camera, shapes] : set.cameras) {
        if (shapes.empty()) continue;
        const fs::path f = dir / (stem + "-" + stem_for(camera) + ".svg");
        if (!app::save_mask_svg(f.string(), shapes, title, error, camera)) return false;
        written.push_back(f.string());
    }
    return true;
}

// Whether any file `write_set` would make for `stem` is there already.
bool stem_taken(const fs::path& dir, const std::string& stem, const app::MaskSet& set) {
    std::error_code ec;
    if (!set.per_camera()) return fs::exists(dir / (stem + ".svg"), ec);
    for (const auto& [camera, shapes] : set.cameras)
        if (fs::exists(dir / (stem + "-" + stem_for(camera) + ".svg"), ec)) return true;
    return false;
}

}  // namespace

std::string stencil_preset_label(const StencilPreset& p) {
    if (p.cameras.empty()) return p.name;
    std::string cams;
    for (const std::string& c : p.cameras) cams += (cams.empty() ? "" : ", ") + c;
    return p.name + " [" + cams + "]";
}

std::string stencil_preset_dir() {
    // The sibling of the dataset presets that name these.
    const fs::path d = fs::path(preset_dir(PresetKind::Dataset)).parent_path() / "stencil";
    std::error_code ec;
    fs::create_directories(d, ec);
    return d.string();
}

std::vector<StencilPreset> list_stencil_presets() { return list_sets(stencil_preset_dir()); }

bool load_stencil_preset(const std::string& name_or_path, app::MaskSet& out,
                         std::string& error) {
    std::string path;
    for (const StencilPreset& p : list_stencil_presets())
        if (path.empty() && p.name == name_or_path) path = p.path;
    std::error_code ec;
    if (path.empty() && fs::is_regular_file(name_or_path, ec)) path = name_or_path;
    if (path.empty()) {
        const fs::path by_file = fs::path(stencil_preset_dir()) / (stem_for(name_or_path) + ".svg");
        if (fs::is_regular_file(by_file, ec)) path = by_file.string();
    }
    if (path.empty()) {
        error = name_or_path;
        return false;
    }
    std::string title;
    return app::load_mask_svg_set(path, out, title, error);
}

bool save_stencil_preset(const std::string& name, const app::MaskSet& set, std::string& path,
                         std::string& error) {
    path.clear();
    const fs::path dir = stencil_preset_dir();
    std::error_code ec;
    // Every file of the old set goes, so a set drawn once per camera and saved
    // again as one list leaves no stale camera file to be read back.
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->path().extension() != ".svg") continue;
        std::vector<app::MaskShape> shapes;
        std::string title, err;
        if (app::load_mask_svg(it->path().string(), shapes, title, err) &&
            (title.empty() ? it->path().stem().string() : title) == name)
            fs::remove(it->path(), ec);
    }
    std::vector<std::string> written;
    if (!write_set(dir, stem_for(name), set, name, written, error)) return false;
    if (!written.empty()) path = written.front();
    return true;
}

std::vector<std::string> save_dataset_stencils(
    const std::string& workspace, const std::vector<std::pair<std::string, app::MaskSet>>& inputs,
    std::string& error) {
    std::vector<std::string> written;
    const fs::path dir = fs::path(workspace) / kDatasetStencilDir;
    std::error_code ec;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
        if (it->path().extension() == ".svg") fs::remove(it->path(), ec);
    const std::string dataset = fs::path(workspace).filename().string();
    std::set<std::string> used;
    for (const auto& [input, set] : inputs) {
        if (set.empty()) continue;
        // The input's own name; a folder called images/ says little, so its parent's too.
        fs::path in = fs::path(input);
        if (!in.has_filename()) in = in.parent_path();
        std::string stem = in.stem().string();
        if (stem == "images" && in.has_parent_path()) stem = in.parent_path().filename().string() + "-" + stem;
        const std::string base = stem_for(stem);
        std::string title = dataset + " - " + in.filename().string();
        std::string file = base;
        // Two inputs of one name get two stems, and two titles, or their
        // camera files would load back as one set.
        for (int k = 2; used.count(file) || stem_taken(dir, file, set); k++) {
            file = base + "-" + std::to_string(k);
            title = dataset + " - " + in.filename().string() + " (" + std::to_string(k) + ")";
        }
        used.insert(file);
        if (!write_set(dir, file, set, title, written, error)) return written;
    }
    return written;
}

std::vector<StencilPreset> list_dataset_stencils(const std::string& workspace) {
    if (workspace.empty()) return {};
    return list_sets(fs::path(workspace) / kDatasetStencilDir);
}

}  // namespace gui

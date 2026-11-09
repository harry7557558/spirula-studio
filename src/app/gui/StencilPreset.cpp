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

std::string set_name(const fs::path& file, const std::string& title) {
    return title.empty() ? file.stem().string() : title;
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
        const std::string name = set_name(f, title);
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

bool replace_set(const fs::path& dir, const std::string& name, const app::MaskSet& set,
                 std::string& path, std::string& error) {
    path.clear();
    std::error_code ec;
    // Every file of the old set goes, so a set drawn once per camera and saved
    // again as one list leaves no stale camera file to be read back.
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->path().extension() != ".svg") continue;
        std::vector<app::MaskShape> shapes;
        std::string title, err;
        if (app::load_mask_svg(it->path().string(), shapes, title, err) &&
            set_name(it->path(), title) == name)
            fs::remove(it->path(), ec);
    }
    std::vector<std::string> written;
    if (!write_set(dir, stem_for(name), set, name, written, error)) return false;
    if (!written.empty()) path = written.front();
    return true;
}

// What the list calls `p`, sorted as the list reads them: its file, or every
// camera's.
std::vector<fs::path> files_of(const StencilPreset& p) {
    if (p.cameras.empty()) return {p.path};
    std::vector<fs::path> files;
    std::error_code ec;
    const fs::path dir = fs::path(p.path).parent_path();
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->path().extension() != ".svg") continue;
        std::vector<app::MaskShape> shapes;
        std::string title, camera, err;
        if (app::load_mask_svg(it->path().string(), shapes, title, err, &camera) &&
            !camera.empty() && set_name(it->path(), title) == p.name)
            files.push_back(it->path());
    }
    std::sort(files.begin(), files.end());
    return files;
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
    return replace_set(stencil_preset_dir(), name, set, path, error);
}

bool update_stencil_preset(StencilPreset& p, const app::MaskSet& set, std::string& error) {
    std::string path;
    if (!replace_set(fs::path(p.path).parent_path(), p.name, set, path, error)) return false;
    p.path = path;
    p.cameras.clear();
    for (const auto& [camera, shapes] : set.cameras)
        if (!shapes.empty()) p.cameras.push_back(camera);
    return true;
}

bool delete_stencil_preset(const StencilPreset& p, std::string& error) {
    std::error_code ec;
    for (const fs::path& f : files_of(p)) {
        fs::remove(f, ec);
        if (ec) {
            error = f.string() + ": " + ec.message();
            return false;
        }
    }
    return true;
}

StencilRename rename_stencil_preset(StencilPreset& p, const std::string& name, std::string& error) {
    if (name == p.name) return StencilRename::Done;
    const fs::path dir = fs::path(p.path).parent_path();
    for (const StencilPreset& other : list_sets(dir))
        if (other.name == name) return StencilRename::NameTaken;

    struct Moved {
        fs::path to;
        std::string camera;
        std::vector<app::MaskShape> shapes;
    };
    const std::vector<fs::path> old = files_of(p);
    if (old.empty()) {
        error = p.path;
        return StencilRename::Failed;
    }
    const std::string stem = stem_for(name);
    std::vector<Moved> moved;
    std::error_code ec;
    for (const fs::path& f : old) {
        Moved m;
        std::string title;
        if (!app::load_mask_svg(f.string(), m.shapes, title, error, &m.camera))
            return StencilRename::Failed;
        m.to = dir / (stem + (m.camera.empty() ? "" : "-" + stem_for(m.camera)) + ".svg");
        // Names differing only in case or punctuation share a file; the set's
        // own may be overwritten, another's may not.
        if (fs::exists(m.to, ec) && std::find(old.begin(), old.end(), m.to) == old.end())
            return StencilRename::NameTaken;
        moved.push_back(std::move(m));
    }
    for (const Moved& m : moved)
        if (!app::save_mask_svg(m.to.string(), m.shapes, name, error, m.camera))
            return StencilRename::Failed;
    for (const fs::path& f : old)
        if (std::none_of(moved.begin(), moved.end(), [&](const Moved& m) { return m.to == f; }))
            fs::remove(f, ec);
    p.name = name;
    p.path = moved.front().to.string();
    return StencilRename::Done;
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

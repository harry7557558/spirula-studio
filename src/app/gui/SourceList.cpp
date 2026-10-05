// SourceList.cpp -- see SourceList.h.

#include "app/gui/SourceList.h"
#include "app/gui/RigGuess.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <filesystem>

namespace fs = std::filesystem;

namespace gui {

namespace {

// A file or folder name that can be a directory of its own: what a path
// separator, a colon or a space would do to `--camera-model DIR=MODEL` is not
// worth finding out.
std::string sanitize_name(std::string s) {
    for (char& c : s) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
        if (!ok) c = '_';
    }
    while (!s.empty() && s.front() == '.') s.erase(s.begin());
    return s.empty() ? std::string("input") : s;
}

// Folder names that say what is inside rather than which capture it is.
// `/lab/images` and `/lab/omni/images` are two different inputs whose own
// names are both "images", so the folder ABOVE is what tells them apart.
bool is_generic_folder_name(std::string n) {
    for (char& c : n) c = (char)std::tolower((unsigned char)c);
    return n == "images" || n == "image" || n == "img" || n == "imgs" ||
           n == "photos" || n == "pictures" || n == "pics" || n == "frames" ||
           n == "input" || n == "inputs" || n == "data";
}

// Names that mean something else inside a dataset: `<ws>/images/images` is
// the layout `sfm auto`'s nested-images shorthand exists for, and taking that
// shorthand drops every other input from the reconstruction.
bool is_reserved_dataset_name(std::string n) {
    for (char& c : n) c = (char)std::tolower((unsigned char)c);
    return n == "images" || n == "masks" || n == "sparse" || n == "features" ||
           n == "depths" || n == "normals" || n == "colmap" || n == "outputs";
}

// The sub-folder one input's frames go into, before de-duplication. It climbs
// past a folder whose name only describes its contents, so `X/images` and
// `Y/images` come out as `X` and `Y`, not `images` and `images_2`.
std::string source_folder_base(const PrepInput& s) {
    fs::path p(s.path);
    if (s.is_video) return sanitize_name(p.stem().string());
    if (!p.empty() && p.filename().empty()) p = p.parent_path();  // trailing '/'
    for (int up = 0; up < 2 && !p.empty(); up++) {
        std::string n = p.filename().string();
        if (n.empty()) break;
        if (!is_generic_folder_name(n)) return sanitize_name(n);
        p = p.parent_path();
    }
    // Nothing but generic names all the way up: keep the leaf, but never as a
    // name the dataset layout already uses.
    fs::path leaf(s.path);
    if (!leaf.empty() && leaf.filename().empty()) leaf = leaf.parent_path();
    std::string n = sanitize_name(leaf.filename().string());
    return is_reserved_dataset_name(n) ? n + "_input" : n;
}

// Never point at an existing non-empty directory (e.g. a previous run) --
// append _2, _3, ... instead of overwriting.
std::string fresh_workspace(const std::string& base) {
    std::error_code ec;
    if (!fs::exists(base, ec) || fs::is_empty(base, ec)) return base;
    for (int i = 2; i < 1000; i++) {
        std::string cand = base + "_" + std::to_string(i);
        if (!fs::exists(cand, ec) || fs::is_empty(cand, ec)) return cand;
    }
    return base;
}

// Is this folder the `images/` of a dataset folder, rather than a folder of
// photos that happens to hold them? (resolve_photo_folder is what put us here.)
bool named_images(const fs::path& p) {
    std::string n = p.filename().empty() ? p.parent_path().filename().string()
                                         : p.filename().string();
    for (char& c : n) c = (char)std::tolower((unsigned char)c);
    return n == "images";
}

}  // namespace


std::string default_lens(const PrepInput& s) {
    return has_fisheye_lens(s) ? "thin-prism-fisheye" : "opencv";
}


void set_packed_lenses(PrepInput& s, int width, int height) {
    if (!is_packed_lens_path(s.path) || s.packed_lenses > 0 || width <= 0 ||
        height <= 0)
        return;
    bool exact = false;
    s.packed_lenses = app::packed_lens_count(width, height, exact);
    if (s.packed_lenses >= 2) s.rig = kRigOwn;
}


PrepInput make_source(const std::string& path, bool use_found_masks) {
    std::error_code ec;
    PrepInput s;
    s.path = path;
    s.is_video = !fs::is_directory(path, ec) && is_video_path(path);
    // Resolved on the path the user picked, so the row shows the folder that
    // will actually be indexed rather than one that merely contains it.
    if (!s.is_video) {
        resolve_photo_folder(path, s.path, s.mask_dir);
        if (!use_found_masks) s.mask_dir.clear();
        s.packed_lenses = probe_packed_lenses(s.path);
        if (s.packed_lenses >= 2) s.rig = kRigOwn;
        s.heif = folder_has_heif(s.path);
    }
    s.camera_model = default_lens(s);
    return s;
}


void probe_sources(std::vector<PrepInput>& sources,
                   const std::string& ffmpeg_exe) {
    static const std::atomic<bool> never{false};
    // What a .360 actually holds: the extraction plan and the lens both follow
    // from the packing, not from the extension.
    for (PrepInput& s : sources) {
        if (!s.is_video || s.pano360.valid() || !is_pano360_path(s.path)) continue;
        const Pano360Probe p = probe_pano360(ffmpeg_exe, s.path, never);
        s.pano360 = p.layout;
        s.pano360_unsupported = p.unsupported;
    }
    // A file with several lenses starts as a rig of its own; the row can
    // still say otherwise.
    for (PrepInput& s : sources) {
        if (!s.is_video || s.video_tracks > 0) continue;
        s.video_tracks = std::max(1, probe_video_tracks(ffmpeg_exe, s.path, never));
        if (s.pano360.valid() || s.video_tracks >= 2) s.rig = kRigOwn;
    }
    for (PrepInput& s : sources) {
        int w = 0, h = 0;
        if (s.is_video && s.packed_lenses == 0 && is_packed_lens_path(s.path) &&
            source_pixel_size(s, ffmpeg_exe, w, h))
            set_packed_lenses(s, w, h);
    }
}


void assign_source_subdirs(std::vector<PrepInput>& sources) {
    // One input keeps the layout a one-video dataset has always had: frames
    // straight into images/. Several need a folder each, which is also what
    // makes them separate cameras.
    std::vector<std::string> taken;
    for (PrepInput& s : sources) {
        if (sources.size() < 2) {
            s.subdir.clear();
            continue;
        }
        std::string base = source_folder_base(s);
        if (is_reserved_dataset_name(base)) base += "_input";
        std::string name = base;
        for (int n = 2; std::find(taken.begin(), taken.end(), name) != taken.end();
             n++)
            name = base + "_" + std::to_string(n);
        taken.push_back(name);
        s.subdir = name;
    }
}


void refresh_subcameras(std::vector<PrepInput>& sources) {
    for (PrepInput& s : sources) {
        std::vector<std::string> found;
        if (!s.is_video && !s.path.empty()) {
            std::error_code ec;
            if (fs::is_directory(s.path, ec)) found = camera_subfolders(s.path);
        }
        // One folder is a nested layout, not a choice to make.
        if (found.size() < 2) {
            s.subcameras.clear();
            continue;
        }
        std::vector<SubCamera> next;
        next.reserve(found.size());
        for (const std::string& rel : found) {
            SubCamera sc;
            sc.rel = rel;
            // Empty means "same as the row above", so a folder nobody has
            // seen starts on the lens its input's kind suggests -- inheriting
            // a 360 file's fisheye is what ordinary photos must not do.
            sc.camera_model = s.camera_model;
            for (const SubCamera& old : s.subcameras)
                if (old.rel == rel) sc = old;
            next.push_back(std::move(sc));
        }
        s.subcameras.swap(next);
    }
}


void guess_source_rigs(std::vector<PrepInput>& sources, bool force) {
    const std::vector<CameraGroup> groups = camera_groups(sources);
    std::vector<size_t> rows;
    for (size_t i = 0; i < groups.size(); i++) {
        const PrepInput& in = sources[groups[i].input];
        // Lenses the input's own frames hold are its rig already.
        if (in.is_video || in.packed_lenses >= 2) continue;
        const int rig = group_rig(sources, groups[i]);
        if (!force && rig != kRigNone) return;
        rows.push_back(i);
    }
    if (rows.size() < 2) return;

    std::vector<RigCandidate> folders;
    for (size_t i : rows) {
        const CameraGroup& g = groups[i];
        const PrepInput& in = sources[g.input];
        RigCandidate c;
        c.name = g.rel.empty() ? fs::path(in.path).filename().string() : g.rel;
        fs::path dir(in.path);
        if (g.sub >= 0) dir /= in.subcameras[(size_t)g.sub].rel;
        std::error_code ec;
        const auto opts = fs::directory_options::follow_directory_symlink |
                          fs::directory_options::skip_permission_denied;
        // A subcamera's images sit directly in its folder; an input's may be
        // nested, and matching is by path relative to it, as the rig's is.
        for (fs::recursive_directory_iterator it(dir, opts, ec), end;
             !ec && it != end; it.increment(ec)) {
            if (g.sub >= 0) it.disable_recursion_pending();
            if (!it->is_regular_file(ec) || !is_image_file(it->path())) continue;
            fs::path rel = it->path().lexically_relative(dir);
            c.images.push_back(rel.replace_extension().generic_string());
        }
        std::sort(c.images.begin(), c.images.end());
        c.images.erase(std::unique(c.images.begin(), c.images.end()), c.images.end());
        folders.push_back(std::move(c));
    }

    const std::vector<int> rig = guess_rigs(folders);
    for (size_t k = 0; k < rows.size(); k++) {
        int& r = group_rig(sources, groups[rows[k]]);
        if (rig[k] >= 0 && rig[k] < kRigShared) r = kRigFirstShared + rig[k];
        else if (force && r >= kRigFirstShared) r = kRigNone;
    }
}


namespace {

std::string workspace_base(const std::vector<PrepInput>& sources, bool& exact) {
    exact = false;
    if (sources.empty()) return {};
    std::string base;
    if (sources.size() == 1) {
        const fs::path p(sources[0].path);
        if (sources[0].is_video) {
            base = (p.parent_path() / (p.stem().string() + "_dataset")).string();
        } else if (named_images(p) && !sources[0].heif) {
            // A dataset folder: images/ (and masks/) are already where every
            // parser looks for them, so the reconstruction belongs beside them
            // as sparse/ -- in that folder, not in a copy of it with a suffix.
            base = p.parent_path().string();
            exact = true;
        } else if (named_images(p)) {
            // ... unless they are HEIC, whose JPEGs need an images/ of their own.
            base = p.parent_path().string() + "_dataset";
        } else {
            base = sources[0].path + "_dataset";
        }
    } else {
        // Several inputs have no single name; the folder they came from is
        // the closest thing to one.
        const fs::path dir = fs::path(sources[0].path).parent_path();
        base = (dir / (dir.filename().string() + "_dataset")).string();
    }
    return base;
}

}  // namespace

std::string default_workspace(const std::vector<PrepInput>& sources) {
    // Normally a folder of its own, suffixed _2, _3, ... rather than pointing
    // at something that already has content in it.
    bool exact = false;
    const std::string base = workspace_base(sources, exact);
    if (base.empty()) return {};
    return exact ? base : fresh_workspace(base);
}

bool workspace_named_by(const std::vector<PrepInput>& sources, const std::string& ws) {
    bool exact = false;
    const std::string base = workspace_base(sources, exact);
    if (base.empty() || ws.empty()) return false;
    if (ws == base) return true;
    if (exact || ws.size() <= base.size() + 1 || ws.compare(0, base.size() + 1, base + "_") != 0)
        return false;
    for (size_t i = base.size() + 1; i < ws.size(); i++)
        if (!std::isdigit((unsigned char)ws[i])) return false;
    return true;
}


bool any_pano360(const std::vector<PrepInput>& sources) {
    for (const PrepInput& s : sources)
        if (s.pano360.valid()) return true;
    return false;
}


// The size box shows what the run will actually use, not a zero standing for
// "work it out later": it is the one number here a user might want to change.
void reset_pano_size(const std::vector<PrepInput>& sources,
                     app::Pano360Options& pano) {
    for (const PrepInput& s : sources)
        if (s.pano360.valid()) {
            pano.size = app::pano360_default_size(s.pano360, pano);
            return;
        }
}


bool source_pixel_size(const PrepInput& s, const std::string& ffmpeg_exe,
                       int& w, int& h) {
    static const std::atomic<bool> never{false};
    w = h = 0;
    std::error_code ec;
    if (s.path.empty()) return false;
    if (s.is_video) {
        if (!fs::is_regular_file(s.path, ec)) return false;
        VideoFacts f;
        if (!ffmpeg_probe_video(ffmpeg_exe, s.path, f, never)) return false;
        w = f.width;
        h = f.height;
    } else {
        if (!fs::is_directory(s.path, ec)) return false;
        if (!DatasetPrep::first_image_dims(s.path, w, h)) return false;
    }
    return w > 0 && h > 0;
}


bool sources_look_equirect(const std::vector<PrepInput>& sources,
                           const std::string& ffmpeg_exe) {
    if (sources.empty()) return false;
    for (const PrepInput& s : sources) {
        int w = 0, h = 0;
        if (!source_pixel_size(s, ffmpeg_exe, w, h)) return false;
        if (std::fabs((double)w / (double)h - 2.0) > 0.02) return false;
    }
    return true;
}


void normalize_source_lenses(std::vector<PrepInput>& sources,
                             std::string& camera_model) {
    const std::vector<CameraGroup> groups = camera_groups(sources);
    if (groups.empty()) return;
    std::string above;
    for (size_t i = 0; i < groups.size(); i++) {
        std::string& m = group_model(sources, groups[i]);
        if (i == 0) {
            // Nothing above to inherit from, so a row the removal of the one
            // above just promoted keeps what it was resolving to.
            if (m.empty()) m = camera_model;
            if (m.empty()) m = default_lens(sources[groups[i].input]);
            above = m;
            continue;
        }
        if (m == above) m.clear();
        else if (!m.empty()) above = m;
    }
    // A group whose images are the whole capture carries no prefix, so nothing
    // names it on the command line -- the dataset-wide --camera-model is what
    // it gets. Keep that equal to the first row, which is the row it is.
    camera_model = group_model(sources, groups[0]);
}


void normalize_source_fps(std::vector<PrepInput>& sources, float& video_fps) {
    float above = 0.0f;
    bool first = true;
    for (PrepInput& s : sources) {
        if (!s.is_video) continue;
        const float rate = std::max(s.fps, 0.0f);
        if (first) {
            if (s.fps != 0.0f) video_fps = rate;
            if (!(video_fps >= 0.0f)) video_fps = 2.0f;
            s.fps = 0.0f;
            above = video_fps;
            first = false;
        } else if (s.fps != 0.0f && rate == above) {
            s.fps = 0.0f;
        } else if (s.fps != 0.0f) {
            above = rate;
        }
    }
}


void apply_lens_to_sources(std::vector<PrepInput>& sources, SfmJob& sfm,
                           const std::string& model) {
    sfm.camera_model = model;
    for (PrepInput& s : sources) {
        s.camera_model = model;
        for (SubCamera& sc : s.subcameras) sc.camera_model.clear();
    }
    normalize_source_lenses(sources, sfm.camera_model);
}


void apply_pano_lens(std::vector<PrepInput>& sources, SfmJob& sfm,
                     ColmapJob& colmap) {
    const app::Pano360Options& p = sfm.prep.pano;
    if (p.mode == app::Pano360Mode::Off) return;
    const bool faces = p.mode == app::Pano360Mode::Faces;
    // All ten views share one focal length in PIXELS, and the factor is
    // resolved against the first image in the tree -- cam0, the view on the
    // lens axis, which is the 90-degree one this halves (build_manifest).
    const float focal = faces ? 0.5f : 0.0f;
    for (PrepInput& s : sources) {
        if (!s.pano360.valid()) continue;
        s.camera_model = faces ? "pinhole" : "equirectangular";
        s.focal_factor = focal;
        for (SubCamera& sc : s.subcameras) {
            sc.camera_model.clear();
            sc.focal_factor = 0.0f;
        }
    }
    if (!sources.empty() && sources[0].pano360.valid())
        sfm.camera_model = sources[0].camera_model;
    // COLMAP has no spherical model; only the faces can reach that engine.
    if (faces) {
        colmap.camera_model = "PINHOLE";
        colmap.init_focal_factor = focal;
    }
    normalize_source_lenses(sources, sfm.camera_model);
}


void apply_capture_defaults(std::vector<PrepInput>& sources, SfmJob& sfm,
                            ColmapJob& colmap) {
    normalize_source_fps(sources, sfm.prep.video_fps);
    if (sources.empty()) return;
    const bool video = sources[0].is_video;
    const bool dual = is_dual_lens(sources[0]);
    sfm.data_type = video ? 1 : 0;
    sfm.pairs = 0;               // automatic
    // NOT sequential for a dual-lens video: the tracks are concatenated, so
    // temporal neighbours miss every cross-lens pair -- 68/118 registered on an
    // X5 capture against 116/118 for automatic, which is content-based.
    colmap.matcher = (video && !dual) ? 2 : 1;
    colmap.seq_loop_closure = true;   // if switched to sequential
    if (has_fisheye_lens(sources[0])) {
        colmap.camera_model = "THIN_PRISM_FISHEYE";
    }
    // Several inputs are several cameras, and so is one dual-lens file.
    if (sources.size() > 1 || dual) {
        sfm.camera_mode = 1;
        colmap.camera_mode = 1;
    }
    if (any_pano360(sources)) {
        // Views of one frame share no features, so temporal neighbours are not
        // the pairs that hold a 360 dataset together; the same reasoning as the
        // dual-lens case above, and content-based selection is the answer.
        sfm.pairs = 0;
        colmap.matcher = 2;
        // Every view of every 360 input is the same camera by construction --
        // one focal, one centre, no distortion -- so folder grouping would hand
        // bundle adjustment six copies of it to drift apart.
        bool all = true;
        for (const PrepInput& s : sources) all = all && s.pano360.valid();
        sfm.camera_mode = all ? 0 : 1;
        colmap.camera_mode = all ? 0 : 1;
        if (sfm.prep.pano.mode == app::Pano360Mode::Off)
            sfm.prep.pano.mode = app::Pano360Mode::Faces;
        reset_pano_size(sources, sfm.prep.pano);
        apply_pano_lens(sources, sfm, colmap);
    }
}


void dataset_adapt_preset(const std::string& preset,
                          std::vector<PrepInput>& sources, SfmJob& sfm,
                          ColmapJob& colmap, const std::string& ffmpeg_exe) {
    if (preset != "360-camera" || sources.empty()) return;
    // A packed dual-lens file (.insv/.360) is already handled: the pano plan
    // warps it into views and decides their lens, which is not this question.
    // An .insp or .lrv measures 2:1 and is two fisheye circles, not a panorama.
    if (any_pano360(sources)) return;
    for (const PrepInput& s : sources)
        if (has_fisheye_lens(s)) return;
    if (!sources_look_equirect(sources, ffmpeg_exe)) return;
    apply_lens_to_sources(sources, sfm, "equirectangular");
    // COLMAP has no spherical model, so its own choice is left alone; the
    // panel warns about the pair, and the built-in engine is the default.
    (void)colmap;
}


void resolve_source_lenses(std::vector<PrepInput>& sources, SfmJob& sfm,
                           ColmapJob& colmap) {
    if (sources.empty()) return;
    if (any_pano360(sources)) {
        // Nothing on the panel can say "leave the packed tracks alone", and
        // the combo draws that state as "views", so a preset saved off a flat
        // capture must not be able to leave a 360 one in it.
        if (sfm.prep.pano.mode == app::Pano360Mode::Off)
            sfm.prep.pano.mode = app::Pano360Mode::Faces;
        if (sfm.prep.pano.size <= 0) reset_pano_size(sources, sfm.prep.pano);
        apply_pano_lens(sources, sfm, colmap);
        return;
    }
    // A capture whose lens the file itself names keeps it; anything else takes
    // the one the settings carry, which is how a preset reaches a whole batch.
    for (const PrepInput& s : sources)
        if (has_fisheye_lens(s)) {
            normalize_source_lenses(sources, sfm.camera_model);
            return;
        }
    apply_lens_to_sources(sources, sfm, sfm.camera_model);
}

}  // namespace gui

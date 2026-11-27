// Paths that arrive by drag-and-drop or from a file picker, routed to the screen
// that takes them.

#include "app/gui/GuiApp.h"

#include "app/gui/GuiAppShared.h"
#include "checkpoint/SplatPly.h"
#include "data/SparseEdit.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/Gui.h"
#include "mesh/MeshImport.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;
namespace i18n = spirula::i18n;
namespace msg = spirula::i18n::msg::gui;
namespace dmsg = spirula::i18n::msg::dataset;

namespace gui {

const std::vector<std::string> kOpenableExtensions = {
    ".ply", ".obj", ".gltf", ".glb", ".stl", ".json", ".bin", ".txt", ".xml", ".psx"};

// What the viewer can open: Gaussians and SfM clouds (.ply), and every mesh
// format the mesher writes -- one list, so the picker and the drop handler agree.
static const std::vector<std::string> kViewableExtensions = {".ply", ".obj", ".gltf",
                                                             ".glb", ".stl"};

// A path's extension, lowercased, for the tables that list them.
static std::string lower_ext(const fs::path& p) {
    std::string e = p.extension().string();
    for (char& c : e) c = (char)std::tolower((unsigned char)c);
    return e;
}

static bool is_image_ext(const fs::path& p) {
    std::string e = p.extension().string();
    for (auto& c : e) c = (char)std::tolower((unsigned char)c);
    return e == ".jpg" || e == ".jpeg" || e == ".png" || e == ".webp" ||
           e == ".tif" || e == ".tiff" || e == ".bmp";
}

// Is this folder a finished model rather than a dataset -- a run directory, or
// one of its checkpoints? Then it belongs in the viewer.
static bool looks_like_model(const fs::path& p) {
    try {
        spirula::find_splat_ply(p.string());
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

ModelContent classify_recent_model(const std::string& path) {
    std::error_code ec;
    const fs::path p = fs::u8path(path);
    if (fs::is_directory(p, ec))
        return looks_like_model(p) ? ModelContent::Splats : ModelContent::Points;
    const std::string ext = lower_ext(p);
    if (ext == ".ply") {
        if (spirula::is_splat_ply(path)) return ModelContent::Splats;
        return meshing::ply_is_mesh(path) ? ModelContent::Mesh : ModelContent::Points;
    }
    if (meshing::is_mesh_path(path)) return ModelContent::Mesh;
    // transforms.json, cameras.bin, a Metashape .xml: a reconstruction.
    return ModelContent::Points;
}

// The folder that several dropped files all come from, when they are a
// reconstruction's files rather than photos or videos and it reads as one.
static std::string model_files_folder(const std::vector<std::string>& paths) {
    if (paths.size() < 2) return {};
    std::error_code ec;
    const fs::path dir = fs::path(paths[0]).parent_path();
    bool model_file = false;
    for (const std::string& s : paths) {
        const fs::path p(s);
        if (!fs::is_regular_file(p, ec) || p.parent_path() != dir || is_image_ext(p) ||
            is_video_path(s))
            return {};
        const std::string ext = lower_ext(p);
        model_file = model_file || p.filename() == "transforms.json" || ext == ".bin" ||
                     ext == ".txt" || ext == ".db" || ext == ".xml" || ext == ".psx";
    }
    if (!model_file || spirula::resolve_sparse_dir(dir.string()).empty()) return {};
    return dir.string();
}

// Videos and folders of photos out of a drop; the rest is logged.
std::vector<std::string> GuiApp::raw_inputs(const std::vector<std::string>& paths) {
    std::error_code ec;
    std::vector<std::string> sources;
    for (const std::string& path : paths) {
        const fs::path p(path);
        if (fs::is_directory(p, ec)) {
            if (folder_has_images(path)) sources.push_back(path);
            else log(i18n::format(dmsg::log_drop_no_images, {path}));
        } else if (fs::is_regular_file(p, ec)) {
            if (is_video_path(path)) sources.push_back(path);
            else log(i18n::format(dmsg::log_drop_unsupported, {path}));
        }
    }
    return sources;
}

void GuiApp::handle_drop(const std::vector<std::string>& paths) {
    std::error_code ec;
    // A laser scan is an input of the dataset screen wherever it lands, and so
    // is a point-cloud PLY there or dropped with photos, videos or scans; a
    // lone one elsewhere is to be looked at.
    std::vector<std::string> scans, clouds, rest;
    for (const std::string& p : paths) {
        const bool file = fs::is_regular_file(p, ec);
        if (file && is_lidar_drop(p)) scans.push_back(p);
        else if (file && ply_is_point_cloud(p)) clouds.push_back(p);
        else rest.push_back(p);
    }
    const bool raw_beside = std::any_of(rest.begin(), rest.end(), [&](const std::string& p) {
        return fs::is_directory(p, ec) ? folder_has_images(p) : is_video_path(p);
    });
    if (_screen == Screen::NewDataset || raw_beside || !scans.empty())
        scans.insert(scans.end(), clouds.begin(), clouds.end());
    if (!scans.empty()) {
        if (native_work_busy()) {
            if (training_busy()) log(dmsg::log_drop_while_training.get());
            return;
        }
        close_native_previews();
        close_splat();
        if (_screen != Screen::NewDataset) {
            clear_sources();
            clear_lidar_sources();
            go_to(Screen::NewDataset);
        }
        const std::vector<std::string> sources = raw_inputs(rest);
        if (!sources.empty()) add_sources(sources, /*replace=*/false);
        add_lidar_sources(scans);
        return;
    }
    // The mesh screen asks which model and which photos, so a drop there
    // answers one of them instead of navigating away. A finished preview does
    // not block it; a run in flight does, its inputs already gone to the child.
    if (_screen == Screen::Mesh && !native_work_busy() && paths.size() == 1) {
        const fs::path p(paths[0]);
        std::string ext = p.extension().string();
        for (char& c : ext) c = (char)std::tolower((unsigned char)c);
        const bool is_file = fs::is_regular_file(p, ec);
        const bool is_dir = fs::is_directory(p, ec);
        // A splat .ply or a *.ckpt / run folder is the model; a dataset-only
        // folder is the photos. A dataset with a splat.ply in it (in-place
        // training) is the model, and the child finds the dataset itself.
        if (is_file && ext == ".ply" && !meshing::ply_is_mesh(paths[0])) {
            set_mesh_source(paths[0]);
            return;
        }
        if (is_dir && looks_like_model(p)) {
            set_mesh_source(paths[0]);
            // The same folder is often the dataset too; offer it rather than
            // making the user type it again.
            if (folder_looks_like_dataset(paths[0]))
                _mesh_job.data_dir = paths[0];
            return;
        }
        // `sparse/0` stands for the dataset it sits in.
        std::string data = paths[0];
        if (is_dir && !folder_looks_like_dataset(data)) data = spirula::resolve_sparse_dir(data);
        if (is_dir && !data.empty() && folder_looks_like_dataset(data)) {
            close_mesh_preview();
            _mesh_job.use_data = true;
            _mesh_job.data_dir = data;
            return;
        }
    }

    // A preset, or the config.json of a run that came out well. Checked first
    // and by content rather than by name: nothing else it could be. Its own
    // kind decides which screen it lands on.
    if (paths.size() == 1 && fs::is_regular_file(paths[0], ec)) {
        const std::optional<PresetKind> kind = probe_preset_kind(paths[0]);
        if (kind == PresetKind::Dataset) {
            load_dataset_preset_file(paths[0]);
            if (!_ds_presets.msg_err) request_screen(Screen::NewDataset);
            return;
        }
        if (kind == PresetKind::Mesh) {
            load_mesh_preset_file(paths[0]);
            if (!_mesh_presets.msg_err) request_screen(Screen::Mesh);
            return;
        }
        if (kind == PresetKind::Train && is_preset_file(paths[0])) {
            load_preset_file(paths[0]);
            if (!_train_presets.msg_err && !_cfg.data.empty())
                request_screen(Screen::Train);
            return;
        }
    }
    // A camera project: its model on the viewer screen, and the render with it.
    if (paths.size() == 1 && lower_ext(paths[0]) == ".json" && open_render_project(paths[0]))
        return;
    // Dropping onto the batch screen extends the queue, which is how a
    // five-dataset run gets set up without typing five paths. What each path
    // is decides what its row does.
    if (_screen == Screen::Batch && !_batch_active) {
        std::vector<std::string> datasets, models, raw;
        for (const std::string& p : paths) {
            if (fs::is_directory(p, ec) && looks_like_model(p)) models.push_back(p);
            else if (fs::is_directory(p, ec) && folder_looks_like_dataset(p))
                datasets.push_back(p);
            else if (fs::is_directory(p, ec) && folder_has_images(p)) raw.push_back(p);
            else if (fs::is_regular_file(p, ec) && is_video_path(p)) raw.push_back(p);
            else if (fs::is_regular_file(p, ec) &&
                     std::find(kViewableExtensions.begin(), kViewableExtensions.end(),
                               lower_ext(p)) != kViewableExtensions.end())
                models.push_back(p);
        }
        if (datasets.size() + models.size() + raw.size() == paths.size() &&
            !paths.empty()) {
            for (const std::string& d : datasets) add_batch_row(d);
            for (const std::string& m : models) add_batch_mesh_row(m);
            // Videos dropped together are one capture, as they are everywhere
            // else; folders are one each.
            std::vector<std::string> clips;
            for (const std::string& r : raw) {
                if (fs::is_directory(r, ec)) add_batch_source_row({r});
                else clips.push_back(r);
            }
            if (!clips.empty()) add_batch_source_row(clips);
            return;
        }
    }
    // Several models dropped together open side by side, which is what the
    // viewer is for.
    {
        std::vector<std::string> models;
        for (const std::string& p : paths) {
            if (!fs::is_regular_file(p, ec)) break;
            std::string ext = fs::path(p).extension().string();
            for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
            if (std::find(kViewableExtensions.begin(), kViewableExtensions.end(),
                          ext) == kViewableExtensions.end())
                break;
            models.push_back(p);
        }
        if (models.size() == paths.size() && models.size() > 1 &&
            !native_work_busy()) {
            open_splat(models[0]);
            for (size_t i = 1; i < models.size() &&
                               i < (size_t)CompareView::kMaxModels; i++)
                add_splat(models[i]);
            if (models.size() > (size_t)CompareView::kMaxModels)
                log(msg::compare_full.get());
            return;
        }
    }
    // A dataset is opened, not added to a list, so it only makes sense alone --
    // and dropping one alongside videos is far more likely a mis-drag than a
    // request to do both.
    if (paths.size() == 1 && fs::is_directory(paths[0], ec) &&
        looks_like_model(paths[0])) {
        // Checked before the dataset test: a run directory sits INSIDE the
        // dataset it was trained from often enough that both would match.
        request_open_splat(paths[0]);
        return;
    }
    // An XGRIDS export: its dataset and its LiDAR map, on the dataset screen.
    if (paths.size() == 1 && fs::is_directory(paths[0], ec) && !native_work_busy() &&
        add_xgrids_export(paths[0])) {
        close_splat();
        go_to(Screen::NewDataset);
        return;
    }
    // A reconstruction that is not a trainable dataset -- a bare `sparse/`,
    // one of its models, or a folder holding one without the images -- is
    // something to look at and clean up, not something to train on.
    if (paths.size() == 1 && fs::is_directory(paths[0], ec) &&
        !folder_looks_like_dataset(paths[0]) &&
        !spirula::resolve_sparse_dir(paths[0]).empty()) {
        request_open_splat(paths[0]);
        return;
    }
    // ... except on the dataset screen, where a finished dataset is an input
    // like any other: that is how one gets masks, depth and normals added to
    // it without its cameras being solved a second time.
    if (paths.size() == 1 && fs::is_directory(paths[0], ec) &&
        folder_looks_like_dataset(paths[0])) {
        if (_screen == Screen::NewDataset && !dataset_busy())
            add_existing_dataset(paths[0]);
        else
            request_open_dataset(paths[0]);
        return;
    }
    if (paths.size() == 1 && fs::is_regular_file(paths[0], ec) &&
        !is_video_path(paths[0])) {
        // A file from inside a dataset (transforms.json, database.db, a
        // COLMAP .bin/.txt, a Metashape camera .xml or .psx) opens the dataset
        // it belongs to.
        const fs::path p(paths[0]);
        std::string ext = p.extension().string();
        for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
        // A .ply is a model to look at -- Gaussians, a point cloud or a mesh,
        // which the viewer works out for itself. (A Metashape dataset FOLDER is
        // caught above, so dropping one still opens the dataset.)
        if (std::find(kViewableExtensions.begin(), kViewableExtensions.end(),
                      ext) != kViewableExtensions.end()) {
            request_open_splat(p.string());
            return;
        }
        if (p.filename() == "transforms.json" || ext == ".db" ||
            ext == ".bin" || ext == ".txt" || ext == ".xml" || ext == ".psx") {
            const std::string dir = p.parent_path().string();
            // A file from a COLMAP model folder names a reconstruction, not a
            // dataset: `sparse/0` has no images beside it to train from.
            if (!folder_looks_like_dataset(dir) &&
                !spirula::resolve_sparse_dir(paths[0]).empty())
                request_open_splat(paths[0]);
            else
                request_open_dataset(dir);
            return;
        }
    }
    // Several files picked out of one model folder (cameras.bin, images.bin,
    // points3D.bin, ...) stand for the folder.
    if (const std::string dir = model_files_folder(paths); !dir.empty()) {
        if (folder_looks_like_dataset(dir)) request_open_dataset(dir);
        else request_open_splat(dir);
        return;
    }

    // Everything else is raw input: videos, and folders of photos.
    const std::vector<std::string> sources = raw_inputs(paths);
    if (sources.empty()) return;
    if (native_work_busy()) {
        if (training_busy()) log(dmsg::log_drop_while_training.get());
        return;
    }
    // Dropping onto the dataset screen adds to what is already listed there;
    // dropping from anywhere else starts a new dataset.
    if (add_sources(sources, /*replace=*/_screen != Screen::NewDataset))
        go_to(Screen::NewDataset);
}

const char* GuiApp::dir_key(PickAction a, FileDialog::Mode m) {
    switch (a) {
        case PickAction::OpenDataset:
        case PickAction::BatchDataset:
        case PickAction::SourceDataset:
        case PickAction::MeshPhotos:        return "dataset";
        case PickAction::SourceImages:
        case PickAction::BatchSourceImages: return "photos";
        case PickAction::SourceVideo:
        case PickAction::BatchSourceVideo:  return "video";
        case PickAction::SourceReplace:
            return m == FileDialog::Mode::File ? "video" : "photos";
        case PickAction::SplatFile:
        case PickAction::AddSplatFile:
        case PickAction::SplatFolder:
        case PickAction::BatchModel:
        case PickAction::EditSaveFile:
        case PickAction::EditSaveFolder:
        case PickAction::RenderAddModel:
        case PickAction::MeshSource:        return "model";
        case PickAction::StencilFile:       return "stencil";
        case PickAction::SourceLut:         return "lut";
        case PickAction::SeedPointcloud:    return "seed_pointcloud";
        case PickAction::LidarSource:       return "lidar";
        case PickAction::ResumeRun:         return "train.resume";   // shared with the field's own picker
        case PickAction::RenderProjectSave:
        case PickAction::RenderProjectOpen: return "render_project";
        case PickAction::RenderOutput:      return "render_output";
        case PickAction::Workspace:
        case PickAction::OutputPrefix:
        case PickAction::BatchOutput:
        case PickAction::MeshOutput:        return "output";
        case PickAction::VocabTree:         return "vocab";
        case PickAction::PresetFile:
        case PickAction::DatasetPresetFile:
        case PickAction::MeshPresetFile:
        case PickAction::PresetSaveFolder:
        case PickAction::BatchPresetFile:
        case PickAction::BatchDatasetPresetFile:
        case PickAction::BatchMeshPresetFile: return "preset";
        case PickAction::MaskModelFile:
        case PickAction::ConfigPath:
        case PickAction::None:              return "";
    }
    return "";
}

void GuiApp::remember_dir(const std::string& key, const std::string& path) {
    if (key.empty() || path.empty()) return;
    std::string dir = fs::path(path).parent_path().string();
    if (!dir.empty()) _dialog_dirs[key] = dir;
}

void GuiApp::open_pick(PickAction a, const std::string& title,
                       FileDialog::Mode mode,
                       const std::vector<std::string>& extensions,
                       const std::string& start_dir, bool multi,
                       const std::string& suggested_name) {
    _pick = a;
    // One remembered folder per flag: a run to resume and a region JSON do
    // not live in the same place.
    _pick_key = a == PickAction::ConfigPath ? "train." + _pick_field
                                            : dir_key(a, mode);
    std::string dir = start_dir;
    if (dir.empty()) {
        auto it = _dialog_dirs.find(_pick_key);
        if (it != _dialog_dirs.end()) dir = it->second;
    }
    _dialog.open(title, mode, extensions, dir, multi, suggested_name);
}

void GuiApp::handle_dialog_result(const std::vector<std::string>& paths) {
    const std::string path = paths.empty() ? std::string() : paths[0];
    remember_dir(_pick_key, path);
    switch (_pick) {
        case PickAction::OpenDataset:
            request_open_dataset(path);
            break;
        case PickAction::ResumeRun:
            open_resume(path);
            break;
        case PickAction::SourceImages:
        case PickAction::SourceVideo:
            if (add_sources(paths, /*replace=*/_screen != Screen::NewDataset))
                go_to(Screen::NewDataset);
            break;
        case PickAction::SourceDataset:
            add_existing_dataset(path);
            go_to(Screen::NewDataset);
            break;
        case PickAction::SourceReplace:
            if (!path.empty() && _pick_source >= 0 &&
                _pick_source < (int)_sources.size())
                replace_source((size_t)_pick_source, path);
            _pick_source = -1;
            break;
        case PickAction::Workspace:
            _workspace = path;
            break;
        case PickAction::OutputPrefix:
            _cfg.output_dir_prefix = path;
            break;
        case PickAction::VocabTree:
            _colmap_job.vocab_tree_path = path;
            break;
        case PickAction::SplatFile:
            request_open_splat(path);
            break;
        case PickAction::AddSplatFile:
            add_splat(path);
            break;
        case PickAction::SplatFolder:
            request_open_splat(path);
            break;
        case PickAction::EditSaveFile:
        case PickAction::EditSaveFolder:
            _compare.edit().save_to(_edit_save_target, path);
            break;
        case PickAction::RenderProjectSave:
        case PickAction::RenderProjectOpen: {
            const bool save = _pick == PickAction::RenderProjectSave;
            _compare.render().picked(save ? gui::render::RenderSession::Pick::SaveProject
                                          : gui::render::RenderSession::Pick::OpenProject,
                                     path);
            // Only if it worked: the session holds the file it picked, or for
            // a save of a bare name, that name plus the .json it was given.
            fs::path want = fs::u8path(path);
            if (save && want.extension().empty()) want += ".json";
            const std::string& now = _compare.render().project_file();
            if (!path.empty() && fs::u8path(now) == want) remember(RecentKind::Project, now);
            break;
        }
        case PickAction::RenderOutput:
            _compare.render().picked(gui::render::RenderSession::Pick::Output, path);
            break;
        case PickAction::RenderAddModel:
            add_splat(path);
            break;
        case PickAction::MeshSource:
            set_mesh_source(path);
            break;
        case PickAction::SeedPointcloud:
            _cfg.seed_pointcloud = path;
            _cfg_ui.touched.insert("seed_pointcloud");
            _parse_dirty = true;
            break;
        case PickAction::ConfigPath: {
            const TrainConfig before = _cfg;
            apply_path_pick(_cfg, _cfg_ui, _pick_field, path);
            if (!parse_settings_equal(before, _cfg)) _parse_dirty = true;
            break;
        }
        case PickAction::SourceLut:
            if (!path.empty() && _pick_lut >= 0 && _pick_lut < (int)_sources.size()) {
                _sources[(size_t)_pick_lut].lut = path;
                close_native_previews();
                refresh_sources();
            }
            _pick_lut = -1;
            break;
        case PickAction::StencilFile:
            if (_segment.is_open()) {
                const StencilTable t = stencil_table();
                const int edited = panel_owner(t);
                if (edited < 0) break;
                settle_follow(t.owners, t.rows);
                const std::vector<app::CameraStencil> before = row_stencils(t.owners, t.rows);
                _segment.load_file(*t.owners[(size_t)edited].stencil, path);
                follow_edit(t.owners, t.rows, before, (size_t)edited);
                refresh_frame_shapes(_sources, _frame_shapes);
            }
            break;
        case PickAction::MeshPhotos:
            _mesh_job.data_dir = path;
            break;
        case PickAction::LidarSource:
            if (_pick_lidar < 0) add_lidar_sources(paths);
            else if (!path.empty()) replace_lidar_source((size_t)_pick_lidar, path);
            _pick_lidar = -1;
            break;
        case PickAction::MeshOutput:
            // Only the folder is picked; the file name keeps whatever
            // set_mesh_source derived, as the preset save dialog does.
            if (!path.empty())
                _mesh_job.output =
                    (fs::path(path) /
                     fs::path(_mesh_job.output.empty() ? "mesh"
                                                       : _mesh_job.output)
                         .filename())
                        .string();
            break;
        case PickAction::PresetFile:
            load_preset_file(path);
            break;
        case PickAction::PresetSaveFolder:
            // Only the folder is picked; the file name stays whatever the
            // name field derived, so the two halves of the path keep their
            // separate owners.
            if (!path.empty())
                _preset_save_path =
                    (fs::path(path) / fs::path(_preset_save_path).filename())
                        .string();
            break;
        case PickAction::DatasetPresetFile:
            load_dataset_preset_file(path);
            break;
        case PickAction::MeshPresetFile:
            load_mesh_preset_file(path);
            break;
        case PickAction::BatchDataset:
            if (!path.empty()) {
                if (_pick_row >= 0 && _pick_row < (int)_batch.size()) {
                    _batch[_pick_row].dataset = path;
                    batch_edited();
                } else {
                    // Several folders picked at once become several rows.
                    for (const std::string& p : paths) add_batch_row(p);
                }
            }
            _pick_row = -1;
            break;
        case PickAction::BatchSourceImages:
        case PickAction::BatchSourceVideo:
            if (!paths.empty()) {
                if (_pick_row >= 0 && _pick_row < (int)_batch.size()) {
                    for (const std::string& p : paths)
                        _batch[_pick_row].sources.push_back(p);
                    batch_edited();
                } else {
                    // Videos picked together are one capture, which is what
                    // the dataset screen does with the same pick. A folder of
                    // photos is a capture on its own.
                    if (_pick == PickAction::BatchSourceVideo)
                        add_batch_source_row(paths);
                    else
                        for (const std::string& p : paths)
                            add_batch_source_row({p});
                }
            }
            _pick_row = -1;
            break;
        case PickAction::BatchModel:
            if (!path.empty()) {
                if (_pick_row >= 0 && _pick_row < (int)_batch.size()) {
                    _batch[_pick_row].model = path;
                    batch_edited();
                } else {
                    for (const std::string& p : paths) add_batch_mesh_row(p);
                }
            }
            _pick_row = -1;
            break;
        case PickAction::BatchOutput:
            if (_pick_row >= 0 && _pick_row < (int)_batch.size()) {
                _batch[_pick_row].output_dir = path;
                batch_edited();
            }
            _pick_row = -1;
            break;
        case PickAction::BatchDatasetPresetFile:
            if (!path.empty() && _pick_row >= 0 && _pick_row < (int)_batch.size()) {
                try {
                    DatasetPreset p = load_dataset_preset(path);
                    _batch[_pick_row].dataset_preset = {p.path, p.name};
                    batch_edited();
                } catch (const std::exception& e) {
                    _batch_msg = i18n::format(msg::preset_failed, {e.what()});
                    _batch_msg_err = true;
                }
            }
            _pick_row = -1;
            break;
        case PickAction::BatchMeshPresetFile:
            if (!path.empty() && _pick_row >= 0 && _pick_row < (int)_batch.size()) {
                try {
                    MeshPreset p = load_mesh_preset(path);
                    _batch[_pick_row].mesh.preset = {p.path, p.name};
                    batch_edited();
                } catch (const std::exception& e) {
                    _batch_msg = i18n::format(msg::preset_failed, {e.what()});
                    _batch_msg_err = true;
                }
            }
            _pick_row = -1;
            break;
        case PickAction::BatchPresetFile:
            if (!path.empty() && (_pick_row == -2 || _pick_row == -3)) {
                try {
                    TrainPreset p = load_preset(path);
                    BatchRun& run = _pick_row == -2 ? _pq.run : _dataset_training.run;
                    run.preset = {p.path, p.name};
                } catch (const std::exception& e) {
                    log(e.what());
                }
            } else if (!path.empty() && _pick_row >= 0 &&
                _pick_row < (int)_batch.size()) {
                try {
                    TrainPreset p = load_preset(path);
                    BatchRow& row = _batch[_pick_row];
                    if (_pick_slot >= 0 && _pick_slot < (int)row.runs.size()) {
                        row.runs[_pick_slot].preset.path = p.path;
                        row.runs[_pick_slot].preset.name = p.name;
                    } else {
                        BatchRun run;
                        run.preset = {p.path, p.name};
                        row.runs.push_back(std::move(run));
                    }
                    batch_edited();
                } catch (const std::exception& e) {
                    _batch_msg = i18n::format(msg::preset_failed, {e.what()});
                    _batch_msg_err = true;
                }
            }
            _pick_row = -1;
            _pick_slot = -1;
            break;
        default:
            break;
    }
    _pick = PickAction::None;
    _pick_key.clear();
}

}  // namespace gui

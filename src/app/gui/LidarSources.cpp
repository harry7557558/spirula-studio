// Laser scans on the dataset screen: the files, what they hold, and the
// switches a run with them has (app/gui/LidarStep.h does the work).

#include "app/gui/GuiApp.h"

#include "app/LidarDataset.h"
#include "app/gui/Ui.h"
#include "checkpoint/SplatPly.h"
#include "data/PointCloudFile.h"
#include "data/XbinReader.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/Lidar.h"
#include "i18n/catalog/SourcePreview.h"
#include "mesh/MeshImport.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <optional>

namespace fs = std::filesystem;
namespace dmsg = spirula::i18n::msg::dataset;
namespace ldmsg = spirula::i18n::msg::lidar;
namespace pmsg = spirula::i18n::msg::preview;
using spirula::i18n::format;

namespace gui {
namespace {

const ImVec4 kWarnColor(0.95f, 0.75f, 0.30f, 1.0f);

std::string lower_ext(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    for (char& c : ext) c = (char)std::tolower((unsigned char)c);
    return ext;
}

std::vector<std::string> las_files_in(const fs::path& dir) {
    std::vector<std::string> out;
    std::error_code ec;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
        if (it->is_regular_file(ec) && lower_ext(it->path().string()) == ".las")
            out.push_back(it->path().string());
    std::sort(out.begin(), out.end());
    return out;
}

// LCC Studio's developer_data/ (raw/ and perspective/ COLMAP datasets beside the
// LiDAR map) or nCore_data/ (both under colmap/, the map under lidar/), matched by
// layout since either may be renamed; `dir` is one of them, inside one, or above.
bool find_xgrids_export(const fs::path& dir, fs::path& raw, std::vector<std::string>& clouds) {
    std::error_code ec;
    fs::path at = dir;
    if (at.filename() == "raw" || at.filename() == "perspective") at = at.parent_path();
    std::vector<fs::path> roots{at, at / "colmap"};
    std::vector<fs::path> subs;
    for (fs::directory_iterator it(at, ec), end; !ec && it != end; it.increment(ec))
        if (it->is_directory(ec)) subs.push_back(it->path());
    std::sort(subs.begin(), subs.end());
    for (const fs::path& s : subs) {
        roots.push_back(s);
        roots.push_back(s / "colmap");
    }
    for (const fs::path& root : roots) {
        if (!fs::is_directory(root / "raw" / "sparse", ec) ||
            !fs::is_directory(root / "raw" / "images", ec))
            continue;
        clouds = las_files_in(root);
        if (clouds.empty()) clouds = las_files_in(root.parent_path() / "lidar");
        if (clouds.empty()) continue;
        raw = root / "raw";
        return true;
    }
    return false;
}

}  // namespace

bool GuiApp::is_lidar_drop(const std::string& path) {
    const std::string ext = lower_ext(path);
    return ext == ".e57" || ext == ".las" || ext == ".laz" || ext == ".xbin";
}

int64_t GuiApp::taken_photos(const LidarInput& in) {
    int64_t n = 0;
    for (const LidarInput::Camera& c : in.cameras)
        if (c.pick.taken) n += app::lidar::frames_at_rate(c.frames, c.seconds, c.pick.fps);
    return n;
}

bool GuiApp::ply_is_point_cloud(const std::string& path) {
    return lower_ext(path) == ".ply" && !spirula::is_splat_ply(path) &&
           !meshing::ply_is_mesh(path);
}

bool GuiApp::read_lidar_input(const std::string& path, LidarInput& in) {
    std::error_code ec;
    in = LidarInput{};
    in.path = in.edit = fs::absolute(path, ec).string();
    // A trained splat reads as points too, but it holds no scanner position
    // and keeps its colour in f_dc_*: no view of it can place the photos, and
    // the Align step would fail only after the whole reconstruction.
    if (spirula::is_splat_ply(in.path)) {
        log(format(ldmsg::scan_is_splat, {in.path}));
        return false;
    }
    try {
        spirula::cloud::Reader r(in.path);
        if (r.e57())
            for (const auto& im : r.e57()->images()) in.photos += im.has_pose;
        if (spirula::xbin::Reader* x = r.xbin()) {
            for (const spirula::xbin::Camera& c : x->cameras())
                in.cameras.push_back({c.name, c.fisheye, c.frames, c.seconds});
            in.photos = taken_photos(in);
        }
        in.points = r.info().points;
        in.stations = r.info().stations;
    } catch (const std::exception& e) {
        log(format(ldmsg::scan_unreadable, {in.path, std::string(e.what())}));
        return false;
    }
    return true;
}

void GuiApp::add_lidar_sources(const std::vector<std::string>& paths) {
    const size_t first_new = _lidar.size();
    for (const std::string& p : paths) {
        if (p.empty()) continue;
        LidarInput in;
        if (!read_lidar_input(p, in)) continue;
        if (std::any_of(_lidar.begin(), _lidar.end(),
                        [&](const LidarInput& l) { return l.path == in.path; }))
            continue;
        // A row per camera from the start, its fisheyes cut, as a 360
        // camera's own files are: the lens border is in every one of their frames.
        for (const LidarInput::Camera& c : in.cameras) {
            in.stencil.cameras[c.name].detect_border = c.fisheye;
            _border_enable = _border_enable || c.fisheye;
        }
        _lidar.push_back(std::move(in));
    }
    if (first_new < _lidar.size()) select_source(_lidar[first_new].path, true);
    refresh_sources();
}

void GuiApp::replace_lidar_source(size_t i, const std::string& path) {
    LidarInput in;
    if (i >= _lidar.size()) return;
    if (!read_lidar_input(path, in)) {
        _lidar[i].edit = _lidar[i].path;
        return;
    }
    _lidar[i] = std::move(in);
    refresh_sources();
}

void GuiApp::clear_lidar_sources() {
    _lidar.clear();
    if (_mask_preview_scan >= 0) _segment.close();
    _mask_preview_scan = -1;
    _mask_try_scan = -1;
    _scan_photos_for = SIZE_MAX;
    _lidar_photos = true;
    _lidar_in_frame = false;
    _lidar_keep_poses = true;
}

bool GuiApp::add_xgrids_export(const std::string& dir) {
    fs::path raw;
    std::vector<std::string> clouds;
    if (!find_xgrids_export(fs::path(dir), raw, clouds)) return false;
    add_dataset_folder(raw.string());
    add_lidar_sources(clouds);
    _lidar_in_frame = true;
    _use_found_masks = true;
    _flip_found_masks = true;
    // Nothing may be written into the export: its photos and masks are read
    // where they are (any other import folds the masks into the dataset's
    // masks/, which here is theirs), and no stencil is merged into them.
    _photo_import = PhotoImport::InPlace;
    _border_enable = false;
    log(ldmsg::xgrids_found.get());
    return true;
}

bool GuiApp::scanner_poses_offered() const {
    if (!_sources.empty() || _lidar.empty()) return false;
    std::vector<std::vector<spirula::cloud::Station>> stations;
    bool photos = false;
    for (const LidarInput& l : _lidar) {
        stations.push_back(l.stations);
        photos = photos || l.photos > 0;
    }
    return photos && app::lidar::scans_share_frame(stations);
}

LidarJob GuiApp::lidar_job() const {
    LidarJob j;
    for (const LidarInput& l : _lidar) {
        j.clouds.push_back(l.path);
        if (l.photos > 0) j.with_photos.push_back(l.path);
        for (const LidarInput::Camera& c : l.cameras) j.xbin_cameras[l.path].push_back(c.pick);
        if (!l.cameras.empty() && l.place_only && !_sources.empty()) j.xbin_place_only.insert(l.path);
    }
    j.ffmpeg_exe = _ffmpeg_exe;
    // With nothing else, the scans' photographs are the dataset: always used,
    // and there is no other model to be in their frame.
    j.scan_photos = _lidar_photos || _sources.empty();
    j.in_frame = _lidar_in_frame && !_sources.empty();
    j.scanner_only = _lidar_keep_poses && scanner_poses_offered();
    j.flip_masks = _use_found_masks && _flip_found_masks;
    return j;
}

void GuiApp::draw_lidar_rows(float path_w, bool one_line) {
    int remove = -1;
    ImGui::PushID("lidar");
    _lidar_row_h.resize(_lidar.size(), 0.0f);
    const int selected = selected_scan();
    for (size_t i = 0; i < _lidar.size(); i++) {
        ImGui::PushID((int)i);
        const float row_y = ImGui::GetCursorScreenPos().y;
        if (row_select_begin(_lidar_row_h[i], (int)i == selected))
            select_source(_lidar[i].path, true);
        ImGui::SetNextItemWidth(path_w);
        ui::InputTextRaw("##in", &_lidar[i].edit);
        if (ImGui::IsItemActivated()) select_source(_lidar[i].path, true);
        if (ImGui::IsItemDeactivatedAfterEdit() && _lidar[i].edit != _lidar[i].path)
            replace_lidar_source(i, _lidar[i].edit);
        ImGui::SameLine();
        if (ui::Button(dmsg::browse)) {
            _pick_lidar = (int)i;
            open_pick(PickAction::LidarSource, ldmsg::pick_scan.get(), FileDialog::Mode::File,
                      {".e57", ".las", ".ply", ".xbin"});
        }
        ImGui::SameLine();
        if (ui::Button(dmsg::remove)) remove = (int)i;
        // Where a video row has its rate box, which a camera's lines below use too.
        ImGui::SameLine();
        const float rate_x = ImGui::GetCursorScreenPos().x;
        if (!one_line) ImGui::NewLine();
        ui::Text(ldmsg::scan_row, {(long long)_lidar[i].points, (long long)_lidar[i].photos});
        draw_capture_cameras(_lidar[i], rate_x);
        _lidar_row_h[i] = ImGui::GetCursorScreenPos().y - row_y - ImGui::GetStyle().ItemSpacing.y;
        ImGui::PopID();
    }
    ImGui::PopID();
    if (remove >= 0 && !dataset_busy()) {
        _lidar.erase(_lidar.begin() + remove);
        forget_scan((size_t)remove);
        refresh_sources();
    }
}

void GuiApp::draw_capture_cameras(LidarInput& in, float rate_x) {
    int taken = 0;
    for (const LidarInput::Camera& c : in.cameras) taken += c.pick.taken;
    for (size_t k = 0; k < in.cameras.size(); k++) {
        LidarInput::Camera& cam = in.cameras[k];
        ImGui::PushID((int)k);
        // The last camera cannot go too: without photographs the capture has no poses to give.
        ImGui::BeginDisabled(dataset_busy() || (cam.pick.taken && taken == 1));
        if (ui::CheckboxRaw(cam.name.c_str(), &cam.pick.taken)) {
            in.photos = taken_photos(in);
            refresh_sources();
        }
        ImGui::EndDisabled();
        ui::help_on_hover(pmsg::camera_toggle_help);
        ImGui::SameLine();
        ui::TextDisabled(cam.fisheye ? dmsg::lens_fisheye_opencv : dmsg::lens_pinhole);
        ImGui::SameLine();
        ImGui::SetCursorScreenPos({std::max(rate_x, ImGui::GetCursorScreenPos().x),
                                   ImGui::GetCursorScreenPos().y});
        // Its own rate to one decimal, so that typing what the box showed keeps every frame.
        const float own = cam.frames > 1 && cam.seconds > 0
                              ? (float)(std::round((cam.frames - 1) / cam.seconds * 10.0) / 10.0)
                              : 0.0f;
        if (!cam.fps_editing) cam.fps_text = fps_label(cam.pick.fps > 0.0f ? cam.pick.fps : own);
        ImGui::BeginDisabled(dataset_busy() || !cam.pick.taken);
        ImGui::SetNextItemWidth(px(84.0f));
        ui::InputTextRaw("##fps", &cam.fps_text);
        cam.fps_editing = ImGui::IsItemActive();
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            if (const std::optional<float> v = parse_fps(cam.fps_text))
                cam.pick.fps = *v == 0.0f || (own > 0.0f && *v >= own) ? 0.0f : *v;
            in.photos = taken_photos(in);
            refresh_sources();
        }
        ImGui::EndDisabled();
        ui::help_on_hover(ldmsg::capture_camera_fps_help, {fps_label(own)});
        ImGui::PopID();
    }
    // Only beside photographs of the user's own: alone, the capture's are all the dataset has.
    if (!in.cameras.empty() && !_sources.empty() && !_lidar_in_frame) {
        ImGui::BeginDisabled(dataset_busy());
        if (ui::Checkbox(ldmsg::scan_photos_place_only, &in.place_only)) refresh_sources();
        ImGui::EndDisabled();
        ui::help_on_hover(ldmsg::scan_photos_place_only_help);
    }
}

void GuiApp::draw_lidar_options() {
    if (_lidar.empty()) return;
    // A capture's photographs are never left out: its points have no colour,
    // so views rendered from them would place nothing. Its row says how they are used.
    bool photos = false;
    for (const LidarInput& l : _lidar) photos = photos || (l.photos > 0 && l.cameras.empty());
    if (scanner_poses_offered()) {
        ui::Checkbox(ldmsg::keep_scanner_poses, &_lidar_keep_poses);
        ui::help_on_hover(ldmsg::keep_scanner_poses_help);
    } else if (photos && !_sources.empty() && !_lidar_in_frame) {
        ui::Checkbox(ldmsg::use_scan_photos, &_lidar_photos);
        ui::help_on_hover(ldmsg::use_scan_photos_help);
    }
    if (!_sources.empty()) {
        ui::Checkbox(ldmsg::scan_in_frame, &_lidar_in_frame);
        ui::help_on_hover(ldmsg::scan_in_frame_help);
    }
    if (effective_engine() != Engine::BuiltIn)
        ui::TextColoredWrapped(kWarnColor, ldmsg::scan_needs_builtin);
    else
        ui::TextDisabled(ldmsg::scan_replaces_geometry);
}

}  // namespace gui

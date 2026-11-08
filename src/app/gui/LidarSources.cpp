// Laser scans on the dataset screen: the files, what they hold, and the
// switches a run with them has (app/gui/LidarStep.h does the work).

#include "app/gui/GuiApp.h"

#include "app/LidarDataset.h"
#include "app/gui/Ui.h"
#include "checkpoint/SplatPly.h"
#include "data/PointCloudFile.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/Lidar.h"
#include "mesh/MeshImport.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;
namespace dmsg = spirula::i18n::msg::dataset;
namespace ldmsg = spirula::i18n::msg::lidar;
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
    return ext == ".e57" || ext == ".las" || ext == ".laz";
}

bool GuiApp::ply_is_point_cloud(const std::string& path) {
    return lower_ext(path) == ".ply" && !spirula::is_splat_ply(path) &&
           !meshing::ply_is_mesh(path);
}

bool GuiApp::read_lidar_input(const std::string& path, LidarInput& in) {
    std::error_code ec;
    in = LidarInput{};
    in.path = in.edit = fs::absolute(path, ec).string();
    try {
        spirula::cloud::Reader r(in.path);
        if (r.e57())
            for (const auto& im : r.e57()->images()) in.photos += im.has_pose;
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
        if (std::none_of(_lidar.begin(), _lidar.end(),
                         [&](const LidarInput& l) { return l.path == in.path; }))
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
    }
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
                      {".e57", ".las", ".ply"});
        }
        ImGui::SameLine();
        if (ui::Button(dmsg::remove)) remove = (int)i;
        if (one_line) ImGui::SameLine();
        ui::Text(ldmsg::scan_row, {(long long)_lidar[i].points, (long long)_lidar[i].photos});
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

void GuiApp::draw_lidar_options() {
    if (_lidar.empty()) return;
    bool photos = false;
    for (const LidarInput& l : _lidar) photos = photos || l.photos > 0;
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

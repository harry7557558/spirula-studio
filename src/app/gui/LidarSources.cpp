// Laser scans on the dataset screen: the files, what they hold, and the two
// switches a run with them has (app/gui/LidarStep.h does the work).

#include "app/gui/GuiApp.h"

#include "app/gui/Ui.h"
#include "data/PointCloudFile.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/Lidar.h"

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
}  // namespace

bool GuiApp::is_lidar_drop(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    for (char& c : ext) c = (char)std::tolower((unsigned char)c);
    // A .ply is as likely a splat model, which the viewer opens.
    return ext == ".e57" || ext == ".las" || ext == ".laz";
}

void GuiApp::add_lidar_sources(const std::vector<std::string>& paths) {
    for (const std::string& p : paths) {
        if (p.empty()) continue;
        std::error_code ec;
        const std::string abs = fs::absolute(p, ec).string();
        if (std::any_of(_lidar.begin(), _lidar.end(),
                        [&](const LidarInput& l) { return l.path == abs; }))
            continue;
        LidarInput in;
        in.path = abs;
        try {
            spirula::cloud::Reader r(abs);
            if (r.e57())
                for (const auto& im : r.e57()->images()) in.photos += im.has_pose;
            in.summary = format(ldmsg::scan_row, {fs::path(abs).filename().string(),
                                                  (long long)r.info().points,
                                                  (long long)in.photos});
        } catch (const std::exception& e) {
            log(format(ldmsg::scan_unreadable, {abs, std::string(e.what())}));
            continue;
        }
        _lidar.push_back(in);
    }
    // A scan alone is a dataset of its photographs, written beside it.
    if (_sources.empty() && _workspace.empty() && !_lidar.empty()) {
        std::error_code ec;
        _workspace = _workspace_auto =
            fs::path(_lidar[0].path).replace_extension("").string() + "_dataset";
    }
}

bool GuiApp::add_xgrids_export(const std::string& dir) {
    // LCC Studio's developer_data/: raw/ (the original lenses) and perspective/
    // (pinhole crops of them), each a COLMAP dataset, beside the LiDAR map.
    fs::path root(dir);
    std::error_code ec;
    if (fs::is_directory(root / "developer_data", ec)) root /= "developer_data";
    if (root.filename() == "raw" || root.filename() == "perspective") root = root.parent_path();
    if (!fs::is_directory(root / "raw" / "sparse", ec) ||
        !fs::is_directory(root / "raw" / "images", ec))
        return false;
    std::vector<std::string> clouds;
    for (const auto& e : fs::directory_iterator(root, ec)) {
        std::string ext = e.path().extension().string();
        for (char& c : ext) c = (char)std::tolower((unsigned char)c);
        if (e.is_regular_file(ec) && ext == ".las") clouds.push_back(e.path().string());
    }
    if (clouds.empty()) return false;
    add_dataset_folder((root / "raw").string());
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

LidarJob GuiApp::lidar_job() const {
    LidarJob j;
    for (const LidarInput& l : _lidar) j.clouds.push_back(l.path);
    j.scan_photos = _lidar_photos;
    j.in_frame = _lidar_in_frame;
    j.flip_masks = _use_found_masks && _flip_found_masks;
    return j;
}

void GuiApp::draw_lidar_sources() {
    if (ui::Button(ldmsg::add_scan)) {
        open_pick(PickAction::LidarSource, ldmsg::pick_scan.get(), FileDialog::Mode::File,
                  {".e57", ".las", ".ply"}, "", /*multi=*/true);
    }
    ui::help_on_hover(ldmsg::add_scan_help);
    if (_lidar.empty()) return;

    int remove = -1;
    bool photos = false;
    for (size_t i = 0; i < _lidar.size(); i++) {
        ImGui::PushID((int)i);
        ui::TextRaw(_lidar[i].summary);
        ImGui::SameLine();
        if (ui::Button(dmsg::remove)) remove = (int)i;
        ImGui::PopID();
        photos = photos || _lidar[i].photos > 0;
    }
    if (remove >= 0) _lidar.erase(_lidar.begin() + remove);
    ImGui::Indent();
    if (photos && !_lidar_in_frame) {
        ui::Checkbox(ldmsg::use_scan_photos, &_lidar_photos);
        ui::help_on_hover(ldmsg::use_scan_photos_help);
    }
    ui::Checkbox(ldmsg::scan_in_frame, &_lidar_in_frame);
    ui::help_on_hover(ldmsg::scan_in_frame_help);
    if (effective_engine() != Engine::BuiltIn)
        ui::TextColoredWrapped(kWarnColor, ldmsg::scan_needs_builtin);
    else
        ui::TextDisabled(ldmsg::scan_replaces_geometry);
    ImGui::Unindent();
}

}  // namespace gui

// The training seed cloud: sparse or dense, and the dense versions on disk.

#include "app/gui/GuiApp.h"

#include "app/gui/GuiAppShared.h"
#include "app/gui/Ui.h"
#include "dense/Artifact.h"
#include "dense/Clouds.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/Dense.h"
#include "i18n/catalog/Edit.h"
#include "i18n/catalog/Gui.h"
#include "i18n/catalog/TrainFields.h"

#include "imgui.h"

#include <algorithm>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;
namespace i18n = spirula::i18n;
namespace msg = spirula::i18n::msg::gui;
namespace emsg = spirula::i18n::msg::edit;
namespace fld = spirula::i18n::msg::field;

namespace gui {

bool GuiApp::dense_cloud_present() {
    // Polled, not cached for good: a dense run in another window can land.
    const double now = ImGui::GetTime();
    if (_cfg.data != _dense_probe_data || now - _dense_probe_time > 1.0) {
        _dense_probe_data = _cfg.data;
        _dense_probe_time = now;
        _dense_probe_found = !_cfg.data.empty() && dense_completed(_cfg.data);
    }
    return _dense_probe_found;
}

void GuiApp::refresh_dense_versions(const std::string& dataset, bool force) {
    const double now = ImGui::GetTime();
    if (!force && dataset == _dense_versions_dataset && now - _dense_versions_at < 1.0) return;
    const std::string selected = _dense_version_index >= 0 && _dense_version_index < (int)_dense_versions.size()
        ? _dense_versions[(size_t)_dense_version_index].first : std::string{};
    _dense_versions_dataset = dataset;
    _dense_versions_at = now;
    _dense_versions.clear();
    if (!dataset.empty())
        for (const auto& v : spirula::dense::clouds(dataset))
            _dense_versions.emplace_back(v.relative, v.current);
    _dense_version_index = 0;
    for (size_t i = 0; i < _dense_versions.size(); i++)
        if (_dense_versions[i].first == selected) _dense_version_index = (int)i;
}

void GuiApp::edit_dense_cloud(const DatasetFolders& folders) {
    if (native_work_busy()) return;
    _sparse_edit_src = folders;
    refresh_dense_versions(folders.dir, true);
    _dense_version_error.clear();
    if (_dense_versions.size() == 1) {
        _edit_after_open = true;
        request_open_splat((fs::path(folders.dir) / "dense" / _dense_versions[0].first).string());
    } else if (!_dense_versions.empty()) _dense_versions_open = true;
}

void GuiApp::draw_dense_versions() {
    namespace dm = spirula::i18n::msg::dense;
    if (_dense_versions_open) {
        _dense_versions_open = false;
        ui::OpenPopup(dm::cloud_versions);
    }
    ImGui::SetNextWindowSize(ImVec2(px(560), 0), ImGuiCond_Appearing);
    if (!ui::BeginPopupModal(dm::cloud_versions)) return;
    refresh_dense_versions(_dense_versions_dataset);
    ui::TextWrapped(dm::cloud_versions_help);
    ImGui::BeginDisabled(native_work_busy());
    ImGui::BeginChild("##dense_versions", ImVec2(0, px(190)), ImGuiChildFlags_Borders);
    for (size_t i = 0; i < _dense_versions.size(); i++) {
        const auto& [path, current] = _dense_versions[i];
        const auto label = current ? i18n::format(dm::cloud_version_current, {path}) : path;
        if (ui::SelectableRaw(label, _dense_version_index == (int)i)) _dense_version_index = (int)i;
    }
    ImGui::EndChild();
    ImGui::BeginDisabled(_dense_versions.empty());
    if (ui::Button(emsg::enter_edit)) {
        const auto path = fs::path(_dense_versions_dataset) / "dense" /
                          _dense_versions[(size_t)_dense_version_index].first;
        _edit_after_open = true;
        ImGui::CloseCurrentPopup();
        request_open_splat(path.string());
    }
    ImGui::SameLine();
    if (ui::Button(dm::delete_cloud_version)) {
        _dense_delete_selection = _dense_versions[(size_t)_dense_version_index].first;
        ui::OpenPopup(dm::delete_cloud_version);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ui::Button(msg::cancel)) ImGui::CloseCurrentPopup();
    ImGui::EndDisabled();
    if (!_dense_version_error.empty()) ui::TextColoredWrappedRaw(kErr, _dense_version_error);
    if (ui::BeginPopupModal(dm::delete_cloud_version)) {
        const std::string path = _dense_delete_selection;
        ui::TextWrapped(dm::delete_cloud_version_help, {path});
        if (ui::Button(dm::delete_cloud_version)) {
            const auto dataset = _dense_versions_dataset;
            _dense_deleted_path = (fs::path(dataset) / "dense" / path).lexically_normal().string();
            _dense_delete = std::async(std::launch::async, [dataset, path] {
                spirula::dense::delete_cloud(dataset, path);
            });
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ui::Button(msg::cancel)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::EndPopup();
}

void GuiApp::pump_dense_delete() {
    if (!_dense_delete.valid() ||
        _dense_delete.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    try {
        _dense_delete.get();
        _dense_version_error.clear();
        refresh_dense_versions(_dense_versions_dataset, true);
        _dense_probe_time = _seed_files_time = -1.0;
        _seed_count_path.clear();
        if (_cfg.data == _dense_versions_dataset) {
            const auto seed = (fs::path(_cfg.data) / _cfg.seed_pointcloud).lexically_normal();
            if (seed == fs::path(_dense_deleted_path)) {
                _cfg.seed_pointcloud.clear();
                _app_seed.clear();
            }
            _parse_dirty = true;
        }
    } catch (const std::exception& e) { _dense_version_error = e.what(); log(e.what()); }
}

// Probed at most once a second, as dense_cloud_present() is: the editor saves
// these while the trainer shows the dataset.
const std::vector<GuiApp::SeedFile>& GuiApp::dataset_seed_files() {
    const double now = ImGui::GetTime();
    if (_cfg.data == _seed_files_data && now - _seed_files_time <= 1.0) return _seed_files;
    _seed_files_data = _cfg.data;
    _seed_files_time = now;
    _seed_files.clear();
    if (_cfg.data.empty()) return _seed_files;
    std::error_code ec;
    const fs::path root(_cfg.data);
    auto entries = [&](const fs::path& dir, bool folders) {
        std::vector<fs::path> out;
        for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
            if (folders ? it->is_directory(ec) : it->is_regular_file(ec)) out.push_back(it->path());
        std::sort(out.begin(), out.end());
        return out;
    };
    for (const auto& v : spirula::dense::clouds(_cfg.data)) {
        if (v.current) continue;
        const auto label = i18n::format(msg::seed_choice_dense_file, {v.relative});
        _seed_files.push_back({label, "dense/" + v.relative, true});
    }
    // The model the dataset itself loads is "Sparse points" already.
    const std::string loaded = find_colmap_model(_cfg.data, _cfg.colmap_recon_dir);
    for (const fs::path& m : entries(root / "sparse", true)) {
        const bool points = fs::is_regular_file(m / "points3D.bin", ec) ||
                            fs::is_regular_file(m / "points3D.txt", ec);
        if (!points || (!loaded.empty() && fs::equivalent(m, loaded, ec))) continue;
        const std::string name = m.filename().string();
        _seed_files.push_back({i18n::format(msg::seed_choice_sparse_model, {name}),
                               "sparse/" + name, false});
    }
    return _seed_files;
}

void GuiApp::draw_seed_cloud(TrainRunner::Phase ph) {
    enum { Dense, Sparse, Other, File };   // File + i is dataset_seed_files()[i]
    const bool dense = dense_cloud_present();
    const std::vector<SeedFile>& files = dataset_seed_files();
    std::vector<int> ids;
    std::vector<std::string> labels;
    auto add = [&](int id, const std::string& label) { ids.push_back(id); labels.push_back(label); };
    auto add_files = [&](bool of_dense) {
        for (size_t i = 0; i < files.size(); i++)
            if (files[i].dense == of_dense) add(File + (int)i, files[i].label);
    };
    if (dense) add(Dense, msg::seed_choice_dense.get());
    add_files(true);
    add(Sparse, msg::seed_choice_sparse.get());
    add_files(false);
    add(Other, msg::seed_choice_other.get());
    const std::string& seed = _cfg.seed_pointcloud;
    const std::string listed = fs::path(seed).lexically_normal().generic_string();
    int file = -1;
    for (size_t i = 0; i < files.size() && file < 0; i++)
        if (files[i].seed == listed) file = File + (int)i;
    const int choice = _seed_other && seed.empty() ? Other
        : seed.empty() ? (dense ? Dense : Sparse)
        : seed == spirula::dense::kSparseSeed ? Sparse
        : file >= 0 ? file
        : spirula::dense::is_dense_seed(_cfg.data, seed) ? Dense : Other;
    int at = (int)(std::find(ids.begin(), ids.end(), choice) - ids.begin());
    if (at == (int)ids.size()) at = (int)ids.size() - 1;
    std::vector<const char*> items;
    for (const std::string& l : labels) items.push_back(l.c_str());
    ImGui::SetNextItemWidth(px(170.0f));
    if (ui::ComboRaw(ui::detail::label(msg::seed_choice), &at, items.data(), (int)items.size())) {
        _seed_other = ids[at] == Other;
        if (ids[at] == Sparse) _cfg.seed_pointcloud = spirula::dense::kSparseSeed;
        else if (ids[at] >= File) _cfg.seed_pointcloud = files[(size_t)(ids[at] - File)].seed;
        else if (ids[at] == Dense || choice != Other) _cfg.seed_pointcloud.clear();
        _cfg_ui.touched.insert("seed_pointcloud");
    }
    ui::help_on_hover_disabled(msg::seed_choice_help);
    if (ids[at] == Other) {
        ImGui::Indent();
        ImGui::SetNextItemWidth(px(-70.0f));
        if (ui::InputTextRaw("##seed_pointcloud", &_cfg.seed_pointcloud))
            _cfg_ui.touched.insert("seed_pointcloud");
        ui::help_on_hover_disabled(fld::seed_pointcloud_help);
        ImGui::SameLine();
        if (ui::ButtonRaw("...##seed_pointcloud_pick", ImVec2(px(kBrowseButtonW), 0))) {
            _seed_other = true;
            open_pick(PickAction::SeedPointcloud, fld::seed_pointcloud.get(),
                      FileDialog::Mode::File, {".ply"});
        }
        ImGui::Unindent();
    }

    // Only the cases where the choice above is not what initializes.
    auto* session = _runner.session();
    const bool parsed = session && ph != TrainRunner::Phase::Loading &&
        ph != TrainRunner::Phase::Preparing && ph != TrainRunner::Phase::LoadError &&
        parse_settings_equal(session->cfg, _cfg);
    const bool random = _cfg.random_init == "always" ||
        (_cfg.random_init == "auto" && parsed && session->random_seeded);
    const bool splat_only = !_cfg.init_ply.empty() && !_cfg.init_ply_add_points;
    if (!_cfg.resume.empty())
        ui::TextWrapped(msg::seed_source_resume);
    else if (!_cfg.init_ply.empty())
        ui::TextWrapped(_cfg.init_ply_add_points ? msg::seed_source_splat_add : msg::seed_source_splat);
    if (_cfg.resume.empty() && !splat_only && random)
        ui::TextWrapped(msg::seed_source_random);
    if (!seed.empty() && seed != spirula::dense::kSparseSeed && (!_cfg.resume.empty() || random || splat_only))
        ui::TextColoredWrapped(kWarn, msg::seed_cloud_unused);
}

}  // namespace gui

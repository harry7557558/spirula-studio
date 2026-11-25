// Depth and normal estimation options on the dataset screen.

#include "app/gui/GuiApp.h"

#include "app/gui/ProgressView.h"
#include "app/gui/Ui.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/Geometry.h"

#include "imgui.h"

#include <algorithm>

namespace i18n = spirula::i18n;
namespace dmsg = spirula::i18n::msg::dataset;
namespace gmsg = spirula::i18n::msg::geometry;

namespace gui {

bool GuiApp::geometry_model_missing() const {
    if (!_geometry.enable) return false;
    if (!geometry_availability().empty()) return false;
    return !geometry_model_cached(_geometry.model);
}

void GuiApp::request_geometry_download() {
    start_downloads_with_consent(_geom_download, geometry_model_downloads(_geometry.model));
}

void GuiApp::open_geometry_preview() {
    // The preview loads a depth backbone, so it is a GPU-consuming operation
    // and freezes the one choice before the panel starts.
    if (native_work_busy()) return;
    pump_source_probes();
    if (!_source_probes_ready) {
        log(dmsg::sensors_reading.get());
        return;
    }
    // One multi-gigabyte backbone at a time: the mask preview holds SAM and
    // this one holds Metric3D, and the inference layer's pool is process-wide.
    stop_inference_users();
    close_splat();
    if (!freeze_native_device()) return;
    const size_t idx =
        _sources.empty() ? 0
                         : std::min((size_t)_mask_preview_input,
                                    _sources.size() - 1);
    std::string lens = _sfm_job.camera_model;
    float focal = 0.0f;
    if (!_sources.empty()) source_lens(idx, lens, focal);
    _geometry_panel.open(preview_source(idx), _workspace,
                         planned_image_dir(_sources, _workspace, _photo_import),
                         lens, focal);
}

void GuiApp::draw_geometry_options() {
    const std::string why = geometry_availability();
    if (!why.empty()) {
        // Nothing here can work in this build, so offer no switch at all --
        // one that fails when the run reaches it is worse than its absence.
        ui::TextDisabledWrapped(dmsg::geom_unavailable);
        ui::help_on_hover_raw(why.c_str());
        return;
    }

    ui::Checkbox(dmsg::geom_enable, &_geometry.enable);
    ui::help_on_hover(dmsg::geom_enable_help);
    if (!_geometry.enable) return;

    ImGui::Indent();

    // ---- the checkpoint ----
    const auto& catalog = geometry_models();
    int model_idx = 1;
    for (size_t i = 0; i < catalog.size(); i++)
        if (_geometry.model == catalog[i].id) model_idx = (int)i;
    ImGui::SetNextItemWidth(px(kFieldW));
    if (ui::BeginCombo(dmsg::geom_model, catalog[(size_t)model_idx].label->get())) {
        for (size_t i = 0; i < catalog.size(); i++) {
            const bool cached = geometry_model_cached(catalog[i].id);
            const std::string label =
                cached ? std::string(catalog[i].label->get())
                       : i18n::format(dmsg::mask_model_needs_download,
                                      {catalog[i].label->get()});
            if (ui::SelectableRaw(label, (int)i == model_idx))
                _geometry.model = catalog[i].id;
            if (ImGui::IsItemHovered()) ui::SetTooltip(*catalog[i].blurb);
        }
        ImGui::EndCombo();
    }
    ui::TextDisabledWrapped(*catalog[(size_t)model_idx].blurb);

    FileDownload& geom_dl = _geom_download.current();
    if (_geom_download.running()) {
        // The overlay is a byte count from curl, not a sentence.
        download_bar(geom_dl, geom_dl.status());
        ImGui::SameLine();
        // The mask download's Stop button carries the same message; two of
        // them can be on screen at once, so this one needs its own ID.
        ImGui::PushID("geomdl");
        if (ui::Button(dmsg::stop)) _geom_download.cancel();
        ImGui::PopID();
    } else if (geometry_model_missing()) {
        // Checked before a 45 GB stream starts, not discovered at 80%.
        const GeometryDisk disk = geometry_model_disk(_geometry.model);
        ImGui::BeginDisabled(!disk.fits);
        if (ui::Button(dmsg::geom_get_model)) request_geometry_download();
        ImGui::EndDisabled();
        ImGui::SameLine();
        ui::TextDisabledRaw(human_bytes(catalog[(size_t)model_idx].bytes));
        if (disk.needed && disk.fits)
            ui::TextDisabledWrapped(dmsg::geom_install_disk,
                                    {human_bytes(disk.disk), disk.where, human_bytes(disk.free)});
        if (!disk.fits)
            ui::TextColoredWrapped(kErr, dmsg::geom_install_no_disk,
                                   {human_bytes(disk.disk), disk.where, human_bytes(disk.spare),
                                    human_bytes(disk.free)});
        if (geom_dl.state() == FileDownload::State::Failed)
            ui::TextColoredWrappedRaw(kErr, geom_dl.status());
    } else {
        ui::TextColored(kOk, dmsg::geom_model_ready);
    }

    // ---- what it writes ----
    ui::Checkbox(dmsg::geom_write_normals, &_geometry.want_normal);
    ImGui::SameLine();
    ui::Checkbox(dmsg::geom_write_depth, &_geometry.want_depth);
    ui::help_on_hover(gmsg::opt_depth);
    if (!_geometry.want_normal && !_geometry.want_depth)
        ui::TextColoredWrapped(kWarn, dmsg::geom_nothing_to_write);

    // Behind the checkpoint, unlike "Try the mask": there is no half of this
    // panel that works without one, and opening it would have the panel fetch
    // the weights itself.
    const bool geometry_preview_busy = native_work_busy();
    const bool geometry_preview_blocked =
        geometry_preview_busy || !_source_probes_ready || geometry_model_missing();
    ImGui::BeginDisabled(geometry_preview_blocked);
    if (ui::Button(dmsg::geom_try)) open_geometry_preview();
    ImGui::EndDisabled();
    ui::help_on_hover_disabled(
        geometry_preview_busy ? dmsg::step_locked
        : !_source_probes_ready ? dmsg::sensors_reading
        : geometry_model_missing() ? dmsg::geom_model_first : dmsg::geom_try_help);

    if (ui::CollapsingHeader(dmsg::geom_advanced)) {
        ImGui::SetNextItemWidth(px(kFieldW));
        if (ui::InputInt(dmsg::geom_max_size, &_geometry.max_size))
            _geometry.max_size = std::clamp(_geometry.max_size, 224, 4096);
        ui::help_on_hover(gmsg::opt_max_size);

        // Enabled whatever the checkpoint is: a Metric3D run passes it and
        // ignores it, and disabling it would need this screen to know which
        // family an .onnx the user pointed at belongs to.
        ImGui::SetNextItemWidth(px(kFieldW));
        if (ui::InputInt(dmsg::geom_num_tokens, &_geometry.num_tokens))
            _geometry.num_tokens = std::clamp(_geometry.num_tokens, 256, 8192);
        ui::help_on_hover(gmsg::opt_num_tokens);

        // png / jpg / relative / mm are what config.json and the flag spell,
        // so the values stay as they are and the label carries the meaning.
        ImGui::SetNextItemWidth(px(kFieldW));
        int fmt = _geometry.normal_jpg ? 1 : 0;
        static const char* kFormats[] = {"png", "jpg"};
        if (ui::ComboRaw(ui::detail::label(dmsg::geom_normal_format), &fmt,
                         kFormats, 2))
            _geometry.normal_jpg = fmt == 1;
        ui::help_on_hover(gmsg::opt_normal_format);
        if (_geometry.normal_jpg) {
            ImGui::SetNextItemWidth(px(kFieldW));
            if (ui::InputInt(dmsg::geom_jpeg_quality, &_geometry.jpeg_quality))
                _geometry.jpeg_quality = std::clamp(_geometry.jpeg_quality, 1, 99);
            ui::help_on_hover(gmsg::opt_jpeg_quality);
        }

        ImGui::BeginDisabled(!_geometry.want_depth);
        ImGui::SetNextItemWidth(px(kFieldW));
        int units = _geometry.depth_mm ? 1 : 0;
        static const char* kUnits[] = {"relative", "mm"};
        if (ui::ComboRaw(ui::detail::label(dmsg::geom_depth_units), &units,
                         kUnits, 2))
            _geometry.depth_mm = units == 1;
        ui::help_on_hover_disabled(gmsg::opt_depth_units);
        ImGui::EndDisabled();

        static const char* kTri[] = {"auto", "yes", "no"};
        ImGui::SetNextItemWidth(px(kFieldW));
        ui::ComboRaw(ui::detail::label(dmsg::geom_split), &_geometry.split, kTri, 3);
        ui::help_on_hover(gmsg::opt_split);
        static const char* kFaceRes[] = {"output", "source"};
        ImGui::SetNextItemWidth(px(kFieldW));
        ui::ComboRaw(ui::detail::label(dmsg::geom_face_res), &_geometry.face_res,
                     kFaceRes, 2);
        ui::help_on_hover(gmsg::opt_face_res);
        ImGui::SetNextItemWidth(px(kFieldW));
        ui::ComboRaw(ui::detail::label(dmsg::geom_ray_depth), &_geometry.ray_depth,
                     kTri, 3);
        ui::help_on_hover(gmsg::opt_ray_depth);

        ui::Checkbox(dmsg::geom_overwrite, &_geometry.overwrite);
        ui::help_on_hover(gmsg::opt_overwrite);
    }

    ImGui::Unindent();
}

}  // namespace gui

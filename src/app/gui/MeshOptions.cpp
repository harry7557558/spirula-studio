#include "app/gui/GuiApp.h"
#include "app/gui/GuiAppShared.h"
#include "app/gui/OptionSearch.h"
#include "app/gui/Ui.h"
#include "i18n/catalog/Gui.h"
#include "i18n/catalog/Mesh.h"
#include "i18n/catalog/MeshOptions.h"
#include "i18n/catalog/TrainFields.h"
#include "i18n/catalog/SfmOptions.h"
#include "i18n/catalog/SfmFields.h"

#include <string_view>
#include <type_traits>

namespace gui {
namespace msg = spirula::i18n::msg::gui;
namespace opt = spirula::i18n::msg::meshoption;
namespace fld = spirula::i18n::msg::field;
namespace help = spirula::i18n::msg::mesh;
using spirula::i18n::Msg;

namespace {
enum Tier { Basic, Advanced, Expert };
enum Group { Source, Photos, Surface, Merge, Cleanup, Color, Texture, Output, Runtime };
const Msg* const headings[] = {&msg::mesh_source, &msg::section_dataset, &msg::mesh_method,
    &help::stage_merge, &help::stage_cleanup, &msg::mesh_color, &msg::mesh_color_texture,
    &opt::export_group, &spirula::i18n::msg::sfmfield::group_runtime};

class Filter {
public:
    Filter(int tier, const std::string& query)
        : tier(tier), tokens(option_search::query_tokens(query.c_str())) {}

    bool show(Group group, Tier rank, const char* flags, const Msg& label, const Msg* hint = nullptr) {
        if (tokens.empty()) {
            if (rank > tier) return false;
        } else {
            std::string text(flags);
            option_search::add_text(text, *headings[group]);
            option_search::add_text(text, label);
            if (hint) option_search::add_text(text, *hint);
            if (!option_search::matches(option_search::lower_ascii(text), tokens)) return false;
        }
        if (last != group) ui::SeparatorText(*headings[group]);
        last = group;
        return true;
    }
    bool empty() const { return last == -1; }
private:
    int tier, last = -1;
    std::vector<std::string> tokens;
};

const Msg* const color_names[] = {&msg::mesh_color_none, &msg::mesh_color_vertex, &msg::mesh_color_texture};
const Msg* const color_help[] = {&opt::color_none_help, &opt::color_vertex_help, &opt::color_texture_help};

template<size_t N, typename ChoiceHelp>
bool choose(const Msg& label, const Msg& tip, const char* const (&items)[N], int& selected, ChoiceHelp choice_help) {
    if (!ui::BeginCombo(label, items[selected])) {
        ui::help_on_hover_disabled(tip);
        return false;
    }
    bool changed = false;
    for (size_t i = 0; i < N; ++i) {
        if (ui::SelectableRaw(items[i], selected == int(i))) {
            selected = int(i);
            changed = true;
        }
        choice_help(i);
    }
    ImGui::EndCombo();
    return changed;
}

void numeric(const char* label, float& value) {
    if (ui::InputFloatRaw(label, &value, "%.6g")) value = std::max(0.0f, value);
}
void numeric(const char* label, int& value) {
    if (ui::InputIntRaw(label, &value)) value = std::max(0, value);
}

}  // namespace

void GuiApp::draw_mesh_options() {
    ui::SeparatorText(msg::section_preset);
    const bool locked = _mesh.busy() || _batch_active;
    ImGui::BeginDisabled(locked);
    draw_mesh_preset_picker();
    ImGui::EndDisabled();
    ImGui::SetNextItemWidth(-px(132));
    ui::InputTextWithHintRaw("##meshsearch", msg::cfg_search_hint, &_mesh_search);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    const Msg* tiers[] = {&msg::cfg_tier_basic, &msg::cfg_tier_advanced, &msg::mesh_tier_expert};
    if (ui::BeginComboRaw("##meshtier", tiers[_mesh_tier]->get())) {
        for (int i = Basic; i <= Expert; ++i)
            if (ui::Selectable(*tiers[i], i == _mesh_tier)) _mesh_tier = i;
        ImGui::EndCombo();
    }
    ui::help_on_hover(msg::cfg_tier_help);
    ImGui::BeginChild("##meshrows");
    ImGui::BeginDisabled(locked);
    Filter filter(_mesh_tier, _mesh_search);
    MeshJob& job = _mesh_job;
    auto path = [&](const char* id, std::string& value, PickAction action, const Msg& title,
                    const Msg& hint, bool model = false) {
        ImGui::PushID(id);
        ImGui::SetNextItemWidth(-px(model ? 106 : 54));
        ui::InputTextWithHintRaw(id, hint, &value);
        ui::help_on_hover_disabled(hint);
        ImGui::SameLine();
        if (ui::ButtonRaw("...", ImVec2(px(44), 0)))
            open_pick(action, title.get(), FileDialog::Mode::Folder);
        if (model) {
            ImGui::SameLine();
            if (ui::ButtonRaw(".ply", ImVec2(px(44), 0)))
                open_pick(action, title.get(), FileDialog::Mode::File, {".ply"});
        }
        ImGui::PopID();
    };
    if (filter.show(Source, Basic, "checkpoint ply", msg::mesh_source, &msg::mesh_source_help)) {
        path("##meshsrc", job.checkpoint, PickAction::MeshSource, msg::mesh_pick_model, msg::mesh_source_help, true);
        ui::TextDisabled(msg::mesh_drop_hint);
    }
    if (filter.show(Photos, Basic, "no-data cameras", msg::mesh_use_photos, &msg::mesh_use_photos_help)) {
        ui::Checkbox(msg::mesh_use_photos, &job.use_data);
        ui::help_on_hover(msg::mesh_use_photos_help);
        if (!job.use_data) ui::TextColoredWrapped(kWarn, msg::mesh_no_photos_warn);
    }
    if (job.use_data && filter.show(Photos, Basic, "data", msg::mesh_photos_dir, &msg::mesh_photos_dir_help)) {
        ui::TextDisabled(msg::mesh_photos_dir);
        path("##meshdata", job.data_dir, PickAction::MeshPhotos, msg::mesh_pick_photos, msg::mesh_photos_dir_help);
        if (job.use_data && !job.checkpoint.empty() && !mesh_dataset_found())
            ui::TextColoredWrapped(kWarn, msg::mesh_photos_missing_warn);
    }
    if (job.use_data && filter.show(Photos, Advanced, "max-cameras", msg::mesh_max_cameras, &msg::mesh_max_cameras_help)) {
        ImGui::SetNextItemWidth(px(kCompactFieldW));
        ui::InputInt(msg::mesh_max_cameras, &job.max_cameras);
        job.max_cameras = std::max(0, job.max_cameras);
        ui::help_on_hover_disabled(msg::mesh_max_cameras_help);
    }
    if (job.use_data && filter.show(Photos, Expert, "data-format colmap nerfstudio metashape", fld::data_format, &fld::data_format_help)) {
        const char* items[] = {msg::mesh_texture_size_auto.get(), "colmap", "nerfstudio", "metashape"};
        int idx = 0;
        for (int i = 1; i < 4; ++i) if (job.data_format == items[i]) idx = i;
        ImGui::SetNextItemWidth(px(kFieldW));
        if (choose(fld::data_format, fld::data_format_help, items, idx, [&](size_t i) {
                if (i) ui::help_on_hover(opt::format_reader_help, {items[i]});
                else ui::help_on_hover(fld::data_format_help);
            })) job.data_format = idx ? items[idx] : "";
    }
    if (job.use_data && filter.show(Surface, Advanced, "method opacity tsdf", msg::mesh_method, &msg::mesh_method_help)) {
        int method = job.tsdf ? 1 : 0;
        ImGui::SetNextItemWidth(px(kFieldW));
        if (ui::ComboWithHelp(msg::mesh_method, &method, msg::mesh_method_help,
                              {{msg::mesh_method_opacity, opt::opacity_help}, {msg::mesh_method_tsdf, opt::tsdf_help}}))
            job.tsdf = method == 1;
    }

    auto numbers = [&](Group section) {
        const bool tsdf = job.use_data && job.tsdf;
        const bool voxel_grid = tsdf && job.tsdf_voxel > 0;
        const bool merging = job.merge_factor > 0;
        const bool textured = job.wants_color(2);
        auto field = [&](Group group, Tier tier, const char* flag, const Msg& label, const Msg& tip, auto& value) {
            if (group != section || !filter.show(group, tier, flag, label, &tip)) return;
            const std::string_view name(flag);
            ImGui::SetNextItemWidth(px(kCompactFieldW));
            const char* widget = ui::detail::label(label);
            if constexpr (std::is_same_v<std::decay_t<decltype(value)>, int>) {
                if (name == "samples-per-gaussian") {
                    const char* items[] = {"1", "7"};
                    int i = value == 1 ? 0 : 1;
                    if (choose(label, tip, items, i, [&](size_t) { ui::help_on_hover(tip); })) value = i ? 7 : 1;
                } else {
                    numeric(widget, value);
                    if (name == "check") value = std::min(2, value);
                }
            } else numeric(widget, value);
            ui::help_on_hover(tip);
        };
#define SS_MESH_CONTROL(type, name, flag, initial, group, tier, tip, visible) \
    if (visible) field(group, tier, flag, opt::name, opt::tip, job.name);
        SS_MESH_NUMERIC_FIELDS(SS_MESH_CONTROL)
#undef SS_MESH_CONTROL
    };
    numbers(Surface);
    if (filter.show(Merge, Advanced, "merge-factor", msg::mesh_detail, &msg::mesh_detail_help)) {
        ImGui::SetNextItemWidth(px(kFieldW));
        ui::SliderFloat(msg::mesh_detail, &job.merge_factor, 0, 4, "%.2f");
        ui::help_on_hover(msg::mesh_detail_help);
    }
    numbers(Merge);
    if (filter.show(Cleanup, Advanced, "floater-min-faces", msg::mesh_drop_specks, &opt::floater_help)) {
        ImGui::SetNextItemWidth(px(kCompactFieldW));
        ui::InputInt(msg::mesh_drop_specks, &job.floater_min_faces);
        job.floater_min_faces = std::max(0, job.floater_min_faces);
        ui::help_on_hover(opt::floater_help);
    }
    if (job.use_data && filter.show(Cleanup, Advanced, "no-cull-unseen", msg::mesh_cull_unseen, &opt::cull_help)) {
        ui::Checkbox(msg::mesh_cull_unseen, &job.cull_unseen);
        ui::help_on_hover(opt::cull_help);
    }
    numbers(Cleanup);
    const char* color_flags[] = {"color none", "color vertex", "color texture"};
    bool drew_color = false;
    for (int i = 0; i < kNumMeshColorModes; ++i) {
        if (filter.show(Color, Basic, color_flags[i], *color_names[i], color_help[i])) {
            if (drew_color && ImGui::GetContentRegionAvail().x > px(400)) ImGui::SameLine();
            ui::Checkbox(*color_names[i], &job.colors[i]);
            ui::help_on_hover(*color_help[i]);
            drew_color = true;
        }
    }
    if (job.wants_color(2) && filter.show(Texture, Advanced, "texture-size", msg::mesh_texture_size, &opt::texture_size_help)) {
        const char* items[] = {msg::mesh_texture_size_auto.get(), "1024", "2048", "4096", "8192"};
        const int sizes[] = {0, 1024, 2048, 4096, 8192};
        int idx = 0;
        for (int i = 1; i < 5; ++i) if (sizes[i] == job.texture_size) idx = i;
        ImGui::SetNextItemWidth(px(kCompactFieldW));
        if (choose(msg::mesh_texture_size, opt::texture_size_help, items, idx, [&](size_t i) {
                if (i) ui::help_on_hover(opt::texture_size_help);
                else ui::help_on_hover(help::help_texture_size, {0});
            })) job.texture_size = sizes[idx];
    }
    numbers(Texture);
    bool drew_format = false;
    for (int i = 0; i < kNumMeshFormats; ++i) {
        bool supported = false;
        for (int c = 0; c < kNumMeshColorModes; ++c)
            supported |= job.colors[c] && mesh_format_carries(i, c);
        if (!supported) continue;
        const std::string flags = std::string("format ") + kMeshFormats[i];
        if (!filter.show(Output, Basic, flags.c_str(), msg::mesh_formats)) continue;
        if (drew_format) ImGui::SameLine();
        else ui::TextDisabled(msg::mesh_formats);
        drew_format = true;
        ui::CheckboxRaw(kMeshFormats[i], &job.formats[i]);
        if (ImGui::IsItemHovered()) {
            std::string modes;
            for (int c = 0; c < kNumMeshColorModes; ++c) {
                if (!mesh_format_carries(i, c)) continue;
                if (!modes.empty()) modes += ", ";
                modes += color_names[c]->get();
            }
            ui::help_on_hover(opt::format_help, {kMeshFormats[i], modes});
        }
    }
    if (filter.show(Output, Basic, "output", msg::mesh_output, &msg::mesh_output_help)) {
        ui::TextDisabled(msg::mesh_output);
        path("##meshout", job.output, PickAction::MeshOutput, msg::mesh_pick_output, msg::mesh_output_help);
    }
    if (filter.show(Runtime, Advanced, "device gpu", msg::section_device, &msg::device_help))
        draw_engine_device_picker(native_work_busy());
    numbers(Runtime);
    if (filter.show(Runtime, Expert, "quiet", opt::quiet, &opt::quiet_help)) {
        ui::Checkbox(opt::quiet, &job.quiet);
        ui::help_on_hover(opt::quiet_help);
    }
    if (filter.show(Runtime, Expert, "extra args", msg::mesh_extra_args, &opt::extra_help)) {
        ui::TextDisabled(msg::mesh_extra_args);
        ImGui::SetNextItemWidth(-1);
        ui::InputTextRaw("##meshextra", &job.extra_args);
        ui::help_on_hover(opt::extra_help);
    }
    if (filter.empty()) ui::TextDisabled(spirula::i18n::msg::sfmoption::filter_no_results);
    ImGui::EndDisabled();
    ImGui::EndChild();
}
}  // namespace gui

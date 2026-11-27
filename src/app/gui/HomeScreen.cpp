// The Home screen and the recent list.

#include "app/gui/GuiApp.h"

#include "app/gui/DatasetPrep.h"
#include "app/gui/GlLoader.h"
#include "app/gui/GuiAppShared.h"
#include "app/gui/Subprocess.h"
#include "app/gui/Ui.h"
#include "app_generated/app_banner.h"
#include "external/stb_image.h"
#include "i18n/catalog/Brand.h"
#include "i18n/catalog/Gui.h"
#include "i18n/catalog/Render.h"

#include "imgui.h"

#include <algorithm>
#include <ctime>
#include <filesystem>

namespace fs = std::filesystem;
namespace i18n = spirula::i18n;
namespace msg = spirula::i18n::msg::gui;
namespace rmsg = spirula::i18n::msg::render;
using spirula::i18n::Msg;

namespace gui {

// ---------------------------------------------------------------------------
// The recent list's words and colours, shared by the home screen and the
// File menu
// ---------------------------------------------------------------------------

std::string recent_name(const RecentItem& it) {
    const fs::path p = fs::u8path(it.path);
    const std::string leaf = p.filename().u8string();
    if (leaf.empty()) return it.path;
    const bool file_kind = it.kind == RecentKind::Model || it.kind == RecentKind::Project;
    const std::string parent = p.parent_path().filename().u8string();
    if (!file_kind || !p.has_extension() || parent.empty()) return leaf;
    return parent + "/" + leaf;
}

const Msg& recent_badge(const RecentItem& it) {
    switch (it.kind) {
        case RecentKind::Dataset:        return msg::home_kind_dataset;
        case RecentKind::Reconstruction: return msg::home_kind_recon;
        case RecentKind::Run:            return msg::home_kind_run;
        case RecentKind::Project:        return msg::home_kind_project;
        case RecentKind::Model:          break;
    }
    switch (it.content) {
        case ModelContent::Splats:  return msg::home_kind_splats;
        case ModelContent::Mesh:    return msg::home_kind_mesh;
        case ModelContent::Points:  return msg::home_kind_points;
        case ModelContent::Unknown: break;
    }
    return msg::home_kind_model;
}

std::vector<std::string> recent_models(const RecentList& r) {
    std::vector<std::string> out;
    for (const RecentItem& it : r.items())
        if (it.kind == RecentKind::Model || it.kind == RecentKind::Run) out.push_back(it.path);
    return out;
}

namespace {

std::tm local_tm(std::time_t t) {
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return tm;
}

ImVec4 recent_color(RecentKind k) {
    switch (k) {
        case RecentKind::Dataset:        return ImVec4(0.45f, 0.68f, 1.00f, 1.0f);
        case RecentKind::Reconstruction: return ImVec4(0.38f, 0.82f, 0.76f, 1.0f);
        case RecentKind::Model:          return ImVec4(0.96f, 0.68f, 0.36f, 1.0f);
        case RecentKind::Run:            return ImVec4(0.58f, 0.84f, 0.44f, 1.0f);
        case RecentKind::Project:        return ImVec4(0.80f, 0.62f, 0.98f, 1.0f);
    }
    return kDim;
}

// "Today 14:32", "Yesterday 09:10", or the date. Empty when the entry came
// from a gui.conf that kept no times.
std::string recent_when(int64_t t) {
    if (t <= 0) return {};
    const std::tm then = local_tm((std::time_t)t);
    const std::tm now = local_tm(std::time(nullptr));
    auto same_day = [](const std::tm& a, const std::tm& b) {
        return a.tm_year == b.tm_year && a.tm_yday == b.tm_yday;
    };
    char buf[32];
    std::strftime(buf, sizeof buf, "%H:%M", &then);
    if (same_day(then, now)) return i18n::format(msg::home_recent_today, {buf});
    std::tm yesterday = now;
    yesterday.tm_mday -= 1;
    yesterday.tm_isdst = -1;
    std::mktime(&yesterday);   // normalizes, and fills tm_yday
    if (same_day(then, yesterday)) return i18n::format(msg::home_recent_yesterday, {buf});
    std::strftime(buf, sizeof buf, "%Y-%m-%d", &then);
    return buf;
}

}  // namespace

bool GuiApp::recent_blocked(const RecentItem& it) const {
    if (it.kind != RecentKind::Reconstruction || !native_work_busy()) return false;
    auto norm = [](const std::string& s) { return fs::u8path(s).lexically_normal().make_preferred(); };
    return !dataset_busy() || norm(it.path) != norm(_workspace);
}

// ===========================================================================
// Home
// ===========================================================================

namespace {

// The masthead texture, decoded and uploaded on the first home screen. There
// is one window and the image never changes, so it outlives every caller and
// the driver frees it with the context.
unsigned banner_texture(int* w, int* h) {
    static int bw = 0, bh = 0;
    static const unsigned tex = [] {
        int comp = 0;
        unsigned char* px = stbi_load_from_memory(
            kAppBanner, (int)kAppBannerSize, &bw, &bh, &comp, 4);
        if (!px) return 0u;
        unsigned id = 0;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bw, bh, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, px);
        stbi_image_free(px);
        return id;
    }();
    *w = bw;
    *h = bh;
    return tex;
}

}  // namespace

// The home masthead: the artwork across the whole window, with the product
// name and tagline over its foot. The words are drawn here rather than baked
// into the image so they stay translatable, and the image carries none.
void GuiApp::draw_home_banner(float avail, float indent) {
    int bw = 0, bh = 0;
    const unsigned tex = banner_texture(&bw, &bh);
    if (!tex || bw <= 0 || bh <= 0) return;

    // A share of the window rather than a fixed height: a short window should
    // not lose a third of itself to the masthead. Clamped at both ends -- the
    // floor is what the name and tagline need, the ceiling keeps it a band.
    const float h = std::clamp(ImGui::GetContentRegionAvail().y * 0.21f,
                               px(120.0f), px(320.0f));
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 p1(p0.x + avail, p0.y + h);

    // Full bleed: the artwork is painted out over the host window's padding on
    // the three sides it touches, so the band meets the menu bar and both
    // edges. Only the drawing grows -- p1 stays where the layout below it is.
    const ImVec2 pad = ImGui::GetStyle().WindowPadding;
    const ImVec2 q0(p0.x - pad.x, p0.y - pad.y);
    const ImVec2 q1(p1.x + pad.x, p1.y);
    const float band_w = q1.x - q0.x, band_h = q1.y - q0.y;

    // Cover, not fit: crop to the band's aspect so the artwork fills the width
    // whatever the window is doing.
    const float want = band_w / band_h, have = (float)bw / (float)bh;
    ImVec2 uv0(0.0f, 0.0f), uv1(1.0f, 1.0f);
    const float f = have > want ? want / have : have / want;
    if (have > want) {
        uv0.x = 0.5f - 0.5f * f;
        uv1.x = 0.5f + 0.5f * f;
    } else {
        uv0.y = 0.5f - 0.5f * f;
        uv1.y = 0.5f + 0.5f * f;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddImage((ImTextureID)(intptr_t)tex, q0, q1, uv0, uv1);
    // The artwork is bright everywhere, so the text needs its own ground.
    const ImU32 clear = IM_COL32(14, 15, 18, 0), dark = IM_COL32(14, 15, 18, 232);
    dl->AddRectFilledMultiColor(ImVec2(q0.x, q0.y + band_h * 0.30f), q1,
                                clear, clear, dark, dark);

    ImGui::SetCursorScreenPos(ImVec2(p0.x + indent, p1.y - px(76.0f)));
    ImGui::SetWindowFontScale(1.7f);
    ui::Text(spirula::i18n::msg::brand::product);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::SetCursorScreenPos(ImVec2(p0.x + indent, p1.y - px(30.0f)));
    ui::TextColored(kDim, spirula::i18n::msg::brand::tagline);

    ImGui::SetCursorScreenPos(ImVec2(p0.x, p1.y));
}

void GuiApp::draw_home() {
    // 480 px fits the longest button label in any language; a narrow window
    // narrows the column instead. The recent list goes beside it when it has
    // room for a path, under it otherwise.
    const float avail = ImGui::GetContentRegionAvail().x;
    const float margin = px(16.0f);
    const float w = std::max(std::min(px(480.0f), avail - margin), px(200.0f));
    const float gap = px(40.0f);
    const float list_w = std::min(px(760.0f), avail - 2.0f * margin - w - gap);
    const bool beside = list_w >= px(420.0f);
    const float bh = px(42.0f);
    const float indent = std::max(0.0f, (avail - (beside ? w + gap + list_w : w)) * 0.5f);

    draw_home_banner(avail, indent);

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
    ImGui::BeginChild("##home", ImVec2(w, 0));
    ImGui::Dummy(ImVec2(0, px(24.0f)));

    // A session (possibly still training) exists -- offer the way back.
    if (_runner.phase() != TrainRunner::Phase::Idle) {
        if (ui::Button(training_busy() ? msg::home_back_to_training
                                       : msg::home_back_to_trainer,
                       ImVec2(-1, bh)))
            go_to(Screen::Train);
        ImGui::Dummy(ImVec2(0, px(10.0f)));
    }

    if (ui::Button(msg::home_open_dataset, ImVec2(-1, bh))) {
        open_pick(PickAction::OpenDataset, msg::menu_open_dataset.get(),
                  FileDialog::Mode::Folder);
    }
    ui::help_on_hover(msg::home_open_dataset_help);
    // Only runs saved with full checkpoints can resume; File menu still has it.
    if (_keep_full_ckpt) {
        ImGui::BeginDisabled(native_work_busy() || training_busy());
        if (ui::Button(msg::resume_training, ImVec2(-1, bh)))
            open_pick(PickAction::ResumeRun, msg::resume_training.get(),
                      FileDialog::Mode::FileOrFolder, {".tar", ".ply"});
        ImGui::EndDisabled();
        ui::help_on_hover_disabled(msg::resume_training_help);
    }

    // Photos and video are one screen and one input list: a capture can hold
    // both, so splitting the entry point in two only asked a question with no
    // right answer. The list is added to there, or dropped onto the window.
    ImGui::BeginDisabled(native_work_busy());
    if (ui::Button(msg::home_new_dataset, ImVec2(-1, bh)) &&
        !native_work_busy())
        go_to(Screen::NewDataset);
    ImGui::EndDisabled();
    ui::help_on_hover(msg::home_new_dataset_help);

    if (ui::Button(msg::home_open_splat, ImVec2(-1, bh))) {
        open_pick(PickAction::SplatFile, msg::viewer_pick_file.get(),
                  FileDialog::Mode::FileOrFolder, kOpenableExtensions);
    }
    ui::help_on_hover(msg::home_open_splat_help);

    if (ui::Button(msg::home_make_mesh, ImVec2(-1, bh))) go_to(Screen::Mesh);
    ui::help_on_hover(msg::home_make_mesh_help);

    if (ui::Button(msg::home_batch, ImVec2(-1, bh))) go_to(Screen::Batch);
    ui::help_on_hover(msg::home_batch_help);

    ImGui::Spacing();
    ui::TextDisabledWrapped(msg::home_drop_hint);

    if (!beside) {
        ImGui::Dummy(ImVec2(0, px(18.0f)));
        draw_home_recent(/*scroll=*/false);
    }

    ImGui::Dummy(ImVec2(0, px(18.0f)));
    // Only worth saying when there is genuinely nothing that can do the job.
    if (!builtin_sfm_available() && !colmap_available())
        ui::TextColored(kDim, msg::home_no_engine);

    ImGui::EndChild();

    if (beside) {
        ImGui::SameLine(0.0f, gap);
        ImGui::BeginChild("##homerecent", ImVec2(list_w, 0));
        ImGui::Dummy(ImVec2(0, px(24.0f)));
        draw_home_recent(/*scroll=*/true);
        ImGui::EndChild();
    }
}

void GuiApp::draw_home_recent(bool scroll) {
    probe_recent();
    struct Tab {
        const Msg* label;
        const Msg* about;
        int kind;   // -1: every kind
    };
    static const Tab kTabs[] = {
        {&msg::home_recent, &msg::home_recent_about, -1},
        {&msg::home_tab_datasets, &msg::home_datasets_about, (int)RecentKind::Dataset},
        {&msg::home_tab_recons, &msg::home_recons_about, (int)RecentKind::Reconstruction},
        {&msg::home_tab_models, &msg::home_models_about, (int)RecentKind::Model},
        {&msg::home_tab_runs, &msg::home_runs_about, (int)RecentKind::Run},
        {&msg::home_tab_projects, &msg::home_projects_about, (int)RecentKind::Project},
    };
    // Its own tooltips say what each tab holds, which a truncated label's
    // would only repeat.
    if (!ImGui::BeginTabBar("##recenttabs", ImGuiTabBarFlags_NoTooltip |
                                                ImGuiTabBarFlags_DrawSelectedOverline))
        return;
    for (const Tab& t : kTabs) {
        const bool open = ui::BeginTabItem(*t.label);
        ui::help_on_hover(*t.about);
        if (!open) continue;

        // A copy: opening, removing and clearing all reorder the list.
        std::vector<RecentItem> rows;
        for (const RecentItem& it : _recent.items())
            if (t.kind < 0 || (int)it.kind == t.kind) rows.push_back(it);
        if (scroll) ImGui::BeginChild("##rows", ImVec2(0, 0));
        ImGui::Dummy(ImVec2(0, px(4.0f)));
        if (rows.empty()) {
            ui::TextDisabled(msg::home_recent_empty);
            ui::TextDisabledWrapped(*t.about);
        }
        bool picked = false;
        RecentItem go;
        for (const RecentItem& it : rows) {
            ImGui::PushID((int)it.kind);
            ImGui::PushID(it.path.c_str());
            // The Models tab keeps the badge: what a file holds is what tells
            // its entries apart.
            if (draw_recent_row(it, t.kind < 0 || it.kind == RecentKind::Model)) {
                go = it;
                picked = true;
            }
            draw_recent_menu(it, t.kind);
            ImGui::PopID();
            ImGui::PopID();
        }
        if (scroll) ImGui::EndChild();
        ImGui::EndTabItem();
        if (picked) open_recent(go);
    }
    ImGui::EndTabBar();
}

// The name over the path, a dot in the kind's colour before them; on the
// right the kind, in that colour, over when it was last used. Drawn rather
// than laid out so that both lines are one item to hover and click.
bool GuiApp::draw_recent_row(const RecentItem& it, bool badge) {
    const float line = ImGui::GetTextLineHeight();
    const float lead = px(2.0f);
    const float w = ImGui::GetContentRegionAvail().x;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool blocked = recent_blocked(it);
    // Lit while its menu is open, so the menu says which entry it is for.
    const bool clicked = ui::SelectableRaw("##row", ImGui::IsPopupOpen("##menu"),
                                           ImGuiSelectableFlags_None,
                                           ImVec2(w, 2.0f * line + lead)) &&
                         !blocked;
    if (blocked) ui::help_on_hover(msg::home_recon_busy);
    if (!ImGui::IsItemVisible()) return clicked;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec4 color = recent_color(it.kind);
    const float r = px(3.5f);
    const float x = p.x + px(6.0f) + 2.0f * r + px(8.0f);
    const float right = p.x + w - px(6.0f);
    const float sep = px(14.0f);
    dl->AddCircleFilled(ImVec2(p.x + px(6.0f) + r, p.y + 0.5f * line), r,
                        ImGui::GetColorU32(color));

    const char* kind = badge ? recent_badge(it).get() : "";
    const std::string when = recent_when(it.time);
    const float kind_w = *kind ? ImGui::CalcTextSize(kind).x + sep : 0.0f;
    const float when_w = when.empty() ? 0.0f : ImGui::CalcTextSize(when.c_str()).x + sep;
    const float min_w = px(60.0f);
    // A path is a path in every language. Cut from the middle: the ends are
    // what tell two of them apart.
    const std::string name = elide_middle(recent_name(it), std::max(min_w, right - x - kind_w));
    const std::string path = elide_middle(it.path, std::max(min_w, right - x - when_w));
    if (!blocked && path != it.path) ui::help_on_hover_raw(it.path.c_str());
    const ImU32 dim = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    dl->AddText(ImVec2(x, p.y), blocked ? dim : ImGui::GetColorU32(ImGuiCol_Text), name.c_str());
    dl->AddText(ImVec2(x, p.y + line + lead), dim, path.c_str());
    if (*kind)
        dl->AddText(ImVec2(right - kind_w + sep, p.y), ImGui::GetColorU32(color), kind);
    if (!when.empty())
        dl->AddText(ImVec2(right - when_w + sep, p.y + line + lead), dim, when.c_str());
    return clicked;
}

void GuiApp::draw_recent_menu(const RecentItem& it, int tab_kind) {
    if (!ImGui::BeginPopupContextItem("##menu")) return;
    if (ui::MenuItem(msg::home_recent_open, nullptr, false, !recent_blocked(it)))
        open_recent(it);
    const bool model = it.kind == RecentKind::Model || it.kind == RecentKind::Run;
    const bool splats = it.kind == RecentKind::Run || it.content == ModelContent::Splats;
    if (model) {
        if (ui::MenuItem(rmsg::train_render)) {
            _render_after_open = true;
            request_open_splat(it.path);
        }
        if (ui::MenuItem(rmsg::train_edit)) {
            _edit_after_open = true;
            request_open_splat(it.path);
        }
    }
    if (model && splats &&
        ui::MenuItem(msg::home_make_mesh, nullptr, false, !_mesh.busy())) {
        set_mesh_source(it.path);
        go_to(Screen::Mesh);
    }
    ImGui::Separator();
    if (ui::MenuItem(msg::home_recent_show)) {
        std::error_code ec;
        const fs::path p = fs::u8path(it.path);
        open_url((fs::is_directory(p, ec) ? p : p.parent_path()).u8string());
    }
    if (ui::MenuItem(msg::home_recent_copy)) ImGui::SetClipboardText(it.path.c_str());
    ImGui::Separator();
    if (ui::MenuItem(msg::home_recent_remove)) {
        _recent.remove(it.kind, it.path);
        save_settings();
    }
    if (ui::MenuItem(msg::home_recent_clear)) {
        if (tab_kind < 0) _recent.clear();
        else _recent.clear((RecentKind)tab_kind);
        save_settings();
    }
    ImGui::EndPopup();
}

void GuiApp::open_recent(RecentItem it) {
    switch (it.kind) {
        case RecentKind::Dataset:        request_open_dataset(it.path); break;
        case RecentKind::Reconstruction: open_reconstruction(it.path); break;
        case RecentKind::Model:
        case RecentKind::Run:            request_open_splat(it.path); break;
        case RecentKind::Project:
            if (!open_render_project(it.path))
                log(i18n::format(rmsg::project_open_failed, {it.path}));
            break;
    }
}

void GuiApp::open_reconstruction(const std::string& workspace) {
    if (recent_blocked({RecentKind::Reconstruction, workspace})) return;
    if (dataset_busy()) {
        go_to(Screen::NewDataset);
        return;
    }
    std::error_code ec;
    std::vector<std::string> inputs;
    for (const PrepInput& in :
         decode_record_inputs(read_dataset_record(workspace).inputs, workspace).rows)
        if (!in.path.empty() && fs::exists(fs::u8path(in.path), ec)) inputs.push_back(in.path);
    forget_redo_requests();
    clear_lidar_sources();
    if (!inputs.empty()) {
        add_sources(inputs, /*replace=*/true);
        _workspace = workspace;
    } else if (folder_looks_like_dataset(workspace)) {
        // Its inputs are gone, but what they made is a dataset: add to it.
        add_existing_dataset(workspace);
    } else {
        close_native_previews();
        close_splat();
        clear_sources();
        refresh_sources();
        _workspace = workspace;
    }
    remember(RecentKind::Reconstruction, workspace);
    go_to(Screen::NewDataset);
}

void GuiApp::probe_recent() {
    if (_recent.poll_probe()) save_settings();
    const double now = ImGui::GetTime();
    if (_recent.probing() || now - _recent_probed_at < 3.0) return;
    _recent_probed_at = now;
    _recent.start_probe(classify_recent_model);
}

}  // namespace gui

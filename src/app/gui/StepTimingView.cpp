// StepTimingView.cpp -- see StepTimingView.h.

#include "app/gui/StepTimingView.h"

#include "app/SettingCosts.h"
#include "app/gui/ConfigUI.h"
#include "app/gui/Ui.h"
#include "i18n/TimeFormat.h"
#include "i18n/catalog/Data.h"
#include "i18n/catalog/Gui.h"
#include "i18n/catalog/Log.h"
#include "i18n/catalog/TrainFields.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace gui {

namespace msg = spirula::i18n::msg::gui;
namespace fld = spirula::i18n::msg::field;
using spirula::TimeBreakdown;
using Msg = spirula::i18n::Msg;

namespace {

// The memory strip's colour for other programs (ResourceForecastView.cpp):
// time nothing here can break down.
const ImVec4 kUnaccounted(0.55f, 0.55f, 0.60f, 1.0f);

// A row or column below this share of the total is left out.
constexpr double kShown = 0.002;

const Msg& part_label(StepPart p) {
    switch (p) {
        case StepPart::Render:       return msg::timing_part_render;
        case StepPart::Backward:     return msg::timing_part_backward;
        case StepPart::Loss:         return msg::timing_part_loss;
        case StepPart::Optimizer:    return msg::timing_part_optimizer;
        case StepPart::Densify:      return msg::timing_part_densify;
        case StepPart::Appearance:   return msg::vram_cat_appearance;
        case StepPart::Calibration:  return msg::timing_part_calibration;
        case StepPart::Geometry:     return msg::timing_part_geometry;
        case StepPart::Masks:        return msg::timing_part_masks;
        case StepPart::Warp:         return msg::timing_part_warp;
        case StepPart::ColorSpace:   return msg::timing_part_color_space;
        case StepPart::Background:   return msg::timing_part_background;
        case StepPart::Regularizer:  return msg::timing_part_regularizer;
        case StepPart::Viewer:       return msg::vram_cat_viewer;
        case StepPart::Unclassified: return msg::timing_part_unclassified;
        case StepPart::Loading:      return msg::timing_part_loading;
        case StepPart::Saving:       return msg::timing_part_saving;
        default:                     return msg::vram_cat_other;
    }
}

const Msg& scale_label(StepScale c) {
    switch (c) {
        case StepScale::Splats:       return msg::vram_cat_splat;
        case StepScale::SplatsPixels: return msg::timing_scale_splats_pixels;
        case StepScale::Pixels:       return msg::timing_scale_pixels;
        case StepScale::Images:       return msg::timing_scale_images;
        default:                      return msg::timing_scale_fixed;
    }
}

// One unit and one precision for the whole card, picked from its total, so
// the rows read as one column: ms per step; s, min or h over the run.
struct Amounts {
    const Msg* unit;
    double per_second;
    const char* digits;

    Amounts(double total, bool run) {
        if (!run) unit = &spirula::i18n::msg::data::xs_ms, per_second = 1e3;
        else if (total < 120.0) unit = &spirula::i18n::msg::log::dur_seconds, per_second = 1.0;
        else if (total < 7200.0) unit = &spirula::i18n::msg::log::dur_minutes, per_second = 1.0 / 60.0;
        else unit = &spirula::i18n::msg::log::dur_hours, per_second = 1.0 / 3600.0;
        const double top = total * per_second;
        digits = top < 10.0 ? "%.2f" : top < 1000.0 ? "%.1f" : "%.0f";
    }

    std::string operator()(double seconds) const {
        char s[32];
        std::snprintf(s, sizeof s, digits, seconds * per_second);
        return ui::format(*unit, {std::string(s)});
    }
};

std::string percent(double share) {
    char s[16];
    std::snprintf(s, sizeof s, "%.1f%%", share * 100.0);
    return s;
}

ImU32 with_alpha(ImVec4 c, float a) {
    c.w = a;
    return ImGui::GetColorU32(c);
}

ImU32 mix(const ImVec4& a, const ImVec4& b, float t) {
    return ImGui::GetColorU32(ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                                     a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t));
}

struct Row {
    const Msg* label;
    double seconds;
    bool unaccounted;
};

// A bar per row, scaled to `scale`, then the amount and its share of
// `total`, each right-aligned in a column of its own.
void bar_list(const Msg& title, const std::vector<Row>& rows, double scale, double total,
              const Amounts& amount) {
    ui::TextDisabled(title);
    std::vector<std::string> amounts, shares;
    float label_w = 0.0f, amount_w = 0.0f, share_w = 0.0f;
    for (const Row& r : rows) {
        amounts.push_back(amount(r.seconds));
        shares.push_back(percent(r.seconds / total));
        label_w = std::max(label_w, ImGui::CalcTextSize(r.label->get()).x);
        amount_w = std::max(amount_w, ImGui::CalcTextSize(amounts.back().c_str()).x);
        share_w = std::max(share_w, ImGui::CalcTextSize(shares.back().c_str()).x);
    }
    const float left = ImGui::GetCursorScreenPos().x;
    const float gap = ImGui::GetStyle().ItemSpacing.x * 2.0f;
    const float bar_w = px(110.0f);
    const float bar_x = left + label_w + gap;
    const float amount_end = bar_x + bar_w + gap + amount_w;
    const float share_end = amount_end + gap + share_w;
    const float h = ImGui::GetTextLineHeight();
    const ImU32 fill = ImGui::GetColorU32(ImGuiCol_PlotHistogram);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const auto right_aligned = [](const std::string& s, float end) {
        ImGui::SameLine();
        ImGui::SetCursorScreenPos(
            ImVec2(end - ImGui::CalcTextSize(s.c_str()).x, ImGui::GetCursorScreenPos().y));
        ui::TextRaw(s);
    };
    for (size_t i = 0; i < rows.size(); ++i) {
        const Row& r = rows[i];
        ui::Text(*r.label);
        ImGui::SameLine();
        ImGui::SetCursorScreenPos(ImVec2(bar_x, ImGui::GetCursorScreenPos().y));
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float w = bar_w * (float)std::clamp(scale > 0.0 ? r.seconds / scale : 0.0, 0.0, 1.0);
        dl->AddRectFilled(p, ImVec2(p.x + bar_w, p.y + h), ImGui::GetColorU32(ImGuiCol_FrameBg));
        dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h),
                          r.unaccounted ? with_alpha(kUnaccounted, 0.8f) : fill);
        ImGui::Dummy(ImVec2(bar_w, h));
        right_aligned(amounts[i], amount_end);
        right_aligned(shares[i], share_end);
    }
}

void lists(const TimeBreakdown& b, const StepTimingViewState& st, const Amounts& amount) {
    const double total = std::max(b.total(), 1e-12);
    std::vector<Row> parts, scales;
    for (int p = 0; p < kStepParts; ++p)
        if (st.part_shown[p]) parts.push_back({&part_label((StepPart)p), b.cells.part((StepPart)p), false});
    for (int c = 0; c < kStepScales; ++c)
        if (st.scale_shown[c])
            scales.push_back({&scale_label((StepScale)c), b.cells.scale((StepScale)c), false});
    if (st.unaccounted_shown) {
        parts.push_back({&msg::timing_unaccounted, b.unaccounted, true});
        scales.push_back({&msg::timing_unaccounted, b.unaccounted, true});
    }
    double scale = 0.0;
    for (const Row& r : parts) scale = std::max(scale, r.seconds);
    for (const Row& r : scales) scale = std::max(scale, r.seconds);
    ImGui::BeginGroup();
    bar_list(msg::timing_by_part, parts, scale, total, amount);
    ImGui::EndGroup();
    ImGui::SameLine(0.0f, px(28.0f));
    ImGui::BeginGroup();
    bar_list(msg::timing_by_scale, scales, scale, total, amount);
    ImGui::EndGroup();
}

// Text centred in [a, b], drawn only where it fits.
bool cell_text(ImDrawList* dl, const ImVec2& a, const ImVec2& b, const std::string& s,
               float y_offset = 0.0f) {
    const ImVec2 size = ImGui::CalcTextSize(s.c_str());
    if (size.x > b.x - a.x - px(4.0f) || size.y > b.y - a.y) return false;
    dl->AddText(ImVec2(a.x + (b.x - a.x - size.x) * 0.5f, a.y + y_offset),
                ImGui::GetColorU32(ImGuiCol_Text), s.c_str());
    return true;
}

// Purpose down, what the time grows with across; each row's height and each
// column's width follow its share, so a cell's area tracks its time when the
// two are independent and its shade says how far they are not.
void grid(const TimeBreakdown& b, const StepTimingViewState& st, const Amounts& amount) {
    const double total = std::max(b.total(), 1e-12);
    std::vector<int> rows, cols;
    for (int p = 0; p < kStepParts; ++p)
        if (st.part_shown[p]) rows.push_back(p);
    for (int c = 0; c < kStepScales; ++c)
        if (st.scale_shown[c]) cols.push_back(c);
    const bool un = st.unaccounted_shown;
    if (cols.empty() && !un) return;

    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float line = ImGui::GetTextLineHeight();
    const float pad = px(4.0f);
    float label_w = un ? ImGui::CalcTextSize(msg::timing_unaccounted.get()).x : 0.0f;
    for (int p : rows)
        label_w = std::max(label_w, ImGui::CalcTextSize(part_label((StepPart)p).get()).x);
    const float share_w = ImGui::CalcTextSize("100%").x + pad;

    double classified = 0.0;
    for (int c : cols) classified += b.cells.scale((StepScale)c);
    std::vector<float> col_w(cols.size());
    float min_w = 0.0f;
    for (size_t i = 0; i < cols.size(); ++i) {
        col_w[i] = std::max(ImGui::CalcTextSize(scale_label((StepScale)cols[i]).get()).x,
                            ImGui::CalcTextSize(amount(total).c_str()).x) + 2.0f * pad;
        min_w += col_w[i];
    }
    const float grid_w = min_w + px(240.0f);
    for (size_t i = 0; i < cols.size(); ++i)
        col_w[i] += (grid_w - min_w) *
                    (float)(classified > 0.0 ? b.cells.scale((StepScale)cols[i]) / classified : 0.0);

    const int n_rows = (int)rows.size() + (un ? 1 : 0);
    const float row_min = line + pad;
    const float grid_h = n_rows * row_min + px(160.0f);
    std::vector<float> row_h(n_rows);
    for (int i = 0; i < n_rows; ++i) {
        const double t = i < (int)rows.size() ? b.cells.part((StepPart)rows[i]) : b.unaccounted;
        row_h[i] = row_min + (grid_h - n_rows * row_min) * (float)(t / total);
    }

    double hottest = 0.0;
    for (int p : rows)
        for (int c : cols) hottest = std::max(hottest, b.cells.s[p][c]);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 o = ImGui::GetCursorScreenPos();
    const float gx = o.x + label_w + spacing;
    const float gy = o.y + line + pad;
    const ImVec4 cold = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
    const ImVec4 hot = ImGui::GetStyleColorVec4(ImGuiCol_PlotHistogram);
    const ImU32 text = ImGui::GetColorU32(ImGuiCol_Text);
    const ImU32 dim = ImGui::GetColorU32(ImGuiCol_TextDisabled);

    float x = gx;
    for (size_t i = 0; i < cols.size(); ++i) {
        dl->PushClipRect(ImVec2(x, o.y), ImVec2(x + col_w[i], gy), true);
        const char* label = scale_label((StepScale)cols[i]).get();
        const float w = ImGui::CalcTextSize(label).x;
        dl->AddText(ImVec2(x + std::max(0.0f, (col_w[i] - w) * 0.5f), o.y), dim, label);
        dl->PopClipRect();
        x += col_w[i];
    }

    float y = gy;
    for (int i = 0; i < n_rows; ++i) {
        const float h = row_h[i];
        const bool strip = i >= (int)rows.size();
        const char* label = strip ? msg::timing_unaccounted.get()
                                  : part_label((StepPart)rows[i]).get();
        dl->AddText(ImVec2(o.x, y + (h - line) * 0.5f), text, label);
        const double row_t = strip ? b.unaccounted : b.cells.part((StepPart)rows[i]);
        const std::string share = percent(row_t / total);
        dl->AddText(ImVec2(gx + grid_w + pad, y + (h - line) * 0.5f), dim, share.c_str());
        if (strip) {
            const ImVec2 a(gx + 1.0f, y + 1.0f), c(gx + grid_w - 1.0f, y + h - 1.0f);
            dl->AddRectFilled(a, c, with_alpha(kUnaccounted, 0.6f));
            cell_text(dl, a, c, amount(row_t), (h - line) * 0.5f);
        } else {
            x = gx;
            for (size_t j = 0; j < cols.size(); ++j) {
                const double t = b.cells.s[rows[i]][cols[j]];
                const ImVec2 a(x + 1.0f, y + 1.0f), c(x + col_w[j] - 1.0f, y + h - 1.0f);
                const float heat = hottest > 0.0 ? (float)std::sqrt(t / hottest) : 0.0f;
                dl->AddRectFilled(a, c, mix(cold, hot, heat));
                if (t > kShown * total) {
                    const bool two = h >= 2.0f * line + pad;
                    const float top = two ? (h - 2.0f * line) * 0.5f : (h - line) * 0.5f;
                    if (cell_text(dl, a, c, amount(t), top) && two)
                        cell_text(dl, a, c, percent(t / total), top + line);
                }
                x += col_w[j];
            }
        }
        y += h;
    }

    x = gx;
    for (size_t i = 0; i < cols.size(); ++i) {
        const std::string share = percent(b.cells.scale((StepScale)cols[i]) / total);
        const float w = ImGui::CalcTextSize(share.c_str()).x;
        dl->AddText(ImVec2(x + (col_w[i] - w) * 0.5f, y + pad * 0.5f), dim, share.c_str());
        x += col_w[i];
    }
    ImGui::Dummy(ImVec2(gx - o.x + grid_w + share_w, y - o.y + line + pad));
    ui::TextDisabled(msg::timing_grid_hint);
}

// The largest savings the breakdown can quantify, each naming the setting as
// the panel labels it and where it is: a feature's row, or a column scaled by
// the exponents TrainForecast.h measured.
void tips(const TimeBreakdown& b, const TrainConfig& cfg, const SavingsContext& ctx) {
    const double total = b.total();
    if (!b.known || total <= 0.0) return;
    const auto col = [&](StepScale c) { return b.cells.scale(c); };
    const auto part = [&](StepPart p) { return b.cells.part(p); };
    const std::string basic = msg::section_basic_options.get();
    const std::string all = msg::section_all_options.get();
    struct Tip { double saved; std::string text; bool always = false; };
    std::vector<Tip> found;
    const auto add = [&](double saved, const Msg& m, const std::string& a = {},
                         const std::string& c = {}, const std::string& d = {}) {
        char pct[16];
        std::snprintf(pct, sizeof pct, "%.0f", 100.0 * saved / total);
        found.push_back({saved, ui::format(m, {std::string(pct), a, c, d})});
    };
    // One or both of two switches, named together when both are on.
    const auto off = [&](double saved, bool on1, const Msg& l1, bool on2, const Msg& l2,
                         const std::string& where) {
        if (on1 && on2) add(saved, msg::timing_tip_off_two, l1.get(), l2.get(), where);
        else if (on1 || on2) add(saved, msg::timing_tip_off, (on1 ? l1 : l2).get(), where);
    };

    add(col(StepScale::Pixels) * 0.75 +
            col(StepScale::SplatsPixels) * (1.0 - std::pow(0.25, spirula::kSplatPixelTimeExp)),
        msg::timing_tip_halve, msg::opt_resolution.get(), basic);
    add(col(StepScale::Splats) * 0.5 +
            col(StepScale::SplatsPixels) * (1.0 - std::pow(0.5, spirula::kSplatPixelSplatExp)),
        msg::timing_tip_halve, msg::opt_max_splats.get(), basic);
    if (ctx.frames_split) {
        // Every view is an image to correct, so without the split the per-image
        // work falls by the views' ratio. The thing to try when appearance
        // correction is what costs, so it is listed whatever its rank.
        const double saved = part(StepPart::Warp) + col(StepScale::Images) * (1.0 - ctx.images_kept);
        if (ctx.preset_360)
            add(saved, msg::timing_tip_preset, msg::section_preset.get(), ctx.preset_360_label,
                ctx.preset_general_label);
        else
            add(saved, msg::timing_tip_off, fld::warp_to_pinhole.get(), all);
        found.back().always = true;
    } else {
        add(col(StepScale::Images) * 0.5, msg::timing_tip_images);
    }
    off(part(StepPart::Appearance), cfg.use_bilateral_grid, msg::opt_bilateral_grid,
        cfg.use_ppisp, msg::opt_ppisp, basic);
    off(part(StepPart::Geometry), cfg.load_depths && cfg.depth_supervision_weight > 0.0f,
        msg::opt_dataset_depths,
        cfg.load_normals && (cfg.normal_supervision_weight > 0.0f ||
                             cfg.median_normal_supervision_weight > 0.0f),
        msg::opt_dataset_normals, basic);
    // A dropdown lowered, with what that does to the kernels it touches
    // (SettingCosts.h): one step, except where the costs are not monotonic.
    const auto step_down = [&](const char* key, const std::string& now, const char* lower,
                               const std::string& label, const std::string& shown) {
        add(spirula::setting_saving(b.cells, key, now, lower), msg::timing_tip_set, label, shown,
            basic);
    };
    const auto lower_of = [](const std::string& v, std::initializer_list<const char*> levels) {
        const char* prev = nullptr;
        for (const char* l : levels) {
            if (v == l) return prev;
            prev = l;
        }
        return (const char*)nullptr;
    };
    if (const char* l = lower_of(cfg.calib_correction, {"off", "basic", "accurate"}))
        step_down("calib_correction", cfg.calib_correction, l, fld::calib_correction.get(),
                  choice_display("calib_correction", l));
    else if (cfg.calib_correction == "off" && cfg.use_calib_warp)
        add(part(StepPart::Calibration), msg::timing_tip_off, fld::use_calib_warp.get(), all);
    // Not monotonic: mild's distortion terms cost more in the backward than
    // strong's, whose size cap thins the tiles. So the lower level that saves most.
    const char* best = nullptr;
    double best_saved = 0.0;
    for (const char* l : {"off", "mild", "moderate", "strong"}) {
        if (cfg.floater_suppression == l) break;
        const double s =
            spirula::setting_saving(b.cells, "floater_suppression", cfg.floater_suppression, l);
        if (s > best_saved) best = l, best_saved = s;
    }
    if (best)
        step_down("floater_suppression", cfg.floater_suppression, best,
                  fld::floater_suppression.get(), choice_display("floater_suppression", best));
    if (cfg.primitive != "3dgs")
        step_down("primitive", cfg.primitive, "3dgs", msg::opt_primitive.get(), "3dgs");
    if (cfg.sh_degree > 0)
        step_down("sh_degree", std::to_string(cfg.sh_degree),
                  std::to_string(cfg.sh_degree - 1).c_str(), msg::opt_sh_degree.get(),
                  std::to_string(cfg.sh_degree - 1));
    add(part(StepPart::Viewer), msg::timing_tip_viewer, msg::viewport_live.get());
    add(part(StepPart::Loading), msg::timing_tip_loading);
    add(part(StepPart::Saving), msg::timing_tip_raise, fld::steps_per_save.get(), all);

    std::stable_sort(found.begin(), found.end(),
                     [](const Tip& a, const Tip& c) { return a.saved > c.saved; });
    int shown = 0;
    for (size_t i = 0; i < found.size(); ++i) {
        const Tip& t = found[i];
        if (t.saved < 0.05 * total) break;
        if (i >= 3 && !t.always) continue;
        if (shown++ == 0) {
            ImGui::Spacing();
            ui::Text(msg::timing_tips_title);
        }
        ImGui::Bullet();
        ui::TextWrappedRaw(t.text);
    }
}

// What crosses the threshold in either mode joins what is shown, for good.
void mark_shown(const TimeBreakdown& b, StepTimingViewState& st) {
    if (!b.known) return;
    const double floor = kShown * b.total();
    for (int p = 0; p < kStepParts; ++p)
        if (b.cells.part((StepPart)p) > floor) st.part_shown[p] = true;
    for (int c = 0; c < kStepScales; ++c)
        if (b.cells.scale((StepScale)c) > floor) st.scale_shown[c] = true;
    if (b.unaccounted > floor) st.unaccounted_shown = true;
}

}  // namespace

void step_timing_card(const TimeBreakdown& step, const TimeBreakdown& run,
                      const TrainConfig& cfg, const SavingsContext& ctx,
                      StepTimingViewState& st, bool pinned) {
    ui::Text(msg::timing_title);
    if (pinned) {
        if (ui::RadioButton(msg::timing_mode_step, !st.whole_run)) st.whole_run = false;
        ImGui::SameLine();
        if (ui::RadioButton(msg::timing_mode_run, st.whole_run)) st.whole_run = true;
        ImGui::SameLine(0.0f, px(24.0f));
        if (ui::RadioButton(msg::timing_view_lists, !st.grid)) st.grid = false;
        ImGui::SameLine();
        if (ui::RadioButton(msg::timing_view_grid, st.grid)) st.grid = true;
    }
    const bool whole = st.whole_run && run.known;
    const TimeBreakdown& b = whole ? run : step;
    if (!b.known) {
        ui::TextDisabled(msg::timing_waiting);
        return;
    }
    if (whole) {
        ui::TextDisabled(msg::timing_run_total,
                         {spirula::i18n::format_duration_coarse(b.total())});
    } else {
        char ms[32];
        std::snprintf(ms, sizeof ms, "%.1f", b.total() * 1e3);
        ui::TextDisabled(msg::timing_step_total, {std::string(ms), b.window});
    }
    mark_shown(step, st);
    mark_shown(run, st);
    ImGui::Spacing();
    ImGui::BeginGroup();
    const Amounts amount(b.total(), whole);
    if (st.grid) grid(b, st, amount);
    else lists(b, st, amount);
    ImGui::EndGroup();
    // The text below wraps at the chart's width; the chart itself must not.
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() +
                           std::max(ImGui::GetItemRectSize().x, px(420.0f)));
    // Savings are a question about the next run, so they read the whole one.
    tips(run.known ? run : step, cfg, ctx);
    if (!pinned) {
        ImGui::Spacing();
        ui::TextDisabled(msg::timing_click_hint);
    }
    ImGui::PopTextWrapPos();
}

}  // namespace gui

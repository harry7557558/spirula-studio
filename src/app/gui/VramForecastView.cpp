// VramForecastView.cpp -- see VramForecastView.h.

#include "app/gui/VramForecastView.h"

#include "i18n/catalog/Gui.h"

#include <algorithm>
#include <utility>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace gui {

namespace msg = spirula::i18n::msg::gui;
using spirula::OomRisk;
using spirula::VramForecast;

namespace {

// The memory strip's colour for other programs (ResourceForecastView.cpp).
const ImVec4 kOthers(0.55f, 0.55f, 0.60f, 1.0f);

constexpr double kGiB = 1024.0 * 1024.0 * 1024.0;

std::string gib(double bytes) {
    char s[32];
    std::snprintf(s, sizeof s, "%.2f", bytes / kGiB);
    return s;
}

ImU32 with_alpha(ImVec4 c, float a) {
    c.w = a;
    return ImGui::GetColorU32(c);
}

void breakdown(const VramForecast& v) {
    struct Row { const spirula::i18n::Msg* label; double now; double peak; ImVec4 color; };
    const ImVec4 bar = ImGui::GetStyleColorVec4(ImGuiCol_PlotHistogram);
    const auto cat = [&](VramCategory c) { return v.category[(int)c]; };
    const double sxi = cat(VramCategory::SplatXImg);
    const double sxi_peak = std::max(sxi, v.grow_peak_mean - v.scratch);
    const Row rows[] = {
        {&msg::vram_cat_splat, cat(VramCategory::Splat), 0.0, bar},
        {&msg::vram_cat_splat_x_img, sxi, v.projected ? sxi_peak : 0.0, bar},
        {&msg::vram_cat_image, cat(VramCategory::Image), 0.0, bar},
        {&msg::vram_cat_appearance, cat(VramCategory::Appearance), 0.0, bar},
        {&msg::vram_cat_viewer, cat(VramCategory::Viewer), 0.0, bar},
        {&msg::vram_cat_other, cat(VramCategory::Other), 0.0, bar},
        {&msg::vram_cat_scratch, v.scratch, 0.0, bar},
        {&msg::vram_cat_unpooled, v.unpooled, 0.0, bar},
        {&msg::vram_legend_others, v.others, 0.0, kOthers},
    };
    const double shown = 1024.0 * 1024.0;   // below 1 MiB a row is noise
    double scale = 1.0;
    float label_w = 0.0f;
    for (const Row& r : rows) {
        if (std::max(r.now, r.peak) < shown) continue;
        scale = std::max(scale, std::max(r.now, r.peak));
        label_w = std::max(label_w, ImGui::CalcTextSize(r.label->get()).x);
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImGuiStyle& st = ImGui::GetStyle();
    const float bar_w = px(220.0f);
    const float h = ImGui::GetTextLineHeight();
    bool faded = false;
    for (const Row& r : rows) {
        if (std::max(r.now, r.peak) < shown) continue;
        ui::Text(*r.label);
        ImGui::SameLine(label_w + st.ItemSpacing.x * 2.0f);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float w_now = bar_w * (float)(r.now / scale);
        const float w_peak = bar_w * (float)(r.peak / scale);
        dl->AddRectFilled(p, ImVec2(p.x + bar_w, p.y + h),
                          ImGui::GetColorU32(ImGuiCol_FrameBg));
        if (w_peak > w_now) {
            dl->AddRectFilled(ImVec2(p.x + w_now, p.y), ImVec2(p.x + w_peak, p.y + h),
                              with_alpha(r.color, 0.3f));
            faded = true;
        }
        dl->AddRectFilled(p, ImVec2(p.x + w_now, p.y + h), ImGui::GetColorU32(r.color));
        ImGui::Dummy(ImVec2(bar_w, h));
        ImGui::SameLine();
        if (r.peak > r.now)
            ui::TextRaw(gib(r.now) + " -> " + gib(r.peak));
        else
            ui::TextRaw(gib(r.now));
    }
    if (faded) ui::TextDisabled(msg::vram_breakdown_growth);
}

// What a basic setting takes off the peak: Splat is sized for cap_max, Image
// follows the pixels, splat x img both (TrainForecast's exponents), and the
// 360-camera preset's five views a frame are five images' appearance grids.
void savings(const VramForecast& v, const SavingsContext& ctx) {
    if (!v.projected || v.peak_mean <= 0.0) return;
    const auto cat = [&](VramCategory c) { return v.category[(int)c]; };
    const double sxi = std::max(cat(VramCategory::SplatXImg), v.grow_peak_mean - v.scratch);
    const std::string basic = msg::section_basic_options.get();
    struct Tip { double saved; std::string text; };
    std::vector<Tip> tips;
    const auto add = [&](double saved, const spirula::i18n::Msg& m, const std::string& a,
                         const std::string& c, const std::string& d = {}) {
        tips.push_back({saved, ui::format(m, {gib(saved), a, c, d})});
    };
    add(cat(VramCategory::Image) * 0.75 + sxi * (1.0 - std::pow(0.25, spirula::kSplatXImgVramExp)),
        msg::vram_tip_halve, msg::opt_resolution.get(), basic);
    add(cat(VramCategory::Splat) * 0.5 + sxi * (1.0 - std::pow(0.5, v.growth_power)),
        msg::vram_tip_halve, msg::opt_max_splats.get(), basic);
    if (ctx.sh_degree > 0 && v.sh_bytes > 0.0) {
        // SH rows hold (d + 1)^2 - 1 coefficients; measured 3 -> 2 -> 1 -> 0
        // freed 84 / 53 / 38 MiB of 176 at 1M splats, within 7% of this.
        const auto coeffs = [](int d) { return (double)((d + 1) * (d + 1) - 1); };
        const int d = ctx.sh_degree;
        add(v.sh_bytes * (1.0 - coeffs(d - 1) / coeffs(d)), msg::vram_tip_set,
            msg::opt_sh_degree.get(), std::to_string(d - 1), basic);
    }
    if (ctx.frames_split && ctx.preset_360)
        add(cat(VramCategory::Appearance) * (1.0 - ctx.images_kept), msg::vram_tip_preset, msg::section_preset.get(),
            ctx.preset_360_label, ctx.preset_general_label);
    std::stable_sort(tips.begin(), tips.end(),
                     [](const Tip& a, const Tip& c) { return a.saved > c.saved; });
    bool titled = false;
    for (const Tip& t : tips) {
        if (t.saved < 0.05 * v.peak_mean) continue;
        if (!titled) {
            ImGui::Spacing();
            ui::Text(msg::vram_tips_title);
            titled = true;
        }
        ImGui::Bullet();
        ui::TextWrappedRaw(t.text);
    }
}

}  // namespace

const spirula::i18n::Msg* oom_risk_label(OomRisk r) {
    switch (r) {
        case OomRisk::Low:    return &msg::oom_risk_low;
        case OomRisk::Medium: return &msg::oom_risk_medium;
        case OomRisk::High:   return &msg::oom_risk_high;
        default:              return nullptr;
    }
}

void vram_forecast_card(const VramForecast& v, int total_steps, const SavingsContext& ctx) {
    ui::Text(msg::vram_chart_title);
    forecast_chart(v, std::max(1, total_steps), std::to_string(total_steps));
    forecast_legend(msg::vram_legend_run);
    if (v.projected && !v.projection.empty() && v.capacity > 0.0) {
        char pct[16];
        std::snprintf(pct, sizeof pct, "%.0f", v.p_oom * 100.0);
        ui::TextColoredWrapped(oom_risk_color(v.risk), msg::vram_chart_peak,
                               {gib(v.peak_mean), gib(v.peak_sigma),
                                gib(v.limit), std::string(pct)});
    } else if (!v.projected) {
        ui::TextDisabledWrapped(msg::vram_chart_waiting);
    }
    ImGui::Spacing();
    ui::Text(msg::vram_breakdown_title);
    breakdown(v);
    savings(v, ctx);
}

}  // namespace gui

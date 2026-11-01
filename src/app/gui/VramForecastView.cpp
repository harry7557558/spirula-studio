// VramForecastView.cpp -- see VramForecastView.h.

#include "app/gui/VramForecastView.h"

#include "i18n/catalog/Gui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

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

}  // namespace

const spirula::i18n::Msg* oom_risk_label(OomRisk r) {
    switch (r) {
        case OomRisk::Low:    return &msg::oom_risk_low;
        case OomRisk::Medium: return &msg::oom_risk_medium;
        case OomRisk::High:   return &msg::oom_risk_high;
        default:              return nullptr;
    }
}

void vram_forecast_card(const VramForecast& v, int total_steps) {
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
}

}  // namespace gui

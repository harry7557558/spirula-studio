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

// The status strip's own pressure colours (GuiApp.cpp).
const ImVec4 kOk(0.35f, 0.85f, 0.45f, 1.0f);
const ImVec4 kWarn(0.95f, 0.75f, 0.30f, 1.0f);
const ImVec4 kErr(1.0f, 0.42f, 0.42f, 1.0f);
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

void dashed_line(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float thick) {
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    const float dash = px(4.0f);
    for (float t = 0.0f; t < len; t += 2.0f * dash) {
        const float t1 = std::min(len, t + dash);
        dl->AddLine(ImVec2(a.x + dx * t / len, a.y + dy * t / len),
                    ImVec2(a.x + dx * t1 / len, a.y + dy * t1 / len), col, thick);
    }
}

void swatch(ImU32 col) {
    const float h = ImGui::GetTextLineHeight();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(p.x, p.y + h * 0.25f), ImVec2(p.x + h * 0.5f, p.y + h * 0.75f), col);
    ImGui::Dummy(ImVec2(h * 0.5f, h));
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
}

// A gridline spacing of 10^k, 2*10^k or 5*10^k GiB that draws at most five.
double tick_step(double max_gib) {
    double step = std::pow(10.0, std::floor(std::log10(std::max(max_gib, 1e-3))));
    for (double m : {1.0, 2.0, 5.0, 10.0})
        if (max_gib / (step * m) <= 5.0) return step * m;
    return step * 10.0;
}

void chart(const VramForecast& v, int total_steps) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImGuiStyle& st = ImGui::GetStyle();
    const float line = ImGui::GetTextLineHeight();
    const float W = px(460.0f), H = px(170.0f);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(W, H + line + st.ItemInnerSpacing.y));

    double y_max = v.total_bytes;
    for (const auto& s : v.history) y_max = std::max(y_max, s.ours + s.others);
    for (const auto& b : v.projection)
        y_max = std::max(y_max, v.others_bytes + b.mean + 2.0 * b.sigma);
    y_max = std::max(y_max * 1.05, 1.0);
    const double T = std::max(1, total_steps);

    const float label_w = ImGui::CalcTextSize("00.0").x + st.ItemInnerSpacing.x;
    const ImVec2 r0(p0.x + label_w, p0.y);
    const ImVec2 r1(p0.x + W, p0.y + H);
    auto X = [&](double step) {
        return r0.x + (r1.x - r0.x) * (float)std::min(1.0, std::max(0.0, step / T));
    };
    auto Y = [&](double bytes) {
        return r1.y - (r1.y - r0.y) * (float)std::min(1.0, std::max(0.0, bytes / y_max));
    };

    dl->AddRectFilled(r0, r1, ImGui::GetColorU32(ImGuiCol_FrameBg));
    const ImU32 grid = ImGui::GetColorU32(ImGuiCol_Border);
    const ImU32 text_dim = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    const double tick = tick_step(y_max / kGiB);
    for (double g = 0.0; g * kGiB <= y_max; g += tick) {
        const float y = Y(g * kGiB);
        dl->AddLine(ImVec2(r0.x, y), ImVec2(r1.x, y), grid);
        char s[16];
        std::snprintf(s, sizeof s, tick < 1.0 ? "%.1f" : "%.0f", g);
        const ImVec2 sz = ImGui::CalcTextSize(s);
        dl->AddText(ImVec2(r0.x - sz.x - st.ItemInnerSpacing.x, y - sz.y * 0.5f),
                    text_dim, s);
    }

    // Other programs at the bottom, this run stacked on top: the gap to the
    // capacity line is what the run has left.
    const ImU32 others_fill = with_alpha(kOthers, 0.35f);
    for (size_t i = 1; i < v.history.size(); ++i) {
        const auto& a = v.history[i - 1];
        const auto& b = v.history[i];
        dl->AddQuadFilled(ImVec2(X(a.step), Y(0)), ImVec2(X(a.step), Y(a.others)),
                          ImVec2(X(b.step), Y(b.others)), ImVec2(X(b.step), Y(0)),
                          others_fill);
    }
    const double last_step = v.history.empty() ? 0.0 : v.history.back().step;
    if (!v.projection.empty())
        dl->AddRectFilled(ImVec2(X(last_step), Y(v.others_bytes)), ImVec2(r1.x, Y(0)),
                          others_fill);

    const ImVec4 run = ImGui::GetStyleColorVec4(ImGuiCol_PlotLines);
    for (size_t i = 1; i < v.projection.size(); ++i) {
        const auto& a = v.projection[i - 1];
        const auto& b = v.projection[i];
        const double o = v.others_bytes;
        dl->AddQuadFilled(ImVec2(X(a.step), Y(o + a.mean + 2.0 * a.sigma)),
                          ImVec2(X(b.step), Y(o + b.mean + 2.0 * b.sigma)),
                          ImVec2(X(b.step), Y(o + b.mean - 2.0 * b.sigma)),
                          ImVec2(X(a.step), Y(o + a.mean - 2.0 * a.sigma)),
                          with_alpha(run, 0.22f));
        dashed_line(dl, ImVec2(X(a.step), Y(o + a.mean)),
                    ImVec2(X(b.step), Y(o + b.mean)), with_alpha(run, 0.9f), px(1.5f));
    }
    for (size_t i = 1; i < v.history.size(); ++i) {
        const auto& a = v.history[i - 1];
        const auto& b = v.history[i];
        dl->AddLine(ImVec2(X(a.step), Y(a.ours + a.others)),
                    ImVec2(X(b.step), Y(b.ours + b.others)),
                    ImGui::GetColorU32(run), px(2.0f));
    }
    if (!v.history.empty())
        dl->AddLine(ImVec2(X(last_step), r0.y), ImVec2(X(last_step), r1.y), grid);
    if (v.total_bytes > 0.0)
        dl->AddLine(ImVec2(r0.x, Y(v.total_bytes)), ImVec2(r1.x, Y(v.total_bytes)),
                    ImGui::GetColorU32(kErr), px(1.5f));
    dl->AddRect(r0, r1, grid);

    char s[32];
    dl->AddText(ImVec2(r0.x, r1.y + st.ItemInnerSpacing.y), text_dim, "0");
    std::snprintf(s, sizeof s, "%d", total_steps);
    dl->AddText(ImVec2(r1.x - ImGui::CalcTextSize(s).x, r1.y + st.ItemInnerSpacing.y),
                text_dim, s);

    swatch(ImGui::GetColorU32(run));
    ui::TextDisabled(msg::vram_legend_run);
    ImGui::SameLine();
    swatch(with_alpha(run, 0.35f));
    ui::TextDisabled(msg::vram_legend_projected);
    ImGui::SameLine();
    swatch(others_fill);
    ui::TextDisabled(msg::vram_legend_others);
    ImGui::SameLine();
    swatch(ImGui::GetColorU32(kErr));
    ui::TextDisabled(msg::vram_legend_capacity);
}

void breakdown(const VramForecast& v) {
    struct Row { const spirula::i18n::Msg* label; double now; double peak; ImVec4 color; };
    const ImVec4 bar = ImGui::GetStyleColorVec4(ImGuiCol_PlotHistogram);
    const auto cat = [&](VramCategory c) { return v.category[(int)c]; };
    const double sxi = cat(VramCategory::SplatXImg);
    const double sxi_peak = std::max(sxi, v.grow_peak_mean - v.scratch);
    const Row rows[] = {
        {&msg::vram_cat_splat, cat(VramCategory::Splat), 0.0, bar},
        {&msg::vram_cat_splat_x_img, sxi, v.valid ? sxi_peak : 0.0, bar},
        {&msg::vram_cat_image, cat(VramCategory::Image), 0.0, bar},
        {&msg::vram_cat_appearance, cat(VramCategory::Appearance), 0.0, bar},
        {&msg::vram_cat_viewer, cat(VramCategory::Viewer), 0.0, bar},
        {&msg::vram_cat_other, cat(VramCategory::Other), 0.0, bar},
        {&msg::vram_cat_scratch, v.scratch, 0.0, bar},
        {&msg::vram_cat_unpooled, v.unpooled, 0.0, bar},
        {&msg::vram_legend_others, v.others_bytes, 0.0, kOthers},
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

ImVec4 oom_risk_color(OomRisk r) {
    return r == OomRisk::High ? kErr : r == OomRisk::Medium ? kWarn : kOk;
}

void vram_forecast_card(const VramForecast& v, int total_steps) {
    ui::Text(msg::vram_chart_title);
    chart(v, total_steps);
    if (v.valid && !v.projection.empty() && v.total_bytes > 0.0) {
        char pct[16];
        std::snprintf(pct, sizeof pct, "%.0f", v.p_oom * 100.0);
        ui::TextColoredWrapped(oom_risk_color(v.risk), msg::vram_chart_peak,
                               {gib(v.peak_mean), gib(v.peak_sigma),
                                gib(0.99 * v.total_bytes - v.others_bytes),
                                std::string(pct)});
        if (v.provisional) ui::TextDisabledWrapped(msg::vram_chart_provisional);
    } else if (!v.valid) {
        ui::TextDisabledWrapped(msg::vram_chart_waiting);
    }
    ImGui::Spacing();
    ui::Text(msg::vram_breakdown_title);
    breakdown(v);
}

}  // namespace gui

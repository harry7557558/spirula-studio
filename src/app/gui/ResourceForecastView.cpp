// ResourceForecastView.cpp -- see ResourceForecastView.h.

#include "app/gui/ResourceForecastView.h"

#include "i18n/TimeFormat.h"
#include "i18n/catalog/Gui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace gui {

namespace msg = spirula::i18n::msg::gui;
using spirula::OomRisk;
using spirula::ResourceForecast;

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

// The ceiling the chart draws: Windows charges VRAM to commit, and with little left that is it.
double ceiling(const ResourceForecast& f) {
    return f.commit_limited ? f.others + f.limit : f.capacity;
}

// Stacked from the left: this job solid, its growth to the projected peak
// faint, then other programs grey. Whole pixels, one rounded outline.
void stacked_bar(float width, const ResourceForecast& f, ImVec4 color) {
    const float h = std::floor(ImGui::GetTextLineHeight());
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    const ImVec2 p(std::floor(cursor.x), std::floor(cursor.y));
    const float w = std::floor(width);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float r = ImGui::GetStyle().FrameRounding;
    auto x = [&](double bytes) {
        return p.x + std::round(w * (float)std::clamp(bytes / f.capacity, 0.0, 1.0));
    };
    const double grown = f.projected ? std::max(f.peak_mean, f.ours) : f.ours;
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), ImGui::GetColorU32(ImGuiCol_FrameBg), r);
    dl->PushClipRect(p, ImVec2(p.x + w, p.y + h), true);
    dl->AddRectFilled(ImVec2(x(grown), p.y), ImVec2(x(grown + f.others), p.y + h),
                      with_alpha(kOthers, 0.45f));
    dl->AddRectFilled(ImVec2(x(f.ours), p.y), ImVec2(x(grown), p.y + h), with_alpha(color, 0.3f));
    dl->AddRectFilled(p, ImVec2(x(f.ours), p.y + h), ImGui::GetColorU32(color));
    dl->PopClipRect();
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), ImGui::GetColorU32(ImGuiCol_Border), r);
}

}  // namespace

ImVec4 oom_risk_color(OomRisk r) {
    return r == OomRisk::High ? kErr : r == OomRisk::Medium ? kWarn : kOk;
}

const spirula::i18n::Msg* memory_risk_label(OomRisk r) {
    switch (r) {
        case OomRisk::Low:    return &msg::memory_risk_low;
        case OomRisk::Medium: return &msg::memory_risk_medium;
        case OomRisk::High:   return &msg::memory_risk_high;
        default:              return nullptr;
    }
}

void forecast_chart(const ResourceForecast& f, double x_end, const std::string& x_end_label) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImGuiStyle& st = ImGui::GetStyle();
    const float line = ImGui::GetTextLineHeight();
    const float W = px(460.0f), H = px(150.0f);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(W, H + line + st.ItemInnerSpacing.y));

    const bool level = f.projected && f.projection.empty();
    double y_max = f.capacity;
    for (const auto& s : f.history) y_max = std::max(y_max, s.ours + s.others);
    for (const auto& b : f.projection) y_max = std::max(y_max, f.others + b.mean + 2.0 * b.sigma);
    if (level) y_max = std::max(y_max, f.others + f.peak_mean + 2.0 * f.peak_sigma);
    y_max = std::max(y_max * 1.05, 1.0);
    const double T = std::max(x_end, 1e-9);

    const float label_w = ImGui::CalcTextSize("00.0").x + st.ItemInnerSpacing.x;
    const ImVec2 r0(p0.x + label_w, p0.y);
    const ImVec2 r1(p0.x + W, p0.y + H);
    auto X = [&](double x) {
        return r0.x + (r1.x - r0.x) * (float)std::clamp(x / T, 0.0, 1.0);
    };
    auto Y = [&](double bytes) {
        return r1.y - (r1.y - r0.y) * (float)std::clamp(bytes / y_max, 0.0, 1.0);
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
        dl->AddText(ImVec2(r0.x - sz.x - st.ItemInnerSpacing.x, y - sz.y * 0.5f), text_dim, s);
    }

    // Feathered edges between adjacent slices would show as stripes.
    const ImDrawListFlags flags = dl->Flags;
    dl->Flags &= ~ImDrawListFlags_AntiAliasedFill;
    const ImU32 others_fill = with_alpha(kOthers, 0.35f);
    for (size_t i = 1; i < f.history.size(); ++i) {
        const auto& a = f.history[i - 1];
        const auto& b = f.history[i];
        dl->AddQuadFilled(ImVec2(X(a.x), Y(0)), ImVec2(X(a.x), Y(a.others)),
                          ImVec2(X(b.x), Y(b.others)), ImVec2(X(b.x), Y(0)), others_fill);
    }
    const double last_x = f.history.empty() ? 0.0 : f.history.back().x;
    if (!f.projection.empty())
        dl->AddRectFilled(ImVec2(X(last_x), Y(f.others)), ImVec2(r1.x, Y(0)), others_fill);
    dl->Flags = flags;

    const ImVec4 run = ImGui::GetStyleColorVec4(ImGuiCol_PlotLines);
    for (size_t i = 1; i < f.projection.size(); ++i) {
        const auto& a = f.projection[i - 1];
        const auto& b = f.projection[i];
        const double o = f.others;
        dl->AddQuadFilled(ImVec2(X(a.x), Y(o + a.mean + 2.0 * a.sigma)),
                          ImVec2(X(b.x), Y(o + b.mean + 2.0 * b.sigma)),
                          ImVec2(X(b.x), Y(o + b.mean - 2.0 * b.sigma)),
                          ImVec2(X(a.x), Y(o + a.mean - 2.0 * a.sigma)), with_alpha(run, 0.22f));
        dashed_line(dl, ImVec2(X(a.x), Y(o + a.mean)), ImVec2(X(b.x), Y(o + b.mean)),
                    with_alpha(run, 0.9f), px(1.5f));
    }
    if (level) {
        const double o = f.others;
        dl->AddRectFilled(ImVec2(r0.x, Y(o + f.peak_mean + 2.0 * f.peak_sigma)),
                          ImVec2(r1.x, Y(o + std::max(0.0, f.peak_mean - 2.0 * f.peak_sigma))),
                          with_alpha(run, 0.16f));
        dashed_line(dl, ImVec2(r0.x, Y(o + f.peak_mean)), ImVec2(r1.x, Y(o + f.peak_mean)),
                    with_alpha(run, 0.9f), px(1.5f));
    }
    for (size_t i = 1; i < f.history.size(); ++i) {
        const auto& a = f.history[i - 1];
        const auto& b = f.history[i];
        dl->AddLine(ImVec2(X(a.x), Y(a.ours + a.others)), ImVec2(X(b.x), Y(b.ours + b.others)),
                    ImGui::GetColorU32(run), px(2.0f));
    }
    if (!f.projection.empty())
        dl->AddLine(ImVec2(X(last_x), r0.y), ImVec2(X(last_x), r1.y), grid);
    if (ceiling(f) > 0.0)
        dl->AddLine(ImVec2(r0.x, Y(ceiling(f))), ImVec2(r1.x, Y(ceiling(f))),
                    ImGui::GetColorU32(kErr), px(1.5f));
    dl->AddRect(r0, r1, grid);

    dl->AddText(ImVec2(r0.x, r1.y + st.ItemInnerSpacing.y), text_dim, "0");
    dl->AddText(ImVec2(r1.x - ImGui::CalcTextSize(x_end_label.c_str()).x,
                       r1.y + st.ItemInnerSpacing.y),
                text_dim, x_end_label.c_str());
}

void forecast_summary(const ResourceForecast& f) {
    if (!f.projected || f.limit <= 0.0) return;
    char pct[16];
    std::snprintf(pct, sizeof pct, "%.0f", f.p_oom * 100.0);
    ui::TextColoredWrapped(oom_risk_color(f.risk), msg::memory_forecast_summary,
                           {gib(f.ours), gib(f.peak_mean), gib(f.peak_sigma), gib(f.limit),
                            std::string(pct)});
}

void forecast_legend(const spirula::i18n::Msg& run_label) {
    const ImVec4 run = ImGui::GetStyleColorVec4(ImGuiCol_PlotLines);
    swatch(ImGui::GetColorU32(run));
    ui::TextDisabled(run_label);
    ImGui::SameLine();
    swatch(with_alpha(run, 0.35f));
    ui::TextDisabled(msg::vram_legend_projected);
    ImGui::SameLine();
    swatch(with_alpha(kOthers, 0.35f));
    ui::TextDisabled(msg::vram_legend_others);
    ImGui::SameLine();
    swatch(ImGui::GetColorU32(kErr));
    ui::TextDisabled(msg::vram_legend_capacity);
}

void memory_strip(const ResourceForecast& ram, const ResourceForecast& vram,
                  const spirula::i18n::Msg& help, float x0, float avail) {
    if (!ram.known) return;
    const OomRisk risk = std::max(ram.risk, vram.risk);
    auto hover = [&] {
        if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_NoSharedDelay) ||
            !ImGui::BeginTooltip())
            return;
        ImGui::PushTextWrapPos(px(460.0f));
        ui::TextDisabled(help);
        ImGui::PopTextWrapPos();
        ImGui::Separator();
        for (const auto* f : {&ram, &vram}) {
            if (!f->known || f->capacity <= 0.0) continue;
            ui::Text(f == &ram ? msg::memory_chart_host : msg::memory_chart_device);
            const double end = f->history.empty() ? 1.0 : f->history.back().x;
            forecast_chart(*f, end, spirula::i18n::format_duration_coarse(end));
            forecast_summary(*f);
        }
        forecast_legend(msg::memory_legend_run);
        ImGui::EndTooltip();
    };

    const ImGuiStyle& st = ImGui::GetStyle();
    const bool device = vram.known && vram.capacity > 0.0;
    const std::string host_text = gib(ram.ours) + " / " + gib(ram.capacity) + " GiB";
    const std::string device_text = device ? gib(vram.ours) + " / " + gib(vram.capacity) + " GiB" : "";
    const auto* label = memory_risk_label(risk);
    // Right-aligned like the training readout; the bars give way first when the row is short.
    float text_w = ImGui::CalcTextSize("RAM").x + ImGui::CalcTextSize(host_text.c_str()).x +
                   st.ItemInnerSpacing.x;
    if (device)
        text_w += ImGui::CalcTextSize("VRAM").x + ImGui::CalcTextSize(device_text.c_str()).x +
                  st.ItemInnerSpacing.x + st.ItemSpacing.x * 2;
    if (label) text_w += ImGui::CalcTextSize(label->get()).x + st.ItemSpacing.x;
    const int bars = device ? 2 : 1;
    float bar_w = px(120.0f);
    float target = x0 + avail - text_w - bars * (bar_w + st.ItemInnerSpacing.x) - px(8.0f);
    if (target <= ImGui::GetCursorPosX()) {
        bar_w = 0.0f;
        target = x0 + avail - text_w - px(8.0f);
    }
    if (target > ImGui::GetCursorPosX()) ImGui::SetCursorPosX(target);
    if (label) {
        ui::TextColored(oom_risk_color(risk), *label);
        hover();
        ImGui::SameLine();
    }
    auto resource = [&](const char* name, const char* id, const std::string& text,
                        const ResourceForecast& f) {
        ui::TextRaw(name);
        hover();
        ImGui::SameLine(0.0f, st.ItemInnerSpacing.x);
        if (bar_w > 0 && f.capacity > 0) {
            stacked_bar(bar_w, f, oom_risk_color(f.risk));
            ui::InvisibleButtonRaw(id, ImVec2(bar_w, ImGui::GetTextLineHeight()));
            hover();
            ImGui::SameLine(0.0f, st.ItemInnerSpacing.x);
        }
        ui::TextDisabledRaw(text);
        hover();
    };
    resource("RAM", "##mem-ram", host_text, ram);
    if (device) {
        ImGui::SameLine(0.0f, st.ItemSpacing.x * 2);
        resource("VRAM", "##mem-vram", device_text, vram);
    }
}

}  // namespace gui

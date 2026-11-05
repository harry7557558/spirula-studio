// DistanceScaleTool.cpp -- see DistanceScaleTool.h.

#include "app/gui/edit/DistanceScaleTool.h"

#include "app/gui/Layout.h"
#include "app/gui/Ui.h"
#include "app/gui/edit/EditDoc.h"
#include "app/gui/edit/SelectShape.h"
#include "i18n/catalog/EditTransform.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace xmsg = spirula::i18n::msg::xform;

namespace gui {

namespace {

constexpr ImU32 kColA = IM_COL32(255, 195, 45, 255);
constexpr ImU32 kColB = IM_COL32(255, 115, 60, 255);
constexpr ImU32 kColLine = IM_COL32(255, 200, 60, 240);
constexpr ImU32 kColHot = IM_COL32(255, 250, 110, 255);
constexpr ImU32 kColPreview = IM_COL32(255, 195, 50, 180);

bool project_pt(const ViewProjection& cam, const double p[3], float& x, float& y) {
    const float q[3] = {(float)p[0], (float)p[1], (float)p[2]};
    float depth;
    return cam.project(q, x, y, depth) && depth > 0.0f;
}

}  // namespace

void DistanceScaleTool::set_active(bool a) {
    if (_active == a) return;
    _active = a;
    if (!_active) reset();
}

void DistanceScaleTool::reset() {
    _preview = false;
    _has_a = false;
    _has_b = false;
    _hover_point = -1;
    _drag_point = -1;
    _hover_surface = false;
}

double DistanceScaleTool::measured_distance() const {
    if (!_has_a || !_has_b) return 0.0;
    const double dx = _pt_b[0] - _pt_a[0];
    const double dy = _pt_b[1] - _pt_a[1];
    const double dz = _pt_b[2] - _pt_a[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

void DistanceScaleTool::set_target_distance(float d) {
    if (d > 0.0f) {
        _target_distance = d;
        _preview = true;
    }
}

int DistanceScaleTool::hint_state() const {
    if (!_has_a) return 0;
    if (!_has_b) return 1;
    return 2;
}

bool DistanceScaleTool::hit_surface(const EditDoc& doc, const ViewProjection& vp,
                                    float px, float py, const spirula::Sim3& to_saved,
                                    double out_saved[3]) const {
    const int was = doc.layer();
    const_cast<EditDoc&>(doc).set_layer(0);
    const int64_t hit = pick_element(doc, vp, px, py, 16.0f);
    const_cast<EditDoc&>(doc).set_layer(was);
    if (hit < 0) return false;
    const float* p = doc.positions_of(0) + hit * 3;
    const double pm[3] = {p[0], p[1], p[2]};
    to_saved.apply(pm, out_saved);
    return true;
}

spirula::Sim3 DistanceScaleTool::preview_step() const {
    const double d = measured_distance();
    if (!_active || !_preview || d <= 1e-6 || !std::isfinite(_target_distance) ||
        _target_distance <= 0.0f) return {};
    const double pivot[3] = {
        (_pt_a[0] + _pt_b[0]) * 0.5,
        (_pt_a[1] + _pt_b[1]) * 0.5,
        (_pt_a[2] + _pt_b[2]) * 0.5,
    };
    return spirula::Sim3::scale_about((double)_target_distance / d, pivot);
}

void DistanceScaleTool::trigger_scale(spirula::Sim3& out_step, bool& should_scale) {
    const double d = measured_distance();
    if (_has_a && _has_b && d > 1e-6 && std::isfinite(_target_distance) && _target_distance > 0.0f) {
        const double factor = (double)_target_distance / d;
        const double pivot[3] = {
            (_pt_a[0] + _pt_b[0]) * 0.5,
            (_pt_a[1] + _pt_b[1]) * 0.5,
            (_pt_a[2] + _pt_b[2]) * 0.5,
        };
        out_step = spirula::Sim3::scale_about(factor, pivot);
        should_scale = true;
        set_active(false);
    }
}

bool DistanceScaleTool::update(const ViewportInput& in, const ViewProjection& vp,
                               const EditDoc& doc, const spirula::Sim3& to_saved,
                               const spirula::Sim3& saved_to_shared,
                               spirula::Sim3& out_step, bool& should_scale) {
    if (!_active) return false;

    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        if (_has_b) _has_b = false;
        else if (_has_a) _has_a = false;
        else _active = false;
        return true;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
        if (_has_a && _has_b && std::isfinite(_target_distance) && _target_distance > 0.0f) {
            trigger_scale(out_step, should_scale);
            return true;
        }
    }

    _hover_point = -1;
    const float grab_r = px(14.0f);
    double sh_a[3], sh_b[3];
    float ax = 0, ay = 0, bx = 0, by = 0;
    bool has_ax = false, has_bx = false;
    if (_has_a) {
        saved_to_shared.apply(_pt_a, sh_a);
        has_ax = project_pt(vp, sh_a, ax, ay);
    }
    if (_has_b) {
        saved_to_shared.apply(_pt_b, sh_b);
        has_bx = project_pt(vp, sh_b, bx, by);
    }

    if (in.hovered) {
        float da = has_ax ? std::hypot(in.x - ax, in.y - ay) : 1e9f;
        float db = has_bx ? std::hypot(in.x - bx, in.y - by) : 1e9f;
        if (da < grab_r && da <= db) _hover_point = 0;
        else if (db < grab_r) _hover_point = 1;
    }

    // A pick is a pass over every element (several on a miss), so the pointer
    // is picked under once it rests and on a click, not on every frame it moves.
    const bool moved = std::fabs(in.x - _last_mouse[0]) > 1.0f || std::fabs(in.y - _last_mouse[1]) > 1.0f;
    _last_mouse[0] = in.x;
    _last_mouse[1] = in.y;
    if (moved) _pick_stale = true;
    auto pick_here = [&] {
        _hover_surface = hit_surface(doc, vp, in.x, in.y, to_saved, _hover_pos);
        _pick_stale = false;
    };

    if (_drag_point >= 0) {
        if (in.down && (moved || !_pick_stale)) return true;
        pick_here();
        if (_hover_surface) {
            double* pt = _drag_point == 0 ? _pt_a : _pt_b;
            for (int k = 0; k < 3; k++) pt[k] = _hover_pos[k];
        }
        if (!in.down) _drag_point = -1;
        return true;
    }

    if (in.clicked && in.hovered && _hover_point >= 0) {
        _drag_point = _hover_point;
        return true;
    }

    if (!in.hovered) _hover_surface = false;
    else if ((_pick_stale && !moved) || in.clicked) pick_here();

    if (in.clicked && in.hovered && _hover_surface) {
        if (!_has_a) {
            for (int k = 0; k < 3; k++) _pt_a[k] = _hover_pos[k];
            _has_a = true;
            return true;
        }
        if (!_has_b) {
            for (int k = 0; k < 3; k++) _pt_b[k] = _hover_pos[k];
            _has_b = true;
            _target_distance = (float)measured_distance();
            return true;
        }
    }

    return in.down || in.clicked;
}

void DistanceScaleTool::draw_overlay(ImDrawList* dl, const ViewportOverlay& v,
                                     const ViewProjection& vp,
                                     const spirula::Sim3& saved_to_shared) const {
    if (!_active || !dl) return;
    const ImVec2 origin(v.x, v.y);

    double sh_a[3], sh_b[3];
    float ax = 0, ay = 0, bx = 0, by = 0;
    bool has_ax = false, has_bx = false;

    if (_has_a) {
        saved_to_shared.apply(_pt_a, sh_a);
        has_ax = project_pt(vp, sh_a, ax, ay);
    }
    if (_has_b) {
        saved_to_shared.apply(_pt_b, sh_b);
        has_bx = project_pt(vp, sh_b, bx, by);
    }

    if (has_ax && has_bx) {
        const ImVec2 pA(origin.x + ax, origin.y + ay);
        const ImVec2 pB(origin.x + bx, origin.y + by);
        dl->AddLine(pA, pB, IM_COL32(0, 0, 0, 180), px(4.0f));
        dl->AddLine(pA, pB, kColLine, px(2.0f));

        float nx = -(pB.y - pA.y), ny = pB.x - pA.x;
        const float nlen = std::hypot(nx, ny);
        if (nlen > 1e-3f) {
            const float tick = px(7.0f) / nlen;
            nx *= tick; ny *= tick;
            dl->AddLine(ImVec2(pA.x - nx, pA.y - ny), ImVec2(pA.x + nx, pA.y + ny),
                        IM_COL32(0, 0, 0, 180), px(3.5f));
            dl->AddLine(ImVec2(pA.x - nx, pA.y - ny), ImVec2(pA.x + nx, pA.y + ny),
                        kColA, px(2.0f));
            dl->AddLine(ImVec2(pB.x - nx, pB.y - ny), ImVec2(pB.x + nx, pB.y + ny),
                        IM_COL32(0, 0, 0, 180), px(3.5f));
            dl->AddLine(ImVec2(pB.x - nx, pB.y - ny), ImVec2(pB.x + nx, pB.y + ny),
                        kColB, px(2.0f));
        }

        const double mid_saved[3] = {
            (_pt_a[0] + _pt_b[0]) * 0.5,
            (_pt_a[1] + _pt_b[1]) * 0.5,
            (_pt_a[2] + _pt_b[2]) * 0.5,
        };
        double sh_mid[3];
        saved_to_shared.apply(mid_saved, sh_mid);
        float mx = 0, my = 0;
        if (project_pt(vp, sh_mid, mx, my)) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.4f m", measured_distance());
            const ImVec2 tsz = ImGui::CalcTextSize(buf);
            const float pad_x = px(8.0f), pad_y = px(4.0f);
            const ImVec2 pMid(origin.x + mx, origin.y + my);
            const ImVec2 bmin(pMid.x - tsz.x * 0.5f - pad_x, pMid.y - tsz.y * 0.5f - pad_y);
            const ImVec2 bmax(pMid.x + tsz.x * 0.5f + pad_x, pMid.y + tsz.y * 0.5f + pad_y);
            dl->AddRectFilled(bmin, bmax, IM_COL32(18, 20, 24, 230), px(4.0f));
            dl->AddRect(bmin, bmax, kColLine, px(4.0f), 0, px(1.0f));
            dl->AddText(ImVec2(pMid.x - tsz.x * 0.5f, pMid.y - tsz.y * 0.5f),
                        IM_COL32(255, 255, 255, 255), buf);
        }
    } else if (has_ax && !_has_b && _hover_surface) {
        double sh_h[3];
        saved_to_shared.apply(_hover_pos, sh_h);
        float hx = 0, hy = 0;
        if (project_pt(vp, sh_h, hx, hy)) {
            const ImVec2 pA(origin.x + ax, origin.y + ay);
            const ImVec2 pH(origin.x + hx, origin.y + hy);
            dl->AddLine(pA, pH, IM_COL32(255, 195, 50, 140), px(1.5f));

            const double dx = _hover_pos[0] - _pt_a[0];
            const double dy = _hover_pos[1] - _pt_a[1];
            const double dz = _hover_pos[2] - _pt_a[2];
            const double cur_d = std::sqrt(dx * dx + dy * dy + dz * dz);
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.4f m", cur_d);
            const ImVec2 tsz = ImGui::CalcTextSize(buf);
            const float pad_x = px(8.0f), pad_y = px(4.0f);
            const ImVec2 pMid((pA.x + pH.x) * 0.5f, (pA.y + pH.y) * 0.5f);
            const ImVec2 bmin(pMid.x - tsz.x * 0.5f - pad_x, pMid.y - tsz.y * 0.5f - pad_y);
            const ImVec2 bmax(pMid.x + tsz.x * 0.5f + pad_x, pMid.y + tsz.y * 0.5f + pad_y);
            dl->AddRectFilled(bmin, bmax, IM_COL32(18, 20, 24, 210), px(4.0f));
            dl->AddRect(bmin, bmax, IM_COL32(255, 195, 50, 160), px(4.0f), 0, px(1.0f));
            dl->AddText(ImVec2(pMid.x - tsz.x * 0.5f, pMid.y - tsz.y * 0.5f),
                        IM_COL32(255, 255, 255, 255), buf);
        }
    }

    if ((!_has_a || !_has_b) && _hover_surface && _hover_point < 0) {
        double sh_h[3];
        saved_to_shared.apply(_hover_pos, sh_h);
        float hx = 0, hy = 0;
        if (project_pt(vp, sh_h, hx, hy)) {
            const ImVec2 pH(origin.x + hx, origin.y + hy);
            dl->AddCircle(pH, px(7.0f), kColPreview, 0, px(1.5f));
            dl->AddCircleFilled(pH, px(2.5f), kColA);
        }
    }

    auto draw_marker = [&](const ImVec2& pt, const char* label, bool is_a, bool hot, bool drag) {
        const ImU32 base_col = is_a ? kColA : kColB;
        const ImU32 col_ring = hot || drag ? kColHot : base_col;
        const ImU32 col_dot = hot || drag ? IM_COL32(255, 255, 255, 255) : base_col;
        dl->AddCircleFilled(pt, px(10.0f), IM_COL32(0, 0, 0, 140));
        dl->AddCircle(pt, px(hot || drag ? 9.0f : 8.0f), col_ring, 0, px(hot || drag ? 2.5f : 2.0f));
        dl->AddCircleFilled(pt, px(3.5f), col_dot);

        const ImVec2 badge_pos(pt.x + px(12.0f), pt.y - px(12.0f));
        const float badge_r = px(7.0f);
        dl->AddCircleFilled(badge_pos, badge_r, IM_COL32(18, 20, 24, 230));
        dl->AddCircle(badge_pos, badge_r, col_ring, 0, px(1.0f));
        const ImVec2 lsz = ImGui::CalcTextSize(label);
        dl->AddText(ImVec2(badge_pos.x - lsz.x * 0.5f, badge_pos.y - lsz.y * 0.5f),
                    IM_COL32(255, 255, 255, 255), label);
    };

    if (has_ax) {
        draw_marker(ImVec2(origin.x + ax, origin.y + ay), "A",
                    true, _hover_point == 0, _drag_point == 0);
    }
    if (has_bx) {
        draw_marker(ImVec2(origin.x + bx, origin.y + by), "B",
                    false, _hover_point == 1, _drag_point == 1);
    }
}

void DistanceScaleTool::draw_viewport_toolbar(const ViewportOverlay& v,
                                              spirula::Sim3& out_step,
                                              bool& should_scale) {
    if (!_active) return;

    ImGui::SetNextWindowPos(ImVec2(v.x + v.w * 0.5f, v.y + px(16.0f)),
                            ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.94f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, px(8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(px(14.0f), px(7.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(px(8.0f), px(4.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, px(1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, kColA);

    if (ImGui::Begin("##distancetoolbar", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing)) {
        ui::Text(xmsg::mode_distance_scale);
        ImGui::SameLine();
        ui::TextDisabledRaw("|");

        ImGui::SameLine();
        ui::Text(xmsg::lbl_measured);
        ImGui::SameLine();
        if (_has_a && _has_b) {
            char mbuf[32];
            std::snprintf(mbuf, sizeof(mbuf), "%.4f m", measured_distance());
            ui::TextRaw(mbuf);
        } else {
            ui::TextDisabledRaw("—");
        }

        ImGui::SameLine();
        ui::TextDisabledRaw("|");

        ImGui::SameLine();
        ui::Text(xmsg::lbl_set_distance);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(px(84.0f));
        const bool enter = ui::InputFloatRaw("##set_dist", &_target_distance, 0.0f, 0.0f,
                                             "%.4f", ImGuiInputTextFlags_EnterReturnsTrue);
        if (ImGui::IsItemEdited()) _preview = true;
        ImGui::SameLine();
        ui::TextDisabledRaw("m");

        const bool can_scale = _has_a && _has_b && measured_distance() > 1e-6 && std::isfinite(_target_distance) && _target_distance > 0.0f;
        ImGui::SameLine();
        ImGui::BeginDisabled(!can_scale);
        if (ui::Button(xmsg::btn_apply_scale) || (enter && can_scale)) {
            trigger_scale(out_step, should_scale);
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ui::Button(xmsg::btn_reset_points)) {
            reset();
        }

    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(4);
}

void DistanceScaleTool::draw_panel_ui(spirula::Sim3& out_step, bool& should_scale) {
    if (!_active) return;
    const ImGuiStyle& st = ImGui::GetStyle();
    const float full = ImGui::GetContentRegionAvail().x;
    const float half = (full - st.ItemSpacing.x) * 0.5f;

    ui::Text(xmsg::lbl_measured);
    ImGui::SameLine();
    if (_has_a && _has_b) {
        char mbuf[32];
        std::snprintf(mbuf, sizeof(mbuf), "%.4f m", measured_distance());
        ui::TextRaw(mbuf);
    } else {
        ui::TextDisabledRaw("—");
    }

    ui::Text(xmsg::lbl_set_distance);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(std::max(full * 0.5f, px(80.0f)));
    const bool enter = ui::InputFloatRaw("##panel_set_dist", &_target_distance, 0.0f, 0.0f,
                                         "%.4f", ImGuiInputTextFlags_EnterReturnsTrue);
    if (ImGui::IsItemEdited()) _preview = true;
    ImGui::SameLine();
    ui::TextDisabledRaw("m");

    const bool can_scale = _has_a && _has_b && measured_distance() > 1e-6 && std::isfinite(_target_distance) && _target_distance > 0.0f;
    ImGui::BeginDisabled(!can_scale);
    if (ui::Button(xmsg::btn_apply_scale, ImVec2(half, 0)) || (enter && can_scale)) {
        trigger_scale(out_step, should_scale);
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ui::Button(xmsg::btn_reset_points, ImVec2(half, 0))) {
        reset();
    }
}

}  // namespace gui


#include "app/gui/edit/EditSession.h"
#include "app/gui/Layout.h"
#include "data/MarkerAlign.h"
#include "imgui.h"

#include <algorithm>
#include <cmath>

namespace gui {

void EditSession::marker_defaults() {
    auto markers = _markers;
    for (auto& m : markers) _marker_dataset.file_from_dataset.apply(m.center, m.center);
    spirula::MarkerSystemConfig config;
    config.three_markers = {_marker_origin, _marker_xaxis, _marker_xyplane, 0};
    spirula::Sim3 frame;
    if (spirula::solve_marker_system(markers, config, frame)) return;
    double up[3] = {0, 0, 1};
    float hint[3];
    if (_doc && _doc->up_hint(hint)) {
        const double view_up[3] = {hint[0], hint[1], hint[2]};
        _doc->view_frame().inverse().rotate(view_up, up);
    }
    const auto suggested = spirula::suggest_marker_system(markers, up);
    _marker_origin = suggested.origin_id;
    _marker_xaxis = suggested.x_axis_id;
    _marker_xyplane = suggested.xy_plane_id;
}

void EditSession::draw_marker_overlay(const ViewportOverlay& v, const ViewProjection& camera) {
    if (_markers.empty() || !v.dl) return;
    const auto to_shared = base_frame() * _doc->placement() * _doc->view_frame() *
                           _marker_dataset.file_from_dataset;
    auto project = [&](const double point[3], ImVec2& screen) {
        double world[3];
        to_shared.apply(point, world);
        const float p[3] = {(float)world[0], (float)world[1], (float)world[2]};
        float x, y, depth;
        if (!camera.project(p, x, y, depth) || !(depth > 0) ||
            !std::isfinite(x) || !std::isfinite(y) || std::abs(x) > 1e6f || std::abs(y) > 1e6f) return false;
        screen = ImVec2(v.x + x, v.y + y);
        return true;
    };
    const ImU32 colors[] = {IM_COL32(255, 90, 85, 255), IM_COL32(90, 230, 115, 255), IM_COL32(85, 155, 255, 255)};
    v.dl->PushClipRect(ImVec2(v.x, v.y), ImVec2(v.x + v.w, v.y + v.h), true);
    for (const auto& m : _markers) {
        ImVec2 at;
        if (!project(m.center, at)) continue;
        const bool selected = m.id == _marker_origin || m.id == _marker_xaxis || m.id == _marker_xyplane;
        const ImU32 color = m.id == _marker_xaxis ? colors[0] : m.id == _marker_xyplane ? colors[1] :
                            m.id == _marker_origin ? IM_COL32_WHITE : IM_COL32(90, 225, 245, 230);
        for (int k = 0; k < 4; ++k) {
            ImVec2 a, b;
            if (project(m.corners[k], a) && project(m.corners[(k + 1) % 4], b))
                v.dl->AddLine(a, b, color, px(selected ? 2 : 1));
        }
        v.dl->AddCircleFilled(at, px(selected ? 5 : 3), color);
        v.dl->AddCircle(at, px(selected ? 6 : 4), IM_COL32(0, 0, 0, 230), 0, px(1));
        const auto label = std::to_string(m.id);
        const ImVec2 text(at.x + px(8), at.y - ImGui::GetFontSize());
        v.dl->AddText(ImVec2(text.x + 1, text.y + 1), IM_COL32_BLACK, label.c_str());
        v.dl->AddText(text, color, label.c_str());
    }
    spirula::MarkerSystemConfig config;
    config.three_markers = {_marker_origin, _marker_xaxis, _marker_xyplane, 0};
    spirula::Sim3 result_from_dataset;
    if (spirula::solve_marker_system(_markers, config, result_from_dataset)) {
        double length = 0;
        for (const auto& m : _markers) if (m.id == _marker_xaxis) {
            double p[3];
            result_from_dataset.apply(m.center, p);
            length = p[0] * 0.35;
        }
        const auto dataset_from_result = result_from_dataset.inverse();
        ImVec2 origin;
        if (project(dataset_from_result.t, origin)) {
            for (int attempt = 0; attempt < 12; ++attempt) {
                float longest = 0;
                for (int axis = 0; axis < 3; ++axis) {
                    double point[3] = {0, 0, 0};
                    point[axis] = length;
                    dataset_from_result.apply(point, point);
                    ImVec2 end;
                    if (!project(point, end)) { longest = 1e9f; break; }
                    longest = std::max(longest, std::hypot(end.x - origin.x, end.y - origin.y));
                }
                if (longest <= px(140)) break;
                length *= 0.5;
            }
            const char* names[] = {"+X", "+Y", "+Z"};
            for (int axis = 0; axis < 3; ++axis) {
                double endpoint[3] = {0, 0, 0};
                endpoint[axis] = length;
                dataset_from_result.apply(endpoint, endpoint);
                ImVec2 end;
                if (!project(endpoint, end)) continue;
                v.dl->AddLine(origin, end, IM_COL32(0, 0, 0, 210), px(5));
                v.dl->AddLine(origin, end, colors[axis], px(3));
                const float dx = end.x - origin.x, dy = end.y - origin.y;
                const float norm = std::hypot(dx, dy);
                if (norm > px(10)) {
                    const float ux = dx / norm * px(9), uy = dy / norm * px(9);
                    v.dl->AddTriangleFilled(end, ImVec2(end.x-ux-uy*0.45f, end.y-uy+ux*0.45f),
                                           ImVec2(end.x-ux+uy*0.45f, end.y-uy-ux*0.45f), colors[axis]);
                }
                const ImVec2 at(end.x + px(6), end.y + px(4));
                v.dl->AddText(ImVec2(at.x + 1, at.y + 1), IM_COL32_BLACK, names[axis]);
                v.dl->AddText(at, colors[axis], names[axis]);
            }
        }
    }
    v.dl->PopClipRect();
}

}  // namespace gui

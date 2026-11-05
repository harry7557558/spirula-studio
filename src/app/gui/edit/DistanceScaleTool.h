#pragma once

// DistanceScaleTool.h -- two-point reference measurement and Sim3 metric scaling.

#include "app/gui/ViewportInput.h"
#include "app/gui/edit/SelectShape.h"
#include "core/Similarity.h"

struct ImDrawList;

namespace gui {

class EditDoc;

class DistanceScaleTool {
public:
    bool active() const { return _active; }
    void set_active(bool a);
    void reset();

    bool has_point_a() const { return _has_a; }
    bool has_point_b() const { return _has_b; }
    const double* point_a() const { return _pt_a; }
    const double* point_b() const { return _pt_b; }
    double measured_distance() const;

    float target_distance() const { return _target_distance; }
    void set_target_distance(float d);

    bool update(const ViewportInput& in, const ViewProjection& vp,
                const EditDoc& doc, const spirula::Sim3& to_saved,
                const spirula::Sim3& saved_to_shared,
                spirula::Sim3& out_step, bool& should_scale);

    void draw_overlay(ImDrawList* dl, const ViewportOverlay& v,
                      const ViewProjection& vp,
                      const spirula::Sim3& saved_to_shared) const;

    void draw_viewport_toolbar(const ViewportOverlay& v,
                               spirula::Sim3& out_step,
                               bool& should_scale);

    void draw_panel_ui(spirula::Sim3& out_step, bool& should_scale);

    spirula::Sim3 preview_step() const;
    int hint_state() const;

private:
    bool hit_surface(const EditDoc& doc, const ViewProjection& vp,
                     float px, float py, const spirula::Sim3& to_saved,
                     double out_saved[3]) const;
    void trigger_scale(spirula::Sim3& out_step, bool& should_scale);

    bool _preview = false;
    bool _active = false;
    bool _has_a = false;
    bool _has_b = false;
    double _pt_a[3] = {0, 0, 0};
    double _pt_b[3] = {0, 0, 0};
    float _target_distance = 1.0f;

    int _hover_point = -1;
    int _drag_point = -1;
    bool _hover_surface = false;
    double _hover_pos[3] = {0, 0, 0};
    float _last_mouse[2] = {-1.0f, -1.0f};
    bool _pick_stale = true;
};

}  // namespace gui


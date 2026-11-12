// distance_scale_test -- DistanceScaleTool Euclidean measurement and Sim3 scaling.

#include "app/gui/edit/DistanceScaleTool.h"
#include "app/gui/edit/SelectShape.h"
#include "app/webviewer/RegionOverlay.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

bool close_to(double a, double b, double tol = 1e-6) {
    return std::fabs(a - b) <= tol;
}

void test_orthographic_selection() {
    gui::ViewProjection view;
    view.camera_model = 4;
    view.fx = 100; view.fy = 120; view.cx = 320; view.cy = 240;
    float x, y, depth;
    const float near_point[3] = {0.5f, -0.25f, 1.0f};
    const float far_point[3] = {0.5f, -0.25f, 100.0f};
    const float behind[3] = {0.5f, -0.25f, -1.0f};
    check(view.project(near_point, x, y, depth) && close_to(x, 370) &&
          close_to(y, 210) && close_to(depth, 1), "orthographic projection");
    check(view.project(far_point, x, y, depth) && close_to(x, 370) &&
          close_to(y, 210) && close_to(depth, 100), "depth-independent projection");
    check(!view.project(behind, x, y, depth), "orthographic behind-camera culling");
    float origin[3], dir[3];
    check(view.unproject(370, 210, origin, dir) && close_to(origin[0], 0.5) &&
          close_to(origin[1], -0.25) && close_to(origin[2], 0) &&
          close_to(dir[0], 0) && close_to(dir[1], 0) && close_to(dir[2], 1),
          "orthographic pick ray origin and direction");
}

void test_orthographic_overlay() {
    spirula::RegionOverlay overlay;
    spirula::RegionOverlay::Layer layer;
    for (int i = 0; i < 3; i++) {
        const float x = i == 0 ? -0.5f : i == 1 ? 0.5f : 0.0f;
        const float z = i == 0 ? -1.0f : i == 1 ? -10.0f : 1.0f;
        layer.xyz.insert(layer.xyz.end(), {x-0.2f,-0.2f,z, x+0.2f,-0.2f,z, x,0.2f,z});
        layer.tri.insert(layer.tri.end(), {uint32_t(3*i),uint32_t(3*i+1),uint32_t(3*i+2)});
    }
    overlay.layers.push_back(layer);
    const float c2w[12] = {1,0,0,0, 0,1,0,0, 0,0,1,0};
    std::vector<uint8_t> pixels(128*128*3, 0);
    spirula::draw_region_overlay(overlay, pixels.data(), 128, 128, nullptr,
                                 c2w, 64, 64, 64, 64, true);
    int count[3] = {};
    for (int y = 0; y < 128; y++) for (int x = 0; x < 128; x++)
        if (pixels[(y*128+x)*3]) count[x < 50 ? 0 : x > 78 ? 1 : 2]++;
    check(count[0] > 10 && count[0] == count[1] && count[2] == 0,
          "orthographic ROI overlay footprint and camera-plane culling");
}

void test_initial_state() {
    gui::DistanceScaleTool tool;
    check(!tool.active(), "initial inactive");
    check(!tool.has_point_a(), "initial no point A");
    check(!tool.has_point_b(), "initial no point B");
    check(close_to(tool.measured_distance(), 0.0), "initial zero distance");
    check(tool.hint_state() == 0, "initial hint state 0");
}

void test_distance_and_scale() {
    gui::DistanceScaleTool tool;
    tool.set_active(true);
    check(tool.active(), "tool active");

    const double pt_a[3] = {1.0, 2.0, 3.0};
    const double pt_b[3] = {4.0, 6.0, 3.0};
    const double dx = pt_b[0] - pt_a[0];
    const double dy = pt_b[1] - pt_a[1];
    const double dz = pt_b[2] - pt_a[2];
    const double measured = std::sqrt(dx * dx + dy * dy + dz * dz);
    check(close_to(measured, 5.0), "euclidean distance is 5.0");

    const float target = 10.0f;
    const double factor = (double)target / measured;
    check(close_to(factor, 2.0), "scale factor is 2.0");

    const double pivot[3] = {
        (pt_a[0] + pt_b[0]) * 0.5,
        (pt_a[1] + pt_b[1]) * 0.5,
        (pt_a[2] + pt_b[2]) * 0.5,
    };
    check(close_to(pivot[0], 2.5), "pivot x is 2.5");
    check(close_to(pivot[1], 4.0), "pivot y is 4.0");
    check(close_to(pivot[2], 3.0), "pivot z is 3.0");

    const spirula::Sim3 step = spirula::Sim3::scale_about(factor, pivot);
    double new_a[3], new_b[3];
    step.apply(pt_a, new_a);
    step.apply(pt_b, new_b);

    check(close_to(new_a[0], -0.5), "new A x is -0.5");
    check(close_to(new_a[1], 0.0), "new A y is 0.0");
    check(close_to(new_a[2], 3.0), "new A z is 3.0");

    check(close_to(new_b[0], 5.5), "new B x is 5.5");
    check(close_to(new_b[1], 8.0), "new B y is 8.0");
    check(close_to(new_b[2], 3.0), "new B z is 3.0");

    const double new_dx = new_b[0] - new_a[0];
    const double new_dy = new_b[1] - new_a[1];
    const double new_dz = new_b[2] - new_a[2];
    const double new_dist = std::sqrt(new_dx * new_dx + new_dy * new_dy + new_dz * new_dz);
    check(close_to(new_dist, 10.0), "scaled distance is exactly target 10.0");
}

void test_reset() {
    gui::DistanceScaleTool tool;
    tool.set_active(true);
    tool.reset();
    check(!tool.has_point_a(), "reset clears point A");
    check(!tool.has_point_b(), "reset clears point B");
    check(close_to(tool.measured_distance(), 0.0), "reset clears distance");
}

}  // namespace

int main() {
    test_orthographic_selection();
    test_orthographic_overlay();
    test_initial_state();
    test_distance_and_scale();
    test_reset();
    if (g_failures != 0) {
        std::printf("%d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("all distance scale tests passed\n");
    return 0;
}


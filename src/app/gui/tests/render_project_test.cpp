// render_project_test -- the render mode's pure half (app/gui/render/): the
// trajectory through its keys, holds, eased ends, constant speed, C2 joins
// and closed loops; when each key is passed; the lens arithmetic and its
// glide between keys; per-key looks; a project's JSON round trip; moved-
// project copies; and a GIF read back through a decoder of its own.

#include "app/gui/render/FlightFit.h"
#include "app/gui/render/GifWriter.h"
#include "app/gui/render/LensPresets.h"
#include "app/gui/render/RenderProject.h"
#include "app/gui/render/Trajectory.h"
#include "app/gui/render/TransitionFx.h"
#include "data/DatasetParser.h"

#include <cstddef>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace gui::render;

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

double dist3(const double a[3], const double b[3]) {
    return std::sqrt((a[0]-b[0])*(a[0]-b[0]) + (a[1]-b[1])*(a[1]-b[1]) + (a[2]-b[2])*(a[2]-b[2]));
}

// |dot| of two unit quaternions: 1 when they are the same rotation.
double qsame(const double a[4], const double b[4]) {
    return std::fabs(a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3]);
}

Keyframe key(double t, double x, double y, double z) {
    Keyframe k;
    k.time = t;
    k.pos[0] = x;
    k.pos[1] = y;
    k.pos[2] = z;
    return k;
}

RenderProject square() {
    RenderProject p;
    p.keys = {key(0, 0, 0, 0), key(1, 1, 0, 0), key(3, 1, 1, 0), key(4, 0, 1, 0)};
    const double target[3] = {0.5, 0.5, -1.0};
    for (Keyframe& k : p.keys) {
        k.aim = true;
        for (int d = 0; d < 3; d++) k.target[d] = target[d];
        update_aim(k, p.up);
    }
    p.keys[0].own_lens = true;
    return p;
}

void test_keys_are_hit() {
    RenderProject p = square();
    p.keys[2].hold = true;
    const Trajectory tr(p);
    bool through = true;
    for (const Keyframe& k : p.keys) {
        const CameraState c = tr.at(k.time);
        through = through && dist3(c.pos, k.pos) < 1e-9 && qsame(c.rot, k.rot) > 1 - 1e-9;
    }
    check(through, "the path passes through every key, position and rotation");

    // Eased ends and a hold: no motion across a tiny step at each.
    auto speed = [&](double t) {
        const CameraState a = tr.at(t - 1e-4), b = tr.at(t + 1e-4);
        return dist3(a.pos, b.pos) / 2e-4;
    };
    const CameraState s0 = tr.at(0.0), s1 = tr.at(1e-4);
    check(dist3(s0.pos, s1.pos) / 1e-4 < 1e-2, "an eased start starts from rest");
    check(speed(3.0) < 1e-2, "a hold stops the camera");
    check(speed(2.0) > 0.1, "between keys it moves");

    p.motion.ease = false;
    const Trajectory lin(p);
    const CameraState e0 = lin.at(0.0), e1 = lin.at(1e-4);
    check(dist3(e0.pos, e1.pos) / 1e-4 > 0.5, "without easing it leaves at speed");
}

void test_constant_speed() {
    RenderProject p;
    // Uneven spacing: a short hop over one second, a long run over another.
    p.keys = {key(0, 0, 0, 0), key(1, 0.1, 0, 0), key(2, 5, 0, 0)};
    p.keys[0].own_lens = true;
    p.motion.constant_speed = true;
    p.motion.ease = false;
    const Trajectory tr(p);
    double lo = 1e30, hi = 0.0;
    for (int i = 1; i < 19; i++) {
        const double t = 2.0 * i / 20.0;
        const CameraState a = tr.at(t), b = tr.at(t + 0.01);
        const double v = dist3(a.pos, b.pos) / 0.01;
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }
    check(hi / lo < 1.05, "constant speed holds the speed along the path within 5%");
    const CameraState end = tr.at(2.0);
    check(std::fabs(end.pos[0] - 5.0) < 1e-6, "constant speed still ends on the last key");
}

void test_aim_and_lens() {
    RenderProject p = square();
    const Trajectory tr(p);
    // An aimed camera looks down -z of its own frame at the target.
    bool aimed = true;
    for (double t = 0.0; t <= 4.0; t += 0.25) {
        const CameraState c = tr.at(t);
        double R[9];
        quat_to_matrix3(c.rot, R);
        double f[3] = {-R[2], -R[5], -R[8]};
        double to[3] = {0.5 - c.pos[0], 0.5 - c.pos[1], -1.0 - c.pos[2]};
        const double n = std::sqrt(to[0]*to[0] + to[1]*to[1] + to[2]*to[2]);
        aimed = aimed && (f[0]*to[0] + f[1]*to[1] + f[2]*to[2]) / n > 1.0 - 1e-6;
    }
    check(aimed, "an aimed move keeps its target in the middle of the picture");

    Keyframe k = p.keys[1];
    k.roll = 30.0;
    update_aim(k, p.up);
    check(std::fabs(roll_of(k.rot, k.pos, k.target, p.up) - 30.0) < 1e-6,
          "roll_of reads back the roll aim_rotation was given");

    Lens l;
    bool fov_ok = true;
    for (Projection pr : {Projection::Perspective, Projection::Fisheye, Projection::Equisolid})
        for (double fov : {30.0, 90.0, 150.0}) {
            l.projection = pr;
            lens_set_fov(l, fov);
            fov_ok = fov_ok && std::fabs(lens_fov(l) - fov) < 1e-6;
        }
    check(fov_ok, "field of view round-trips through the focal ratio, every model");
    l.projection = Projection::Perspective;
    lens_set_fov(l, 2.0 * std::atan(18.0 / 50.0) * 180.0 / 3.14159265358979);
    check(std::fabs(lens_mm(l) - 50.0) < 1e-6, "a 50 mm lens is 50 mm");

    // A zoom glides in log focal length from one key's lens to the next.
    RenderProject z;
    z.keys = {key(0, 0, 0, 0), key(2, 0, 0, 0)};
    z.keys[0].own_lens = true;
    z.keys[0].lens.focal = 1.0;
    z.keys[1].own_lens = true;
    z.keys[1].lens.focal = 4.0;
    z.motion.ease = false;
    const Trajectory zt(z);
    check(std::fabs(zt.at(1.0).lens.focal - 2.0) < 0.2, "halfway through a zoom is near the geometric mean");
}

void test_json_and_moves() {
    RenderProject p = square();
    p.keys[1].own_lens = true;
    p.keys[1].lens.projection = Projection::Fisheye;
    p.keys[1].lens.tier = 2;
    p.keys[1].lens.dist[0] = 0.05f;
    p.keys[2].hold = true;
    p.shots = {{0.0, 0, Transition::Crossfade, 0.5}, {2.0, 1, Transition::Sweep, 1.5}};
    p.sources = {{"a.ply", {}}, {"b.ply", {}}};
    p.sources[1].style.point_style = PointStyle::Sphere;
    p.fade_out.colour = FadeColour::White;
    p.output.kind = OutputKind::Frames;
    p.output.codec = Codec::Av1;
    p.output.format = ImageFormat::PngAlpha;
    p.motion.curve = Curve::CatmullRom;
    p.motion.loop = true;
    p.sources[0].style.primitive = "3dgut";
    const RenderProject q = project_from_json(project_to_json(p));
    bool same = q.keys.size() == p.keys.size();
    for (size_t i = 0; same && i < p.keys.size(); i++)
        same = dist3(q.keys[i].pos, p.keys[i].pos) < 1e-12 && q.keys[i].aim == p.keys[i].aim &&
               q.keys[i].hold == p.keys[i].hold && (i == 0 || q.keys[i].own_lens == p.keys[i].own_lens);
    same = same && q.keys[1].lens == p.keys[1].lens && q.shots.size() == 2 &&
           q.shots[1].transition == Transition::Sweep && q.sources[1].style.point_style == PointStyle::Sphere &&
           q.fade_out.colour == FadeColour::None && q.shots[1].exit.own &&
           q.shots[1].exit.transition == Transition::Dip && q.shots[1].exit.colour[0] == 1.0f &&
           q.output.kind == OutputKind::Frames &&
           q.output.codec == Codec::Av1 && q.output.format == ImageFormat::PngAlpha &&
           q.motion.curve == Curve::CatmullRom && q.motion.loop &&
           q.sources[0].style.primitive == "3dgut";
    check(same, "a project survives its JSON, an older fade out as the last shot's dip out");
    const RenderProject old = project_from_json(
        "{\"format\":\"spirula-render\",\"version\":1,\"motion\":{\"smooth\":true},"
        "\"output\":{\"image_format\":\"png\",\"transparent\":true}}");
    check(old.motion.curve == Curve::CatmullRom && old.output.format == ImageFormat::PngAlpha,
          "an older project keeps its curve and its transparency");
    check(project_to_json(project_from_json(project_to_json(q))) == project_to_json(q),
          "and, once read, writes back byte for byte");

    // Turning the whole path keeps every camera aimed at the turned target.
    const double axis[3] = {0, 0, 1}, c[3] = {0, 0, 0};
    const spirula::Sim3 turn = spirula::Sim3::rotation_about(axis, 0.7, c);
    RenderProject t = p;
    transform_project(t, turn);
    const Trajectory a(p), b(t);
    bool rigid = true;
    for (double s = 0.0; s <= 4.0; s += 0.5) {
        const CameraState x = a.at(s), y = b.at(s);
        double moved[3];
        turn.apply(x.pos, moved);
        rigid = rigid && dist3(moved, y.pos) < 1e-9;
    }
    check(rigid, "transform_project moves the whole path rigidly");

    // A model saved moved gets moved copies beside it; the originals stay.
    const fs::path dir = fs::temp_directory_path() / "spirula_render_project_test";
    fs::remove_all(dir);
    fs::create_directories(dir / "old" / "renders");
    fs::create_directories(dir / "new");
    { std::FILE* f = std::fopen((dir / "old" / "model.ply").string().c_str(), "wb"); std::fclose(f); }
    { std::FILE* f = std::fopen((dir / "new" / "model.ply").string().c_str(), "wb"); std::fclose(f); }
    save_project(p, (dir / "old" / "renders" / "shot.json").string());
    std::string where;
    const int n = copy_moved_projects((dir / "old" / "model.ply").string(),
                                      (dir / "new" / "model.ply").string(), turn, where);
    const RenderProject moved = load_project((dir / "new" / "renders" / "shot.json").string());
    const RenderProject kept = load_project((dir / "old" / "renders" / "shot.json").string());
    double m[3];
    turn.apply(p.keys[2].pos, m);
    check(n == 1 && dist3(moved.keys[2].pos, m) < 1e-9 && dist3(kept.keys[2].pos, p.keys[2].pos) < 1e-12,
          "a moved model's projects are copied moved, the originals untouched");
    check(default_project_dir((dir / "new" / "model.ply").string()) == (dir / "new" / "renders").string(),
          "the moved copies are what the moved model finds first");
    fs::remove_all(dir);
}

void test_dataset_lenses() {
    ParsedDataset ds;
    ds.num_cameras = 9;
    for (int i = 0; i < 9; i++) {
        const bool wide = i >= 6;
        ds.widths.push_back(1920);
        ds.heights.push_back(1080);
        ds.camera_models.push_back(0);
        ds.camera_distortions.push_back(0);
        const float f = (wide ? 1000.0f : 1600.0f) + (float)(i % 3);
        ds.intrins.insert(ds.intrins.end(), {f, f, 960.0f, 540.0f});
        for (int d = 0; d < 8; d++) ds.dist_coeffs.push_back(0.0f);
    }
    const std::vector<DatasetLens> l = cluster_dataset_lenses(ds);
    check(l.size() == 2 && l[0].count == 6 && l[1].count == 3 &&
              std::fabs(l[0].lens.focal * 1920.0 - 1601.0) < 0.01,
          "a zoom's two ends come out as two lenses, the common one first");
}

// The second derivative either side of each interior key, by differences.
double accel(const Trajectory& tr, double t, double h, int side) {
    const double a = t + side * 2.0 * h, b = t + side * h;
    const CameraState p0 = tr.at(t), p1 = tr.at(b), p2 = tr.at(a);
    double m = 0.0;
    for (int d = 0; d < 3; d++) m = std::max(m, std::fabs(p2.pos[d] - 2.0 * p1.pos[d] + p0.pos[d]) / (h * h));
    return m;
}

void test_spline_is_c2() {
    RenderProject p;
    p.keys = {key(0, 0, 0, 0), key(1, 1, 0.5, 0), key(2.5, 2, -0.3, 0.4), key(3, 3, 0, 0),
              key(4, 3.5, 1, 0)};
    p.keys[0].own_lens = true;
    p.motion.ease = false;
    const Trajectory tr(p);
    bool c2 = true;
    for (int i = 1; i + 1 < (int)p.keys.size(); i++) {
        const double t = p.keys[(size_t)i].time, h = 1e-3;
        // Second differences from each side, against the pair taken across.
        const CameraState l2 = tr.at(t - 2 * h), l1 = tr.at(t - h), c = tr.at(t),
                          r1 = tr.at(t + h), r2 = tr.at(t + 2 * h);
        for (int d = 0; d < 3; d++) {
            const double left = (c.pos[d] - 2 * l1.pos[d] + l2.pos[d]) / (h * h);
            const double right = (r2.pos[d] - 2 * r1.pos[d] + c.pos[d]) / (h * h);
            c2 = c2 && std::fabs(left - right) < 0.05 * (1.0 + std::fabs(left));
        }
    }
    check(c2, "the spline's acceleration is continuous through every key");

    p.motion.curve = Curve::CatmullRom;
    const Trajectory cr(p);
    bool jumps = false;
    for (int i = 1; i + 1 < (int)p.keys.size(); i++) {
        const double t = p.keys[(size_t)i].time;
        jumps = jumps || std::fabs(accel(cr, t, 1e-3, -1) - accel(cr, t, 1e-3, 1)) > 0.5;
    }
    check(jumps, "Catmull-Rom, for comparison, is only C1");
}

void test_closed_loop() {
    // Eight keys round a circle, aimed at its middle, as a closed loop.
    RenderProject p;
    const double kPi = 3.14159265358979323846;
    for (int i = 0; i < 8; i++) {
        const double a = 2.0 * kPi * i / 8.0;
        Keyframe k = key(1.5 * i, 3.0 * std::cos(a), 3.0 * std::sin(a), 1.0);
        k.aim = true;
        update_aim(k, p.up);
        p.keys.push_back(k);
    }
    p.keys[0].own_lens = true;
    p.motion.loop = true;
    p.motion.ease = false;
    p.end = 12.0;
    const Trajectory tr(p);
    check(std::fabs(p.duration() - 12.0) < 1e-12, "a loop lasts until it is back at the start");
    const CameraState a = tr.at(0.0), b = tr.at(12.0);
    check(dist3(a.pos, b.pos) < 1e-9 && qsame(a.rot, b.rot) > 1 - 1e-9, "and ends where it began");
    const CameraState a1 = tr.at(0.01), b1 = tr.at(11.99);
    double va[3], vb[3];
    for (int d = 0; d < 3; d++) {
        va[d] = (a1.pos[d] - a.pos[d]) / 0.01;
        vb[d] = (b.pos[d] - b1.pos[d]) / 0.01;
    }
    check(dist3(va, vb) < 0.02 * std::sqrt(va[0]*va[0] + va[1]*va[1] + va[2]*va[2]),
          "with no kink in its speed across the seam");
    double r_lo = 1e30, r_hi = 0.0;
    for (int i = 0; i < 120; i++) {
        const CameraState c = tr.at(0.1 * i);
        const double r = std::sqrt(c.pos[0] * c.pos[0] + c.pos[1] * c.pos[1]);
        r_lo = std::min(r_lo, r);
        r_hi = std::max(r_hi, r);
    }
    check(r_hi / r_lo < 1.01, "eight keys on a circle make a circle within 1%");

    // A full turn of rotation, not aimed: the double cover comes back as -q.
    RenderProject t;
    for (int i = 0; i < 4; i++) {
        Keyframe k = key(i, 0, 0, 0);
        const double h = kPi * i / 4.0;   // half of a quarter turn about +Z
        k.rot[0] = std::cos(h);
        k.rot[3] = std::sin(h);
        t.keys.push_back(k);
    }
    t.keys[0].own_lens = true;
    t.motion.loop = true;
    t.motion.ease = false;
    const Trajectory tt(t);
    bool steady = true;
    for (int i = 0; i < 40; i++) {
        const CameraState x = tt.at(0.1 * i), y = tt.at(0.1 * i + 0.1);
        const double step = 2.0 * std::acos(std::min(1.0, qsame(x.rot, y.rot)));
        steady = steady && std::fabs(step - kPi / 20.0) < 0.01;
    }
    check(steady, "a looped full turn keeps turning the same way through the seam");
}

void test_lens_kept_on_sort() {
    RenderProject p;
    p.keys = {key(1, 0, 0, 0), key(2, 1, 0, 0), key(0.5, 2, 0, 0)};
    p.keys[0].own_lens = true;
    lens_set_fov(p.keys[0].lens, 90.0);
    p.sort_keys();
    check(std::fabs(lens_fov(p.keys[0].lens) - 90.0) < 1e-9 && p.keys[0].own_lens,
          "a key that becomes the first keeps the lens it was seen through");
}

void test_lens_glides() {
    RenderProject p;
    p.keys = {key(0, 0, 0, 0), key(1, 1, 0, 0), key(3, 2, 0, 0), key(4, 3, 0, 0)};
    p.keys[0].own_lens = true;
    p.keys[0].lens.focal = 1.0;
    p.keys[2].own_lens = true;
    p.keys[2].lens.focal = 9.0;
    check(std::fabs(p.lens_at(1).focal - std::cbrt(9.0)) < 1e-9,
          "a key with no lens of its own glides in log focal between the keys around it");
    check(p.lens_at(3).focal == 9.0, "past the last key with a lens, that lens is held");
    p.motion.loop = true;
    p.end = 6.0;
    check(std::fabs(p.lens_at(3).focal - std::pow(9.0, 2.0 / 3.0)) < 1e-9,
          "a loop glides back to the first key's lens");
    p.motion.loop = false;
    p.keys[0].lens.tier = p.keys[2].lens.tier = 1;
    p.keys[2].lens.dist[0] = 0.3f;
    check(std::fabs(p.lens_at(1).dist[0] - 0.1f) < 1e-6, "distortion glides with the zoom");
    p.keys[2].lens.projection = Projection::Fisheye;
    const Lens held = p.lens_at(1);
    check(held.projection == Projection::Perspective && held.focal == 1.0,
          "across a change of projection the lens before is held");
}

void test_key_times() {
    RenderProject p;
    p.keys = {key(0, 0, 0, 0), key(1, 0.1, 0, 0), key(2, 5, 0, 0)};
    p.keys[0].own_lens = true;
    const Trajectory plain(p);
    check(plain.key_times() == std::vector<double>({0.0, 1.0, 2.0}),
          "without constant speed a key is passed at its own time");
    p.motion.constant_speed = true;
    for (bool ease : {false, true}) {
        p.motion.ease = ease;
        const Trajectory tr(p);
        const std::vector<double>& v = tr.key_times();
        const bool ends = v.size() == 3 && v[0] == 0.0 && std::fabs(v[2] - 2.0) < 1e-9;
        const bool on = ends && dist3(tr.at(v[1]).pos, p.keys[1].pos) < 1e-3;
        check(on && v[1] < 0.5, std::string("at constant speed the camera is on each key at its "
                                            "passing time") + (ease ? ", eased" : ""));
    }
}

void test_looks() {
    RenderProject p;
    p.sources.resize(1);
    p.sources[0].style.colour = false;
    p.sources[0].style.point_px = 2.0f;
    p.keys = {key(0, 0, 0, 0), key(2, 1, 0, 0), key(4, 2, 0, 0)};
    p.keys[0].own_lens = true;
    KeyLook l;
    l.style = p.sources[0].style;
    l.style.colour = true;
    l.style.point_px = 6.0f;
    p.keys[2].looks.push_back(l);
    const std::vector<double> times = {0.0, 2.0, 4.0};
    SourceStyle a, b;
    float mix = -1.0f;
    p.look_at(0, 1.0, times, a, b, mix);
    check(!a.colour && b.colour && std::fabs(mix - 0.25f) < 1e-6,
          "between two looks the picture is mixed by how far along it is");
    check(a.point_px == 3.0f && b.point_px == 3.0f, "a size glides instead of being mixed");
    p.look_at(0, 5.0, times, a, b, mix);
    check(a == l.style && b == l.style && mix == 0.0f, "after the last look it is held");
    p.look_at(0, 0.0, times, a, b, mix);
    check(a == p.sources[0].style && mix == 0.0f, "the model's own style is the look at the start");

    const RenderProject q = project_from_json(project_to_json(p));
    check(q.keys.size() == 3 && q.keys[2].looks.size() == 1 && q.keys[2].looks[0].style == l.style,
          "a keyframe's looks survive the JSON round trip");
    p.keys[1].looks.push_back({3, l.style});
    check(project_from_json(project_to_json(p)).keys[1].looks.empty(),
          "a look for a model the project does not have is dropped on reading");
}

void test_refit_after_delete() {
    // A wobbly path: deleting a key straightens it; the refit should bring
    // the camera back near where it went and where it looked.
    RenderProject before;
    for (int i = 0; i < 8; i++) {
        Keyframe k = key(i, i, 0.4 * std::sin(1.3 * i), 0.2 * std::cos(0.7 * i));
        k.aim = i % 2 == 0;
        k.target[0] = i;
        k.target[1] = 3.0;
        update_aim(k, before.up);
        before.keys.push_back(k);
    }
    before.keys[0].own_lens = true;
    before.keys[0].lens.focal = 1.3;
    auto error = [&](const RenderProject& now) {
        const Trajectory a(before), b(now);
        double worst = 0.0;
        for (int j = 0; j <= 200; j++) {
            const double t = before.duration() * j / 200.0;
            const CameraState ca = a.at(t), cb = b.at(t);
            const double turn = 2.0 * std::acos(std::min(1.0, qsame(ca.rot, cb.rot)));
            worst = std::max(worst, dist3(ca.pos, cb.pos) + turn);
        }
        return worst;
    };
    RenderProject p = before;
    p.keys.erase(p.keys.begin() + 3);
    const double plain = error(p);
    const Lens lens = p.keys[0].lens;
    refit_keys(p, before, 1.0);
    const double fitted = error(p);
    check(fitted < 0.6 * plain, "a key deleted and the rest refitted: the path moves back toward the old one");
    check(p.keys.size() == 7 && p.keys[0].lens == lens && p.keys[0].time == 0.0 &&
              p.keys[6].time == 7.0,
          "the refit leaves the keys' times and lenses alone");

    // An aimed orbit, as the preset makes one, missing a key.
    RenderProject orbit;
    for (int i = 0; i < 8; i++) {
        const double a = 2.0 * 3.14159265358979 * i / 8.0;
        Keyframe k = key(1.5 * i, 2.0 * std::cos(a), 2.0 * std::sin(a), 0.8);
        k.aim = true;
        update_aim(k, orbit.up);
        orbit.keys.push_back(k);
    }
    orbit.keys[0].own_lens = true;
    orbit.motion.loop = true;
    orbit.end = 12.0;
    before = orbit;
    p = orbit;
    p.keys.erase(p.keys.begin() + 5);
    const double gap = error(p);
    refit_keys(p, before, 2.0);
    check(error(p) < 0.5 * gap, "an orbit missing a key is refitted back toward its circle");
}

void test_transitions() {
    // Settings survive the file; a file from before the dips and wipes each
    // became one comes in as the same thing.
    RenderProject p;
    p.sources.resize(2);
    Shot a;
    a.transition = Transition::Spiral;
    shot_defaults(a);
    a.param[0] = 2.5f;
    Shot b;
    b.start = 3.0;
    b.transition = Transition::Dip;
    shot_defaults(b);
    b.colour[0] = 0.25f;
    p.shots = {a, b};
    const RenderProject q = project_from_json(project_to_json(p));
    check(q.shots.size() == 2 && q.shots[0].transition == Transition::Spiral &&
              q.shots[0].param[0] == 2.5f && q.shots[1].transition == Transition::Dip &&
              q.shots[1].colour[0] == 0.25f,
          "a transition's settings survive the JSON round trip");
    const RenderProject old = project_from_json(
        R"({"format":"spirula-render","version":1,"shots":[)"
        R"({"start":0,"source":0,"transition":"dip_white","duration":1},)"
        R"({"start":2,"source":0,"transition":"wipe_right","duration":1}]})");
    check(old.shots.size() == 2 && old.shots[0].transition == Transition::Dip &&
              old.shots[0].colour[0] == 1.0f && old.shots[1].transition == Transition::Wipe &&
              old.shots[1].param[0] == 180.0f,
          "an older file's dip to white and wipe right read as a white dip and a 180-degree wipe");

    // A way out of its own, and which way a 3D one goes, survive too.
    {
        RenderProject r;
        r.sources.resize(2);
        Shot x;
        x.transition = Transition::Ripple;
        shot_defaults(x);
        x.camera = true;
        x.exit.own = true;
        x.exit.transition = Transition::Dust;
        exit_defaults(x.exit);
        x.exit.offset = -0.5;
        x.exit.duration = 2.0;
        Shot y;
        y.start = 4.0;
        r.shots = {x, y};
        const RenderProject back = project_from_json(project_to_json(r));
        check(back.shots.size() == 2 && back.shots[0].camera && back.shots[0].exit.own &&
                  back.shots[0].exit.transition == Transition::Dust &&
                  back.shots[0].exit.offset == -0.5 && back.shots[0].exit.duration == 2.0 &&
                  back.shots[0].exit.camera && !back.shots[1].exit.own,
              "a shot's own way out, its offset and its camera setting survive the file");
        Shot d;
        d.transition = Transition::Dust;
        shot_defaults(d);
        Shot w;
        w.transition = Transition::Sweep;
        shot_defaults(w);
        check(d.camera && !w.camera, "dust falls down the picture, a sweep keeps the world's level");
    }

    // Quantiles and the share below a value undo each other.
    FxGeo g;
    g.radius = 2.0f;
    for (int i = 0; i < kFxQuantiles; i++) {
        const float f = (float)i / (kFxQuantiles - 1);
        g.qh[i] = f * f * 3.0f - 1.0f;
    }
    bool inverse = true;
    for (float f = 0.0f; f <= 1.0f; f += 0.01f)
        inverse = inverse && std::fabs(fx_cdf(g.qh, fx_quantile(g.qh, f)) - f) < 1e-4f;
    check(inverse && fx_cdf(g.qh, -5.0f) == 0.0f && fx_cdf(g.qh, 50.0f) == 1.0f,
          "a share of the elements and where it lies are each other's inverse");

    // Every 3D transition starts with the old model as it was and ends with
    // the new one as it is -- out to the scene's outliers too.
    std::vector<std::array<float, 3>> pts;
    for (uint32_t i = 0; i < 400; i++) {
        float r1, r2;
        fx_random(i, r1, r2);
        const float reach = i % 10 == 0 ? 3.0f : 1.0f;
        pts.push_back({(r1 - 0.5f) * 4.0f * reach, (r2 - 0.5f) * 4.0f * reach,
                       (r1 * r2 - 0.3f) * 3.0f});
    }
    {
        std::vector<float> h, a, r;
        for (const auto& q : pts) {
            h.push_back(q[2]);
            a.push_back(q[0]);
            r.push_back(std::sqrt(q[0] * q[0] + q[1] * q[1]));
        }
        auto table = [&](std::vector<float>& v, float* out) {
            std::sort(v.begin(), v.end());
            for (int i = 0; i < kFxQuantiles; i++)
                out[i] = v[(size_t)std::lround((double)i / (kFxQuantiles - 1) * (v.size() - 1))];
        };
        table(h, g.qh);
        table(a, g.qa);
        table(r, g.qr);
    }
    bool ends = true;
    std::string bad;
    for (int kind = (int)Transition::Dust; kind <= (int)Transition::Ripple; kind++) {
        Shot sh;
        sh.transition = (Transition)kind;
        shot_defaults(sh);
        for (uint32_t i = 0; i < 400; i++) {
            float r1, r2;
            fx_random(i, r1, r2);
            const float* pos = pts[i].data();
            float d[3], al, sz;
            auto still = [&](float want_alpha) {
                return std::fabs(d[0]) + std::fabs(d[1]) + std::fabs(d[2]) < 1e-3f &&
                       std::fabs(al - want_alpha) < 1e-3f && (want_alpha == 0.0f || std::fabs(sz - 1.0f) < 1e-3f);
            };
            fx_apply(kind, false, 0.0f, sh.param, g, pos, r1, r2, d, al, sz);
            bool ok = still(1.0f);
            fx_apply(kind, true, 1.0f, sh.param, g, pos, r1, r2, d, al, sz);
            ok = ok && still(1.0f);
            fx_apply(kind, true, 0.0f, sh.param, g, pos, r1, r2, d, al, sz);
            ok = ok && al < 1e-3f;
            fx_apply(kind, false, 1.0f, sh.param, g, pos, r1, r2, d, al, sz);
            ok = ok && al < 1e-3f;
            if (!ok) { ends = false; bad = std::to_string(kind); }
        }
    }
    // With nothing on the other side a transition has the whole change: from
    // its first moment nothing waits, and it ends where its half would.
    bool solo = true;
    for (int kind = (int)Transition::Dust; kind <= (int)Transition::Ripple; kind++) {
        Shot sh;
        sh.transition = (Transition)kind;
        shot_defaults(sh);
        float moved_in = 0.0f;
        for (uint32_t i = 0; i < 400; i++) {
            float r1, r2, d[3], al, sz;
            fx_random(i, r1, r2);
            const float* pos = pts[i].data();
            fx_apply(kind, true, fx_solo_time(kind, true, 0.0f, sh.param), sh.param, g, pos, r1, r2, d, al, sz);
            solo = solo && al < 1e-3f;
            fx_apply(kind, true, fx_solo_time(kind, true, 1.0f, sh.param), sh.param, g, pos, r1, r2, d, al, sz);
            solo = solo && std::fabs(al - 1.0f) < 1e-3f;
            fx_apply(kind, false, fx_solo_time(kind, false, 1.0f, sh.param), sh.param, g, pos, r1, r2, d, al, sz);
            solo = solo && al < 1e-3f;
            fx_apply(kind, true, fx_solo_time(kind, true, 0.2f, sh.param), sh.param, g, pos, r1, r2, d, al, sz);
            moved_in = std::max(moved_in, al);
        }
        if (kind != (int)Transition::Sweep && kind != (int)Transition::Grow) solo = solo && moved_in > 0.05f;
    }
    check(solo, "with nothing on the other side, a 3D transition starts at once and still ends at rest");
    check(ends, "every 3D transition begins and ends where the models rest" +
                    (bad.empty() ? std::string() : " (fails: " + bad + ")"));
}

// Which lenses want 3DGUT: the sphere, and a distortion that folds in frame.
void test_auto_primitive() {
    Lens pin;
    pin.focal = 0.5;
    Lens sphere;
    sphere.projection = Projection::Equirect;
    Lens mild = pin, folding = pin;
    mild.tier = folding.tier = 1;
    mild.dist[0] = -0.05f;
    folding.dist[0] = -0.3f;
    check(!lens_needs_ut(pin, 1920, 1080) && lens_needs_ut(sphere, 1920, 960) &&
              !lens_needs_ut(mild, 1920, 1080) && lens_needs_ut(folding, 1920, 1080),
          "3DGUT for the sphere and a lens folding in frame, 3DGS for the rest");
    check(resolve_primitive("", "3dgs", folding, 1920, 1080) == "3dgut" &&
              resolve_primitive("", "3dgs", pin, 1920, 1080) == "3dgs" &&
              resolve_primitive("", "mip", pin, 1920, 1080) == "mip" &&
              resolve_primitive("", "3dgut", pin, 1920, 1080) == "3dgs" &&
              resolve_primitive("mip", "3dgs", sphere, 1920, 960) == "mip",
          "the automatic primitive: 3DGUT for the lens that needs it, else as trained; "
          "a chosen one stays");
}

// Who is on screen when, and how far each is through its way in or out.
void test_shot_mix() {
    auto near = [](double a, double b) { return std::fabs(a - b) < 1e-9; };
    Shot a, b;
    b.start = 4.0;
    b.transition = Transition::Crossfade;
    b.duration = 2.0;
    ShotMix m = shot_mix({a, b}, 5.0, 10.0);
    check(m.in == 1 && m.out == 0 && near(m.u_in, 0.5) && !m.own,
          "a crossfade takes the one before out as the next comes in");
    Shot first;
    first.transition = Transition::Crossfade;
    first.duration = 2.0;
    m = shot_mix({first}, 1.0, 10.0);
    check(m.in == 0 && m.out == -1 && near(m.u_in, 0.5) && !m.own,
          "the first shot can arrive from nothing");
    m = shot_mix({first}, 5.0, 10.0);
    check(m.in == 0 && m.out == -1 && near(m.u_in, 1.0), "and is there once it has");

    // Leaving a second early, over two, as the next rains in over two.
    a.exit.own = true;
    a.exit.transition = Transition::Dust;
    a.exit.duration = 2.0;
    a.exit.offset = -1.0;
    b.transition = Transition::Rain;
    const std::vector<Shot> shots = {a, b};
    m = shot_mix(shots, 2.9, 10.0);
    check(m.in == 0 && m.out == -1 && !m.own, "before it leaves, the shot is simply there");
    m = shot_mix(shots, 3.5, 10.0);
    check(m.in == -1 && m.out == 0 && near(m.u_out, 0.25) && m.own,
          "leaving early, it goes before the next arrives");
    m = shot_mix(shots, 4.5, 10.0);
    check(m.in == 1 && m.out == 0 && near(m.u_in, 0.25) && near(m.u_out, 0.75) && m.own,
          "then the two overlap, each through its own");
    m = shot_mix(shots, 5.5, 10.0);
    check(m.in == 1 && m.out == -1 && near(m.u_in, 0.75) && m.own,
          "gone, while the next is still arriving");
    // Leaving a second after the next has come, at once: both whole meanwhile.
    Shot c = a, d = b;
    c.exit.transition = Transition::Cut;
    c.exit.offset = 1.0;
    d.transition = Transition::Cut;
    m = shot_mix({c, d}, 4.5, 10.0);
    check(m.in == 1 && m.out == 0 && near(m.u_in, 1.0) && near(m.u_out, 0.0) && m.own,
          "a later way out keeps both on screen");
    m = shot_mix({c, d}, 5.5, 10.0);
    check(m.in == 1 && m.out == -1, "and a cut out takes it at once");

    // The last shot leaves by its own way at the end: a dip into black a
    // second before the end, over two.
    Shot only;
    only.exit.own = true;
    only.exit.transition = Transition::Dip;
    only.exit.duration = 2.0;
    only.exit.offset = -1.0;
    m = shot_mix({only}, 6.0, 10.0);
    check(m.in == 0 && m.out == -1 && !m.own, "the last shot is simply there before it leaves");
    m = shot_mix({only}, 8.0, 10.0);
    check(m.in == -1 && m.out == 0 && near(m.u_out, 0.5) && m.own,
          "then it leaves by its own way, ending at the end less the offset");
    m = shot_mix({only}, 9.5, 10.0);
    check(m.out == 0 && near(m.u_out, 1.0), "and stays gone, into its colour, to the end");

    // An older file's fades become the first shot's dip in and the last's dip out.
    RenderProject old;
    old.fade_in.colour = FadeColour::White;
    old.fade_in.seconds = 1.5;
    old.fade_out.colour = FadeColour::Black;
    old.fade_out.seconds = 2.0;
    settle_shots(old);
    check(old.shots.size() == 1 && old.shots[0].transition == Transition::Dip &&
              old.shots[0].colour[0] == 1.0f && near(old.shots[0].duration, 1.5) &&
              old.shots[0].exit.own && old.shots[0].exit.transition == Transition::Dip &&
              old.shots[0].exit.colour[0] == 0.0f && near(old.shots[0].exit.duration, 2.0) &&
              old.fade_in.colour == FadeColour::None && old.fade_out.colour == FadeColour::None,
          "the old fades in and out become the shots' own");
    RenderProject kept;
    kept.shots.resize(2);
    kept.shots[0].transition = Transition::Rain;
    kept.shots[0].exit.own = true;
    kept.shots[0].exit.transition = Transition::Dip;
    kept.fade_in.colour = FadeColour::Black;
    settle_shots(kept);
    check(kept.fade_in.colour == FadeColour::Black && kept.shots[0].transition == Transition::Rain &&
              kept.shots[0].exit.transition == Transition::Crossfade,
          "a fade that has no free place stays, and only the last shot ends in a dip");
}

// A hand-flown move: still for a second, round a quarter circle, a two
// second stop half way, the rest of the half circle, still again.
void test_flight() {
    std::vector<FlightSample> fl;
    const double pi = 3.14159265358979;
    auto at = [&](double t, double a) {
        FlightSample s;
        s.t = t;
        s.pos[0] = 4.0 * std::cos(a);
        s.pos[1] = 4.0 * std::sin(a);
        s.pos[2] = 1.0;
        // Facing the middle as it goes: a yaw about +Z.
        const double yaw = a + pi / 2.0;
        s.rot[0] = std::cos(yaw / 2.0);
        s.rot[3] = std::sin(yaw / 2.0);
        // A hand's wobble, a millimetre.
        s.pos[2] += 0.001 * std::sin(t * 37.0);
        fl.push_back(s);
    };
    double t = 0.0;
    for (; t < 1.0; t += 1.0 / 60.0) at(t, 0.0);
    for (double a = 0.0; a < pi / 4.0; a += pi / 4.0 / 120.0, t += 1.0 / 60.0) at(t, a);
    for (double e = t + 2.0; t < e; t += 1.0 / 60.0) at(t, pi / 4.0);
    for (double a = pi / 4.0; a < pi / 2.0; a += pi / 4.0 / 120.0, t += 1.0 / 60.0) at(t, a);
    for (double e = t + 1.0; t < e; t += 1.0 / 60.0) at(t, pi / 2.0);

    RenderProject p;
    FlightFit fit;
    fit.timing = 1.0;
    const int keys = fit_flight(fl, fit, 4.0, Lens{}, p);
    const double flown = p.keys.empty() ? 0.0 : p.keys.back().time;
    check(keys >= 2 && keys < 40 && std::fabs(flown - 6.0) < 0.1,
          "as flown, the still ends go and the stop in the middle stays: " +
              std::to_string(keys) + " keys, " + std::to_string(flown) + " s");
    // Where it rests half way, the camera is still for most of two seconds.
    {
        const Trajectory tr(p);
        double moved = 0.0;
        CameraState last = tr.at(2.4);
        for (double x = 2.5; x < 3.6; x += 0.1) {
            const CameraState c = tr.at(x);
            moved += std::hypot(c.pos[0] - last.pos[0], c.pos[1] - last.pos[1]);
            last = c;
        }
        check(moved < 0.05, "as flown, a stop is kept: moved " + std::to_string(moved));
    }
    fit.timing = 0.5;
    const double cut = flight_length(fl, fit, 4.0);
    fit_flight(fl, fit, 4.0, Lens{}, p);
    check(p.keys.size() >= 2 && std::fabs(p.keys.back().time - cut) < 1e-6 && cut < 4.4 && cut > 3.6,
          "between the two, the stop drops out and the flying stays: " + std::to_string(cut) + " s");
    {
        // Every tenth of a second the camera is on its way: nowhere still.
        const Trajectory tr(p);
        double least = 1e9;
        for (double x = 0.2; x + 0.2 < cut; x += 0.1) {
            const CameraState a = tr.at(x), b = tr.at(x + 0.1);
            least = std::min(least, std::hypot(a.pos[0] - b.pos[0], a.pos[1] - b.pos[1]));
        }
        check(least > 0.02, "with the stop dropped, the camera never halts: " + std::to_string(least));
        // And it passes within the tolerance of where it was flown.
        double worst = 0.0;
        for (const FlightSample& s : fl) {
            double best = 1e9;
            for (double x = 0.0; x <= cut; x += 0.01) {
                const CameraState c = tr.at(x);
                best = std::min(best, std::hypot(c.pos[0] - s.pos[0], c.pos[1] - s.pos[1]));
            }
            worst = std::max(worst, best);
        }
        check(worst < 0.05, "the fit passes where the flight went: off by " + std::to_string(worst));
    }
    fit.detail = 1.0;
    const int close = fit_flight(fl, fit, 4.0, Lens{}, p);
    fit.detail = 0.0;
    const int loose = fit_flight(fl, fit, 4.0, Lens{}, p);
    check(close > loose, "more detail keeps more keys: " + std::to_string(loose) + " then " +
                             std::to_string(close));
    fit.length = 10.0;
    fit_flight(fl, fit, 4.0, Lens{}, p);
    check(std::fabs(p.keys.back().time - 10.0) < 1e-6, "a length asked for is the length");
    // Smoothing the timing makes the speed between keys change gradually: a
    // crawl through one gap of a straight run goes, and a fitted flight comes
    // out smoother than spacing its keys evenly in time makes it.
    {
        RenderProject run;
        run.motion.ease = false;
        for (int i = 0; i < 7; i++) {
            Keyframe k;
            k.pos[0] = i;
            k.time = i < 3 ? i : i + 2.0;          // the gap from 2 to 3 takes 3 s
            run.keys.push_back(k);
        }
        run.keys[0].own_lens = true;
        const double before = motion_energy(run, 1.0);
        RenderProject once = run;
        smooth_key_speeds(once, std::vector<uint8_t>(7, 1), 0.5, 1.0);
        check(motion_energy(once, 1.0) < 0.8 * before && once.keys.front().time == 0.0 &&
                  once.keys.back().time == run.keys.back().time,
              "smoothing the timing evens a crawl out of a straight run, the ends kept");
        fit.length = 0.0;
        fit.timing = 0.5;
        fit.detail = 0.5;
        RenderProject flown;
        fit_flight(fl, fit, 4.0, Lens{}, flown);
        const double e0 = motion_energy(flown, 4.0);
        RenderProject speeds = flown, even = flown;
        smooth_key_speeds(speeds, std::vector<uint8_t>(flown.keys.size(), 1), 0.5, 4.0);
        const int n = (int)even.keys.size();
        const double t0 = even.keys.front().time, t1 = even.keys.back().time;
        for (int i = 1; i + 1 < n; i++)
            even.keys[(size_t)i].time += 0.5 * (t0 + (t1 - t0) * i / (n - 1) - even.keys[(size_t)i].time);
        check(motion_energy(speeds, 4.0) <= e0 * 1.001 &&
                  motion_energy(speeds, 4.0) < motion_energy(even, 4.0),
              "on a fitted flight, smoothing the speeds lowers the acceleration to " +
                  std::to_string(motion_energy(speeds, 4.0) / e0) + " (evening the gaps: " +
                  std::to_string(motion_energy(even, 4.0) / e0) + ")");
    }
    std::vector<FlightSample> still(30);
    for (int i = 0; i < 30; i++) still[(size_t)i].t = i / 30.0;
    check(fit_flight(still, fit, 4.0, Lens{}, p) == 0, "a flight that never moved makes no keys");
}

// ---- a GIF decoder, just enough to read back what GifWriter wrote ----

struct Gif {
    int w = 0, h = 0;
    std::vector<std::vector<uint8_t>> frames;   // RGB
    std::vector<int> delays;
    bool ok = false;
};

Gif read_gif(const std::vector<uint8_t>& b) {
    Gif g;
    size_t at = 6;
    auto u16 = [&](size_t i) { return b[i] | (b[i + 1] << 8); };
    if (b.size() < 13 || std::string(b.begin(), b.begin() + 6) != "GIF89a") return g;
    g.w = u16(6);
    g.h = u16(8);
    at = 13;
    int delay = 0;
    while (at < b.size()) {
        const uint8_t tag = b[at++];
        if (tag == 0x3B) { g.ok = true; break; }
        if (tag == 0x21) {
            const uint8_t label = b[at++];
            if (label == 0xF9) delay = u16(at + 2);
            while (b[at]) at += b[at] + 1;
            at++;
            continue;
        }
        if (tag != 0x2C) return g;
        const int fw = u16(at + 4), fh = u16(at + 6), packed = b[at + 8];
        at += 9;
        std::vector<uint8_t> pal;
        if (packed & 0x80) {
            const int n = 3 << ((packed & 7) + 1);
            pal.assign(b.begin() + (ptrdiff_t)at, b.begin() + (ptrdiff_t)(at + n));
            at += (size_t)n;
        }
        const int min = b[at++];
        std::vector<uint8_t> data;
        while (b[at]) {
            data.insert(data.end(), b.begin() + (ptrdiff_t)at + 1, b.begin() + (ptrdiff_t)(at + 1 + b[at]));
            at += b[at] + 1;
        }
        at++;
        // LZW, the reader's way round.
        const int clear = 1 << min, eoi = clear + 1;
        std::vector<std::vector<uint8_t>> table;
        auto reset = [&] {
            table.assign((size_t)eoi + 1, {});
            for (int i = 0; i < clear; i++) table[(size_t)i] = {(uint8_t)i};
        };
        reset();
        int width = min + 1, prev = -1;
        size_t bit = 0;
        std::vector<uint8_t> idx;
        for (;;) {
            if (bit + width > data.size() * 8) break;
            int code = 0;
            for (int k = 0; k < width; k++, bit++)
                code |= ((data[bit >> 3] >> (bit & 7)) & 1) << k;
            if (code == clear) { reset(); width = min + 1; prev = -1; continue; }
            if (code == eoi) break;
            std::vector<uint8_t> entry;
            if (code < (int)table.size() && !table[(size_t)code].empty()) entry = table[(size_t)code];
            else if (prev >= 0) { entry = table[(size_t)prev]; entry.push_back(table[(size_t)prev][0]); }
            else return g;
            idx.insert(idx.end(), entry.begin(), entry.end());
            if (prev >= 0 && table.size() < 4096) {
                std::vector<uint8_t> e = table[(size_t)prev];
                e.push_back(entry[0]);
                table.push_back(e);
                if ((int)table.size() == (1 << width) && width < 12) width++;
            }
            prev = code;
        }
        if ((int)idx.size() != fw * fh) return g;
        std::vector<uint8_t> rgb((size_t)fw * fh * 3);
        for (size_t i = 0; i < idx.size(); i++)
            for (int c = 0; c < 3; c++) rgb[i * 3 + c] = pal[(size_t)idx[i] * 3 + c];
        g.frames.push_back(rgb);
        g.delays.push_back(delay);
    }
    return g;
}

void test_gif() {
    const int W = 97, H = 61;
    const fs::path path = fs::temp_directory_path() / "spirula_render_test.gif";
    std::vector<std::vector<uint8_t>> frames;
    for (int f = 0; f < 3; f++) {
        std::vector<uint8_t> rgb((size_t)W * H * 3);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                uint8_t* p = &rgb[((size_t)y * W + x) * 3];
                p[0] = (uint8_t)(x * 255 / (W - 1));
                p[1] = (uint8_t)(y * 255 / (H - 1));
                p[2] = (uint8_t)((x + y + 40 * f) % 256);
            }
        frames.push_back(rgb);
    }
    {
        GifWriter w;
        bool ok = w.open(path.string(), W, H);
        for (int f = 0; f < 3 && ok; f++)
            ok = w.write(gif_encode_frame(frames[(size_t)f].data(), W, H, 256, gif_delay_cs(f, 3, 30.0)));
        ok = w.close() && ok;
        check(ok, "a GIF is written");
    }
    std::FILE* fp = std::fopen(path.string().c_str(), "rb");
    std::vector<uint8_t> bytes;
    if (fp) {
        int c;
        while ((c = std::fgetc(fp)) != EOF) bytes.push_back((uint8_t)c);
        std::fclose(fp);
    }
    fs::remove(path);
    const Gif g = read_gif(bytes);
    bool close = g.ok && g.w == W && g.h == H && g.frames.size() == 3;
    double err = 0.0;
    for (size_t f = 0; close && f < 3; f++)
        for (size_t i = 0; i < frames[f].size(); i++)
            err += std::fabs((double)g.frames[f][i] - frames[f][i]);
    err /= 3.0 * W * H * 3;
    check(close && err < 12.0, "and read back frame for frame, near the original colours");
    check(g.delays.size() == 3 && g.delays[0] + g.delays[1] + g.delays[2] >= 9 &&
              g.delays[0] + g.delays[1] + g.delays[2] <= 11,
          "at 30 frames a second, a tenth of a second for three frames");
    int kept = 0, total = 0, shortest = 100;
    for (int f = 0; f < 60; f++) {
        const int d = gif_delay_cs(f, 60, 60.0);
        if (d <= 0) continue;
        kept++;
        total += d;
        shortest = std::min(shortest, d);
    }
    check(kept < 60 && shortest >= 2 && total == 100,
          "at 60 frames a second it drops frames to keep every delay playable, and the time right");
}

}  // namespace

int main() {
    test_keys_are_hit();
    test_constant_speed();
    test_aim_and_lens();
    test_json_and_moves();
    test_dataset_lenses();
    test_spline_is_c2();
    test_closed_loop();
    test_lens_kept_on_sort();
    test_lens_glides();
    test_key_times();
    test_looks();
    test_refit_after_delete();
    test_transitions();
    test_shot_mix();
    test_auto_primitive();
    test_flight();
    test_gif();
    if (g_failures) {
        std::printf("%d FAILED\n", g_failures);
        return 1;
    }
    std::printf("PASS\n");
    return 0;
}

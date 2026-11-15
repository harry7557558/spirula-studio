// RenderProject.cpp -- see RenderProject.h.

#include "app/gui/render/RenderProject.h"

#include "data/CameraMath.h"
#include "data/Json.h"
#include "data/JsonWrite.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace gui::render {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr const char* kFormat = "spirula-render";
constexpr int kVersion = 1;

const char* const kProjectionNames[kNumProjections] = {
    "perspective", "fisheye", "equisolid", "equirectangular", "orthographic"};
const char* const kTransitionNames[kNumTransitions] = {
    "cut", "crossfade", "dip", "wipe", "iris", "zoom", "sweep", "grow", "dust",
    "spiral", "scatter", "rain", "dissolve", "ripple"};
const char* const kPointStyleNames[kNumPointStyles] = {
    "square", "circle", "gaussian", "sphere"};
const char* const kOutputNames[3] = {"photo", "video", "frames"};
const char* const kFadeNames[3] = {"none", "black", "white"};
const char* const kImageFormatNames[kNumImageFormats] = {"png", "png_alpha", "jpeg"};
const char* const kCodecNames[kNumCodecs] = {"h264", "h265", "av1", "gif", "av1_webm"};
const char* const kCurveNames[kNumCurves] = {"spline", "catmull_rom", "linear"};

template <int N>
int name_index(const char* const (&names)[N], const std::string& s, int def) {
    for (int i = 0; i < N; i++)
        if (s == names[i]) return i;
    return def;
}

double dot3(const double a[3], const double b[3]) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}
void cross3(const double a[3], const double b[3], double o[3]) {
    o[0] = a[1]*b[2] - a[2]*b[1];
    o[1] = a[2]*b[0] - a[0]*b[2];
    o[2] = a[0]*b[1] - a[1]*b[0];
}
bool normalize3(double v[3]) {
    const double n = std::sqrt(dot3(v, v));
    if (!(n > 1e-300)) return false;
    for (int k = 0; k < 3; k++) v[k] /= n;
    return true;
}

// Row-major 3x3 with the camera axes as COLUMNS (x right, y up, z back).
void quat_from_columns(const double X[3], const double Y[3], const double Z[3],
                       double q[4]) {
    spirula::Sim3 s;
    for (int r = 0; r < 3; r++) {
        s.R[r*3+0] = X[r];
        s.R[r*3+1] = Y[r];
        s.R[r*3+2] = Z[r];
    }
    s.quat(q);
}

void quat_to_matrix(const double q[4], double R[9]) {
    const double w = q[0], x = q[1], y = q[2], z = q[3];
    R[0] = 1 - 2*(y*y + z*z); R[1] = 2*(x*y - w*z);     R[2] = 2*(x*z + w*y);
    R[3] = 2*(x*y + w*z);     R[4] = 1 - 2*(x*x + z*z); R[5] = 2*(y*z - w*x);
    R[6] = 2*(x*z - w*y);     R[7] = 2*(y*z + w*x);     R[8] = 1 - 2*(x*x + y*y);
}

// ---- JSON helpers ----

void write_vec(JsonWriter& w, const char* key, const double* v, int n) {
    w.key(key).array();
    for (int i = 0; i < n; i++) w.raw(json_number_exact(v[i]));
    w.end();
}
void write_vecf(JsonWriter& w, const char* key, const float* v, int n) {
    w.key(key).array();
    for (int i = 0; i < n; i++) w.raw(json_number_exact((double)v[i]));
    w.end();
}
bool read_vec(const JsonValue& o, const char* key, double* out, int n) {
    const JsonValue* v = o.find(key);
    if (!v || !v->is_array() || (int)v->arr.size() != n) return false;
    for (int i = 0; i < n; i++) out[i] = v->arr[(size_t)i].as_double(out[i]);
    return true;
}
bool read_vecf(const JsonValue& o, const char* key, float* out, int n) {
    double tmp[16];
    for (int i = 0; i < n; i++) tmp[i] = out[i];
    if (!read_vec(o, key, tmp, n)) return false;
    for (int i = 0; i < n; i++) out[i] = (float)tmp[i];
    return true;
}
std::string get_str(const JsonValue& o, const char* key) {
    const JsonValue* v = o.find(key);
    return v ? v->as_string() : std::string();
}
bool get_bool(const JsonValue& o, const char* key, bool def) {
    const JsonValue* v = o.find(key);
    return v ? v->as_bool(def) : def;
}
double get_num(const JsonValue& o, const char* key, double def) {
    const double v = o.get_double(key, def);
    return std::isfinite(v) ? v : def;
}

void write_lens(JsonWriter& w, const Lens& l) {
    w.object();
    w.field("projection", kProjectionNames[(int)l.projection]);
    w.key("focal").raw(json_number_exact(l.focal));
    w.key("ortho_distance").raw(json_number_exact(l.ortho_distance));
    w.field("distortion", l.tier);
    write_vecf(w, "coefficients", l.dist, kLensCoeffs);
    w.end();
}
Lens read_lens(const JsonValue& o) {
    Lens l;
    l.projection = (Projection)name_index(kProjectionNames, get_str(o, "projection"), 0);
    l.focal = std::max(get_num(o, "focal", l.focal), 1e-4);
    l.ortho_distance = std::max(get_num(o, "ortho_distance", 1.0), 1e-12);
    l.tier = std::clamp((int)get_num(o, "distortion", 0), 0, 2);
    read_vecf(o, "coefficients", l.dist, kLensCoeffs);
    if (l.projection == Projection::Orthographic) {
        l.tier = 0;
        for (float& d : l.dist) d = 0.0f;
    }
    return l;
}

void write_style(JsonWriter& w, const SourceStyle& y) {
    w.object();
    w.field("points", kPointStyleNames[(int)y.point_style]);
    w.field("point_px", y.point_px);
    w.field("sphere_radius", y.sphere_radius);
    w.field("cameras", y.cameras);
    w.field("shade", y.shade);
    w.field("flat", y.flat);
    w.field("colour", y.colour);
    w.field("sh_degree", y.sh_degree);
    if (!y.primitive.empty()) w.field("primitive", y.primitive);
    w.end();
}
SourceStyle read_style(const JsonValue& o) {
    SourceStyle y;
    y.point_style = (PointStyle)name_index(kPointStyleNames, get_str(o, "points"), 1);
    y.point_px = (float)std::clamp(get_num(o, "point_px", 3.0), 0.5, 64.0);
    y.sphere_radius = (float)std::clamp(get_num(o, "sphere_radius", 0.004), 1e-6, 1.0);
    y.cameras = get_bool(o, "cameras", false);
    y.shade = get_bool(o, "shade", true);
    y.flat = get_bool(o, "flat", false);
    y.colour = get_bool(o, "colour", true);
    y.sh_degree = std::clamp((int)get_num(o, "sh_degree", -1), -1, 3);
    y.primitive = get_str(o, "primitive");
    if (y.primitive == "auto") y.primitive.clear();   // the automatic choice, spelt out
    return y;
}

}  // namespace


// ===========================================================================
// Lens
// ===========================================================================

bool Lens::operator==(const Lens& o) const {
    if (projection != o.projection || focal != o.focal || ortho_distance != o.ortho_distance || tier != o.tier)
        return false;
    for (int i = 0; i < kLensCoeffs; i++)
        if (dist[i] != o.dist[i]) return false;
    return true;
}

double lens_fov(const Lens& l) {
    const double f = std::max(l.focal, 1e-6);
    switch (l.projection) {
        case Projection::Equirect:  return 360.0;
        case Projection::Fisheye:   return 2.0 * (0.5 / f) * 180.0 / kPi;
        case Projection::Equisolid: {
            const double s = std::min(0.25 / f, 1.0);
            return 4.0 * std::asin(s) * 180.0 / kPi;
        }
        default: return 2.0 * std::atan(0.5 / f) * 180.0 / kPi;
    }
}

void lens_set_fov(Lens& l, double degrees) {
    const double half = std::max(degrees, 0.1) * 0.5 * kPi / 180.0;
    switch (l.projection) {
        case Projection::Equirect:  break;
        case Projection::Fisheye:   l.focal = 0.5 / half; break;
        case Projection::Equisolid: l.focal = 0.5 / (2.0 * std::sin(std::min(half, kPi) * 0.5)); break;
        default: l.focal = 0.5 / std::tan(std::min(half, 0.4999 * kPi)); break;
    }
}

void lens_intrinsics(const Lens& l, int w, int h, float out[4], double model_to_world_scale) {
    if (l.projection == Projection::Equirect) {
        out[0] = (float)(w / (2.0 * kPi));
        out[1] = (float)(h / kPi);
    } else {
        out[0] = out[1] = (float)(l.focal * w);
        if (l.projection == Projection::Orthographic)
            out[0] = out[1] = (float)(l.focal * w * model_to_world_scale / std::max(l.ortho_distance, 1e-12));
    }
    out[2] = 0.5f * (float)w;
    out[3] = 0.5f * (float)h;
}


bool lens_needs_ut(const Lens& l, int w, int h) {
    if (l.projection == Projection::Orthographic) return false;
    if (l.projection == Projection::Equirect) return true;
    const int tier = std::clamp(l.tier, 0, 2);
    if (tier == 0 || !camhost::has_distortion(tier, l.dist)) return false;
    float in[4];
    lens_intrinsics(l, w, h, in);
    // The lens's own coordinates across twice the frame, for a fold that
    // lands inside it.
    const double x0 = -2.0 * in[2] / in[0], x1 = 2.0 * (w - in[2]) / in[0];
    const double y0 = -2.0 * in[3] / in[1], y1 = 2.0 * (h - in[3]) / in[1];
    constexpr int n = 48;
    for (int j = 0; j <= n; j++)
        for (int i = 0; i <= n; i++) {
            const double u = x0 + (x1 - x0) * i / n, v = y0 + (y1 - y0) * j / n;
            if (camhost::valid_distortion(u, v, tier, l.dist)) continue;
            double d[2];
            camhost::distort_lens(u, v, tier, l.dist, d);
            const double px = d[0] * in[0] + in[2], py = d[1] * in[1] + in[3];
            if (px >= 0.0 && px < w && py >= 0.0 && py < h) return true;
        }
    return false;
}

std::string resolve_primitive(const std::string& want, const std::string& trained, const Lens& l,
                              int w, int h) {
    if (!want.empty() && want != "auto") return want;
    if (lens_needs_ut(l, w, h)) return "3dgut";
    return trained == "mip" ? "mip" : "3dgs";
}


// ===========================================================================
// Project
// ===========================================================================

double RenderProject::duration() const {
    const double t = keys.empty() ? 0.0 : keys.back().time;
    if (!looped()) return t;
    if (end > t + 1e-6) return end;
    const double spacing = (t - keys.front().time) / (double)(keys.size() - 1);
    return t + std::max(spacing, 0.1);
}

Lens RenderProject::lens_at(int i) const {
    if (keys.empty()) return Lens();
    const int n = (int)keys.size();
    i = std::clamp(i, 0, n - 1);
    int a = i;
    while (a > 0 && !keys[(size_t)a].own_lens) a--;
    const Lens& la = keys[(size_t)a].lens;
    if (a == i) return la;
    int b = i + 1;
    while (b < n && !keys[(size_t)b].own_lens) b++;
    // A loop glides back to the first key's lens.
    double tb = 0.0;
    if (b < n) tb = keys[(size_t)b].time;
    else if (looped()) { b = 0; tb = duration(); }
    else return la;
    const Lens& lb = keys[(size_t)b].lens;
    if (lb.projection != la.projection) return la;
    const double ta = keys[(size_t)a].time, span = tb - ta;
    const double w = span > 1e-9 ? std::clamp((keys[(size_t)i].time - ta) / span, 0.0, 1.0)
                                 : 0.0;
    Lens l = la;
    const double fa = std::log(std::max(la.focal, 1e-6)), fb = std::log(std::max(lb.focal, 1e-6));
    l.focal = std::exp(fa + (fb - fa) * w);
    const double da = std::log(std::max(la.ortho_distance, 1e-12));
    const double db = std::log(std::max(lb.ortho_distance, 1e-12));
    l.ortho_distance = std::exp(da + (db - da) * w);
    if (la.tier == lb.tier)
        for (int d = 0; d < kLensCoeffs; d++)
            l.dist[d] = (float)(la.dist[d] + (lb.dist[d] - la.dist[d]) * w);
    return l;
}

void transition_defaults(Transition t, float param[2], float colour[3], bool& camera) {
    float p0 = 0.0f, p1 = 0.0f, c[3] = {0.0f, 0.0f, 0.0f};
    switch (t) {
        case Transition::Wipe: p0 = 0.0f; p1 = 0.03f; break;       // direction, degrees; softness
        case Transition::Iris: p1 = 0.03f; break;                  // softness
        case Transition::Zoom: p0 = 0.5f; break;                   // strength
        case Transition::Sweep:                                    // down (1) or up; glow
            p1 = 0.6f;
            c[0] = 1.0f; c[1] = 0.86f; c[2] = 0.6f;
            break;
        case Transition::Dust: p1 = 0.5f; break;                   // fall, rise or blow; turbulence
        case Transition::Spiral: p0 = 1.25f; p1 = 1.0f; break;     // turns; spread
        case Transition::Scatter: p0 = 1.0f; p1 = 0.5f; break;     // distance; randomness
        case Transition::Rain: p0 = 1.2f; p1 = 0.7f; break;        // height; stagger
        case Transition::Dissolve: p0 = 0.6f; break;               // sparkle
        case Transition::Ripple: p0 = 0.25f; p1 = 0.3f; break;     // height; width
        default: break;
    }
    param[0] = p0;
    param[1] = p1;
    for (int k = 0; k < 3; k++) colour[k] = c[k];
    // Dust falls, rain drops and a spiral turns as seen; a sweep is a level
    // and a ripple runs over the ground, which the world's up has.
    camera = t == Transition::Dust || t == Transition::Spiral || t == Transition::Rain;
}

void shot_defaults(Shot& s) { transition_defaults(s.transition, s.param, s.colour, s.camera); }

void exit_defaults(ShotExit& e) { transition_defaults(e.transition, e.param, e.colour, e.camera); }

ShotMix shot_mix(const std::vector<Shot>& shots, double t, double end) {
    ShotMix m;
    const int n = (int)shots.size();
    if (!n) return m;
    int j = 0;
    for (int i = 0; i < n; i++)
        if (shots[(size_t)i].start <= t) j = i;
    const Shot& sh = shots[(size_t)j];
    // Before the first shot starts, it is already there.
    double p = 1.0;
    if (sh.transition != Transition::Cut && sh.duration > 1e-6 && t >= sh.start)
        p = std::min((t - sh.start) / sh.duration, 1.0);
    m.in = j;
    m.u_in = p;
    auto from_of = [&](int i) {
        const ShotExit& e = shots[(size_t)i].exit;
        const double dur = e.transition == Transition::Cut ? 0.0 : e.duration;
        return i + 1 < n ? shots[(size_t)i + 1].start + e.offset : end - dur + e.offset;
    };
    auto leaving = [&](int i) {
        const ShotExit& e = shots[(size_t)i].exit;
        const double from = from_of(i);
        if (e.transition == Transition::Cut || e.duration <= 1e-6) return t >= from ? 1.0 : 0.0;
        return std::clamp((t - from) / e.duration, 0.0, 1.0);
    };
    if (j > 0 && shots[(size_t)j - 1].exit.own) {
        m.own = true;
        const double u = leaving(j - 1);
        if (u < 1.0) {
            m.out = j - 1;
            m.u_out = u;
        }
    }
    // Gone early, before the next one arrives; or the last, on its way out.
    if (m.out < 0 && shots[(size_t)j].exit.own && t >= from_of(j) &&
        (j + 1 < n ? t < shots[(size_t)j + 1].start : true)) {
        m.own = true;
        m.out = j;
        m.in = -1;
        m.u_out = leaving(j);
    }
    if (!m.own && p < 1.0) {
        m.out = j - 1;
        m.u_out = p;
    }
    return m;
}

void settle_shots(RenderProject& p) {
    for (size_t i = 0; i + 1 < p.shots.size(); i++)
        if (p.shots[i].exit.transition == Transition::Dip) p.shots[i].exit.transition = Transition::Crossfade;
    auto colour_of = [](const Fade& f, float out[3]) {
        for (int k = 0; k < 3; k++) out[k] = f.colour == FadeColour::White ? 1.0f : 0.0f;
    };
    const bool have = p.fade_in.colour != FadeColour::None || p.fade_out.colour != FadeColour::None;
    if (have && p.shots.empty()) p.shots.push_back(Shot{});
    if (p.fade_in.colour != FadeColour::None && p.shots[0].transition == Transition::Cut) {
        Shot& s = p.shots[0];
        s.transition = Transition::Dip;
        shot_defaults(s);
        colour_of(p.fade_in, s.colour);
        s.duration = std::max(p.fade_in.seconds, 0.05);
        p.fade_in.colour = FadeColour::None;
    }
    if (p.fade_out.colour != FadeColour::None && !p.shots.back().exit.own) {
        ShotExit& e = p.shots.back().exit;
        e.own = true;
        e.transition = Transition::Dip;
        exit_defaults(e);
        colour_of(p.fade_out, e.colour);
        e.duration = std::max(p.fade_out.seconds, 0.05);
        e.offset = 0.0;
        p.fade_out.colour = FadeColour::None;
    }
}

bool SourceStyle::operator==(const SourceStyle& o) const {
    return point_style == o.point_style && point_px == o.point_px &&
           sphere_radius == o.sphere_radius && cameras == o.cameras && shade == o.shade &&
           flat == o.flat && colour == o.colour && sh_degree == o.sh_degree &&
           primitive == o.primitive;
}

void RenderProject::look_at(int source, double t, const std::vector<double>& key_times,
                            SourceStyle& from, SourceStyle& to, float& mix) const {
    mix = 0.0f;
    if (source < 0 || source >= (int)sources.size()) { from = to = SourceStyle(); return; }
    from = to = sources[(size_t)source].style;
    const int n = std::min((int)keys.size(), (int)key_times.size());
    if (n == 0) return;
    auto look = [&](int k) -> const SourceStyle* {
        for (const KeyLook& l : keys[(size_t)k].looks)
            if (l.source == source) return &l.style;
        return nullptr;
    };
    // The model's own style is the look at the first key unless it sets one.
    double ta = key_times[0], tb = 0.0;
    const SourceStyle* a = look(0) ? look(0) : &sources[(size_t)source].style;
    const SourceStyle* b = nullptr;
    for (int k = 1; k < n; k++) {
        const SourceStyle* l = look(k);
        if (!l) continue;
        if (key_times[(size_t)k] <= t) { a = l; ta = key_times[(size_t)k]; }
        else { b = l; tb = key_times[(size_t)k]; break; }
    }
    from = *a;
    if (!b || t <= ta) { to = from; return; }
    const double w = std::clamp((t - ta) / std::max(tb - ta, 1e-9), 0.0, 1.0);
    // Sizes glide; what can only be one thing or the other is crossfaded.
    from.point_px = (float)(a->point_px + (b->point_px - a->point_px) * w);
    from.sphere_radius = (float)(a->sphere_radius + (b->sphere_radius - a->sphere_radius) * w);
    to = *b;
    to.point_px = from.point_px;
    to.sphere_radius = from.sphere_radius;
    if (to == from) return;
    mix = (float)w;
}

void RenderProject::sort_keys() {
    if (keys.empty()) return;
    std::vector<std::pair<Keyframe, Lens>> tagged;
    tagged.reserve(keys.size());
    for (size_t i = 0; i < keys.size(); i++) tagged.push_back({keys[i], lens_at((int)i)});
    std::stable_sort(tagged.begin(), tagged.end(), [](const auto& a, const auto& b) {
        return a.first.time < b.first.time;
    });
    for (size_t i = 0; i < keys.size(); i++) keys[i] = tagged[i].first;
    if (!keys[0].own_lens) keys[0].lens = tagged[0].second;
    keys[0].own_lens = true;
}

void aim_rotation(const double pos[3], const double target[3],
                  const double up[3], double roll, double out[4]) {
    double f[3] = {target[0] - pos[0], target[1] - pos[1], target[2] - pos[2]};
    if (!normalize3(f)) { out[0] = 1; out[1] = out[2] = out[3] = 0; return; }
    double u[3] = {up[0], up[1], up[2]};
    if (!normalize3(u)) { u[0] = 0; u[1] = 0; u[2] = 1; }
    double x[3];
    cross3(f, u, x);
    // Straight up or down has no "up" left in the picture; any sideways axis
    // will do, and +Y of the frame is the one a plan view puts on top.
    if (!normalize3(x)) {
        const double alt[3] = {0, 1, 0};
        cross3(f, alt, x);
        if (!normalize3(x)) { x[0] = 1; x[1] = 0; x[2] = 0; }
    }
    double y[3];
    cross3(x, f, y);
    const double a = roll * kPi / 180.0, c = std::cos(a), s = std::sin(a);
    double xr[3], yr[3];
    for (int k = 0; k < 3; k++) {
        xr[k] = c * x[k] + s * y[k];
        yr[k] = -s * x[k] + c * y[k];
    }
    const double z[3] = {-f[0], -f[1], -f[2]};
    quat_from_columns(xr, yr, z, out);
}

double roll_of(const double rot[4], const double pos[3], const double target[3],
               const double up[3]) {
    double q0[4];
    aim_rotation(pos, target, up, 0.0, q0);
    double R0[9], R[9];
    quat_to_matrix(q0, R0);
    quat_to_matrix(rot, R);
    // The camera's own x against the unrolled x and y, both in the image plane.
    const double x[3] = {R[0], R[3], R[6]};
    const double x0[3] = {R0[0], R0[3], R0[6]}, y0[3] = {R0[1], R0[4], R0[7]};
    return std::atan2(dot3(x, y0), dot3(x, x0)) * 180.0 / kPi;
}

void update_aim(Keyframe& k, const double up[3]) {
    if (k.aim) aim_rotation(k.pos, k.target, up, k.roll, k.rot);
}

void transform_project(RenderProject& p, const spirula::Sim3& s) {
    spirula::Sim3 rot;
    for (int i = 0; i < 9; i++) rot.R[i] = s.R[i];
    double rq[4];
    rot.quat(rq);
    for (Keyframe& k : p.keys) {
        if (k.lens.projection == Projection::Orthographic) k.lens.ortho_distance *= s.s;
        double q[3];
        s.apply(k.pos, q);
        for (int i = 0; i < 3; i++) k.pos[i] = q[i];
        s.apply(k.target, q);
        for (int i = 0; i < 3; i++) k.target[i] = q[i];
        // rq * k.rot, both (w, x, y, z).
        const double a0 = rq[0], a1 = rq[1], a2 = rq[2], a3 = rq[3];
        const double b0 = k.rot[0], b1 = k.rot[1], b2 = k.rot[2], b3 = k.rot[3];
        k.rot[0] = a0*b0 - a1*b1 - a2*b2 - a3*b3;
        k.rot[1] = a0*b1 + a1*b0 + a2*b3 - a3*b2;
        k.rot[2] = a0*b2 - a1*b3 + a2*b0 + a3*b1;
        k.rot[3] = a0*b3 + a1*b2 - a2*b1 + a3*b0;
    }
    double u[3];
    s.rotate(p.up, u);
    for (int i = 0; i < 3; i++) p.up[i] = u[i];
}


void fit_output_path(Output& o) {
    if (o.path.empty()) return;
    fs::path p = fs::u8path(o.path);
    const std::string ext = p.extension().string();
    if (o.kind == OutputKind::Frames) {
        if (!ext.empty()) p.replace_extension();
    } else if (o.kind == OutputKind::Video && o.codec == Codec::Gif) {
        if (ext != ".gif") p.replace_extension(".gif");
    } else if (o.kind == OutputKind::Video && o.codec == Codec::Av1Webm) {
        if (ext != ".webm") p.replace_extension(".webm");
    } else if (o.kind == OutputKind::Video) {
        if (ext != ".mp4" && ext != ".h264" && ext != ".h265" && ext != ".obu")
            p.replace_extension(".mp4");
    } else {
        const char* want = o.format == ImageFormat::Jpeg ? ".jpg" : ".png";
        if (ext != want && !(o.format == ImageFormat::Jpeg && ext == ".jpeg"))
            p.replace_extension(want);
    }
    o.path = p.string();
}


// ===========================================================================
// JSON
// ===========================================================================

std::string project_to_json(const RenderProject& p) {
    JsonWriter w;
    w.object();
    w.field("format", kFormat);
    w.field("version", kVersion);
    write_vec(w, "up", p.up, 3);
    write_vecf(w, "background", p.background, 3);
    if (p.end > 0.0) w.key("end").raw(json_number_exact(p.end));
    if (!p.placement.is_identity(1e-12)) {
        double a[12];
        p.placement.to_3x4(a);
        write_vec(w, "placement", a, 12);
    }

    w.key("output").object();
    w.field("kind", kOutputNames[(int)p.output.kind]);
    w.field("width", p.output.width);
    w.field("height", p.output.height);
    w.key("fps").raw(json_number_exact(p.output.fps));
    w.field("image_format", kImageFormatNames[(int)p.output.format]);
    w.field("jpeg_quality", p.output.jpeg_quality);
    w.field("codec", kCodecNames[(int)p.output.codec]);
    w.field("quality", p.output.quality);
    if (!p.output.path.empty()) w.field("path", p.output.path);
    w.end();

    w.key("motion").object();
    w.field("curve", kCurveNames[(int)p.motion.curve]);
    w.field("ease", p.motion.ease);
    w.field("constant_speed", p.motion.constant_speed);
    w.field("loop", p.motion.loop);
    w.key("tension").raw(json_number_exact(p.motion.tension));
    w.end();

    auto fade = [&](const char* key, const Fade& f) {
        w.key(key).object();
        w.field("colour", kFadeNames[(int)f.colour]);
        w.key("seconds").raw(json_number_exact(f.seconds));
        w.end();
    };
    fade("fade_in", p.fade_in);
    fade("fade_out", p.fade_out);

    w.key("sources").array();
    for (const Source& s : p.sources) {
        w.object();
        w.field("path", s.path);
        w.key("style");
        write_style(w, s.style);
        w.end();
    }
    w.end();

    w.key("shots").array();
    for (const Shot& s : p.shots) {
        w.object();
        w.key("start").raw(json_number_exact(s.start));
        w.field("source", s.source);
        w.field("transition", kTransitionNames[(int)s.transition]);
        w.key("duration").raw(json_number_exact(s.duration));
        write_vecf(w, "params", s.param, 2);
        write_vecf(w, "colour", s.colour, 3);
        w.field("camera", s.camera);
        if (s.exit.own) {
            const ShotExit& e = s.exit;
            w.key("exit").object();
            w.field("transition", kTransitionNames[(int)e.transition]);
            w.key("duration").raw(json_number_exact(e.duration));
            w.key("offset").raw(json_number_exact(e.offset));
            write_vecf(w, "params", e.param, 2);
            write_vecf(w, "colour", e.colour, 3);
            w.field("camera", e.camera);
            w.end();
        }
        w.end();
    }
    w.end();

    w.key("keyframes").array();
    for (size_t i = 0; i < p.keys.size(); i++) {
        const Keyframe& k = p.keys[i];
        w.object();
        w.key("time").raw(json_number_exact(k.time));
        write_vec(w, "position", k.pos, 3);
        write_vec(w, "rotation", k.rot, 4);
        if (k.aim) {
            write_vec(w, "look_at", k.target, 3);
            w.key("roll").raw(json_number_exact(k.roll));
        }
        if (k.own_lens || i == 0) {
            w.key("lens");
            write_lens(w, k.lens);
        }
        if (k.hold) w.field("hold", true);
        if (!k.looks.empty()) {
            w.key("looks").array();
            for (const KeyLook& l : k.looks) {
                w.object();
                w.field("source", l.source);
                w.key("style");
                write_style(w, l.style);
                w.end();
            }
            w.end();
        }
        w.end();
    }
    w.end();
    w.end();
    return w.str();
}

RenderProject project_from_json(const std::string& text) {
    const JsonValue root = json_parse(text);
    if (!root.is_object() || get_str(root, "format") != kFormat)
        throw std::runtime_error("not a render project");
    RenderProject p;
    read_vec(root, "up", p.up, 3);
    read_vecf(root, "background", p.background, 3);
    p.end = std::max(0.0, get_num(root, "end", 0.0));
    {
        double a[12] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
        if (read_vec(root, "placement", a, 12)) p.placement = spirula::Sim3::from_3x4(a);
    }

    if (const JsonValue* o = root.find("output"); o && o->is_object()) {
        Output& out = p.output;
        out.kind = (OutputKind)name_index(kOutputNames, get_str(*o, "kind"), 1);
        out.width = std::clamp((int)get_num(*o, "width", out.width), 16, 16384);
        out.height = std::clamp((int)get_num(*o, "height", out.height), 16, 16384);
        out.fps = std::clamp(get_num(*o, "fps", out.fps), 1.0, 240.0);
        out.format = (ImageFormat)name_index(kImageFormatNames, get_str(*o, "image_format"), 0);
        // Written before PNG with transparency was a format of its own.
        if (out.format == ImageFormat::Png && get_bool(*o, "transparent", false))
            out.format = ImageFormat::PngAlpha;
        out.jpeg_quality = std::clamp((int)get_num(*o, "jpeg_quality", 95), 10, 100);
        out.codec = (Codec)name_index(kCodecNames, get_str(*o, "codec"), 0);
        out.quality = std::clamp((int)get_num(*o, "quality", 1), 0, 2);
        out.path = get_str(*o, "path");
    }
    if (const JsonValue* o = root.find("motion"); o && o->is_object()) {
        // A file from before "curve" meant Catmull-Rom, or lines when not smooth.
        const std::string curve = get_str(*o, "curve");
        if (!curve.empty())
            p.motion.curve = (Curve)name_index(kCurveNames, curve, 0);
        else
            p.motion.curve = get_bool(*o, "smooth", true) ? Curve::CatmullRom : Curve::Linear;
        p.motion.ease = get_bool(*o, "ease", true);
        p.motion.constant_speed = get_bool(*o, "constant_speed", false);
        p.motion.loop = get_bool(*o, "loop", false);
        p.motion.tension = std::clamp(get_num(*o, "tension", 0.0), 0.0, 1.0);
    }
    auto fade = [&](const char* key, Fade& f) {
        const JsonValue* o = root.find(key);
        if (!o || !o->is_object()) return;
        f.colour = (FadeColour)name_index(kFadeNames, get_str(*o, "colour"), 0);
        f.seconds = std::clamp(get_num(*o, "seconds", 1.0), 0.0, 3600.0);
    };
    fade("fade_in", p.fade_in);
    fade("fade_out", p.fade_out);

    if (const JsonValue* a = root.find("sources"); a && a->is_array()) {
        for (const JsonValue& o : a->arr) {
            Source s;
            s.path = get_str(o, "path");
            if (const JsonValue* st = o.find("style"); st && st->is_object())
                s.style = read_style(*st);
            p.sources.push_back(std::move(s));
        }
    }
    if (const JsonValue* a = root.find("shots"); a && a->is_array()) {
        for (const JsonValue& o : a->arr) {
            Shot s;
            s.start = std::max(0.0, get_num(o, "start", 0.0));
            s.source = std::max(-1, (int)get_num(o, "source", 0));
            // Files from before the dips and wipes each became one with a
            // setting: their colour and direction carry over.
            const std::string tr = get_str(o, "transition");
            struct Old { const char* name; Transition t; float p0; float colour; };
            static const Old kOld[] = {
                {"dip_black", Transition::Dip, 0.0f, 0.0f}, {"dip_white", Transition::Dip, 0.0f, 1.0f},
                {"wipe_left", Transition::Wipe, 0.0f, 0.0f}, {"wipe_right", Transition::Wipe, 180.0f, 0.0f},
                {"wipe_up", Transition::Wipe, 270.0f, 0.0f}, {"wipe_down", Transition::Wipe, 90.0f, 0.0f}};
            s.transition = (Transition)name_index(kTransitionNames, tr, 0);
            for (const Old& od : kOld)
                if (tr == od.name) s.transition = od.t;
            shot_defaults(s);
            for (const Old& od : kOld)
                if (tr == od.name) {
                    if (od.t == Transition::Wipe) s.param[0] = od.p0;
                    for (float& c : s.colour) c = od.t == Transition::Dip ? od.colour : c;
                }
            read_vecf(o, "params", s.param, 2);
            read_vecf(o, "colour", s.colour, 3);
            s.camera = get_bool(o, "camera", s.camera);
            s.duration = std::clamp(get_num(o, "duration", 1.0), 0.0, 3600.0);
            if (const JsonValue* x = o.find("exit"); x && x->is_object()) {
                ShotExit& e = s.exit;
                e.own = true;
                e.transition = (Transition)name_index(kTransitionNames, get_str(*x, "transition"),
                                                      (int)Transition::Crossfade);
                exit_defaults(e);
                read_vecf(*x, "params", e.param, 2);
                read_vecf(*x, "colour", e.colour, 3);
                e.camera = get_bool(*x, "camera", e.camera);
                e.duration = std::clamp(get_num(*x, "duration", 1.0), 0.0, 3600.0);
                e.offset = std::clamp(get_num(*x, "offset", 0.0), -3600.0, 3600.0);
            }
            p.shots.push_back(s);
        }
        std::stable_sort(p.shots.begin(), p.shots.end(),
                         [](const Shot& a, const Shot& b) { return a.start < b.start; });
    }
    settle_shots(p);
    if (const JsonValue* a = root.find("keyframes"); a && a->is_array()) {
        for (const JsonValue& o : a->arr) {
            Keyframe k;
            k.time = std::max(0.0, get_num(o, "time", 0.0));
            read_vec(o, "position", k.pos, 3);
            if (read_vec(o, "rotation", k.rot, 4)) {
                const double n = std::sqrt(k.rot[0]*k.rot[0] + k.rot[1]*k.rot[1] +
                                           k.rot[2]*k.rot[2] + k.rot[3]*k.rot[3]);
                if (n > 1e-12) for (double& v : k.rot) v /= n;
                else { k.rot[0] = 1; k.rot[1] = k.rot[2] = k.rot[3] = 0; }
            }
            k.aim = read_vec(o, "look_at", k.target, 3);
            k.roll = get_num(o, "roll", 0.0);
            if (const JsonValue* l = o.find("lens"); l && l->is_object()) {
                k.own_lens = true;
                k.lens = read_lens(*l);
            }
            k.hold = get_bool(o, "hold", false);
            if (const JsonValue* ls = o.find("looks"); ls && ls->is_array()) {
                for (const JsonValue& lo : ls->arr) {
                    const JsonValue* st = lo.find("style");
                    const int src = (int)get_num(lo, "source", -1);
                    if (src < 0 || src >= (int)p.sources.size() || !st || !st->is_object()) continue;
                    k.looks.push_back({src, read_style(*st)});
                }
            }
            update_aim(k, p.up);
            p.keys.push_back(k);
        }
    }
    p.sort_keys();
    return p;
}

RenderProject load_project(const std::string& path) {
    std::ifstream f(fs::u8path(path), std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + path);
    std::string text((std::istreambuf_iterator<char>(f)),
                     std::istreambuf_iterator<char>());
    return project_from_json(text);
}

void save_project(const RenderProject& p, const std::string& path) {
    const fs::path target = fs::u8path(path);
    std::error_code ec;
    if (target.has_parent_path()) fs::create_directories(target.parent_path(), ec);
    // Written beside and renamed over, so a full disk cannot leave half a file
    // where the last good project was.
    const fs::path tmp = target.string() + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) throw std::runtime_error("cannot write " + path);
        f << project_to_json(p);
        if (!f) throw std::runtime_error("cannot write " + path);
    }
    fs::rename(tmp, target, ec);
    if (ec) {
        fs::remove(target, ec);
        fs::rename(tmp, target, ec);
        if (ec) throw std::runtime_error("cannot write " + path + ": " + ec.message());
    }
}

std::string default_project_dir(const std::string& model_path) {
    std::error_code ec;
    fs::path p = fs::u8path(model_path);
    if (p.empty()) return {};
    // Projects already kept with this very file win: a model saved moved has
    // its moved copies there.
    if (fs::is_regular_file(p, ec) && fs::is_directory(p.parent_path() / "renders", ec))
        return (p.parent_path() / "renders").string();
    // A reconstruction: <dataset>/sparse/0, or the dataset folder itself.
    if (fs::is_directory(p, ec)) {
        if (p.parent_path().filename() == "sparse") return (p.parent_path().parent_path() / "renders").string();
        if (p.filename() == "sparse") return (p.parent_path() / "renders").string();
        return (p / "renders").string();
    }
    // A model from a run: the run's config.json names the dataset it read.
    for (fs::path dir = p.parent_path(); !dir.empty(); dir = dir.parent_path()) {
        const fs::path cfg = dir / "config.json";
        if (fs::is_regular_file(cfg, ec)) {
            try {
                const JsonValue v = json_parse_file(cfg.string());
                const JsonValue* d = v.find("data");
                if (d && !d->as_string().empty()) {
                    fs::path data = fs::u8path(d->as_string());
                    if (data.is_relative()) data = dir / data;
                    if (data.filename() == "images") data = data.parent_path();
                    if (fs::is_directory(data, ec)) return (data / "renders").string();
                }
            } catch (const std::exception&) {
            }
            break;
        }
        if (dir == p.parent_path().parent_path()) break;
    }
    return (p.parent_path() / "renders").string();
}

}  // namespace gui::render

namespace gui::render {

int copy_moved_projects(const std::string& from_model, const std::string& to_model,
                        const spirula::Sim3& move, std::string& dir) {
    if (move.is_identity(1e-9)) return 0;
    std::error_code ec;
    const fs::path src = fs::u8path(default_project_dir(from_model));
    fs::path to = fs::u8path(to_model);
    if (!fs::is_directory(to, ec)) to = to.parent_path();
    const fs::path dst = to / "renders";
    if (!fs::is_directory(src, ec)) return 0;
    int n = 0;
    for (const auto& e : fs::directory_iterator(src, ec)) {
        if (e.path().extension() != ".json") continue;
        try {
            RenderProject p = load_project(e.path().string());
            // Its poses are in the placement it was laid out against; the new
            // file's coordinates are the original's moved by `move`.
            transform_project(p, move * p.placement.inverse());
            p.placement = spirula::Sim3();
            fs::path out = dst / e.path().filename();
            // Never over an original: the same folder gets a new name.
            if (fs::equivalent(src, dst, ec))
                out = dst / (e.path().stem().string() + "_moved.json");
            save_project(p, out.string());
            n++;
        } catch (const std::exception&) {
        }
    }
    if (n) dir = dst.string();
    return n;
}

}  // namespace gui::render

// FrameRenderer.cpp -- see FrameRenderer.h.

#include "app/gui/render/FrameRenderer.h"

#include "app/gui/render/TransitionFx.h"

#include "app/gui/GlLoader.h"
#include "checkpoint/SplatPly.h"
#include "engine/Engine.h"

#include <cstddef>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace gui::render {

namespace {

constexpr float kDeadOpacity = -30.0f;
const char* const kModelNames[4] = {"PINHOLE", "FISHEYE", "EQUISOLID",
                                    "EQUIRECTANGULAR"};
const char* const kTierNames[3] = {"NONE", "OPENCV", "THIN_PRISM"};

TorchTensorView tv(std::vector<float>& v, std::vector<int64_t> shape) {
    return {(uint64_t)(uintptr_t)v.data(), (uint32_t)sizeof(float), std::move(shape)};
}

float logit(float p) {
    p = std::clamp(p, 1e-6f, 1.0f - 1e-6f);
    return std::log(p / (1.0f - p));
}
float sigmoid(float x) { return 1.0f / (1.0f + std::exp(-x)); }

// The camera in a model's normalized frame, row-major 3x4, OpenGL axes.
void camera_in(const CameraState& c, const spirula::Sim3& norm_to_world,
               float out[12]) {
    const spirula::Sim3 inv = norm_to_world.inverse();
    double R[9], p[3];
    quat_to_matrix3(c.rot, R);
    inv.apply(c.pos, p);
    for (int r = 0; r < 3; r++) {
        for (int k = 0; k < 3; k++) {
            double v = 0.0;
            for (int m = 0; m < 3; m++) v += inv.R[r*3+m] * R[m*3+k];
            out[r*4+k] = (float)v;
        }
        out[r*4+3] = (float)p[r];
    }
}

// The world plane up.P = level, in a frame whose points reach the world
// through `to_world`: n.X = d, n in world units per frame unit.
void plane_in(const spirula::Sim3& to_world, const double up[3], double level,
              double n[3], double& d) {
    for (int k = 0; k < 3; k++)
        n[k] = to_world.s * (to_world.R[0*3+k]*up[0] + to_world.R[1*3+k]*up[1] +
                             to_world.R[2*3+k]*up[2]);
    d = level - (up[0]*to_world.t[0] + up[1]*to_world.t[1] + up[2]*to_world.t[2]);
}

const char* kCompVert = R"(#version 150
out vec2 v_uv;
void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    v_uv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";

// Premultiplied throughout. v_uv is GL's: (0,0) at the bottom left.
const char* kCompFrag = R"(#version 150
in vec2 v_uv;
uniform sampler2D u_a;
uniform sampler2D u_b;
uniform int u_has_a;
uniform int u_has_b;
uniform int u_flip_a;
uniform int u_flip_b;
uniform int u_mode;
uniform int u_mask;
uniform float u_mix;
uniform vec2 u_dir;
uniform float u_aspect;
uniform vec4 u_bg;
uniform vec4 u_tint0;
uniform vec4 u_tint1;
uniform float u_soft;
uniform float u_zoom;
// Mode 3, per layer: opacity, mask, its progress, its softness; the wipe's
// direction, the zoom and its blur; and whether the mask keeps the outside.
uniform vec4 u_la0;
uniform vec4 u_lb0;
uniform int u_lo0;
uniform vec4 u_la1;
uniform vec4 u_lb1;
uniform int u_lo1;
out vec4 frag;
vec4 fetch(sampler2D t, int flip, vec2 uv) {
    if (flip == 1) uv.y = 1.0 - uv.y;
    return texture(t, uv);
}
vec4 over(vec4 top, vec4 under) { return top + (1.0 - top.a) * under; }
// Magnified `scale` times about the middle and smeared outward by `blur`.
vec4 zoomed(sampler2D t, int flip, vec2 uv, float scale, float blur) {
    vec4 acc = vec4(0.0);
    for (int i = 0; i < 12; i++) {
        vec2 p = 0.5 + (uv - 0.5) / (scale * (1.0 + blur * float(i) / 11.0));
        if (all(greaterThanEqual(p, vec2(0.0))) && all(lessThanEqual(p, vec2(1.0))))
            acc += fetch(t, flip, p);
    }
    return acc / 12.0;
}
vec4 tint(vec4 c, vec4 t) {
    return t.a > 0.0 ? mix(c, vec4(t.rgb, 1.0), t.a) : c;
}
// How much of the picture a wipe (1) or an iris (2) at `m` lets through.
float mask_at(int kind, float m, float soft, vec2 dir) {
    vec2 img = vec2(v_uv.x, 1.0 - v_uv.y);
    float e = max(soft, 0.001);
    float s;
    if (kind == 1) {
        s = dot(img - 0.5, dir) + 0.5;
    } else {
        vec2 d = (img - 0.5) * vec2(u_aspect, 1.0);
        s = length(d) / (0.5 * sqrt(u_aspect * u_aspect + 1.0));
    }
    return 1.0 - smoothstep(m - e, m + e, s);
}
vec4 layer(sampler2D t, int has, int flip, vec4 la, vec4 lb, int keep_out) {
    if (has == 0 || la.x <= 0.0) return vec4(0.0);
    vec4 c = lb.z != 1.0 || lb.w > 0.0 ? zoomed(t, flip, v_uv, lb.z, lb.w)
                                       : fetch(t, flip, v_uv);
    int kind = int(la.y + 0.5);
    float m = kind == 0 ? 1.0 : mask_at(kind, la.z, la.w, lb.xy);
    if (kind != 0 && keep_out == 1) m = 1.0 - m;
    return c * (m * la.x);
}
void main() {
    vec4 a = u_has_a == 1 ? fetch(u_a, u_flip_a, v_uv) : vec4(0.0);
    vec4 b = u_has_b == 1 ? fetch(u_b, u_flip_b, v_uv) : vec4(0.0);
    vec4 c;
    if (u_mode == 1) {
        c = over(over(b, a), u_bg);
    } else if (u_mode == 3) {
        c = over(layer(u_b, u_has_b, u_flip_b, u_la1, u_lb1, u_lo1),
                 over(layer(u_a, u_has_a, u_flip_a, u_la0, u_lb0, u_lo0), u_bg));
    } else if (u_mode == 2) {
        // The old picture rushes past the camera, the new one arrives from far.
        float m = u_mix;
        float za = 1.0 + 2.0 * u_zoom * m * m;
        float zb = 1.0 / (1.0 + 2.0 * u_zoom * (1.0 - m) * (1.0 - m));
        vec4 ca = u_has_a == 1 ? zoomed(u_a, u_flip_a, v_uv, za, u_zoom * m) : vec4(0.0);
        vec4 cb = u_has_b == 1 ? zoomed(u_b, u_flip_b, v_uv, zb, u_zoom * (1.0 - m)) : vec4(0.0);
        c = mix(over(ca, u_bg), over(cb, u_bg), smoothstep(0.3, 0.7, m));
    } else {
        float m = u_mix;
        vec2 img = vec2(v_uv.x, 1.0 - v_uv.y);
        float e = max(u_soft, 0.001);
        if (u_mask == 1) {
            // Along the wipe: 0 where it starts, 1 where it ends.
            float s = dot(img - 0.5, u_dir) + 0.5;
            m = 1.0 - smoothstep(u_mix - e, u_mix + e, s);
        } else if (u_mask == 2) {
            vec2 d = (img - 0.5) * vec2(u_aspect, 1.0);
            float r = length(d) / (0.5 * sqrt(u_aspect * u_aspect + 1.0));
            m = 1.0 - smoothstep(u_mix - e, u_mix + e, r);
        }
        c = mix(over(a, u_bg), over(b, u_bg), m);
    }
    frag = tint(tint(c, u_tint0), u_tint1);
}
)";

// One texture, or two mixed; into a target the right way up for GL.
const char* kBlendFrag = R"(#version 150
in vec2 v_uv;
uniform sampler2D u_t0;
uniform sampler2D u_t1;
uniform int u_flip0;
uniform int u_flip1;
uniform int u_has1;
uniform float u_w;
out vec4 frag;
vec4 fetch(sampler2D t, int flip, vec2 uv) {
    if (flip == 1) uv.y = 1.0 - uv.y;
    return texture(t, uv);
}
void main() {
    vec4 a = fetch(u_t0, u_flip0, v_uv);
    vec4 b = u_has1 == 1 ? fetch(u_t1, u_flip1, v_uv) : a;
    frag = mix(a, b, u_w);
}
)";

// A transition's frame carried into one whose points reach the world
// through `to_world`: a similarity, so the axes only turn and every length
// scales alike.
FxGeo fx_geo_in(const LayerFx& fx, const spirula::Sim3& to_world) {
    FxGeo g = fx.q;
    const spirula::Sim3 inv = to_world.inverse();
    double c[3], ax[3][3];
    inv.apply(fx.centre, c);
    inv.rotate(fx.up, ax[0]);
    inv.rotate(fx.e1, ax[1]);
    inv.rotate(fx.e2, ax[2]);
    float* out[3] = {g.up, g.e1, g.e2};
    for (int a = 0; a < 3; a++) {
        const double n = std::sqrt(ax[a][0]*ax[a][0] + ax[a][1]*ax[a][1] + ax[a][2]*ax[a][2]);
        for (int k = 0; k < 3; k++) out[a][k] = (float)(ax[a][k] / std::max(n, 1e-12));
    }
    const float k = (float)(1.0 / std::max(to_world.s, 1e-12));
    for (int i = 0; i < 3; i++) g.c[i] = (float)c[i];
    g.radius = (float)fx.radius * k;
    for (int i = 0; i < kFxQuantiles; i++) {
        g.qh[i] *= k;
        g.qa[i] *= k;
        g.qr[i] *= k;
    }
    return g;
}

double now_s() {
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

GLuint compile_shader(GLenum type, const char* src) {
    GLuint sh = glx::CreateShader(type);
    glx::ShaderSource(sh, 1, &src, nullptr);
    glx::CompileShader(sh);
    GLint ok = 0;
    glx::GetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glx::GetShaderInfoLog(sh, sizeof log, nullptr, log);
        std::fprintf(stderr, "[render] shader error: %s\n", log);
        glx::DeleteShader(sh);
        return 0;
    }
    return sh;
}

}  // namespace


// The splats of one model host-side, for the effects that rewrite them per
// frame. The worker thread that renders the model is the only one to touch
// the scratch arrays.
struct FrameRenderer::SplatHost {
    int64_t n = 0;
    std::vector<float> means, scales, opacities;
    std::vector<float> op, sc, mn;      // what one frame uploads
    std::vector<float> base_op;         // what is put back after it
};

struct FrameRenderer::Slot {
    SourceView view;
    std::unique_ptr<RenderWorker> worker;
    std::unique_ptr<PreviewRenderer> gl;
    bool gl_failed = false;
    std::shared_ptr<SplatHost> host;
    std::thread loader;
    std::atomic<int> load_state{0};     // 0 not asked, 1 reading, 2 ready, 3 failed
    std::shared_ptr<const std::vector<uint8_t>> masked_for;
    // Its elements thinned, world frame, for the transitions: as of the
    // placement `pts_at`, with their SceneCore; and the last quantiles asked.
    std::vector<double> pts;
    spirula::Sim3 pts_at;
    bool pts_valid = false;
    SceneCore core;
    double q_key[21] = {};
    FxGeo q;
    bool q_valid = false;

    ~Slot() {
        if (loader.joinable()) loader.join();
        if (worker) worker->stop();
    }
};

FrameRenderer::FrameRenderer() = default;

FrameRenderer::~FrameRenderer() {
    for (auto& s : _slots)
        if (s->worker) s->worker->stop();
}

void FrameRenderer::destroy_gl() {
    for (auto& s : _slots)
        if (s->gl) s->gl->destroy_gl();
    for (unsigned& t : _tex_pool)
        if (t) { GLuint g = t; glDeleteTextures(1, &g); t = 0; }
    _pool_w = _pool_h = 0;
    if (_blend_fbo) { GLuint f = _blend_fbo; glx::DeleteFramebuffers(1, &f); _blend_fbo = 0; }
    if (_blend_prog) { glx::DeleteProgram(_blend_prog); _blend_prog = 0; }
    if (_fbo) { GLuint f = _fbo; glx::DeleteFramebuffers(1, &f); _fbo = 0; }
    if (_out_tex) { GLuint t = _out_tex; glDeleteTextures(1, &t); _out_tex = 0; }
    if (_vao) { GLuint v = _vao; glx::DeleteVertexArrays(1, &v); _vao = 0; }
    if (_prog) { glx::DeleteProgram(_prog); _prog = 0; }
    _out_w = _out_h = 0;
}

void FrameRenderer::set_sources(const std::vector<SourceView>& sources) {
    const bool same_count = sources.size() == _slots.size();
    bool rebuild = !same_count;
    for (size_t i = 0; same_count && i < sources.size(); i++)
        if (sources[i].key != _slots[i]->view.key) rebuild = true;
    if (!rebuild) {
        for (size_t i = 0; i < sources.size(); i++) {
            Slot& s = *_slots[i];
            if (s.view.alive != sources[i].alive) s.masked_for.reset();
            s.view = sources[i];
        }
        return;
    }
    _stage = Stage::Idle;
    _passes.clear();
    for (auto& s : _slots)
        if (s->gl) s->gl->destroy_gl();
    _slots.clear();
    for (const SourceView& v : sources) {
        auto s = std::make_unique<Slot>();
        s->view = v;
        if (v.kind == SourceView::Splats && v.hooks.engine_mutex) {
            s->worker = std::make_unique<RenderWorker>();
            s->worker->start(v.cfg, v.hooks);
        }
        _slots.push_back(std::move(s));
    }
}

bool FrameRenderer::effects_ready(int source) {
    if (source < 0 || source >= (int)_slots.size()) return true;
    Slot& s = *_slots[(size_t)source];
    if (s.view.kind != SourceView::Splats) return true;
    const int st = s.load_state.load();
    if (st == 2 || st == 3) {
        if (s.loader.joinable()) s.loader.join();
        return true;
    }
    if (st == 0 && !s.view.file.empty()) {
        s.load_state = 1;
        const std::string file = s.view.file;
        Slot* sp = &s;
        s.loader = std::thread([sp, file] {
            try {
                spirula::SplatCloud c = spirula::read_splat_ply(file, false);
                auto h = std::make_shared<SplatHost>();
                h->n = c.num;
                h->means = std::move(c.means);
                h->scales = std::move(c.scales);
                h->opacities = std::move(c.opacities);
                sp->host = std::move(h);
                sp->load_state = 2;
            } catch (const std::exception& e) {
                std::fprintf(stderr, "[render] %s\n", e.what());
                sp->load_state = 3;
            }
        });
    }
    return false;
}

bool FrameRenderer::scene_core(int source, SceneCore& out) {
    if (source < 0 || source >= (int)_slots.size()) return false;
    Slot& s = *_slots[(size_t)source];
    const spirula::Sim3 file_to_world = s.view.norm_to_world * s.view.file_to_norm;
    if (s.pts_valid && std::memcmp(&s.pts_at, &file_to_world, sizeof file_to_world) == 0) {
        out = s.core;
        return true;
    }
    std::vector<double> w;
    auto add = [&](const auto* p, int64_t n) {
        // Enough for every quantile a frame asks, cheap to sort per frame.
        const int64_t step = std::max<int64_t>(1, n / 40000);
        for (int64_t i = 0; i < n; i += step) {
            const double x[3] = {(double)p[i*3], (double)p[i*3+1], (double)p[i*3+2]};
            double q[3];
            file_to_world.apply(x, q);
            w.insert(w.end(), q, q + 3);
        }
    };
    switch (s.view.kind) {
        case SourceView::Splats:
            if (!effects_ready(source) || !s.host) return false;
            add(s.host->means.data(), s.host->n);
            break;
        case SourceView::Points:
            if (!s.view.ds) return false;
            add(s.view.ds->points.xyz.data(), s.view.ds->points.num());
            break;
        case SourceView::Mesh:
            if (!s.view.mesh || s.view.mesh->V.empty()) return false;
            add(s.view.mesh->V[0].data(), (int64_t)s.view.mesh->V.size());
            break;
    }
    const size_t m = w.size() / 3;
    if (!m) return false;
    // Every point counts the same, whatever its size or opacity: the centre
    // is the median, and what floats past four median distances is not the
    // scene. A trained scene keeps floaters and a sky far outside it.
    SceneCore st;
    std::vector<double> tmp(m);
    for (int d = 0; d < 3; d++) {
        for (size_t i = 0; i < m; i++) tmp[i] = w[i * 3 + d];
        std::nth_element(tmp.begin(), tmp.begin() + (ptrdiff_t)(m / 2), tmp.end());
        st.centre[d] = tmp[m / 2];
    }
    std::vector<double> dist(m);
    for (size_t i = 0; i < m; i++) {
        const double dx = w[i*3] - st.centre[0], dy = w[i*3+1] - st.centre[1],
                     dz = w[i*3+2] - st.centre[2];
        dist[i] = std::sqrt(dx*dx + dy*dy + dz*dz);
    }
    tmp = dist;
    std::nth_element(tmp.begin(), tmp.begin() + (ptrdiff_t)(m / 2), tmp.end());
    const double reach = 4.0 * tmp[m / 2];
    std::vector<double> core;
    for (size_t i = 0; i < m; i++)
        if (dist[i] <= reach) core.push_back(dist[i]);
    if (core.empty()) return false;
    const size_t k = std::min(core.size() - 1, (size_t)(0.9 * (double)(core.size() - 1)));
    std::nth_element(core.begin(), core.begin() + (ptrdiff_t)k, core.end());
    st.radius = std::max(core[k], 1e-9);
    s.pts = std::move(w);
    s.pts_at = file_to_world;
    s.pts_valid = true;
    s.q_valid = false;
    s.core = out = st;
    return true;
}

bool FrameRenderer::scene_quantiles(int source, const double centre[3], const double up[3],
                                    const double e1[3], const double e2[3], FxGeo& q,
                                    const FxView* view) {
    SceneCore core;
    if (!scene_core(source, core)) return false;
    Slot& s = *_slots[(size_t)source];
    double key[21] = {centre[0], centre[1], centre[2], up[0], up[1], up[2],
                      e1[0], e1[1], e1[2], e2[0], e2[1], e2[2]};
    if (view) {
        for (int k = 0; k < 3; k++) { key[12 + k] = view->pos[k]; key[15 + k] = view->fwd[k]; }
        key[18] = view->tx;
        key[19] = view->ty;
        key[20] = view->cone + 1.0;
    }
    if (s.q_valid && std::memcmp(key, s.q_key, sizeof key) == 0) {
        std::memcpy(q.qh, s.q.qh, sizeof q.qh);
        std::memcpy(q.qa, s.q.qa, sizeof q.qa);
        std::memcpy(q.qr, s.q.qr, sizeof q.qr);
        return true;
    }
    const size_t all = s.pts.size() / 3;
    // What the camera sees, when it sees enough to go by.
    std::vector<size_t> seen;
    if (view) {
        const double cos_cone = std::cos(view->cone);
        for (size_t i = 0; i < all; i++) {
            const double d[3] = {s.pts[i*3] - view->pos[0], s.pts[i*3+1] - view->pos[1],
                                 s.pts[i*3+2] - view->pos[2]};
            const double z = d[0]*view->fwd[0] + d[1]*view->fwd[1] + d[2]*view->fwd[2];
            if (view->cone > 0.0) {
                const double len = std::sqrt(d[0]*d[0] + d[1]*d[1] + d[2]*d[2]);
                if (z >= cos_cone * len) seen.push_back(i);
                continue;
            }
            if (z <= 0.0) continue;
            const double x = d[0]*view->right[0] + d[1]*view->right[1] + d[2]*view->right[2];
            const double y = d[0]*view->up[0] + d[1]*view->up[1] + d[2]*view->up[2];
            if (std::fabs(x) <= view->tx * z && std::fabs(y) <= view->ty * z) seen.push_back(i);
        }
    }
    const bool subset = seen.size() >= 64;
    const size_t m = subset ? seen.size() : all;
    std::vector<float> h(m), a(m), r(m);
    for (size_t j = 0; j < m; j++) {
        const size_t i = subset ? seen[j] : j;
        const double d[3] = {s.pts[i*3] - centre[0], s.pts[i*3+1] - centre[1],
                             s.pts[i*3+2] - centre[2]};
        const double x1 = d[0]*e1[0] + d[1]*e1[1] + d[2]*e1[2];
        const double x2 = d[0]*e2[0] + d[1]*e2[1] + d[2]*e2[2];
        h[j] = (float)(d[0]*up[0] + d[1]*up[1] + d[2]*up[2]);
        a[j] = (float)x1;
        r[j] = (float)std::sqrt(x1*x1 + x2*x2);
    }
    auto table = [&](std::vector<float>& v, float* out) {
        std::sort(v.begin(), v.end());
        for (int i = 0; i < kFxQuantiles; i++)
            out[i] = v[(size_t)std::lround((double)i / (kFxQuantiles - 1) * (double)(m - 1))];
    };
    table(h, s.q.qh);
    table(a, s.q.qa);
    table(r, s.q.qr);
    std::memcpy(s.q_key, key, sizeof key);
    s.q_valid = true;
    std::memcpy(q.qh, s.q.qh, sizeof q.qh);
    std::memcpy(q.qa, s.q.qa, sizeof q.qa);
    std::memcpy(q.qr, s.q.qr, sizeof q.qr);
    return true;
}

std::string FrameRenderer::take_error() {
    std::string e;
    e.swap(_error);
    return e;
}

void FrameRenderer::request(const FrameSpec& f) {
    _spec = f;
    _stage = Stage::Rendering;
    _passes.clear();
    for (int layer = 0; layer < 2; layer++) {
        const LayerSpec& l = layer ? f.b : f.a;
        _layer_tex[layer] = 0;
        _layer_flip[layer] = false;
        if (l.source < 0 || l.source >= (int)_slots.size()) continue;
        for (int v = 0; v < std::clamp(l.variants, 1, 2); v++) {
            Pass p;
            p.layer = layer;
            p.variant = v;
            _passes.push_back(p);
        }
    }
    for (Pass& p : _passes) submit(p);
}

void FrameRenderer::submit(Pass& p) {
    if (p.id || p.ready) return;
    const LayerSpec& l = p.layer ? _spec.b : _spec.a;
    Slot& s = *_slots[(size_t)l.source];
    if (s.view.kind != SourceView::Splats) return;       // drawn in poll()
    if (!s.worker) { p.ready = true; return; }
    for (const Pass& o : _passes) {
        const LayerSpec& ol = o.layer ? _spec.b : _spec.a;
        if (&o != &p && ol.source == l.source && o.id && !o.ready) return;   // its turn comes
    }
    const SourceStyle& style = l.style[std::clamp(p.variant, 0, 1)];
    const CameraState& c = _spec.cam;
    ViewRequest q;
    camera_in(c, s.view.norm_to_world, q.c2w);
    float intr[4];
    lens_intrinsics(c.lens, _spec.width, _spec.height, intr);
    q.fx = intr[0]; q.fy = intr[1]; q.cx = intr[2]; q.cy = intr[3];
    q.W = _spec.width;
    q.H = _spec.height;
    const int proj = std::clamp((int)c.lens.projection, 0, 3);
    q.model = kModelNames[proj];
    const int tier = proj == 3 ? 0 : std::clamp(c.lens.tier, 0, 2);
    q.distortion = kTierNames[tier];
    for (int k = 0; k < 8; k++) q.dist[k] = tier ? c.lens.dist[k] : 0.0f;
    q.raw = true;
    q.primitive = resolve_primitive(style.primitive, s.view.cfg.primitive, c.lens, _spec.width,
                                    _spec.height);
    q.sh_degree = style.sh_degree;

    const bool effect = l.grow != 1.0f || l.fade_in < 1.0f || l.clip != 0 || l.fx.kind != 0;
    if (effect && effects_ready(l.source) && s.host) {
        std::shared_ptr<SplatHost> h = s.host;
        const int slot = s.view.cfg.scene_slot;
        std::shared_ptr<const std::vector<uint8_t>> alive = s.view.alive;
        // The cut in the file's own coordinates, where the means are.
        double n[3] = {0, 0, 1}, d = 0.0;
        if (l.clip) {
            plane_in(s.view.norm_to_world * s.view.file_to_norm, l.clip_n, l.level, n, d);
            if (l.clip == 2) { for (double& v : n) v = -v; d = -d; }
        }
        const float log_grow = std::log(std::max(l.grow, 1e-4f));
        const float fade = std::clamp(l.fade_in, 0.0f, 1.0f);
        const int clip = l.clip;
        const double band = std::max((double)l.glow, 1e-9);
        const LayerFx fx = l.fx;
        const FxGeo geo = fx_geo_in(fx, s.view.norm_to_world * s.view.file_to_norm);
        q.before_render = [h, slot, alive, n0 = n[0], n1 = n[1], n2 = n[2], d,
                           log_grow, fade, clip, band, fx, geo] {
            const int64_t N = h->n;
            h->op.resize((size_t)N);
            h->sc.resize((size_t)N * 3);
            h->base_op.resize((size_t)N);
            if (fx.kind) h->mn.resize((size_t)N * 3);
            const uint8_t* live = alive && (int64_t)alive->size() == N ? alive->data() : nullptr;
#pragma omp parallel for schedule(static)
            for (int64_t i = 0; i < N; i++) {
                const bool dead = live && !live[i];
                const float o = dead ? kDeadOpacity : h->opacities[(size_t)i];
                h->base_op[(size_t)i] = o;
                float keep = fade;
                if (clip) {
                    const float* p = &h->means[(size_t)i * 3];
                    const double sgn = n0 * p[0] + n1 * p[1] + n2 * p[2] - d;
                    // Soft over a band: a splat straddling the cut fades.
                    keep *= (float)std::clamp(-sgn / band, 0.0, 1.0);
                }
                float grow = log_grow;
                if (fx.kind) {
                    const float* p = &h->means[(size_t)i * 3];
                    float r1, r2, dp[3], a, sz;
                    fx_random((uint32_t)i, r1, r2);
                    fx_apply(fx.kind, fx.incoming, fx.t, fx.p, geo, p, r1, r2, dp, a, sz);
                    for (int k = 0; k < 3; k++) h->mn[(size_t)i * 3 + k] = p[k] + dp[k];
                    keep *= std::clamp(a, 0.0f, 1.0f);
                    grow += std::log(std::max(sz, 1e-3f));
                }
                h->op[(size_t)i] = dead || keep <= 0.0f
                                       ? kDeadOpacity
                                       : (keep >= 1.0f ? o : logit(sigmoid(o) * keep));
                for (int k = 0; k < 3; k++)
                    h->sc[(size_t)i * 3 + k] = h->scales[(size_t)i * 3 + k] + grow;
            }
            engine_scene_update(slot, "opacities", tv(h->op, {N, 1}));
            engine_scene_update(slot, "scales", tv(h->sc, {N, 3}));
            if (fx.kind) engine_scene_update(slot, "means", tv(h->mn, {N, 3}));
        };
        q.after_render = [h, slot, moved = fx.kind != 0] {
            const int64_t N = h->n;
            engine_scene_update(slot, "opacities", tv(h->base_op, {N, 1}));
            engine_scene_update(slot, "scales", tv(h->scales, {N, 3}));
            if (moved) engine_scene_update(slot, "means", tv(h->means, {N, 3}));
        };
    }
    p.id = s.worker->submit(q);
    p.sent = now_s();
}

unsigned FrameRenderer::draw_gl_layer(Slot& s, const LayerSpec& l,
                                      const SourceStyle& style) {
    if (s.gl_failed) return 0;
    if (!s.gl) {
        s.gl = std::make_unique<PreviewRenderer>();
        bool ok = false;
        if (s.view.kind == SourceView::Points && s.view.ds && s.view.post)
            ok = s.gl->build(*s.view.ds, *s.view.post);
        else if (s.view.kind == SourceView::Mesh && s.view.mesh)
            ok = s.gl->build(*s.view.mesh, s.view.mesh_t2n);
        if (!ok) {
            s.gl_failed = true;
            s.gl.reset();
            return 0;
        }
    }
    const CameraState& c = _spec.cam;
    float c2w[12];
    camera_in(c, s.view.norm_to_world, c2w);
    float view[16];
    for (int r = 0; r < 3; r++) {
        const float ax = c2w[0*4 + r], ay = c2w[1*4 + r], az = c2w[2*4 + r];
        view[r*4 + 0] = ax;
        view[r*4 + 1] = ay;
        view[r*4 + 2] = az;
        view[r*4 + 3] = -(ax*c2w[3] + ay*c2w[7] + az*c2w[11]);
    }
    view[12] = view[13] = view[14] = 0.0f;
    view[15] = 1.0f;
    const int W = _spec.width, H = _spec.height;
    float intr[4];
    lens_intrinsics(c.lens, W, H, intr);

    PreviewStyle ps;
    ps.transparent = true;
    ps.point_shape = (int)style.point_style;
    ps.point_px = style.point_px * (float)H / 1080.0f * l.grow;
    ps.point_radius = style.sphere_radius * l.grow;
    const int proj = std::clamp((int)c.lens.projection, 0, 3);
    ps.tier = proj == 3 ? 0 : std::clamp(c.lens.tier, 0, 2);
    for (int k = 0; k < 8; k++) ps.dist[k] = ps.tier ? c.lens.dist[k] : 0.0f;
    if (l.clip) {
        double n[3], d;
        plane_in(s.view.norm_to_world, l.clip_n, l.level, n, d);
        if (l.clip == 2) { for (double& v : n) v = -v; d = -d; }
        for (int k = 0; k < 3; k++) ps.plane[k] = (float)n[k];
        ps.plane[3] = (float)d;
        ps.clip = true;
        ps.glow = l.glow;
    }
    for (int k = 0; k < 3; k++) ps.glow_col[k] = l.glow_col[k];
    if (l.fx.kind) {
        ps.fx = l.fx.kind;
        ps.fx_in = l.fx.incoming;
        ps.fx_t = l.fx.t;
        ps.fx_p[0] = l.fx.p[0];
        ps.fx_p[1] = l.fx.p[1];
        ps.fx_geo = fx_geo_in(l.fx, s.view.norm_to_world);
    }
    s.gl->set_mesh_display(style.shade, style.flat, style.colour);
    const float dist = (float)std::sqrt(c2w[3]*c2w[3] + c2w[7]*c2w[7] + c2w[11]*c2w[11]);
    const float target[3] = {0, 0, 0};
    return s.gl->render(W, H, view, (PreviewProjection)proj,
                        intr[0] / (0.5f * W), intr[1] / (0.5f * H),
                        std::max(2.0f, dist), std::max(dist, 0.05f), target,
                        style.cameras, 1.0f, false, 0.0f, &ps);
}

unsigned FrameRenderer::target(int index) {
    const int W = _spec.width, H = _spec.height;
    if (W != _pool_w || H != _pool_h) {
        for (unsigned& t : _tex_pool)
            if (t) { GLuint g = t; glDeleteTextures(1, &g); t = 0; }
        _pool_w = W;
        _pool_h = H;
    }
    unsigned& t = _tex_pool[std::clamp(index, 0, 5)];
    if (!t) {
        GLuint g = 0;
        glGenTextures(1, &g);
        t = g;
        glBindTexture(GL_TEXTURE_2D, g);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    return t;
}

bool FrameRenderer::poll(double wait) {
    if (_stage != Stage::Rendering) return false;
    // Splat passes: collect what came back, and send the next of each worker.
    bool waiting = false;
    for (size_t i = 0; i < _passes.size(); i++) {
        Pass& p = _passes[i];
        const LayerSpec& l = p.layer ? _spec.b : _spec.a;
        Slot& s = *_slots[(size_t)l.source];
        if (s.view.kind != SourceView::Splats || p.ready) continue;
        if (!p.id) submit(p);
        if (p.ready) continue;
        if (!p.id) { waiting = true; continue; }
        ViewResult res;
        const bool got = s.worker->take_result(p.id, res, wait);
        if (!got) {
            // A render that never comes back would hold the frame forever.
            if (now_s() - p.sent > 60.0) {
                if (_error.empty()) _error = "a splat render did not come back";
                p.ready = true;
            } else {
                waiting = true;
            }
            continue;
        }
        p.ready = true;
        if (!res.error.empty() || res.rgba8.empty()) {
            if (_error.empty()) _error = res.error;
        } else {
            p.tex = target((int)i);
            glBindTexture(GL_TEXTURE_2D, p.tex);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, res.W, res.H, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, res.rgba8.data());
            glBindTexture(GL_TEXTURE_2D, 0);
            p.flip = true;
        }
        for (Pass& o : _passes)
            if (!o.id && !o.ready) submit(o);
    }
    if (waiting) return false;
    // Points and meshes, drawn now and kept, since the next draw of the same
    // model reuses its renderer's target.
    for (size_t i = 0; i < _passes.size(); i++) {
        Pass& p = _passes[i];
        const LayerSpec& l = p.layer ? _spec.b : _spec.a;
        Slot& s = *_slots[(size_t)l.source];
        if (s.view.kind == SourceView::Splats) continue;
        const unsigned t = draw_gl_layer(s, l, l.style[std::clamp(p.variant, 0, 1)]);
        if (t) {
            p.tex = target((int)i);
            blend(t, false, 0, false, 0.0f, p.tex);
            p.flip = false;
        }
        p.ready = true;
    }
    // Each layer: its one look, or its two mixed.
    for (int layer = 0; layer < 2; layer++) {
        const Pass* v[2] = {nullptr, nullptr};
        for (const Pass& p : _passes)
            if (p.layer == layer && p.tex) v[std::clamp(p.variant, 0, 1)] = &p;
        const LayerSpec& l = layer ? _spec.b : _spec.a;
        if (v[0] && v[1]) {
            _layer_tex[layer] = target(4 + layer);
            blend(v[0]->tex, v[0]->flip, v[1]->tex, v[1]->flip, l.style_mix, _layer_tex[layer]);
            _layer_flip[layer] = false;
        } else if (v[0] || v[1]) {
            const Pass* one = v[0] ? v[0] : v[1];
            _layer_tex[layer] = one->tex;
            _layer_flip[layer] = one->flip;
        }
    }
    composite();
    _stage = Stage::Idle;
    _passes.clear();
    _done++;
    return true;
}

void FrameRenderer::blend(unsigned a, bool flip_a, unsigned b, bool flip_b, float w,
                          unsigned dst) {
    if (!ensure_compositor() || !_blend_prog) return;
    if (!_blend_fbo) {
        GLuint f = 0;
        glx::GenFramebuffers(1, &f);
        _blend_fbo = f;
    }
    glx::BindFramebuffer(GL_FRAMEBUFFER, _blend_fbo);
    glx::FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dst, 0);
    glViewport(0, 0, _spec.width, _spec.height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glx::UseProgram(_blend_prog);
    glx::ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, a);
    glx::ActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, b ? b : a);
    glx::Uniform1i(_bu[0], 0);
    glx::Uniform1i(_bu[1], 1);
    glx::Uniform1i(_bu[2], flip_a ? 1 : 0);
    glx::Uniform1i(_bu[3], flip_b ? 1 : 0);
    glx::Uniform1i(_bu[4], b ? 1 : 0);
    glx::Uniform1f(_bu[5], w);
    glx::BindVertexArray(_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glx::BindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glx::ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glx::UseProgram(0);
    glx::BindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool FrameRenderer::ensure_compositor() {
    if (_prog) return true;
    if (!glx::init()) return false;
    GLuint vs = compile_shader(GL_VERTEX_SHADER, kCompVert);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, kCompFrag);
    if (!vs || !fs) return false;
    _prog = glx::CreateProgram();
    glx::AttachShader(_prog, vs);
    glx::AttachShader(_prog, fs);
    glx::LinkProgram(_prog);
    glx::DeleteShader(vs);
    glx::DeleteShader(fs);
    GLint ok = 0;
    glx::GetProgramiv(_prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        glx::DeleteProgram(_prog);
        _prog = 0;
        return false;
    }
    const char* names[] = {"u_a", "u_b", "u_has_a", "u_has_b", "u_flip_a",
                           "u_flip_b", "u_mode", "u_mask", "u_mix", "u_dir",
                           "u_aspect", "u_bg", "u_tint0", "u_tint1", "u_soft", "u_zoom",
                           "u_la0", "u_lb0", "u_lo0", "u_la1", "u_lb1", "u_lo1"};
    for (int i = 0; i < 22; i++) _u[i] = glx::GetUniformLocation(_prog, names[i]);
    GLuint vao = 0;
    glx::GenVertexArrays(1, &vao);
    _vao = vao;
    GLuint bvs = compile_shader(GL_VERTEX_SHADER, kCompVert);
    GLuint bfs = compile_shader(GL_FRAGMENT_SHADER, kBlendFrag);
    if (bvs && bfs) {
        _blend_prog = glx::CreateProgram();
        glx::AttachShader(_blend_prog, bvs);
        glx::AttachShader(_blend_prog, bfs);
        glx::LinkProgram(_blend_prog);
        glx::GetProgramiv(_blend_prog, GL_LINK_STATUS, &ok);
        if (!ok) { glx::DeleteProgram(_blend_prog); _blend_prog = 0; }
    }
    if (bvs) glx::DeleteShader(bvs);
    if (bfs) glx::DeleteShader(bfs);
    const char* bnames[] = {"u_t0", "u_t1", "u_flip0", "u_flip1", "u_has1", "u_w"};
    for (int i = 0; _blend_prog && i < 6; i++) _bu[i] = glx::GetUniformLocation(_blend_prog, bnames[i]);
    return true;
}

void FrameRenderer::composite() {
    if (!ensure_compositor()) {
        _error = "compositor unavailable (OpenGL 3.2 required)";
        return;
    }
    const int W = _spec.width, H = _spec.height;
    if (!_fbo || W != _out_w || H != _out_h) {
        if (!_fbo) {
            GLuint f = 0, t = 0;
            glx::GenFramebuffers(1, &f);
            glGenTextures(1, &t);
            _fbo = f;
            _out_tex = t;
        }
        glBindTexture(GL_TEXTURE_2D, _out_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);
        glx::BindFramebuffer(GL_FRAMEBUFFER, _fbo);
        glx::FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  GL_TEXTURE_2D, _out_tex, 0);
        glx::BindFramebuffer(GL_FRAMEBUFFER, 0);
        _out_w = W;
        _out_h = H;
    }
    glx::BindFramebuffer(GL_FRAMEBUFFER, _fbo);
    glViewport(0, 0, W, H);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glx::UseProgram(_prog);
    for (int i = 0; i < 2; i++) {
        glx::ActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, _layer_tex[i]);
    }
    glx::Uniform1i(_u[0], 0);
    glx::Uniform1i(_u[1], 1);
    glx::Uniform1i(_u[2], _layer_tex[0] ? 1 : 0);
    glx::Uniform1i(_u[3], _layer_tex[1] ? 1 : 0);
    glx::Uniform1i(_u[4], _layer_flip[0] ? 1 : 0);
    glx::Uniform1i(_u[5], _layer_flip[1] ? 1 : 0);
    glx::Uniform1i(_u[6], _spec.mode);
    glx::Uniform1i(_u[7], _spec.mask);
    glx::Uniform1f(_u[8], _spec.mix);
    glx::Uniform2f(_u[9], _spec.wipe_dir[0], _spec.wipe_dir[1]);
    glx::Uniform1f(_u[10], (float)W / (float)std::max(H, 1));
    glx::Uniform4f(_u[11], _spec.background[0], _spec.background[1],
                   _spec.background[2], _spec.transparent ? 0.0f : 1.0f);
    glx::Uniform4f(_u[12], _spec.tint[0][0], _spec.tint[0][1], _spec.tint[0][2],
                   _spec.tint[0][3]);
    glx::Uniform4f(_u[13], _spec.tint[1][0], _spec.tint[1][1], _spec.tint[1][2],
                   _spec.tint[1][3]);
    glx::Uniform1f(_u[14], _spec.soft);
    glx::Uniform1f(_u[15], _spec.zoom);
    for (int i = 0; i < 2; i++) {
        const LayerSpec& l = i ? _spec.b : _spec.a;
        glx::Uniform4f(_u[16 + 3 * i], l.opacity, (float)l.mask, l.mask_t, l.mask_soft);
        glx::Uniform4f(_u[17 + 3 * i], l.mask_dir[0], l.mask_dir[1], l.zoom, l.zoom_blur);
        glx::Uniform1i(_u[18 + 3 * i], l.mask_out ? 1 : 0);
    }
    glx::BindVertexArray(_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glx::BindVertexArray(0);
    for (int i = 1; i >= 0; i--) {
        glx::ActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glx::UseProgram(0);
    glx::BindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FrameRenderer::read(std::vector<uint8_t>& out, bool alpha) {
    const int W = _out_w, H = _out_h;
    if (!_fbo || W <= 0 || H <= 0) { out.clear(); return; }
    std::vector<uint8_t> rgba((size_t)W * H * 4);
    glx::BindFramebuffer(GL_FRAMEBUFFER, _fbo);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glx::BindFramebuffer(GL_FRAMEBUFFER, 0);
    const int C = alpha ? 4 : 3;
    out.resize((size_t)W * H * C);
#pragma omp parallel for schedule(static)
    for (int y = 0; y < H; y++) {
        const uint8_t* src = &rgba[(size_t)(H - 1 - y) * W * 4];
        uint8_t* dst = &out[(size_t)y * W * C];
        for (int x = 0; x < W; x++) {
            const uint8_t* p = src + x * 4;
            uint8_t* q = dst + x * C;
            if (alpha) {
                const int a = p[3];
                for (int c = 0; c < 3; c++)
                    q[c] = a ? (uint8_t)std::min(255, (p[c] * 255 + a / 2) / a) : 0;
                q[3] = (uint8_t)a;
            } else {
                q[0] = p[0];
                q[1] = p[1];
                q[2] = p[2];
            }
        }
    }
}

}  // namespace gui::render

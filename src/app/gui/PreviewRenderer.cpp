// PreviewRenderer.cpp -- see PreviewRenderer.h.

#include "app/gui/PreviewRenderer.h"

#include "app/webviewer/RegionOverlay.h"

#include "app/gui/GlLoader.h"
#include "app/TrainerCore.h"
#include "data/CameraMath.h"
#include "data/FrustumTemplate.h"
#include "mesh/MeshImport.h"   // mesh_compute_normals

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

namespace gui {

namespace {

// One shader for all draws. Vertices are (pos, aux); for the point cloud
// and the grid aux is the RGB color (u_scale = 0), for camera frusta aux is
// the offset from the camera center, scaled by the live frustum-size
// uniform (u_color.a > 0 selects the flat line color).
// Projection happens in the shader so the preview supports the same camera
// models as the engine viewer: u_model selects pinhole(0) / fisheye-
// equidistant(1) / equisolid(2) / equirectangular(3); u_s is the engine
// intrinsics normalized to NDC (fx/(W/2), fy/(H/2)). Depth is the linear
// view-space distance mapped over u_zrange (fine for points/lines and valid
// for the >180-degree projections where a perspective z is meaningless).
//
// The projection function is shared with the fragment shader: a line
// segment crossing a projection discontinuity (equirectangular +-180-degree
// seam, fisheye backward point, pinhole behind-camera) rasterizes as the
// chord between the wrapped endpoints; re-projecting the interpolated
// view-space position exposes such fragments as a huge reprojection error
// (same fix as the web viewer's LINE_FS, viewer/js/shaders.js).
// The reprojection check alone still keeps ~5%-of-viewport stubs at both
// ends of a wrapped segment (the error shrinks to zero at the endpoints),
// which stack into ladder artifacts at the equirect image edges. Line
// vertices therefore also carry a_delta = this-endpoint minus
// other-endpoint (pre-scale), and the vertex shader kills the WHOLE
// segment (v_kill) when its chord crosses the equirect seam -- the
// half-plane x = 0, z > 0 in view space (azimuth +-180 degrees).
const char* kProj = R"(
uniform int u_model;
uniform vec2 u_s;
uniform int u_tier;
uniform float u_dist[8];
// The engine's lens tiers (shaders/projection_utils.slang), in its CV
// convention: y down, so the image's y is flipped around it.
vec2 lens(vec2 gl) {
    if (u_tier == 0) return gl;
    float u = gl.x, v = -gl.y, r2 = u*u + v*v;
    vec2 cv;
    if (u_tier == 1) {
        float k1 = u_dist[0], k2 = u_dist[1], p1 = u_dist[2], p2 = u_dist[3];
        cv = vec2(u, v) * (1.0 + r2*(k1 + r2*k2)) +
             vec2(2.0*p1*u*v + p2*(r2 + 2.0*u*u), 2.0*p2*u*v + p1*(r2 + 2.0*v*v));
    } else {
        float k1 = u_dist[0], k2 = u_dist[1], k3 = u_dist[2], k4 = u_dist[3];
        float p1 = u_dist[4], p2 = u_dist[5], s1 = u_dist[6], s2 = u_dist[7];
        cv = vec2(u, v) * (1.0 + r2*(k1 + r2*(k2 + r2*(k3 + r2*k4)))) +
             vec2(2.0*p1*u*v + p2*(r2 + 2.0*u*u) + s1*r2,
                  2.0*p2*u*v + p1*(r2 + 2.0*v*v) + s2*r2);
    }
    return vec2(cv.x, -cv.y);
}
// The engine's is_valid_distortion: where the lens folds over itself, which
// it does not draw either. The y flip leaves the determinant, the diagonal
// and the dot product as they are.
bool lens_valid(vec2 gl) {
    if (u_tier == 0) return true;
    const float e = 1e-3;
    vec2 f = lens(gl);
    vec2 jx = (lens(gl + vec2(e, 0.0)) - f) / e, jy = (lens(gl + vec2(0.0, e)) - f) / e;
    float jd = min(jx.x * jy.y - jy.x * jx.y, min(jx.x, jy.y));
    return jd > 0.25 && jd < 4.0 && dot(gl, f) >= 0.0;
}
vec2 project_ndc(vec3 v, out bool clipped) {
    float dist = max(length(v), 1e-9);
    clipped = false;
    if (u_model == 0) {                     // pinhole
        if (v.z > -1e-6) clipped = true;
        vec2 q = v.xy / -v.z;
        if (!lens_valid(q)) clipped = true;
        return u_s * lens(q);
    } else if (u_model == 3) {              // equirectangular
        float lon = atan(v.x, -v.z);
        float lat = asin(clamp(v.y / dist, -1.0, 1.0));
        return u_s * vec2(lon, lat);
    }                                       // fisheye / equisolid
    float theta = acos(clamp(-v.z / dist, -1.0, 1.0));
    float rlen = length(v.xy);
    vec2 dir2 = rlen > 1e-9 ? v.xy / rlen : vec2(0.0);
    float r = (u_model == 1) ? theta : 2.0 * sin(0.5 * theta);
    if (!lens_valid(dir2 * r)) clipped = true;
    return u_s * lens(dir2 * r);
}
)";

// Fragment stages only: a vertex shader may not hold a discard. A world-space
// cut for the reveal effects -- past the plane is not drawn, and a band
// before it glows.
const char* kClip = R"(
uniform int u_clip_on;
uniform vec4 u_clip;
uniform float u_glow;
uniform vec3 u_glow_col;
vec3 clip_colour(vec3 col, vec3 world) {
    if (u_clip_on == 0) return col;
    float s = dot(u_clip.xyz, world) - u_clip.w;
    if (s > 0.0) discard;
    if (u_glow > 0.0 && s > -u_glow)
        col = mix(col, u_glow_col, 0.85 * (1.0 + s / u_glow));
    return col;
}
)";

const char* kVertMain = R"(
in vec3 a_pos;
in vec3 a_aux;
in vec3 a_delta;
uniform mat4 u_view;
uniform float u_scale;
uniform float u_dscale;
uniform vec4 u_color;
uniform vec2 u_zrange;
uniform vec2 u_vp;
uniform float u_psize;      // screen size of a point, pixels
uniform float u_pradius;    // > 0: a sphere of this radius instead
uniform int u_points;
out vec4 v_col;
out vec3 v_view;
out vec3 v_world;
out float v_kill;
out float v_fxa;
out float v_fxr;
out float v_dash;
void main() {
    v_dash = a_aux.x;
    vec3 p = a_pos + u_scale * a_aux;
    float fxs = 1.0;
    v_fxa = 1.0;
    v_fxr = 0.0;
    if (u_fx != 0 && u_points > 0) {
        float r1 = fx_hashu(uint(gl_VertexID)), r2 = fx_hashu(uint(gl_VertexID) ^ 0x9E3779B9u);
        vec3 dd;
        fx_apply(p, r1, r2, dd, v_fxa, fxs);
        p += dd;
        v_fxr = fract(r1 * 7.31 + r2);
    }
    vec3 v = (u_view * vec4(p, 1.0)).xyz;
    float dist = max(length(v), 1e-9);
    bool clipped;
    vec2 ndc = project_ndc(v, clipped);
    float z = (dist - u_zrange.x) / (u_zrange.y - u_zrange.x) * 2.0 - 1.0;
    if (clipped) z = 3.0;
    v_col = (u_color.a > 0.0) ? u_color : vec4(a_aux, 1.0);
    v_view = v;
    v_world = p;
    // A soft point's half-maximum spans the size asked for, as wide as a
    // hard one looks: exp(-4 r^2) halves at r = 0.42 of the sprite's radius.
    gl_PointSize = u_pradius > 0.0
        ? clamp(u_pradius * u_s.x * u_vp.x / dist, 1.0, 256.0)
        : u_points == 3 ? min(u_psize * 2.4, 256.0) : u_psize;
    gl_PointSize = clamp(gl_PointSize * fxs, 1.0, 256.0);
    // Whole-segment equirect seam kill (see the comment above kProj). Both
    // vertices of a segment compute the same flag, so it interpolates flat.
    v_kill = 0.0;
    if (u_model == 3) {
        vec3 v2 = (u_view * vec4(p - u_dscale * a_delta, 1.0)).xyz;
        if (v.x * v2.x < 0.0) {
            float t = v.x / (v.x - v2.x);
            if (mix(v.z, v2.z, t) > 0.0) v_kill = 1.0;
        }
    }
    gl_Position = vec4(ndc, z, 1.0);
}
)";

const char* kFragMain = R"(
in vec4 v_col;
in vec3 v_view;
in vec3 v_world;
in float v_kill;
in float v_fxa;
in float v_fxr;
in float v_dash;
uniform vec2 u_vp;
uniform vec2 u_zrange;
uniform float u_alpha;      // lines and triangles: 1 opaque
uniform float u_dash;       // > 0: lines dashed with this period of v_dash
uniform int u_points;       // drawing the cloud: 0 square, 1 circle, 2 gaussian, 3 sphere
uniform float u_pradius;
out vec4 frag;
void main() {
    // Written on every path, or the paths that do not write it get an
    // undefined depth once the sphere below writes it.
    gl_FragDepth = gl_FragCoord.z;
    if (v_kill > 0.5) discard;
    if (u_points > 0) {
        vec2 c = gl_PointCoord * 2.0 - 1.0;
        float r2 = dot(c, c);
        int shape = u_points - 1;
        if (shape != 0 && r2 > 1.0) discard;
        // A transition's opacity: a soft point fades, a hard one is there
        // or not, each at its own moment.
        if (v_fxa <= 0.0 || (shape != 2 && v_fxa < v_fxr)) discard;
        vec3 col = clip_colour(v_col.rgb, v_world);
        if (shape == 2) {
            // Premultiplied, for the blend the cloud is drawn with.
            float a = exp(-4.0 * r2) * v_fxa;
            frag = vec4(col * a, a);
        } else if (shape == 3) {
            float nz = sqrt(max(1.0 - r2, 0.0));
            frag = vec4(col * (0.3 + 0.7 * nz), 1.0);
            gl_FragDepth = gl_FragCoord.z -
                0.5 * u_pradius * nz / (u_zrange.y - u_zrange.x);
        } else {
            frag = vec4(col, 1.0);
        }
        return;
    }
    bool clipped;
    vec2 ndc = project_ndc(v_view, clipped);
    vec2 px = (0.5 * ndc + 0.5) * u_vp;
    if (clipped ||
        length(px - gl_FragCoord.xy) > 0.05 * min(u_vp.x, u_vp.y)) discard;
    if (u_dash > 0.0 && fract(v_dash / u_dash) > 0.55) discard;
    frag = vec4(clip_colour(v_col.rgb, v_world), u_alpha);
}
)";

// Mesh program. Shares kProj with the point/line program above -- one
// projection implementation, so a mesh and a point cloud of the same scene
// land on the same pixels -- and adds per-vertex normal / color / uv and a
// fixed headlight.
//
// The discontinuity handling is the lines' rule adapted to filled triangles:
// a triangle's projected outline is exact under pinhole (the projection of a
// triangle IS the triangle of projected vertices), so no fragment is
// discarded there; under the curved models the edges bend, so the check would
// eat legitimate interior fragments and only the behind-camera / seam cases
// are rejected.
const char* kMeshVert = R"(
in vec3 a_pos;
in vec3 a_nrm;
in vec3 a_col;
in vec2 a_uv;
uniform mat4 u_view;
uniform vec2 u_zrange;
out vec3 v_nrm;
out vec3 v_col;
out vec2 v_uv;
out vec3 v_view;
out vec3 v_world;
out float v_fxa;
void main() {
    vec3 pos = a_pos;
    v_fxa = 1.0;
    if (u_fx != 0) {
        float s = 3.0 / u_fx_radius, fs;
        vec3 dd;
        fx_apply(a_pos, fx_noise(a_pos * s), fx_noise(a_pos * s + 17.3), dd, v_fxa, fs);
        pos += dd;
    }
    vec3 v = (u_view * vec4(pos, 1.0)).xyz;
    float dist = max(length(v), 1e-9);
    float z = (dist - u_zrange.x) / (u_zrange.y - u_zrange.x) * 2.0 - 1.0;
    v_nrm = mat3(u_view) * a_nrm;
    v_col = a_col;
    v_uv = a_uv;
    v_view = v;
    v_world = a_pos;
    if (u_model == 0 && u_tier == 0) {
        // PINHOLE: emit a REAL clip-space position (w = -z_view) so the
        // hardware clips triangles at the near plane. Writing NDC with w = 1
        // (which is what the point/line program does, and what this used to
        // do) leaves a triangle straddling the camera plane to rasterize
        // between a finite vertex and a wrapped one -- the glitching wedges.
        // The projected triangle IS the triangle of projected vertices under
        // perspective, so past the clip there is nothing to discard.
        float w = -v.z;
        gl_Position = vec4(u_s * v.xy, z * w, w);
    } else {
        // The curved models have no linear clip space; NDC goes out directly
        // and the fragment shader rejects what wrapped (see kMeshFrag).
        bool clipped;
        vec2 ndc = project_ndc(v, clipped);
        gl_Position = vec4(ndc, clipped ? 3.0 : z, 1.0);
    }
}
)";

const char* kMeshFrag = R"(
in vec3 v_nrm;
in vec3 v_col;
in vec2 v_uv;
in vec3 v_view;
in vec3 v_world;
in float v_fxa;
uniform vec2 u_vp;
uniform int u_mode;          // 0 flat, 1 vertex color, 2 texture
uniform int u_color_on;      // show vertex/texture color
uniform int u_shade;         // apply the headlight
uniform int u_flat;          // face normals instead of interpolated ones
uniform sampler2D u_tex;
out vec4 frag;
void main() {
    // A transition fading a mesh drops it grain by grain, a grain being a
    // cell of the surface where it rests.
    if (u_fx != 0 && v_fxa < fx_h3(floor(v_world * (60.0 / u_fx_radius)))) discard;
    if (u_model != 0 || u_tier != 0) {
        // Reject fragments of a triangle that crosses a projection
        // discontinuity (the equirect +-180-degree seam, the fisheye backward
        // point). Two tests, because either alone leaves artifacts:
        //   * the reprojection ERROR is large across most of a wrapped
        //     triangle -- but it vanishes at the vertices, leaving stubs;
        //   * the error's GRADIENT stays O(1) right up to those vertices,
        //     while an ordinary triangle's curvature error and gradient are
        //     both small.
        // Same pair the web viewer's MESH_FS uses, so the two agree on which
        // triangles disappear.
        bool clipped;
        vec2 ndc = project_ndc(v_view, clipped);
        if (clipped) discard;
        vec2 err = (0.5 * ndc + 0.5) * u_vp - gl_FragCoord.xy;
        vec2 gx = dFdx(err), gy = dFdy(err);
        if (length(err) > 0.05 * min(u_vp.x, u_vp.y) ||
            max(length(gx), length(gy)) > 0.5) discard;
    }
    vec3 base = vec3(0.78);
    if (u_color_on == 1) {
        if (u_mode == 2) base = texture(u_tex, v_uv).rgb;
        else if (u_mode == 1) base = v_col;
    }
    float shade = 1.0;
    if (u_shade == 1) {
        vec3 n = v_nrm;
        if (u_flat == 1 || dot(n, n) < 1e-12)
            n = cross(dFdx(v_view), dFdy(v_view));   // face normal, view space
        n = normalize(n);
        // Headlight: the light follows the camera, so the surface reads from
        // every angle; abs() lights back faces too, because a black backface
        // reads as a hole rather than as an orientation problem.
        vec3 l = normalize(-v_view);
        shade = 0.25 + 0.75 * abs(dot(n, l));
    }
    frag = vec4(clip_colour(base * shade, v_world), 1.0);
}
)";

GLuint compile(GLenum type, const char* src) {
    GLuint sh = glx::CreateShader(type);
    glx::ShaderSource(sh, 1, &src, nullptr);
    glx::CompileShader(sh);
    GLint ok = 0;
    glx::GetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glx::GetShaderInfoLog(sh, sizeof log, nullptr, log);
        std::fprintf(stderr, "[preview] shader error: %s\n", log);
        glx::DeleteShader(sh);
        return 0;
    }
    return sh;
}

struct V { float px, py, pz, ax, ay, az; };
// Line vertex: V plus a_delta = this-endpoint minus other-endpoint of the
// segment, pre-scale (frusta: in offset units; grid: in position units), so
// the vertex shader can kill whole segments crossing the equirect seam.
struct VL { float px, py, pz, ax, ay, az, dx, dy, dz; };

// Fill the deltas of a GL_LINES vertex array built as consecutive pairs.
// For frusta both vertices of a pair share a_pos (the camera center), so the
// endpoint difference is the a_aux (offset) difference; for the grid a_aux
// is the color and u_scale = 0, so it is the a_pos difference. delta_from_aux
// selects which.
void fill_line_deltas(std::vector<VL>& v, bool delta_from_aux) {
    for (size_t i = 0; i + 1 < v.size(); i += 2) {
        VL& a = v[i];
        VL& b = v[i + 1];
        float d[3];
        if (delta_from_aux) {
            d[0] = a.ax - b.ax; d[1] = a.ay - b.ay; d[2] = a.az - b.az;
        } else {
            d[0] = a.px - b.px; d[1] = a.py - b.py; d[2] = a.pz - b.pz;
        }
        a.dx = d[0]; a.dy = d[1]; a.dz = d[2];
        b.dx = -d[0]; b.dy = -d[1]; b.dz = -d[2];
    }
}

// ---- camera-frustum template --------------------------------------------
// data/FrustumTemplate.h, the web viewer's shape: distorted and >180-degree
// fisheye cameras draw as domes, not as tan-based pyramids.

using P3 = camhost::FrustumPoint;
using FrustumTemplate = camhost::FrustumShape;
using camhost::FrustumLine;
using camhost::frustum_template;
constexpr int kASeg = camhost::kFrustumAnchorSeg;
constexpr int M_PINHOLE = 0;

}  // namespace

bool PreviewRenderer::ensure_program() {
    if (_prog) return true;
    if (!glx::init()) return false;
    std::string vs_src = std::string("#version 150\n") + kProj + render::fx_glsl() + kVertMain;
    std::string fs_src = std::string("#version 150\n") + kProj + kClip + kFragMain;
    GLuint vs = compile(GL_VERTEX_SHADER, vs_src.c_str());
    GLuint fs = compile(GL_FRAGMENT_SHADER, fs_src.c_str());
    if (!vs || !fs) return false;
    _prog = glx::CreateProgram();
    glx::AttachShader(_prog, vs);
    glx::AttachShader(_prog, fs);
    glx::BindAttribLocation(_prog, 0, "a_pos");
    glx::BindAttribLocation(_prog, 1, "a_aux");
    glx::BindAttribLocation(_prog, 2, "a_delta");
    glx::LinkProgram(_prog);
    glx::DeleteShader(vs);
    glx::DeleteShader(fs);
    GLint ok = 0;
    glx::GetProgramiv(_prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glx::GetProgramInfoLog(_prog, sizeof log, nullptr, log);
        std::fprintf(stderr, "[preview] link error: %s\n", log);
        glx::DeleteProgram(_prog);
        _prog = 0;
        return false;
    }
    _u_view = glx::GetUniformLocation(_prog, "u_view");
    _u_scale = glx::GetUniformLocation(_prog, "u_scale");
    _u_dscale = glx::GetUniformLocation(_prog, "u_dscale");
    _u_color = glx::GetUniformLocation(_prog, "u_color");
    _u_alpha = glx::GetUniformLocation(_prog, "u_alpha");
    _u_dash = glx::GetUniformLocation(_prog, "u_dash");
    _u_model = glx::GetUniformLocation(_prog, "u_model");
    _u_s = glx::GetUniformLocation(_prog, "u_s");
    _u_zrange = glx::GetUniformLocation(_prog, "u_zrange");
    _u_vp = glx::GetUniformLocation(_prog, "u_vp");
    _u_points = glx::GetUniformLocation(_prog, "u_points");
    _u_psize = glx::GetUniformLocation(_prog, "u_psize");
    _u_pradius = glx::GetUniformLocation(_prog, "u_pradius");
    style_locations(0, _prog);
    return true;
}

void PreviewRenderer::style_locations(int program, unsigned prog) {
    StyleLoc& l = _sloc[program];
    auto at = [&](const char* name) { return glx::GetUniformLocation(prog, name); };
    l = {at("u_tier"), at("u_dist"), at("u_clip_on"), at("u_clip"), at("u_glow"),
         at("u_glow_col"), at("u_fx"), at("u_fx_in"), at("u_fx_t"), at("u_fx_p"), at("u_fx_c"),
         at("u_fx_up"), at("u_fx_e1"), at("u_fx_e2"), at("u_fx_radius"), at("u_fx_qh"),
         at("u_fx_qa"), at("u_fx_qr")};
}

void PreviewRenderer::set_style_uniforms(int program, const PreviewStyle& st) {
    const StyleLoc& l = _sloc[program];
    glx::Uniform1i(l.tier, st.tier);
    glx::Uniform1fv(l.dist, 8, st.dist);
    glx::Uniform1i(l.clip_on, st.clip ? 1 : 0);
    glx::Uniform4f(l.clip, st.plane[0], st.plane[1], st.plane[2], st.plane[3]);
    glx::Uniform1f(l.glow, st.glow);
    glx::Uniform3f(l.glow_col, st.glow_col[0], st.glow_col[1], st.glow_col[2]);
    const render::FxGeo& g = st.fx_geo;
    glx::Uniform1i(l.fx, st.fx);
    glx::Uniform1i(l.fx_in, st.fx_in ? 1 : 0);
    glx::Uniform1f(l.fx_t, st.fx_t);
    glx::Uniform2f(l.fx_p, st.fx_p[0], st.fx_p[1]);
    glx::Uniform3f(l.fx_c, g.c[0], g.c[1], g.c[2]);
    glx::Uniform3f(l.fx_up, g.up[0], g.up[1], g.up[2]);
    glx::Uniform3f(l.fx_e1, g.e1[0], g.e1[1], g.e1[2]);
    glx::Uniform3f(l.fx_e2, g.e2[0], g.e2[1], g.e2[2]);
    glx::Uniform1f(l.fx_radius, std::max(g.radius, 1e-6f));
    glx::Uniform1fv(l.fx_qh, render::kFxQuantiles, g.qh);
    glx::Uniform1fv(l.fx_qa, render::kFxQuantiles, g.qa);
    glx::Uniform1fv(l.fx_qr, render::kFxQuantiles, g.qr);
}

bool PreviewRenderer::ensure_mesh_program() {
    if (_mprog) return true;
    if (!glx::init()) return false;
    std::string vs_src = std::string("#version 150\n") + kProj + render::fx_glsl() + kMeshVert;
    std::string fs_src = std::string("#version 150\n") + kProj + kClip + render::fx_glsl() +
                         kMeshFrag;
    GLuint vs = compile(GL_VERTEX_SHADER, vs_src.c_str());
    GLuint fs = compile(GL_FRAGMENT_SHADER, fs_src.c_str());
    if (!vs || !fs) return false;
    _mprog = glx::CreateProgram();
    glx::AttachShader(_mprog, vs);
    glx::AttachShader(_mprog, fs);
    glx::BindAttribLocation(_mprog, 0, "a_pos");
    glx::BindAttribLocation(_mprog, 1, "a_nrm");
    glx::BindAttribLocation(_mprog, 2, "a_col");
    glx::BindAttribLocation(_mprog, 3, "a_uv");
    glx::LinkProgram(_mprog);
    glx::DeleteShader(vs);
    glx::DeleteShader(fs);
    GLint ok = 0;
    glx::GetProgramiv(_mprog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glx::GetProgramInfoLog(_mprog, sizeof log, nullptr, log);
        std::fprintf(stderr, "[preview] mesh link error: %s\n", log);
        glx::DeleteProgram(_mprog);
        _mprog = 0;
        return false;
    }
    _mu_view = glx::GetUniformLocation(_mprog, "u_view");
    _mu_model = glx::GetUniformLocation(_mprog, "u_model");
    _mu_s = glx::GetUniformLocation(_mprog, "u_s");
    _mu_zrange = glx::GetUniformLocation(_mprog, "u_zrange");
    _mu_vp = glx::GetUniformLocation(_mprog, "u_vp");
    _mu_mode = glx::GetUniformLocation(_mprog, "u_mode");
    _mu_tex = glx::GetUniformLocation(_mprog, "u_tex");
    _mu_color_on = glx::GetUniformLocation(_mprog, "u_color_on");
    _mu_shade = glx::GetUniformLocation(_mprog, "u_shade");
    _mu_flat = glx::GetUniformLocation(_mprog, "u_flat");
    style_locations(1, _mprog);
    return true;
}

// Ground-plane grid + axes, generated in the TRAINING (saved-splat) frame
// so lines mark round coordinates of the exported model, then mapped into
// the normalized frame the preview renders in (_t2n). Minor cells at a
// power of 10 from the current view distance (zoom-adaptive), brighter
// lines every 10th; the finite line patch follows the nav target (snapped
// to the lattice, with hysteresis) so the grid stays under the viewer at
// any zoom, while the positive X/Y/Z half-axes stay anchored at the frame
// origin. Cell edges are subdivided so lines stay curved (and seam-safe)
// under the nonlinear preview projections.
void PreviewRenderer::ensure_grid(float scene_radius, float view_dist,
                                  const float target_norm[3]) {
    // Normalized-frame inputs -> train units, where the cell decade lives.
    const float a = _t2n_scale > 0.0f ? _t2n_scale : 1.0f;
    const float dist_t = std::max(view_dist, 1e-6f) / a;
    const float radius_t = std::max(scene_radius, 1e-6f) / a;
    float s = std::pow(10.0f, std::floor(std::log10(dist_t / 2.0f)));
    int half = (int)std::ceil(radius_t / s);
    half = std::clamp(half, 20, 100);
    // Orbit target -> train frame (inverse of the _t2n similarity:
    // M = aR|t  =>  p_t = M^T (p_n - t) / a^2).
    float d[3] = {target_norm[0] - _t2n[3], target_norm[1] - _t2n[7],
                  target_norm[2] - _t2n[11]};
    float tx = (_t2n[0]*d[0] + _t2n[4]*d[1] + _t2n[8]*d[2]) / (a*a);
    float ty = (_t2n[1]*d[0] + _t2n[5]*d[1] + _t2n[9]*d[2]) / (a*a);
    // Recenter only when the target drifts toward the patch edge, so slow
    // pans don't rebuild the VBO every frame.
    bool recenter = std::abs(tx - _grid_center[0]) > 0.25f * half * s ||
                    std::abs(ty - _grid_center[1]) > 0.25f * half * s;
    if (_vao_grid && s == _grid_spacing && half == _grid_half && !recenter)
        return;
    const long gx0 = std::lround(tx / s), gy0 = std::lround(ty / s);
    const float cx = gx0 * s, cy = gy0 * s;
    constexpr int kSub = 4;                // sub-segments per cell edge
    const float kMinor[3] = {0.28f, 0.30f, 0.33f};
    const float kMajor[3] = {0.45f, 0.47f, 0.50f};
    const float kAxis[3][3] = {{0.98f, 0.20f, 0.31f},    // +X
                               {0.55f, 0.86f, 0.00f},    // +Y
                               {0.16f, 0.55f, 0.98f}};   // +Z
    std::vector<VL> g;
    g.reserve(((size_t)(4*half + 2) * 2*half + 3*half) * 2 * kSub);
    auto vert = [&](float x, float y, float z, const float c[3]) {
        // train -> normalized (the preview's world frame)
        g.push_back({_t2n[0]*x + _t2n[1]*y + _t2n[2]*z  + _t2n[3],
                     _t2n[4]*x + _t2n[5]*y + _t2n[6]*z  + _t2n[7],
                     _t2n[8]*x + _t2n[9]*y + _t2n[10]*z + _t2n[11],
                     c[0], c[1], c[2], 0, 0, 0});
    };
    auto seg = [&](float ax, float ay, float az, float bx, float by, float bz,
                   const float c[3]) {
        for (int k = 0; k < kSub; k++) {
            float t0 = (float)k / kSub, t1 = (float)(k + 1) / kSub;
            vert(ax + (bx-ax)*t0, ay + (by-ay)*t0, az + (bz-az)*t0, c);
            vert(ax + (bx-ax)*t1, ay + (by-ay)*t1, az + (bz-az)*t1, c);
        }
    };
    for (int i = -half; i <= half; i++) {
        // Major/minor from the GLOBAL line index, not the patch-local one.
        const float* cX = ((gx0 + i) % 10 == 0) ? kMajor : kMinor;
        const float* cY = ((gy0 + i) % 10 == 0) ? kMajor : kMinor;
        for (int j = -half; j < half; j++) {
            seg(cx + i*s, cy + j*s, 0, cx + i*s, cy + (j+1)*s, 0, cX);
            seg(cx + j*s, cy + i*s, 0, cx + (j+1)*s, cy + i*s, 0, cY);
        }
    }
    // Positive half-axes drawn last so they win the equal-depth tie against
    // the grid lines they overlap.
    for (int a2 = 0; a2 < 3; a2++)
        for (int j = 0; j < half; j++) {
            float p0[3] = {0, 0, 0}, p1[3] = {0, 0, 0};
            p0[a2] = j*s;
            p1[a2] = (j+1)*s;
            seg(p0[0], p0[1], p0[2], p1[0], p1[1], p1[2], kAxis[a2]);
        }

    fill_line_deltas(g, /*delta_from_aux=*/false);

    if (!_vao_grid) {
        GLuint va = 0, vb = 0;
        glx::GenVertexArrays(1, &va);
        glx::GenBuffers(1, &vb);
        _vao_grid = va;
        _vbo_grid = vb;
        glx::BindVertexArray(va);
        glx::BindBuffer(GL_ARRAY_BUFFER, vb);
        glx::EnableVertexAttribArray(0);
        glx::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VL), (void*)0);
        glx::EnableVertexAttribArray(1);
        glx::VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VL),
                                 (void*)(3 * sizeof(float)));
        glx::EnableVertexAttribArray(2);
        glx::VertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(VL),
                                 (void*)(6 * sizeof(float)));
    } else {
        glx::BindVertexArray(_vao_grid);
        glx::BindBuffer(GL_ARRAY_BUFFER, (GLuint)_vbo_grid);
    }
    glx::BufferData(GL_ARRAY_BUFFER, (glx::glSizeiptr)(g.size() * sizeof(VL)),
                    g.data(), GL_STATIC_DRAW);
    glx::BindVertexArray(0);
    _num_grid_verts = (int64_t)g.size();
    _grid_spacing = s;
    _grid_half = half;
    _grid_center[0] = cx;
    _grid_center[1] = cy;
}

bool PreviewRenderer::build(const spirula::TrainerSession& session) {
    return build(session.ds, session.post);
}

bool PreviewRenderer::build(const meshing::MeshData& mesh,
                            const float to_normalized[12]) {
    destroy_gl();
    // Both programs: the mesh has its own, but the grid and the axes draw
    // with the line program like everything else.
    if (!ensure_mesh_program() || !ensure_program()) return false;
    if (mesh.V.empty() || mesh.F.empty()) return false;

    // The similarity into the navigated frame, or identity. Same 3x4
    // row-major layout the point path keeps in _t2n, so the grid generated
    // there works over a mesh unchanged.
    const float ident[12] = {1,0,0,0, 0,1,0,0, 0,0,1,0};
    const float* A = to_normalized ? to_normalized : ident;
    for (int i = 0; i < 12; i++) _t2n[i] = A[i];
    _t2n_scale = std::sqrt(A[0]*A[0] + A[4]*A[4] + A[8]*A[8]);
    if (!(_t2n_scale > 1e-20f)) _t2n_scale = 1.0f;

    // Normals are what makes a shaded mesh readable; a file without them
    // gets them here rather than rendering flat.
    std::vector<std::array<float, 3>> gen_n;
    const std::vector<std::array<float, 3>>* N = &mesh.N;
    if (mesh.N.size() != mesh.V.size()) {
        meshing::MeshData tmp;
        tmp.V = mesh.V;
        tmp.F = mesh.F;
        meshing::mesh_compute_normals(tmp);
        gen_n = std::move(tmp.N);
        N = &gen_n;
    }

    const bool has_c = mesh.C.size() == mesh.V.size();
    const bool has_uv = mesh.UV.size() == mesh.V.size() &&
                        !mesh.texture.empty() && mesh.tex_width > 0 &&
                        mesh.tex_height > 0;
    _mesh_mode = has_uv ? 2 : (has_c ? 1 : 0);

    struct MV { float px, py, pz, nx, ny, nz, r, g, b, u, v; };
    std::vector<MV> verts(mesh.V.size());
    for (size_t i = 0; i < mesh.V.size(); i++) {
        const auto& p = mesh.V[i];
        MV& o = verts[i];
        o.px = A[0]*p[0] + A[1]*p[1] + A[2]*p[2] + A[3];
        o.py = A[4]*p[0] + A[5]*p[1] + A[6]*p[2] + A[7];
        o.pz = A[8]*p[0] + A[9]*p[1] + A[10]*p[2] + A[11];
        const auto& n = (*N)[i];
        // A similarity leaves directions' orientation alone up to the scale,
        // which normalizing removes.
        float nx = A[0]*n[0] + A[1]*n[1] + A[2]*n[2];
        float ny = A[4]*n[0] + A[5]*n[1] + A[6]*n[2];
        float nz = A[8]*n[0] + A[9]*n[1] + A[10]*n[2];
        const float len = std::sqrt(nx*nx + ny*ny + nz*nz);
        const float s = len > 1e-20f ? 1.0f / len : 0.0f;
        o.nx = nx * s; o.ny = ny * s; o.nz = nz * s;
        if (has_c) {
            o.r = mesh.C[i][0] / 255.0f;
            o.g = mesh.C[i][1] / 255.0f;
            o.b = mesh.C[i][2] / 255.0f;
        } else {
            o.r = o.g = o.b = 0.72f;
        }
        if (has_uv) { o.u = mesh.UV[i][0]; o.v = mesh.UV[i][1]; }
        else { o.u = o.v = 0.0f; }
    }

    std::vector<uint32_t> idx;
    idx.reserve(mesh.F.size() * 3);
    for (const auto& f : mesh.F) {
        idx.push_back((uint32_t)f[0]);
        idx.push_back((uint32_t)f[1]);
        idx.push_back((uint32_t)f[2]);
    }
    _num_mesh_idx = (int64_t)idx.size();

    GLuint vao = 0, vbo = 0, ebo = 0;
    glx::GenVertexArrays(1, &vao);
    glx::GenBuffers(1, &vbo);
    glx::GenBuffers(1, &ebo);
    glx::BindVertexArray(vao);
    glx::BindBuffer(GL_ARRAY_BUFFER, vbo);
    glx::BufferData(GL_ARRAY_BUFFER,
                    (glx::glSizeiptr)(verts.size() * sizeof(MV)), verts.data(),
                    GL_STATIC_DRAW);
    const GLsizei st = (GLsizei)sizeof(MV);
    glx::EnableVertexAttribArray(0);
    glx::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, st, (void*)0);
    glx::EnableVertexAttribArray(1);
    glx::VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, st,
                             (void*)(3 * sizeof(float)));
    glx::EnableVertexAttribArray(2);
    glx::VertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, st,
                             (void*)(6 * sizeof(float)));
    glx::EnableVertexAttribArray(3);
    glx::VertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, st,
                             (void*)(9 * sizeof(float)));
    glx::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glx::BufferData(GL_ELEMENT_ARRAY_BUFFER,
                    (glx::glSizeiptr)(idx.size() * sizeof(uint32_t)), idx.data(),
                    GL_STATIC_DRAW);
    glx::BindVertexArray(0);
    _vao_mesh = vao;
    _vbo_mesh = vbo;
    _ebo_mesh = ebo;

    if (has_uv) {
        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, mesh.tex_width,
                     mesh.tex_height, 0, GL_RGB, GL_UNSIGNED_BYTE,
                     mesh.texture.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        _tex_mesh = tex;
    }

    // Double-click picking over the mesh vertices, exactly as over points.
    _pick_xyz.resize(verts.size() * 3);
    for (size_t i = 0; i < verts.size(); i++) {
        _pick_xyz[3*i + 0] = verts[i].px;
        _pick_xyz[3*i + 1] = verts[i].py;
        _pick_xyz[3*i + 2] = verts[i].pz;
    }
    _num_points = 0;   // the mesh replaces the cloud, it does not add to it
    _built = true;
    _gl_ok = true;
    return true;
}

bool PreviewRenderer::build(const ParsedDataset& ds, const PostSplitCameras& post,
                            const uint8_t* cam_selected, const float* cam_rgb) {
    destroy_gl();
    if (!ensure_program()) return false;

    // train -> normalized frame similarity, matching how the viewport frames
    // the scene.
    double A[16];
    dsparse::train_to_normalized_inverse(ds, A);
    auto map_pt = [&](const auto* p, float out[3]) {
        for (int r = 0; r < 3; r++)
            out[r] = (float)(A[r*4+0]*p[0] + A[r*4+1]*p[1] + A[r*4+2]*p[2] + A[r*4+3]);
    };
    // Rotation+scale part for direction vectors, and the similarity scale.
    double sA = std::sqrt(A[0]*A[0] + A[4]*A[4] + A[8]*A[8]);
    if (sA < 1e-12) sA = 1.0;
    auto map_dir = [&](const double d[3], float out[3]) {
        for (int r = 0; r < 3; r++)
            out[r] = (float)((A[r*4+0]*d[0] + A[r*4+1]*d[1] + A[r*4+2]*d[2]) / sA);
    };
    // Keep the similarity around for ensure_grid: the grid is generated in
    // the training frame and mapped into the preview's normalized frame.
    for (int r = 0; r < 12; r++) _t2n[r] = (float)A[r];
    _t2n_scale = (float)sA;

    // ---- point cloud (capped; stride-sampled) -------------------------------
    int64_t n_src = ds.points.num();
    int64_t stride = std::max<int64_t>(1, n_src / 4000000);
    std::vector<V> pts;
    pts.reserve(n_src / stride + 1);
    for (int64_t i = 0; i < n_src; i += stride) {
        V v;
        float p[3];
        map_pt(&ds.points.xyz[i*3], p);
        v.px = p[0]; v.py = p[1]; v.pz = p[2];
        v.ax = ds.points.rgb[i*3 + 0] / 255.0f;
        v.ay = ds.points.rgb[i*3 + 1] / 255.0f;
        v.az = ds.points.rgb[i*3 + 2] / 255.0f;
        pts.push_back(v);
    }
    _num_points = (int64_t)pts.size();
    // Keep the displayed points host-side for double-click picking.
    _pick_xyz.resize(pts.size() * 3);
    for (size_t i = 0; i < pts.size(); i++) {
        _pick_xyz[i*3 + 0] = pts[i].px;
        _pick_xyz[i*3 + 1] = pts[i].py;
        _pick_xyz[i*3 + 2] = pts[i].pz;
    }

    // ---- camera frusta (per-INPUT cameras, true camera model) ---------------
    // Size-1 templates in CV camera space via frustum_template (cached per
    // distinct intrinsics), rotated into the normalized frame. Bright verts
    // (border + anchors) first, dim interior gridlines after, so render()
    // can draw the two ranges with different colors.
    std::vector<VL> hot, bright, dim;
    // Per-camera colours: a vertex list per distinct colour (quantized to 8
    // bits), the dim gridlines at half that colour in their own group.
    std::unordered_map<uint32_t, std::vector<VL>> by_color;
    std::vector<uint32_t> color_order;
    auto color_key = [](const float* c, float mul) {
        uint32_t k = 0;
        for (int j = 0; j < 3; j++)
            k = (k << 8) | (uint32_t)std::lround(std::clamp(c[j] * mul, 0.0f, 1.0f) * 255.0f);
        return k;
    };
    auto color_list = [&](uint32_t key) -> std::vector<VL>& {
        auto it = by_color.find(key);
        if (it == by_color.end()) {
            color_order.push_back(key);
            it = by_color.emplace(key, std::vector<VL>()).first;
        }
        return it->second;
    };
    std::unordered_map<std::string, FrustumTemplate> templates;
    for (int64_t i = 0; i < ds.num_cameras; i++) {
        const bool selected = cam_selected && cam_selected[i];
        const uint32_t key_bright = cam_rgb ? color_key(&cam_rgb[i * 3], 1.0f) : 0;
        const uint32_t key_dim = cam_rgb ? color_key(&cam_rgb[i * 3], 0.5f) : 0;
        const float* M = &ds.c2w[i*12];
        float c[3];
        float t[3] = {M[3], M[7], M[11]};
        map_pt(t, c);
        // Column basis (OpenGL convention; -Z forward), mapped + unit-scaled.
        float X[3], Y[3], Z[3];
        double dx[3] = {M[0], M[4], M[8]};
        double dy[3] = {M[1], M[5], M[9]};
        double dz[3] = {M[2], M[6], M[10]};
        map_dir(dx, X); map_dir(dy, Y); map_dir(dz, Z);

        int model = ds.camera_models.empty() ? M_PINHOLE : (int)ds.camera_models[i];
        int tier = ds.camera_distortions.empty()
                       ? (int)CameraDistortionType::None : ds.camera_distortions[i];
        float fx = ds.intrins[i*4 + 0], fy = ds.intrins[i*4 + 1];
        float cx = ds.intrins[i*4 + 2], cy = ds.intrins[i*4 + 3];
        static const float kZeroDist[kCameraDistortionParams] = {};
        const float* dist = ds.dist_coeffs.empty() ? kZeroDist
                                                   : &ds.dist_coeffs[i*kCameraDistortionParams];
        char key[256];
        std::snprintf(key, sizeof key, "%d|%d|%d|%d|%.3f|%.3f|%.3f|%.3f",
                      model, tier, ds.widths[i], ds.heights[i], fx, fy, cx, cy);
        std::string k = key;
        for (int j = 0; j < kCameraDistortionParams; j++) {
            std::snprintf(key, sizeof key, "|%g", dist[j]);
            k += key;
        }
        auto it = templates.find(k);
        if (it == templates.end())
            it = templates.emplace(k, frustum_template(model, tier, ds.widths[i],
                     ds.heights[i], fx, fy, cx, cy, dist)).first;
        const FrustumTemplate& tmpl = it->second;

        // CV cam-space point -> normalized-frame offset: the CV cam->world
        // rotation is the OpenGL c2w basis with the y,z columns negated.
        auto emit = [&](std::vector<VL>& out, const P3& p) {
            VL v = {};
            v.px = c[0]; v.py = c[1]; v.pz = c[2];
            v.ax = p.x*X[0] - p.y*Y[0] - p.z*Z[0];
            v.ay = p.x*X[1] - p.y*Y[1] - p.z*Z[1];
            v.az = p.x*X[2] - p.y*Y[2] - p.z*Z[2];
            out.push_back(v);
        };
        for (const FrustumLine& line : tmpl.lines) {
            std::vector<VL>& out = cam_rgb ? color_list(line.dim ? key_dim : key_bright)
                                           : (selected ? hot : (line.dim ? dim : bright));
            size_t n = line.pts.size();
            for (size_t j = 0; j + 1 < n; j++) {
                emit(out, line.pts[j]);
                emit(out, line.pts[j+1]);
            }
            if (line.closed && n > 1) {
                emit(out, line.pts[n-1]);
                emit(out, line.pts[0]);
            }
        }
        // Anchor lines: apex -> corner / view direction, subdivided so they
        // curve correctly under nonlinear display projections.
        for (const P3& p : tmpl.anchors) {
            std::vector<VL>& out = cam_rgb ? color_list(key_bright) : (selected ? hot : bright);
            for (int j = 0; j < kASeg; j++) {
                emit(out, {p.x*j/kASeg, p.y*j/kASeg, p.z*j/kASeg});
                emit(out, {p.x*(j+1)/kASeg, p.y*(j+1)/kASeg, p.z*(j+1)/kASeg});
            }
        }
    }
    _num_cam_sel = (int64_t)hot.size();
    _num_cam_bright = (int64_t)bright.size();
    std::vector<VL> cams = std::move(hot);
    cams.insert(cams.end(), bright.begin(), bright.end());
    cams.insert(cams.end(), dim.begin(), dim.end());
    _cam_groups.clear();
    for (uint32_t key : color_order) {
        const std::vector<VL>& v = by_color[key];
        CamGroup g;
        g.first = (int64_t)cams.size();
        g.count = (int64_t)v.size();
        for (int j = 0; j < 3; j++) g.rgb[j] = ((key >> (8 * (2 - j))) & 255) / 255.0f;
        _cam_groups.push_back(g);
        cams.insert(cams.end(), v.begin(), v.end());
    }
    _num_cam_verts = (int64_t)cams.size();
    fill_line_deltas(cams, /*delta_from_aux=*/true);

    _base_cam_size = viewer_camera_size_heuristic(post) * (float)sA;

    auto make_vao = [&](unsigned& vao, unsigned& vbo, const void* data,
                        size_t bytes, size_t stride, bool with_delta) {
        GLuint va = 0, vb = 0;
        glx::GenVertexArrays(1, &va);
        glx::GenBuffers(1, &vb);
        glx::BindVertexArray(va);
        glx::BindBuffer(GL_ARRAY_BUFFER, vb);
        glx::BufferData(GL_ARRAY_BUFFER, (glx::glSizeiptr)bytes, data,
                        GL_STATIC_DRAW);
        glx::EnableVertexAttribArray(0);
        glx::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, (int)stride, (void*)0);
        glx::EnableVertexAttribArray(1);
        glx::VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, (int)stride,
                                 (void*)(3 * sizeof(float)));
        if (with_delta) {
            glx::EnableVertexAttribArray(2);
            glx::VertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, (int)stride,
                                     (void*)(6 * sizeof(float)));
        }
        // else: attribute 2 stays disabled -> the generic (0,0,0) value,
        // so a_delta = 0 and the seam kill is a no-op (points).
        glx::BindVertexArray(0);
        vao = va;
        vbo = vb;
    };
    make_vao(_vao_pts, _vbo_pts, pts.data(), pts.size() * sizeof(V),
             sizeof(V), false);
    _pts_stride = stride;
    _pts_rgb.resize(pts.size() * 3);
    for (size_t i = 0; i < pts.size(); i++) {
        _pts_rgb[i * 3] = pts[i].ax;
        _pts_rgb[i * 3 + 1] = pts[i].ay;
        _pts_rgb[i * 3 + 2] = pts[i].az;
    }
    _pts_tinted = false;
    make_vao(_vao_cam, _vbo_cam, cams.data(), cams.size() * sizeof(VL),
             sizeof(VL), true);

    _gl_ok = true;
    _built = true;
    return true;
}

bool PreviewRenderer::pick_point(const float ro[3], const float rd[3],
                                 float out[3]) const {
    // Nearest point to the ray by angular distance (perp/t), like the web
    // viewer's ssv_ds_pick_point; accepted within a 3% cone (rd is unit).
    if (_pick_xyz.empty()) return false;
    double best_score = 1e30;
    double best_t = 0.0, best_perp = 1e30;
    for (size_t i = 0; i * 3 < _pick_xyz.size(); i++) {
        double rx = _pick_xyz[i*3+0] - ro[0];
        double ry = _pick_xyz[i*3+1] - ro[1];
        double rz = _pick_xyz[i*3+2] - ro[2];
        double t = rx*rd[0] + ry*rd[1] + rz*rd[2];
        if (t <= 0.0) continue;
        double px = rx - t*rd[0], py = ry - t*rd[1], pz = rz - t*rd[2];
        double perp = std::sqrt(px*px + py*py + pz*pz);
        double score = perp / t;
        if (score < best_score) {
            best_score = score;
            best_t = t;
            best_perp = perp;
        }
    }
    if (!(best_t > 0.0) || best_perp >= 0.03 * best_t) return false;
    for (int r = 0; r < 3; r++) out[r] = ro[r] + (float)best_t * rd[r];
    return true;
}

bool PreviewRenderer::ensure_fbo(int W, int H) {
    if (_fbo && W == _fbo_w && H == _fbo_h) return true;
    if (!_fbo) {
        GLuint fbo = 0, tex = 0, rb = 0;
        glx::GenFramebuffers(1, &fbo);
        glGenTextures(1, &tex);
        glx::GenRenderbuffers(1, &rb);
        _fbo = fbo; _color_tex = tex; _depth_rb = rb;
    }
    glBindTexture(GL_TEXTURE_2D, (GLuint)_color_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glx::BindRenderbuffer(GL_RENDERBUFFER, (GLuint)_depth_rb);
    glx::RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, W, H);
    glx::BindFramebuffer(GL_FRAMEBUFFER, (GLuint)_fbo);
    glx::FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D, (GLuint)_color_tex, 0);
    glx::FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                 GL_RENDERBUFFER, (GLuint)_depth_rb);
    bool ok = glx::CheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glx::BindFramebuffer(GL_FRAMEBUFFER, 0);
    _fbo_w = W;
    _fbo_h = H;
    return ok;
}

unsigned PreviewRenderer::render(int W, int H, const float view[16],
                                 PreviewProjection proj, float sx, float sy,
                                 float scene_radius, float view_dist,
                                 const float view_target[3], bool show_cams,
                                 float frustum_scale, bool show_grid,
                                 float ortho_back, const PreviewStyle* style) {
    if (!_built || !_gl_ok || W < 1 || H < 1) return 0;
    if (!ensure_fbo(W, H)) return 0;
    static const PreviewStyle kViewport;
    const PreviewStyle& st = style ? *style : kViewport;
    if (show_grid) ensure_grid(scene_radius, view_dist, view_target);

    // Depth is LINEAR over this range, so a near plane costs no precision;
    // what it must do is hold a camera that a placement moved a long way off.
    float zn = std::max(1e-7f, 0.002f * std::min(scene_radius, view_dist));
    float zf = std::max({10.0f * zn, 500.0f * scene_radius, 20.0f * view_dist});
    if (ortho_back > 0.0f) {
        zn = std::max(zn, ortho_back - zf);
        zf = ortho_back + zf;
    }

    glx::BindFramebuffer(GL_FRAMEBUFFER, (GLuint)_fbo);
    glViewport(0, 0, W, H);
    if (st.transparent) glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    else glClearColor(0.05f, 0.055f, 0.065f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Mesh first: it owns the depth buffer the grid and the cloud then test
    // against, and it is the only draw with its own program.
    if (_num_mesh_idx > 0 && _mprog) {
        glx::UseProgram(_mprog);
        glx::UniformMatrix4fv(_mu_view, 1, GL_TRUE, view);
        glx::Uniform1i(_mu_model, (int)proj);
        glx::Uniform2f(_mu_s, sx, sy);
        glx::Uniform2f(_mu_zrange, zn, zf);
        glx::Uniform2f(_mu_vp, (float)W, (float)H);
        glx::Uniform1i(_mu_mode, _mesh_mode);
        glx::Uniform1i(_mu_color_on, _mesh_color_on ? 1 : 0);
        glx::Uniform1i(_mu_shade, _mesh_shade ? 1 : 0);
        glx::Uniform1i(_mu_flat, _mesh_flat ? 1 : 0);
        set_style_uniforms(1, st);
        if (_mesh_mode == 2 && _mesh_color_on && _tex_mesh) {
            glx::ActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, (GLuint)_tex_mesh);
            glx::Uniform1i(_mu_tex, 0);
        }
        glx::BindVertexArray(_vao_mesh);
        glDrawElements(GL_TRIANGLES, (GLsizei)_num_mesh_idx, GL_UNSIGNED_INT,
                       nullptr);
        glx::BindVertexArray(0);
        if (_mesh_mode == 2 && _mesh_color_on && _tex_mesh)
            glBindTexture(GL_TEXTURE_2D, 0);
    }

    glx::UseProgram(_prog);
    glx::UniformMatrix4fv(_u_view, 1, GL_TRUE, view);
    glx::Uniform1i(_u_model, (int)proj);
    glx::Uniform2f(_u_s, sx, sy);
    glx::Uniform2f(_u_zrange, zn, zf);
    glx::Uniform2f(_u_vp, (float)W, (float)H);
    set_style_uniforms(0, st);
    glx::Uniform1i(_u_points, 0);
    glx::Uniform1f(_u_psize, 1.0f);
    glx::Uniform1f(_u_pradius, 0.0f);
    glx::Uniform1f(_u_alpha, 1.0f);
    glx::Uniform1f(_u_dash, 0.0f);

    // Grid + axes (aux = vertex color; depth-tested like everything else;
    // a_delta in position units -> u_dscale = 1).
    if (show_grid && _num_grid_verts > 0) {
        glx::Uniform1f(_u_scale, 0.0f);
        glx::Uniform1f(_u_dscale, 1.0f);
        glx::Uniform4f(_u_color, 0, 0, 0, 0);
        glx::BindVertexArray(_vao_grid);
        glDrawArrays(GL_LINES, 0, (GLsizei)_num_grid_verts);
    }

    // Point cloud (aux = vertex color; a_delta disabled -> seam kill no-op).
    // A soft point is blended rather than depth-written: splats of it
    // overlap, and a hard edge from the depth test is what it is avoiding.
    const bool soft = st.point_shape == 2;
    glEnable(GL_PROGRAM_POINT_SIZE);
    glx::Uniform1i(_u_points, st.point_shape + 1);
    glx::Uniform1f(_u_psize, std::max(st.point_px, 1.0f));
    glx::Uniform1f(_u_pradius, st.point_shape == 3 ? st.point_radius : 0.0f);
    if (soft) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
    }
    glx::Uniform1f(_u_scale, 0.0f);
    glx::Uniform1f(_u_dscale, 0.0f);
    glx::Uniform4f(_u_color, 0, 0, 0, 0);
    glx::BindVertexArray(_vao_pts);
    glDrawArrays(GL_POINTS, 0, (GLsizei)_num_points);
    if (soft) {
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }
    glDisable(GL_PROGRAM_POINT_SIZE);
    glx::Uniform1i(_u_points, 0);
    glx::Uniform1f(_u_pradius, 0.0f);

    // Camera frusta (aux = offset): bright border/anchor range, then the
    // dimmed interior gridlines of wide (dome/globe) cameras. a_delta is in
    // offset units -> u_dscale = the live frustum scale.
    if (show_cams && _num_cam_verts > 0) {
        float fs = _base_cam_size * std::max(frustum_scale, 1e-3f);
        glx::Uniform1f(_u_scale, fs);
        glx::Uniform1f(_u_dscale, fs);
        glx::BindVertexArray(_vao_cam);
        // Selected first, in a colour the frusta are not already drawn in:
        // theirs is orange, which is what a selected POINT is tinted.
        if (_num_cam_sel > 0) {
            glx::Uniform4f(_u_color, 0.25f, 0.92f, 1.0f, 1.0f);
            glDrawArrays(GL_LINES, 0, (GLsizei)_num_cam_sel);
        }
        glx::Uniform4f(_u_color, 1.0f, 0.62f, 0.25f, 1.0f);
        glDrawArrays(GL_LINES, (GLint)_num_cam_sel, (GLsizei)_num_cam_bright);
        int64_t grouped = 0;
        for (const CamGroup& g : _cam_groups) grouped += g.count;
        const int64_t rest = _num_cam_verts - _num_cam_sel - _num_cam_bright - grouped;
        if (rest > 0) {
            glx::Uniform4f(_u_color, 0.5f, 0.31f, 0.125f, 1.0f);
            glDrawArrays(GL_LINES, (GLint)(_num_cam_sel + _num_cam_bright),
                         (GLsizei)rest);
        }
        for (const CamGroup& g : _cam_groups) {
            glx::Uniform4f(_u_color, g.rgb[0], g.rgb[1], g.rgb[2], 1.0f);
            glDrawArrays(GL_LINES, (GLint)g.first, (GLsizei)g.count);
        }
    }

    if (_ov_visible && _ov) draw_overlay(view);

    glx::BindVertexArray(0);
    glx::UseProgram(0);
    glDisable(GL_DEPTH_TEST);
    glx::BindFramebuffer(GL_FRAMEBUFFER, 0);
    return _color_tex;
}

void PreviewRenderer::destroy_mesh_gl() {
    if (_vbo_mesh) {
        GLuint b[2] = {(GLuint)_vbo_mesh, (GLuint)_ebo_mesh};
        glx::DeleteBuffers(2, b);
    }
    if (_vao_mesh) {
        GLuint a = (GLuint)_vao_mesh;
        glx::DeleteVertexArrays(1, &a);
    }
    if (_tex_mesh) {
        GLuint t = (GLuint)_tex_mesh;
        glDeleteTextures(1, &t);
    }
    _vbo_mesh = _ebo_mesh = _vao_mesh = _tex_mesh = 0;
    _num_mesh_idx = 0;
    _mesh_mode = 0;
}

void PreviewRenderer::set_overlay(std::shared_ptr<const spirula::RegionOverlay> ov, bool visible) {
    _ov = std::move(ov);
    _ov_visible = visible;
}

void PreviewRenderer::dim_points_outside(std::shared_ptr<const std::vector<uint8_t>> flags, bool on) {
    on = on && flags;
    if (!_vbo_pts || _pts_rgb.empty() || (!on && !_pts_tinted) ||
        (on && _pts_tinted && flags == _tint_flags))
        return;
    static const std::vector<uint8_t> none;
    const std::vector<uint8_t>& inside = flags ? *flags : none;
    std::vector<V> pts(_pts_rgb.size() / 3);
    for (size_t i = 0; i < pts.size(); i++) {
        V& v = pts[i];
        v.px = _pick_xyz[i * 3];
        v.py = _pick_xyz[i * 3 + 1];
        v.pz = _pick_xyz[i * 3 + 2];
        float c[3] = {_pts_rgb[i * 3], _pts_rgb[i * 3 + 1], _pts_rgb[i * 3 + 2]};
        const size_t src = i * (size_t)_pts_stride;
        if (on && src < inside.size() && !inside[src]) {
            const float g = 0.3f * (0.299f * c[0] + 0.587f * c[1] + 0.114f * c[2]);
            for (float& x : c) x = 0.15f * x + 0.85f * g;
        }
        v.ax = c[0];
        v.ay = c[1];
        v.az = c[2];
    }
    glx::BindBuffer(GL_ARRAY_BUFFER, (GLuint)_vbo_pts);
    glx::BufferData(GL_ARRAY_BUFFER, (glx::glSizeiptr)(pts.size() * sizeof(V)), pts.data(), GL_STATIC_DRAW);
    glx::BindBuffer(GL_ARRAY_BUFFER, 0);
    _pts_tinted = on;
    _tint_flags = on ? flags : nullptr;
}

void PreviewRenderer::destroy_overlay_gl() {
    GLuint b[2] = {(GLuint)_vbo_ov, (GLuint)_vbo_ovl};
    GLuint a[2] = {(GLuint)_vao_ov, (GLuint)_vao_ovl};
    if (_vbo_ov) glx::DeleteBuffers(2, b);
    if (_vao_ov) glx::DeleteVertexArrays(2, a);
    _vbo_ov = _vbo_ovl = _vao_ov = _vao_ovl = 0;
    _ov_uploaded.reset();
    _ov_local.reset();
}

// Into the normalized frame the preview draws in, like the points.
void PreviewRenderer::upload_overlay() {
    destroy_overlay_gl();
    _ov_local = std::make_shared<spirula::RegionOverlay>(*_ov);
    std::vector<VL> tris;
    _ov_first.clear();
    _ov_dash.clear();
    for (spirula::RegionOverlay::Layer& l : _ov_local->layers) {
        for (size_t v = 0; v < l.xyz.size(); v += 3) {
            const float p[3] = {l.xyz[v], l.xyz[v + 1], l.xyz[v + 2]};
            for (int r = 0; r < 3; r++)
                l.xyz[v + r] = _t2n[r * 4] * p[0] + _t2n[r * 4 + 1] * p[1] + _t2n[r * 4 + 2] * p[2] + _t2n[r * 4 + 3];
        }
        _ov_first.push_back((int64_t)tris.size());
        for (uint32_t i : l.tri) {
            VL v{};
            v.px = l.xyz[i * 3];
            v.py = l.xyz[i * 3 + 1];
            v.pz = l.xyz[i * 3 + 2];
            tris.push_back(v);
        }
        double len = 0;
        for (size_t e = 0; e < l.edge.size(); e += 2) {
            const float* a = &l.xyz[l.edge[e] * 3];
            const float* b = &l.xyz[l.edge[e + 1] * 3];
            len += std::sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) +
                             (a[2] - b[2]) * (a[2] - b[2]));
        }
        _ov_dash.push_back(l.edge.empty() ? 0.0f : (float)(1.5 * len / (double)(l.edge.size() / 2)));
    }
    _ov_first.push_back((int64_t)tris.size());
    for (int pass = 0; pass < 2; pass++) {
        GLuint va = 0, vb = 0;
        glx::GenVertexArrays(1, &va);
        glx::GenBuffers(1, &vb);
        glx::BindVertexArray(va);
        glx::BindBuffer(GL_ARRAY_BUFFER, vb);
        if (pass == 0)
            glx::BufferData(GL_ARRAY_BUFFER, (glx::glSizeiptr)(tris.size() * sizeof(VL)), tris.data(),
                            GL_STATIC_DRAW);
        for (int k = 0; k < 3; k++) {
            glx::EnableVertexAttribArray(k);
            glx::VertexAttribPointer(k, 3, GL_FLOAT, GL_FALSE, sizeof(VL), (void*)(3 * k * sizeof(float)));
        }
        (pass == 0 ? _vao_ov : _vao_ovl) = va;
        (pass == 0 ? _vbo_ov : _vbo_ovl) = vb;
    }
    glx::BindVertexArray(0);
    _ov_uploaded = _ov;
}

// A translucent fill that writes no depth, then the silhouette dashed; the
// outline follows the eye, so it is rebuilt every frame.
void PreviewRenderer::draw_overlay(const float view[16]) {
    if (_ov != _ov_uploaded) upload_overlay();
    if (!_ov_local || !_vao_ov) return;
    float eye[3];
    for (int c = 0; c < 3; c++)
        eye[c] = -(view[0 * 4 + c] * view[3] + view[1 * 4 + c] * view[7] + view[2 * 4 + c] * view[11]);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glx::Uniform1f(_u_scale, 0.0f);
    glx::Uniform1f(_u_dscale, 0.0f);
    const std::vector<spirula::RegionOverlay::Layer>& layers = _ov_local->layers;
    const float fill = layers.size() > 1 ? 0.12f : 0.2f;
    glx::BindVertexArray(_vao_ov);
    for (size_t k = 0; k < layers.size(); k++) {
        glx::Uniform4f(_u_color, layers[k].rgb[0], layers[k].rgb[1], layers[k].rgb[2], 1.0f);
        glx::Uniform1f(_u_alpha, fill);
        glDrawArrays(GL_TRIANGLES, (GLint)_ov_first[k], (GLsizei)(_ov_first[k + 1] - _ov_first[k]));
    }
    std::vector<VL> lines;
    std::vector<int64_t> first;
    std::vector<float> seg;
    for (const spirula::RegionOverlay::Layer& l : layers) {
        first.push_back((int64_t)lines.size());
        spirula::region_outline(l, eye, seg);
        for (size_t i = 0; i + 6 <= seg.size(); i += 6) {
            const float* a = &seg[i];
            const float* b = &seg[i + 3];
            const float len = std::sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) +
                                        (a[2] - b[2]) * (a[2] - b[2]));
            VL va{}, vb{};
            va.px = a[0]; va.py = a[1]; va.pz = a[2];
            vb.px = b[0]; vb.py = b[1]; vb.pz = b[2];
            vb.ax = len;
            lines.push_back(va);
            lines.push_back(vb);
        }
    }
    first.push_back((int64_t)lines.size());
    glx::BindVertexArray(_vao_ovl);
    glx::BindBuffer(GL_ARRAY_BUFFER, (GLuint)_vbo_ovl);
    glx::BufferData(GL_ARRAY_BUFFER, (glx::glSizeiptr)(lines.size() * sizeof(VL)), lines.data(), GL_STREAM_DRAW);
    for (size_t k = 0; k < layers.size(); k++) {
        glx::Uniform4f(_u_color, layers[k].rgb[0], layers[k].rgb[1], layers[k].rgb[2], 1.0f);
        glx::Uniform1f(_u_alpha, 0.95f);
        glx::Uniform1f(_u_dash, _ov_dash[k]);
        glDrawArrays(GL_LINES, (GLint)first[k], (GLsizei)(first[k + 1] - first[k]));
    }
    glx::Uniform1f(_u_alpha, 1.0f);
    glx::Uniform1f(_u_dash, 0.0f);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void PreviewRenderer::destroy_gl() {
    destroy_overlay_gl();
    destroy_mesh_gl();
    if (_vbo_pts) { GLuint b[2] = {(GLuint)_vbo_pts, (GLuint)_vbo_cam}; glx::DeleteBuffers(2, b); }
    if (_vao_pts) { GLuint a[2] = {(GLuint)_vao_pts, (GLuint)_vao_cam}; glx::DeleteVertexArrays(2, a); }
    _vbo_pts = _vbo_cam = _vao_pts = _vao_cam = 0;
    if (_vbo_grid) { GLuint b = (GLuint)_vbo_grid; glx::DeleteBuffers(1, &b); }
    if (_vao_grid) { GLuint a = (GLuint)_vao_grid; glx::DeleteVertexArrays(1, &a); }
    _vbo_grid = _vao_grid = 0;
    _num_grid_verts = 0;
    _grid_spacing = 0.0f;
    _grid_half = 0;
    _grid_center[0] = _grid_center[1] = 0.0f;
    if (_fbo) {
        GLuint fbo = (GLuint)_fbo;
        glx::DeleteFramebuffers(1, &fbo);
        GLuint tex = (GLuint)_color_tex;
        glDeleteTextures(1, &tex);
        GLuint rb = (GLuint)_depth_rb;
        glx::DeleteRenderbuffers(1, &rb);
        _fbo = _color_tex = _depth_rb = 0;
        _fbo_w = _fbo_h = 0;
    }
    _built = false;
    _num_points = _num_cam_verts = _num_cam_sel = _num_cam_bright = 0;
    _cam_groups.clear();
}

}  // namespace gui

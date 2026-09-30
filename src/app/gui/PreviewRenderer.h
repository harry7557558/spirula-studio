#pragma once

// PreviewRenderer draws sparse points, camera frusta, and mesh previews in an
// offscreen OpenGL framebuffer. Geometry stays in the dataset's normalized
// frame; ViewportPanel selects the navigation up axis. Its projection shader
// matches the engine viewer's pinhole and fisheye models.

#include "app/gui/render/TransitionFx.h"
#include "data/DatasetParser.h"
#include "mesh/MeshExport.h"   // meshing::MeshData

#include <cstdint>
#include <memory>
#include <vector>

namespace spirula { class TrainerSession; struct RegionOverlay; }

namespace gui {

// Projection selector for render() (matches ViewportPanel's camera-model
// dropdown order / the web client's #camera-model).
enum class PreviewProjection {
    Pinhole = 0, Fisheye, Equisolid, Equirectangular
};

// How a frame for a FILE is drawn, as opposed to one for the viewport
// (app/gui/render/). Every default is what the viewport already does.
struct PreviewStyle {
    // Cleared to nothing rather than to the viewport's grey, so the frame
    // composites: alpha is 1 where something was drawn.
    bool transparent = false;
    // The cloud: 0 square, 1 circle, 2 gaussian, 3 sphere of `point_radius`
    // (normalized frame); the screen shapes are `point_px` across.
    int point_shape = 0;
    float point_px = 2.0f;
    float point_radius = 0.0f;
    // Lens distortion: CameraDistortionType and its coefficients.
    int tier = 0;
    float dist[8] = {};
    // Nothing past the plane n.p = d (normalized frame) is drawn, and the
    // `glow` before it lights up: the sweep reveal.
    bool clip = false;
    float plane[4] = {0, 0, 1, 0};
    float glow = 0.0f;
    float glow_col[3] = {1.0f, 0.86f, 0.6f};
    // A 3D transition moving the cloud's points or the mesh's vertices
    // (render/TransitionFx.h): its kind, side, time, settings and scene, the
    // scene in the normalized frame.
    int fx = 0;
    bool fx_in = false;
    float fx_t = 0.0f;
    float fx_p[2] = {0.0f, 0.0f};
    gui::render::FxGeo fx_geo;
};

class PreviewRenderer {
public:
    // Build GL buffers from the parsed dataset. Requires a current GL
    // context (GUI thread) and the session's load_dataset() to have
    // completed. Returns false when GL init fails (missing functions).
    bool build(const spirula::TrainerSession& session);
    // The same over parsed data alone (a point file has no cameras, so no
    // frusta). `cam_selected` is one flag per camera, drawn in the highlight
    // colour; `cam_rgb`, [N,3] in 0..1, gives every camera its own colour instead.
    bool build(const ParsedDataset& ds, const PostSplitCameras& post,
               const uint8_t* cam_selected = nullptr, const float* cam_rgb = nullptr);
    // A triangle mesh, drawn shaded instead of a point cloud. `to_normalized`
    // is the similarity that maps the mesh's own coordinates into the frame
    // the viewport navigates (scale + center, as SplatViewer computes for a
    // splat file); pass nullptr for identity. Vertex colors are used when the
    // mesh has them, the baked atlas when it has UVs and a texture, and a flat
    // grey otherwise -- in every case lit by a fixed headlight so shape reads.
    bool build(const meshing::MeshData& mesh, const float to_normalized[12]);
    bool built() const { return _built; }
    bool has_mesh() const { return _num_mesh_idx > 0; }
    int64_t num_triangles() const { return _num_mesh_idx / 3; }
    // What the mesh carries, so a caller only offers the switches that mean
    // something: 0 = geometry only, 1 = vertex colors, 2 = a texture atlas.
    int mesh_color_kind() const { return _mesh_mode; }

    // Mesh display switches (live uniforms, no rebuild). `shade` applies the
    // headlight, `flat` uses face normals instead of the interpolated vertex
    // ones, `color` shows the vertex/texture color rather than plain grey.
    void set_mesh_display(bool shade, bool flat, bool color) {
        _mesh_shade = shade;
        _mesh_flat = flat;
        _mesh_color_on = color;
    }

    // Render into the internal FBO; returns the color texture (0 on error).
    // view is a row-major 4x4 world-to-view matrix. sx/sy are the engine
    // intrinsics normalized to NDC: fx/(W/2), fy/(H/2). frustum_scale
    // multiplies the base camera size; scene_radius drives the depth range
    // and the grid extent; view_dist + view_target (nav pose, normalized
    // frame) drive the zoom-adaptive grid cell size and its patch center.
    unsigned render(int W, int H, const float view[16],
                    PreviewProjection proj, float sx, float sy,
                    float scene_radius, float view_dist,
                    const float view_target[3], bool show_cams,
                    float frustum_scale, bool show_grid,
                    // How far an orthographic view's camera was pulled back
                    // along its axis (ViewportPanel::ortho_pullback), 0 if not.
                    float ortho_back = 0.0f,
                    const PreviewStyle* style = nullptr);

    // A region's boundary drawn over the cloud (app/webviewer/RegionOverlay.h),
    // in the frame the built points are in; null for none. Survives a rebuild.
    void set_overlay(std::shared_ptr<const spirula::RegionOverlay> ov, bool visible);
    // Greys the points whose flag (one per point of the parsed cloud) is 0;
    // `on` false puts the colours back. Uploads only on a change.
    void dim_points_outside(std::shared_ptr<const std::vector<uint8_t>> flags, bool on);

    // Base frustum size (camhost::frustum_display_size, normalized frame).
    float base_camera_size() const { return _base_cam_size; }
    int64_t num_points() const { return _num_points; }

    // Double-click pick: nearest displayed point to the ray (normalized
    // frame, rd unit) by angular distance, accepted within a 3% cone --
    // port of the web viewer's ssv_ds_pick_point + datasetPointHit.
    bool pick_point(const float ro[3], const float rd[3],
                    float out[3]) const;

    void destroy_gl();

private:
    bool ensure_program();
    bool ensure_mesh_program();
    bool ensure_fbo(int W, int H);
    void destroy_mesh_gl();

    bool _built = false;
    bool _gl_ok = false;
    unsigned _prog = 0;
    int _u_view = -1, _u_scale = -1, _u_dscale = -1, _u_color = -1;
    int _u_model = -1, _u_s = -1, _u_zrange = -1, _u_vp = -1;
    // Style uniforms, one set per program: [0] lines and points, [1] mesh.
    struct StyleLoc {
        int tier = -1, dist = -1, clip_on = -1, clip = -1, glow = -1, glow_col = -1;
        int fx = -1, fx_in = -1, fx_t = -1, fx_p = -1, fx_c = -1, fx_up = -1, fx_e1 = -1,
            fx_e2 = -1, fx_radius = -1, fx_qh = -1, fx_qa = -1, fx_qr = -1;
    } _sloc[2];
    void style_locations(int program, unsigned prog);
    int _u_points = -1, _u_psize = -1, _u_pradius = -1;
    void set_style_uniforms(int program, const PreviewStyle& st);

    // Mesh program: same projection GLSL, shaded triangles.
    unsigned _mprog = 0;
    int _mu_view = -1, _mu_model = -1, _mu_s = -1, _mu_zrange = -1;
    int _mu_vp = -1, _mu_mode = -1, _mu_tex = -1;
    int _mu_color_on = -1, _mu_shade = -1, _mu_flat = -1;
    unsigned _vao_mesh = 0, _vbo_mesh = 0, _ebo_mesh = 0, _tex_mesh = 0;
    int64_t _num_mesh_idx = 0;
    // 0 = flat grey, 1 = vertex color, 2 = texture atlas
    int _mesh_mode = 0;
    bool _mesh_shade = true, _mesh_flat = false, _mesh_color_on = true;
    void ensure_grid(float scene_radius, float view_dist,
                     const float target_norm[3]);

    unsigned _vao_pts = 0, _vbo_pts = 0;
    unsigned _vao_cam = 0, _vbo_cam = 0;
    unsigned _vao_grid = 0, _vbo_grid = 0;
    int64_t _num_grid_verts = 0;
    float _grid_spacing = 0.0f;      // minor cell size (train units) built at
    int _grid_half = 0;              // half-extent (minor cells) built at
    float _grid_center[2] = {0, 0};  // patch center (train units) built at
    // train -> normalized similarity (set in build; identity by default):
    // the grid is generated in the training/saved frame so its lines mark
    // round coordinates of the exported model, then mapped into the
    // normalized frame the preview renders in.
    float _t2n[12] = {1,0,0,0, 0,1,0,0, 0,0,1,0};
    float _t2n_scale = 1.0f;         // normalized units per train unit
    int64_t _num_points = 0;
    // Host copy of the displayed (stride-sampled, normalized-frame) points
    // for double-click picking. CPU RAM only.
    std::vector<float> _pick_xyz;
    std::vector<float> _pts_rgb;   // the displayed points' own colours
    int64_t _pts_stride = 1;
    bool _pts_tinted = false;
    std::shared_ptr<const std::vector<uint8_t>> _tint_flags;
    // Frustum verts, in draw order: the selected cameras' lines, then the
    // rest's borders and anchors, then the rest's dimmed interior gridlines.
    int64_t _num_cam_verts = 0;
    int64_t _num_cam_sel = 0;
    int64_t _num_cam_bright = 0;
    // With per-camera colours instead: one draw per distinct colour.
    struct CamGroup { int64_t first, count; float rgb[3]; };
    std::vector<CamGroup> _cam_groups;
    float _base_cam_size = 0.1f;

    std::shared_ptr<const spirula::RegionOverlay> _ov, _ov_uploaded;
    std::shared_ptr<spirula::RegionOverlay> _ov_local;   // in the normalized frame
    bool _ov_visible = false;
    unsigned _vao_ov = 0, _vbo_ov = 0, _vao_ovl = 0, _vbo_ovl = 0;
    std::vector<int64_t> _ov_first;   // per layer, into the triangle buffer
    std::vector<float> _ov_dash;      // per layer, dash period
    int _u_alpha = -1, _u_dash = -1;
    void upload_overlay();
    void draw_overlay(const float view[16]);
    void destroy_overlay_gl();

    unsigned _fbo = 0, _color_tex = 0, _depth_rb = 0;
    int _fbo_w = 0, _fbo_h = 0;
};

}  // namespace gui

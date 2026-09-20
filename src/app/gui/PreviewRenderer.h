#pragma once

// PreviewRenderer draws sparse points, camera frusta, and mesh previews in an
// offscreen OpenGL framebuffer. Geometry stays in the dataset's normalized
// frame; ViewportPanel selects the navigation up axis. Its projection shader
// matches the engine viewer's pinhole and fisheye models.

#include "data/DatasetParser.h"
#include "mesh/MeshExport.h"   // meshing::MeshData

#include <cstdint>
#include <vector>

namespace spirula { class TrainerSession; }

namespace gui {

// Projection selector for render() (matches ViewportPanel's camera-model
// dropdown order / the web client's #camera-model).
enum class PreviewProjection {
    Pinhole = 0, Fisheye, Equisolid, Equirectangular
};

class PreviewRenderer {
public:
    // Build GL buffers from the parsed dataset. Requires a current GL
    // context (GUI thread) and the session's load_dataset() to have
    // completed. Returns false when GL init fails (missing functions).
    bool build(const spirula::TrainerSession& session);
    // The same, from the parsed data alone. A point cloud opened in the
    // viewer has no session behind it and no cameras at all (`post` empty,
    // ds.num_cameras == 0), which this handles: it simply draws no frusta.
    bool build(const ParsedDataset& ds, const PostSplitCameras& post);
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
                    float frustum_scale, bool show_grid);

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
    int64_t _num_cam_verts = 0;    // total frustum verts (bright then dim)
    int64_t _num_cam_bright = 0;   // border + anchor verts (drawn full-color)
    float _base_cam_size = 0.1f;

    unsigned _fbo = 0, _color_tex = 0, _depth_rb = 0;
    int _fbo_w = 0, _fbo_h = 0;
};

}  // namespace gui

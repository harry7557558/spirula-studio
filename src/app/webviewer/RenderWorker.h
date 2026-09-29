#pragma once

// RenderWorker -- the interactive render path shared by the web-viewer HTTP
// server (Viewer.cpp, which JPEG-encodes the result) and the native GUI
// viewport (gui/, which uploads it to an OpenGL texture): the c2w remap into
// the training frame, the engine render, the annotation overlay, and the
// rgb/depth/alpha/normals/median/distortion display transforms.
//
// Threading model: submit() stores the request in a single latest-wins slot
// and synchronously flips hooks.set_render_pending(true) so the training
// loop yields the engine mutex at its next iteration boundary. The worker
// thread takes hooks.engine_mutex around all engine work and publishes a
// ViewResult; callers either block (wait_result, HTTP thread) or poll
// (try_get_result, GUI frame loop).

#include "data/DatasetParser.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

// Static per-run info the render path needs (subset of model config).
namespace spirula { struct RegionOverlay; }

struct ViewerRenderConfig {
    std::string primitive = "3dgs";
    bool  packed = false;
    int   sh_degree_warmup_every = 1000;
    std::optional<float> relative_scale;
    // Median-depth buffers are rendered only when a median loss is
    // configured; requesting them otherwise 400s.
    bool  output_median = false;
    // Distortion render enabled when a distortion regularizer is configured;
    // also enabled on demand when a *_distortion buffer is requested.
    bool  distortion_reg_on = false;
    // Splat colours are stored in a linear and/or wide-gamut space, so the
    // render is converted to sRGB before display and `rgb_raw` is offered.
    bool  color_space_on = false;
    // Viewer-client c2w remap into the training frame.
    float train_frame_scale = 1.0f;
    std::array<float, 16> train_to_normalized{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    // Unscaled frustum size (the return value of viewer_upload_cameras),
    // multiplied by ViewRequest::cam_size_scale per render; 0 disables live
    // frustum-size updates.
    float base_camera_size = 0.0f;
    // Which engine scene slot this worker renders (Engine.h "Viewer scenes").
    // < 0 = whatever is bound, which is what a training session wants.
    int scene_slot = -1;
    // What the client's view orbits about and Reset frames, one point per
    // dsparse::CenterMode in the client's normalized frame. `center_cameras`
    // is false over a file, which has none, so those modes are not offered.
    dsparse::CenterTable centers{};
    bool center_cameras = false;
};

struct ViewerHooks {
    // Serializes engine access between the training loop and viewer renders.
    std::mutex* engine_mutex = nullptr;
    // Current training step (for the SH-degree warmup schedule).
    std::function<int()> current_step;
    // JSON body for /progress (built by the training loop, which owns the
    // timing state).
    std::function<std::string()> progress_json;
    // Toggle pause; returns the new paused state.
    std::function<bool()> pause_toggle;
    // "Render desired" flag: called with true synchronously from the
    // submitting thread, with false from the worker once the queue drains.
    std::function<void(bool)> set_render_pending;
};

// One interactive render request. c2w is a row-major 3x4 camera-to-world in
// the OpenGL/nerfstudio convention, expressed in the client's normalized
// frame (the worker remaps through cfg.train_to_normalized).
struct ViewRequest {
    float c2w[12] = {1,0,0,0, 0,1,0,0, 0,0,1,0};
    float fx = 500, fy = 500, cx = 256, cy = 256;
    int W = 512, H = 512;
    std::string model = "PINHOLE";
    std::string key = "rgb";
    bool show_cams = false;
    bool show_grid = false;   // axes/grid overlay (viewer_upload_grid)
    // Nav view distance (camera to orbit target) in the client's normalized
    // frame; drives the zoom-adaptive grid cell size. 0 = unknown (the grid
    // keeps its current spacing and center).
    float grid_dist = 0.0f;
    // Nav orbit target (client's normalized frame); the grid's line patch
    // recenters on it so it stays under the viewer at any zoom.
    float grid_target[3] = {0, 0, 0};
    // Frustum-size multiplier applied to cfg.base_camera_size when
    // show_cams is on (user-facing "camera size" slider).
    float cam_size_scale = 1.0f;
    // The region of interest's outline (RenderWorker::set_region_overlay),
    // pinhole views only.
    bool show_roi = false;
    // Double-click pick: when >= 0, also return the 3D point under this
    // pixel (render resolution), read from the ray-depth channel the render
    // already downloads -- no extra VRAM or render pass.
    int pick_px = -1, pick_py = -1;

    // ---- per-request overrides of the static config ----
    // A viewer looking at a finished model can legitimately want to see it
    // rendered a different way than it was trained (the web client offers
    // exactly these), and both are cheap: the primitive selects which
    // projection/raster pair runs, and the SH degree only bounds a loop.
    // They ride on the REQUEST rather than mutating the shared config so a
    // render in flight is never half-switched.
    std::string primitive;    // "" = ViewerRenderConfig::primitive
    int sh_degree = -1;       // < 0 = the warmup schedule

    // ---- a frame for a file rather than for the screen ----
    // No overlays and no display transform: ViewResult::rgba8 comes back as
    // the render premultiplied over nothing, alpha = 1 - transmittance.
    bool raw = false;
    // The lens distortion tier by name ("NONE" / "OPENCV" / "THIN_PRISM")
    // and its coefficients in that tier's order.
    std::string distortion = "NONE";
    float dist[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    // Run under the engine lock around this render, with its scene bound:
    // an effect that rewrites the splats for one frame puts them back after.
    std::function<void()> before_render, after_render;
};

struct ViewResult {
    uint64_t id = 0;
    int W = 0, H = 0;
    std::vector<uint8_t> rgb8;   // [H, W, 3]
    std::vector<uint8_t> rgba8;  // [H, W, 4], ViewRequest::raw only
    std::string error;           // non-empty on failure
    // Pick result (ViewRequest::pick_px/py): point under the pixel in the
    // client's normalized frame; pick_hit false = background / invalid ray.
    bool pick_hit = false;
    float pick_point[3] = {0, 0, 0};
    // What this render cost, including the wait for the engine mutex -- time
    // the training loop was not stepping either. Measured on the worker, not
    // across submit/try_get_result: a client that stops polling for a while
    // would otherwise charge its own absence to the render.
    double secs = 0.0;
};

class RenderWorker {
public:
    RenderWorker();
    ~RenderWorker();

    void start(ViewerRenderConfig cfg, ViewerHooks hooks);
    void stop();

    // Latest-wins submit; returns the request id.
    uint64_t submit(const ViewRequest& q);
    // Block up to timeout_s for the result of request `id`.
    bool wait_result(uint64_t id, ViewResult& out, double timeout_s);
    // Non-blocking: true when the result of request `id` is available.
    bool try_get_result(uint64_t id, ViewResult& out);
    // The same, handing the result over rather than copying it -- a frame's
    // pixels, for the one consumer that asked; 0 s does not wait.
    bool take_result(uint64_t id, ViewResult& out, double timeout_s);

    const ViewerRenderConfig& config() const;
    // Drawn over frames that ask for it (ViewRequest::show_roi), in the
    // training frame; null for none.
    void set_region_overlay(std::shared_ptr<const spirula::RegionOverlay> ov);

    // Viewable buffer keys for this config (the Python /buffers set; the
    // distortion buffers appear only when a distortion regularizer is
    // configured -- they cost full-resolution VRAM to produce).
    std::vector<std::string> buffer_keys() const;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

// One-shot upload of the post-split camera table for frustum annotation +
// thumbnails. Call after engine_setup_data_manager, before rendering with
// show_cams. Returns the frustum size, for ViewerRenderConfig::base_camera_size.
float viewer_upload_cameras(const PostSplitCameras& post);

// The frustum size alone (camhost::frustum_display_size in the training
// frame), for renderers that draw frusta without the engine.
float viewer_camera_size_heuristic(const PostSplitCameras& post);

// Unit ray direction (CV convention: x right, y down, z forward) for a
// viewer pixel, u = (px - cx)/fx, v = (py - cy)/fy. Host port of
// projection_utils generate_ray for the distortion-free display models
// 0 pinhole / 1 fisheye-equidistant / 2 equisolid / 3 equirectangular;
// false when the pixel is outside the model's domain.
bool viewer_pixel_ray(int camera_model, float u, float v, float dir[3]);

// Its inverse: a camera-space direction (need not be unit) to the same
// normalized coordinates, false behind the model's horizon. Selecting in the
// viewport is this run over every element, so the two must stay one pair.
bool viewer_ray_pixel(int camera_model, const float dir[3], float& u, float& v);

// One-shot upload of the axes/grid overlay (engine_viewer_set_grid). The
// grid is axis-aligned in the engine's training frame -- the frame splats
// are saved in -- so grid lines mark round coordinates of the exported
// model; its cell size then adapts to ViewRequest::grid_dist per render.
// Call alongside viewer_upload_cameras; drawn when ViewRequest::show_grid
// is set.
void viewer_upload_grid(const PostSplitCameras& post);

// Viewer.cpp -- see Viewer.h. The render path lives in RenderWorker.cpp.

#include "app/webviewer/Viewer.h"
#include "app/webviewer/HttpServer.h"

// Implementation lives in external/stb_image_write_impl.cpp (shared with the
// mesh texture export); declarations only here.
#include "external/stb_image_write.h"

#include "app_generated/viewer_html.h"   // kViewerHtml[] (CMake-embedded)

#include "core/Camera.h"    // camera_model_from_name

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>
#include "core/Env.h"


namespace {

void jpeg_write_cb(void* ctx, void* data, int size) {
    auto* out = (std::vector<uint8_t>*)ctx;
    out->insert(out->end(), (uint8_t*)data, (uint8_t*)data + size);
}

}  // namespace


// ===========================================================================
// Impl
// ===========================================================================

struct ViewerServer::Impl {
    ViewerHooks hooks;
    HttpServer http;
    RenderWorker worker;
    std::vector<std::string> buffer_keys;
    dsparse::CenterTable centers{};
    bool center_cameras = false;
    bool orbit_navigation = true;
    bool started = false;

    // ---- endpoint handlers -------------------------------------------------

    HttpResponse handle_index() {
        // Dev override so viewer.html edits don't need a rebuild.
        if (const char* p = spirula::env("VIEWER_HTML")) {
            std::ifstream f(p, std::ios::binary);
            if (f) {
                HttpResponse r;
                r.content_type = "text/html; charset=utf-8";
                r.body.assign(std::istreambuf_iterator<char>(f), {});
                return r;
            }
            std::fprintf(stderr, "[viewer] SS_VIEWER_HTML=%s not readable; "
                         "serving embedded copy\n", p);
        }
        HttpResponse r;
        r.content_type = "text/html; charset=utf-8";
        r.body.assign(kViewerHtml, kViewerHtml + kViewerHtmlSize);
        return r;
    }

    // Shared /render + /pick camera/request parsing; returns a non-empty
    // error message for a 400.
    std::string parse_view_request(const HttpRequest& req, ViewRequest& q) {
        q.key = req.get("buffer_key", "rgb");
        std::string c2w_str = req.get("c2w");
        {
            size_t pos = 0;
            int i = 0;
            while (i < 12 && pos <= c2w_str.size()) {
                size_t comma = c2w_str.find(',', pos);
                try {
                    q.c2w[i++] = std::stof(c2w_str.substr(
                        pos, comma == std::string::npos ? std::string::npos : comma - pos));
                } catch (...) { return "bad c2w"; }
                if (comma == std::string::npos) break;
                pos = comma + 1;
            }
            if (i != 12) return "c2w must have 12 values";
        }
        q.fx = (float)req.get_double("fx", 500);
        q.fy = (float)req.get_double("fy", 500);
        q.cx = (float)req.get_double("cx", 256);
        q.cy = (float)req.get_double("cy", 256);
        q.W  = req.get_int("width", 512);
        q.H  = req.get_int("height", 512);
        q.model = req.get("camera_model", "PINHOLE");
        q.show_cams = req.get_bool("show_training_cameras", false);
        q.show_grid = req.get_bool("show_grid", false);
        q.grid_dist = (float)req.get_double("grid_dist", 0.0);
        q.grid_target[0] = (float)req.get_double("grid_tx", 0.0);
        q.grid_target[1] = (float)req.get_double("grid_ty", 0.0);
        q.grid_target[2] = (float)req.get_double("grid_tz", 0.0);
        q.cam_size_scale = (float)req.get_double("camera_size_scale", 1.0);

        constexpr int MAX_DIM = 2160;   // prevent OOM
        if (q.W <= 0 || q.H <= 0 || std::max(q.W, q.H) > MAX_DIM)
            return "image too large";
        if (std::find(buffer_keys.begin(), buffer_keys.end(), q.key) == buffer_keys.end())
            return "unsupported buffer_key";
        if ((int)camera_model_from_name(q.model) < 0)
            return "unsupported camera_model";
        return "";
    }

    HttpResponse handle_render(const HttpRequest& req) {
        ViewRequest q;
        std::string err = parse_view_request(req, q);
        if (!err.empty()) return HttpResponse::text(400, err);
        int quality = req.get_int("jpeg_quality", 75);

        uint64_t id = worker.submit(q);
        ViewResult res;
        if (!worker.wait_result(id, res, 10.0))
            return HttpResponse::text(500, "render timeout");
        if (!res.error.empty())
            return HttpResponse::text(500, res.error);

        // JPEG encode outside the engine lock.
        std::vector<uint8_t> jpeg;
        jpeg.reserve((size_t)res.W * res.H / 4);
        if (!stbi_write_jpg_to_func(jpeg_write_cb, &jpeg, res.W, res.H, 3,
                                    res.rgb8.data(), quality))
            return HttpResponse::text(500, "JPEG encode failed");
        HttpResponse r;
        r.content_type = "image/jpeg";
        r.body = std::move(jpeg);
        return r;
    }

    // Double-click centering: render at the client's camera/resolution and
    // return the 3D point (client normalized frame) under pixel (px, py),
    // read from the depth channel -- no extra VRAM. JSON: {"hit":bool,
    // "p":[x,y,z]}.
    HttpResponse handle_pick(const HttpRequest& req) {
        ViewRequest q;
        std::string err = parse_view_request(req, q);
        if (!err.empty()) return HttpResponse::text(400, err);
        q.key = "rgb";   // picking only needs the depth+alpha side channels
        q.pick_px = req.get_int("px", -1);
        q.pick_py = req.get_int("py", -1);
        if (q.pick_px < 0 || q.pick_px >= q.W ||
            q.pick_py < 0 || q.pick_py >= q.H)
            return HttpResponse::text(400, "px/py out of range");

        uint64_t id = worker.submit(q);
        ViewResult res;
        if (!worker.wait_result(id, res, 10.0))
            return HttpResponse::text(500, "render timeout");
        if (!res.error.empty())
            return HttpResponse::text(500, res.error);
        char body[160];
        if (res.pick_hit)
            std::snprintf(body, sizeof body,
                          "{\"hit\": true, \"p\": [%.9g, %.9g, %.9g]}",
                          res.pick_point[0], res.pick_point[1],
                          res.pick_point[2]);
        else
            std::snprintf(body, sizeof body, "{\"hit\": false}");
        return HttpResponse::json(body);
    }

    HttpResponse handle_buffers() {
        std::string body = "[";
        for (size_t i = 0; i < buffer_keys.size(); i++)
            body += (i ? ", \"" : "\"") + buffer_keys[i] + "\"";
        body += "]";
        return HttpResponse::json(body);
    }

    // What the client cannot work out for itself: it holds no splats and no
    // camera table, only the normalized frame both are expressed in.
    HttpResponse handle_scene() {
        std::string body = "{\"center_modes\": [";
        for (int i = 0; i < dsparse::kNumCenterModes; i++) {
            char one[128];
            std::snprintf(one, sizeof one,
                          "%s{\"name\": \"%s\", \"center\": [%.9g, %.9g, %.9g]}",
                          i ? ", " : "", dsparse::kCenterModeNames[i],
                          centers[i][0], centers[i][1], centers[i][2]);
            body += one;
        }
        body += "], \"has_cameras\": ";
        body += center_cameras ? "true" : "false";
        body += ", \"orbit_navigation\": ";
        body += orbit_navigation ? "true" : "false";
        body += "}";
        return HttpResponse::json(body);
    }
};


// ===========================================================================
// ViewerServer
// ===========================================================================

ViewerServer::ViewerServer() : _impl(new Impl) {}
ViewerServer::~ViewerServer() { stop(); }

void ViewerServer::start(const std::string& host, int port,
                         ViewerRenderConfig cfg, ViewerHooks hooks,
                         const PostSplitCameras& post) {
    Impl& im = *_impl;
    im.hooks = hooks;

    // One-shot upload of the post-split camera table for frustum annotation
    // + thumbnails (annotation.ensure_viewer_initialized). Host views are
    // fine -- the engine copies.
    cfg.base_camera_size = viewer_upload_cameras(post);
    viewer_upload_grid(post);

    im.centers = cfg.centers;
    im.center_cameras = cfg.center_cameras;
    im.orbit_navigation = cfg.orbit_navigation;
    im.worker.start(std::move(cfg), std::move(hooks));
    im.buffer_keys = im.worker.buffer_keys();
    im.started = true;

    im.http.route("/",           [&im](const HttpRequest&) { return im.handle_index(); });
    im.http.route("/index.html", [&im](const HttpRequest&) { return im.handle_index(); });
    im.http.route("/render",     [&im](const HttpRequest& r) { return im.handle_render(r); });
    im.http.route("/pick",       [&im](const HttpRequest& r) { return im.handle_pick(r); });
    im.http.route("/buffers",    [&im](const HttpRequest&) { return im.handle_buffers(); });
    im.http.route("/scene",      [&im](const HttpRequest&) { return im.handle_scene(); });
    im.http.route("/progress",   [&im](const HttpRequest&) {
        return HttpResponse::json(im.hooks.progress_json ? im.hooks.progress_json()
                                                         : "{}");
    });
    im.http.route("/pause-toggle", [&im](const HttpRequest&) {
        if (!im.hooks.pause_toggle)
            return HttpResponse::json(
                "{\"paused\": false, \"error\": \"pause_toggle not available\"}");
        bool paused = im.hooks.pause_toggle();
        return HttpResponse::json(std::string("{\"paused\": ") +
                                  (paused ? "true" : "false") + "}");
    });
    im.http.start(host, port);
}

void ViewerServer::stop() {
    Impl& im = *_impl;
    if (!im.started) return;
    im.started = false;
    im.worker.stop();
    im.http.stop();
}

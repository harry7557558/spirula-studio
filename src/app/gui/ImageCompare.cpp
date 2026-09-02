// ImageCompare.cpp -- see ImageCompare.h.

#include "app/gui/ImageCompare.h"

#include "app/DepthColor.h"
#include "app/TrainerCore.h"
#include "app/gui/Layout.h"
#include "app/gui/Ui.h"
#include "core/ExrImage.h"
#include "engine/Engine.h"
#include "external/stb_image.h"

#include "i18n/catalog/Gui.h"

#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace msg = spirula::i18n::msg::gui;

namespace gui {

namespace {

uint8_t to_byte(float v) {
    return (uint8_t)(std::min(std::max(v, 0.0f), 1.0f) * 255.0f + 0.5f);
}

// A split image's views are drawn as one picture, unfolded as a cross (five
// of the twelve cube edges join, as many as any flat unfolding can): per frame
// of camhost::*_face_axes, its cell and the quarter turns that seat its roll.
struct CrossCell { int col, row, turns; };
const CrossCell kCrossFisheye[6] = {
    {1, 1, 0}, {2, 1, 1}, {1, 2, 2}, {0, 1, 3}, {1, 0, 0}, {3, 1, 2},
};
const CrossCell kCrossEquirect[6] = {
    {1, 1, 0}, {2, 1, 0}, {1, 2, 2}, {0, 1, 0}, {1, 0, 0}, {3, 1, 0},
};

// The views of image `index` on the canvas: a crop's place inside its frame's
// cell comes straight from its intrinsics, so the seams meet to the pixel.
void face_layout(const PostSplitCameras& post, int model, int index,
                 const std::vector<AtlasRect>& sizes, std::vector<FaceCell>& cells,
                 int& cw, int& ch) {
    cells.clear();
    const int views = (int)sizes.size();
    const bool split = views > 1 && !post.K_per_camera.empty() &&
                       index < (int)post.K_per_camera.size() &&
                       post.K_per_camera[(size_t)index] == views;
    if (!split) {
        int vw = 1, vh = 1;
        for (const AtlasRect& r : sizes) { vw = std::max(vw, r.w); vh = std::max(vh, r.h); }
        const int cols = (int)std::ceil(std::sqrt((double)std::max(views, 1)));
        const int rows = (std::max(views, 1) + cols - 1) / cols;
        for (int v = 0; v < views; v++)
            cells.push_back({(float)(v % cols * vw), (float)(v / cols * vh),
                             (float)sizes[(size_t)v].w, (float)sizes[(size_t)v].h, 0});
        cw = std::max(1, cols * vw);
        ch = std::max(1, rows * vh);
        return;
    }
    const bool equi = model == (int)CameraModelType::EQUIRECTANGULAR;
    const CrossCell* cross = equi ? kCrossEquirect : kCrossFisheye;
    const int off = post.post_offsets[(size_t)index];
    // Every face of a camera shares a focal, and a frame spans -1..1.
    const float f = post.intrins[(size_t)off * 4];
    const float side = 2.0f * f;
    for (int v = 0; v < views; v++) {
        const size_t p = (size_t)(off + v);
        const int face = std::clamp(post.post_faces[p], 0, 5);
        const float fx = post.intrins[p * 4 + 0], fy = post.intrins[p * 4 + 1];
        const float cx = post.intrins[p * 4 + 2], cy = post.intrins[p * 4 + 3];
        const float w = (float)post.post_widths[p], h = (float)post.post_heights[p];
        // The crop's corners inside its cell, then turned about the centre.
        float x0 = (-cx / fx + 1.0f) * f, x1 = ((w - cx) / fx + 1.0f) * f;
        float y0 = (-cy / fy + 1.0f) * f, y1 = ((h - cy) / fy + 1.0f) * f;
        for (int t = 0; t < (cross[face].turns & 3); t++) {
            const float nx0 = side - y1, nx1 = side - y0;
            y0 = x0; y1 = x1;
            x0 = nx0; x1 = nx1;
        }
        cells.push_back({cross[face].col * side + x0, cross[face].row * side + y0,
                         x1 - x0, y1 - y0, cross[face].turns});
    }
    // The canvas is what is actually DRAWN, not the whole 3x3 cross: a face
    // cropped to its lens leaves its cell partly empty, and fitting the cross
    // would draw the picture at a fraction of the pane and off centre.
    float x0 = cells[0].x, y0 = cells[0].y, x1 = x0, y1 = y0;
    for (const FaceCell& c : cells) {
        x0 = std::min(x0, c.x);          y0 = std::min(y0, c.y);
        x1 = std::max(x1, c.x + c.w);    y1 = std::max(y1, c.y + c.h);
    }
    for (FaceCell& c : cells) { c.x -= x0; c.y -= y0; }
    cw = std::max(1, (int)std::lround(x1 - x0));
    ch = std::max(1, (int)std::lround(y1 - y0));
}

// Nearest-resample one view into its rect of the packed texture. `src` is
// [sh, sw, srcC] and `cvt` turns one source element into a byte; a source of
// one channel is broadcast, which is what a mask wants.
template <typename T, typename Cvt>
void blit(std::vector<uint8_t>& dst, int atlas_w, const AtlasRect& r,
          const T* src, int sh, int sw, int srcC, int dstC, Cvt cvt) {
    if (!src || sh <= 0 || sw <= 0) return;
    for (int y = 0; y < r.h; y++) {
        const int sy = (sh == r.h) ? y : (int)((int64_t)y * sh / r.h);
        const T* srow = src + (size_t)sy * sw * srcC;
        uint8_t* d = dst.data() + ((size_t)(r.y + y) * atlas_w + r.x) * dstC;
        for (int x = 0; x < r.w; x++) {
            const int sx = (sw == r.w) ? x : (int)((int64_t)x * sw / r.w);
            for (int c = 0; c < dstC; c++)
                d[(size_t)x * dstC + c] = cvt(srow[(size_t)sx * srcC +
                                                   (srcC == 1 ? 0 : c)]);
        }
    }
}

// How the modality blocks fill the panel. Blocks pack `bc` to a row, a pair
// spanning pair_w x pair_h cells; only the LAST block can be a lone pane, so a
// row is as wide as its blocks and as tall as the tallest of them.
struct GridPlan {
    int bc;
    int pair_w, pair_h;
    int cols, rows;        // cells
    float cell_w, cell_h;
};

GridPlan grid_shape(int n, bool lone, int bc, int pair_w, int pair_h) {
    GridPlan g{bc, pair_w, pair_h, 0, 0, 0.0f, 0.0f};
    for (int first = 0; first < n; first += bc) {
        const int last = std::min(first + bc, n);
        const bool has_lone = lone && last == n;
        const int pairs = last - first - (has_lone ? 1 : 0);
        g.cols = std::max(g.cols, pairs * pair_w + (has_lone ? 1 : 0));
        g.rows += pairs > 0 ? pair_h : 1;
    }
    return g;
}

// The arrangement that draws the pictures largest. Every pane shares one
// canvas aspect, so the winner is simply the cell shape closest to it.
GridPlan plan_grid(int n, bool lone, const ImVec2& avail, float aspect,
                   const ImVec2& spacing, const ImVec2& chrome) {
    GridPlan best = grid_shape(n, lone, 1, 2, 1);
    float best_score = -1e30f;
    for (int stacked = 0; stacked < 2; stacked++) {
        for (int bc = 1; bc <= n; bc++) {
            GridPlan g = grid_shape(n, lone, bc, stacked ? 1 : 2,
                                    stacked ? 2 : 1);
            g.cell_w = (avail.x - spacing.x * (g.cols - 1)) / g.cols;
            g.cell_h = (avail.y - spacing.y * (g.rows - 1)) / g.rows;
            float score = std::min((g.cell_w - chrome.x) / aspect,
                                   g.cell_h - chrome.y);
            // A pair is read across, so stacking has to win by a margin.
            if (stacked) score *= 0.97f;
            if (score > best_score) {
                best_score = score;
                best = g;
            }
        }
    }
    return best;
}

std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// A one-line placeholder where the pair would be, laid out like the
// viewport's (ViewportPanel::draw).
void centered_note(const spirula::i18n::Msg& m) {
    ImGui::Dummy(ImVec2(0, ImGui::GetContentRegionAvail().y * 0.4f));
    const float w = ImGui::CalcTextSize(m.get()).x;
    ImGui::SetCursorPosX(std::max(0.0f, (ImGui::GetWindowWidth() - w) * 0.5f));
    ui::TextDisabled(m);
}

}  // namespace

// ---------------------------------------------------------------------------
// Attach / detach + worker
// ---------------------------------------------------------------------------

ImageCompare::~ImageCompare() { detach(); }

void ImageCompare::attach(spirula::TrainerSession& session) {
    detach();
    _session = &session;
    _index = std::clamp(_index, 0,
                        (int)std::max<int64_t>(session.ds.num_cameras - 1, 0));
    _error.clear();
    _shot = Shot{};
    _requested = Job{-1, false, false, false};
    _has_error_map =
        spirula::build_step_config(session.cfg, session.st, 0).loss.compute_loss_map;
    _dirty = true;
    _running = true;
    _worker = std::thread([this] { worker_loop(); });
}

void ImageCompare::detach() {
    if (_running) {
        {
            std::lock_guard<std::mutex> lk(_mu);
            _running = false;
        }
        _cv.notify_all();
        if (_worker.joinable()) _worker.join();
    }
    _session = nullptr;
    _in_flight = 0;
    _has_pending = false;
    _shot = Shot{};
    _tex_dirty = true;
}

void ImageCompare::destroy_gl() {
    for (Pane* p : {&_gt, &_render, &_gt_depth, &_render_depth,
                    &_gt_normal, &_render_normal, &_err_map})
        if (p->tex) { glDeleteTextures(1, &p->tex); p->tex = 0; }
}

void ImageCompare::worker_loop() {
    for (;;) {
        Job j;
        uint64_t id = 0;
        {
            std::unique_lock<std::mutex> lk(_mu);
            _cv.wait(lk, [this] { return _has_pending || !_running; });
            if (!_running) return;
            j = _pending;
            id = _pending_id;
            _has_pending = false;
        }
        Shot s;
        s.job = j;
        const auto t0 = std::chrono::steady_clock::now();
        try {
            run_job(j, s);
        } catch (const std::exception& e) {
            s.error = e.what();
        }
        s.secs = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - t0).count();
        s.id = id;
        bool more;
        {
            std::lock_guard<std::mutex> lk(_mu);
            _result = std::move(s);
            more = _has_pending;
        }
        // Nothing here wants the engine any more; let the trainer step.
        if (!more) _session->render_pending = false;
    }
}

void ImageCompare::run_job(const Job& j, Shot& out) {
    spirula::TrainerSession& s = *_session;
    out.step = s.cur_step.load();

    const int sh_deg = out.step / std::max(s.cfg.sh_degree_warmup_every, 1);
    // Members, not locals: a 4K pair is ~200 MB of readback, and freshly
    // mapped scratch is faulted in and zeroed by the kernel on every job.
    std::vector<float>& gt = _wgt;
    std::vector<float>& render = _wrender;
    std::vector<float>& render_raw = _wrender_raw;
    std::vector<uint8_t>& alpha = _walpha;
    gt.clear(); render.clear(); render_raw.clear(); alpha.clear();
    _wgt_depth.clear(); _wr_depth.clear();
    _wgt_normal.clear(); _wr_normal.clear(); _werr.clear();
    // The source file is shown undecoded, so pair it with the render before
    // the working-space -> sRGB conversion rather than after.
    const auto color = spirula::resolve_color(s.cfg);
    const bool want_raw = j.source_gt && color.splat_on();
    int64_t C = 3;
    bool got_err = false;
    // This step's config, so the forward renders the channels the trainer's
    // does and the error map is weighted the way this step weights it.
    const LossConfig loss =
        spirula::build_step_config(s.cfg, s.st, out.step).loss;

    // One pass per run of equal-size faces (Engine.h, engine_preview_forward).
    // Views of one pass share a size, so each pass appends a block to every
    // buffer and takes a row of the packed texture.
    struct Pass {
        int views = 0, H = 0, W = 0;
        int64_t mh = 0, mw = 0, dh = 0, dw = 0, nh = 0, nw = 0;
        size_t rgb = 0, mask = 0, depth = 0, normal = 0, err = 0;
        size_t r_depth = 0, r_normal = 0;
    };
    std::vector<Pass> passes;
    double se = 0.0;
    int64_t se_n = 0;
    int npass = 1;

    for (int pi = 0; pi < npass; pi++) {
        std::lock_guard<std::mutex> lk(s.engine_mutex);
        const int views = engine_preview_forward(
            j.index, s.cfg.primitive, sh_deg, s.cfg.packed || s.cfg.use_bvh,
            j.color_correct, loss, pi, &npass);
        auto shape = engine_get_render_rgb_shape();
        Pass q;
        q.views = (int)std::get<0>(shape);
        q.H = (int)std::get<1>(shape);
        q.W = (int)std::get<2>(shape);
        C = std::get<3>(shape);
        if (views <= 0 || q.views <= 0 || q.H <= 0 || q.W <= 0 || C <= 0) {
            // Nothing came back and nothing threw. Say so rather than leave
            // the panel on "rendering" for the rest of the run.
            out.error = "engine_preview_forward produced no image";
            return;
        }
        const int64_t B = q.views, H = q.H, W = q.W;
        auto tv = [](std::vector<float>& v, size_t off, int64_t b, int64_t h,
                     int64_t w, int64_t c) {
            return TorchTensorView{(uint64_t)(uintptr_t)(v.data() + off), 4,
                                   {b, h, w, c}};
        };

        const size_t n = (size_t)(B * H * W * C);
        q.rgb = render.size();
        render.resize(q.rgb + n);
        gt.resize(q.rgb + n);
        if (want_raw) render_raw.resize(q.rgb + n);
        engine_copy_render_to_host(
            tv(render, q.rgb, B, H, W, C),
            TorchTensorView{0, 0, {}}, TorchTensorView{0, 0, {}},
            want_raw ? tv(render_raw, q.rgb, B, H, W, C)
                     : TorchTensorView{0, 0, {}},
            TorchTensorView{0, 0, {}});
        engine_copy_gt_rgb_to_host(tv(gt, q.rgb, B, H, W, C));

        // The mask keeps its own resolution: the loss samples it rather than
        // resizing it, so an unsplit 1920x1920 fisheye trained against a
        // 1600x1600 mask is ordinary. Only the batch dimension has to agree.
        auto ashape = engine_get_gt_alpha_shape();
        if (std::get<0>(ashape) == B) {
            q.mh = std::get<1>(ashape);
            q.mw = std::get<2>(ashape);
            q.mask = alpha.size();
            alpha.resize(q.mask + (size_t)(B * q.mh * q.mw));
            engine_copy_gt_alpha_to_host(
                TorchTensorView{(uint64_t)(uintptr_t)(alpha.data() + q.mask), 1,
                                {B, q.mh, q.mw, 1LL}});
        }

        // The supervision modalities. Only what the run loads: a reference
        // pane for something no weight reads would be a picture of nothing.
        if (s.has_depth) {
            auto sh = engine_get_gt_depth_shape();
            if (std::get<0>(sh) == B) {
                q.dh = std::get<1>(sh);
                q.dw = std::get<2>(sh);
                q.depth = _wgt_depth.size();
                _wgt_depth.resize(q.depth + (size_t)(B * q.dh * q.dw));
                engine_copy_gt_depth_to_host(
                    tv(_wgt_depth, q.depth, B, q.dh, q.dw, 1));
            }
            q.r_depth = _wr_depth.size();
            _wr_depth.resize(q.r_depth + (size_t)(B * H * W));
            engine_copy_render_to_host(TorchTensorView{0, 0, {}},
                                       tv(_wr_depth, q.r_depth, B, H, W, 1),
                                       TorchTensorView{0, 0, {}},
                                       TorchTensorView{0, 0, {}},
                                       TorchTensorView{0, 0, {}});
        }
        if (s.has_normal) {
            auto sh = engine_get_gt_normal_shape();
            if (std::get<0>(sh) == B) {
                q.nh = std::get<1>(sh);
                q.nw = std::get<2>(sh);
                q.normal = _wgt_normal.size();
                _wgt_normal.resize(q.normal + (size_t)(B * q.nh * q.nw * 3));
                engine_copy_gt_normal_to_host(
                    tv(_wgt_normal, q.normal, B, q.nh, q.nw, 3));
            }
            q.r_normal = _wr_normal.size();
            _wr_normal.resize(q.r_normal + (size_t)(B * H * W * 3));
            engine_copy_render_depth_normal_to_host(
                tv(_wr_normal, q.r_normal, B, H, W, 3));
        }

        if (j.error_map) {
            q.err = _werr.size();
            _werr.resize(q.err + (size_t)(B * H * W));
            got_err = engine_preview_loss_map(
                          loss, tv(_werr, q.err, B, H, W, 1)) || got_err;
        }

        // Scored over the pixels the loss is computed on, which is why the
        // mask enters here and not only on screen.
        for (int64_t b = 0; b < B; b++)
            for (int64_t y = 0; y < H; y++)
                for (int64_t x = 0; x < W; x++) {
                    if (q.mh > 0) {
                        const int64_t my = q.mh == H ? y : y * q.mh / H;
                        const int64_t mx = q.mw == W ? x : x * q.mw / W;
                        if (!alpha[q.mask + (size_t)((b * q.mh + my) * q.mw + mx)])
                            continue;
                    }
                    const size_t pix = q.rgb + (size_t)(((b * H + y) * W + x) * C);
                    for (int64_t c = 0; c < C; c++) {
                        const double d = (double)gt[pix + (size_t)c] -
                                         (double)render[pix + (size_t)c];
                        se += d * d;
                    }
                    se_n += C;
                }
        passes.push_back(q);
    }
    if (se_n > 0)
        out.psnr = (float)(-10.0 * std::log10(std::max(se / (double)se_n, 1e-12)));

    // ---- the packed texture: a row per pass, a rect per view ----
    out.views = 0;
    out.view_w = out.view_h = 0;
    out.atlas_w = out.atlas_h = 0;
    out.uv.clear();
    for (const Pass& q : passes) {
        out.atlas_w = std::max(out.atlas_w, q.views * q.W);
        out.atlas_h += q.H;
        out.view_w = std::max(out.view_w, q.W);
        out.view_h = std::max(out.view_h, q.H);
        out.views += q.views;
    }
    {
        int y = 0;
        for (const Pass& q : passes) {
            for (int v = 0; v < q.views; v++)
                out.uv.push_back(AtlasRect{v * q.W, y, q.W, q.H});
            y += q.H;
        }
    }
    face_layout(s.post, s.ds.camera_models[(size_t)j.index], j.index, out.uv,
                out.cells, out.canvas_w, out.canvas_h);

    const size_t atlas_rgb = (size_t)out.atlas_w * out.atlas_h * 3;
    out.gt.assign(atlas_rgb, 0);
    out.render.assign(atlas_rgb, 0);
    if (!alpha.empty()) out.mask.assign((size_t)out.atlas_w * out.atlas_h, 0);
    auto byte = [](float v) { return to_byte(v); };
    auto keep = [](uint8_t v) { return v; };
    int view = 0;
    for (const Pass& q : passes) {
        for (int v = 0; v < q.views; v++, view++) {
            const AtlasRect& r = out.uv[(size_t)view];
            const size_t px = (size_t)v * q.H * q.W;
            blit(out.gt, out.atlas_w, r, gt.data() + q.rgb + px * (size_t)C,
                 q.H, q.W, (int)C, 3, byte);
            const std::vector<float>& src = render_raw.empty() ? render : render_raw;
            blit(out.render, out.atlas_w, r, src.data() + q.rgb + px * (size_t)C,
                 q.H, q.W, (int)C, 3, byte);
            if (q.mh > 0)
                blit(out.mask, out.atlas_w, r,
                     alpha.data() + q.mask + (size_t)v * q.mh * q.mw,
                     (int)q.mh, (int)q.mw, 1, 1, keep);
        }
    }

    // Coloured over ALL the views at once, so a split capture's faces share
    // one depth range instead of each getting its own.
    auto colour = [&](std::vector<float>& src, bool normal, bool skip_zero,
                      bool reference, std::vector<uint8_t>& dst) {
        if (src.empty()) return;
        const size_t n = src.size() / (normal ? 3 : 1);
        _wmodrgb.resize(n * 3);
        if (normal) app::normal_to_rgb(src.data(), n, _wmodrgb.data());
        else        app::depth_to_rgb(src.data(), n, skip_zero, _wmodrgb.data());
        dst.assign(atlas_rgb, 0);
        int vi = 0;
        for (const Pass& q : passes) {
            const int64_t h = reference ? (normal ? q.nh : q.dh) : q.H;
            const int64_t w = reference ? (normal ? q.nw : q.dw) : q.W;
            // The buffer's offset counts floats; a normal has three per pixel
            // and the coloured copy is indexed in pixels.
            const size_t off = (reference ? (normal ? q.normal : q.depth)
                                          : (normal ? q.r_normal : q.r_depth)) /
                               (normal ? 3u : 1u);
            for (int v = 0; v < q.views; v++, vi++) {
                if (h <= 0 || w <= 0) continue;
                blit(dst, out.atlas_w, out.uv[(size_t)vi],
                     _wmodrgb.data() + (off + (size_t)v * h * w) * 3,
                     (int)h, (int)w, 3, 3, keep);
            }
        }
    };
    // 0 is the trainer's "no ground truth here" and must not drag the range
    // down to it; the render's own depth has no such sentinel.
    colour(_wgt_depth,  false, true,  true,  out.gt_depth);
    colour(_wr_depth,   false, false, false, out.render_depth);
    colour(_wgt_normal, true,  false, true,  out.gt_normal);
    colour(_wr_normal,  true,  false, false, out.render_normal);

    if (got_err && !_werr.empty()) {
        double sum = 0.0;
        for (float v : _werr) { sum += v; out.err_max = std::max(out.err_max, v); }
        out.err_mean = (float)(sum / (double)_werr.size());
        _wmodrgb.resize(_werr.size() * 3);
        app::error_to_rgb(_werr.data(), _werr.size(), 0.0f, _wmodrgb.data());
        out.err_map.assign(atlas_rgb, 0);
        int vi = 0;
        for (const Pass& q : passes)
            for (int v = 0; v < q.views; v++, vi++)
                blit(out.err_map, out.atlas_w, out.uv[(size_t)vi],
                     _wmodrgb.data() + (q.err + (size_t)v * q.H * q.W) * 3,
                     q.H, q.W, 3, 3, keep);
    }

    if (j.source_gt && j.index < (int)s.ds.image_filenames.size()) {
        const std::string& src = s.ds.image_filenames[(size_t)j.index];
        int w = 0, h = 0, ch = 0;
        if (exr::is_exr(src)) {
            // "Rec.709, not linear" is the identity: this pane shows the file's
            // own values, so an EXR is quantized without a transfer curve.
            exr::Info info;
            if (exr::decode_srgb8(src, exr::Options(), info, out.src,
                                  "Rec.709", false).empty()) {
                out.src_w = info.width;
                out.src_h = info.height;
            } else {
                out.src.clear();
            }
        } else if (stbi_uc* img = stbi_load(src.c_str(), &w, &h, &ch, 3)) {
            out.src.assign(img, img + (size_t)w * h * 3);
            out.src_w = w;
            out.src_h = h;
            stbi_image_free(img);
        }
    }
}

void ImageCompare::submit(const Job& j) {
    {
        std::lock_guard<std::mutex> lk(_mu);
        _pending = j;
        _pending_id = _next_id++;
        _has_pending = true;
        _in_flight = _pending_id;
    }
    // Synchronously, so the training loop yields at its next boundary instead
    // of after the whole step this job would otherwise queue behind.
    _session->render_pending = true;
    _cv.notify_one();
}

void ImageCompare::poll() {
    if (!_in_flight) return;
    Shot s;
    {
        std::lock_guard<std::mutex> lk(_mu);
        if (_result.id != _in_flight) return;
        s = std::move(_result);
        _result = Shot{};
    }
    _in_flight = 0;
    if (s.secs > 0)
        _job_secs = _job_secs > 0 ? 0.7 * _job_secs + 0.3 * s.secs : s.secs;
    if (!s.error.empty()) {
        _error = s.error;
        return;
    }
    _error.clear();
    _shot = std::move(s);
    _rendered_step = _shot.step;
    _tex_dirty = true;
}

// ---------------------------------------------------------------------------
// Compositing + upload
// ---------------------------------------------------------------------------

void ImageCompare::upload(GLuint& tex, const uint8_t* rgb, int w, int h) {
    if (!tex) {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, rgb);
}

void ImageCompare::rebuild_textures() {
    _tex_dirty = false;
    if (_shot.atlas_w <= 0 || _shot.atlas_h <= 0) return;

    const int pw = _shot.atlas_w, ph = _shot.atlas_h;

    // The excluded region is marked the same way on both sides, so a
    // difference between the panes is a difference in the model rather than
    // in what each of them chose to hide. The mask is in packed coordinates,
    // which is what the render side is in too; only a source-file photograph
    // (never split, so one packed cell) has to be scaled into it.
    auto composite = [&](const uint8_t* rgb, int w, int h,
                         std::vector<uint8_t>& out) {
        out.assign(rgb, rgb + (size_t)w * h * 3);
        if (_mask_show == MaskShow::Unmarked || _shot.mask.empty()) return;
        for (int y = 0; y < h; y++) {
            const int my = (h == ph) ? y : (int)((int64_t)y * ph / h);
            for (int x = 0; x < w; x++) {
                const int mx = (w == pw) ? x : (int)((int64_t)x * pw / w);
                if (_shot.mask[(size_t)my * pw + mx]) continue;
                uint8_t* p = out.data() + ((size_t)y * w + x) * 3;
                if (_mask_show == MaskShow::Hide) p[0] = p[1] = p[2] = 0;
                else for (int c = 0; c < 3; c++) p[c] = (uint8_t)(p[c] / 4);
            }
        }
    };

    auto to_pane = [&](Pane& p, const uint8_t* rgb, int w, int h, bool packed,
                       std::vector<uint8_t>& buf) {
        composite(rgb, w, h, buf);
        upload(p.tex, buf.data(), w, h);
        p.tex_w = w;
        p.tex_h = h;
        if (packed) {
            p.cells = _shot.cells;
            p.uv = _shot.uv;
            p.canvas_w = _shot.canvas_w;
            p.canvas_h = _shot.canvas_h;
        } else {
            p.cells.assign(1, FaceCell{0.0f, 0.0f, (float)w, (float)h, 0});
            p.uv.assign(1, AtlasRect{0, 0, w, h});
            p.canvas_w = w;
            p.canvas_h = h;
        }
    };

    std::vector<uint8_t> buf;
    // The source file is only offered for an unsplit image, so its canvas has
    // the same aspect as the render's and one zoom still serves both.
    const bool use_src = _shot.job.source_gt && !_shot.src.empty();
    if (use_src) to_pane(_gt, _shot.src.data(), _shot.src_w, _shot.src_h, false, buf);
    else         to_pane(_gt, _shot.gt.data(), pw, ph, true, buf);
    to_pane(_render, _shot.render.data(), pw, ph, true, buf);
    struct { Pane* pane; const std::vector<uint8_t>* px; } extra[] = {
        {&_gt_depth, &_shot.gt_depth},     {&_render_depth, &_shot.render_depth},
        {&_gt_normal, &_shot.gt_normal},   {&_render_normal, &_shot.render_normal},
        {&_err_map, &_shot.err_map},
    };
    for (auto& e : extra)
        if (!e.px->empty()) to_pane(*e.pane, e.px->data(), pw, ph, true, buf);
}

// ---------------------------------------------------------------------------
// Selection
// ---------------------------------------------------------------------------

int ImageCompare::faces() const {
    const auto& K = _session->post.K_per_camera;
    if (K.empty() || _index >= (int)K.size()) return 1;
    return std::max(1, K[(size_t)_index]);
}

bool ImageCompare::has_masks() const {
    return !_session->ds.mask_filenames.empty() || _session->post.any_fov_mask;
}

void ImageCompare::select(int index) {
    const int n = (int)_session->ds.num_cameras;
    if (n <= 0) return;
    index = std::clamp(index, 0, n - 1);
    if (index == _index) return;
    _index = index;
    _dirty = true;
}

// ---------------------------------------------------------------------------
// Toolbar
// ---------------------------------------------------------------------------

void ImageCompare::draw_image_picker() {
    const auto& names = _session->ds.image_filenames;
    const int n = (int)names.size();
    auto base = [&](int i) {
        return std::filesystem::path(names[(size_t)i]).filename().string();
    };

    place(ImGui::GetFrameHeight() * 2 + ImGui::GetStyle().ItemSpacing.x);
    ImGui::BeginDisabled(n <= 1);
    if (ui::ButtonRaw("<")) select(_index - 1);
    ImGui::SameLine();
    if (ui::ButtonRaw(">")) select(_index + 1);
    ImGui::EndDisabled();
    ui::help_on_hover(msg::compare_navigate_help);

    // "12 / 240" is two numbers and a separator: the same in every language.
    char fmt[32];
    std::snprintf(fmt, sizeof fmt, "%%d / %d", std::max(n, 1));
    int one_based = _index + 1;
    place(px(190.0f));
    ImGui::SetNextItemWidth(px(190.0f));
    if (ui::SliderIntRaw("##imgidx", &one_based, 1, std::max(n, 1), fmt))
        select(one_based - 1);
    ui::help_on_hover(msg::compare_slider_help);

    place(px(260.0f));
    ImGui::SetNextItemWidth(px(260.0f));
    // File names are file names in every language.
    if (ui::BeginComboRaw("##imgname", n > 0 ? base(_index).c_str() : "")) {
        ImGui::SetNextItemWidth(-1);
        ui::InputTextWithHintRaw("##imgfilter", msg::compare_search_hint,
                                 &_filter);
        const std::string needle = lower(_filter);
        std::vector<int> hits;
        hits.reserve((size_t)n);
        for (int i = 0; i < n; i++)
            if (needle.empty() || lower(base(i)).find(needle) != std::string::npos)
                hits.push_back(i);
        ImGui::BeginChild("##imglist", ImVec2(0, 260));
        ImGuiListClipper clip;
        clip.Begin((int)hits.size());
        while (clip.Step())
            for (int r = clip.DisplayStart; r < clip.DisplayEnd; r++) {
                const int i = hits[(size_t)r];
                if (ui::SelectableRaw(base(i), i == _index)) {
                    select(i);
                    ImGui::CloseCurrentPopup();
                }
            }
        clip.End();
        ImGui::EndChild();
        ImGui::EndCombo();
    }
    ui::help_on_hover(msg::compare_pick_help);
}

// One flowing row of controls, wrapping onto as many rows as they need --
// which ones are present depends on the dataset and on the run, so a fixed
// layout would overflow on some combinations and waste a row on others.
// `used` is TRACKED rather than read back from the last item's rectangle,
// which would pick up a tooltip's width whenever one is hovered.
void ImageCompare::place(float w) {
    const ImGuiStyle& st = ImGui::GetStyle();
    if (_row_used > 0.0f && _row_used + st.ItemSpacing.x + w <= _row_w) {
        ImGui::SameLine();
        _row_used += st.ItemSpacing.x + w;
    } else {
        _row_used = w;
    }
}

void ImageCompare::draw_toolbar(bool training) {
    const ImGuiStyle& st = ImGui::GetStyle();
    _row_w = ImGui::GetContentRegionAvail().x;
    _row_used = 0.0f;
    auto check_w = [&](const spirula::i18n::Msg& m) {
        return ImGui::GetFrameHeight() + st.ItemInnerSpacing.x +
               ImGui::CalcTextSize(m.get()).x;
    };

    draw_image_picker();

    if (has_masks()) {
        const float w = ImGui::CalcTextSize(msg::compare_mask_dim.get()).x + 60;
        place(w);
        ImGui::SetNextItemWidth(w);
        int m = (int)_mask_show;
        if (ui::ComboRaw("##maskshow", &m,
                         {&msg::compare_mask_dim, &msg::compare_mask_unmarked,
                          &msg::compare_mask_hide})) {
            _mask_show = (MaskShow)m;
            _tex_dirty = true;
        }
        ui::help_on_hover(msg::compare_mask_help);
    }

    if (_session->st.bilagrid_rgb_init || _session->st.ppisp_init) {
        place(check_w(msg::compare_color_match));
        if (ui::Checkbox(msg::compare_color_match, &_color_correct))
            _dirty = true;
        ui::help_on_hover(msg::compare_color_match_help);
    }

    // A split image has no single source frame its views line up with, so
    // there is nothing to offer there.
    const bool split = faces() > 1;
    place(check_w(msg::compare_source_gt));
    ImGui::BeginDisabled(split);
    if (ui::Checkbox(msg::compare_source_gt, &_source_gt)) _dirty = true;
    ImGui::EndDisabled();
    ui::help_on_hover(split ? msg::compare_source_gt_split
                            : msg::compare_source_gt_help);

    // Only where the run loads them: a toggle for a row that cannot appear
    // says the dataset has something it has not.
    if (_session->has_depth) {
        place(check_w(msg::compare_show_depth));
        ui::Checkbox(msg::compare_show_depth, &_show_depth);
        ui::help_on_hover(msg::compare_show_depth_help);
    }
    if (_session->has_normal) {
        place(check_w(msg::compare_show_normal));
        ui::Checkbox(msg::compare_show_normal, &_show_normal);
        ui::help_on_hover(msg::compare_show_normal_help);
    }
    if (_has_error_map) {
        place(check_w(msg::compare_show_error));
        ui::Checkbox(msg::compare_show_error, &_show_error);
        ui::help_on_hover(msg::compare_show_error_help);
    }

    place(check_w(msg::compare_smooth));
    ui::Checkbox(msg::compare_smooth, &_smooth);
    ui::help_on_hover(msg::compare_smooth_help);

    if (training) {
        place(check_w(msg::viewport_live));
        ui::Checkbox(msg::viewport_live, &_live);
        ui::help_on_hover(msg::compare_live_help);
    }

    // A percentage is a percentage in every language.
    char z[16];
    std::snprintf(z, sizeof z, "%d%%", (int)std::lround(_zoom * 100.0f));
    place(ImGui::CalcTextSize(msg::compare_reset_zoom.get()).x +
          2.0f * st.FramePadding.x + st.ItemSpacing.x +
          ImGui::CalcTextSize(z).x);
    if (ui::Button(msg::compare_reset_zoom)) {
        _zoom = 1.0f;
        _cx = _cy = 0.5f;
    }
    ImGui::SameLine();
    ui::TextDisabledRaw(z);
    ui::help_on_hover(msg::compare_zoom_help);
}

// ---------------------------------------------------------------------------
// Panes
// ---------------------------------------------------------------------------

void ImageCompare::handle_view_input() {
    ImGuiIO& io = ImGui::GetIO();
    const float dw = std::max(_hot_w, 1.0f), dh = std::max(_hot_h, 1.0f);

    if (_hot && io.MouseWheel != 0.0f) {
        // Keep whatever the cursor is over where it is.
        const float u = _cx + (io.MousePos.x - _hot_min.x - _hot_size.x * 0.5f) / dw;
        const float v = _cy + (io.MousePos.y - _hot_min.y - _hot_size.y * 0.5f) / dh;
        const float prev = _zoom;
        _zoom = std::clamp(_zoom * std::pow(1.2f, io.MouseWheel), 1.0f, 64.0f);
        const float k = prev / _zoom;
        _cx = u + (_cx - u) * k;
        _cy = v + (_cy - v) * k;
    }

    if (_hot && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) _dragging = true;
    if (_dragging) {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) _dragging = false;
        else {
            _cx -= io.MouseDelta.x / dw;
            _cy -= io.MouseDelta.y / dh;
        }
    }
}

void ImageCompare::draw_pane(const Pane& p, const ImVec2& box,
                             const spirula::i18n::Msg& caption) {
    // No scrolling: the wheel is the zoom here, and a child that can scroll
    // eats it before the pane sees it.
    ImGui::BeginChild(caption.id, box, ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar |
                      ImGuiWindowFlags_NoScrollWithMouse);
    ui::TextDisabled(caption);
    ImVec2 avail = ImGui::GetContentRegionAvail();
    avail.x = std::max(avail.x, 16.0f);
    avail.y = std::max(avail.y, 16.0f);

    if (p.tex && p.canvas_w > 0 && p.canvas_h > 0) {
        // The canvas is aspect-fit at zoom 1 and cropped beyond it, so the
        // drawn area never overflows the pane: both panes stay the same size
        // whatever each one's resolution is, and one zoom drives both.
        const float fit = std::min(avail.x / (float)p.canvas_w,
                                   avail.y / (float)p.canvas_h);
        const float disp_w = (float)p.canvas_w * fit * _zoom;
        const float disp_h = (float)p.canvas_h * fit * _zoom;
        const float vw = std::min(avail.x, disp_w), vh = std::min(avail.y, disp_h);
        const float fu = vw / disp_w, fv = vh / disp_h;
        _cx = std::clamp(_cx, fu * 0.5f, 1.0f - fu * 0.5f);
        _cy = std::clamp(_cy, fv * 0.5f, 1.0f - fv * 0.5f);

        ImGui::SetCursorPos(
            ImVec2(ImGui::GetCursorPosX() + (avail.x - vw) * 0.5f,
                   ImGui::GetCursorPosY() + (avail.y - vh) * 0.5f));
        const ImVec2 org = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(vw, vh));

        // Canvas origin in screen space, and canvas pixels to screen. Each
        // view is drawn straight from its slot in the packed texture onto its
        // own quad, so the cells the cross leaves empty simply are not drawn.
        const float ox = org.x - (_cx - fu * 0.5f) * disp_w;
        const float oy = org.y - (_cy - fv * 0.5f) * disp_h;
        const float sc = fit * _zoom;
        // Half a texel in from each edge: adjacent cells come from slots that
        // are NOT adjacent in the texture, and linear filtering at the seam
        // would otherwise blend in whichever face happens to be packed next.
        // A lone view has no neighbour to bleed from and is left exact, so a
        // source-file photograph still lines up with the render texel for texel.
        const bool inset = p.cells.size() > 1;
        const float iu = inset ? 0.5f / (float)p.tex_w : 0.0f;
        const float iv = inset ? 0.5f / (float)p.tex_h : 0.0f;

        const ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        // The backend binds its own LINEAR sampler object, which overrides
        // anything glTexParameteri put on the texture -- point sampling has to
        // be asked for through these two draw callbacks, in a matched pair.
        const bool point = !_smooth && pio.DrawCallback_SetSamplerNearest &&
                           pio.DrawCallback_SetSamplerLinear;
        dl->PushClipRect(org, ImVec2(org.x + vw, org.y + vh), true);
        if (point) dl->AddCallback(pio.DrawCallback_SetSamplerNearest, nullptr);
        for (size_t v = 0; v < p.cells.size(); v++) {
            const FaceCell& fc = p.cells[v];
            const float x0 = ox + fc.x * sc, x1 = x0 + fc.w * sc;
            const float y0 = oy + fc.y * sc, y1 = y0 + fc.h * sc;
            if (x1 < org.x || x0 > org.x + vw || y1 < org.y || y0 > org.y + vh)
                continue;
            const AtlasRect& a = p.uv[v];
            const float u0 = (float)a.x / p.tex_w + iu;
            const float u1 = (float)(a.x + a.w) / p.tex_w - iu;
            const float v0 = (float)a.y / p.tex_h + iv;
            const float v1 = (float)(a.y + a.h) / p.tex_h - iv;
            const ImVec2 quad[4] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
            const ImVec2 uv[4]   = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};
            // A quarter turn clockwise is one step around the corner list.
            const int t = fc.turns & 3;
            dl->AddImageQuad((ImTextureID)(intptr_t)p.tex,
                             quad[0], quad[1], quad[2], quad[3],
                             uv[(4 - t) & 3], uv[(5 - t) & 3],
                             uv[(6 - t) & 3], uv[(7 - t) & 3]);
        }
        if (point) dl->AddCallback(pio.DrawCallback_SetSamplerLinear, nullptr);
        dl->PopClipRect();

        // The wheel and the drag are applied ONCE per frame, after both panes
        // (see draw_panes): handling them per pane would double every delta on
        // a drag that leaves one pane hovered.
        if (ImGui::IsItemHovered()) {
            _hot = true;
            _hot_min = org;
            _hot_size = ImVec2(vw, vh);
            _hot_w = disp_w;
            _hot_h = disp_h;
        }
    }
    ImGui::EndChild();
}

// One block per modality: a reference / render pair, or the error map on its
// own. They share the zoom, because a depth map is judged against the
// photograph beside it far more often than on its own.
struct ImageCompare::Block {
    const Pane* pane[2];
    const spirula::i18n::Msg* caption[2];
    int n;
};

int ImageCompare::collect(Block* out) const {
    int n = 0;
    out[n++] = {{&_gt, &_render},
                {&msg::compare_pane_gt, &msg::compare_pane_render}, 2};
    if (_show_depth && !_shot.gt_depth.empty())
        out[n++] = {{&_gt_depth, &_render_depth},
                    {&msg::compare_pane_gt_depth,
                     &msg::compare_pane_render_depth}, 2};
    if (_show_normal && !_shot.gt_normal.empty())
        out[n++] = {{&_gt_normal, &_render_normal},
                    {&msg::compare_pane_gt_normal,
                     &msg::compare_pane_render_normal}, 2};
    if (_show_error && !_shot.err_map.empty())
        out[n++] = {{&_err_map, nullptr}, {&msg::compare_pane_error, nullptr}, 1};
    return n;
}

void ImageCompare::draw_panes(const ImVec2& avail) {
    Block blocks[4];
    const int n = collect(blocks);
    const bool lone = blocks[n - 1].n == 1;

    const ImGuiStyle& st = ImGui::GetStyle();
    // What a pane spends on its border, padding and caption before the picture
    // gets any of it -- the grid is chosen on what is left, not on the box.
    const ImVec2 chrome(
        2.0f * (st.WindowPadding.x + st.ChildBorderSize),
        2.0f * (st.WindowPadding.y + st.ChildBorderSize) +
            ImGui::GetTextLineHeight() + st.ItemSpacing.y);
    const float aspect = _render.canvas_h > 0
        ? (float)_render.canvas_w / (float)_render.canvas_h : 1.0f;
    const GridPlan g = plan_grid(n, lone, avail, aspect, st.ItemSpacing, chrome);

    // The cell is what fits; the box is only what the picture needs of it.
    // Trimming the slack and centring what is left keeps every border tight
    // around a picture instead of framing empty space beside it.
    const float drawn = std::max(
        std::min((g.cell_w - chrome.x) / aspect, g.cell_h - chrome.y), 32.0f);
    const ImVec2 box(drawn * aspect + chrome.x, drawn + chrome.y);
    const float step_x = box.x + st.ItemSpacing.x;
    const float step_y = box.y + st.ItemSpacing.y;

    const ImVec2 cur = ImGui::GetCursorPos();
    const ImVec2 base(
        cur.x + std::max(0.0f, (avail.x - g.cols * step_x +
                                st.ItemSpacing.x) * 0.5f),
        cur.y + std::max(0.0f, (avail.y - g.rows * step_y +
                                st.ItemSpacing.y) * 0.5f));

    _hot = false;
    int cell_y = 0;
    for (int first = 0; first < n; first += g.bc) {
        const int last = std::min(first + g.bc, n);
        const bool has_lone = lone && last == n;
        const int pairs = last - first - (has_lone ? 1 : 0);
        const int wide = pairs * g.pair_w + (has_lone ? 1 : 0);
        const int tall = pairs > 0 ? g.pair_h : 1;
        // Every row is centred on the widest, so a short last row sits under
        // the middle of the others rather than off to one side.
        float x = base.x + (g.cols - wide) * step_x * 0.5f;
        const float y = base.y + cell_y * step_y;
        for (int i = first; i < last; i++) {
            const Block& b = blocks[i];
            for (int k = 0; k < b.n; k++) {
                // The second of a pair is one cell along the block; a lone
                // pane takes one cell, centred down the row.
                const float ox = b.n == 2 ? (float)(k * (g.pair_w - 1)) : 0.0f;
                const float oy = b.n == 2 ? (float)(k * (g.pair_h - 1))
                                          : (tall - 1) * 0.5f;
                ImGui::SetCursorPos(ImVec2(x + ox * step_x, y + oy * step_y));
                draw_pane(*b.pane[k], box, *b.caption[k]);
            }
            x += (b.n == 2 ? g.pair_w : 1) * step_x;
        }
        cell_y += tall;
    }
    handle_view_input();
}

void ImageCompare::handle_keys() {
    if (ImGui::GetIO().WantTextInput) return;
    if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) &&
        !ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
        return;
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true))  select(_index - 1);
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true)) select(_index + 1);
    if (ImGui::IsKeyPressed(ImGuiKey_PageUp, true))     select(_index - 10);
    if (ImGui::IsKeyPressed(ImGuiKey_PageDown, true))   select(_index + 10);
    if (ImGui::IsKeyPressed(ImGuiKey_Home, false))      select(0);
    if (ImGui::IsKeyPressed(ImGuiKey_End, false))
        select((int)_session->ds.num_cameras - 1);
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------

void ImageCompare::draw(bool training, int step) {
    if (!_session || _session->ds.num_cameras == 0) {
        centered_note(msg::compare_waiting);
        return;
    }

    const double now = ImGui::GetTime();
    if (step >= 0 && step != _last_seen_step) {
        if (_last_seen_step >= 0 && step > _last_seen_step && _last_step_time > 0) {
            const double per = (now - _last_step_time) / (step - _last_seen_step);
            _step_secs = _step_secs > 0 ? 0.8 * _step_secs + 0.2 * per : per;
        }
        _last_seen_step = step;
        _last_step_time = now;
    }

    poll();
    draw_toolbar(training);
    handle_keys();

    // Re-request when the selection changed, and -- while training -- often
    // enough to follow the optimization without taking much of it. Paced in
    // ITERATIONS from the measured cost of a job against the cost of a step,
    // so the share it takes does not depend on how fast the dataset trains.
    const Job want{_index, _color_correct, _source_gt,
                   _show_error && _has_error_map};
    bool follow = false;
    if (training && _live && _rendered_step >= 0 && step >= 0) {
        const int interval = (_job_secs > 0 && _step_secs > 0)
            ? (int)std::clamp(std::ceil(_job_secs / (0.08 * _step_secs)),
                              25.0, 2000.0)
            : 50;
        follow = step - _rendered_step >= interval;
    }
    if (!_in_flight && (_dirty || follow || !(want == _requested))) {
        _dirty = false;
        _requested = want;
        submit(want);
    }
    if (_tex_dirty) rebuild_textures();

    if (!_error.empty()) {
        ui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), msg::viewport_render_error,
                        {_error});
        return;
    }

    if (_shot.view_w == 0) {
        centered_note(msg::viewport_rendering);
        return;
    }

    // One line of numbers: the resolution the loss sees, what the file itself
    // holds when the two differ, how many views a split image became, the
    // score, and the step the render came from.
    ui::TextDisabled(msg::compare_trained_size, {_shot.view_w, _shot.view_h});
    ui::help_on_hover(msg::compare_trained_size_help);
    if (_shot.job.source_gt && _shot.src_w > 0) {
        ImGui::SameLine();
        ui::TextDisabled(msg::compare_source_size, {_shot.src_w, _shot.src_h});
    }
    if (_shot.views > 1) {
        ImGui::SameLine();
        ui::TextDisabled(msg::compare_faces, {_shot.views});
        ui::help_on_hover(msg::compare_faces_help);
    }
    ImGui::SameLine();
    if (_shot.psnr >= 0) {
        char v[32];
        std::snprintf(v, sizeof v, "%.2f", (double)_shot.psnr);
        ui::TextDisabled(msg::compare_psnr, {v});
    } else {
        ui::TextDisabled(msg::compare_psnr_none);
    }
    ui::help_on_hover(msg::compare_psnr_help);
    if (_shot.step >= 0) {
        ImGui::SameLine();
        ui::TextDisabled(msg::compare_at_step, {(long long)_shot.step});
    }
    if (!_shot.err_map.empty()) {
        ImGui::SameLine();
        // %g, because the modes differ by orders of magnitude.
        char mean[24], max[24];
        std::snprintf(mean, sizeof mean, "%.3g", (double)_shot.err_mean);
        std::snprintf(max, sizeof max, "%.3g", (double)_shot.err_max);
        ui::TextDisabled(msg::compare_error_stats, {mean, max});
        ui::help_on_hover(msg::compare_show_error_help);
    }

    ImVec2 avail = ImGui::GetContentRegionAvail();
    avail.x = std::max(avail.x, 64.0f);
    avail.y = std::max(avail.y, 64.0f);
    draw_panes(avail);
}

}  // namespace gui

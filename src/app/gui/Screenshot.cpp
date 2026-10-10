// Screenshot.cpp -- see Screenshot.h.

#include "app/gui/Screenshot.h"

#include "app/TrainerCore.h"
#include "app/gui/CompareView.h"
#include "app/gui/GuiApp.h"
#include "app/gui/Ui.h"
#include "app/gui/ViewportPanel.h"
#include "backend/api/BackendRuntime.h"
#include "data/JsonWrite.h"
#include "external/miniz.h"
#include "external/stb_image_write.h"
#include "i18n/catalog/Gui.h"

#include "app_generated/ui_font.h"   // kUiFont

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "imstb_truetype.h"

#include "imgui.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <future>
#include <map>
#include <mutex>
#include <stdexcept>

namespace fs = std::filesystem;
namespace msg = spirula::i18n::msg::gui;

namespace gui {

namespace {

constexpr const char* kPrefix = "screenshot.";
// How much the info box darkens what is behind it: enough to read on, light
// enough to still see the picture.
constexpr float kBoxShade = 0.4f;

std::tm local_now() {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return tm;
}

// What Windows refuses in a file name, and what no shell wants either:
// spaces become dashes, so "View 1" is "View-1".
std::string file_safe(const std::string& s) {
    std::string out;
    for (char c : s)
        out += c == ' ' ? '-'
               : (unsigned char)c < 0x20 || std::strchr("<>:\"/\\|?*", c) ? '_' : c;
    while (!out.empty() && (out.back() == ' ' || out.back() == '.')) out.pop_back();
    return out;
}

bool is_sep(char c) { return c == '_' || c == '-' || c == ' ' || c == '.'; }

// {key} -> value. An empty value takes one separator with it, so
// "{run}_{view}_{n}" without a view is "run_001", not "run__001".
std::string expand(const std::string& pat, const std::map<std::string, std::string>& vars) {
    std::string out;
    for (size_t i = 0; i < pat.size();) {
        const size_t close = pat[i] == '{' ? pat.find('}', i) : std::string::npos;
        const auto it = close == std::string::npos
                            ? vars.end() : vars.find(pat.substr(i + 1, close - i - 1));
        if (it == vars.end()) {
            out += pat[i++];
            continue;
        }
        i = close + 1;
        if (!it->second.empty()) out += it->second;
        else if (!out.empty() && is_sep(out.back())) out.pop_back();
        else if (out.empty() && i < pat.size() && is_sep(pat[i])) i++;
    }
    return out;
}

std::string with_commas(int64_t v) {
    std::string s = std::to_string(v < 0 ? -v : v), out;
    for (size_t i = 0; i < s.size(); i++) {
        if (i && (s.size() - i) % 3 == 0) out += ',';
        out += s[i];
    }
    return v < 0 ? "-" + out : out;
}

std::string splat_count(int64_t v) {
    char buf[32];
    if (v >= 1000000) std::snprintf(buf, sizeof buf, "%.2f M", v / 1e6);
    else return with_commas(v);
    return buf;
}

// ---- the info box, drawn with the UI's own font into the pixels ----

struct Font {
    stbtt_fontinfo info{};
    bool ok = false;
    float scale = 1.0f;
    int ascent = 0;
    Font(float px) {
        ok = stbtt_InitFont(&info, kUiFont, stbtt_GetFontOffsetForIndex(kUiFont, 0)) != 0;
        if (!ok) return;
        scale = stbtt_ScaleForPixelHeight(&info, px);
        int desc = 0, gap = 0;
        stbtt_GetFontVMetrics(&info, &ascent, &desc, &gap);
    }
    // Width of `s`; drawn at (x, baseline) as well when `img` is given, in
    // `ink` at `strength` coverage.
    float run(const std::string& s, float x, float baseline, uint8_t* img = nullptr,
              int W = 0, int H = 0, uint8_t ink = 255, float strength = 1.0f) const {
        const float x0 = x;
        std::vector<uint8_t> bm;
        for (size_t i = 0; i < s.size(); i++) {
            const int c = (unsigned char)s[i];
            int adv = 0, lsb = 0;
            stbtt_GetCodepointHMetrics(&info, c, &adv, &lsb);
            if (img) {
                int bx0, by0, bx1, by1;
                stbtt_GetCodepointBitmapBox(&info, c, scale, scale, &bx0, &by0, &bx1, &by1);
                const int bw = bx1 - bx0, bh = by1 - by0;
                if (bw > 0 && bh > 0) {
                    bm.assign((size_t)bw * bh, 0);
                    stbtt_MakeCodepointBitmap(&info, bm.data(), bw, bh, bw, scale, scale, c);
                    const int ox = (int)std::lround(x) + bx0, oy = (int)std::lround(baseline) + by0;
                    for (int r = 0; r < bh; r++)
                        for (int q = 0; q < bw; q++) {
                            const int px = ox + q, py = oy + r;
                            if (px < 0 || py < 0 || px >= W || py >= H) continue;
                            const float a = bm[(size_t)r * bw + q] / 255.0f * strength;
                            uint8_t* d = img + ((size_t)py * W + px) * 3;
                            for (int k = 0; k < 3; k++) d[k] = (uint8_t)(d[k] + (ink - d[k]) * a);
                        }
                }
            }
            x += adv * scale;
            if (i + 1 < s.size())
                x += stbtt_GetCodepointKernAdvance(&info, c, (unsigned char)s[i + 1]) * scale;
        }
        return x - x0;
    }
};

// Labels in English, like the numbers they sit beside: an image is compared
// across runs and machines, not read in the UI's language.
void draw_overlay(ShotJob& j) {
    std::vector<std::string> lines;
    if (j.step > 0 && j.total > 0) lines.push_back("Step  " + with_commas(j.step) + " / " + with_commas(j.total));
    lines.push_back("Splats  " + splat_count(j.splats) +
                    (j.cap > 0 ? " / " + splat_count(j.cap) : std::string()));
    if (j.has_vram) {
        char buf[64];
        std::snprintf(buf, sizeof buf, "VRAM  %.1f / %.1f GiB", j.vram_used / 1073741824.0,
                      j.vram_total / 1073741824.0);
        lines.push_back(buf);
    }
    const float s = j.H / 1080.0f;
    const Font font(26.0f * s);
    if (!font.ok) return;
    const float lh = 34.0f * s, pad = 14.0f * s, margin = 24.0f * s;
    float tw = 0.0f;
    for (const std::string& l : lines) tw = std::max(tw, font.run(l, 0, 0));
    const int bx1 = j.W - (int)margin, by1 = j.H - (int)margin;
    const int bx0 = std::max(0, bx1 - (int)(tw + 2 * pad));
    const int by0 = std::max(0, by1 - (int)(lines.size() * lh + 2 * pad));
    for (int y = by0; y < by1; y++)
        for (int x = bx0; x < bx1; x++) {
            uint8_t* d = j.rgb.data() + ((size_t)y * j.W + x) * 3;
            for (int k = 0; k < 3; k++) d[k] = (uint8_t)(d[k] * (1.0f - kBoxShade));
        }
    // A soft dark halo under the white: the box lets the picture through, and
    // a bright sky or wall behind it must not swallow the text.
    const float asc = font.ascent * font.scale;
    const float o = std::max(1.0f, 1.5f * s);
    for (size_t i = 0; i < lines.size(); i++) {
        const float x = bx0 + pad, y = by0 + pad + i * lh + asc + (lh - 26.0f * s) * 0.5f;
        const float halo[4][2] = {{o, o}, {-o, o}, {o, -o}, {-o, -o}};
        for (const auto& d : halo)
            font.run(lines[i], x + d[0], y + d[1], j.rgb.data(), j.W, j.H, 0, 0.35f);
        font.run(lines[i], x, y, j.rgb.data(), j.W, j.H);
    }
}

std::string metadata(const ShotJob& j) {
    JsonWriter w;
    w.object();
    w.field("app", "Spirula Studio");
    w.field("dataset", j.dataset_dir);
    w.field("run", j.run);
    w.field("view", j.view);
    w.field("step", j.step);
    w.field("total_steps", j.total);
    w.field("splats", (long long)j.splats);
    w.field("cap_max", (long long)j.cap);
    if (j.has_vram) {
        w.field("vram_used_bytes", (long long)j.vram_used);
        w.field("vram_total_bytes", (long long)j.vram_total);
    }
    w.field("width", j.W);
    w.field("height", j.H);
    if (j.pose.set) {
        w.key("camera").object();
        w.field("frame", "dataset");
        w.key("pos").array();
        for (double v : j.pose.pos) w.raw(json_number_exact(v));
        w.end();
        w.key("rot_wxyz").array();
        for (double v : j.pose.rot) w.raw(json_number_exact(v));
        w.end();
        w.key("target").array();
        for (double v : j.pose.target) w.raw(json_number_exact(v));
        w.end();
        w.field("camera_model", j.pose.cam_model);
        w.field("fov_deg", j.pose.fov_deg);
        w.field("ortho", j.pose.ortho);
        w.end();
    }
    w.end();
    return w.str();
}

void append(void* ctx, void* data, int n) {
    auto* v = static_cast<std::vector<uint8_t>*>(ctx);
    v->insert(v->end(), (uint8_t*)data, (uint8_t*)data + n);
}

// The JSON as a JPEG COM segment right after SOI, or a PNG iTXt chunk right
// after IHDR -- where every reader looks for a comment.
void embed(std::vector<uint8_t>& f, const std::string& text, bool png) {
    std::vector<uint8_t> seg;
    if (!png) {
        const size_t n = std::min<size_t>(text.size(), 65533);
        seg = {0xFF, 0xFE, (uint8_t)((n + 2) >> 8), (uint8_t)((n + 2) & 0xFF)};
        seg.insert(seg.end(), text.begin(), text.begin() + (long)n);
        if (f.size() >= 2) f.insert(f.begin() + 2, seg.begin(), seg.end());
        return;
    }
    std::vector<uint8_t> body = {'i', 'T', 'X', 't'};
    const char kw[] = "Comment";
    body.insert(body.end(), kw, kw + sizeof kw);   // with its NUL
    body.insert(body.end(), {0, 0, 0, 0});          // uncompressed, no language
    body.insert(body.end(), text.begin(), text.end());
    const uint32_t len = (uint32_t)(body.size() - 4);
    const uint32_t crc = (uint32_t)mz_crc32(MZ_CRC32_INIT, body.data(), body.size());
    seg = {(uint8_t)(len >> 24), (uint8_t)(len >> 16), (uint8_t)(len >> 8), (uint8_t)len};
    seg.insert(seg.end(), body.begin(), body.end());
    seg.insert(seg.end(), {(uint8_t)(crc >> 24), (uint8_t)(crc >> 16), (uint8_t)(crc >> 8),
                           (uint8_t)crc});
    if (f.size() >= 33) f.insert(f.begin() + 33, seg.begin(), seg.end());
}

// "step-000009644.ckpt" -> 9644; 0 for anything else.
int checkpoint_step(const std::string& folder) {
    if (folder.rfind("step-", 0) != 0) return 0;
    return std::atoi(folder.c_str() + 5);
}

// The saved view the camera sits on, as the tooltip names it; "" for none.
std::string view_name(const ViewportPanel& v) {
    const int slot = v.current_view();
    if (slot < 0) return {};
    const ViewBookmark& b = v.views()->slot[(size_t)slot];
    return b.name.empty() ? ui::format(msg::view_default_name, {slot + 1}) : b.name;
}

// Hand the frame to a writer thread; the future yields the path written.
std::future<std::string> start_write(ShotJob job, ViewResult&& r) {
    job.W = r.W;
    job.H = r.H;
    job.rgb = std::move(r.rgb8);
    return std::async(std::launch::async,
                      [job = std::move(job)]() mutable { return write_screenshot(job); });
}

std::string dataset_dir_of(const std::string& data) {
    std::error_code ec;
    fs::path d = fs::u8path(data);
    if (!fs::is_directory(d, ec)) d = d.parent_path();
    return d.u8string();
}

}  // namespace

bool read_screenshot_setting(ScreenshotSettings& s, const std::string& k,
                             const std::string& v) {
    if (k.rfind(kPrefix, 0) != 0) return false;
    const std::string key = k.substr(std::strlen(kPrefix));
    if (key == "folder") s.folder = v;
    else if (key == "name") s.name = v;
    else if (key == "format") s.png = v == "png";
    else if (key == "quality") s.quality = std::clamp(std::atoi(v.c_str()), 1, 100);
    else if (key == "width") s.width = std::clamp(std::atoi(v.c_str()), 16, 8192);
    else if (key == "height") s.height = std::clamp(std::atoi(v.c_str()), 16, 8192);
    else if (key == "overlay") s.overlay = v != "0";
    return true;
}

void write_screenshot_settings(FILE* f, const ScreenshotSettings& s) {
    std::fprintf(f, "%sfolder=%s\n", kPrefix, s.folder.c_str());
    std::fprintf(f, "%sname=%s\n", kPrefix, s.name.c_str());
    std::fprintf(f, "%sformat=%s\n", kPrefix, s.png ? "png" : "jpg");
    std::fprintf(f, "%squality=%d\n", kPrefix, s.quality);
    std::fprintf(f, "%swidth=%d\n", kPrefix, s.width);
    std::fprintf(f, "%sheight=%d\n", kPrefix, s.height);
    std::fprintf(f, "%soverlay=%d\n", kPrefix, s.overlay ? 1 : 0);
}

std::string screenshot_folder(const ScreenshotSettings& s, const std::string& dataset_dir,
                              const std::string& dataset, const std::string& run) {
    const std::map<std::string, std::string> vars = {{"dataset", file_safe(dataset)},
                                                     {"run", file_safe(run)}};
    fs::path p = fs::u8path(expand(s.folder.empty() ? "Screenshots" : s.folder, vars));
    if (p.is_relative()) p = fs::u8path(dataset_dir) / p;
    return p.lexically_normal().u8string();
}

std::string write_screenshot(ShotJob& j) {
    if (j.rgb.size() != (size_t)j.W * j.H * 3 || j.W <= 0 || j.H <= 0)
        throw std::runtime_error("empty render");
    if (j.cfg.overlay) draw_overlay(j);

    const fs::path dir = fs::u8path(screenshot_folder(j.cfg, j.dataset_dir, j.dataset, j.run));
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (!fs::is_directory(dir, ec)) throw std::runtime_error("cannot create " + dir.u8string());

    const std::tm tm = local_now();
    char date[16], tod[16];
    std::strftime(date, sizeof date, "%Y-%m-%d", &tm);
    std::strftime(tod, sizeof tod, "%H%M%S", &tm);
    const std::map<std::string, std::string> vars = {
        {"run", file_safe(j.run)},     {"view", file_safe(j.view)},
        {"step", std::to_string(j.step)}, {"dataset", file_safe(j.dataset)},
        {"date", date},                {"time", tod},
        {"n", "\x01"}};                // split on below
    // Split on the {n} marker BEFORE cleaning, which would turn it into '_'.
    std::string name = expand(j.cfg.name.empty() ? "{run}_{n}" : j.cfg.name, vars);
    if (name.find('\x01') == std::string::npos) name += "_\x01";
    const std::string ext = j.cfg.png ? ".png" : ".jpg";
    const size_t at = name.find('\x01');
    const std::string head = file_safe(name.substr(0, at));
    const std::string tail = file_safe(name.substr(at + 1)) + ext;

    // Two shots in quick succession must not both pick the same number.
    static std::mutex mu;
    std::lock_guard<std::mutex> lk(mu);
    int next = 1;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        const std::string f = e.path().filename().u8string();
        if (f.size() <= head.size() + tail.size() || f.compare(0, head.size(), head) != 0 ||
            f.compare(f.size() - tail.size(), tail.size(), tail) != 0)
            continue;
        const std::string mid = f.substr(head.size(), f.size() - head.size() - tail.size());
        if (mid.find_first_not_of("0123456789") == std::string::npos)
            next = std::max(next, std::atoi(mid.c_str()) + 1);
    }
    char num[16];
    std::snprintf(num, sizeof num, "%03d", next);
    const fs::path out = dir / fs::u8path(head + num + tail);

    std::vector<uint8_t> bytes;
    const int ok = j.cfg.png
        ? stbi_write_png_to_func(append, &bytes, j.W, j.H, 3, j.rgb.data(), j.W * 3)
        : stbi_write_jpg_to_func(append, &bytes, j.W, j.H, 3, j.rgb.data(),
                                 std::clamp(j.cfg.quality, 1, 100));
    if (!ok || bytes.empty()) throw std::runtime_error("encoding failed");
    embed(bytes, metadata(j), j.cfg.png);
    std::ofstream f(out, std::ios::binary | std::ios::trunc);
    f.write((const char*)bytes.data(), (std::streamsize)bytes.size());
    if (!f) throw std::runtime_error("cannot write " + out.u8string());
    return out.u8string();
}

// ---------------------------------------------------------------------------
// ViewportPanel members
// ---------------------------------------------------------------------------

bool ViewportPanel::request_capture(int W, int H, std::function<void(ViewResult&&)> done) {
    if (_mode != Mode::Engine || _shot_done) return false;
    _shot_w = W;
    _shot_h = H;
    _shot_done = std::move(done);
    _dirty = true;
    return true;
}

// Submitted only when nothing else is in flight, and nothing is submitted
// while it is, so the worker's latest-wins queue can never drop it.
void ViewportPanel::submit_capture() {
    ViewRequest q;
    build_request(q, _shot_w, _shot_h);
    q.show_cams = false;
    q.show_grid = false;
    q.show_roi = false;
    _pending = _worker.submit(q);
    _shot_inflight = true;
}

void ViewportPanel::poll_capture() {
    ViewResult res;
    if (!_worker.try_get_result(_pending, res)) return;
    _pending = 0;
    _shot_inflight = false;
    _dirty = true;
    std::function<void(ViewResult&&)> done = std::move(_shot_done);
    _shot_done = nullptr;
    if (done) done(std::move(res));
}

// ---------------------------------------------------------------------------
// GuiApp members
// ---------------------------------------------------------------------------

void GuiApp::poll_screenshot_jobs() {
    for (size_t i = 0; i < _shot_jobs.size();) {
        std::future<std::string>& f = _shot_jobs[i];
        if (f.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            i++;
            continue;
        }
        try {
            log(ui::format(msg::screenshot_saved, {f.get()}));
        } catch (const std::exception& e) {
            log(ui::format(msg::screenshot_failed, {std::string(e.what())}));
        }
        _shot_jobs.erase(_shot_jobs.begin() + (long)i);
    }
}

void GuiApp::draw_screenshot_settings() {
    if (!ImGui::BeginPopup("##shotpopup")) return;
    bool changed = false;
    ui::Text(msg::screenshot_folder);
    ImGui::SetNextItemWidth(px(300.0f));
    ui::InputTextRaw("##shotdir", &_shot.folder);
    changed |= ImGui::IsItemDeactivatedAfterEdit();
    ui::help_on_hover(msg::screenshot_folder_help);
    ui::Text(msg::screenshot_name);
    ImGui::SetNextItemWidth(px(300.0f));
    ui::InputTextRaw("##shotname", &_shot.name);
    changed |= ImGui::IsItemDeactivatedAfterEdit();
    ui::help_on_hover(msg::screenshot_name_help);

    ImGui::AlignTextToFramePadding();
    ui::Text(msg::screenshot_format);
    ImGui::SameLine();
    if (ui::RadioButtonRaw("JPG", !_shot.png)) _shot.png = false, changed = true;
    ImGui::SameLine();
    if (ui::RadioButtonRaw("PNG", _shot.png)) _shot.png = true, changed = true;
    if (!_shot.png) {
        ImGui::AlignTextToFramePadding();
        ui::Text(msg::screenshot_quality);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(px(140.0f));
        ui::SliderIntRaw("##shotq", &_shot.quality, 50, 100, "%d");
        changed |= ImGui::IsItemDeactivatedAfterEdit();
    }
    ImGui::AlignTextToFramePadding();
    ui::Text(msg::screenshot_size);
    ImGui::SameLine();
    int wh[2] = {_shot.width, _shot.height};
    ImGui::SetNextItemWidth(px(160.0f));
    if (ui::InputInt2Raw("##shotwh", wh)) {
        _shot.width = std::clamp(wh[0], 16, 8192);
        _shot.height = std::clamp(wh[1], 16, 8192);
    }
    changed |= ImGui::IsItemDeactivatedAfterEdit();
    changed |= ui::Checkbox(msg::screenshot_overlay, &_shot.overlay);
    ui::help_on_hover(msg::screenshot_overlay_help);
    ImGui::Separator();
    if (ui::Button(msg::screenshot_defaults)) {
        _shot = ScreenshotSettings();
        changed = true;
    }
    ImGui::EndPopup();
    if (changed) save_settings();
}

void GuiApp::draw_screenshot_controls(float right) {
    poll_screenshot_jobs();
    const ImGuiStyle& st = ImGui::GetStyle();
    const float bw = ImGui::CalcTextSize(msg::screenshot.get()).x + 2.0f * st.FramePadding.x;
    const float ow = ImGui::CalcTextSize("...").x + 2.0f * st.FramePadding.x;
    ImGui::SameLine(std::max(0.0f, ImGui::GetContentRegionMax().x - right - bw - ow -
                                       2.0f * st.ItemSpacing.x));
    spirula::TrainerSession* s = _runner.session();
    const bool ready = s && _viewport.attached() && !_preview_images;
    ImGui::BeginDisabled(!ready || _viewport.capture_pending());
    if (ui::Button(msg::screenshot)) take_screenshot();
    ImGui::EndDisabled();
    if (ready) {
        const std::string dir = screenshot_folder(
            _shot, dataset_dir_of(s->cfg.data),
            fs::u8path(dataset_dir_of(s->cfg.data)).filename().u8string(),
            s->out_dir.filename().u8string());
        ui::help_on_hover_raw(
            ui::format(msg::screenshot_help, {_shot.width, _shot.height, dir}).c_str(),
            ImGuiHoveredFlags_AllowWhenDisabled);
    } else {
        ui::help_on_hover_disabled(msg::screenshot_unavailable);
    }
    ImGui::SameLine();
    if (ui::ButtonRaw("...##shotopts")) ImGui::OpenPopup("##shotpopup");
    ui::help_on_hover(msg::screenshot_settings);
    draw_screenshot_settings();
}

void GuiApp::take_screenshot() {
    spirula::TrainerSession* s = _runner.session();
    if (!s || !_viewport.attached()) return;
    ShotJob job;
    job.cfg = _shot;
    job.dataset_dir = dataset_dir_of(s->cfg.data);
    job.dataset = fs::u8path(job.dataset_dir).filename().u8string();
    job.run = s->out_dir.filename().u8string();
    job.view = view_name(_viewport);
    job.pose = _viewport.current_pose();
    job.cap = s->cfg.cap_max;
    const int fallback_total = s->cfg.num_iterations;
    _viewport.request_capture(
        std::clamp(_shot.width, 16, 8192), std::clamp(_shot.height, 16, 8192),
        [this, job, fallback_total](ViewResult&& r) mutable {
            if (!r.error.empty()) {
                log(ui::format(msg::screenshot_failed, {r.error}));
                return;
            }
            // Read now, beside the render, rather than at the click.
            const spirula::TrainerProgress p = _runner.latest_progress();
            job.step = p.step + 1;
            job.total = p.total_steps > 0 ? p.total_steps : fallback_total;
            job.splats = p.num_splats;
            const backend::MemoryUsage m = backend::memory_usage();
            job.has_vram = m.has_total && m.total_bytes > 0 && (m.has_used || m.has_process);
            job.vram_used = m.has_used ? m.used_bytes : m.process_bytes;
            job.vram_total = m.total_bytes;
            _shot_jobs.push_back(start_write(std::move(job), std::move(r)));
        });
}

void GuiApp::draw_viewer_screenshot_controls() {
    poll_screenshot_jobs();
    const std::vector<CompareView::PaneShot> panes = _compare.splat_panes();
    bool busy = false;
    for (const CompareView::PaneShot& p : panes) busy |= p.panel->capture_pending();
    ImGui::BeginDisabled(panes.empty() || busy);
    if (ui::Button(msg::screenshot)) take_viewer_screenshots();
    ImGui::EndDisabled();
    if (panes.empty())
        ui::help_on_hover_disabled(msg::screenshot_viewer_unavailable);
    else
        ui::help_on_hover_raw(
            ui::format(msg::screenshot_viewer_help, {_shot.width, _shot.height}).c_str(),
            ImGuiHoveredFlags_AllowWhenDisabled);
    ImGui::SameLine();
    if (ui::ButtonRaw("...##shotopts")) ImGui::OpenPopup("##shotpopup");
    ui::help_on_hover(msg::screenshot_settings);
    draw_screenshot_settings();
}

// A file has no live run behind it: the numbers come from the run folder it
// sits in, and VRAM is left out -- now it would describe this PC, not the run.
void GuiApp::take_viewer_screenshots() {
    const int W = std::clamp(_shot.width, 16, 8192), H = std::clamp(_shot.height, 16, 8192);
    for (const CompareView::PaneShot& p : _compare.splat_panes()) {
        const FileHome home = file_home(p.asked, p.file);
        if (home.folder.empty()) continue;
        const fs::path f = fs::u8path(p.file.empty() ? p.asked : p.file);
        ShotJob job;
        job.cfg = _shot;
        job.dataset_dir = home.folder;
        job.dataset = fs::u8path(home.folder).filename().u8string();
        job.run = home.run_dir.empty() ? f.stem().u8string()
                                       : fs::u8path(home.run_dir).filename().u8string();
        job.step = checkpoint_step(f.parent_path().filename().u8string());
        if (!home.run_dir.empty()) {
            try {
                const JsonValue cfg =
                    json_parse_file((fs::u8path(home.run_dir) / "config.json").u8string());
                job.total = (int)cfg.get_double("num_iterations", 0.0);
                job.cap = (int64_t)cfg.get_double("cap_max", 0.0);
            } catch (const std::exception&) {
            }
        }
        job.splats = p.splats;
        job.view = view_name(*p.panel);
        job.pose = p.panel->current_pose();
        p.panel->request_capture(W, H, [this, job](ViewResult&& r) mutable {
            if (!r.error.empty()) {
                log(ui::format(msg::screenshot_failed, {r.error}));
                return;
            }
            _shot_jobs.push_back(start_write(std::move(job), std::move(r)));
        });
    }
}

std::vector<CompareView::PaneShot> CompareView::splat_panes() {
    std::vector<PaneShot> out;
    for (auto& m : _models)
        if (m->attached && m->src.kind() == SplatViewer::Kind::Splats && m->panel.attached())
            out.push_back({&m->panel, m->path, m->src.file(), m->src.num_splats()});
    return out;
}

}  // namespace gui

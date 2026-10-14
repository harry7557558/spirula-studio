// PickSheetView.cpp -- see PickSheetView.h.

#include "app/gui/PickSheetView.h"

#include "app/gui/Layout.h"
#include "app/gui/Ui.h"
#include "core/ImageFile.h"
#include "i18n/catalog/Dataset.h"

#include "external/stb_image.h"

#include <algorithm>
#include <cmath>

namespace gui {

namespace dmsg = spirula::i18n::msg::dataset;

namespace {

// JPEGs decoded on the UI thread in one frame: about a millisecond each.
constexpr int kDecodesPerFrame = 6;
// How far past 1:1 the open frame magnifies, in screen pixels a source pixel.
constexpr float kMaxScale = 8.0f;

GLuint upload_rgb(const uint8_t* rgb, int w, int h) {
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb);
    return id;
}

const spirula::i18n::Msg& verdict_line(const Pick& p) {
    switch (p.verdict) {
        case app::PickVerdict::Blurry:  return dmsg::pick_blurry;
        case app::PickVerdict::Similar: return dmsg::pick_similar;
        case app::PickVerdict::Kept:    break;
    }
    return dmsg::pick_kept;
}

void verdict_text(const Pick& p) {
    if (p.verdict == app::PickVerdict::Similar)
        ui::Text(dmsg::pick_similar, {(long long)p.index, (long long)p.dist});
    else if (p.blur < 0.0f)
        ui::Text(dmsg::pick_unmeasured, {(long long)p.index});
    else
        ui::Text(verdict_line(p), {(long long)p.index, (long long)std::lround(p.blur)});
}

// The verdict as a mark: a tick for a frame kept, a cross for one left out.
void verdict_mark(ImDrawList* dl, ImVec2 at, float side, bool kept, const PickStyle& s) {
    const ImU32 ink = ImGui::GetColorU32(kept ? s.good : s.bad);
    const float w = std::max(2.0f, side * 0.12f);
    dl->AddRectFilled(at, ImVec2(at.x + side, at.y + side),
                      ImGui::GetColorU32(ImGuiCol_WindowBg, 0.85f), side * 0.2f);
    auto pt = [&](float x, float y) { return ImVec2(at.x + side * x, at.y + side * y); };
    if (kept) {
        const ImVec2 tick[3] = {pt(0.24f, 0.54f), pt(0.43f, 0.72f), pt(0.77f, 0.30f)};
        dl->AddPolyline(tick, 3, ink, 0, w);
    } else {
        dl->AddLine(pt(0.28f, 0.28f), pt(0.72f, 0.72f), ink, w);
        dl->AddLine(pt(0.72f, 0.28f), pt(0.28f, 0.72f), ink, w);
    }
}

}  // namespace

PickSheetView::~PickSheetView() {
    if (_loader.joinable()) _loader.join();
}

ImU32 PickSheetView::colour(const PickStyle& s, float blur) {
    if (blur < 0.0f) return ImGui::GetColorU32(ImGuiCol_TextDisabled);
    const float t = std::clamp(1.0f - blur / (2.0f * std::max(s.ref, 1.0f)), 0.0f, 1.0f);
    const ImVec4& a = t < 0.5f ? s.bad : s.mid;
    const ImVec4& b = t < 0.5f ? s.mid : s.good;
    const float u = t < 0.5f ? t * 2.0f : t * 2.0f - 1.0f;
    return ImGui::GetColorU32(ImVec4(a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u,
                                     a.z + (b.z - a.z) * u, 1.0f));
}

const PickSheetView::Tex* PickSheetView::texture(const PickSheet& sheet, size_t row,
                                                 size_t index, bool now) {
    const uint64_t key = ((uint64_t)row << 32) | (uint64_t)index;
    auto it = _tex.find(key);
    if (it != _tex.end()) {
        it->second.used = ++_clock;
        it->second.frame = _frame;
        return &it->second;
    }
    if (!now && _budget <= 0) return nullptr;
    _budget--;

    Tex t;
    t.used = ++_clock;
    t.frame = _frame;
    const std::vector<uint8_t> jpeg = sheet.jpeg(row, index);
    int w = 0, h = 0, c = 0;
    unsigned char* rgb = jpeg.empty() ? nullptr
                                      : stbi_load_from_memory(jpeg.data(), (int)jpeg.size(),
                                                              &w, &h, &c, 3);
    if (rgb) {
        t.id = upload_rgb(rgb, w, h);
        stbi_image_free(rgb);
        t.w = w;
        t.h = h;
        if (!_aspect.count(row)) _aspect[row] = (float)h / (float)w;
    }
    if (_tex.size() >= kPickTextures) {
        auto old = _tex.begin();
        for (auto i = _tex.begin(); i != _tex.end(); ++i)
            if (i->second.used < old->second.used) old = i;
        if (old->second.frame != _frame) {
            if (old->second.id) glDeleteTextures(1, &old->second.id);
            _tex.erase(old);
        }
    }
    return &(_tex[key] = t);
}

void PickSheetView::draw(const PickSheet& sheet, size_t row,
                         const std::vector<Pick>& picks, float width, float cell,
                         const PickStyle& style) {
    if (sheet.generation() != _gen) {
        destroy_gl();
        _gen = sheet.generation();
    }
    if (picks.empty() || width <= 0.0f) return;
    if (_frame != ImGui::GetFrameCount()) {
        _frame = ImGui::GetFrameCount();
        _budget = kDecodesPerFrame;
    }

    const bool pictures = cell > 0.0f;
    const float gap = px(2.0f);
    const float cw = pictures ? cell : px(9.0f);
    const auto shape = _aspect.find(row);
    const float ch = pictures ? cw * (shape != _aspect.end() ? shape->second : 0.75f) : cw;
    const size_t cols = (size_t)std::max(1, (int)((width + gap) / (cw + gap)));
    const ImVec2 g0 = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 blank = ImGui::GetColorU32(ImGuiCol_FrameBg);
    const bool hot = ImGui::IsWindowHovered();
    int hovered = -1;
    for (size_t i = 0; i < picks.size(); i++) {
        const ImVec2 lo(g0.x + (cw + gap) * (float)(i % cols),
                        g0.y + (ch + gap) * (float)(i / cols));
        const ImVec2 hi(lo.x + cw, lo.y + ch);
        if (!ImGui::IsRectVisible(lo, hi)) continue;
        const Pick& p = picks[i];
        const bool kept = p.verdict == app::PickVerdict::Kept;
        const ImU32 ink = colour(style, p.blur);
        if (!pictures) {
            // Hollow is left out; the colour is how sharp it was either way.
            if (kept) dl->AddRectFilled(lo, hi, ink, px(2.0f));
            else dl->AddRect(lo, hi, ink, px(2.0f));
        } else {
            const Tex* t = texture(sheet, row, i, false);
            if (t && t->id)
                dl->AddImage((ImTextureID)(intptr_t)t->id, lo, hi, ImVec2(0, 0),
                             ImVec2(1, 1), kept ? IM_COL32_WHITE : IM_COL32(255, 255, 255, 70));
            else
                dl->AddRectFilled(lo, hi, blank);
            dl->AddRect(lo, hi, ink, 0.0f, 0, px(kept ? 2.0f : 1.0f));
        }
        if (hot && ImGui::IsMouseHoveringRect(lo, hi)) hovered = (int)i;
    }
    const size_t lines = (picks.size() + cols - 1) / cols;
    ImGui::Dummy(ImVec2(width, (ch + gap) * (float)lines));

    if (hovered < 0) return;
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        open(sheet, row, hovered);
        return;
    }
    const Tex* t = texture(sheet, row, (size_t)hovered, true);
    ImGui::BeginTooltip();
    // Wrapped at the picture's width, so the tooltip is no wider than it.
    const bool pic = t && t->id;
    if (pic) ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + px((float)t->w));
    verdict_text(picks[(size_t)hovered]);
    if (pic) {
        ImGui::PopTextWrapPos();
        ImGui::Image((ImTextureID)(intptr_t)t->id, ImVec2(px((float)t->w), px((float)t->h)));
    }
    ImGui::EndTooltip();
}

// ---------------------------------------------------------------------------
// One frame, full size
// ---------------------------------------------------------------------------

void PickSheetView::open(const PickSheet& sheet, size_t row, int index) {
    _open_row = row;
    _open = index;
    _mag = 1.0f;
    _centre = ImVec2(0.5f, 0.5f);
    const std::string path = sheet.path(row, (size_t)index);
    if (path == _want) return;
    // A frame decodes in a few hundred milliseconds, so the one still in
    // flight is waited for rather than raced.
    if (_loader.joinable()) _loader.join();
    _want = path;
    {
        std::lock_guard<std::mutex> lk(_mu);
        _full = Full{};
    }
    if (path.empty()) return;
    _loader = std::thread([this, path] {
        Full f;
        f.path = path;
        f.done = true;
        int w = 0, h = 0, c = 0;
        imagefile::Info info;
        if (imagefile::handles(path)) {   // a photograph as it was given
            if (imagefile::decode_srgb8(path, imagefile::Options(), info, f.rgb).empty()) {
                f.w = info.width;
                f.h = info.height;
            } else {
                f.rgb.clear();
            }
        } else if (unsigned char* p = stbi_load(path.c_str(), &w, &h, &c, 3)) {
            f.rgb.assign(p, p + (size_t)w * h * 3);
            f.w = w;
            f.h = h;
            stbi_image_free(p);
        }
        std::lock_guard<std::mutex> lk(_mu);
        _full = std::move(f);
    });
}

void PickSheetView::close() {
    _open = -1;
    if (_full_tex) glDeleteTextures(1, &_full_tex);
    _full_tex = 0;
    _full_path.clear();
}

void PickSheetView::draw_viewer(const PickSheet& sheet, const std::vector<Pick>& picks,
                                ImVec2 size, const PickStyle& style) {
    if (_open < 0) return;
    if (sheet.generation() != _gen || (size_t)_open >= picks.size()) {
        close();
        return;
    }
    if (_frame != ImGui::GetFrameCount()) {
        _frame = ImGui::GetFrameCount();
        _budget = kDecodesPerFrame;
    }

    // The frame, once its thread has it. Over the largest texture a device
    // takes it stays a thumbnail, which is said below rather than hidden.
    bool missing = _want.empty();
    {
        std::lock_guard<std::mutex> lk(_mu);
        if (_full.done && _full.path == _want && _full_path != _want) {
            GLint limit = 0;
            glGetIntegerv(GL_MAX_TEXTURE_SIZE, &limit);
            if (_full_tex) glDeleteTextures(1, &_full_tex);
            _full_tex = 0;
            if (!_full.rgb.empty() && _full.w <= limit && _full.h <= limit) {
                _full_tex = upload_rgb(_full.rgb.data(), _full.w, _full.h);
                _full_w = _full.w;
                _full_h = _full.h;
            }
            _full_path = _want;
            _full.rgb = std::vector<uint8_t>();
        }
        missing = missing || (_full_path == _want && !_full_tex);
    }
    const bool full = _full_tex && _full_path == _want;
    const Tex* thumb = texture(sheet, _open_row, (size_t)_open, true);
    const GLuint tex = full ? _full_tex : (thumb ? thumb->id : 0);
    const float iw = full ? (float)_full_w : (thumb ? (float)thumb->w : 0.0f);
    const float ih = full ? (float)_full_h : (thumb ? (float)thumb->h : 0.0f);

    const float top = ImGui::GetCursorPosY();
    int step = 0;
    if (ui::Button(dmsg::pick_back)) {
        close();
        return;
    }
    ImGui::SameLine();
    if (ui::ArrowButtonRaw("##pickprev", ImGuiDir_Left)) step = -1;
    ImGui::SameLine();
    if (ui::ArrowButtonRaw("##picknext", ImGuiDir_Right)) step = 1;
    ImGui::SameLine();
    verdict_text(picks[(size_t)_open]);
    ui::help_on_hover(dmsg::pick_view_help);
    // Beside the caption, not under it: a line of its own would resize the
    // picture between one frame and the next.
    if (missing) {
        ImGui::SameLine();
        ui::TextDisabled(dmsg::pick_no_file);
    }

    const ImVec2 box(size.x, std::max(px(60.0f), size.y - (ImGui::GetCursorPosY() - top)));
    ImGui::BeginChild("##pickfull", box, 0,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const ImVec2 mid(at.x + box.x * 0.5f, at.y + box.y * 0.5f);
    ui::InvisibleButtonRaw("##pickcanvas", box);
    const bool hot = ImGui::IsItemHovered();
    if (tex && iw > 0.0f && ih > 0.0f) {
        const float fit = std::min(box.x / iw, box.y / ih);
        const float top_mag = std::max(1.0f, kMaxScale / fit);
        float scale = fit * _mag;
        const ImGuiIO& io = ImGui::GetIO();
        // The point of the picture under the pointer stays under it.
        auto zoom_to = [&](float mag) {
            const ImVec2 under(_centre.x + (io.MousePos.x - mid.x) / (iw * scale),
                               _centre.y + (io.MousePos.y - mid.y) / (ih * scale));
            _mag = std::clamp(mag, 1.0f, top_mag);
            scale = fit * _mag;
            _centre = ImVec2(under.x - (io.MousePos.x - mid.x) / (iw * scale),
                             under.y - (io.MousePos.y - mid.y) / (ih * scale));
        };
        if (hot && io.MouseWheel != 0.0f) zoom_to(_mag * std::pow(1.25f, io.MouseWheel));
        if (hot && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            zoom_to(_mag > 1.0f ? 1.0f : 1.0f / fit);
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
            _centre.x -= io.MouseDelta.x / (iw * scale);
            _centre.y -= io.MouseDelta.y / (ih * scale);
        }
        if (_mag <= 1.0f) _centre = ImVec2(0.5f, 0.5f);
        _centre.x = std::clamp(_centre.x, 0.0f, 1.0f);
        _centre.y = std::clamp(_centre.y, 0.0f, 1.0f);
        const ImVec2 lo(mid.x - _centre.x * iw * scale, mid.y - _centre.y * ih * scale);
        const ImVec2 hi(lo.x + iw * scale, lo.y + ih * scale);
        const Pick& p = picks[(size_t)_open];
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->PushClipRect(at, ImVec2(at.x + box.x, at.y + box.y), true);
        dl->AddImage((ImTextureID)(intptr_t)tex, lo, hi);
        // Its sharpness as a frame round what of it shows, and the verdict as
        // a mark in the pane's corner, where magnifying cannot push it away.
        const ImVec2 a(std::max(lo.x, at.x), std::max(lo.y, at.y));
        const ImVec2 b(std::min(hi.x, at.x + box.x), std::min(hi.y, at.y + box.y));
        dl->AddRect(a, b, colour(style, p.blur), 0.0f, 0, px(2.0f));
        verdict_mark(dl, ImVec2(a.x + px(8.0f), a.y + px(8.0f)), px(30.0f),
                     p.verdict == app::PickVerdict::Kept, style);
        dl->PopClipRect();
    }
    const bool keys = hot || ImGui::IsWindowFocused();
    ImGui::EndChild();

    if (keys && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        close();
        return;
    }
    // As a shortcut, or keyboard navigation takes the arrow first and walks
    // off to the nearest button instead of the next frame.
    const ImGuiInputFlags route = ImGuiInputFlags_RouteFocused |
                                  ImGuiInputFlags_RouteFromRootWindow | ImGuiInputFlags_Repeat;
    if (ImGui::Shortcut(ImGuiKey_LeftArrow, route)) step = -1;
    if (ImGui::Shortcut(ImGuiKey_RightArrow, route)) step = 1;
    const int next = _open + step;
    if (step != 0 && next >= 0 && (size_t)next < picks.size()) {
        const float mag = _mag;
        const ImVec2 centre = _centre;
        open(sheet, _open_row, next);
        // The same spot of the next frame: that is how two are compared.
        _mag = mag;
        _centre = centre;
    }
}

void PickSheetView::destroy_gl() {
    for (auto& kv : _tex)
        if (kv.second.id) glDeleteTextures(1, &kv.second.id);
    _tex.clear();
    _aspect.clear();
    close();
}

}  // namespace gui

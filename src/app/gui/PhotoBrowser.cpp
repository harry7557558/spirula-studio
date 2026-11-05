// PhotoBrowser.cpp -- see PhotoBrowser.h.

#include "app/gui/PhotoBrowser.h"

#include "app/gui/Layout.h"
#include "app/gui/Ui.h"
#include "core/HardwareThreads.h"
#include "i18n/catalog/SourcePreview.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace pmsg = spirula::i18n::msg::preview;

namespace gui {

namespace {

// Thumbnails kept on the GPU at once: a 192-pixel picture is ~110 KB, so the
// cap is ~50 MB however long the folder is.
constexpr size_t kMaxThumbs = 480;
// Uploads per frame, so that a page of thumbnails arriving together does not
// stall one frame for all of them.
constexpr int kUploadsPerFrame = 8;
// A request not renewed for this many frames has scrolled out of view.
constexpr uint64_t kStaleFrames = 20;

std::string count_text(long long v) {
    std::string out;
    ui::detail::group_digits(std::to_string(v).c_str(), 0, out);
    return out;
}

}  // namespace

PhotoBrowser::~PhotoBrowser() { stop(); }

void PhotoBrowser::start() {
    if (!_workers.empty()) return;
    _quit = false;
    _cancel = false;
    const unsigned n = std::clamp(spirula::hardware_threads() / 4, 2u, 4u);
    for (unsigned i = 0; i < n; i++) _workers.emplace_back([this] { work(); });
}

void PhotoBrowser::stop() {
    {
        std::lock_guard<std::mutex> lk(_mu);
        _quit = true;
    }
    _cancel = true;
    _cv.notify_all();
    for (std::thread& t : _workers) t.join();
    _workers.clear();
    _cancel = false;
}

void PhotoBrowser::forget_pictures() {
    std::lock_guard<std::mutex> lk(_mu);
    for (auto& [i, t] : _thumbs) t.tex.reset();
    _thumbs.clear();
    _generation++;
    _big_want = _big_index = _big_tex_index = -1;
    _big_made = 0;
    _big_pic = Picture{};
    _big_fresh = false;
    _big_error.clear();
    _big_tex.reset();
}

void PhotoBrowser::set(const std::string& key, std::vector<PhotoItem> items, PhotoLoader load) {
    if (key == _key && items.size() == _items.size()) return;
    stop();
    forget_pictures();
    _key = key;
    _items = std::move(items);
    _load = std::move(load);
    _any_mask = std::any_of(_items.begin(), _items.end(),
                            [](const PhotoItem& p) { return !p.mask.empty(); });
    _single = -1;
}

void PhotoBrowser::clear() {
    stop();
    forget_pictures();
    _key.clear();
    _items.clear();
    _load = nullptr;
    _single = -1;
}

void PhotoBrowser::open(size_t i) {
    if (i >= _items.size()) return;
    _single = (int)i;
}

void PhotoBrowser::destroy_gl() {
    std::lock_guard<std::mutex> lk(_mu);
    for (auto& [i, t] : _thumbs) {
        t.tex.reset();
        if (t.state == 2 && t.pic.empty()) t.state = 0;
    }
    _big_tex.reset();
    _big_tex_index = -1;
    _big_index = -1;
}

void PhotoBrowser::work() {
    std::unique_lock<std::mutex> lk(_mu);
    while (!_quit) {
        const uint64_t gen = _generation;
        if (_big_want >= 0 && _big_want < (int)_items.size() && !_big_busy &&
            (_big_index != _big_want || _big_made < _big_side)) {
            const int index = _big_want, side = _big_side;
            const PhotoItem item = _items[(size_t)index];
            const PhotoLoader load = _load;
            const bool mask = _with_mask;
            _big_busy = true;
            lk.unlock();
            Picture pic;
            std::string error;
            const bool ok = load && load(item, side, mask, pic, error, _cancel);
            lk.lock();
            _big_busy = false;
            if (gen != _generation) continue;
            _big_index = index;
            _big_made = side;
            _big_error = ok ? std::string() : error;
            if (ok) {
                _big_pic = std::move(pic);
                _big_fresh = true;
            }
            continue;
        }
        size_t pick = 0;
        Thumb* best = nullptr;
        const uint64_t floor = _clock > kStaleFrames ? _clock - kStaleFrames : 0;
        for (auto& [i, t] : _thumbs)
            if (t.state == 0 && t.wanted_at >= (double)floor &&
                (!best || t.wanted_at > best->wanted_at)) {
                best = &t;
                pick = i;
            }
        if (!best || pick >= _items.size()) {
            _cv.wait(lk);
            continue;
        }
        best->state = 1;
        const PhotoItem item = _items[pick];
        const PhotoLoader load = _load;
        const bool mask = _with_mask;
        const int side = _thumb_side;
        lk.unlock();
        Picture pic;
        std::string error;
        const bool ok = load && load(item, side, mask, pic, error, _cancel);
        lk.lock();
        if (gen != _generation) continue;
        auto it = _thumbs.find(pick);
        if (it == _thumbs.end()) continue;
        it->second.state = ok ? 2 : 3;
        if (ok) it->second.pic = std::move(pic);
    }
}

void PhotoBrowser::draw(float height, const std::function<void(size_t)>& info) {
    if (_items.empty()) return;
    start();
    if (_single >= (int)_items.size()) _single = -1;

    // Toolbar: which view, how big, and the masks when there are any.
    if (_single >= 0) {
        if (ui::Button(pmsg::back_to_grid)) {
            _single = -1;
            _scroll_to_single = true;
        }
    } else {
        ui::Text(pmsg::photo_count, {count_text((long long)_items.size())});
        ImGui::SameLine();
        ImGui::SetNextItemWidth(px(120.0f));
        ui::SliderFloatRaw("##thumbsize", &_cell, 80.0f, 320.0f, "");
        ui::help_on_hover(pmsg::thumb_size_help);
    }
    if (_any_mask) {
        ImGui::SameLine();
        bool m = _with_mask;
        if (ui::Checkbox(pmsg::show_masks, &m)) {
            _with_mask = m;
            forget_pictures();
        }
        ui::help_on_hover(pmsg::show_masks_help);
    }
    const float h = std::max(px(60.0f), height - ImGui::GetFrameHeightWithSpacing());
    if (_single >= 0) draw_single(h, info);
    else draw_grid(h);
}

void PhotoBrowser::draw_grid(float height) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(px(4.0f), px(4.0f)));
    ImGui::BeginChild("##photogrid", ImVec2(0, height));
    const float cell = px(_cell), gap = px(4.0f);
    const float avail = ImGui::GetContentRegionAvail().x;
    const int cols = std::max(1, (int)((avail + gap) / (cell + gap)));
    const int rows = (int)((_items.size() + cols - 1) / cols);
    {
        // Pictures sized for the cell, in steps, so the slider does not reload
        // every thumbnail on every pixel of a drag.
        const int want = std::clamp(((int)cell + 63) / 64 * 64, 128, 384);
        std::lock_guard<std::mutex> lk(_mu);
        if (want > _thumb_side) {
            _thumb_side = want;
            for (auto& [i, t] : _thumbs) t.tex.reset();
            _thumbs.clear();
            _generation++;
        }
    }

    const uint64_t now = ++_clock;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 back = ImGui::GetColorU32(ImGuiCol_FrameBg);
    const ImU32 edge = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
    const ImU32 last = ImGui::GetColorU32(ImGuiCol_PlotHistogram);
    int uploads = 0;
    std::vector<size_t> want;
    ImGuiListClipper clip;
    clip.Begin(rows, cell + gap);
    const int focus_row = _scroll_to_single && _big_tex_index >= 0 ? _big_tex_index / cols : -1;
    if (focus_row >= 0) clip.IncludeItemByIndex(focus_row);
    int opened = -1;
    while (clip.Step()) {
        for (int r = clip.DisplayStart; r < clip.DisplayEnd; r++) {
            for (int c = 0; c < cols; c++) {
                const size_t i = (size_t)r * cols + c;
                if (i >= _items.size()) break;
                if (c > 0) ImGui::SameLine();
                ImGui::PushID((int)i);
                const ImVec2 at = ImGui::GetCursorScreenPos();
                if (ui::InvisibleButtonRaw("##thumb", ImVec2(cell, cell))) opened = (int)i;
                const bool hovered = ImGui::IsItemHovered();
                ImGui::PopID();
                const ImVec2 corner(at.x + cell, at.y + cell);
                dl->AddRectFilled(at, corner, back);
                PreviewTexture* tex = nullptr;
                {
                    std::lock_guard<std::mutex> lk(_mu);
                    Thumb& t = _thumbs[i];
                    t.used = now;
                    if (t.state == 0) {
                        t.wanted_at = (double)now;
                        want.push_back(i);
                    }
                    if (t.state == 2 && !t.pic.empty() && uploads < kUploadsPerFrame) {
                        t.tex.upload(t.pic);
                        t.pic = Picture{};
                        uploads++;
                    }
                    if (t.tex.ready()) tex = &t.tex;
                }
                if (tex) {
                    ImVec2 lo, hi;
                    fit_rect((float)tex->w, (float)tex->h, at, ImVec2(cell, cell), lo, hi);
                    dl->AddImage(tex->id(), lo, hi);
                }
                if ((int)i == _big_tex_index)
                    dl->AddRect(at, corner, last, 0.0f, 0, px(2.0f));
                if (hovered) {
                    dl->AddRect(at, corner, edge, 0.0f, 0, px(2.0f));
                    const PhotoItem& it = _items[i];
                    if (it.w > 0)
                        ui::SetTooltipRaw(it.name + "\n" + std::to_string(it.w) + " x " +
                                          std::to_string(it.h));
                    else
                        ui::SetTooltipRaw(it.name);
                }
            }
            if (r == focus_row) {
                ImGui::SetScrollHereY(0.5f);
                _scroll_to_single = false;
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    if (!want.empty()) _cv.notify_all();
    if (opened >= 0) _single = opened;

    // Least recently drawn first, never one on screen or being loaded.
    std::lock_guard<std::mutex> lk(_mu);
    if (_thumbs.size() <= kMaxThumbs) return;
    std::vector<std::pair<uint64_t, size_t>> order;
    for (auto& [i, t] : _thumbs)
        if (t.used != now && t.state != 1) order.push_back({t.used, i});
    std::sort(order.begin(), order.end());
    for (size_t k = 0; k < order.size() && _thumbs.size() > kMaxThumbs; k++) {
        auto it = _thumbs.find(order[k].second);
        it->second.tex.reset();
        _thumbs.erase(it);
    }
}

void PhotoBrowser::draw_single(float height, const std::function<void(size_t)>& info) {
    const int n = (int)_items.size();
    ImGui::SameLine();
    ImGui::BeginDisabled(_single <= 0);
    if (ui::ArrowButtonRaw("##prev", ImGuiDir_Left)) _single--;
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(_single >= n - 1);
    if (ui::ArrowButtonRaw("##next", ImGuiDir_Right)) _single++;
    ImGui::EndDisabled();
    ImGui::SameLine();
    ui::TextRaw(count_text(_single + 1) + " / " + count_text(n));
    ImGui::SameLine();
    ui::TextDisabledRaw(elide_middle(_items[(size_t)_single].name,
                                     ImGui::GetContentRegionAvail().x));

    ImGui::BeginChild("##photosingle", ImVec2(0, height), 0,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) &&
        !ImGui::GetIO().WantTextInput) {
        // Shortcut() claims the key: imgui's nav would also walk the focus on it.
        const ImGuiInputFlags route = ImGuiInputFlags_RouteFocused |
                                      ImGuiInputFlags_RouteFromRootWindow;
        const ImGuiInputFlags rep = route | ImGuiInputFlags_Repeat;
        if (ImGui::Shortcut(ImGuiKey_LeftArrow, rep) && _single > 0) _single--;
        if (ImGui::Shortcut(ImGuiKey_RightArrow, rep) && _single < n - 1) _single++;
        if (ImGui::Shortcut(ImGuiKey_Home, route)) _single = 0;
        if (ImGui::Shortcut(ImGuiKey_End, route)) _single = n - 1;
        if (ImGui::Shortcut(ImGuiKey_Escape, route)) {
            _single = -1;
            _scroll_to_single = true;
            ImGui::EndChild();
            return;
        }
    }
    const float info_h = info ? ImGui::GetTextLineHeightWithSpacing() * 3.2f : 0.0f;
    const ImVec2 box(ImGui::GetContentRegionAvail().x,
                     std::max(px(40.0f), ImGui::GetContentRegionAvail().y - info_h));
    const int side = std::clamp((((int)std::max(box.x, box.y) + 255) / 256) * 256, 256, 4096);
    std::string error;
    {
        std::lock_guard<std::mutex> lk(_mu);
        _big_want = _single;
        _big_side = side;
        if (_big_fresh) {
            _big_tex.upload(_big_pic);
            _big_tex_index = _big_index;
            _big_pic = Picture{};
            _big_fresh = false;
        }
        if (_big_index == _single) error = _big_error;
    }
    _cv.notify_all();

    const ImVec2 at = ImGui::GetCursorScreenPos();
    ImGui::Dummy(box);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // Until the full picture arrives, its thumbnail stands in -- softer, but
    // where it should be, so stepping through a folder never flashes black.
    const PreviewTexture* tex = _big_tex_index == _single ? &_big_tex : nullptr;
    if (!tex) {
        std::lock_guard<std::mutex> lk(_mu);
        auto it = _thumbs.find((size_t)_single);
        if (it != _thumbs.end() && it->second.tex.ready()) tex = &it->second.tex;
    }
    if (tex && tex->ready()) {
        ImVec2 lo, hi;
        fit_rect((float)tex->w, (float)tex->h, at, box, lo, hi);
        dl->AddImage(tex->id(), lo, hi);
    }
    if (!error.empty()) {
        ImGui::SetCursorScreenPos(ImVec2(at.x + px(8.0f), at.y + px(8.0f)));
        ui::TextColoredWrappedRaw(ImVec4(0.95f, 0.45f, 0.40f, 1.0f), error);
    } else if (!tex) {
        ImGui::SetCursorScreenPos(ImVec2(at.x + px(8.0f), at.y + px(8.0f)));
        ui::TextDisabled(pmsg::loading);
    }
    ImGui::SetCursorScreenPos(ImVec2(at.x, at.y + box.y + ImGui::GetStyle().ItemSpacing.y));
    if (info) info((size_t)_single);
    ImGui::EndChild();
}

}  // namespace gui

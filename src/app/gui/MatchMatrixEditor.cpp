#include "app/gui/MatchMatrixEditor.h"

#include "app/gui/Layout.h"
#include "app/gui/Ui.h"
#include "i18n/catalog/Dataset.h"

#ifdef SS_TOOL_SFM
#include "sfm/core/Features.h"
#include "sfm/core/Matches.h"
#include "sfm/geometry/TwoView.h"
#endif

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>

namespace dmsg = spirula::i18n::msg::dataset;

namespace gui {

namespace {

constexpr ImU32 kFilteredCol = IM_COL32(18, 19, 24, 255);
constexpr ImU32 kNoMatchCol  = IM_COL32(104, 30, 34, 255);
constexpr ImU32 kMatchedCol  = IM_COL32(160, 230, 255, 255);

void legend_item(ImU32 col, const spirula::i18n::Msg& label) {
    const float h = ImGui::GetTextLineHeight();
    const ImVec2 at = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(at.x, at.y + h * 0.15f), ImVec2(at.x + h * 0.8f, at.y + h * 0.95f),
        col);
    ImGui::Dummy(ImVec2(h, h));
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    ui::TextDisabled(label);
}

}  // namespace

MatchMatrixEditor::MatchMatrixEditor() = default;
MatchMatrixEditor::~MatchMatrixEditor() = default;

void MatchMatrixEditor::configure(const std::string& image_dir, const std::string& mask_dir,
                                  const std::string& features_dir, const std::string& matches_path) {
    _image_dir = image_dir;
    _mask_dir = mask_dir;
    _features_dir = features_dir;
    _matches_path = matches_path;
    _stems = read_image_stems(features_dir, matches_path);
}

void MatchMatrixEditor::open() {
    _open = true;
    _selection.clear();
    _selected_matched_pairs.clear();
    _selected_pair_idx = 0;
    _status.clear();
    _zoom = 1.0f;
    _pan = ImVec2(0.0f, 0.0f);
    ui::OpenPopup(dmsg::matrix_editor_title);
}

uint64_t MatchMatrixEditor::count_selected_matches(const PairMatrix& pm) const {
    if (!_selection.active || pm.empty()) return 0;
    uint64_t total = 0;
    const uint32_t r_end = std::min(_selection.r1, pm.bins);
    const uint32_t c_end = std::min(_selection.c1, pm.bins);
    for (uint32_t r = _selection.r0; r < r_end; r++) {
        for (uint32_t c = _selection.c0; c < c_end; c++) {
            total += pm.at(r, c);
        }
    }
    return total;
}

uint32_t MatchMatrixEditor::count_selected_pairs(const PairMatrix& pm) const {
    if (!_selection.active || pm.empty()) return 0;
    uint32_t total = 0;
    const uint32_t r_end = std::min(_selection.r1, pm.bins);
    const uint32_t c_end = std::min(_selection.c1, pm.bins);
    for (uint32_t r = _selection.r0; r < r_end; r++) {
        for (uint32_t c = _selection.c0; c < c_end; c++) {
            if (pm.at(r, c) > 0) total++;
        }
    }
    return total;
}

void MatchMatrixEditor::update_selected_pairs(const PairMatrix& pm, PairPreview& preview) {
    _selected_matched_pairs.clear();
    _selected_pair_idx = 0;
    if (!_selection.active || pm.empty()) return;

    const uint32_t r_end = std::min(_selection.r1, pm.bins);
    const uint32_t c_end = std::min(_selection.c1, pm.bins);
    for (uint32_t r = _selection.r0; r < r_end; r++) {
        for (uint32_t c = _selection.c0; c < c_end; c++) {
            if (pm.at(r, c) > 0) {
                const uint32_t img_r = (uint32_t)((uint64_t)r * pm.n_images / pm.bins);
                const uint32_t img_c = (uint32_t)((uint64_t)c * pm.n_images / pm.bins);
                _selected_matched_pairs.push_back({img_r, img_c});
            }
        }
    }

    if (_selection.is_single()) {
        const uint32_t img_r = (uint32_t)((uint64_t)_selection.r0 * pm.n_images / pm.bins);
        const uint32_t img_c = (uint32_t)((uint64_t)_selection.c0 * pm.n_images / pm.bins);
        preview.show(PairBlock{img_r, img_r + 1, img_c, img_c + 1});
    } else if (!_selected_matched_pairs.empty()) {
        const auto& p = _selected_matched_pairs[0];
        preview.show(PairBlock{p.first, p.first + 1, p.second, p.second + 1});
    } else {
        preview.show(PairBlock{_selection.r0, _selection.r1, _selection.c0, _selection.c1});
    }
}

bool MatchMatrixEditor::remove_selected_matches(PairMatrix& pm, MatchMatrix& mm, PairPreview& preview) {
#ifdef SS_TOOL_SFM
    if (!_selection.active || _matches_path.empty()) return false;
    std::error_code ec;
    if (!std::filesystem::exists(_matches_path, ec)) return false;

    sfm::MatchesDatabase db;
    try {
        db = sfm::readMatches(_matches_path);
    } catch (const std::exception& e) {
        _status = std::string(e.what());
        return false;
    }

    const std::string bak_path = _matches_path + ".bak";
    if (!std::filesystem::exists(bak_path, ec)) {
        std::filesystem::copy_file(_matches_path, bak_path, ec);
    }

    UndoStep step;
    std::vector<sfm::TwoViewMatches> kept;
    kept.reserve(db.pairs.size());

    uint64_t matches_removed = 0;
    uint32_t pairs_removed = 0;

    for (auto& p : db.pairs) {
        if (_selection.contains(p.image1, p.image2)) {
            matches_removed += p.matches.size();
            pairs_removed++;
            step.removed.push_back(std::move(p));
        } else {
            kept.push_back(std::move(p));
        }
    }

    if (pairs_removed == 0) return false;

    db.pairs = std::move(kept);
    try {
        sfm::writeMatches(_matches_path, db);
    } catch (const std::exception& e) {
        _status = std::string(e.what());
        return false;
    }

    _undo_stack.push_back(std::move(step));

    int64_t dummy_mtime = 0;
    read_pair_matrix_from_matches(_matches_path, dummy_mtime, pm);
    mm.set(pm);

    update_selected_pairs(pm, preview);
    _status = "Removed " + std::to_string(matches_removed) + " matches across " +
              std::to_string(pairs_removed) + " pair(s).";
    return true;
#else
    (void)pm; (void)mm; (void)preview;
    return false;
#endif
}

bool MatchMatrixEditor::undo_removal(PairMatrix& pm, MatchMatrix& mm, PairPreview& preview) {
#ifdef SS_TOOL_SFM
    if (_undo_stack.empty() || _matches_path.empty()) return false;
    UndoStep step = std::move(_undo_stack.back());
    _undo_stack.pop_back();

    sfm::MatchesDatabase db;
    try {
        db = sfm::readMatches(_matches_path);
    } catch (const std::exception& e) {
        _status = std::string(e.what());
        return false;
    }

    const uint32_t restored_count = (uint32_t)step.removed.size();
    for (auto& p : step.removed) db.pairs.push_back(std::move(p));

    try {
        sfm::writeMatches(_matches_path, db);
    } catch (const std::exception& e) {
        _status = std::string(e.what());
        return false;
    }

    int64_t dummy_mtime = 0;
    read_pair_matrix_from_matches(_matches_path, dummy_mtime, pm);
    mm.set(pm);

    update_selected_pairs(pm, preview);
    _status = "Restored " + std::to_string(restored_count) + " pair(s).";
    return true;
#else
    (void)pm; (void)mm; (void)preview;
    return false;
#endif
}

bool MatchMatrixEditor::match_single_pair(uint32_t a, uint32_t b, PairMatrix& pm, MatchMatrix& mm, PairPreview& preview) {
#ifdef SS_TOOL_SFM
    if (_features_dir.empty() || _matches_path.empty()) return false;
    if (_stems.empty()) _stems = read_image_stems(_features_dir, _matches_path);
    if (a >= _stems.size() || b >= _stems.size() || a == b) return false;

    const std::string fa_path = (std::filesystem::path(_features_dir) / (_stems[a] + ".bin")).string();
    const std::string fb_path = (std::filesystem::path(_features_dir) / (_stems[b] + ".bin")).string();
    std::error_code ec;
    if (!std::filesystem::exists(fa_path, ec) || !std::filesystem::exists(fb_path, ec)) {
        _status = "Feature files not found.";
        return false;
    }

    sfm::FeatureSet fa, fb;
    try {
        fa = sfm::readFeatures(fa_path, true);
        fb = sfm::readFeatures(fb_path, true);
    } catch (const std::exception& e) {
        _status = std::string(e.what());
        return false;
    }

    if (fa.dtype != sfm::DType::U8 || fb.dtype != sfm::DType::U8 || fa.dim != 128 || fb.dim != 128) {
        _status = "Descriptor type not supported for on-demand matching.";
        return false;
    }

    const size_t na = fa.count(), nb = fb.count();
    if (na == 0 || nb == 0) {
        _status = "No features found in image.";
        return false;
    }

    std::vector<int> match_a_to_b(na, -1);
    const uint8_t* da = fa.descriptors.data();
    const uint8_t* db = fb.descriptors.data();
    for (size_t i = 0; i < na; i++) {
        const uint8_t* pa = da + i * 128;
        int best_d = std::numeric_limits<int>::max();
        int sec_d = std::numeric_limits<int>::max();
        int best_j = -1;
        for (size_t j = 0; j < nb; j++) {
            const uint8_t* pb = db + j * 128;
            int dist2 = 0;
            for (int k = 0; k < 128; k++) {
                int diff = (int)pa[k] - (int)pb[k];
                dist2 += diff * diff;
            }
            if (dist2 < best_d) {
                sec_d = best_d;
                best_d = dist2;
                best_j = (int)j;
            } else if (dist2 < sec_d) {
                sec_d = dist2;
            }
        }
        if (best_j >= 0 && (float)best_d < 0.64f * (float)sec_d) {
            match_a_to_b[i] = best_j;
        }
    }

    std::vector<sfm::FeatureMatch> putative;
    for (size_t j = 0; j < nb; j++) {
        const uint8_t* pb = db + j * 128;
        int best_d = std::numeric_limits<int>::max();
        int best_i = -1;
        for (size_t i = 0; i < na; i++) {
            const uint8_t* pa = da + i * 128;
            int dist2 = 0;
            for (int k = 0; k < 128; k++) {
                int diff = (int)pa[k] - (int)pb[k];
                dist2 += diff * diff;
            }
            if (dist2 < best_d) {
                best_d = dist2;
                best_i = (int)i;
            }
        }
        if (best_i >= 0 && match_a_to_b[best_i] == (int)j) {
            putative.push_back({(uint32_t)best_i, (uint32_t)j});
        }
    }

    if (putative.size() < 15) {
        _status = "Putative matches below threshold.";
        return false;
    }

    std::vector<sfm::Vec2> p1(putative.size()), p2(putative.size());
    for (size_t k = 0; k < putative.size(); k++) {
        p1[k] = {fa.keypoints[putative[k].idx1].x, fa.keypoints[putative[k].idx1].y};
        p2[k] = {fb.keypoints[putative[k].idx2].x, fb.keypoints[putative[k].idx2].y};
    }
    sfm::TwoViewOptions tvo;
    sfm::TwoViewGeometry tvg = sfm::estimateTwoView(p1, p2, tvo);
    if (tvg.num_inliers < tvo.min_num_inliers) {
        _status = "Verification found no valid geometry.";
        return false;
    }

    std::vector<sfm::FeatureMatch> inliers;
    for (size_t k = 0; k < putative.size(); k++) {
        if (k < tvg.inlier_mask.size() && tvg.inlier_mask[k])
            inliers.push_back(putative[k]);
    }

    sfm::MatchesDatabase mdb;
    try {
        mdb = sfm::readMatches(_matches_path);
    } catch (const std::exception& e) {
        _status = std::string(e.what());
        return false;
    }

    bool replaced = false;
    for (auto& p : mdb.pairs) {
        if ((p.image1 == a && p.image2 == b) || (p.image1 == b && p.image2 == a)) {
            p.image1 = a;
            p.image2 = b;
            p.config = (int32_t)tvg.config;
            p.matches = std::move(inliers);
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        sfm::TwoViewMatches tvm;
        tvm.image1 = a;
        tvm.image2 = b;
        tvm.config = (int32_t)tvg.config;
        tvm.matches = std::move(inliers);
        mdb.pairs.push_back(std::move(tvm));
    }

    try {
        sfm::writeMatches(_matches_path, mdb);
    } catch (const std::exception& e) {
        _status = std::string(e.what());
        return false;
    }

    int64_t dummy_mtime = 0;
    read_pair_matrix_from_matches(_matches_path, dummy_mtime, pm);
    mm.set(pm);
    preview.show(PairBlock{a, a + 1, b, b + 1});

    _status = "Matched pair: " + std::to_string(tvg.num_inliers) + " inliers verified.";
    return true;
#else
    (void)a; (void)b; (void)pm; (void)mm; (void)preview;
    return false;
#endif
}

bool MatchMatrixEditor::draw(PairMatrix& pm, MatchMatrix& mm, PairPreview& preview) {
    if (!_open) return false;

    const ImVec2 display_size = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize(
        ImVec2(std::min(display_size.x * 0.94f, px(1480.0f)),
               std::min(display_size.y * 0.90f, px(920.0f))),
        ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(
        ImVec2(display_size.x * 0.5f, display_size.y * 0.5f),
        ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    bool modified = false;
    if (!ui::BeginPopupModal(dmsg::matrix_editor_title, &_open, ImGuiWindowFlags_NoScrollbar))
        return false;

    if (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
        if (remove_selected_matches(pm, mm, preview)) modified = true;
    }
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
        if (undo_removal(pm, mm, preview)) modified = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        if (_selection.active) _selection.clear();
        else _open = false;
    }

    // Top toolbar
    if (ui::ButtonRaw("-##zoom_out")) _zoom = std::max(0.4f, _zoom / 1.35f);
    ImGui::SameLine();
    if (ui::ButtonRaw("+##zoom_in"))  _zoom = std::min(50.0f, _zoom * 1.35f);
    ImGui::SameLine();
    if (ui::Button(dmsg::matrix_reset_view)) {
        _zoom = 1.0f;
        _pan = ImVec2(0.0f, 0.0f);
    }
    ImGui::SameLine();
    ui::TextRaw(std::to_string((int)(_zoom * 100.0f)) + "%");

    ImGui::SameLine(0.0f, px(18.0f));
    ui::Checkbox(dmsg::matrix_symmetric_sel, &_selection.symmetric);

    const uint64_t sel_matches = count_selected_matches(pm);
    const uint32_t sel_pairs = count_selected_pairs(pm);

    ImGui::SameLine(0.0f, px(18.0f));
    const bool can_remove = _selection.active && sel_matches > 0;
    if (!can_remove) ImGui::BeginDisabled();
    if (ui::Button(dmsg::matrix_remove_matches)) {
        if (remove_selected_matches(pm, mm, preview)) modified = true;
    }
    if (!can_remove) ImGui::EndDisabled();

    ImGui::SameLine();
    const bool undo_avail = can_undo();
    if (!undo_avail) ImGui::BeginDisabled();
    if (ui::Button(dmsg::matrix_undo)) {
        if (undo_removal(pm, mm, preview)) modified = true;
    }
    if (!undo_avail) ImGui::EndDisabled();

    ImGui::SameLine();
    if (!_selection.active) ImGui::BeginDisabled();
    if (ui::Button(dmsg::matrix_clear_selection)) {
        _selection.clear();
        _selected_matched_pairs.clear();
    }
    if (!_selection.active) ImGui::EndDisabled();

    if (_selection.is_single()) {
        const uint32_t a = (uint32_t)((uint64_t)_selection.r0 * pm.n_images / pm.bins);
        const uint32_t b = (uint32_t)((uint64_t)_selection.c0 * pm.n_images / pm.bins);
        if (a != b) {
            ImGui::SameLine();
            if (ui::Button(dmsg::matrix_match_pair)) {
                if (match_single_pair(a, b, pm, mm, preview)) modified = true;
            }
        }
    }

    if (_selection.active) {
        ImGui::SameLine(0.0f, px(18.0f));
        std::string sel_info = "Selected: " + std::to_string(sel_pairs) + " pair(s), " +
                               std::to_string(sel_matches) + " matches";
        ui::TextDisabledRaw(sel_info);
    }

    ImGui::Separator();

    // Main workspace: Left Canvas + Right Pair Inspection
    const float avail_w = ImGui::GetContentRegionAvail().x;
    const float footer_h = px(36.0f);
    const float canvas_h = ImGui::GetContentRegionAvail().y - footer_h;
    const float left_w = std::max(px(320.0f), avail_w * 0.54f);
    const float right_w = avail_w - left_w - ImGui::GetStyle().ItemSpacing.x;

    // Left pane: Matrix canvas
    ImGui::BeginChild("##matrix_canvas_pane", ImVec2(left_w, canvas_h), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    {
        const ImVec2 cpos = ImGui::GetCursorScreenPos();
        const ImVec2 csize = ImGui::GetContentRegionAvail();
        const ImVec2 ccenter(cpos.x + csize.x * 0.5f, cpos.y + csize.y * 0.5f);
        const float base_side = std::max(px(100.0f), std::min(csize.x, csize.y) - px(16.0f));
        const float side = base_side * _zoom;
        const ImVec2 origin(ccenter.x - side * 0.5f + _pan.x, ccenter.y - side * 0.5f + _pan.y);

        ui::InvisibleButtonRaw("##canvas_area", csize,
                               ImGuiButtonFlags_MouseButtonLeft |
                               ImGuiButtonFlags_MouseButtonRight |
                               ImGuiButtonFlags_MouseButtonMiddle);
        const bool hovered = ImGui::IsItemHovered();
        const ImVec2 mpos = ImGui::GetIO().MousePos;

        if (hovered && ImGui::GetIO().MouseWheel != 0.0f) {
            const float factor = std::pow(1.22f, ImGui::GetIO().MouseWheel);
            const float new_zoom = std::clamp(_zoom * factor, 0.4f, 50.0f);
            const ImVec2 norm_m((mpos.x - origin.x) / side, (mpos.y - origin.y) / side);
            const float new_side = base_side * new_zoom;
            const ImVec2 new_origin(mpos.x - norm_m.x * new_side, mpos.y - norm_m.y * new_side);
            _pan.x = new_origin.x + new_side * 0.5f - ccenter.x;
            _pan.y = new_origin.y + new_side * 0.5f - ccenter.y;
            _zoom = new_zoom;
        }

        const bool space = ImGui::IsKeyDown(ImGuiKey_Space);
        const bool is_pan_btn = ImGui::IsMouseDown(ImGuiMouseButton_Middle) ||
                                ImGui::IsMouseDown(ImGuiMouseButton_Right) ||
                                (space && ImGui::IsMouseDown(ImGuiMouseButton_Left));
        if (hovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
                        ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
                        (space && ImGui::IsMouseClicked(ImGuiMouseButton_Left)))) {
            _panning = true;
            _pan_start_mouse = mpos;
            _pan_start_pan = _pan;
        }
        if (_panning) {
            if (is_pan_btn) {
                _pan.x = _pan_start_pan.x + (mpos.x - _pan_start_mouse.x);
                _pan.y = _pan_start_pan.y + (mpos.y - _pan_start_mouse.y);
            } else {
                _panning = false;
            }
        }

        auto to_cell = [&](const ImVec2& p) -> std::pair<int, int> {
            if (pm.bins == 0 || side <= 0.0f) return {-1, -1};
            const float u = (p.x - origin.x) / side;
            const float v = (p.y - origin.y) / side;
            const int c = (int)std::floor(u * (float)pm.bins);
            const int r = (int)std::floor(v * (float)pm.bins);
            return {r, c};
        };

        if (hovered && !_panning && !space &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            auto [r, c] = to_cell(mpos);
            if (r >= 0 && r < (int)pm.bins && c >= 0 && c < (int)pm.bins) {
                _dragging = true;
                _drag_r0 = (uint32_t)r;
                _drag_c0 = (uint32_t)c;
                _drag_r1 = (uint32_t)r;
                _drag_c1 = (uint32_t)c;
            }
        }
        if (_dragging) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                auto [r, c] = to_cell(mpos);
                r = std::clamp(r, 0, (int)pm.bins - 1);
                c = std::clamp(c, 0, (int)pm.bins - 1);
                _drag_r1 = (uint32_t)r;
                _drag_c1 = (uint32_t)c;
            } else {
                _dragging = false;
                _selection.active = true;
                _selection.r0 = std::min(_drag_r0, _drag_r1);
                _selection.r1 = std::max(_drag_r0, _drag_r1) + 1;
                _selection.c0 = std::min(_drag_c0, _drag_c1);
                _selection.c1 = std::max(_drag_c0, _drag_c1) + 1;
                update_selected_pairs(pm, preview);
            }
        }

        // Draw matrix
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->PushClipRect(cpos, ImVec2(cpos.x + csize.x, cpos.y + csize.y), true);
        dl->AddRectFilled(cpos, ImVec2(cpos.x + csize.x, cpos.y + csize.y), IM_COL32(14, 15, 18, 255));

        mm.ensure_texture();
        if (mm.texture()) {
            dl->AddImage((ImTextureID)(intptr_t)mm.texture(), origin,
                         ImVec2(origin.x + side, origin.y + side));
        }

        const float cell_px = side / (float)std::max(1u, pm.bins);
        if (cell_px >= 9.0f) {
            const ImU32 grid_col = IM_COL32(255, 255, 255, cell_px >= 18.0f ? 35 : 18);
            for (uint32_t i = 0; i <= pm.bins; i++) {
                const float x = origin.x + (float)i * cell_px;
                const float y = origin.y + (float)i * cell_px;
                dl->AddLine(ImVec2(x, origin.y), ImVec2(x, origin.y + side), grid_col);
                dl->AddLine(ImVec2(origin.x, y), ImVec2(origin.x + side, y), grid_col);
            }
        }

        auto draw_box = [&](uint32_t r0, uint32_t r1, uint32_t c0, uint32_t c1,
                            ImU32 fill, ImU32 border) {
            const ImVec2 bmin(origin.x + (float)c0 * cell_px, origin.y + (float)r0 * cell_px);
            const ImVec2 bmax(origin.x + (float)c1 * cell_px, origin.y + (float)r1 * cell_px);
            dl->AddRectFilled(bmin, bmax, fill);
            dl->AddRect(bmin, bmax, border, 0.0f, 0, px(2.0f));
        };

        if (_selection.active) {
            const ImU32 fill = IM_COL32(255, 180, 50, 60);
            const ImU32 border = IM_COL32(255, 200, 60, 240);
            draw_box(_selection.r0, _selection.r1, _selection.c0, _selection.c1, fill, border);
            if (_selection.symmetric) {
                draw_box(_selection.c0, _selection.c1, _selection.r0, _selection.r1, fill, border);
            }
        }

        if (_dragging) {
            const uint32_t dr0 = std::min(_drag_r0, _drag_r1);
            const uint32_t dr1 = std::max(_drag_r0, _drag_r1) + 1;
            const uint32_t dc0 = std::min(_drag_c0, _drag_c1);
            const uint32_t dc1 = std::max(_drag_c0, _drag_c1) + 1;
            const ImU32 drag_fill = IM_COL32(255, 220, 90, 55);
            const ImU32 drag_border = IM_COL32(255, 230, 110, 255);
            draw_box(dr0, dr1, dc0, dc1, drag_fill, drag_border);
            if (_selection.symmetric) {
                draw_box(dc0, dc1, dr0, dr1, drag_fill, drag_border);
            }
        }

        dl->AddRect(origin, ImVec2(origin.x + side, origin.y + side),
                    IM_COL32(85, 90, 100, 255), 0.0f, 0, px(1.0f));
        dl->PopClipRect();

        if (hovered && !_dragging && !_panning) {
            auto [r, c] = to_cell(mpos);
            if (r >= 0 && r < (int)pm.bins && c >= 0 && c < (int)pm.bins) {
                ImGui::BeginTooltip();
                const uint32_t lo_r = (uint32_t)((uint64_t)r * pm.n_images / pm.bins);
                const uint32_t hi_r = (uint32_t)(((uint64_t)r + 1) * pm.n_images / pm.bins);
                const uint32_t lo_c = (uint32_t)((uint64_t)c * pm.n_images / pm.bins);
                const uint32_t hi_c = (uint32_t)(((uint64_t)c + 1) * pm.n_images / pm.bins);
                if (hi_r - lo_r <= 1 && hi_c - lo_c <= 1) {
                    const std::string name_a = lo_r < _stems.size() ? _stems[lo_r] : std::to_string(lo_r);
                    const std::string name_b = lo_c < _stems.size() ? _stems[lo_c] : std::to_string(lo_c);
                    ui::TextRaw(name_a + " <-> " + name_b);
                    ui::Text(dmsg::matrix_cell_pair,
                             {(long long)lo_r, (long long)lo_c,
                              (long long)pm.at((uint32_t)r, (uint32_t)c)});
                } else {
                    ui::Text(dmsg::matrix_cell_range,
                             {(long long)lo_r, (long long)(hi_r - 1), (long long)lo_c,
                              (long long)(hi_c - 1),
                              (long long)pm.at((uint32_t)r, (uint32_t)c)});
                }
                const uint32_t cnt = pm.at((uint32_t)r, (uint32_t)c);
                if (cnt > 0) ui::TextDisabled(dmsg::matrix_key_matched);
                else ui::TextDisabled(dmsg::matrix_key_none);
                ImGui::EndTooltip();
            }
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Right pane: Pair preview and inspection
    ImGui::BeginChild("##matrix_preview_pane", ImVec2(right_w, canvas_h));
    {
        bool show_feat = preview.show_keypoints();
        if (ui::Checkbox(dmsg::matrix_show_features, &show_feat)) {
            preview.set_show_keypoints(show_feat);
        }
        ImGui::SameLine();
        bool show_mat = preview.show_matches();
        if (ui::Checkbox(dmsg::matrix_show_matches, &show_mat)) {
            preview.set_show_matches(show_mat);
        }

        if (_selected_matched_pairs.size() > 1) {
            ImGui::SameLine(0.0f, px(16.0f));
            if (ui::ButtonRaw("<##prev_pair")) {
                if (_selected_pair_idx > 0) {
                    _selected_pair_idx--;
                    const auto& p = _selected_matched_pairs[(size_t)_selected_pair_idx];
                    preview.show(PairBlock{p.first, p.first + 1, p.second, p.second + 1});
                }
            }
            ImGui::SameLine();
            ui::TextRaw(std::to_string(_selected_pair_idx + 1) + " / " +
                        std::to_string(_selected_matched_pairs.size()));
            ImGui::SameLine();
            if (ui::ButtonRaw(">##next_pair")) {
                if (_selected_pair_idx + 1 < (int)_selected_matched_pairs.size()) {
                    _selected_pair_idx++;
                    const auto& p = _selected_matched_pairs[(size_t)_selected_pair_idx];
                    preview.show(PairBlock{p.first, p.first + 1, p.second, p.second + 1});
                }
            }
        }

        const ImVec2 preview_box = ImGui::GetContentRegionAvail();
        preview.draw(preview_box);
    }
    ImGui::EndChild();

    // Footer
    ImGui::Separator();
    legend_item(kMatchedCol, dmsg::matrix_key_matched);
    ImGui::SameLine();
    legend_item(kNoMatchCol, dmsg::matrix_key_none);
    ImGui::SameLine();
    legend_item(kFilteredCol, dmsg::matrix_key_skipped);

    if (!_status.empty()) {
        ImGui::SameLine(0.0f, px(24.0f));
        ui::TextRaw(_status);
    }

    const float close_w = px(90.0f);
    ImGui::SameLine(ImGui::GetWindowWidth() - close_w - ImGui::GetStyle().WindowPadding.x);
    if (ui::ButtonRaw("Close###editor_close", ImVec2(close_w, 0.0f))) {
        _open = false;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
    return modified;
}

}  // namespace gui

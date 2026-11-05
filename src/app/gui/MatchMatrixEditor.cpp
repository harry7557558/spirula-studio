#include "app/gui/MatchMatrixEditor.h"

#include "app/gui/Layout.h"
#include "app/gui/Ui.h"
#include "core/AtomicFile.h"
#include "i18n/catalog/Dataset.h"

#ifdef SS_TOOL_SFM
#include "sfm/core/Matches.h"
#endif

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <set>
#include <tuple>
#include <utility>

namespace fs = std::filesystem;
namespace dmsg = spirula::i18n::msg::dataset;

namespace gui {

#ifdef SS_TOOL_SFM
// Each removed pair with its index in the file before the removal, ascending,
// so an undo puts the file back byte for byte.
struct MatchMatrixEditor::UndoStep {
    std::vector<std::pair<size_t, sfm::TwoViewMatches>> removed;
};
#else
struct MatchMatrixEditor::UndoStep {};
#endif

namespace {

int64_t file_stamp(const std::string& path) {
    std::error_code ec;
    const auto t = fs::last_write_time(path, ec);
    return ec ? 0 : (int64_t)t.time_since_epoch().count();
}

#ifdef SS_TOOL_SFM
uint64_t pair_bytes(const sfm::TwoViewMatches& p) { return 16 + 8 * (uint64_t)p.matches.size(); }

// Writes beside `path` and renames over it, so a crash or a reader never sees
// half a file. `size` is what the result must measure: the old file plus or
// minus the pair records, which is all an edit changes.
void publish(const std::string& path, const sfm::MatchesDatabase& db, uint64_t size) {
    const std::string tmp = path + ".tmp";
    sfm::writeMatches(tmp, db);
    std::error_code ec;
    sfm::MatchesIndex idx;
    if (!sfm::indexMatches(tmp, idx) || idx.pairs.size() != db.pairs.size() ||
        fs::file_size(tmp, ec) != size) {
        fs::remove(tmp, ec);
        throw std::runtime_error("cannot write " + tmp);
    }
    spirula::replace_file(tmp, path);
}

// The backup is matches.bin before the first edit. Edits only remove pairs and
// put them back, so a backup that does not hold every current pair is from an
// older run and is taken again.
void ensure_backup(const std::string& path, const sfm::MatchesDatabase& db) {
    const std::string bak = path + ".bak";
    sfm::MatchesIndex old;
    bool ancestor = sfm::indexMatches(bak, old) && old.images.size() == db.images.size();
    for (size_t i = 0; ancestor && i < db.images.size(); i++)
        ancestor = old.images[i].name == db.images[i].name &&
                   old.images[i].num_features == db.images[i].num_features;
    if (ancestor) {
        std::set<std::tuple<uint32_t, uint32_t, uint32_t>> had;
        for (const sfm::MatchesIndex::Entry& e : old.pairs) had.insert({e.image1, e.image2, e.count});
        for (const sfm::TwoViewMatches& p : db.pairs)
            if (!had.count({p.image1, p.image2, (uint32_t)p.matches.size()})) {
                ancestor = false;
                break;
            }
    }
    if (ancestor) return;
    std::error_code ec;
    fs::copy_file(path, bak, fs::copy_options::overwrite_existing, ec);
    if (ec) throw std::runtime_error("cannot write " + bak + ": " + ec.message());
}
#endif

}  // namespace

MatchMatrixEditor::MatchMatrixEditor() = default;
MatchMatrixEditor::~MatchMatrixEditor() = default;

bool MatchMatrixEditor::can_undo() const { return !_undo.empty(); }

bool MatchMatrixEditor::can_restore_backup() const {
    std::error_code ec;
    return !_matches_path.empty() && fs::is_regular_file(_matches_path + ".bak", ec);
}

void MatchMatrixEditor::configure(const std::string& image_dir, const std::string& mask_dir,
                                  const std::string& features_dir,
                                  const std::string& matches_path) {
    if (matches_path != _matches_path) _undo.clear();
    _image_dir = image_dir;
    _mask_dir = mask_dir;
    _features_dir = features_dir;
    _matches_path = matches_path;
    reload();
    // The preview numbers images by the feature files; a features/ that no
    // longer holds exactly the images matches.bin indexes would name the wrong
    // ones, so it is then shown the pictures alone.
    const bool same = read_image_stems(features_dir, std::string()) == _names;
    _preview.configure(image_dir, mask_dir, same ? features_dir : std::string(), matches_path);
}

void MatchMatrixEditor::open() {
    _want_open = true;
    _sel = MatrixSelection{};
    _sel_pairs.clear();
    _sel_matches = 0;
    _status.clear();
    _zoom = 1.0f;
    _pan = ImVec2(0.0f, 0.0f);
}

void MatchMatrixEditor::close() {
    _open = _want_open = false;
    _dragging = _panning = false;
    _mm.clear();
    _mm.destroy_gl();
    _preview.clear();
    _preview.destroy_gl();
}

void MatchMatrixEditor::reload() {
    _names.clear();
    _pairs.clear();
    PairMatrix pm;
    int64_t stamp = 0;
    if (!read_pair_matrix_from_matches(_matches_path, stamp, pm, kEditorMatrixBins)) {
        _mm.clear();
        _stamp = 0;
        select(MatrixSelection{});
        return;
    }
    _mm.set(pm);
    _stamp = stamp;
#ifdef SS_TOOL_SFM
    sfm::MatchesIndex idx;
    if (sfm::indexMatches(_matches_path, idx)) {
        for (const sfm::ImageEntry& im : idx.images) _names.push_back(im.name);
        for (const sfm::MatchesIndex::Entry& e : idx.pairs)
            _pairs.push_back({e.image1, e.image2, e.count});
    }
#endif
    _preview.reload();
    select(_sel);
}

bool MatchMatrixEditor::file_is_ours() {
    if (file_stamp(_matches_path) == _stamp) return true;
    _undo.clear();
    reload();
    _status = dmsg::matrix_file_changed.get();
    return false;
}

void MatchMatrixEditor::select(const MatrixSelection& sel) {
    _sel = sel;
    _sel_pairs.clear();
    _sel_matches = 0;
    _sel_idx = 0;
    const PairMatrix& pm = _mm.matrix();
    if (_sel.active && !pm.empty()) {
        for (PairRef p : _pairs) {
            if (p.a >= pm.n_images || p.b >= pm.n_images) continue;
            const uint32_t ra = pm.cell_of(p.a), rb = pm.cell_of(p.b);
            if (!_sel.contains(ra, rb)) continue;
            if (!(ra >= _sel.r0 && ra < _sel.r1 && rb >= _sel.c0 && rb < _sel.c1))
                std::swap(p.a, p.b);
            _sel_pairs.push_back(p);
            _sel_matches += p.count;
        }
        std::sort(_sel_pairs.begin(), _sel_pairs.end(), [](const PairRef& x, const PairRef& y) {
            return x.a != y.a ? x.a < y.a : x.b < y.b;
        });
    }
    show_selected();
}

void MatchMatrixEditor::show_selected() {
    const PairMatrix& pm = _mm.matrix();
    if (!_sel_pairs.empty()) {
        const PairRef& p = _sel_pairs[std::min(_sel_idx, _sel_pairs.size() - 1)];
        _preview.show_pair(p.a, p.b);
    } else if (_sel.single() && !pm.empty()) {
        // An unmatched cell still has two pictures worth comparing.
        const auto lo = [&](uint32_t c) { return (uint32_t)((uint64_t)c * pm.n_images / pm.bins); };
        _preview.show(PairBlock{lo(_sel.r0), std::max(lo(_sel.r1), lo(_sel.r0) + 1),
                                lo(_sel.c0), std::max(lo(_sel.c1), lo(_sel.c0) + 1)});
    } else {
        _preview.clear();
    }
}

bool MatchMatrixEditor::remove_selected() {
#ifdef SS_TOOL_SFM
    if (_sel_pairs.empty() || !file_is_ours()) return false;
    try {
        sfm::MatchesDatabase db = sfm::readMatches(_matches_path);
        ensure_backup(_matches_path, db);
        const PairMatrix& pm = _mm.matrix();
        UndoStep step;
        std::vector<sfm::TwoViewMatches> kept;
        kept.reserve(db.pairs.size());
        uint64_t bytes = 0, matches = 0;
        for (size_t i = 0; i < db.pairs.size(); i++) {
            sfm::TwoViewMatches& p = db.pairs[i];
            const bool in = p.image1 < pm.n_images && p.image2 < pm.n_images &&
                            _sel.contains(pm.cell_of(p.image1), pm.cell_of(p.image2));
            if (!in) {
                kept.push_back(std::move(p));
                continue;
            }
            bytes += pair_bytes(p);
            matches += p.matches.size();
            step.removed.emplace_back(i, std::move(p));
        }
        if (step.removed.empty()) return false;
        const size_t removed = step.removed.size();
        db.pairs = std::move(kept);
        publish(_matches_path, db, fs::file_size(_matches_path) - bytes);
        _undo.push_back(std::move(step));
        reload();
        _status = spirula::i18n::format(dmsg::matrix_removed,
                                        {(long long)removed, (long long)matches});
        return true;
    } catch (const std::exception& e) {
        _status = spirula::i18n::format(dmsg::matrix_edit_failed, {e.what()});
        return false;
    }
#else
    return false;
#endif
}

bool MatchMatrixEditor::undo() {
#ifdef SS_TOOL_SFM
    if (_undo.empty() || !file_is_ours()) return false;
    try {
        sfm::MatchesDatabase db = sfm::readMatches(_matches_path);
        const UndoStep& step = _undo.back();
        std::vector<sfm::TwoViewMatches> merged;
        merged.reserve(db.pairs.size() + step.removed.size());
        size_t next = 0;
        uint64_t bytes = 0, matches = 0;
        for (const auto& [at, p] : step.removed) {
            while (merged.size() < at && next < db.pairs.size())
                merged.push_back(std::move(db.pairs[next++]));
            merged.push_back(p);
            bytes += pair_bytes(p);
            matches += p.matches.size();
        }
        while (next < db.pairs.size()) merged.push_back(std::move(db.pairs[next++]));
        const size_t restored = step.removed.size();
        db.pairs = std::move(merged);
        publish(_matches_path, db, fs::file_size(_matches_path) + bytes);
        _undo.pop_back();
        reload();
        _status = spirula::i18n::format(dmsg::matrix_restored,
                                        {(long long)restored, (long long)matches});
        return true;
    } catch (const std::exception& e) {
        _status = spirula::i18n::format(dmsg::matrix_edit_failed, {e.what()});
        return false;
    }
#else
    return false;
#endif
}

bool MatchMatrixEditor::restore_backup() {
#ifdef SS_TOOL_SFM
    if (!file_is_ours()) return false;
    const std::string backup = _matches_path + ".bak";
    const std::string tmp = _matches_path + ".tmp";
    try {
        fs::copy_file(backup, tmp, fs::copy_options::overwrite_existing);
        sfm::MatchesIndex restored, current;
        if (!sfm::indexMatches(tmp, restored) || !sfm::indexMatches(_matches_path, current))
            throw std::runtime_error("cannot read matches for backup restore");
        bool same = restored.images.size() == current.images.size();
        for (size_t i = 0; same && i < current.images.size(); i++)
            same = restored.images[i].name == current.images[i].name &&
                   restored.images[i].num_features == current.images[i].num_features;
        if (!same) throw std::runtime_error("backup belongs to different image features");
        spirula::replace_file(tmp, _matches_path);
        _undo.clear();
        reload();
        uint64_t matches = 0;
        for (const auto& p : restored.pairs) matches += p.count;
        _status = spirula::i18n::format(dmsg::matrix_restored,
                                      {(long long)restored.pairs.size(), (long long)matches});
        return true;
    } catch (const std::exception& e) {
        std::error_code ec;
        fs::remove(tmp, ec);
        _status = spirula::i18n::format(dmsg::matrix_edit_failed, {e.what()});
        return false;
    }
#else
    return false;
#endif
}

bool MatchMatrixEditor::draw(bool busy) {
    if (_want_open) {
        ui::OpenPopup(dmsg::matrix_editor_title);
        _want_open = false;
        _open = true;
    }
    if (!_open) return false;

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize(ImVec2(std::min(display.x * 0.94f, px(1480.0f)),
                                    std::min(display.y * 0.90f, px(920.0f))),
                             ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.5f), ImGuiCond_Appearing,
                            ImVec2(0.5f, 0.5f));
    bool keep_open = true;
    if (!ui::BeginPopupModal(dmsg::matrix_editor_title, &keep_open,
                             ImGuiWindowFlags_NoScrollbar)) {
        close();
        return false;
    }
    bool modified = false;
    bool want_close = !keep_open;
    const PairMatrix& pm = _mm.matrix();

    const bool can_remove = !busy && !_sel_pairs.empty();
    const bool can_undo_now = !busy && !_undo.empty();
    if (can_remove &&
        (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace)))
        modified |= remove_selected();
    if (can_undo_now && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z))
        modified |= undo();
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        if (_sel.active) select(MatrixSelection{});
        else want_close = true;
    }

    ui::TextDisabledWrapped(dmsg::matrix_editor_help);

    if (ui::ButtonRaw("-##zoom_out")) _zoom = std::max(0.4f, _zoom / 1.35f);
    ImGui::SameLine();
    if (ui::ButtonRaw("+##zoom_in")) _zoom = std::min(50.0f, _zoom * 1.35f);
    ImGui::SameLine();
    if (ui::Button(dmsg::matrix_reset_view)) {
        _zoom = 1.0f;
        _pan = ImVec2(0.0f, 0.0f);
    }
    ImGui::SameLine();
    ui::TextRaw(std::to_string((int)std::lround(_zoom * 100.0f)) + "%");

    ImGui::SameLine(0.0f, px(18.0f));
    ImGui::BeginDisabled(!can_remove);
    if (ui::Button(dmsg::matrix_remove_matches)) modified |= remove_selected();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!can_undo_now);
    if (ui::Button(dmsg::matrix_undo)) modified |= undo();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(busy || !can_restore_backup());
    if (ui::Button(dmsg::matrix_restore_backup)) modified |= restore_backup();
    ImGui::EndDisabled();
    ui::help_on_hover(dmsg::matrix_restore_backup_help);
    ImGui::SameLine();
    ImGui::BeginDisabled(!_sel.active);
    if (ui::Button(dmsg::matrix_clear_selection)) select(MatrixSelection{});
    ImGui::EndDisabled();
    if (_sel.active) {
        ImGui::SameLine(0.0f, px(18.0f));
        ui::TextDisabled(dmsg::matrix_selection_counts,
                         {(long long)_sel_pairs.size(), (long long)_sel_matches});
    }
    ImGui::Separator();

    const float avail_w = ImGui::GetContentRegionAvail().x;
    const float footer_h = ImGui::GetFrameHeightWithSpacing() + px(6.0f);
    const float canvas_h = std::max(px(120.0f), ImGui::GetContentRegionAvail().y - footer_h);
    const float left_w = std::max(px(320.0f), avail_w * 0.54f);
    const float right_w = std::max(px(80.0f), avail_w - left_w - ImGui::GetStyle().ItemSpacing.x);

    ImGui::BeginChild("##matrix_canvas", ImVec2(left_w, canvas_h), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    {
        const ImVec2 cpos = ImGui::GetCursorScreenPos();
        const ImVec2 csize = ImGui::GetContentRegionAvail();
        const ImVec2 ccenter(cpos.x + csize.x * 0.5f, cpos.y + csize.y * 0.5f);
        const float base_side = std::max(px(100.0f), std::min(csize.x, csize.y) - px(16.0f));
        const float side = base_side * _zoom;
        const ImVec2 origin(ccenter.x - side * 0.5f + _pan.x, ccenter.y - side * 0.5f + _pan.y);

        ui::InvisibleButtonRaw("##canvas", csize,
                               ImGuiButtonFlags_MouseButtonLeft |
                                   ImGuiButtonFlags_MouseButtonRight |
                                   ImGuiButtonFlags_MouseButtonMiddle);
        const bool hovered = ImGui::IsItemHovered();
        const ImGuiIO& io = ImGui::GetIO();
        const ImVec2 mpos = io.MousePos;

        if (hovered && io.MouseWheel != 0.0f) {
            // About the cursor: the cell under it stays under it.
            const float zoom = std::clamp(_zoom * std::pow(1.22f, io.MouseWheel), 0.4f, 50.0f);
            const ImVec2 at((mpos.x - origin.x) / side, (mpos.y - origin.y) / side);
            const float new_side = base_side * zoom;
            _pan.x = mpos.x - at.x * new_side + new_side * 0.5f - ccenter.x;
            _pan.y = mpos.y - at.y * new_side + new_side * 0.5f - ccenter.y;
            _zoom = zoom;
        }

        const bool space = ImGui::IsKeyDown(ImGuiKey_Space);
        const bool pan_down = ImGui::IsMouseDown(ImGuiMouseButton_Middle) ||
                              ImGui::IsMouseDown(ImGuiMouseButton_Right) ||
                              (space && ImGui::IsMouseDown(ImGuiMouseButton_Left));
        if (hovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
                        ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
                        (space && ImGui::IsMouseClicked(ImGuiMouseButton_Left)))) {
            _panning = true;
            _pan_start_mouse = mpos;
            _pan_start = _pan;
        }
        if (_panning) {
            if (pan_down) {
                _pan.x = _pan_start.x + (mpos.x - _pan_start_mouse.x);
                _pan.y = _pan_start.y + (mpos.y - _pan_start_mouse.y);
            } else {
                _panning = false;
            }
        }

        const int bins = (int)pm.bins;
        auto cell_at = [&](const ImVec2& p, int& r, int& c) {
            c = (int)std::floor((p.x - origin.x) / side * (float)bins);
            r = (int)std::floor((p.y - origin.y) / side * (float)bins);
            return bins > 0 && r >= 0 && r < bins && c >= 0 && c < bins;
        };
        int hr = -1, hc = -1;
        const bool on_cell = hovered && cell_at(mpos, hr, hc);

        if (on_cell && !_panning && !space && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            _dragging = true;
            _drag_r0 = _drag_r1 = (uint32_t)hr;
            _drag_c0 = _drag_c1 = (uint32_t)hc;
        }
        if (_dragging) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                int r = 0, c = 0;
                cell_at(mpos, r, c);
                _drag_r1 = (uint32_t)std::clamp(r, 0, bins - 1);
                _drag_c1 = (uint32_t)std::clamp(c, 0, bins - 1);
            } else {
                _dragging = false;
                MatrixSelection s;
                s.active = true;
                s.r0 = std::min(_drag_r0, _drag_r1);
                s.r1 = std::max(_drag_r0, _drag_r1) + 1;
                s.c0 = std::min(_drag_c0, _drag_c1);
                s.c1 = std::max(_drag_c0, _drag_c1) + 1;
                select(s);
            }
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->PushClipRect(cpos, ImVec2(cpos.x + csize.x, cpos.y + csize.y), true);
        dl->AddRectFilled(cpos, ImVec2(cpos.x + csize.x, cpos.y + csize.y), IM_COL32(14, 15, 18, 255));
        if (const GLuint tex = _mm.texture()) {
            // Square cells once magnified, as on the dataset screen (MatchMatrix.cpp).
            const ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
            const bool point = side >= (float)bins && pio.DrawCallback_SetSamplerNearest &&
                               pio.DrawCallback_SetSamplerLinear;
            if (point) dl->AddCallback(pio.DrawCallback_SetSamplerNearest, nullptr);
            dl->AddImage((ImTextureID)(intptr_t)tex, origin, ImVec2(origin.x + side, origin.y + side));
            if (point) dl->AddCallback(pio.DrawCallback_SetSamplerLinear, nullptr);
        }
        const float cell_px = side / (float)std::max(1, bins);
        if (cell_px >= 9.0f) {
            const ImU32 grid = IM_COL32(255, 255, 255, cell_px >= 18.0f ? 35 : 18);
            const int i0 = std::max(0, (int)((cpos.x - origin.x) / cell_px));
            const int i1 = std::min(bins, (int)((cpos.x + csize.x - origin.x) / cell_px) + 1);
            const int j0 = std::max(0, (int)((cpos.y - origin.y) / cell_px));
            const int j1 = std::min(bins, (int)((cpos.y + csize.y - origin.y) / cell_px) + 1);
            for (int i = i0; i <= i1; i++) {
                const float x = origin.x + (float)i * cell_px;
                dl->AddLine(ImVec2(x, origin.y), ImVec2(x, origin.y + side), grid);
            }
            for (int j = j0; j <= j1; j++) {
                const float y = origin.y + (float)j * cell_px;
                dl->AddLine(ImVec2(origin.x, y), ImVec2(origin.x + side, y), grid);
            }
        }
        auto draw_box = [&](uint32_t r0, uint32_t r1, uint32_t c0, uint32_t c1, ImU32 fill,
                            ImU32 border) {
            const ImVec2 lo(origin.x + (float)c0 * cell_px, origin.y + (float)r0 * cell_px);
            const ImVec2 hi(origin.x + (float)c1 * cell_px, origin.y + (float)r1 * cell_px);
            dl->AddRectFilled(lo, hi, fill);
            dl->AddRect(lo, hi, border, 0.0f, 0, px(2.0f));
        };
        // Both orders of every pair in the box, since both cells hold them.
        if (_sel.active) {
            const ImU32 fill = IM_COL32(255, 180, 50, 60), border = IM_COL32(255, 200, 60, 240);
            draw_box(_sel.r0, _sel.r1, _sel.c0, _sel.c1, fill, border);
            draw_box(_sel.c0, _sel.c1, _sel.r0, _sel.r1, fill, border);
        }
        if (_dragging) {
            const uint32_t r0 = std::min(_drag_r0, _drag_r1), r1 = std::max(_drag_r0, _drag_r1) + 1;
            const uint32_t c0 = std::min(_drag_c0, _drag_c1), c1 = std::max(_drag_c0, _drag_c1) + 1;
            const ImU32 fill = IM_COL32(255, 220, 90, 55), border = IM_COL32(255, 230, 110, 255);
            draw_box(r0, r1, c0, c1, fill, border);
            draw_box(c0, c1, r0, r1, fill, border);
        }
        dl->AddRect(origin, ImVec2(origin.x + side, origin.y + side), IM_COL32(85, 90, 100, 255),
                    0.0f, 0, px(1.0f));
        dl->PopClipRect();

        if (on_cell && !_dragging && !_panning) {
            const uint32_t lo_r = (uint32_t)((uint64_t)hr * pm.n_images / pm.bins);
            const uint32_t hi_r = (uint32_t)(((uint64_t)hr + 1) * pm.n_images / pm.bins);
            const uint32_t lo_c = (uint32_t)((uint64_t)hc * pm.n_images / pm.bins);
            const uint32_t hi_c = (uint32_t)(((uint64_t)hc + 1) * pm.n_images / pm.bins);
            const uint32_t n = pm.at((uint32_t)hr, (uint32_t)hc);
            _preview.show(PairBlock{lo_r, std::max(hi_r, lo_r + 1),
                                    lo_c, std::max(hi_c, lo_c + 1)});
            ImGui::BeginTooltip();
            if (hi_r - lo_r <= 1 && hi_c - lo_c <= 1) {
                if (lo_r < _names.size() && lo_c < _names.size())
                    ui::TextRaw(_names[lo_r] + "  /  " + _names[lo_c]);
                ui::Text(dmsg::matrix_cell_pair, {(long long)lo_r, (long long)lo_c, (long long)n});
            } else {
                ui::Text(dmsg::matrix_cell_range,
                         {(long long)lo_r, (long long)(hi_r - 1), (long long)lo_c,
                          (long long)(hi_c - 1), (long long)n});
            }
            // A finished matches.bin holds only the pairs that survived.
            if (!n) ui::TextDisabled(dmsg::matrix_key_skipped);
            ImGui::EndTooltip();
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##matrix_pair", ImVec2(right_w, canvas_h));
    {
        bool feats = _preview.show_keypoints();
        if (ui::Checkbox(dmsg::matrix_show_features, &feats)) _preview.set_show_keypoints(feats);
        ImGui::SameLine();
        bool lines = _preview.show_matches();
        if (ui::Checkbox(dmsg::matrix_show_matches, &lines)) _preview.set_show_matches(lines);
        if (_sel_pairs.size() > 1) {
            ImGui::SameLine(0.0f, px(16.0f));
            if (ui::ButtonRaw("<##prev_pair") && _sel_idx > 0) {
                _sel_idx--;
                show_selected();
            }
            ImGui::SameLine();
            ui::TextRaw(std::to_string(_sel_idx + 1) + " / " + std::to_string(_sel_pairs.size()));
            ImGui::SameLine();
            if (ui::ButtonRaw(">##next_pair") && _sel_idx + 1 < _sel_pairs.size()) {
                _sel_idx++;
                show_selected();
            }
        }
        _preview.draw(ImGui::GetContentRegionAvail());
    }
    ImGui::EndChild();

    ImGui::Separator();
    if (!_status.empty()) ui::TextRaw(_status);
    else ImGui::Dummy(ImVec2(0.0f, 0.0f));
    const float close_w = px(90.0f);
    ImGui::SameLine(ImGui::GetWindowWidth() - close_w - ImGui::GetStyle().WindowPadding.x);
    if (ui::Button(dmsg::matrix_close, ImVec2(close_w, 0.0f))) want_close = true;

    if (want_close) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
    if (want_close) close();
    return modified;
}

}  // namespace gui

#!/bin/bash
# Structural tripwire over MaskPanel.cpp: each gate below is still there,
# and the known wrong replacements are not. Text only, never behaviour; the exit
# status is the number of lines that missed. Anchors are whole statements where a
# shorter one also matches elsewhere (the pen commit vs handle_keys' Enter).

cd "$(dirname "$0")/../.." || exit 1
F=src/app/gui/mask/MaskPanel.cpp
FAILS=0
has() {
    local n
    n=$(command grep -cF -- "$2" "$F")
    if [ "$n" -ge "$1" ]; then echo "ok   $n >= $1: $2"; else echo "FAIL $n >= $1: $2"; FAILS=$((FAILS + 1)); fi
}
none() {
    local n
    n=$(command grep -cF -- "$1" "$F")
    if [ "$n" -eq 0 ]; then echo "ok   0 == 0: $1"; else echo "FAIL $n == 0: $1"; FAILS=$((FAILS + 1)); fi
}
has 1 '_strip.h > 0.0f'
has 1 '_canvas_h = size.y'
has 1 '_shown_valid = false'
has 1 'set_radius(wheel_brush(radius(), io.MouseWheel))'
has 1 'sam_prompt_point(fx, fy, sam_click_mode(io.KeyShift, io.KeyCtrl), in.clicked)'
has 1 'draw_sam_clicks(dl, m, ox, origin.y)'
has 1 'upload_rect(commit_stroke(stroke, paint_now(in.shift, in.ctrl), m));'
has 1 'upload_rect(commit_stroke(stroke, paint_now(_path.mode_shift(), _path.mode_ctrl()), m));'
has 1 'const Paint mode = path_mode() ? paint_now(_path.mode_shift(), _path.mode_ctrl())'
has 1 'note_shown(m, origin.x, origin.y, npanes, pane_w, gap)'
has 1 'SetItemKeyOwner(ImGuiKey_Tab)'
has 1 'const bool typing = ImGui::GetIO().WantTextInput;'
has 1 '} else if (path_mode()) {'
has 1 'const int active = bind_pane(hover_pane,'
# EditTool's Polygon has no test seam (EditTool.cpp needs ImGui, which
# mask_doc_test never links), so its input is pinned to the bound pane here.
has 1 'const ImVec2 porg(origin.x + pane_left(active, pane_w, gap), origin.y);'
has 1 'in.x = io.MousePos.x - porg.x;'
has 2 'switch_view(_view_mode,'
has 2 'in_each_pane('
# The propagate row. The 1-based UI range becomes 0-based here
# only, and only Propagate (never Undo propagate) waits for a SAM job.
has 1 'propagate(scope, _prop_from - 1, _prop_to - 1);'
has 1 'ImGui::BeginDisabled(_prop_scope != 1);'
has 1 'ImGui::BeginDisabled(!can_undo_propagate());'
has 1 'ui::TextDisabled(msg::prop_warn_moves);'
has 3 'note_row_width();'
gate=$(awk '/BeginDisabled\(sam_work_pending\(\)\);/{on=1} on{print} on&&/EndDisabled\(\);/{exit}' "$F")
if printf '%s' "$gate" | command grep -qF 'msg::prop_go)' &&
   ! printf '%s' "$gate" | command grep -qF 'msg::prop_undo)'; then
    echo "ok   the SAM gate holds Propagate and not Undo propagate"
else echo "FAIL the SAM gate holds Propagate and not Undo propagate"; FAILS=$((FAILS + 1)); fi
# The row gate, the warning's placement, and the disabled-aware tooltip,
# anchored past the identical revert-frame gate.
row_gate=$(awk '
/\/\/ Row B: propagate\. Row C: its warning, drawn whether or not B is enabled\./ { getline; print; exit }
' "$F")
if printf '%s' "$row_gate" | command grep -qF 'ImGui::BeginDisabled(!_doc || !idle() || _slide_playing);'; then
    echo "ok   row B/C gated by !_doc || !idle() || _slide_playing on the line right after the Row B/C comment"
else
    echo "FAIL row B/C gated by !_doc || !idle() || _slide_playing on the line right after the Row B/C comment"
    FAILS=$((FAILS + 1))
fi
# The warning row (the propagate bar while one runs) opens right after the gate.
warn_order=$(awk '
{ line=$0; gsub(/^[ \t]+|[ \t]+$/, "", line)
  if (line ~ /^\/\//) next
  if (line == "if (const int total = propagate_total(); total > 0) {") { print p2; print p1; exit }
  p2=p1; p1=line }
' "$F")
if [ "$warn_order" = "$(printf 'ImGui::EndDisabled();\nnote_row_width();')" ]; then
    echo "ok   prop_warn_moves immediately follows the row's EndDisabled(); note_row_width(): warning stays outside the row gate"
else
    echo "FAIL prop_warn_moves immediately follows the row's EndDisabled(); note_row_width(): warning stays outside the row gate"
    FAILS=$((FAILS + 1))
fi
has 1 'ui::help_on_hover_disabled(msg::prop_go_help);'
has 1 'ui::help_on_hover_disabled(msg::prop_undo_help);'
# Play waits for SAM, the worker, a stroke and a pen path, and
# says why when greyed; playing greys Revert all (both clauses kept), the
# navigation row and the SAM strip; the slideshow branch returns before !_doc.
play_gate=$(awk '/Play waits for anything that would land on the document it releases/{on=1} on{print} on&&/EndDisabled\(\);/{exit}' "$F")
for clause in 'frame_count() < 2 ||' '(!_slide_playing && (!_doc || !idle() || sam_work_pending() ||' \
              'shape_open())));' 'else start_slideshow();'; do
    if printf '%s' "$play_gate" | command grep -qF -- "$clause"; then echo "ok   Play's gate holds: $clause"
    else echo "FAIL Play's gate holds: $clause"; FAILS=$((FAILS + 1)); fi
done
if printf '%s' "$play_gate" | command grep -qE '_tool\.in_progress\(\)|_path\.in_progress\(\)'; then
    echo "FAIL Play's gate asks shape_open(), not the two tools inline"; FAILS=$((FAILS + 1))
else echo "ok   Play's gate asks shape_open(), not the two tools inline"; fi
has 1 'ui::help_on_hover_disabled(msg::slide_help);'
has 1 'ImGui::BeginDisabled(!idle() || _slide_playing ||'
has 1 '(corrected_count() == 0 && !(_doc && _doc->dirty())));'
nav=$(awk '/if \(ui::ButtonRaw\("<"\)\) go_to\(_idx - 1\);/{print p; exit} {p=$0}' "$F")
if [ "$(printf '%s' "$nav" | sed 's/^ *//')" = 'ImGui::BeginDisabled(!idle() || _slide_playing || shape);' ]; then
    echo "ok   the navigation row is greyed while playing, and while a shape is half drawn"
else echo "FAIL the navigation row is greyed while playing, and while a shape is half drawn"; FAILS=$((FAILS + 1)); fi
has 1 'const bool shape = shape_open();'
sam=$(awk '/^ *draw_sam_status\(\);$/{print p; exit} {p=$0}' "$F")
if [ "$(printf '%s' "$sam" | sed 's/^ *//')" = 'ImGui::BeginDisabled(_slide_playing);' ]; then
    echo "ok   the SAM strip is drawn greyed while playing"
else echo "FAIL the SAM strip is drawn greyed while playing"; FAILS=$((FAILS + 1)); fi
order=$(awk '/dl->AddRectFilled\(origin, far_corner,/{a=NR} /draw_slideshow\(dl, origin.x, origin.y, size.x, size.y\);/{b=NR} /^    if \(!_doc\) \{$/{c=NR} END{print (a && b && c && a < b && b < c) ? "yes" : "no"}' "$F")
if [ "$order" = yes ]; then echo "ok   the slideshow branch sits between the canvas fill and !_doc"
else echo "FAIL the slideshow branch sits between the canvas fill and !_doc"; FAILS=$((FAILS + 1)); fi
has 1 'if (!fresh && input) {'
has 1 '_slide_fresh = false;'
if command grep -qF '_pen.in_progress() || _path.in_progress())' src/app/gui/mask/MaskSession.cpp &&
   command grep -qF 'bool animating() const { return _compare.animating() || _mask_editor.animating(); }' src/app/gui/GuiApp.h; then
    echo "ok   start_slideshow refuses a pen path; GuiApp::animating() asks the editor"
else echo "FAIL start_slideshow refuses a pen path; GuiApp::animating() asks the editor"; FAILS=$((FAILS + 1)); fi
# Row D, the frame keys and the navigation row stand down for the
# worker, a half-drawn shape or pen path, and play; help shows when greyed;
# M / Shift+M sit inside the keys' guard; the key list is the slider's tooltip.
has 1 'return _tool.in_progress() || _path.in_progress() || _pen.in_progress();'
find=$(awk '/if \(ui::Button\(msg::find_first\)\) go_to\(0\);/{print p; exit} {p=$0}' "$F")
if [ "$(printf '%s' "$find" | sed 's/^ *//')" = 'ImGui::BeginDisabled(!idle() || _slide_playing || shape_open());' ]; then
    echo "ok   row D's find buttons wait for the worker, a half-drawn shape and play"
else echo "FAIL row D's find buttons wait for the worker, a half-drawn shape and play"; FAILS=$((FAILS + 1)); fi
has 1 'const spirula::i18n::Msg& find_tip = shape_open() ? msg::nav_locked : msg::find_help;'
has 4 'ui::help_on_hover_disabled(find_tip);'
none 'ui::help_on_hover(msg::find_help);'
has 1 'if (ui::Button(msg::find_prev)) go_to_missing(-1);'
has 1 'if (ui::Button(msg::find_next)) go_to_missing(+1);'
has 1 'band_edit(lo_pct, hi_pct, lo_changed);'
keys=$(awk '/ImGui::Shortcut\(ImGuiKey_V, route\)/{v=1} v&&/if \(!shape_open\(\) && idle\(\) && !io.KeyCtrl\) \{/{g=1; next} g&&/^    \}$/{exit} g{print}' "$F")
for line in 'if (ImGui::Shortcut(ImGuiKey_LeftArrow, rep)) go_to(_idx - 1);' \
            'if (ImGui::Shortcut(ImGuiKey_M, route)) go_to_missing(+1);' \
            'if (ImGui::Shortcut(ImGuiMod_Shift | ImGuiKey_M, route)) go_to_missing(-1);'; do
    if printf '%s\n' "$keys" | sed 's/^ *//' | command grep -qxF -- "$line"; then
        echo "ok   inside the keys' guard (after V; no shape, idle, no Ctrl): $line"
    else echo "FAIL inside the keys' guard (after V; no shape, idle, no Ctrl): $line"; FAILS=$((FAILS + 1)); fi
done
tip=$(awk 'p~/IsItemDeactivatedAfterEdit\(\) && _slider_idx != _idx\) go_to\(_slider_idx\);/{print; exit} {p=$0}' "$F")
if [ "$(printf '%s' "$tip" | sed 's/^ *//')" = 'ui::help_on_hover_disabled(shape ? msg::nav_locked : msg::hint_keys);' ]; then
    echo "ok   the key list is the frame slider's tooltip, shown when greyed, read after its commit"
else echo "FAIL the key list is the frame slider's tooltip, shown when greyed, read after its commit"; FAILS=$((FAILS + 1)); fi
none 'TextDisabledWrapped(msg::hint_keys)'
# Play marks its press frame, or keyboard Play stops itself.
if command grep -qF '    _slide_fresh = true;' src/app/gui/mask/MaskSession.cpp; then
    echo "ok   start_slideshow marks the press frame (_slide_fresh = true)"
else echo "FAIL start_slideshow marks the press frame (_slide_fresh = true)"; FAILS=$((FAILS + 1)); fi
# The slideshow ticks into the member picture and re-uploads into the
# texture it has; the decoder count is budgeted.
has 1 'slideshow_tick(ImGui::GetTime(), side, _slide_pic)'
has 1 'glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _slide_pic.w, _slide_pic.h, GL_RGB, GL_UNSIGNED_BYTE, _slide_pic.rgb.data());'
slide_fn=$(awk '/^void MaskSession::draw_slideshow\(/{on=1} on{print} on&&/^}/{exit}' "$F")
if ! printf '%s\n' "$slide_fn" | command grep -qE '(^|[^_A-Za-z0-9])(gui::)?Picture[[:space:]]+[A-Za-z_][A-Za-z0-9_]*[[:space:]]*[;{(=]'; then
    echo "ok   draw_slideshow declares no local Picture, of any name"
else echo "FAIL draw_slideshow declares no local Picture, of any name"; FAILS=$((FAILS + 1)); fi
sub_if=$(printf '%s\n' "$slide_fn" | awk '/glTexSubImage2D\(/{print p; exit} {p=$0}' | sed 's/^ *//')
if [ "$sub_if" = 'if (_slide_pic.w == _slide_tex_w && _slide_pic.h == _slide_tex_h) {' ] &&
   printf '%s\n' "$slide_fn" | command grep -qxE ' *_slide_tex_w = _slide_pic\.w;' &&
   printf '%s\n' "$slide_fn" | command grep -qxE ' *_slide_tex_h = _slide_pic\.h;'; then
    echo "ok   a same-size picture goes through glTexSubImage2D, and a re-specify records the size"
else echo "FAIL a same-size picture goes through glTexSubImage2D, and a re-specify records the size"; FAILS=$((FAILS + 1)); fi
stop_fn=$(awk '/^void MaskSession::stop_slideshow\(/{on=1} on{print} on&&/^}/{exit}' src/app/gui/mask/MaskSession.cpp)
if printf '%s\n' "$stop_fn" | command grep -qxF '    _slide_pic = Picture{};'; then
    echo "ok   stop_slideshow releases the picture on screen"
else echo "FAIL stop_slideshow releases the picture on screen"; FAILS=$((FAILS + 1)); fi
if command grep -qF '_slide_threads = mask::slide_threads(' src/app/gui/mask/MaskSession.cpp; then
    echo "ok   Play budgets its decoders"
else echo "FAIL Play budgets its decoders"; FAILS=$((FAILS + 1)); fi
none '_path_mode'
none 'paint_for('
none '_status_h > 0.0f'
none 'const ImVec2 far('
none '_stroke_right'
none '_stroke_pane'
exit "$FAILS"

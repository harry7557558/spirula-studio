#pragma once

// ui:: -- the only text-bearing ImGui entry points the GUI is allowed to call.
//
// `Msg` (src/i18n/Message.h) makes an INCOMPLETE translation a compile error.
// It cannot do anything about ImGui::Button("Start"), which compiles perfectly
// well and is simply never translated. This header closes that hole from the
// other side: every wrapper here takes a `const Msg&`, so a bare literal does
// not compile, and tools/check_i18n.sh fails the build if any of the wrapped
// ImGui functions is called anywhere in src/app/gui/ except from this file.
//
// Between the two, "forgot to translate" has no way in:
//   type system  -> a message with a missing language
//   the lint     -> a string that never became a message
//
// The `*Raw` overloads are the deliberate exceptions, and they are named so
// that using one is a visible decision in the diff. They are for text that
// MUST NOT be translated:
//   - paths, filenames, numbers, device names
//   - engine log lines and backend error strings (English, and grepped for)
//   - text already produced by i18n::format(), which is translated
//
// Widget IDs: everything with an ID gets "text###<message name>", so a widget
// keeps its identity when the language changes. Without it, switching to
// Japanese would collapse every open header and reset every scroll position,
// because ImGui derives a widget's ID from its label. Two call sites sharing
// one message therefore share an ID too -- wrap one in ImGui::PushID() exactly
// as you would for two identical literals.

#include "app/gui/Automation.h"
#include "app/gui/Layout.h"
#include "i18n/Message.h"

#include "imgui.h"
#include "imgui_stdlib.h"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace ui {

using ::spirula::i18n::Arg;
using ::spirula::i18n::Msg;
using ::spirula::i18n::format;

namespace detail {

// "text###id" in a small rotating buffer. Valid until this thread has built
// four more labels, which is long enough for any single ImGui call and short
// enough to never grow. ImGui hashes what follows ### and renders what
// precedes it.
inline const char* label(const char* text, const char* id) {
    static thread_local std::string ring[4];
    static thread_local unsigned next = 0;
    std::string& s = ring[next++ & 3u];
    s.assign(text);
    s += "###";
    s += id;
    return s.c_str();
}

inline const char* label(const Msg& m) { return label(m.get(), m.id); }

inline const char* label(const std::string& text, const Msg& m) {
    return label(text.c_str(), m.id);
}

// A running dataset job disables its form, which also stops a combo from
// listing its choices and a header from opening. A click on one still gets
// through to look: popups inherit the disabled state, so nothing can change.
bool locked_click();  // in Ui.cpp: imgui_internal.h clashes with stb_truetype
// After a combo: `id` is its own, since an open popup moves the last item.
void view_locked_combo(ImGuiID id);

// ImGui's combo reports no label to the item hooks (Automation.h), so a script
// could not find it by its message id. The id is taken first: an open popup
// moves ImGui's last item.
template <class Draw>
inline bool named_combo(const char* label, Draw&& draw) {
    const ImGuiID id = ImGui::GetID(label);
    const bool r = draw();
    ::gui::automation::name_item(id, label);
    view_locked_combo(id);
    return r;
}

// After a header or tree node that drew as `open`.
bool view_locked_node(bool open);

// Msg* list -> the const char*[] ImGui's Combo wants.
inline const std::vector<const char*>& items(
        std::initializer_list<const Msg*> ms) {
    static thread_local std::vector<const char*> v;
    v.clear();
    for (const Msg* m : ms) v.push_back(m->get());
    return v;
}

// "10000000" and "1000000" look alike, so an integer of five digits or more
// is shown as "10,000,000", while it is typed too. Parsing skips the commas.
inline int group_digits(const char* s, int cursor, std::string& out) {
    std::string digits;
    int before = 0;
    for (int i = 0; s[i]; i++) {
        if (s[i] < '0' || s[i] > '9') continue;
        digits += s[i];
        if (i < cursor) before++;
    }
    out = s[0] == '-' ? "-" : "";
    int pos = (int)out.size();
    const size_t n = digits.size();
    for (size_t i = 0; i < n; i++) {
        if (i && n >= 5 && (n - i) % 3 == 0) out += ',';
        out += digits[i];
        if ((int)i < before) pos = (int)out.size();
    }
    return pos;
}

inline int int_field_callback(ImGuiInputTextCallbackData* d) {
    if (d->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) {
        const ImWchar c = d->EventChar;
        return !((c >= '0' && c <= '9') || c == '-' || c == ',');
    }
    std::string out;
    const int pos = group_digits(d->Buf, d->CursorPos, out);
    if (out != d->Buf) {
        d->DeleteChars(0, d->BufTextLen);
        d->InsertChars(0, out.c_str());
    }
    d->CursorPos = d->SelectionStart = d->SelectionEnd = pos;
    return 0;
}

// ImGui::InputInt's layout and IDs, so automation finds the same widgets.
inline bool input_int(const char* label, int* v, int step, int step_fast) {
    std::string text;
    group_digits(std::to_string(*v).c_str(), 0, text);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%s", text.c_str());
    const ImGuiInputTextFlags flags = ImGuiInputTextFlags_AutoSelectAll |
                                      ImGuiInputTextFlags_CallbackCharFilter |
                                      ImGuiInputTextFlags_CallbackEdit;
    const float button = ImGui::GetFrameHeight();
    const float gap = ImGui::GetStyle().ItemInnerSpacing.x;
    if (step) {
        ImGui::BeginGroup();
        ImGui::PushID(label);
        ImGui::SetNextItemWidth(std::max(1.0f, ImGui::CalcItemWidth() - (button + gap) * 2));
    }
    bool changed = false;
    if (ImGui::InputText(step ? "" : label, buf, sizeof buf, flags, int_field_callback)) {
        long long parsed = 0;
        bool any = false;
        for (const char* p = buf; *p; p++) {
            if (*p < '0' || *p > '9') continue;
            parsed = std::min(parsed * 10 + (*p - '0'), (long long)INT_MAX + 1);
            any = true;
        }
        if (buf[0] == '-') parsed = -parsed;
        const int nv = (int)std::clamp(parsed, (long long)INT_MIN, (long long)INT_MAX);
        if (any && nv != *v) { *v = nv; changed = true; }
    }
    if (step) {
        const ImGuiStyle& st = ImGui::GetStyle();
        const int d = ImGui::GetIO().KeyCtrl && step_fast ? step_fast : step;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(st.FramePadding.y, st.FramePadding.y));
        ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
        ImGui::SameLine(0, gap);
        if (ImGui::Button("-", ImVec2(button, button))) { *v -= d; changed = true; }
        ImGui::SameLine(0, gap);
        if (ImGui::Button("+", ImVec2(button, button))) { *v += d; changed = true; }
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
        const char* end = std::strstr(label, "##");
        if (!end) end = label + std::strlen(label);
        if (end != label) {
            ImGui::SameLine(0, gap);
            ImGui::TextUnformatted(label, end);
        }
        ImGui::PopID();
        ImGui::EndGroup();
    }
    return changed;
}

}  // namespace detail

// ---------------------------------------------------------------------------
// Text
// ---------------------------------------------------------------------------

inline void Text(const Msg& m) { ImGui::TextUnformatted(m.get()); }
inline void Text(const Msg& m, std::initializer_list<Arg> a) {
    ImGui::TextUnformatted(format(m, a).c_str());
}

inline void TextWrapped(const Msg& m) {
    ImGui::PushTextWrapPos();
    ImGui::TextUnformatted(m.get());
    ImGui::PopTextWrapPos();
}
inline void TextWrapped(const Msg& m, std::initializer_list<Arg> a) {
    ImGui::PushTextWrapPos();
    ImGui::TextUnformatted(format(m, a).c_str());
    ImGui::PopTextWrapPos();
}

inline void TextDisabled(const Msg& m) {
    ImGui::TextDisabled("%s", m.get());
}
inline void TextDisabledWrapped(const Msg& m) {
    ImGui::PushTextWrapPos();
    ImGui::TextDisabled("%s", m.get());
    ImGui::PopTextWrapPos();
}
inline void TextDisabled(const Msg& m, std::initializer_list<Arg> a) {
    ImGui::TextDisabled("%s", format(m, a).c_str());
}
inline void TextDisabledWrapped(const Msg& m, std::initializer_list<Arg> a) {
    ImGui::PushTextWrapPos();
    ImGui::TextDisabled("%s", format(m, a).c_str());
    ImGui::PopTextWrapPos();
}

inline void TextColored(const ImVec4& c, const Msg& m) {
    ImGui::TextColored(c, "%s", m.get());
}
inline void TextColored(const ImVec4& c, const Msg& m,
                        std::initializer_list<Arg> a) {
    ImGui::TextColored(c, "%s", format(m, a).c_str());
}

inline void TextColoredWrapped(const ImVec4& c, const Msg& m) {
    ImGui::PushTextWrapPos();
    ImGui::TextColored(c, "%s", m.get());
    ImGui::PopTextWrapPos();
}
inline void TextColoredWrapped(const ImVec4& c, const Msg& m,
                               std::initializer_list<Arg> a) {
    ImGui::PushTextWrapPos();
    ImGui::TextColored(c, "%s", format(m, a).c_str());
    ImGui::PopTextWrapPos();
}

// ---- raw: paths, numbers, log lines, and already-formatted text ----
inline void TextRaw(const char* s)        { ImGui::TextUnformatted(s); }
inline void TextRaw(const std::string& s) { ImGui::TextUnformatted(s.c_str()); }
inline void TextDisabledRaw(const char* s)        { ImGui::TextDisabled("%s", s); }
inline void TextDisabledRaw(const std::string& s) { ImGui::TextDisabled("%s", s.c_str()); }
inline void TextColoredRaw(const ImVec4& c, const char* s) {
    ImGui::TextColored(c, "%s", s);
}
inline void TextColoredRaw(const ImVec4& c, const std::string& s) {
    ImGui::TextColored(c, "%s", s.c_str());
}
inline void TextWrappedRaw(const std::string& s) {
    ImGui::PushTextWrapPos();
    ImGui::TextUnformatted(s.c_str());
    ImGui::PopTextWrapPos();
}
inline void TextColoredWrappedRaw(const ImVec4& c, const std::string& s) {
    ImGui::PushTextWrapPos();
    ImGui::TextColored(c, "%s", s.c_str());
    ImGui::PopTextWrapPos();
}

// ---------------------------------------------------------------------------
// Tooltips
// ---------------------------------------------------------------------------
// The delayed "(?)"-style hover help every option on the dataset and training
// screens carries. Kept here rather than in ConfigUI.h so that the wrapped-text
// tooltip and the widgets it annotates cannot drift apart.

inline void help_on_hover_raw(const char* text,
                              ImGuiHoveredFlags extra = 0) {
    if (!text || !*text) return;
    // A control that has been used needs no caption, and the help would sit on
    // top of whatever the control is changing -- so mute it from the first
    // click until the pointer leaves. Only one item is ever hovered at a time.
    static thread_local ImGuiID muted = 0;
    const ImGuiID id = ImGui::GetItemID();
    if (id != 0) {   // 0 is a Text and the like: nothing to click, never muted
        if (ImGui::IsItemActive() || ImGui::IsItemActivated()) muted = id;
        else if (muted == id &&
                 !ImGui::IsItemHovered(
                     ImGuiHoveredFlags_AllowWhenDisabled |
                     ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
            muted = 0;
        if (muted == id) return;
    }
    // NoSharedDelay: walking a toolbar should not pop one tooltip per item.
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort |
                             ImGuiHoveredFlags_NoSharedDelay | extra) &&
        ImGui::BeginTooltip()) {
        ImGui::PushTextWrapPos(gui::px(420.0f));
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}
inline void help_on_hover(const Msg& m) { help_on_hover_raw(m.get()); }
// For an item that may be greyed out, where the help is what explains why.
inline void help_on_hover_disabled(const Msg& m) {
    help_on_hover_raw(m.get(), ImGuiHoveredFlags_AllowWhenDisabled);
}
inline void help_on_hover(const Msg& m, std::initializer_list<Arg> a) {
    help_on_hover_raw(format(m, a).c_str());
}

inline void SetTooltip(const Msg& m) { ImGui::SetTooltip("%s", m.get()); }
inline void SetTooltip(const Msg& m, std::initializer_list<Arg> a) {
    ImGui::SetTooltip("%s", format(m, a).c_str());
}
inline void SetTooltipRaw(const char* s) { ImGui::SetTooltip("%s", s); }
// SetTooltip for a sentence or more: wrapped at help_on_hover's width.
inline void SetTooltipWrapped(const Msg& m) {
    if (!ImGui::BeginTooltip()) return;
    ImGui::PushTextWrapPos(gui::px(420.0f));
    ImGui::TextUnformatted(m.get());
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}
inline void SetTooltipRaw(const std::string& s) { ImGui::SetTooltip("%s", s.c_str()); }

// ---------------------------------------------------------------------------
// Buttons and toggles
// ---------------------------------------------------------------------------

inline bool Button(const Msg& m, const ImVec2& size = ImVec2(0, 0)) {
    return ImGui::Button(detail::label(m), size);
}
inline bool Button(const Msg& m, std::initializer_list<Arg> a,
                   const ImVec2& size = ImVec2(0, 0)) {
    return ImGui::Button(detail::label(format(m, a), m), size);
}
// The key in the corner of the button it belongs to: a modal grammar nobody
// can see is a modal grammar nobody uses. Drawn over the last item.
inline void corner_key(const char* key) {
    if (!key || !*key) return;
    const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    const ImGuiStyle& st = ImGui::GetStyle();
    const float tw = ImGui::CalcTextSize(key).x;
    if (tw + 2.0f * st.FramePadding.x > b.x - a.x) return;   // no room
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(b.x - st.FramePadding.x - tw, a.y + st.FramePadding.y),
        IM_COL32(255, 255, 255, 90), key);
}
// A button with its key in the corner, drawn pressed while `on`.
inline bool KeyButton(const Msg& m, float w, const char* key, bool on = false) {
    if (on) {
        const ImVec4 c = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
        ImGui::PushStyleColor(ImGuiCol_Button, c);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, c);
    }
    const bool hit = ImGui::Button(detail::label(m), ImVec2(w, 0));
    if (on) ImGui::PopStyleColor(2);
    corner_key(key);
    return hit;
}
inline bool SmallButton(const Msg& m) {
    return ImGui::SmallButton(detail::label(m));
}
// A button whose face is punctuation or a glyph ("...", "<"), not a word.
inline bool ButtonRaw(const char* id, const ImVec2& size = ImVec2(0, 0)) {
    return ImGui::Button(id, size);
}
// An arrow, which has no words to translate.
inline bool ArrowButtonRaw(const char* id, ImGuiDir dir) {
    return ImGui::ArrowButton(id, dir);
}
// A region that holds the mouse without drawing anything: a canvas that is
// dragged on, which without an item under it would drag the window instead.
inline bool InvisibleButtonRaw(const char* id, const ImVec2& size,
                               ImGuiButtonFlags flags = 0) {
    return ImGui::InvisibleButton(id, size, flags);
}
inline bool Checkbox(const Msg& m, bool* v) {
    return ImGui::Checkbox(detail::label(m), v);
}
inline bool Checkbox(const Msg& m, std::initializer_list<Arg> a, bool* v) {
    return ImGui::Checkbox(detail::label(format(m, a), m), v);
}
inline bool CheckboxRaw(const char* id, bool* v) {
    return ImGui::Checkbox(id, v);
}
inline bool RadioButton(const Msg& m, bool active) {
    return ImGui::RadioButton(detail::label(m), active);
}
inline bool Selectable(const Msg& m, bool selected = false) {
    return ImGui::Selectable(detail::label(m), selected);
}
inline bool SelectableRaw(const char* s, bool selected = false) {
    return ImGui::Selectable(s, selected);
}
inline bool SelectableRaw(const std::string& s, bool selected = false) {
    return ImGui::Selectable(s.c_str(), selected);
}
inline bool SelectableRaw(const char* s, bool selected, ImGuiSelectableFlags f) {
    return ImGui::Selectable(s, selected, f);
}
inline bool SelectableRaw(const char* s, bool selected, ImGuiSelectableFlags f,
                          const ImVec2& size) {
    return ImGui::Selectable(s, selected, f, size);
}
inline bool RadioButtonRaw(const char* s, bool active) {
    return ImGui::RadioButton(s, active);
}

// ---------------------------------------------------------------------------
// Menus, headers, popups
// ---------------------------------------------------------------------------

inline bool BeginMenu(const Msg& m, bool enabled = true) {
    return ImGui::BeginMenu(detail::label(m), enabled);
}
// For the language menu, whose label is an icon plus a language's own name --
// neither of which is translated, and both of which would be wrong to
// translate. Pass detail::label(text, msg) so it still keeps a stable ID.
inline bool BeginMenuRaw(const char* s, bool enabled = true) {
    return ImGui::BeginMenu(s, enabled);
}
inline bool MenuItem(const Msg& m, const char* shortcut = nullptr,
                     bool selected = false, bool enabled = true) {
    return ImGui::MenuItem(detail::label(m), shortcut, selected, enabled);
}
inline bool MenuItem(const Msg& m, const char* shortcut, bool* selected,
                     bool enabled = true) {
    return ImGui::MenuItem(detail::label(m), shortcut, selected, enabled);
}
inline bool MenuItem(const Msg& m, std::initializer_list<Arg> a) {
    return ImGui::MenuItem(detail::label(format(m, a), m));
}
// An entry whose face is a number, not a word: the interface-size percentages.
inline bool MenuItemRaw(const char* s, bool selected = false) {
    return ImGui::MenuItem(s, nullptr, selected);
}
// ... and one with a dim note where a shortcut would go. No defaults, so a
// two-argument call cannot convert the note to `selected`.
inline bool MenuItemRaw(const char* s, const char* note, bool selected, bool enabled) {
    return ImGui::MenuItem(s, note, selected, enabled);
}
inline bool BeginTabItem(const Msg& m, ImGuiTabItemFlags flags = 0) {
    return ImGui::BeginTabItem(detail::label(m), nullptr, flags);
}
inline bool CollapsingHeader(const Msg& m, ImGuiTreeNodeFlags flags = 0) {
    return detail::view_locked_node(ImGui::CollapsingHeader(detail::label(m), flags));
}
inline bool TreeNode(const Msg& m) {
    return detail::view_locked_node(ImGui::TreeNode(detail::label(m)));
}
inline bool TreeNode(const Msg& m, std::initializer_list<Arg> a) {
    return detail::view_locked_node(ImGui::TreeNode(detail::label(format(m, a), m)));
}
inline void SeparatorText(const Msg& m) {
    ImGui::SeparatorText(detail::label(m));
}
inline void OpenPopup(const Msg& m) { ImGui::OpenPopup(detail::label(m)); }
inline bool BeginPopupModal(const Msg& m, bool* open = nullptr,
                            ImGuiWindowFlags flags = 0) {
    return ImGui::BeginPopupModal(detail::label(m), open, flags);
}
// A modal whose title the caller already built -- the file dialog is opened
// with a different title depending on what it is picking.
inline bool BeginPopupModalRaw(const char* title, bool* open = nullptr,
                               ImGuiWindowFlags flags = 0) {
    return ImGui::BeginPopupModal(title, open, flags);
}

// ---------------------------------------------------------------------------
// Value editors
// ---------------------------------------------------------------------------
// A combo's items are messages too -- pass them as a braced list of pointers:
//   ui::Combo(msg::quality, &q, {&msg::q_fast, &msg::q_balanced});

inline bool Combo(const Msg& m, int* cur, std::initializer_list<const Msg*> its) {
    const auto& v = detail::items(its);
    const char* l = detail::label(m);
    return detail::named_combo(l, [&] { return ImGui::Combo(l, cur, v.data(), (int)v.size()); });
}
inline bool ComboRaw(const char* id, int* cur, const char* const items[],
                     int count) {
    const ImGuiID iid = ImGui::GetID(id);
    const bool r = ImGui::Combo(id, cur, items, count);
    detail::view_locked_combo(iid);
    return r;
}
// Unlabelled combo, translated items: the viewport's toolbar is a single row
// of controls with no room for labels, but what they offer is still words.
inline bool ComboRaw(const char* id, int* cur,
                     std::initializer_list<const Msg*> its) {
    const auto& v = detail::items(its);
    const ImGuiID iid = ImGui::GetID(id);
    const bool r = ImGui::Combo(id, cur, v.data(), (int)v.size());
    detail::view_locked_combo(iid);
    return r;
}
inline bool ComboRaw(const char* id, int* cur, const std::vector<const Msg*>& its) {
    static thread_local std::vector<const char*> v;
    v.clear();
    for (const Msg* m : its) v.push_back(m->get());
    const ImGuiID iid = ImGui::GetID(id);
    const bool r = ImGui::Combo(id, cur, v.data(), (int)v.size());
    detail::view_locked_combo(iid);
    return r;
}
inline bool BeginCombo(const Msg& m, const char* preview, ImGuiComboFlags flags = 0) {
    const char* l = detail::label(m);
    return detail::named_combo(l, [&] { return ImGui::BeginCombo(l, preview, flags); });
}
inline bool BeginComboRaw(const char* id, const char* preview) {
    const ImGuiID iid = ImGui::GetID(id);
    const bool r = ImGui::BeginCombo(id, preview);
    detail::view_locked_combo(iid);
    return r;
}

inline bool SliderInt(const Msg& m, int* v, int lo, int hi) {
    return ImGui::SliderInt(detail::label(m), v, lo, hi);
}
// Unlabelled sliders whose value format string carries the meaning
// ("size x%.2f", "fov %.0f"). The format is a printf pattern ImGui fills in,
// so it cannot be a Msg -- but the words inside it can be, via format().
inline bool SliderIntRaw(const char* id, int* v, int lo, int hi,
                         const char* fmt) {
    return ImGui::SliderInt(id, v, lo, hi, fmt);
}
inline bool SliderFloatRaw(const char* id, float* v, float lo, float hi,
                           const char* fmt, ImGuiSliderFlags flags = 0) {
    return ImGui::SliderFloat(id, v, lo, hi, fmt, flags);
}
inline bool SliderFloat(const Msg& m, float* v, float lo, float hi,
                        const char* fmt = "%.3f") {
    return ImGui::SliderFloat(detail::label(m), v, lo, hi, fmt);
}
inline bool SliderFloat3(const Msg& m, float v[3], float lo, float hi,
                         const char* fmt = "%.3f") {
    return ImGui::SliderFloat3(detail::label(m), v, lo, hi, fmt);
}
inline bool ColorEdit3Raw(const char* id, float v[3],
                          ImGuiColorEditFlags flags = 0) {
    return ImGui::ColorEdit3(id, v, flags);
}
inline bool InputInt(const Msg& m, int* v, int step = 0, int step_fast = 0) {
    return detail::input_int(detail::label(m), v, step, step_fast);
}
inline bool InputIntRaw(const char* id, int* v) {
    return detail::input_int(id, v, 0, 0);
}
inline bool InputFloat(const Msg& m, float* v, float step = 0.0f,
                       float step_fast = 0.0f, const char* fmt = "%.3f") {
    return ImGui::InputFloat(detail::label(m), v, step, step_fast, fmt);
}
inline bool InputText(const Msg& m, std::string* v,
                      ImGuiInputTextFlags flags = 0) {
    return ImGui::InputText(detail::label(m), v, flags);
}
inline bool InputTextWithHint(const Msg& m, const Msg& hint, std::string* v,
                              ImGuiInputTextFlags flags = 0) {
    return ImGui::InputTextWithHint(detail::label(m), hint.get(), v, flags);
}

// A field whose CONTENT is English whatever the interface language is,
// because it is not read by a person: a mask prompt goes to a text encoder
// trained on English (src/app/gui/MaskPrompt.h). The label IS translated --
// it tells the user what the field is for -- but `example` is not, because it
// is an example of what to type into an English box, and translating it would
// be advice to type something that works worse.
inline bool InputTextEnglish(const Msg& m, const char* example, std::string* v,
                             ImGuiInputTextFlags flags = 0) {
    return ImGui::InputTextWithHint(detail::label(m), example, v, flags);
}
inline bool InputTextEnglishRaw(const char* id, const char* example,
                                std::string* v,
                                ImGuiInputTextFlags flags = 0) {
    return ImGui::InputTextWithHint(id, example, v, flags);
}

// Unlabelled fields ("##outdir"): no text to translate, but they still go
// through here so the lint does not have to special-case them.
inline bool InputTextRaw(const char* id, std::string* v,
                         ImGuiInputTextFlags flags = 0) {
    return ImGui::InputText(id, v, flags);
}
inline bool InputTextWithHintRaw(const char* id, const Msg& hint,
                                 std::string* v,
                                 ImGuiInputTextFlags flags = 0) {
    return ImGui::InputTextWithHint(id, hint.get(), v, flags);
}
inline bool InputFloatRaw(const char* id, float* v, const char* fmt) {
    return ImGui::InputFloat(id, v, 0.0f, 0.0f, fmt);
}
inline bool InputFloatRaw(const char* id, float* v, float step, float step_fast,
                          const char* fmt) {
    return ImGui::InputFloat(id, v, step, step_fast, fmt);
}
inline bool InputDoubleRaw(const char* id, double* v, double step,
                           double step_fast, const char* fmt) {
    return ImGui::InputDouble(id, v, step, step_fast, fmt);
}
inline bool InputInt2Raw(const char* id, int v[2], ImGuiInputTextFlags flags = 0) {
    return ImGui::InputInt2(id, v, flags);
}
inline bool InputInt2(const Msg& m, int v[2], ImGuiInputTextFlags flags = 0) {
    return ImGui::InputInt2(detail::label(m), v, flags);
}
inline bool InputDouble(const Msg& m, double* v, const char* fmt) {
    return ImGui::InputDouble(detail::label(m), v, 0.0, 0.0, fmt);
}
inline bool DragFloatRaw(const char* id, float* v, float speed, float lo,
                         float hi, const char* fmt) {
    return ImGui::DragFloat(id, v, speed, lo, hi, fmt);
}
inline bool DragFloat3Raw(const char* id, float v[3], float speed, const char* fmt) {
    return ImGui::DragFloat3(id, v, speed, 0.0f, 0.0f, fmt);
}
// Doubles, for coordinates a geo-referenced model puts beyond float's reach.
inline bool DragDoubleNRaw(const char* id, double* v, int n, float speed, double lo,
                           double hi, const char* fmt) {
    return ImGui::DragScalarN(id, ImGuiDataType_Double, v, n, speed, &lo, &hi, fmt);
}
inline bool InputTextHintBufRaw(const char* id, const Msg& hint, char* buf,
                                size_t buf_size) {
    return ImGui::InputTextWithHint(id, hint.get(), buf, buf_size);
}
// A box for text the CALLER sizes, because it grows with what is in it.
inline bool InputTextMultilineRaw(const char* id, std::string* v,
                                  const ImVec2& size,
                                  ImGuiInputTextFlags flags = 0) {
    return ImGui::InputTextMultiline(id, v, size, flags);
}
// The example an empty multi-line box shows: ImGui draws a hint for the
// one-line field and not for this one, so it is drawn where that one's goes.
inline void hint_over_last_item_raw(const char* example) {
    const ImVec2 at = ImGui::GetItemRectMin();
    const ImVec2 pad = ImGui::GetStyle().FramePadding;
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(at.x + pad.x, at.y + pad.y),
        ImGui::GetColorU32(ImGuiCol_TextDisabled), example);
}

// ---------------------------------------------------------------------------
// Tables
// ---------------------------------------------------------------------------
// Only the column headers carry text; BeginTable / TableNextRow /
// TableNextColumn / TableHeadersRow do not, and are called directly.

inline void TableSetupColumn(const Msg& m, ImGuiTableColumnFlags flags = 0,
                             float width = 0.0f) {
    ImGui::TableSetupColumn(detail::label(m), flags, width);
}
// A column whose heading is a glyph or a number ("#"), not a word.
inline void TableSetupColumnRaw(const char* s, ImGuiTableColumnFlags flags = 0,
                                float width = 0.0f) {
    ImGui::TableSetupColumn(s, flags, width);
}

// ---------------------------------------------------------------------------
// Progress
// ---------------------------------------------------------------------------

inline void ProgressBar(float frac, const ImVec2& size, const Msg& m) {
    ImGui::ProgressBar(frac, size, m.get());
}
inline void ProgressBar(float frac, const ImVec2& size, const Msg& m,
                        std::initializer_list<Arg> a) {
    ImGui::ProgressBar(frac, size, format(m, a).c_str());
}
inline void ProgressBarRaw(float frac, const ImVec2& size, const char* overlay) {
    ImGui::ProgressBar(frac, size, overlay);
}
// ImGui's PlotLines tooltips BOTH ends of the hovered segment and offers no way
// to change that, and it spaces points by index. `xs` carries the x of each
// sample -- the training step -- so the axis means elapsed training, not order.
inline void PlotLinesRaw(const float* xs, const float* ys, int count,
                         const ImVec2& size) {
    const ImGuiStyle& st = ImGui::GetStyle();
    ImVec2 frame = size;
    if (frame.x <= 0.0f) frame.x = ImGui::GetContentRegionAvail().x + frame.x;
    if (frame.y <= 0.0f) frame.y = ImGui::GetFrameHeight() + frame.y;
    if (frame.x < 1.0f) frame.x = 1.0f;
    if (frame.y < 1.0f) frame.y = 1.0f;

    const ImVec2 lo = ImGui::GetCursorScreenPos();
    const ImVec2 hi(lo.x + frame.x, lo.y + frame.y);
    ImGui::Dummy(frame);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(lo, hi, ImGui::GetColorU32(ImGuiCol_FrameBg),
                      st.FrameRounding);
    if (st.FrameBorderSize > 0.0f)
        dl->AddRect(lo, hi, ImGui::GetColorU32(ImGuiCol_Border),
                    st.FrameRounding, 0, st.FrameBorderSize);
    if (count < 2) return;

    float y_lo = FLT_MAX, y_hi = -FLT_MAX;
    for (int i = 0; i < count; i++) {
        if (ys[i] != ys[i]) continue;   // NaN, as PlotEx does
        if (ys[i] < y_lo) y_lo = ys[i];
        if (ys[i] > y_hi) y_hi = ys[i];
    }
    if (y_lo > y_hi) return;
    // A run that has not moved off its first step yet has no x extent to
    // spread over; index spacing is the only thing left to draw it by.
    const float x_lo = xs[0], x_hi = xs[count - 1];
    const bool by_x = x_hi > x_lo;

    const ImVec2 in_lo(lo.x + st.FramePadding.x, lo.y + st.FramePadding.y);
    const ImVec2 in_hi(hi.x - st.FramePadding.x, hi.y - st.FramePadding.y);
    if (in_hi.x <= in_lo.x || in_hi.y <= in_lo.y) return;
    const float inv_y = (y_hi > y_lo) ? 1.0f / (y_hi - y_lo) : 0.0f;
    const float inv_x = by_x ? 1.0f / (x_hi - x_lo) : 0.0f;
    auto point_at = [&](int i) {
        const float v = (ys[i] == ys[i]) ? ys[i] : y_lo;
        const float tx = by_x ? (xs[i] - x_lo) * inv_x
                              : (float)i / (float)(count - 1);
        const float ty = 1.0f - (v - y_lo) * inv_y;
        return ImVec2(in_lo.x + tx * (in_hi.x - in_lo.x),
                      in_lo.y + ty * (in_hi.y - in_lo.y));
    };

    static thread_local std::vector<ImVec2> pts;
    pts.clear();
    pts.reserve((size_t)count);
    for (int i = 0; i < count; i++) pts.push_back(point_at(i));
    const ImU32 col = ImGui::GetColorU32(ImGuiCol_PlotLines);
    const float th = gui::px(1.5f);
    dl->AddPolyline(pts.data(), count, col, ImDrawFlags_None, th);
    // AddPolyline butts its segments together, which notches every corner the
    // curve turns; a dot on each interior vertex is the round join it lacks.
    for (int i = 1; i + 1 < count; i++)
        dl->AddCircleFilled(pts[(size_t)i], th * 0.5f, col);

    if (!ImGui::IsItemHovered()) return;
    const float mx = ImGui::GetIO().MousePos.x;
    if (mx < in_lo.x || mx > in_hi.x) return;
    int hit = 0;
    float best = FLT_MAX;
    for (int i = 0; i < count; i++) {
        const float d = pts[(size_t)i].x - mx;
        if (d * d < best) { best = d * d; hit = i; }
    }
    dl->AddCircleFilled(pts[(size_t)hit], th * 1.4f,
                        ImGui::GetColorU32(ImGuiCol_PlotLinesHovered));
    ImGui::SetTooltip("%d: %.4g", (int)xs[hit], (double)ys[hit]);
}

}  // namespace ui

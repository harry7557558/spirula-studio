// Automation.cpp -- see Automation.h.

#include "app/gui/Automation.h"

#include "app/AppPaths.h"
#include "app/webviewer/HttpServer.h"
#include "core/Env.h"
#include "app/gui/GlLoader.h"
#include "external/stb_image_write.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <map>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// ===========================================================================
// State
// ===========================================================================

// One widget as the last completed frame saw it. `key` is what follows "###"
// in the label -- the i18n message name (src/app/gui/Ui.h), which is the same
// in every language and is what a script addresses.
struct Item {
    unsigned id = 0;
    std::string key;
    std::string label;
    std::string window;
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    int flags = 0;
};

// One frame's worth of injected input; several make a gesture. Exactly one
// is applied per frame, which is what keeps a move and the click after it in
// separate frames -- the hover test between them is what makes the click land.
struct Step {
    enum class Kind { None, MousePos, MouseButton, Wheel, Key, Text, Focus, Drop };
    Kind kind = Kind::None;
    float x = 0, y = 0;
    int button = 0;
    bool down = false;
    std::vector<int> keys;     // ImGuiKey
    std::string text;
};

struct State {
    bool armed = false;
    HttpServer http;
    std::string token;
    std::function<std::string()> state_source;   // GUI thread only
    std::string app_state;          // guarded by mu; state_source's last answer

    std::mutex mu;
    std::condition_variable cv;

    std::vector<Item> building;     // GUI thread only, the frame in flight
    std::vector<std::string> dropped;   // GUI thread only, until take_drop()
    std::vector<Item> published;    // guarded by mu
    uint64_t frame_no = 0;          // guarded by mu
    float display_w = 0, display_h = 0, fb_scale = 1;
    int fb_w = 0, fb_h = 0;

    std::deque<Step> queue;         // guarded by mu
    uint64_t queued = 0, applied = 0;

    bool want_shot = false;         // guarded by mu
    bool shot_ready = false;
    int shot_w = 0;                 // 0 = the framebuffer's own width
    bool shot_jpeg = false;
    int shot_quality = 85;
    std::vector<uint8_t> shot;
};

State& st() {
    static State s;
    return s;
}

constexpr size_t kMaxItems = 8192;

// ===========================================================================
// JSON
// ===========================================================================

void json_str(std::string& out, const std::string& s) {
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += (char)c;
                }
        }
    }
    out += '"';
}

std::string json_num(double v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", v);
    return buf;
}

HttpResponse ok_json(const std::string& body) { return HttpResponse::json(body); }

HttpResponse err_json(int status, const std::string& what) {
    std::string body = "{\"ok\":false,\"error\":";
    json_str(body, what);
    body += "}";
    return HttpResponse::text(status, body, "application/json");
}

// ===========================================================================
// Keys
// ===========================================================================

struct KeyName { const char* name; ImGuiKey key; };

const KeyName kKeys[] = {
    {"tab", ImGuiKey_Tab}, {"left", ImGuiKey_LeftArrow},
    {"right", ImGuiKey_RightArrow}, {"up", ImGuiKey_UpArrow},
    {"down", ImGuiKey_DownArrow}, {"pageup", ImGuiKey_PageUp},
    {"pagedown", ImGuiKey_PageDown}, {"home", ImGuiKey_Home},
    {"end", ImGuiKey_End}, {"insert", ImGuiKey_Insert},
    {"delete", ImGuiKey_Delete}, {"backspace", ImGuiKey_Backspace},
    {"space", ImGuiKey_Space}, {"enter", ImGuiKey_Enter},
    {"escape", ImGuiKey_Escape}, {"esc", ImGuiKey_Escape},
    {"ctrl", ImGuiKey_LeftCtrl}, {"shift", ImGuiKey_LeftShift},
    {"alt", ImGuiKey_LeftAlt}, {"super", ImGuiKey_LeftSuper},
    {"minus", ImGuiKey_Minus}, {"equal", ImGuiKey_Equal},
    {"comma", ImGuiKey_Comma}, {"period", ImGuiKey_Period},
    {"slash", ImGuiKey_Slash}, {"kpdecimal", ImGuiKey_KeypadDecimal},
    {"kpenter", ImGuiKey_KeypadEnter},
    {"bracketleft", ImGuiKey_LeftBracket}, {"bracketright", ImGuiKey_RightBracket},
};

std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// ImGui swaps Ctrl<->Super under ConfigMacOSXBehaviors, so the physical
// key driving logical io.KeyCtrl here is LeftSuper, not LeftCtrl -- the
// latter silently becomes a held Super, i.e. macOS's right-click modifier.
ImGuiKey logical_ctrl_key() {
    return ImGui::GetIO().ConfigMacOSXBehaviors ? ImGuiKey_LeftSuper : ImGuiKey_LeftCtrl;
}

// "Ctrl+Shift+A" -> the key events to hold, innermost last. Empty on a name
// this table does not know.
bool parse_chord(const std::string& spec, std::vector<int>& out) {
    size_t pos = 0;
    while (pos <= spec.size()) {
        size_t plus = spec.find('+', pos);
        std::string part = lower(spec.substr(pos, plus == std::string::npos
                                                 ? std::string::npos
                                                 : plus - pos));
        if (part.empty()) return false;
        ImGuiKey k = ImGuiKey_None;
        for (const KeyName& kn : kKeys)
            if (part == kn.name) { k = kn.key; break; }
        if (k == ImGuiKey_LeftCtrl) k = logical_ctrl_key();
        if (k == ImGuiKey_None && part.size() == 1) {
            char c = part[0];
            if (c >= 'a' && c <= 'z') k = (ImGuiKey)(ImGuiKey_A + (c - 'a'));
            else if (c >= '0' && c <= '9') k = (ImGuiKey)(ImGuiKey_0 + (c - '0'));
        }
        if (k == ImGuiKey_None && part.size() == 3 && part.rfind("kp", 0) == 0 &&
            part[2] >= '0' && part[2] <= '9')
            k = (ImGuiKey)(ImGuiKey_Keypad0 + (part[2] - '0'));
        if (k == ImGuiKey_None && part.size() >= 2 && part[0] == 'f') {
            int n = std::atoi(part.c_str() + 1);
            if (n >= 1 && n <= 12) k = (ImGuiKey)(ImGuiKey_F1 + (n - 1));
        }
        if (k == ImGuiKey_None) return false;
        out.push_back((int)k);
        if (plus == std::string::npos) break;
        pos = plus + 1;
    }
    return !out.empty();
}

// The modifier ImGui derives from a physical key, so a chord reaches a
// shortcut test as well as a key-down test.
ImGuiKey mod_of(int key) {
    switch ((ImGuiKey)key) {
        case ImGuiKey_LeftCtrl: case ImGuiKey_RightCtrl:  return ImGuiMod_Ctrl;
        case ImGuiKey_LeftShift: case ImGuiKey_RightShift: return ImGuiMod_Shift;
        case ImGuiKey_LeftAlt: case ImGuiKey_RightAlt:    return ImGuiMod_Alt;
        case ImGuiKey_LeftSuper: case ImGuiKey_RightSuper: return ImGuiMod_Super;
        default: return ImGuiKey_None;
    }
}

// ===========================================================================
// Queue
// ===========================================================================

uint64_t enqueue(const std::vector<Step>& steps) {
    State& s = st();
    std::lock_guard<std::mutex> lk(s.mu);
    for (const Step& step : steps) s.queue.push_back(step);
    s.queued += steps.size();
    return s.queued;
}

// Blocks until every step queued up to `mark` has been applied and one more
// frame has been drawn with the result. False on timeout.
bool wait_applied(uint64_t mark, double timeout_s) {
    State& s = st();
    std::unique_lock<std::mutex> lk(s.mu);
    uint64_t want_frame = 0;
    if (!s.cv.wait_for(lk, std::chrono::duration<double>(timeout_s),
                       [&] { return s.applied >= mark; }))
        return false;
    want_frame = s.frame_no + 2;
    return s.cv.wait_for(lk, std::chrono::duration<double>(timeout_s),
                         [&] { return s.frame_no >= want_frame; });
}

// A move, then the press and the release, each in its own frame. `settle`
// frames of nothing follow, which is what lets a popup open before the next
// command looks for what is in it.
void push_click(std::vector<Step>& out, float x, float y, int button,
                bool dbl, int settle) {
    Step move;
    move.kind = Step::Kind::MousePos;
    move.x = x; move.y = y;
    out.push_back(move);
    out.push_back(move);
    for (int i = 0; i < (dbl ? 2 : 1); i++) {
        Step b;
        b.kind = Step::Kind::MouseButton;
        b.button = button;
        b.down = true;
        out.push_back(b);
        b.down = false;
        out.push_back(b);
    }
    for (int i = 0; i < settle; i++) out.push_back(Step{});
}

// ===========================================================================
// Item lookup
// ===========================================================================

bool find_item(const std::string& key, int index, Item& out, std::string& err) {
    State& s = st();
    std::lock_guard<std::mutex> lk(s.mu);
    int seen = 0;
    for (const Item& it : s.published) {
        if (it.key != key) continue;
        if (seen++ != index) continue;
        if (it.x1 <= it.x0 || it.y1 <= it.y0) {
            err = "item has an empty rect: " + key;
            return false;
        }
        out = it;
        return true;
    }
    err = seen == 0 ? "no item on screen with id: " + key
                    : "only " + std::to_string(seen) + " item(s) with id: " + key;
    return false;
}

}  // namespace

// ===========================================================================
// ImGui item hooks
//
// Declared by imgui_internal.h under IMGUI_ENABLE_TEST_ENGINE. Nothing runs
// until ctx->TestEngineHookItems is set, which only arm() does.
// ===========================================================================

void ImGuiTestEngineHook_ItemAdd(ImGuiContext* ctx, ImGuiID id, const ImRect& bb,
                                 const ImGuiLastItemData* item_data) {
    (void)item_data;
    State& s = st();
    if (s.building.size() >= kMaxItems) return;
    Item it;
    it.id = (unsigned)id;
    it.x0 = bb.Min.x; it.y0 = bb.Min.y;
    it.x1 = bb.Max.x; it.y1 = bb.Max.y;
    if (ctx->CurrentWindow) it.window = ctx->CurrentWindow->Name;
    s.building.push_back(std::move(it));
}

void ImGuiTestEngineHook_ItemInfo(ImGuiContext* ctx, ImGuiID id, const char* label,
                                  ImGuiItemStatusFlags flags) {
    (void)ctx;
    State& s = st();
    for (size_t i = s.building.size(); i-- > 0; ) {
        if (s.building[i].id != (unsigned)id) continue;
        Item& it = s.building[i];
        it.flags = (int)flags;
        if (!label) return;
        const char* hash = std::strstr(label, "###");
        if (hash) {
            it.label.assign(label, hash - label);
            it.key = hash + 3;
        } else {
            const char* hide = std::strstr(label, "##");
            it.label.assign(label, hide ? hide - label : std::strlen(label));
            it.key = label;
        }
        return;
    }
}

void ImGuiTestEngineHook_Log(ImGuiContext* ctx, const char* fmt, ...) {
    (void)ctx; (void)fmt;
}

const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext* ctx, ImGuiID id) {
    (void)ctx;
    State& s = st();
    for (const Item& it : s.building)
        if (it.id == (unsigned)id && !it.label.empty()) return it.label.c_str();
    return nullptr;
}

// ===========================================================================
// Endpoints
// ===========================================================================

namespace {

std::string state_json() {
    State& s = st();
    std::string out = "{\"ok\":true";
    {
        std::lock_guard<std::mutex> lk(s.mu);
        out += ",\"frame\":" + std::to_string(s.frame_no);
        out += ",\"items\":" + std::to_string(s.published.size());
        out += ",\"queued\":" + std::to_string(s.queued - s.applied);
        out += ",\"display\":[" + json_num(s.display_w) + "," +
               json_num(s.display_h) + "]";
        out += ",\"framebuffer\":[" + std::to_string(s.fb_w) + "," +
               std::to_string(s.fb_h) + "]";
        out += ",\"fb_scale\":" + json_num(s.fb_scale);
        // False until the first frame ends and the app's fields exist.
        out += std::string(",\"app_ready\":") + (s.app_state.empty() ? "false" : "true");
        if (!s.app_state.empty()) out += "," + s.app_state;
    }
    out += "}";
    return out;
}

HttpResponse handle_tree(const HttpRequest& r) {
    const std::string q = lower(r.get("q"));
    const std::string window = r.get("window");
    const bool named_only = r.get_bool("named", true);
    State& s = st();
    std::string out = "{\"ok\":true,\"items\":[";
    bool first = true;
    std::lock_guard<std::mutex> lk(s.mu);
    for (const Item& it : s.published) {
        if (named_only && it.key.empty()) continue;
        if (!window.empty() && it.window != window) continue;
        if (!q.empty() && lower(it.key).find(q) == std::string::npos &&
            lower(it.label).find(q) == std::string::npos)
            continue;
        if (!first) out += ",";
        first = false;
        out += "{\"id\":";
        json_str(out, it.key);
        out += ",\"label\":";
        json_str(out, it.label);
        out += ",\"window\":";
        json_str(out, it.window);
        out += ",\"rect\":[" + json_num(it.x0) + "," + json_num(it.y0) + "," +
               json_num(it.x1) + "," + json_num(it.y1) + "]";
        out += ",\"flags\":" + std::to_string(it.flags) + "}";
    }
    out += "]}";
    return ok_json(out);
}

// Where a command acts: the centre of the item named by `id`, or the `at=x,y`
// the caller gave instead.
bool resolve_point(const HttpRequest& r, float& x, float& y, std::string& err) {
    const std::string at = r.get("at");
    if (!at.empty()) {
        if (std::sscanf(at.c_str(), "%f,%f", &x, &y) != 2) {
            err = "at= wants x,y";
            return false;
        }
        return true;
    }
    const std::string id = r.get("id");
    if (id.empty()) {
        err = "give id= or at=";
        return false;
    }
    Item it;
    if (!find_item(id, r.get_int("index", 0), it, err)) return false;
    x = 0.5f * (it.x0 + it.x1);
    y = 0.5f * (it.y0 + it.y1);
    return true;
}

HttpResponse finish(const HttpRequest& r, uint64_t mark) {
    if (r.get_bool("nowait", false)) return ok_json("{\"ok\":true}");
    if (!wait_applied(mark, r.get_double("timeout", 15.0)))
        return err_json(504, "timed out waiting for the frame loop");
    return ok_json(state_json());
}

HttpResponse handle_click(const HttpRequest& r) {
    float x = 0, y = 0;
    std::string err;
    if (!resolve_point(r, x, y, err)) return err_json(404, err);
    std::vector<Step> steps;
    push_click(steps, x, y, r.get_int("button", 0), r.get_bool("double", false),
               r.get_int("settle", 2));
    return finish(r, enqueue(steps));
}

HttpResponse handle_move(const HttpRequest& r) {
    float x = 0, y = 0;
    std::string err;
    if (!resolve_point(r, x, y, err)) return err_json(404, err);
    Step m;
    m.kind = Step::Kind::MousePos;
    m.x = x; m.y = y;
    return finish(r, enqueue({m, Step{}}));
}

// shift=1 / ctrl=1 hold that modifier down before the press and up after
// the release, so it reads on the completing frame like a real held key
// (a bare /ui/key pairs its own down+up and can't stay held across this).
HttpResponse handle_drag(const HttpRequest& r) {
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    if (std::sscanf(r.get("from").c_str(), "%f,%f", &x0, &y0) != 2 ||
        std::sscanf(r.get("to").c_str(), "%f,%f", &x1, &y1) != 2)
        return err_json(400, "drag wants from=x,y and to=x,y");
    const int n = std::max(1, std::min(256, r.get_int("steps", 8)));
    const int button = r.get_int("button", 0);
    const bool hold_shift = r.get_bool("shift", false);
    const bool hold_ctrl = r.get_bool("ctrl", false);
    const bool hold_space = r.get_bool("space", false);
    const bool esc_mid = r.get_bool("esc_mid", false);
    Step mod;
    mod.kind = Step::Kind::Key;
    if (hold_shift) mod.keys.push_back((int)ImGuiKey_LeftShift);
    if (hold_ctrl) mod.keys.push_back((int)logical_ctrl_key());
    if (hold_space) mod.keys.push_back((int)ImGuiKey_Space);
    std::vector<Step> steps;
    if (!mod.keys.empty()) { mod.down = true; steps.push_back(mod); }
    Step m;
    m.kind = Step::Kind::MousePos;
    m.x = x0; m.y = y0;
    steps.push_back(m);
    steps.push_back(m);
    Step b;
    b.kind = Step::Kind::MouseButton;
    b.button = button;
    b.down = true;
    steps.push_back(b);
    for (int i = 1; i <= n; i++) {
        Step p;
        p.kind = Step::Kind::MousePos;
        p.x = x0 + (x1 - x0) * (float)i / (float)n;
        p.y = y0 + (y1 - y0) * (float)i / (float)n;
        steps.push_back(p);
        if (esc_mid && i == n / 2) {
            Step esc_down; esc_down.kind = Step::Kind::Key;
            esc_down.keys = {(int)ImGuiKey_Escape}; esc_down.down = true;
            Step esc_up = esc_down; esc_up.down = false;
            steps.push_back(esc_down);
            steps.push_back(esc_up);
        }
    }
    b.down = false;
    steps.push_back(b);
    steps.push_back(Step{});
    if (!mod.keys.empty()) { mod.down = false; steps.push_back(mod); }
    steps.push_back(Step{});
    return finish(r, enqueue(steps));
}

HttpResponse handle_scroll(const HttpRequest& r) {
    float x = 0, y = 0;
    std::string err;
    if (!resolve_point(r, x, y, err)) return err_json(404, err);
    Step m;
    m.kind = Step::Kind::MousePos;
    m.x = x; m.y = y;
    Step w;
    w.kind = Step::Kind::Wheel;
    w.x = (float)r.get_double("dx", 0.0);
    w.y = (float)r.get_double("dy", -1.0);
    // Held the way handle_drag holds one: a modified wheel is a distinct
    // gesture (the mask editor's Alt+wheel), not the same one with a flag.
    Step mod;
    mod.kind = Step::Kind::Key;
    if (r.get_bool("shift", false)) mod.keys.push_back((int)ImGuiKey_LeftShift);
    if (r.get_bool("ctrl", false)) mod.keys.push_back((int)logical_ctrl_key());
    if (r.get_bool("alt", false)) mod.keys.push_back((int)ImGuiKey_LeftAlt);
    if (mod.keys.empty()) return finish(r, enqueue({m, m, w, Step{}}));
    std::vector<Step> steps;
    mod.down = true;
    steps.push_back(mod);
    steps.push_back(m);
    steps.push_back(m);
    steps.push_back(w);
    steps.push_back(Step{});
    mod.down = false;
    steps.push_back(mod);
    steps.push_back(Step{});
    return finish(r, enqueue(steps));
}

HttpResponse handle_key(const HttpRequest& r) {
    const std::string spec = r.get("keys");
    if (spec.empty()) return err_json(400, "key wants keys=Ctrl+S");
    std::vector<int> chord;
    if (!parse_chord(spec, chord)) return err_json(400, "unknown key: " + spec);
    Step down, up;
    down.kind = up.kind = Step::Kind::Key;
    down.keys = chord;
    down.down = true;
    up.keys = chord;
    up.down = false;
    return finish(r, enqueue({down, up, Step{}, Step{}}));
}

HttpResponse handle_text(const HttpRequest& r) {
    float x = 0, y = 0;
    std::string err;
    if (!resolve_point(r, x, y, err)) return err_json(404, err);
    std::vector<Step> steps;
    push_click(steps, x, y, 0, false, 1);
    std::vector<int> select_all{(int)logical_ctrl_key(), (int)ImGuiKey_A};
    Step down, up;
    down.kind = up.kind = Step::Kind::Key;
    down.keys = select_all;
    down.down = true;
    up.keys = select_all;
    up.down = false;
    steps.push_back(down);
    steps.push_back(up);
    Step t;
    t.kind = Step::Kind::Text;
    t.text = r.get("value");
    steps.push_back(t);
    steps.push_back(Step{});
    if (r.get_bool("enter", true)) {
        std::vector<int> ret{(int)ImGuiKey_Enter};
        Step ed, eu;
        ed.kind = eu.kind = Step::Kind::Key;
        ed.keys = ret;
        ed.down = true;
        eu.keys = ret;
        eu.down = false;
        steps.push_back(ed);
        steps.push_back(eu);
    }
    steps.push_back(Step{});
    return finish(r, enqueue(steps));
}

// Files dropped on the window, '\n'-separated in `paths=`.
HttpResponse handle_drop(const HttpRequest& r) {
    Step d;
    d.kind = Step::Kind::Drop;
    d.text = r.get("paths");
    if (d.text.empty()) return err_json(400, "paths= is empty");
    return finish(r, enqueue({d, Step{}}));
}

HttpResponse handle_wait(const HttpRequest& r) {
    const int frames = std::max(0, std::min(600, r.get_int("frames", 2)));
    std::vector<Step> steps((size_t)frames + 1, Step{});
    return finish(r, enqueue(steps));
}

// Area average, which is what a screenshot wants: a downscale that samples
// would drop every one-pixel line the interface is drawn with.
std::vector<uint8_t> box_resize(const std::vector<uint8_t>& src, int sw, int sh,
                                int dw, int dh) {
    std::vector<uint8_t> dst((size_t)dw * dh * 4);
    for (int y = 0; y < dh; y++) {
        const int y0 = (int)((int64_t)y * sh / dh);
        const int y1 = std::max(y0 + 1, (int)((int64_t)(y + 1) * sh / dh));
        for (int x = 0; x < dw; x++) {
            const int x0 = (int)((int64_t)x * sw / dw);
            const int x1 = std::max(x0 + 1, (int)((int64_t)(x + 1) * sw / dw));
            int acc[4] = {0, 0, 0, 0};
            for (int sy = y0; sy < y1; sy++)
                for (int sx = x0; sx < x1; sx++)
                    for (int c = 0; c < 4; c++)
                        acc[c] += src[((size_t)sy * sw + sx) * 4 + c];
            const int n = (y1 - y0) * (x1 - x0);
            for (int c = 0; c < 4; c++)
                dst[((size_t)y * dw + x) * 4 + c] = (uint8_t)(acc[c] / n);
        }
    }
    return dst;
}

void png_write_cb(void* ctx, void* data, int size) {
    auto* out = (std::vector<uint8_t>*)ctx;
    out->insert(out->end(), (uint8_t*)data, (uint8_t*)data + size);
}

HttpResponse handle_screenshot(const HttpRequest& r) {
    State& s = st();
    const bool jpeg = r.get("format", "png") == "jpg" ||
                      r.get("format", "png") == "jpeg";
    std::vector<uint8_t> png;
    {
        std::unique_lock<std::mutex> lk(s.mu);
        s.shot.clear();
        s.shot_ready = false;
        s.shot_w = std::max(0, r.get_int("width", 0));
        s.shot_jpeg = jpeg;
        s.shot_quality = std::max(1, std::min(100, r.get_int("quality", 85)));
        s.want_shot = true;
        if (!s.cv.wait_for(lk, std::chrono::duration<double>(
                                   r.get_double("timeout", 10.0)),
                           [&] { return s.shot_ready; })) {
            s.want_shot = false;
            return err_json(504, "the frame loop did not deliver a frame");
        }
        png.swap(s.shot);
    }
    const std::string path = r.get("path");
    if (!path.empty()) {
        std::ofstream f(path, std::ios::binary);
        if (!f) return err_json(500, "cannot write " + path);
        f.write((const char*)png.data(), (std::streamsize)png.size());
        std::string body = "{\"ok\":true,\"bytes\":" +
                           std::to_string(png.size()) + ",\"path\":";
        json_str(body, path);
        body += "}";
        return ok_json(body);
    }
    HttpResponse out;
    out.content_type = jpeg ? "image/jpeg" : "image/png";
    out.body = std::move(png);
    return out;
}

// Every route goes through this: the server is on the loopback interface, but
// a page in a browser can still reach it, and these are GETs with effects.
HttpServer::Handler guard(HttpServer::Handler h) {
    return [h](const HttpRequest& r) -> HttpResponse {
        if (!st().token.empty() && r.get("token") != st().token)
            return err_json(403, "bad or missing token=");
        return h(r);
    };
}

std::string make_token() {
    std::random_device rd;
    std::uniform_int_distribution<int> d(0, 15);
    std::string t;
    for (int i = 0; i < 32; i++) t += "0123456789abcdef"[d(rd)];
    return t;
}

}  // namespace

// ===========================================================================
// Frame loop
// ===========================================================================

namespace gui {
namespace automation {

bool armed() { return st().armed; }

std::vector<std::string> take_drop() {
    std::vector<std::string> out;
    out.swap(st().dropped);
    return out;
}

void set_state_source(std::function<std::string()> f) {
    st().state_source = std::move(f);
}

void arm() {
    if (!spirula::env_on("GUI_AUTOMATION")) return;
    State& s = st();

    ImGuiContext* ctx = ImGui::GetCurrentContext();
    if (!ctx) return;
    ctx->TestEngineHookItems = true;
    // A driven window is rarely the focused one, and ImGui clears the input
    // state it was handed when it believes focus was lost.
    ImGui::GetIO().ConfigDebugIgnoreFocusLoss = true;

    if (const char* t = spirula::env("GUI_AUTOMATION_TOKEN")) s.token = t;
    else s.token = make_token();

    const char* port_env = spirula::env("GUI_AUTOMATION_PORT");
    const int port = port_env ? std::atoi(port_env) : 7777;

    s.http.route("/ui/state", guard([](const HttpRequest&) {
        return ok_json(state_json());
    }));
    s.http.route("/ui/tree", guard(handle_tree));
    s.http.route("/ui/click", guard(handle_click));
    s.http.route("/ui/move", guard(handle_move));
    s.http.route("/ui/drag", guard(handle_drag));
    s.http.route("/ui/scroll", guard(handle_scroll));
    s.http.route("/ui/key", guard(handle_key));
    s.http.route("/ui/text", guard(handle_text));
    s.http.route("/ui/wait", guard(handle_wait));
    s.http.route("/ui/drop", guard(handle_drop));
    s.http.route("/ui/screenshot", guard(handle_screenshot));

    try {
        s.http.start("127.0.0.1", port);
    } catch (const std::exception& e) {
        ctx->TestEngineHookItems = false;
        std::fprintf(stderr, "[automation] not listening: %s\n", e.what());
        return;
    }
    s.armed = true;

    const std::string file = app::config_dir() + "/automation.json";
    std::ofstream f(file);
    f << "{\"host\":\"127.0.0.1\",\"port\":" << port << ",\"token\":\""
      << s.token << "\"}\n";
    std::fprintf(stderr, "[automation] http://127.0.0.1:%d  token in %s\n",
                 port, file.c_str());
}

bool begin_frame() {
    State& s = st();
    if (!s.armed) return false;
    s.building.clear();

    Step step;
    bool popped = false;
    size_t left = 0;
    {
        std::lock_guard<std::mutex> lk(s.mu);
        if (!s.queue.empty()) {
            step = s.queue.front();
            s.queue.pop_front();
            popped = true;
        }
        left = s.queue.size();
    }

    ImGuiIO& io = ImGui::GetIO();
    switch (step.kind) {
        case Step::Kind::None: break;
        case Step::Kind::MousePos: io.AddMousePosEvent(step.x, step.y); break;
        case Step::Kind::MouseButton:
            io.AddMouseButtonEvent(step.button, step.down);
            break;
        case Step::Kind::Wheel: io.AddMouseWheelEvent(step.x, step.y); break;
        case Step::Kind::Key: {
            // Held keys go down outermost-first and come up in the same order,
            // so a modifier is already down when the key it modifies arrives.
            for (int k : step.keys) {
                if (ImGuiKey m = mod_of(k); m != ImGuiKey_None)
                    io.AddKeyEvent(m, step.down);
                io.AddKeyEvent((ImGuiKey)k, step.down);
            }
            break;
        }
        case Step::Kind::Text:
            io.AddInputCharactersUTF8(step.text.c_str());
            break;
        case Step::Kind::Focus: io.AddFocusEvent(true); break;
        case Step::Kind::Drop:
            for (size_t a = 0, b; a < step.text.size(); a = b + 1) {
                b = step.text.find('\n', a);
                if (b == std::string::npos) b = step.text.size();
                if (b > a) s.dropped.push_back(step.text.substr(a, b - a));
            }
            break;
    }

    if (popped) {
        std::lock_guard<std::mutex> lk(s.mu);
        s.applied++;
    }
    s.cv.notify_all();
    return popped || left > 0;
}

void end_frame(int fb_w, int fb_h) {
    State& s = st();
    if (!s.armed) return;

    bool shoot = false;
    int want_w = 0, quality = 85;
    bool jpeg = false;
    // Sampled here, between frames: the HTTP thread calling it would read the
    // app mid-frame, and a torn read of a SAM result's fields was observed.
    std::string app_state = s.state_source ? s.state_source() : std::string();
    {
        std::lock_guard<std::mutex> lk(s.mu);
        s.app_state = std::move(app_state);
        s.published.swap(s.building);
        s.frame_no++;
        const ImGuiIO& io = ImGui::GetIO();
        s.display_w = io.DisplaySize.x;
        s.display_h = io.DisplaySize.y;
        s.fb_w = fb_w;
        s.fb_h = fb_h;
        s.fb_scale = io.DisplaySize.x > 0 ? (float)fb_w / io.DisplaySize.x : 1.0f;
        shoot = s.want_shot;
        want_w = s.shot_w;
        jpeg = s.shot_jpeg;
        quality = s.shot_quality;
    }
    s.building.clear();
    if (!shoot || fb_w <= 0 || fb_h <= 0) {
        s.cv.notify_all();
        return;
    }

    std::vector<uint8_t> rgba((size_t)fb_w * fb_h * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, fb_w, fb_h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    // GL reads bottom-up; every PNG reader expects the other one.
    const size_t row = (size_t)fb_w * 4;
    std::vector<uint8_t> flip((size_t)fb_w * fb_h * 4);
    for (int y = 0; y < fb_h; y++)
        std::memcpy(flip.data() + (size_t)y * row,
                    rgba.data() + (size_t)(fb_h - 1 - y) * row, row);

    int out_w = fb_w, out_h = fb_h;
    if (want_w > 0 && want_w < fb_w) {
        out_w = want_w;
        out_h = std::max(1, (int)((int64_t)fb_h * want_w / fb_w));
        flip = box_resize(flip, fb_w, fb_h, out_w, out_h);
    }
    std::vector<uint8_t> png;
    if (jpeg)
        stbi_write_jpg_to_func(png_write_cb, &png, out_w, out_h, 4,
                               flip.data(), quality);
    else
        stbi_write_png_to_func(png_write_cb, &png, out_w, out_h, 4,
                               flip.data(), out_w * 4);
    {
        std::lock_guard<std::mutex> lk(s.mu);
        s.shot = std::move(png);
        s.shot_ready = true;
        s.want_shot = false;
    }
    s.cv.notify_all();
}

void shutdown() {
    State& s = st();
    if (!s.armed) return;
    s.http.stop();
    s.armed = false;
    s.cv.notify_all();
}

}  // namespace automation
}  // namespace gui

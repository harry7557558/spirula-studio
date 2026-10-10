// ViewBookmarks.cpp -- see ViewBookmarks.h.

#include "app/gui/ViewBookmarks.h"

#include "app/TrainerCore.h"
#include "app/gui/Ui.h"
#include "app/gui/ViewportPanel.h"
#include "data/Json.h"
#include "data/JsonWrite.h"
#include "i18n/catalog/Gui.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <vector>

namespace fs = std::filesystem;
namespace msg = spirula::i18n::msg::gui;
using spirula::Sim3;

namespace gui {

namespace {

// Missing or unreadable is an empty object: nothing saved yet.
JsonValue read_json(const fs::path& p) {
    try {
        return json_parse_file(p.u8string());
    } catch (const std::exception&) {
        return {};
    }
}

void read_doubles(const JsonValue& o, const char* key, double* out, size_t n) {
    const JsonValue* v = o.find(key);
    if (!v || !v->is_array() || v->arr.size() != n) return;
    for (size_t i = 0; i < n; i++) out[i] = v->arr[i].as_double(out[i]);
}

void write_doubles(JsonWriter& w, const char* key, const double* v, int n) {
    w.key(key).array();
    for (int i = 0; i < n; i++) w.raw(json_number_exact(v[i]));
    w.end();
}

// Row-major rotation <-> unit quaternion (w, x, y, z).
void rot_to_quat(const double R[9], double q[4]) {
    Sim3 t;
    std::memcpy(t.R, R, sizeof t.R);
    t.orthonormalize();
    t.quat(q);
}

void quat_to_rot(const double q[4], double R[9]) {
    const double w = q[0], x = q[1], y = q[2], z = q[3];
    const double M[9] = {1 - 2*(y*y + z*z), 2*(x*y - w*z),     2*(x*z + w*y),
                         2*(x*y + w*z),     1 - 2*(x*x + z*z), 2*(y*z - w*x),
                         2*(x*z - w*y),     2*(y*z + w*x),     1 - 2*(x*x + y*y)};
    std::memcpy(R, M, sizeof M);
}

void mat3_mul(const double a[9], const double b[9], double out[9]) {
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            out[r*3+c] = a[r*3+0]*b[0*3+c] + a[r*3+1]*b[1*3+c] + a[r*3+2]*b[2*3+c];
}

// The camera's pose carried through `S`: the eye and pivot as points, the
// view axes rotated, the scale left to the distances.
void map_pose(const Sim3& S, const double pos[3], const double rot_wxyz[4],
              const double target[3], double pos_out[3], double rot_out[4],
              double target_out[3]) {
    double R[9], Rm[9];
    quat_to_rot(rot_wxyz, R);
    mat3_mul(S.R, R, Rm);
    rot_to_quat(Rm, rot_out);
    S.apply(pos, pos_out);
    S.apply(target, target_out);
}

void load(ViewBookmarkSet& s) {
    const JsonValue root = read_json(fs::u8path(s.file));
    if (!root.is_object()) return;
    const JsonValue* views = root.find("views");
    if (!views || !views->is_array()) return;
    for (const JsonValue& v : views->arr) {
        const JsonValue* sl = v.find("slot");
        const int i = sl ? (int)sl->as_int(0) - 1 : -1;
        if (i < 0 || i >= kNumViews) continue;
        ViewBookmark b;
        if (const JsonValue* n = v.find("name")) b.name = n->as_string();
        read_doubles(v, "pos", b.pos, 3);
        read_doubles(v, "rot_wxyz", b.rot, 4);
        read_doubles(v, "target", b.target, 3);
        const double qn = std::sqrt(b.rot[0]*b.rot[0] + b.rot[1]*b.rot[1] +
                                    b.rot[2]*b.rot[2] + b.rot[3]*b.rot[3]);
        if (!(qn > 1e-9) || !std::isfinite(qn)) continue;
        for (double& c : b.rot) c /= qn;
        if (const JsonValue* m = v.find("camera_model"))
            b.cam_model = std::clamp((int)m->as_int(0), 0, 3);
        b.fov_deg = (float)v.get_double("fov_deg", b.fov_deg);
        if (const JsonValue* o = v.find("ortho")) b.ortho = o->as_bool(false);
        b.aspect = (float)v.get_double("aspect", 0.0);
        if (const JsonValue* t = v.find("saved_unix")) b.saved_unix = t->as_int(0);
        b.set = true;
        s.slot[(size_t)i] = b;
    }
}

// The dataset -> splat frame a run trained in (scene_transform.json), so a
// pose saved over one run lands on the same spot in the next.
Sim3 train_from_world(const fs::path& run) {
    const JsonValue st = read_json(run / "scene_transform.json");
    const JsonValue* tw = st.find("train_from_world");
    const JsonValue* m = tw ? tw->find("matrix_3x4_flat_row_major") : nullptr;
    if (!m || !m->is_array() || m->arr.size() != 12) return {};
    double a[12];
    for (int k = 0; k < 12; k++) a[k] = m->arr[(size_t)k].as_double();
    return Sim3::from_3x4(a);
}

std::string saved_time(int64_t t) {
    if (t <= 0) return "-";
    std::tm tm{};
    const std::time_t tt = (std::time_t)t;
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", &tm);
    return buf;
}

}  // namespace

bool ViewBookmarkSet::save() {
    JsonWriter w;
    w.object();
    w.field("version", 1);
    w.field("frame", "dataset");
    w.key("views").array();
    for (int i = 0; i < kNumViews; i++) {
        const ViewBookmark& b = slot[(size_t)i];
        if (!b.set) continue;
        w.object();
        w.field("slot", i + 1);
        if (!b.name.empty()) w.field("name", b.name);
        write_doubles(w, "pos", b.pos, 3);
        write_doubles(w, "rot_wxyz", b.rot, 4);
        write_doubles(w, "target", b.target, 3);
        w.field("camera_model", b.cam_model);
        w.field("fov_deg", b.fov_deg);
        w.field("ortho", b.ortho);
        w.field("aspect", b.aspect);
        w.field("saved_unix", (long long)b.saved_unix);
        w.end();
    }
    w.end();
    w.end();
    // Through a temporary and a rename: a crash mid-write keeps the old file.
    std::error_code ec;
    const fs::path out = fs::u8path(file);
    const fs::path tmp = fs::u8path(file + ".tmp");
    bool ok = false;
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (f) {
            f << w.str();
            ok = (bool)f;
        }
    }
    if (ok) {
        fs::rename(tmp, out, ec);
        ok = !ec;
    }
    if (!ok) fs::remove(tmp, ec);
    error = ok ? std::string() : file;
    return ok;
}

std::shared_ptr<ViewBookmarkSet> view_bookmarks(const std::string& folder) {
    static std::map<std::string, std::shared_ptr<ViewBookmarkSet>> sets;
    std::error_code ec;
    fs::path dir = fs::weakly_canonical(fs::u8path(folder), ec);
    if (ec || dir.empty()) dir = fs::absolute(fs::u8path(folder), ec).lexically_normal();
    std::string key = dir.generic_u8string();
#ifdef _WIN32
    for (char& c : key)
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
#endif
    std::shared_ptr<ViewBookmarkSet>& s = sets[key];
    if (!s) {
        s = std::make_shared<ViewBookmarkSet>();
        s->file = (dir / kViewsFile).u8string();
        load(*s);
    }
    return s;
}

// The trainer navigates the normalized frame: train_to_normalized takes it to
// the parser's frame, which is the dataset's shifted by `center`.
void bind_session_views(ViewportPanel& panel, const spirula::TrainerSession& s) {
    std::error_code ec;
    fs::path dir = fs::u8path(s.cfg.data);
    if (!fs::is_directory(dir, ec)) dir = dir.parent_path();
    if (dir.empty()) {
        panel.set_views(nullptr, Sim3());
        return;
    }
    float a[12];
    for (int k = 0; k < 12; k++) a[k] = s.ds.train_to_normalized[(size_t)k];
    const double c[3] = {s.ds.center[0], s.ds.center[1], s.ds.center[2]};
    panel.set_views(view_bookmarks(dir.u8string()),
                    Sim3::translation(c) * Sim3::from_3x4(a));
}

FileHome file_home(const std::string& asked, const std::string& file) {
    std::error_code ec;
    const fs::path a = fs::u8path(asked);
    const fs::path f = fs::u8path(file.empty() ? asked : file);
    FileHome h;
    fs::path folder;
    // config.json sits in the run folder: beside a mesh, one up from a
    // checkpoint's splat.ply, or it is the folder that was opened.
    std::vector<fs::path> runs = {f.parent_path(), f.parent_path().parent_path()};
    if (fs::is_directory(f, ec)) runs.insert(runs.begin(), f);
    for (const fs::path& run : runs) {
        if (run.empty() || !fs::is_regular_file(run / "config.json", ec)) continue;
        const JsonValue cfg = read_json(run / "config.json");
        if (!cfg.is_object()) continue;
        h.run_dir = run.u8string();
        const JsonValue* data = cfg.find("data");
        if (data && !data->as_string().empty()) {
            fs::path dir = fs::u8path(data->as_string());
            if (dir.is_relative()) dir = run / dir;
            if (!fs::is_directory(dir, ec)) dir = dir.parent_path();
            // A run trained elsewhere (a cloud pod) names a path that does not
            // exist here; filed under <dataset>/outputs/ it still says whose it is.
            if (!fs::is_directory(dir, ec) && run.parent_path().filename() == "outputs")
                dir = run.parent_path().parent_path();
            if (fs::is_directory(dir, ec)) {
                folder = dir;
                h.file_to_dataset = train_from_world(run).inverse();
            }
        }
        break;
    }
    if (folder.empty()) folder = fs::is_directory(a, ec) ? a : f.parent_path();
    h.folder = folder.u8string();
    return h;
}

void bind_file_views(ViewportPanel& panel, const std::string& asked,
                     const std::string& file, const Sim3& file_to_model) {
    const FileHome h = file_home(asked, file);
    if (h.folder.empty()) {
        panel.set_views(nullptr, Sim3());
        return;
    }
    panel.set_views(view_bookmarks(h.folder), h.file_to_dataset * file_to_model.inverse());
}

// ---------------------------------------------------------------------------
// ViewportPanel members
// ---------------------------------------------------------------------------

void ViewportPanel::set_views(std::shared_ptr<ViewBookmarkSet> set,
                              const Sim3& model_to_store) {
    _views = std::move(set);
    _views_m2s = model_to_store;
}

ViewBookmark ViewportPanel::current_pose() const {
    ViewBookmark b;
    if (!_views) return b;
    // shared -> model -> saved frame
    const Sim3 S = _views_m2s * Sim3::from_3x4(_m2s).inverse();
    const double pos[3] = {_cam.pos[0], _cam.pos[1], _cam.pos[2]};
    const double tgt[3] = {_cam.target[0], _cam.target[1], _cam.target[2]};
    const double rot[4] = {_cam.rot[3], _cam.rot[0], _cam.rot[1], _cam.rot[2]};
    map_pose(S, pos, rot, tgt, b.pos, b.rot, b.target);
    b.cam_model = _cam_model;
    b.fov_deg = _fov_deg[_cam_model];
    b.ortho = _ortho;
    b.aspect = _img_h > 1.0f ? _img_w / _img_h : 0.0f;
    b.saved_unix = (int64_t)std::time(nullptr);
    b.set = true;
    return b;
}

int ViewportPanel::current_view() const {
    if (!_views) return -1;
    const ViewBookmark now = current_pose();
    for (int i = 0; i < kNumViews; i++) {
        const ViewBookmark& b = _views->slot[(size_t)i];
        if (!b.set || b.cam_model != now.cam_model || b.ortho != now.ortho ||
            std::fabs(b.fov_deg - now.fov_deg) > 0.05f)
            continue;
        // A recall lands through float, so "on the view" is within rounding.
        double dp = 0.0, dq = 0.0, scale = 1e-9;
        for (int k = 0; k < 3; k++) {
            dp += (b.pos[k] - now.pos[k]) * (b.pos[k] - now.pos[k]);
            scale += (b.target[k] - b.pos[k]) * (b.target[k] - b.pos[k]);
        }
        for (int k = 0; k < 4; k++) dq += b.rot[k] * now.rot[k];
        if (dp <= 1e-8 * scale && std::fabs(dq) > 0.99999) return i;
    }
    return -1;
}

void ViewportPanel::save_view(int slot) {
    if (!_views || slot < 0 || slot >= kNumViews) return;
    const std::string name = _views->slot[(size_t)slot].name;
    _views->slot[(size_t)slot] = current_pose();
    _views->slot[(size_t)slot].name = name;
    _views->save();
}

void ViewportPanel::go_to_view(int slot) {
    if (!_views || slot < 0 || slot >= kNumViews) return;
    const ViewBookmark& b = _views->slot[(size_t)slot];
    if (!b.set) return;
    const Sim3 S = Sim3::from_3x4(_m2s) * _views_m2s.inverse();
    double pos[3], rot[4], tgt[3];
    map_pose(S, b.pos, b.rot, b.target, pos, rot, tgt);
    NavCamera to = _cam;
    for (int k = 0; k < 3; k++) {
        to.pos[k] = (float)pos[k];
        to.target[k] = (float)tgt[k];
    }
    to.rot[0] = (float)rot[1];
    to.rot[1] = (float)rot[2];
    to.rot[2] = (float)rot[3];
    to.rot[3] = (float)rot[0];
    _anim = false;
    _frame_anim = false;
    set_view_lens(b.cam_model, b.fov_deg);
    if (b.ortho) set_ortho(true);
    else _ortho = _ortho_auto = false;
    _glide_from = _cam;
    _glide_to = to;
    _glide_last = _cam;
    _glide = true;
    _glide_t0 = ImGui::GetTime();
    _dirty = true;
}

void ViewportPanel::handle_view_keys() {
    if (!_views) return;
    const ImGuiIO& io = ImGui::GetIO();
    // Ctrl saves, Shift goes; plain digits belong to the editor's tools.
    if (io.KeyAlt || io.KeySuper || io.KeyCtrl == io.KeyShift) return;
    for (int i = 0; i < kNumViews; i++) {
        if (!ImGui::IsKeyPressed((ImGuiKey)(ImGuiKey_1 + i), false)) continue;
        if (io.KeyCtrl) save_view(i);
        else go_to_view(i);
    }
}

float ViewportPanel::views_width() const {
    const ImGuiStyle& st = ImGui::GetStyle();
    return ImGui::CalcTextSize(msg::viewport_views.get()).x +
           kNumViews * (ImGui::GetFrameHeight() + st.ItemSpacing.x);
}

void ViewportPanel::draw_view_buttons() {
    ImGui::PushID("##views");
    ImGui::AlignTextToFramePadding();
    if (_views->error.empty()) {
        ui::Text(msg::viewport_views);
    } else {
        ui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), msg::viewport_views);
        ui::help_on_hover(msg::view_save_failed, {_views->error});
    }
    int rename = -1;
    const float fh = ImGui::GetFrameHeight();
    for (int i = 0; i < kNumViews; i++) {
        ViewBookmark& b = _views->slot[(size_t)i];
        ImGui::SameLine();
        ImGui::PushID(i);
        char face[4], go[16], put[16];
        std::snprintf(face, sizeof face, "%d", i + 1);
        std::snprintf(go, sizeof go, "Shift+%d", i + 1);
        std::snprintf(put, sizeof put, "Ctrl+%d", i + 1);
        if (!b.set) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.4f);
        const bool hit = ui::ButtonRaw(face, ImVec2(fh, 0));
        if (!b.set) ImGui::PopStyleVar();
        if (b.set) {
            const std::string name =
                b.name.empty() ? ui::format(msg::view_default_name, {i + 1}) : b.name;
            char fov[32];
            if (b.cam_model == 3) std::snprintf(fov, sizeof fov, "360\xc2\xb0 x 180\xc2\xb0");
            else std::snprintf(fov, sizeof fov, "FOV %.0f\xc2\xb0", (double)b.fov_deg);
            ui::help_on_hover(msg::view_slot_help,
                              {name, b.ortho ? msg::view_orthographic.get()
                                             : camera_model_label(b.cam_model),
                               fov, saved_time(b.saved_unix), go, put});
        } else {
            ui::help_on_hover(msg::view_empty_help, {put});
        }
        if (hit) {
            if (b.set) go_to_view(i);
            else save_view(i);
        }
        if (ImGui::BeginPopupContextItem("##viewmenu")) {
            if (ui::MenuItem(msg::view_save_here, put)) save_view(i);
            if (ui::MenuItem(msg::view_rename, nullptr, false, b.set)) rename = i;
            if (ui::MenuItem(msg::view_clear, nullptr, false, b.set)) {
                b = ViewBookmark();
                _views->save();
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    if (rename >= 0) {
        _view_rename = rename;
        _view_name_edit = _views->slot[(size_t)rename].name;
        ImGui::OpenPopup("##viewname");
    }
    if (ImGui::BeginPopup("##viewname")) {
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(px(220.0f));
        if (ui::InputTextRaw("##name", &_view_name_edit,
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (_view_rename >= 0 && _view_rename < kNumViews) {
                _views->slot[(size_t)_view_rename].name = _view_name_edit;
                _views->save();
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();
}

}  // namespace gui

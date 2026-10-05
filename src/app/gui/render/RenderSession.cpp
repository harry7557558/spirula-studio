// RenderSession.cpp -- the render mode's state, its viewport interaction and
// the export; the panel is RenderPanel.cpp, the timeline RenderTimeline.cpp.

#include "app/gui/render/RenderSession.h"

#include "app/AppPaths.h"
#include "app/gui/Layout.h"
#include "app/gui/Subprocess.h"
#include "app/gui/ViewportPanel.h"
#include "app/gui/edit/SelectShape.h"
#include "data/CameraMath.h"
#include "data/FrustumSize.h"
#include "i18n/catalog/Render.h"

#include "imgui.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;
namespace msg = spirula::i18n::msg::render;
using spirula::Sim3;
using spirula::i18n::format;
using spirula::i18n::Msg;

namespace gui::render {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr ImU32 kPathCol = IM_COL32(255, 214, 102, 220);
constexpr ImU32 kKeyCol = IM_COL32(235, 235, 235, 230);
constexpr ImU32 kSelCol = IM_COL32(255, 150, 40, 255);
constexpr ImU32 kHotCol = IM_COL32(255, 235, 90, 255);
constexpr ImU32 kHeadCol = IM_COL32(90, 220, 255, 255);
constexpr ImU32 kAxis[3] = {IM_COL32(250, 51, 79, 255), IM_COL32(140, 219, 0, 255),
                            IM_COL32(41, 140, 250, 255)};

double now_s() {
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

void rot_of_c2w(const float c2w[12], double R[9]) {
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) R[r*3+c] = c2w[r*4+c];
}

// A rotation (w, x, y, z) taken through a similarity's rotation.
void rotate_quat(const Sim3& s, const double q[4], double out[4]) {
    double R[9], M[9];
    quat_to_matrix3(q, R);
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) {
            double v = 0.0;
            for (int k = 0; k < 3; k++) v += s.R[r*3+k] * R[k*3+c];
            M[r*3+c] = v;
        }
    quat_from_matrix3(M, out);
}

double smooth(double u) {
    u = std::clamp(u, 0.0, 1.0);
    return u * u * (3.0 - 2.0 * u);
}

std::string seconds_text(double s) {
    char b[32];
    std::snprintf(b, sizeof b, "%.1f", s);
    return b;
}

}  // namespace


RenderSession::RenderSession() = default;

RenderSession::~RenderSession() {
    cancel_export();
    if (_probe.joinable()) _probe.join();
}

void RenderSession::note(const std::string& s, bool done) {
    _log.push_back(s);
    _status = s;
    _status_err = false;
    _status_done = done;
}

std::vector<std::string> RenderSession::drain_log() {
    std::vector<std::string> out;
    out.swap(_log);
    return out;
}

void RenderSession::open(ViewportPanel* panel) {
    _panel = panel;
    if (_panel) _panel->set_interactor(this);
    _preview_key.clear();
    if (_encoder_probe.load() == 0) probe_encoder();
}

void RenderSession::close(bool switching) {
    cancel_export();
    // A camera move is work: one left unsaved goes to autosave.json, never
    // over a project the user named.
    if (_panel && _have_project && _project.keys.size() >= 2 && dirty() && !_sources.empty()) {
        const std::string dir = default_project_dir(_sources[0].path);
        if (!dir.empty()) {
            try {
                const std::string path = (fs::u8path(dir) / "autosave.json").string();
                _project.placement = primary_placement();
                save_project(_project, path);
                if (!switching) _log.push_back(format(msg::project_autosaved, {path}));
            } catch (const std::exception&) {
            }
        }
    }
    _playing = false;
    stop_flight(false);
    _xform.cancel();
    _pick_target = _pick_waiting = false;
    if (_panel) _panel->set_interactor(nullptr);
    _panel = nullptr;
    _frames.set_sources({});
    _frames.destroy_gl();
    _preview_key.clear();
}

bool RenderSession::animating() const {
    return _playing || _flying || _job.state != Job::Idle || _frames.busy() || _pick_waiting;
}

bool RenderSession::dirty() const {
    return _have_project && !_project.keys.empty() &&
           project_to_json(_project) != _saved_json;
}


// ===========================================================================
// Sources
// ===========================================================================

void RenderSession::set_sources(std::vector<SourceInfo> sources) {
    _sources = std::move(sources);
    std::vector<SourceView> views;
    views.reserve(_sources.size());
    std::string sig;
    for (const SourceInfo& s : _sources) {
        views.push_back(s.view);
        sig += s.view.key + "|";
    }
    _frames.set_sources(views);
    if (sig != _sources_sig) {
        _sources_sig = sig;
        _preview_key.clear();
    }
    if (!_have_project) return;
    follow_placement();
    follow_scene_up();
    reconcile_sources();
}

// The project's models against the open ones, by pane, else by path (a file,
// an undo). Open and unlisted joins the list -- or, after an undo, is closed,
// as listed and not open is opened.
void RenderSession::reconcile_sources() {
    const int n = (int)_project.sources.size(), m = (int)_sources.size();
    _src_uid.resize((size_t)n, 0);
    _rt.assign((size_t)n, -1);
    std::vector<uint8_t> taken((size_t)m, 0);
    for (int i = 0; i < n; i++)
        for (int j = 0; j < m && _rt[(size_t)i] < 0; j++)
            if (!taken[(size_t)j] && _src_uid[(size_t)i] && _sources[(size_t)j].uid == _src_uid[(size_t)i]) {
                _rt[(size_t)i] = j;
                taken[(size_t)j] = 1;
            }
    for (int i = 0; i < n; i++)
        for (int j = 0; j < m && _rt[(size_t)i] < 0; j++)
            if (!taken[(size_t)j] && _sources[(size_t)j].path == _project.sources[(size_t)i].path) {
                _rt[(size_t)i] = j;
                taken[(size_t)j] = 1;
                _src_uid[(size_t)i] = _sources[(size_t)j].uid;
            }
    bool settled = true;
    for (int j = 0; j < m; j++) {
        if (taken[(size_t)j]) continue;
        const SourceInfo& s = _sources[(size_t)j];
        if (_sync_models == 2) {
            settled = false;
            if (std::find(_asked_close.begin(), _asked_close.end(), s.uid) == _asked_close.end()) {
                _asked_close.push_back(s.uid);
                if (_remove_model) _remove_model(s.pane);
            }
            continue;
        }
        Source src;
        src.path = s.path;
        _project.sources.push_back(src);
        _src_uid.push_back(s.uid);
        _rt.push_back(j);
    }
    for (int i = 0; i < n; i++) {
        if (_rt[(size_t)i] >= 0 || !_sync_models) continue;
        const std::string& path = _project.sources[(size_t)i].path;
        if (std::find(_asked_open.begin(), _asked_open.end(), path) != _asked_open.end()) continue;
        _asked_open.push_back(path);
        if (_add_model && !path.empty()) _add_model(path);
    }
    for (int i = 0; i < n; i++) settled = settled && _rt[(size_t)i] >= 0;
    if (settled) {
        _sync_models = 0;
        _asked_open.clear();
        _asked_close.clear();
    }
}

int RenderSession::project_source_of(int runtime) const {
    for (int i = 0; i < (int)_rt.size(); i++)
        if (_rt[(size_t)i] == runtime) return i;
    return -1;
}

std::string RenderSession::source_name(int i) const {
    const int r = rt(i);
    if (r >= 0) return _sources[(size_t)r].name;
    if (i < 0 || i >= (int)_project.sources.size()) return {};
    return fs::u8path(_project.sources[(size_t)i].path).filename().string();
}


// ===========================================================================
// Project and history
// ===========================================================================

void RenderSession::new_project() {
    _project = RenderProject();
    _have_project = true;
    _src_uid.clear();
    for (const SourceInfo& s : _sources) {
        Source src;
        src.path = s.path;
        if (s.view.kind == SourceView::Points) src.style.point_style = PointStyle::Circle;
        _project.sources.push_back(src);
        _src_uid.push_back(s.uid);
    }
    _sync_models = 0;
    reconcile_sources();
    // The model on screen, from the start: the first shot is there to change.
    _project.shots.assign(1, Shot{});
    _up_known = scene_up(_project.up);
    _up_taken = false;
    if (_panel) {
        add_key_from_view(0.0, true);
        if (_panel->view_model() == 3) {
            _project.output.width = 3840;
            _project.output.height = 1920;
        }
    }
    _project.placement = primary_placement();
    _tracked_load = _sources.empty() ? 0 : _sources[0].load_id;
    _baked_path.clear();
    _project_path.clear();
    _saved_json = project_to_json(_project);
    reset_history();
    _time = 0.0;
    project_changed();
}

void RenderSession::project_changed() {
    _revision++;
    _sel.resize(_project.keys.size(), 0);
}

const Trajectory& RenderSession::trajectory() {
    if (!_traj || _traj_rev != _revision) {
        _traj = std::make_unique<Trajectory>(_project);
        _traj_rev = _revision;
    }
    return *_traj;
}

void RenderSession::reset_history() {
    _hist.assign(1, Step{project_to_json(_project), &msg::hist_start, _src_uid});
    _head = 0;
}

void RenderSession::commit_history() {
    if (!_have_project || _hist.empty()) return;
    const std::string cur = project_to_json(_project);
    if (cur == _hist[(size_t)_head].json) return;
    // Mid-gesture: a drag is one step, taken when it lets go.
    if (ImGui::IsAnyItemActive() || _xform.active() || _drag_key >= 0 || _scrubbing)
        return;
    const Msg* label = change_label(_hist[(size_t)_head].json, cur);
    _hist.resize((size_t)_head + 1);
    _hist.push_back(Step{cur, label, _src_uid});
    if (_hist.size() > 200) _hist.erase(_hist.begin());
    _head = (int)_hist.size() - 1;
    _hist_scroll = true;
    project_changed();
}

// What a step did, from the project before and after it, as the history
// lists it. Both sides come through the JSON, which is what a step keeps.
const Msg* RenderSession::change_label(const std::string& before, const std::string& after) const {
    RenderProject a, b;
    try {
        a = project_from_json(before);
        b = project_from_json(after);
    } catch (const std::exception&) {
        return &msg::hist_change;
    }
    if (a.sources.size() != b.sources.size())
        return a.sources.size() < b.sources.size() ? &msg::hist_model_add : &msg::hist_model_remove;
    if (a.keys.size() != b.keys.size())
        return a.keys.size() < b.keys.size() ? &msg::hist_keys_add : &msg::hist_keys_delete;
    if (a.shots.size() != b.shots.size())
        return a.shots.size() < b.shots.size() ? &msg::hist_shot_add : &msg::hist_shot_remove;
    bool pose = false, time = false, lens = false, looks = false, other = false;
    for (size_t i = 0; i < a.keys.size(); i++) {
        const Keyframe& x = a.keys[i];
        const Keyframe& y = b.keys[i];
        for (int d = 0; d < 3; d++)
            pose = pose || x.pos[d] != y.pos[d] || x.target[d] != y.target[d];
        for (int d = 0; d < 4; d++) pose = pose || x.rot[d] != y.rot[d];
        pose = pose || x.roll != y.roll;
        time = time || x.time != y.time;
        lens = lens || x.own_lens != y.own_lens || (x.own_lens && x.lens != y.lens);
        looks = looks || x.looks.size() != y.looks.size();
        for (size_t k = 0; k < x.looks.size() && k < y.looks.size(); k++)
            looks = looks || x.looks[k].source != y.looks[k].source || x.looks[k].style != y.looks[k].style;
        other = other || x.hold != y.hold || x.aim != y.aim;
    }
    if (pose) return &msg::hist_keys_move;
    if (time) return &msg::hist_keys_time;
    if (lens) return &msg::hist_lens;
    if (looks) return &msg::hist_looks;
    if (other) return &msg::hist_key_settings;
    const Motion& ma = a.motion;
    const Motion& mb = b.motion;
    if (ma.curve != mb.curve || ma.ease != mb.ease || ma.constant_speed != mb.constant_speed ||
        ma.loop != mb.loop || ma.tension != mb.tension || a.end != b.end)
        return &msg::hist_motion;
    for (size_t i = 0; i < a.sources.size(); i++)
        if (a.sources[i].style != b.sources[i].style || a.sources[i].path != b.sources[i].path)
            return &msg::hist_model_style;
    for (size_t i = 0; i < a.shots.size(); i++) {
        const Shot& x = a.shots[i];
        const Shot& y = b.shots[i];
        if (x.start != y.start || x.source != y.source || x.transition != y.transition ||
            x.duration != y.duration || std::memcmp(x.param, y.param, sizeof x.param) != 0 ||
            std::memcmp(x.colour, y.colour, sizeof x.colour) != 0)
            return &msg::hist_shots;
    }
    if (a.fade_in.colour != b.fade_in.colour || a.fade_in.seconds != b.fade_in.seconds ||
        a.fade_out.colour != b.fade_out.colour || a.fade_out.seconds != b.fade_out.seconds ||
        std::memcmp(a.background, b.background, sizeof a.background) != 0)
        return &msg::hist_fades;
    const Output& oa = a.output;
    const Output& ob = b.output;
    if (oa.kind != ob.kind || oa.width != ob.width || oa.height != ob.height || oa.fps != ob.fps ||
        oa.format != ob.format || oa.jpeg_quality != ob.jpeg_quality || oa.codec != ob.codec ||
        oa.quality != ob.quality || oa.path != ob.path)
        return &msg::hist_output;
    if (std::memcmp(a.up, b.up, sizeof a.up) != 0) return &msg::hist_up;
    return &msg::hist_change;
}

void RenderSession::goto_step(int i) {
    if (i < 0 || i >= (int)_hist.size() || i == _head) return;
    try {
        _project = project_from_json(_hist[(size_t)i].json);
    } catch (const std::exception&) {
        return;
    }
    _head = i;
    _src_uid = _hist[(size_t)i].uids;
    // The models open are brought back to the ones this step had.
    _sync_models = 2;
    _asked_open.clear();
    _asked_close.clear();
    reconcile_sources();
    // Same keys, same selection: undoing a move leaves the camera in hand.
    if (_sel.size() != _project.keys.size()) _sel.assign(_project.keys.size(), 0);
    project_changed();
}

void RenderSession::undo() { goto_step(_head - 1); }
void RenderSession::redo() { goto_step(_head + 1); }

// The primary model's placement now, file coordinates: what the poses are
// laid out against.
Sim3 RenderSession::primary_placement() const {
    if (_sources.empty()) return Sim3();
    return _sources[0].view.norm_to_world * _sources[0].view.file_to_norm;
}

// A dataset's guess at up holds for the frame its model was read in. A model
// turned in the editor was levelled by hand against the pane, so up is then
// the pane's, not the guess turned along with the model.
bool RenderSession::scene_up(double out[3]) const {
    const double z[3] = {0, 0, 1};
    _w2s.inverse().rotate(z, out);
    bool turned = false;
    for (const SourceInfo& s : _sources) {
        const Sim3 placement = s.view.norm_to_world * s.view.file_to_norm;
        Sim3 rot;
        std::copy(placement.R, placement.R + 9, rot.R);
        if (!rot.is_identity(1e-9)) {
            turned = true;
        } else if (s.has_up) {
            placement.rotate(s.up, out);
            return true;
        }
    }
    return turned;
}

// A name for what is made of this move: the project's own, else the primary
// model's, past the file and folder names every run and dataset share.
std::string RenderSession::base_name() const {
    if (!_project_path.empty()) return fs::u8path(_project_path).stem().string();
    if (_sources.empty()) return "render";
    auto generic = [](const std::string& n) {
        static const char* const kNames[] = {"splat", "splats", "point_cloud", "points3D",
                                             "points", "mesh", "mesh_vertexcolor", "model",
                                             "scene", "sparse", "output", "outputs",
                                             "renders", "images", "dense"};
        if (n.empty() || n.rfind("step-", 0) == 0) return true;
        for (const char* k : kNames)
            if (n == k) return true;
        return std::all_of(n.begin(), n.end(), [](char c) { return c >= '0' && c <= '9'; });
    };
    const fs::path p = fs::u8path(_sources[0].path);
    std::string name = p.stem().string();
    for (fs::path d = p.parent_path(); generic(name) && d.has_relative_path(); d = d.parent_path())
        name = d.filename().string();
    return generic(name) ? std::string("render") : name;
}

std::string RenderSession::suggested_project_name() const { return base_name() + ".json"; }

std::string RenderSession::suggested_output_name() const {
    Output o = _project.output;
    o.path = base_name();
    fit_output_path(o);
    return fs::u8path(o.path).filename().string();
}

bool RenderSession::save_in_place() {
    if (_project_path.empty()) return false;
    save_to(_project_path);
    return !_status_err;
}

void RenderSession::ask_save_as() {
    if (!_pick) return;
    const std::string dir = default_project_dir(_sources.empty() ? "" : _sources[0].path);
    _pick(Pick::SaveProject,
          _project_path.empty() ? dir : fs::u8path(_project_path).parent_path().string(),
          _project_path.empty() ? suggested_project_name()
                                : fs::u8path(_project_path).filename().string());
}

void RenderSession::save_to(const std::string& path) {
    _project.placement = primary_placement();
    try {
        save_project(_project, path);
        _project_path = path;
        _saved_json = project_to_json(_project);
        note(format(msg::project_saved, {path}));
    } catch (const std::exception& e) {
        _status = format(msg::project_save_failed, {e.what()});
        _status_err = true;
        _log.push_back(_status);
    }
}

void RenderSession::open_from(const std::string& path) {
    try {
        RenderProject p = load_project(path);
        // Laid out against another placement of this model: carried across.
        const Sim3 now = primary_placement();
        Sim3 diff = now * p.placement.inverse();
        if (!diff.is_identity(1e-9)) transform_project(p, diff);
        p.placement = now;
        _project = std::move(p);
        _have_project = true;
        _up_taken = false;
        _tracked_load = _sources.empty() ? 0 : _sources[0].load_id;
        _baked_path.clear();
        // Its models by path: what it names and is not open is opened, and
        // what is open besides joins it.
        _src_uid.assign(_project.sources.size(), 0);
        _sync_models = 1;
        _asked_open.clear();
        _asked_close.clear();
        reconcile_sources();
        // No shots meant the model on screen throughout: that is the first.
        if (_project.shots.empty()) {
            _project.shots.assign(1, Shot{});
            _project.shots[0].source = std::max(0, project_source_of(0));
        }
        _project_path = path;
        _saved_json = project_to_json(_project);
        reset_history();
        _sel.assign(_project.keys.size(), 0);
        _time = 0.0;
        project_changed();
        note(format(msg::project_opened, {path}));
    } catch (const std::exception& e) {
        _status = format(msg::project_open_failed, {e.what()});
        _status_err = true;
        _log.push_back(_status);
    }
}

void RenderSession::picked(Pick kind, const std::string& path) {
    if (path.empty()) return;
    switch (kind) {
        case Pick::SaveProject: {
            // A name typed without its extension still gets it.
            fs::path p = fs::u8path(path);
            if (p.extension().empty()) p += ".json";
            save_to(p.string());
            break;
        }
        case Pick::OpenProject: open_from(path); break;
        case Pick::Output:
            _project.output.path = path;
            fit_output_path(_project.output);
            project_changed();
            // Asked for by the render button: it goes ahead now.
            if (_export_after_pick) {
                _export_after_pick = false;
                start_export();
            }
            break;
        case Pick::AddModel: break;
    }
}


// ===========================================================================
// Keyframes
// ===========================================================================

bool RenderSession::view_pose(double pos[3], double rot[4], double target[3]) const {
    if (!_panel) return false;
    float c2w[12], tgt[3];
    _panel->nav_pose(c2w, tgt);
    const Sim3 s2w = _w2s.inverse();
    const double p[3] = {c2w[3], c2w[7], c2w[11]}, t[3] = {tgt[0], tgt[1], tgt[2]};
    s2w.apply(p, pos);
    s2w.apply(t, target);
    double R[9], q[4];
    rot_of_c2w(c2w, R);
    quat_from_matrix3(R, q);
    rotate_quat(s2w, q, rot);
    return true;
}

bool RenderSession::selected(int i) const {
    return i >= 0 && i < (int)_sel.size() && _sel[(size_t)i];
}

int RenderSession::single_selected() const {
    int found = -1;
    const int n = std::min((int)_sel.size(), (int)_project.keys.size());
    for (int i = 0; i < n; i++)
        if (_sel[(size_t)i]) {
            if (found >= 0) return -1;
            found = i;
        }
    return found;
}

void RenderSession::select_only(int index) {
    _sel.assign(_project.keys.size(), 0);
    if (index >= 0 && index < (int)_sel.size()) _sel[(size_t)index] = 1;
    _sel_anchor = index;
}

// A list's click: alone, Ctrl toggles one, Shift takes the run from the
// last one clicked.
void RenderSession::click_select(int index, bool ctrl, bool shift) {
    const int n = (int)_project.keys.size();
    if (index < 0 || index >= n) return;
    _sel.resize((size_t)n, 0);
    if (shift && _sel_anchor >= 0 && _sel_anchor < n) {
        if (!ctrl) _sel.assign((size_t)n, 0);
        const int a = std::min(_sel_anchor, index), b = std::max(_sel_anchor, index);
        for (int i = a; i <= b; i++) _sel[(size_t)i] = 1;
        return;
    }
    if (ctrl) _sel[(size_t)index] ^= 1;
    else _sel.assign((size_t)n, 0), _sel[(size_t)index] = 1;
    _sel_anchor = index;
}

void RenderSession::set_playing(bool on) {
    _playing = on && !_project.keys.empty();
    if (!_playing) return;
    if (_time >= _project.duration() - 1e-6) _time = _project.keys.front().time;
    _play_from = _time;
    _play_clock = now_s();
}

double RenderSession::key_visit(int i) {
    const std::vector<double>& v = trajectory().key_times();
    if (i >= 0 && i < (int)v.size()) return v[(size_t)i];
    return i >= 0 && i < (int)_project.keys.size() ? _project.keys[(size_t)i].time : _time;
}

double RenderSession::next_key_time() const {
    const std::vector<Keyframe>& k = _project.keys;
    if (k.empty()) return 0.0;
    int one = single_selected();
    // A playhead on a key counts as that key chosen: two keys at one time
    // would make a gap of no time at all.
    const double half_frame = 0.5 / std::max(_project.output.fps, 1.0);
    for (int i = 0; one < 0 && i < (int)k.size(); i++)
        if (std::fabs(k[(size_t)i].time - _time) < half_frame) one = i;
    if (one >= 0 && one + 1 < (int)k.size())
        return 0.5 * (k[(size_t)one].time + k[(size_t)one + 1].time);
    if (one >= 0 || _time >= k.back().time - 1e-6) {
        // Past the end, one average spacing on.
        const double gap = k.size() >= 2 ? (k.back().time - k.front().time) / (k.size() - 1) : 2.0;
        return k.back().time + std::max(gap, 0.5);
    }
    return _time;
}

int RenderSession::add_key(double time, bool select) {
    std::vector<Keyframe>& keys = _project.keys;
    time = std::max(0.0, time);
    const double frame = 0.5 / std::max(_project.output.fps, 1.0);
    const bool inside = keys.size() >= 2 && time > keys.front().time + frame &&
                        (time < keys.back().time - frame ||
                         (_project.looped() && time < _project.duration() - frame));
    if (!inside) return add_key_from_view(time, select);
    // Where the camera already is at that moment, so nothing else moves.
    const CameraState c = trajectory().at(time);
    Keyframe k;
    k.time = time;
    for (int d = 0; d < 3; d++) k.pos[d] = c.pos[d];
    for (int d = 0; d < 4; d++) k.rot[d] = c.rot[d];
    int before = 0;
    for (int i = 0; i < (int)keys.size(); i++)
        if (keys[(size_t)i].time <= time) before = i;
    const Keyframe& a = keys[(size_t)before];
    const Keyframe& b = keys[((size_t)before + 1) % keys.size()];
    if (a.aim && b.aim) {
        // Aimed on both sides: aimed here too, at the point between theirs.
        const double span = std::max(b.time > a.time ? b.time - a.time
                                                     : _project.duration() - a.time, 1e-9);
        const double f = std::clamp((time - a.time) / span, 0.0, 1.0);
        k.aim = true;
        for (int d = 0; d < 3; d++) k.target[d] = a.target[d] + (b.target[d] - a.target[d]) * f;
        k.roll = roll_of(k.rot, k.pos, k.target, _project.up);
        update_aim(k, _project.up);
    } else {
        double R[9];
        quat_to_matrix3(k.rot, R);
        const double ahead = 1.0 / std::max(_w2s.s, 1e-12);
        for (int d = 0; d < 3; d++) k.target[d] = k.pos[d] - R[d*3+2] * ahead;
    }
    keys.insert(keys.begin() + before + 1, k);
    _sel.insert(_sel.begin() + std::min((int)_sel.size(), before + 1), 0);
    project_changed();
    if (select) select_only(before + 1);
    _time = time;
    return before + 1;
}

int RenderSession::add_key_from_view(double time, bool select) {
    Keyframe k;
    k.time = std::max(0.0, time);
    if (!view_pose(k.pos, k.rot, k.target)) return -1;
    // A new first key starts with the lens the move started with.
    if (_project.keys.empty() || k.time < _project.keys.front().time) {
        k.own_lens = true;
        k.lens = _project.keys.empty() ? view_lens() : _project.lens_at(0);
    }
    _project.keys.push_back(k);
    const double t = k.time;
    _project.sort_keys();
    int index = 0;
    for (int i = 0; i < (int)_project.keys.size(); i++)
        if (_project.keys[(size_t)i].time == t) index = i;
    _sel.insert(_sel.begin() + std::min((int)_sel.size(), index), 0);
    project_changed();
    if (select) select_only(index);
    _time = t;
    return index;
}

Lens RenderSession::view_lens() const {
    Lens l;
    if (!_panel) return l;
    l.projection = (Projection)std::clamp(_panel->view_model(), 0, 3);
    lens_set_fov(l, _panel->view_fov());
    return l;
}

// Aimed at what it was already looking at, a little way on.
void RenderSession::aim_ahead(Keyframe& k) {
    double R[9];
    quat_to_matrix3(k.rot, R);
    const double d = 1.0 / std::max(_w2s.s, 1e-12);
    for (int a = 0; a < 3; a++) k.target[a] = k.pos[a] - R[a*3+2] * d;
    k.aim = true;
    k.roll = roll_of(k.rot, k.pos, k.target, _project.up);
    update_aim(k, _project.up);
}

void RenderSession::update_key_from_view(int index) {
    if (index < 0 || index >= (int)_project.keys.size()) return;
    Keyframe& k = _project.keys[(size_t)index];
    double target[3];
    if (!view_pose(k.pos, k.rot, target)) return;
    if (k.aim) k.roll = roll_of(k.rot, k.pos, k.target, _project.up);
    update_aim(k, _project.up);
    project_changed();
}

void RenderSession::look_through(double time) {
    if (!_panel || _project.keys.empty()) return;
    const CameraState c = trajectory().at(time);
    double c2w[12];
    c.c2w(c2w);
    const double p[3] = {c2w[3], c2w[7], c2w[11]};
    double ps[3];
    _w2s.apply(p, ps);
    double R[9], Rs[9];
    quat_to_matrix3(c.rot, R);
    for (int r = 0; r < 3; r++)
        for (int k = 0; k < 3; k++) {
            double v = 0.0;
            for (int m = 0; m < 3; m++) v += _w2s.R[r*3+m] * R[m*3+k];
            Rs[r*3+k] = v;
        }
    float out[12];
    for (int r = 0; r < 3; r++) {
        for (int k = 0; k < 3; k++) out[r*4+k] = (float)Rs[r*3+k];
        out[r*4+3] = (float)ps[r];
    }
    // The pivot stays as far ahead as it was, so orbiting from here turns
    // about something in front of the camera.
    float nav[12], tgt[3];
    _panel->nav_pose(nav, tgt);
    float d = std::sqrt((tgt[0]-nav[3])*(tgt[0]-nav[3]) + (tgt[1]-nav[7])*(tgt[1]-nav[7]) +
                        (tgt[2]-nav[11])*(tgt[2]-nav[11]));
    if (!(d > 1e-6f)) d = 1.0f;
    const float ahead[3] = {out[3] - out[2] * d, out[7] - out[6] * d, out[11] - out[10] * d};
    _panel->set_nav_pose(out, ahead);
    _panel->set_view_lens((int)c.lens.projection, (float)lens_fov(c.lens));
}

void RenderSession::delete_selected(bool fit) {
    const RenderProject before = _project;
    std::vector<Keyframe> kept;
    // A deleted key's own lens passes to the next key that has none, so the
    // zoom still reaches it.
    bool carry = false;
    Lens carried;
    int first = -1;
    for (int i = 0; i < (int)before.keys.size(); i++) {
        const Keyframe& k = before.keys[(size_t)i];
        if (selected(i)) {
            if (k.own_lens || i == 0) { carry = true; carried = k.lens; }
            continue;
        }
        if (first < 0) first = i;
        kept.push_back(k);
        if (carry && !k.own_lens) {
            kept.back().own_lens = true;
            kept.back().lens = carried;
        }
        carry = false;
    }
    if (kept.size() == before.keys.size()) return;
    if (!kept.empty() && !kept[0].own_lens) {
        kept[0].own_lens = true;
        kept[0].lens = before.lens_at(first);
    }
    _project.keys = std::move(kept);
    if (fit) refit_keys(_project, before, 1.0 / std::max(_w2s.s, 1e-12));
    _sel.assign(_project.keys.size(), 0);
    _sel_anchor = -1;
    project_changed();
}

void RenderSession::space_evenly(double total) {
    const int n = (int)_project.keys.size();
    if (n < 2) return;
    // A loop spends one more interval getting back to the start.
    const int gaps = _project.motion.loop ? n : n - 1;
    total = std::max(total, 0.1 * gaps);
    for (int i = 0; i < n; i++) _project.keys[(size_t)i].time = total * i / gaps;
    _project.end = _project.motion.loop ? total : 0.0;
    // At constant speed the path, not the times, says when a key is passed.
    if (_project.motion.constant_speed) {
        _project.motion.constant_speed = false;
        note(msg::space_evenly_speed.get());
    }
    project_changed();
}

// `what` is what may move: the poses at their times, the times along the
// path, or both. A press is kept only if the camera accelerates less over
// the whole move; else it is tried at half the strength, a few times.
void RenderSession::smooth_keys(double strength, int what) {
    if (_project.keys.size() < 3) return;
    strength = std::clamp(strength, 0.01, 1.0);
    if (what != 0 && _project.motion.constant_speed) {
        // Times mean nothing at constant speed: take the ones the path gave.
        const std::vector<double> visits = trajectory().key_times();
        for (size_t i = 0; i < _project.keys.size() && i < visits.size(); i++)
            _project.keys[i].time = visits[i];
        _project.motion.constant_speed = false;
        note(msg::smooth_speed_off.get());
        project_changed();
    }
    const RenderProject before = _project;
    const double unit = 1.0 / std::max(_w2s.s, 1e-12);
    const double was = motion_energy(before, unit);
    bool any = false;
    for (int i = 0; i < (int)_project.keys.size(); i++) any = any || selected(i);
    std::vector<uint8_t> movable(_project.keys.size(), 1);
    for (int i = 0; any && i < (int)movable.size(); i++) movable[(size_t)i] = selected(i) ? 1 : 0;
    double s = strength;
    for (int tries = 0; tries < 6; tries++, s *= 0.5) {
        _project = before;
        if (what != 1) {
            smooth_pass(s);
            smooth_pass(1.0 / (0.1 - 1.0 / s));
        }
        if (what != 0) smooth_key_speeds(_project, movable, s, unit);
        if (motion_energy(_project, unit) < was * (1.0 - 1e-6)) {
            project_changed();
            return;
        }
    }
    _project = before;
    note(msg::smooth_no_gain.get());
}

// Each key moves part way to where its neighbours' straight line passes at
// its time; the ends, the stops and the timing stay. Taubin's outward second
// pass keeps an orbit from shrinking with every press.
void RenderSession::smooth_pass(double strength) {
    std::vector<Keyframe>& k = _project.keys;
    const int n = (int)k.size();
    bool any = false;
    for (int i = 0; i < n; i++) any = any || selected(i);
    const bool loop = _project.looped();
    const double period = _project.duration() - k.front().time;
    const std::vector<Keyframe> was = k;
    auto lerp_at = [&](int i, auto get, int dims, double* out) {
        const int a = i - 1, b = i + 1;
        const Keyframe& ka = was[(size_t)((a + n) % n)];
        const Keyframe& kb = was[(size_t)(b % n)];
        const double ta = a < 0 ? ka.time - period : ka.time;
        const double tb = b >= n ? kb.time + period : kb.time;
        const double f = tb > ta ? (was[(size_t)i].time - ta) / (tb - ta) : 0.5;
        for (int d = 0; d < dims; d++) out[d] = get(ka, d) + (get(kb, d) - get(ka, d)) * f;
        return f;
    };
    for (int i = 0; i < n; i++) {
        if ((any && !selected(i)) || was[(size_t)i].hold) continue;
        if (!loop && (i == 0 || i == n - 1)) continue;
        Keyframe& ki = k[(size_t)i];
        double p[3];
        const double f = lerp_at(i, [](const Keyframe& x, int d) { return x.pos[d]; }, 3, p);
        for (int d = 0; d < 3; d++) ki.pos[d] += strength * (p[d] - ki.pos[d]);
        const Keyframe& ka = was[(size_t)((i - 1 + n) % n)];
        const Keyframe& kb = was[(size_t)((i + 1) % n)];
        if (ki.aim && ka.aim && kb.aim) {
            double t[3];
            lerp_at(i, [](const Keyframe& x, int d) { return x.target[d]; }, 3, t);
            for (int d = 0; d < 3; d++) ki.target[d] += strength * (t[d] - ki.target[d]);
            ki.roll += strength * ((ka.roll + (kb.roll - ka.roll) * f) - ki.roll);
            update_aim(ki, _project.up);
        } else if (ki.aim) {
            update_aim(ki, _project.up);
        } else {
            // The turn its neighbours make between them, at its time.
            double qa[4], qb[4], mid[4];
            for (int d = 0; d < 4; d++) { qa[d] = ka.rot[d]; qb[d] = kb.rot[d]; }
            double dot = qa[0]*qb[0] + qa[1]*qb[1] + qa[2]*qb[2] + qa[3]*qb[3];
            if (dot < 0) { for (double& v : qb) v = -v; dot = -dot; }
            for (int d = 0; d < 4; d++) mid[d] = qa[d] + (qb[d] - qa[d]) * f;
            double own[4] = {ki.rot[0], ki.rot[1], ki.rot[2], ki.rot[3]};
            if (own[0]*mid[0] + own[1]*mid[1] + own[2]*mid[2] + own[3]*mid[3] < 0)
                for (double& v : own) v = -v;
            double q[4], nq = 0.0;
            for (int d = 0; d < 4; d++) { q[d] = own[d] + strength * (mid[d] - own[d]); nq += q[d] * q[d]; }
            nq = std::sqrt(nq);
            if (nq > 1e-12) for (int d = 0; d < 4; d++) ki.rot[d] = q[d] / nq;
        }
    }
}

void RenderSession::note_saved(const std::string& saved, const Sim3& placement) {
    _baked_path = saved;
    _baked = placement;
}

// The keys live in the primary model's saved coordinates, which its editor
// placement moves: when the placement changes, so do they. A model read back
// from a file that has the placement written in has not moved at all.
void RenderSession::follow_placement() {
    if (!_have_project || _sources.empty()) return;
    const SourceInfo& s = _sources[0];
    const Sim3 now = primary_placement();
    Sim3 move;
    if (s.load_id != _tracked_load) {
        const bool baked = _tracked_load != 0 && !_baked_path.empty() && s.path == _baked_path;
        _tracked_load = s.load_id;
        if (!baked) {
            _project.placement = now;
            return;
        }
        // File coordinates became the old ones moved by the saved placement.
        move = now * _baked * _project.placement.inverse();
        _baked_path.clear();
    } else {
        move = now * _project.placement.inverse();
    }
    if (move.is_identity(1e-9)) {
        _project.placement = now;
        return;
    }
    // Not a step anyone takes back: every snapshot records its own placement
    // and comes back through here.
    const bool stable = !_hist.empty() && project_to_json(_project) == _hist[(size_t)_head].json;
    const bool saved = project_to_json(_project) == _saved_json;
    transform_project(_project, move);
    _project.placement = now;
    if (stable) _hist[(size_t)_head].json = project_to_json(_project);
    if (saved) _saved_json = project_to_json(_project);
    _preview_key.clear();
    project_changed();
}

// Up is the scene's until something is laid out against it: a dataset read
// late, a model turned in the editor, a pane levelled or not. Like a
// placement, not a step anyone takes back.
void RenderSession::follow_scene_up() {
    if (_project.keys.size() > 1 || _up_taken) return;
    double up[3];
    if (!scene_up(up)) return;
    _up_known = true;
    double diff = 0.0;
    for (int k = 0; k < 3; k++) diff += std::fabs(up[k] - _project.up[k]);
    if (diff < 1e-9) return;
    const bool stable = !_hist.empty() && project_to_json(_project) == _hist[(size_t)_head].json;
    const bool saved = project_to_json(_project) == _saved_json;
    std::copy(up, up + 3, _project.up);
    for (Keyframe& k : _project.keys) {
        if (k.aim) k.roll = roll_of(k.rot, k.pos, k.target, _project.up);
        update_aim(k, _project.up);
    }
    if (stable) _hist[(size_t)_head].json = project_to_json(_project);
    if (saved) _saved_json = project_to_json(_project);
    project_changed();
}

void RenderSession::remove_source(int index) {
    if (index < 0 || index >= (int)_project.sources.size() || _project.sources.size() < 2) return;
    const int r = rt(index);
    const int pane = r >= 0 ? _sources[(size_t)r].pane : -1;
    for (Shot& sh : _project.shots) {
        if (sh.source == index) sh.source = 0;
        else if (sh.source > index) sh.source--;
    }
    for (Keyframe& k : _project.keys) {
        std::vector<KeyLook> kept;
        for (KeyLook l : k.looks) {
            if (l.source == index) continue;
            if (l.source > index) l.source--;
            kept.push_back(l);
        }
        k.looks = std::move(kept);
    }
    _project.sources.erase(_project.sources.begin() + index);
    if (index < (int)_src_uid.size()) _src_uid.erase(_src_uid.begin() + index);
    // The model shown goes: the next one open is shown instead, and the keys
    // cross from the old one's placement to its, not to wherever it was read.
    if (r == 0 && _sources.size() >= 2) {
        _tracked_load = _sources[1].load_id;
        _baked_path.clear();
    }
    reconcile_sources();
    project_changed();
    if (_remove_model && pane >= 0) _remove_model(pane);
}

void RenderSession::view_source(int index, bool edit) {
    const int r = rt(index);
    if (r < 0 || !_view_model) return;
    // Shown from another model: the keys cross to its placement as above.
    if (r != 0) {
        _tracked_load = _sources[(size_t)r].load_id;
        _baked_path.clear();
    }
    _view_model(_sources[(size_t)r].pane, edit);
}

// The view's own up, for a model no dataset levels: turn the view until the
// horizon is level, then take it.
void RenderSession::take_up_from_view() {
    double pos[3], rot[4], target[3];
    if (!view_pose(pos, rot, target)) return;
    double R[9];
    quat_to_matrix3(rot, R);
    for (int k = 0; k < 3; k++) _project.up[k] = R[k*3+1];
    _up_known = true;
    _up_taken = true;
    for (Keyframe& k : _project.keys) update_aim(k, _project.up);
    project_changed();
}

// A full turn about the point the view orbits, at the view's own distance
// and height: the classic product shot, as ordinary keys.
void RenderSession::make_orbit() {
    if (!_panel) return;
    double pos[3], rot[4], target[3];
    if (!view_pose(pos, rot, target)) return;
    const double* up = _project.up;
    double rel[3] = {pos[0] - target[0], pos[1] - target[1], pos[2] - target[2]};
    double h = rel[0]*up[0] + rel[1]*up[1] + rel[2]*up[2];
    double flat[3] = {rel[0] - h*up[0], rel[1] - h*up[1], rel[2] - h*up[2]};
    double r = std::sqrt(flat[0]*flat[0] + flat[1]*flat[1] + flat[2]*flat[2]);
    const double dist = std::sqrt(r*r + h*h);
    if (!(dist > 1e-9)) return;
    // From overhead the circle would shrink to a spin on the spot: tip it to
    // 30 degrees, toward the bottom of the picture so the scene keeps its way up.
    if (r < 0.5 * dist) {
        if (r < 1e-3 * dist) {
            const double y[3] = {0, 1, 0};
            double sy[3];
            quat_rotate(rot, y, sy);
            const double k = sy[0]*up[0] + sy[1]*up[1] + sy[2]*up[2];
            for (int d = 0; d < 3; d++) flat[d] = -(sy[d] - k * up[d]);
            r = std::sqrt(flat[0]*flat[0] + flat[1]*flat[1] + flat[2]*flat[2]);
            if (!(r > 1e-9)) return;
        }
        for (double& v : flat) v *= dist * std::cos(kPi / 6) / r;
        r = dist * std::cos(kPi / 6);
        h = (h < 0 ? -1.0 : 1.0) * dist * std::sin(kPi / 6);
    }
    double side[3] = {up[1]*flat[2] - up[2]*flat[1], up[2]*flat[0] - up[0]*flat[2],
                      up[0]*flat[1] - up[1]*flat[0]};
    const double sl = std::sqrt(side[0]*side[0] + side[1]*side[1] + side[2]*side[2]);
    for (double& v : side) v /= std::max(sl, 1e-12);
    const Lens lens = _project.keys.empty() ? view_lens() : _project.lens_at(0);
    _project.keys.clear();
    constexpr int kSteps = 8;
    constexpr double kSeconds = 12.0;
    for (int i = 0; i < kSteps; i++) {
        const double a = 2.0 * kPi * i / kSteps;
        Keyframe k;
        k.time = kSeconds * i / kSteps;
        for (int d = 0; d < 3; d++)
            k.pos[d] = target[d] + h * up[d] + std::cos(a) * flat[d] + std::sin(a) * r * side[d];
        k.aim = true;
        for (int d = 0; d < 3; d++) k.target[d] = target[d];
        if (i == 0) { k.own_lens = true; k.lens = lens; }
        update_aim(k, _project.up);
        _project.keys.push_back(k);
    }
    // A closed loop: the video ends where it began, and plays round and round.
    _project.motion.curve = Curve::Spline;
    _project.motion.constant_speed = true;
    _project.motion.ease = false;
    _project.motion.loop = true;
    _project.end = kSeconds;
    _project.sort_keys();
    _sel.assign(_project.keys.size(), 0);
    _time = 0.0;
    project_changed();
}

// The capture's own path: the dataset's cameras in the order they were
// taken, thinned to a key every few metres of travel.
void RenderSession::follow_capture() {
    const ParsedDataset* ds = nullptr;
    int source = -1;
    for (size_t i = 0; i < _sources.size() && !ds; i++) {
        if (_sources[i].view.kind == SourceView::Points) { ds = _sources[i].view.ds; source = (int)i; }
        else if (_sources[i].dataset) { ds = _sources[i].dataset; source = (int)i; }
    }
    if (!ds || ds->num_cameras < 2) return;
    // Cameras live in the model's file frame: the points' own, or the
    // training frame a model from the dataset was saved in.
    const Sim3 to_world = _sources[(size_t)source].view.norm_to_world *
                          _sources[(size_t)source].view.file_to_norm;
    std::vector<int> order((size_t)ds->num_cameras);
    for (int i = 0; i < (int)order.size(); i++) order[(size_t)i] = i;
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return ds->image_filenames.size() > (size_t)std::max(a, b)
                   ? ds->image_filenames[(size_t)a] < ds->image_filenames[(size_t)b]
                   : a < b;
    });
    const int want = std::clamp((int)order.size() / 12, 4, 16);
    // The capture's own lens, as the frames were taken.
    Lens lens = _project.keys.empty() ? view_lens() : _project.lens_at(0);
    const std::vector<DatasetLens> lenses = cluster_dataset_lenses(*ds);
    if (!lenses.empty()) lens = lenses.front().lens;
    _project.keys.clear();
    for (int j = 0; j < want; j++) {
        const int i = order[(size_t)((order.size() - 1) * j / (want - 1))];
        const float* M = &ds->c2w[(size_t)i * 12];
        Keyframe k;
        k.time = 1.5 * j;
        const double p[3] = {M[3], M[7], M[11]};
        to_world.apply(p, k.pos);
        double R[9], q[4];
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++) R[r*3+c] = M[r*4+c];
        quat_from_matrix3(R, q);
        rotate_quat(to_world, q, k.rot);
        if (j == 0) { k.own_lens = true; k.lens = lens; }
        _project.keys.push_back(k);
    }
    _project.motion.curve = Curve::Spline;
    _project.motion.ease = true;
    _project.motion.loop = false;
    _project.end = 0.0;
    _project.sort_keys();
    _sel.assign(_project.keys.size(), 0);
    _time = 0.0;
    project_changed();
}


void RenderSession::start_flight() {
    if (!_panel || _flying) return;
    _flying = true;
    _playing = false;
    _xform.cancel();
    _pick_target = _pick_waiting = false;
    // The keys would take the letters the flight flies with.
    _sel.assign(_project.keys.size(), 0);
    if (_preview_mode == PreviewMode::Through) _preview_mode = PreviewMode::Corner;
    _flight.clear();
    // Kept through what it was flown through.
    _flight_lens = view_lens();
    _fly_t0 = now_s();
    _fly_elapsed = 0.0;
}

void RenderSession::stop_flight(bool keep) {
    if (!_flying) return;
    _flying = false;
    if (!keep) {
        _flight.clear();
        return;
    }
    _fit_length_set = false;
    _fit.length = 0.0;
    refit_flight();
    if (_fit_keys.empty()) note(msg::fly_nothing.get());
    else note(format(msg::fly_done, {(long long)_project.keys.size(), seconds_text(_project.duration())}));
}

std::string RenderSession::keys_json() const {
    RenderProject k;
    k.keys = _project.keys;
    return project_to_json(k);
}

void RenderSession::refit_flight() {
    FlightFit f = _fit;
    if (!_fit_length_set) f.length = 0.0;
    RenderProject p = _project;
    if (fit_flight(_flight, f, 1.0 / std::max(_w2s.s, 1e-12), _flight_lens, p) == 0) {
        _fit_keys.clear();
        return;
    }
    _project.keys = std::move(p.keys);
    _project.motion = p.motion;
    _project.end = p.end;
    _fit.length = _project.keys.back().time - _project.keys.front().time;
    _sel.assign(_project.keys.size(), 0);
    _sel_anchor = -1;
    _time = 0.0;
    project_changed();
    _fit_keys = keys_json();
}


// ===========================================================================
// Viewport
// ===========================================================================

bool RenderSession::view_projection(ViewProjection& vp, int W, int H) const {
    if (!_panel || W <= 0 || H <= 0) return false;
    _panel->nav_camera(W, H, vp.w2c, vp.fx, vp.fy, vp.camera_model, vp.eye);
    vp.cx = 0.5f * W;
    vp.cy = 0.5f * H;
    vp.W = W;
    vp.H = H;
    vp.ortho_back = _panel->ortho_pullback(true);
    return true;
}

void RenderSession::to_shared(const double w[3], double s[3]) const { _w2s.apply(w, s); }

bool RenderSession::key_screen(int index, const ViewProjection& vp, float& x,
                               float& y) const {
    double s[3];
    to_shared(_project.keys[(size_t)index].pos, s);
    const float p[3] = {(float)s[0], (float)s[1], (float)s[2]};
    float d;
    return vp.project(p, x, y, d) && d > 0.0f;
}

int RenderSession::hit_key(float x, float y) const {
    float ix, iy, iw, ih;
    if (!_panel) return -1;
    _panel->image_rect(ix, iy, iw, ih);
    ViewProjection vp;
    if (!view_projection(vp, (int)iw, (int)ih)) return -1;
    // The apex from a little further off, then any line of the camera.
    int best = -1;
    float best_d = px(12.0f);
    for (int i = 0; i < (int)_project.keys.size(); i++) {
        float kx, ky;
        if (!key_screen(i, vp, kx, ky)) continue;
        const float d = std::hypot(kx - x, ky - y);
        if (d < best_d) { best_d = d; best = i; }
    }
    if (best >= 0) return best;
    best_d = px(5.0f);
    for (int i = 0; i < (int)_key_lines.size() && i < (int)_project.keys.size(); i++) {
        const std::vector<float>& l = _key_lines[(size_t)i];
        for (size_t j = 0; j + 3 < l.size(); j += 4) {
            const float ax = l[j], ay = l[j + 1], bx = l[j + 2], by = l[j + 3];
            const float dx = bx - ax, dy = by - ay, len2 = dx * dx + dy * dy;
            const float f = len2 > 0.0f ? std::clamp(((x - ax) * dx + (y - ay) * dy) / len2,
                                                     0.0f, 1.0f) : 0.0f;
            const float d = std::hypot(ax + f * dx - x, ay + f * dy - y);
            if (d < best_d) { best_d = d; best = i; }
        }
    }
    return best;
}

bool RenderSession::xform_frame(XformFrame& f, XformKind kind) {
    float ix, iy, iw, ih;
    if (!_panel) return false;
    _panel->image_rect(ix, iy, iw, ih);
    if (!view_projection(f.cam, (int)iw, (int)ih)) return false;
    const std::vector<Keyframe>& keys = _xform.active() ? _xform_from : _project.keys;
    double c[3] = {0, 0, 0};
    int n = 0, one = -1;
    for (int i = 0; i < (int)keys.size() && i < (int)_sel.size(); i++) {
        if (!_sel[(size_t)i]) continue;
        for (int d = 0; d < 3; d++) c[d] += keys[(size_t)i].pos[d];
        n++;
        if (one < 0 || i == _sel_anchor) one = i;
    }
    if (!n) return false;
    // One camera turns on the spot; several about the pivot chosen.
    for (double& v : c) v /= n;
    (void)kind;
    if (n > 1 && _pivot == 0) {
        c[0] = c[1] = c[2] = 0.0;
    } else if (n > 1 && _pivot == 1) {
        for (int d = 0; d < 3; d++) {
            std::vector<double> v;
            for (int i = 0; i < (int)keys.size() && i < (int)_sel.size(); i++)
                if (_sel[(size_t)i]) v.push_back(keys[(size_t)i].pos[d]);
            std::sort(v.begin(), v.end());
            c[d] = v.size() % 2 ? v[v.size() / 2]
                                : 0.5 * (v[v.size() / 2 - 1] + v[v.size() / 2]);
        }
    }
    to_shared(c, f.pivot);
    for (int a = 0; a < 3; a++)
        for (int k = 0; k < 3; k++) f.global_axes[a*3+k] = _w2s.R[k*3+a];
    // Its own axes: the one camera's, or with several the one clicked
    // last, whose step each of the others then takes in its own.
    {
        double R[9];
        quat_to_matrix3(keys[(size_t)one].rot, R);
        for (int a = 0; a < 3; a++)
            for (int k = 0; k < 3; k++) {
                double v = 0.0;
                for (int m = 0; m < 3; m++) v += _w2s.R[k*3+m] * R[m*3+a];
                f.local_axes[a*3+k] = v;
            }
    }
    f.unit = _w2s.s;
    f.grid_cell = _panel->world_grid_cell();
    f.each = n > 1;
    return true;
}

void RenderSession::begin_xform(XformKind kind, float mx, float my, bool drag,
                                int axis, bool plane) {
    if (_project.keys.empty()) return;
    bool any = false;
    for (uint8_t s : _sel) any = any || s;
    if (!any) return;
    _xform_from = _project.keys;
    XformFrame f;
    if (!xform_frame(f, kind)) return;
    _xform.begin(kind, f, mx, my, drag, axis, plane);
}

void RenderSession::apply_xform(const Sim3& step_shared, bool scale_fov, double factor) {
    const Sim3 s2w = _w2s.inverse();
    Sim3 step = s2w * step_shared * _w2s;
    int n = 0;
    for (uint8_t s : _sel) n += s ? 1 : 0;
    // An axis pressed twice with several cameras: each takes the step the
    // one clicked last takes, in its own frame and about itself -- R X X
    // tilts every camera of an orbit by the same pitch.
    const bool each = n > 1 && _xform.local() && _xform.kind() != XformKind::Scale;
    auto frame_of = [&](const double pos[3], const double axes[9]) {
        Sim3 F;
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++) F.R[r*3+c] = axes[c*3+r];
        for (int d = 0; d < 3; d++) F.t[d] = pos[d];
        return F;
    };
    // From the keys as the operator found them, confirmed or not.
    _project.keys = _xform_from;
    Sim3 in_ref;
    if (each) {
        XformFrame f;
        if (!xform_frame(f, _xform.kind())) return;
        const Sim3 F = frame_of(f.pivot, f.local_axes);
        in_ref = F.inverse() * step_shared * F;
    }
    for (int i = 0; i < (int)_project.keys.size() && i < (int)_sel.size(); i++) {
        if (!_sel[(size_t)i]) continue;
        Keyframe& k = _project.keys[(size_t)i];
        if (each) {
            double R[9], axes[9], p[3];
            quat_to_matrix3(k.rot, R);
            for (int a = 0; a < 3; a++)
                for (int d = 0; d < 3; d++) {
                    double v = 0.0;
                    for (int m = 0; m < 3; m++) v += _w2s.R[d*3+m] * R[m*3+a];
                    axes[a*3+d] = v;
                }
            to_shared(k.pos, p);
            const Sim3 F = frame_of(p, axes);
            step = s2w * (F * in_ref * F.inverse()) * _w2s;
        }
        if (scale_fov) {
            if (!k.own_lens) { k.lens = _project.lens_at(i); k.own_lens = true; }
            // A bigger frustum is a wider view.
            const double fov = lens_fov(k.lens);
            const double t = std::tan(std::min(fov, 179.0) * kPi / 360.0) * factor;
            if (k.lens.projection == Projection::Perspective)
                lens_set_fov(k.lens, std::clamp(std::atan(t) * 360.0 / kPi, 1.0, 170.0));
            else if (k.lens.projection != Projection::Equirect)
                lens_set_fov(k.lens, std::clamp(fov * factor, 10.0, 360.0));
            continue;
        }
        double p[3], t[3];
        step.apply(k.pos, p);
        for (int d = 0; d < 3; d++) k.pos[d] = p[d];
        if (_xform.kind() == XformKind::Rotate) {
            double q[4];
            rotate_quat(step, k.rot, q);
            for (int d = 0; d < 4; d++) k.rot[d] = q[d];
            if (k.aim) {
                // What it looks at swings round with it, and the roll is
                // whatever the turn left.
                step.apply(k.target, t);
                for (int d = 0; d < 3; d++) k.target[d] = t[d];
                k.roll = roll_of(k.rot, k.pos, k.target, _project.up);
                update_aim(k, _project.up);
            }
        } else if (k.aim) {
            // Moved on its own, an aimed camera keeps looking at its point;
            // moved with others, the point goes along. Scaling spreads the
            // cameras only: an orbit widened still looks at its subject.
            if (n > 1 && _xform.kind() == XformKind::Move) {
                step.apply(k.target, t);
                for (int d = 0; d < 3; d++) k.target[d] = t[d];
            }
            update_aim(k, _project.up);
        }
    }
    project_changed();
}

// The selected cameras, or all of them, with room for their frusta.
bool RenderSession::frame_bounds(double centre[3], double& radius) {
    if (!_panel || _xform.active() || _project.keys.empty()) return false;
    bool any = false;
    for (int i = 0; i < (int)_project.keys.size(); i++) any = any || selected(i);
    double c[3] = {0, 0, 0};
    int n = 0;
    for (int i = 0; i < (int)_project.keys.size(); i++) {
        if (any && !selected(i)) continue;
        for (int d = 0; d < 3; d++) c[d] += _project.keys[(size_t)i].pos[d];
        n++;
    }
    for (double& v : c) v /= std::max(n, 1);
    double r = 0.0;
    for (int i = 0; i < (int)_project.keys.size(); i++) {
        if (any && !selected(i)) continue;
        const double* p = _project.keys[(size_t)i].pos;
        r = std::max(r, std::sqrt((p[0]-c[0])*(p[0]-c[0]) + (p[1]-c[1])*(p[1]-c[1]) +
                                  (p[2]-c[2])*(p[2]-c[2])));
    }
    to_shared(c, centre);
    radius = (r + 2.0 * camera_size()) * _w2s.s;
    return radius > 0.0;
}

bool RenderSession::owns_left_button() const {
    if (_flying) return false;
    return _xform.active() || _pick_target || _pick_waiting || _hot >= 0 ||
           _handle_hot >= 0 || _pip_hot || _pip_resizing || over_pip();
}

bool RenderSession::over_pip() const {
    return _pip_rect[2] > 0.0f && _mouse_in && _mouse[0] >= _pip_rect[0] &&
           _mouse[0] <= _pip_rect[0] + _pip_rect[2] && _mouse[1] >= _pip_rect[1] &&
           _mouse[1] <= _pip_rect[1] + _pip_rect[3];
}

bool RenderSession::blocks_fly_keys() const {
    if (_flying) return false;
    if (_xform.active() || _fly_block_key != 0) return true;
    for (uint8_t s : _sel)
        if (s) return true;
    return false;
}

bool RenderSession::on_viewport_input(const ViewportInput& in) {
    _mouse[0] = in.x;
    _mouse[1] = in.y;
    _mouse_in = in.hovered;
    // Flying: the view is the navigation's alone.
    if (_flying) { _hot = _handle_hot = -1; return false; }

    // The corner picture: its grip resizes it, a double-click makes it the view.
    if (_pip_resizing) {
        if (!in.down) {
            _pip_resizing = false;
        } else if (_pip_pane_w > 1.0f) {
            const float m = px(10.0f);
            const float w = _pip_right ? _pip_rect[0] + _pip_rect[2] - in.x : in.x - m;
            _pip_share = std::clamp(w / _pip_pane_w, 0.1f, 0.9f);
            _preview_key.clear();
        }
        return true;
    }
    // A running operator takes every event, over the corner picture too.
    if (_xform.active()) {
        XformFrame f;
        if (!xform_frame(f, _xform.kind())) { _xform.cancel(); return true; }
        const TransformTool::Result r = _xform.update(in, f);
        // S on one camera is its zoom; on several, their spread.
        const bool fov = _xform.kind() == XformKind::Scale && single_selected() >= 0;
        if (r == TransformTool::Result::Cancelled) {
            _project.keys = _xform_from;
            project_changed();
        } else {
            apply_xform(_xform.delta(), fov, _xform.delta().s);
        }
        if (!_xform.active() && _press_key >= 0) {
            if (std::hypot(in.x - _press[0], in.y - _press[1]) < px(4.0f)) select_only(_press_key);
            _press_key = -1;
        }
        return true;
    }
    _pip_hot = false;
    if (_pip_rect[2] > 0.0f && in.hovered) {
        const float gx = _pip_right ? _pip_rect[0] : _pip_rect[0] + _pip_rect[2];
        const float gy = _pip_rect[1];
        _pip_hot = std::fabs(in.x - gx) < px(14.0f) && std::fabs(in.y - gy) < px(14.0f);
        const bool inside = in.x >= _pip_rect[0] && in.x <= _pip_rect[0] + _pip_rect[2] &&
                            in.y >= _pip_rect[1] && in.y <= _pip_rect[1] + _pip_rect[3];
        if (_pip_hot && in.clicked) {
            _pip_resizing = true;
            return true;
        }
        if (inside && in.double_clicked) {
            _preview_mode = PreviewMode::Through;
            _preview_key.clear();
            return true;
        }
        if (_pip_hot || inside) return true;
    }
    if (_project.keys.empty() && !_pick_target) { _hot = _handle_hot = -1; return false; }

    // Escape is routed as in handle_keys: a window over the viewer keeps it.
    if (_pick_target) {
        if (in.clicked) {
            _panel->request_pick(in.x / std::max(in.W, 1), in.y / std::max(in.H, 1));
            _pick_waiting = true;
            _pick_target = false;
        } else if (in.right_clicked ||
                   ImGui::Shortcut(ImGuiKey_Escape, ImGuiInputFlags_RouteFocused |
                                                    ImGuiInputFlags_RouteFromRootWindow)) {
            _pick_target = false;
        }
        return true;
    }

    _handle_hot = -1;
    const int one = single_selected();
    if (one >= 0) {
        XformFrame f;
        if (xform_frame(f, _handle_mode)) {
            _handle_hot = _xform.hit_handle(_handle_mode, f, in.x, in.y);
            if (_handle_hot >= 0 && in.clicked) {
                const int axis = _handle_hot < 3 ? _handle_hot
                               : _handle_hot < 6 ? _handle_hot - 3 : -1;
                begin_xform(_handle_mode, in.x, in.y, true, axis, _handle_hot >= 3 && _handle_hot < 6);
                return true;
            }
        }
    }
    _hot = in.hovered ? hit_key(in.x, in.y) : -1;
    if (_hot >= 0 && in.clicked) {
        // As in the list: Ctrl toggles, Shift takes the run from the last.
        _press[0] = in.x;
        _press[1] = in.y;
        _press_key = -1;
        if (in.shift || in.ctrl) click_select(_hot, in.ctrl, in.shift);
        else if (!selected(_hot)) select_only(_hot);
        else _press_key = _hot;
        _time = key_visit(_hot);
        // Pressing on a camera and dragging moves it, the way a handle does.
        if (selected(_hot)) begin_xform(XformKind::Move, in.x, in.y, true);
        else _press_key = -1;
        return true;
    }
    // A click on nothing clears the selection; a drag from there still turns
    // the view.
    if (in.clicked && _handle_hot < 0) {
        _press_empty = !in.shift && !in.ctrl;
        _press[0] = in.x;
        _press[1] = in.y;
    }
    if (in.released) {
        if (_press_empty && std::hypot(in.x - _press[0], in.y - _press[1]) < px(4.0f))
            _sel.assign(_project.keys.size(), 0);
        _press_empty = false;
    }
    return _hot >= 0 || _handle_hot >= 0;
}

const camhost::FrustumShape& RenderSession::frustum_shape(const Lens& l) const {
    const int W = std::max(_project.output.width, 1), H = std::max(_project.output.height, 1);
    char key[160];
    std::snprintf(key, sizeof key, "%d|%d|%.9g|%dx%d", (int)l.projection, l.tier, l.focal, W, H);
    std::string k = key;
    for (int d = 0; l.tier && d < kLensCoeffs; d++) {
        std::snprintf(key, sizeof key, "|%.9g", l.dist[d]);
        k += key;
    }
    auto it = _shapes.find(k);
    if (it != _shapes.end()) return it->second;
    // Keys between two zooms each have a lens of their own.
    if (_shapes.size() > 512) _shapes.clear();
    float in[4];
    lens_intrinsics(l, W, H, in);
    const int model = std::clamp((int)l.projection, 0, 3);
    const int tier = model == 3 ? 0 : std::clamp(l.tier, 0, 2);
    return _shapes.emplace(k, camhost::frustum_template(model, tier, W, H, in[0], in[1], in[2],
                                                        in[3], l.dist)).first->second;
}

void RenderSession::draw_camera(ImDrawList* dl, const ViewProjection& vp, float ox,
                                float oy, const CameraState& c, unsigned col,
                                float scale, bool axes, std::vector<float>* hit) const {
    double s[3];
    to_shared(c.pos, s);
    const float sp[3] = {(float)s[0], (float)s[1], (float)s[2]};
    float cx, cy, depth;
    if (!vp.project(sp, cx, cy, depth) || depth <= 0.0f) return;
    // One size in the world for every camera, however near or far it is.
    const double L = camera_size() * scale;
    double R[9];
    quat_to_matrix3(c.rot, R);
    // Camera axes are OpenGL's; the frustum's are CV (+Y down, +Z on).
    auto world_gl = [&](double x, double y, double z, double out[3]) {
        for (int r = 0; r < 3; r++)
            out[r] = c.pos[r] + L * (R[r*3+0]*x + R[r*3+1]*y + R[r*3+2]*z);
    };
    auto screen_of = [&](const double w[3], ImVec2& out) {
        double sh[3];
        to_shared(w, sh);
        const float p[3] = {(float)sh[0], (float)sh[1], (float)sh[2]};
        float x, y, d;
        if (!vp.project(p, x, y, d) || d <= 0.0f) return false;
        out = ImVec2(ox + x, oy + y);
        return true;
    };
    auto screen_cv = [&](float x, float y, float z, ImVec2& out) {
        double w[3];
        world_gl(x, -y, -z, w);
        return screen_of(w, out);
    };
    auto line = [&](ImVec2 a, ImVec2 b, ImU32 cl, float t) {
        dl->AddLine(a, b, cl, t);
        if (hit) hit->insert(hit->end(), {a.x - ox, a.y - oy, b.x - ox, b.y - oy});
    };
    const ImVec2 apex(ox + cx, oy + cy);
    const float th = px(1.5f);
    const ImU32 dim = (col & 0x00ffffff) | 0x60000000;

    // The web viewer's frustum (data/FrustumTemplate.h): the picture's border
    // through the lens, a dome for a wide one, a globe for the whole sphere.
    const camhost::FrustumShape& shape = frustum_shape(c.lens);
    std::vector<ImVec2> pts;
    std::vector<uint8_t> ok;
    for (const camhost::FrustumLine& fl : shape.lines) {
        const size_t n = fl.pts.size();
        pts.resize(n);
        ok.resize(n);
        for (size_t i = 0; i < n; i++)
            ok[i] = screen_cv(fl.pts[i].x, fl.pts[i].y, fl.pts[i].z, pts[i]) ? 1 : 0;
        const ImU32 cl = fl.dim ? dim : (ImU32)col;
        const float t = fl.dim ? px(1.0f) : th;
        for (size_t i = 0; i + 1 < n; i++)
            if (ok[i] && ok[i + 1]) line(pts[i], pts[i + 1], cl, t);
        if (fl.closed && n > 1 && ok[n - 1] && ok[0]) line(pts[n - 1], pts[0], cl, t);
    }
    // The apex to each anchor, in pieces so a curved view bends them too.
    constexpr int kA = camhost::kFrustumAnchorSeg;
    for (const camhost::FrustumPoint& p : shape.anchors) {
        ImVec2 last = apex;
        bool have = true;
        for (int j = 1; j <= kA; j++) {
            const float f = (float)j / kA;
            ImVec2 e;
            const bool v = screen_cv(p.x * f, p.y * f, p.z * f, e);
            if (v && have) line(last, e, col, th);
            last = e;
            have = v;
        }
    }
    // Which way is up: a triangle on the top edge, as camera gizmos have.
    if (c.lens.projection != Projection::Equirect && !shape.lines.empty()) {
        const std::vector<camhost::FrustumPoint>& b = shape.lines[0].pts;
        constexpr int N = camhost::kFrustumSeg;
        const camhost::FrustumPoint& m = b[N / 2];
        const float lift = 0.25f * std::sqrt(m.x * m.x + m.y * m.y + m.z * m.z);
        ImVec2 ea, eb, em;
        if (screen_cv(b[N * 3 / 8].x, b[N * 3 / 8].y, b[N * 3 / 8].z, ea) &&
            screen_cv(b[N * 5 / 8].x, b[N * 5 / 8].y, b[N * 5 / 8].z, eb) &&
            screen_cv(m.x, m.y - lift, m.z, em))
            dl->AddTriangleFilled(ea, eb, em, (col & 0x00ffffff) | 0x90000000);
    }
    if (axes) {
        for (int a = 0; a < 3; a++) {
            double w[3];
            world_gl(a == 0 ? 0.7 : 0.0, a == 1 ? 0.7 : 0.0, a == 2 ? 0.7 : 0.0, w);
            ImVec2 e;
            if (!screen_of(w, e)) continue;
            line(apex, e, kAxis[a], px(2.0f));
            const float dx = e.x - apex.x, dy = e.y - apex.y;
            const float l = std::max(std::hypot(dx, dy), 1e-3f);
            const float ux = dx / l, uy = dy / l, hh = px(7.0f), ww = px(3.5f);
            dl->AddTriangleFilled(ImVec2(e.x + ux * hh, e.y + uy * hh),
                                  ImVec2(e.x - uy * ww, e.y + ux * ww),
                                  ImVec2(e.x + uy * ww, e.y - ux * ww), kAxis[a]);
            if (hit) hit->insert(hit->end(), {e.x - ox, e.y - oy, e.x + ux * hh - ox,
                                              e.y + uy * hh - oy});
        }
    }
    dl->AddCircleFilled(apex, px(3.5f), col, 12);
}

double RenderSession::camera_size() const {
    // The viewport's rule for a dataset's cameras (data/FrustumSize.h), over
    // the keys and over the dataset: the keys never come out smaller than
    // the photos, and a wide move gets cameras to match.
    double base = 0.0;
    for (const SourceInfo& s : _sources) {
        const ParsedDataset* ds = s.view.kind == SourceView::Points ? s.view.ds : s.dataset;
        if (!ds || ds->num_cameras <= 0 || ds->c2w.size() < (size_t)ds->num_cameras * 12)
            continue;
        char key[64];
        std::snprintf(key, sizeof key, "%p|%lld", (const void*)ds, (long long)ds->num_cameras);
        if (key != _cam_base_key) {
            _cam_base_key = key;
            _cam_base = camhost::frustum_display_size(ds->c2w.data(), ds->num_cameras);
        }
        base = _cam_base * (s.view.norm_to_world * s.view.file_to_norm).s;
        break;
    }
    // Held while an operator runs, so the cameras do not swell as they move.
    const std::vector<Keyframe>& keys = _project.keys;
    if (!_xform.active() && _key_size_rev != _revision) {
        _key_size_rev = _revision;
        _key_size = 0.0;
        double lo[3] = {1e300, 1e300, 1e300}, hi[3] = {-1e300, -1e300, -1e300};
        std::vector<float> c2w(keys.size() * 12, 0.0f);
        for (size_t i = 0; i < keys.size(); i++)
            for (int d = 0; d < 3; d++) {
                c2w[i * 12 + d * 4 + 3] = (float)keys[i].pos[d];
                lo[d] = std::min(lo[d], keys[i].pos[d]);
                hi[d] = std::max(hi[d], keys[i].pos[d]);
            }
        const double spread = keys.size() >= 2 ? std::max({hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]})
                                               : 0.0;
        if (spread > 1e-6 / std::max(_w2s.s, 1e-12))
            _key_size = camhost::frustum_display_size(c2w.data(), (int64_t)keys.size());
    }
    base = std::max(base, _key_size);
    if (!(base > 0.0)) base = 0.06 / std::max(_w2s.s, 1e-12);
    return base * _cam_size;
}

void RenderSession::draw_viewport_overlay(const ViewportOverlay& v) {
    if (!_have_project || !_panel) return;
    ImDrawList* dl = v.dl;
    // Flying: the view as it is, a red dot and the time, and the way out.
    if (_flying) {
        const ImVec2 c(v.x + px(22.0f), v.y + px(22.0f));
        const bool blink = std::fmod(now_s() - _fly_t0, 1.0) < 0.6;
        dl->AddCircleFilled(c, px(7.0f), blink ? IM_COL32(235, 50, 50, 255) : IM_COL32(120, 30, 30, 255));
        const std::string t = format(msg::fly_recording, {seconds_text(_fly_elapsed)});
        dl->AddText(ImVec2(c.x + px(14.0f), c.y - ImGui::GetFontSize() * 0.5f),
                    IM_COL32(255, 255, 255, 235), t.c_str());
        return;
    }
    ViewProjection vp;
    if (!view_projection(vp, (int)v.w, (int)v.h)) return;
    const Trajectory& tr = trajectory();

    // The path.
    if (_project.keys.size() >= 2) {
        const int n = std::clamp((int)_project.keys.size() * 40, 80, 800);
        const std::vector<double> pts = tr.sample_path(n);
        ImVec2 last;
        bool have = false;
        for (size_t i = 0; i + 2 < pts.size(); i += 3) {
            double s[3];
            to_shared(&pts[i], s);
            const float p[3] = {(float)s[0], (float)s[1], (float)s[2]};
            float x, y, d;
            if (!vp.project(p, x, y, d) || d <= 0.0f) { have = false; continue; }
            const ImVec2 e(v.x + x, v.y + y);
            if (have) dl->AddLine(last, e, kPathCol, px(2.0f));
            last = e;
            have = true;
        }
    }

    // Aimed keys: the point each looks at.
    for (size_t i = 0; i < _project.keys.size(); i++) {
        const Keyframe& k = _project.keys[i];
        if (!k.aim) continue;
        double s[3], c[3];
        to_shared(k.target, s);
        to_shared(k.pos, c);
        const float p[3] = {(float)s[0], (float)s[1], (float)s[2]};
        const float q[3] = {(float)c[0], (float)c[1], (float)c[2]};
        float x, y, d, x2, y2, d2;
        if (!vp.project(p, x, y, d) || d <= 0.0f) continue;
        const ImVec2 t(v.x + x, v.y + y);
        const ImU32 col = selected((int)i) ? kSelCol : IM_COL32(255, 255, 255, 140);
        dl->AddCircle(t, px(6.0f), col, 16, px(1.5f));
        dl->AddLine(ImVec2(t.x - px(9), t.y), ImVec2(t.x + px(9), t.y), col, px(1.0f));
        dl->AddLine(ImVec2(t.x, t.y - px(9)), ImVec2(t.x, t.y + px(9)), col, px(1.0f));
        if (selected((int)i) && vp.project(q, x2, y2, d2) && d2 > 0.0f)
            dl->AddLine(t, ImVec2(v.x + x2, v.y + y2), (col & 0x00ffffff) | 0x60000000, px(1.0f));
    }

    // The keys, then the camera at the playhead on top of them.
    _key_lines.resize(_project.keys.size());
    for (int i = 0; i < (int)_project.keys.size(); i++) {
        const Keyframe& k = _project.keys[(size_t)i];
        CameraState c;
        for (int d = 0; d < 3; d++) c.pos[d] = k.pos[d];
        for (int d = 0; d < 4; d++) c.rot[d] = k.rot[d];
        c.lens = _project.lens_at(i);
        const ImU32 col = selected(i) ? kSelCol : i == _hot ? kHotCol : kKeyCol;
        _key_lines[(size_t)i].clear();
        draw_camera(dl, vp, v.x, v.y, c, col, 1.0f, true, &_key_lines[(size_t)i]);
    }
    if (!_project.keys.empty())
        draw_camera(dl, vp, v.x, v.y, tr.at(_time), kHeadCol, 1.0f, false);

    // An axis of each camera's own, for an operator that moves each in its own.
    if (_xform.active() && _xform.local()) {
        int count = 0;
        for (int i = 0; i < (int)_project.keys.size(); i++) count += selected(i) ? 1 : 0;
        const int a = std::clamp(_xform.axis(), 0, 2);
        const double reach = camera_size() * 3.0;
        for (int i = 0; count > 1 && i < (int)_project.keys.size(); i++) {
            if (!selected(i)) continue;
            const Keyframe& k = _project.keys[(size_t)i];
            double R[9], e[2][3], s[2][3];
            quat_to_matrix3(k.rot, R);
            for (int d = 0; d < 3; d++) {
                e[0][d] = k.pos[d] - R[d*3+a] * reach;
                e[1][d] = k.pos[d] + R[d*3+a] * reach;
            }
            to_shared(e[0], s[0]);
            to_shared(e[1], s[1]);
            const float p0[3] = {(float)s[0][0], (float)s[0][1], (float)s[0][2]};
            const float p1[3] = {(float)s[1][0], (float)s[1][1], (float)s[1][2]};
            float x0, y0, d0, x1, y1, d1;
            if (vp.project(p0, x0, y0, d0) && vp.project(p1, x1, y1, d1) && d0 > 0.0f && d1 > 0.0f)
                dl->AddLine(ImVec2(v.x + x0, v.y + y0), ImVec2(v.x + x1, v.y + y1),
                            (kAxis[a] & 0x00ffffffu) | 0xb0000000u, px(1.5f));
        }
    }

    // The handles of the one selected key, or the operator running on it.
    XformFrame f;
    if (xform_frame(f, _xform.active() ? _xform.kind() : _handle_mode)) {
        const ImVec2 origin(v.x, v.y);
        if (_xform.active()) _xform.draw_overlay(dl, origin, f);
        else if (single_selected() >= 0) _xform.draw_handles(dl, origin, _handle_mode, f, _handle_hot);
    }

    // The camera's picture in the corner -- the other corner when the
    // selected camera is under it. Its inner corner drags to resize it.
    _pip_rect[2] = 0.0f;
    if (_preview_mode == PreviewMode::Corner && !_project.keys.empty() && _frames.texture() &&
        _frames.width() > 0) {
        const float aspect = (float)_frames.height() / (float)_frames.width();
        const float m = px(10.0f);
        float w = std::clamp(v.w * _pip_share, px(120.0f), std::max(px(120.0f), v.w - 2.0f * m));
        if (w * aspect > v.h - 2.0f * m) w = std::max(px(60.0f), (v.h - 2.0f * m) / aspect);
        const float h = w * aspect;
        preview_size(w, h, _preview_w, _preview_h);
        ImVec2 a(v.x + v.w - w - m, v.y + v.h - h - m);
        const int one = single_selected();
        float kx, ky;
        if (!_pip_resizing && one >= 0 && key_screen(one, vp, kx, ky) &&
            v.x + kx > a.x - px(40.0f) && v.y + ky > a.y - px(40.0f))
            a.x = v.x + m;
        const ImVec2 b(a.x + w, a.y + h);
        dl->AddRectFilled(ImVec2(a.x - 2, a.y - 2), ImVec2(b.x + 2, b.y + 2),
                          IM_COL32(0, 0, 0, 200));
        dl->AddImage((ImTextureID)(intptr_t)_frames.texture(), a, b, ImVec2(0, 1), ImVec2(1, 0));
        dl->AddRect(ImVec2(a.x - 2, a.y - 2), ImVec2(b.x + 2, b.y + 2), kHeadCol, 0.0f, 0,
                    px(1.5f));
        // The grip: the corner facing into the pane.
        const bool right = a.x > v.x + v.w * 0.5f;
        const ImVec2 g(right ? a.x - 2 : b.x + 2, a.y - 2);
        const float gs = px(12.0f), dir = right ? 1.0f : -1.0f;
        dl->AddTriangleFilled(g, ImVec2(g.x + dir * gs, g.y), ImVec2(g.x, g.y + gs),
                              _pip_hot || _pip_resizing ? kHotCol : kHeadCol);
        _pip_rect[0] = a.x - v.x;
        _pip_rect[1] = a.y - v.y;
        _pip_rect[2] = w;
        _pip_rect[3] = h;
        _pip_right = right;
        _pip_pane_w = v.w;
    }
}


// ===========================================================================
// Frames
// ===========================================================================

// The frame a 3D transition moves `source` in: the world's up, or the
// camera's -- down the picture for what falls, round its middle for what
// turns or ripples, about the view's own axis at the scene's depth.
bool RenderSession::fx_frame(Transition kind, bool camera, int source, const FrameSpec& f,
                             LayerFx& x) {
    SceneCore core;
    if (source < 0 || !_frames.scene_core(source, core)) return false;
    for (int k = 0; k < 3; k++) x.centre[k] = core.centre[k];
    x.radius = core.radius;
    const bool seen = camera && transition_has_camera(kind);
    FxView view;
    if (seen) {
        const CameraState& cam = f.cam;
        double R[9];
        quat_to_matrix3(cam.rot, R);
        const double right[3] = {R[0], R[3], R[6]}, cup[3] = {R[1], R[4], R[7]},
                     back[3] = {R[2], R[5], R[8]};
        double depth = 0.0;
        for (int k = 0; k < 3; k++) depth -= (core.centre[k] - cam.pos[k]) * back[k];
        depth = std::max(depth, 0.5 * core.radius);
        if (kind == Transition::Spiral || kind == Transition::Ripple) {
            for (int k = 0; k < 3; k++) {
                x.centre[k] = cam.pos[k] - back[k] * depth;
                x.up[k] = kind == Transition::Spiral ? -back[k] : back[k];
                x.e1[k] = right[k];
                x.e2[k] = cup[k];
            }
        } else {
            for (int k = 0; k < 3; k++) {
                x.up[k] = cup[k];
                x.e1[k] = right[k];
                x.e2[k] = back[k];
            }
        }
        // What the picture takes in: only that is ordered, and the moves are
        // as wide as the picture at the scene's depth, so a long lens or a
        // camera up close still sees the change from its start.
        const double hx = 0.5 * std::min(lens_fov(cam.lens), 360.0) * kPi / 180.0;
        const double aspect = (double)std::max(f.height, 1) / std::max(f.width, 1);
        const bool pinhole = cam.lens.projection == Projection::Perspective;
        const double hy = pinhole ? std::atan(std::tan(hx) * aspect) : hx * aspect;
        for (int k = 0; k < 3; k++) {
            view.pos[k] = cam.pos[k];
            view.fwd[k] = -back[k];
            view.right[k] = right[k];
            view.up[k] = cup[k];
        }
        if (pinhole) {
            view.tx = std::tan(hx) * 1.1;
            view.ty = std::tan(hy) * 1.1;
        } else {
            view.cone = std::min(std::max(hx, hy) * 1.1, kPi);
        }
        const double a = std::clamp(std::min(hx, hy), 5.0 * kPi / 180.0, kPi / 3.0);
        x.radius = std::clamp(depth * std::tan(a), 0.05 * core.radius, core.radius);
    } else {
        double n = 0.0;
        for (int k = 0; k < 3; k++) n += _project.up[k] * _project.up[k];
        n = std::sqrt(std::max(n, 1e-24));
        for (int k = 0; k < 3; k++) x.up[k] = _project.up[k] / n;
        // Any two across up, the same two every frame.
        double a[3] = {1, 0, 0};
        if (std::fabs(x.up[0]) > 0.9) { a[0] = 0; a[1] = 1; }
        double e1[3] = {a[1]*x.up[2] - a[2]*x.up[1], a[2]*x.up[0] - a[0]*x.up[2],
                        a[0]*x.up[1] - a[1]*x.up[0]};
        const double n1 = std::sqrt(e1[0]*e1[0] + e1[1]*e1[1] + e1[2]*e1[2]);
        for (int k = 0; k < 3; k++) x.e1[k] = e1[k] / std::max(n1, 1e-12);
        x.e2[0] = x.up[1]*x.e1[2] - x.up[2]*x.e1[1];
        x.e2[1] = x.up[2]*x.e1[0] - x.up[0]*x.e1[2];
        x.e2[2] = x.up[0]*x.e1[1] - x.up[1]*x.e1[0];
    }
    return _frames.scene_quantiles(source, x.centre, x.up, x.e1, x.e2, x.q, seen ? &view : nullptr);
}

namespace {

// A sweep's level when `k` of the elements are behind it, world frame: in
// step with the elements rather than the distance, so a far sky costs no
// more of it than the rest; led in and out past the first and the last.
double sweep_level(const LayerFx& x, double k) {
    const double base = x.centre[0]*x.up[0] + x.centre[1]*x.up[1] + x.centre[2]*x.up[2];
    const double span = std::max((double)(fx_quantile(x.q.qh, 0.98f) - fx_quantile(x.q.qh, 0.02f)),
                                 1e-9 * x.radius);
    constexpr double m = 0.06;
    const double kk = -m + k * (1.0 + 2.0 * m);
    if (kk < 0.0) return base + x.q.qh[0] + kk / m * 0.08 * span;
    if (kk > 1.0) return base + x.q.qh[kFxQuantiles - 1] + (kk - 1.0) / m * 0.08 * span;
    return base + fx_quantile(x.q.qh, (float)kk);
}

void set_sweep(LayerSpec& l, const LayerFx& x, double k, bool keep_below, float glow,
               const float colour[3]) {
    l.clip = keep_below ? 1 : 2;
    for (int d = 0; d < 3; d++) l.clip_n[d] = x.up[d];
    l.level = sweep_level(x, k);
    const double span = fx_quantile(x.q.qh, 0.98f) - fx_quantile(x.q.qh, 0.02f);
    l.glow = (float)(0.07 * std::max(span, 1e-9 * x.radius) * std::clamp(glow, 0.0f, 1.0f));
    for (int d = 0; d < 3; d++) l.glow_col[d] = colour[d];
}

}  // namespace

// One layer's own way in or out, `u` through it, composited on its own
// (mode 3) with the whole change to itself. At the video's start or end
// (`edge`) a dip is from or into its colour.
void RenderSession::side_effect(FrameSpec& f, LayerSpec& l, Transition kind, double u, bool in,
                                const float param[2], const float colour[3], bool camera,
                                bool edge) {
    if (l.source < 0) return;
    const double v = in ? u : 1.0 - u;   // how much of it there is
    // Arriving from nothing, on its way at once rather than easing in.
    const double shown = in ? 1.0 - (1.0 - u) * (1.0 - u) : smooth(v);
    switch (kind) {
        case Transition::Cut:
            if (!in && u >= 1.0) l.source = -1;
            break;
        case Transition::Crossfade:
            l.opacity = (float)shown;
            break;
        case Transition::Dip: {
            for (int k = 0; k < 3; k++) f.tint[0][k] = colour[k];
            // From its colour at the start, into it at the end; elsewhere
            // through it, the model there from the middle on.
            const double amount = edge ? (in ? 1.0 - smooth(u) : smooth(u))
                                : u < 0.5 ? smooth(u * 2.0) : 1.0 - smooth(u * 2.0 - 1.0);
            f.tint[0][3] = (float)std::max((double)f.tint[0][3], amount);
            if (!edge && (in ? u < 0.5 : u >= 0.5)) l.opacity = 0.0f;
            break;
        }
        case Transition::Wipe: {
            const double ang = param[0] * kPi / 180.0;
            l.mask = 1;
            l.mask_t = (float)u;
            l.mask_out = !in;
            l.mask_dir[0] = (float)std::cos(ang);
            l.mask_dir[1] = (float)-std::sin(ang);
            l.mask_soft = std::max(param[1], 0.002f);
            break;
        }
        case Transition::Iris:
            l.mask = 2;
            l.mask_t = (float)(in ? shown : smooth(u));
            l.mask_out = !in;
            l.mask_soft = std::max(param[1], 0.002f);
            break;
        case Transition::Zoom: {
            // Arriving from far and smeared, or rushing past the camera; in
            // sight for all but the far end of it.
            const double z = std::clamp(param[0], 0.05f, 2.0f), r = 1.0 - v;
            l.zoom = (float)(in ? 1.0 / (1.0 + 2.0 * z * r * r) : 1.0 + 2.0 * z * r * r);
            l.zoom_blur = (float)(z * r);
            l.opacity = (float)smooth(std::clamp(v / 0.45, 0.0, 1.0));
            break;
        }
        case Transition::Grow:
            l.grow = (float)(0.02 + 0.98 * smooth(v));
            l.fade_in = (float)std::min(1.0, v * 2.5);
            break;
        default: {
            LayerFx x;
            if (!fx_frame(kind, camera, l.source, f, x)) {
                l.opacity = (float)shown;
                break;
            }
            if (kind == Transition::Sweep) {
                // The level passes it: what is left of it on the far side.
                const bool down = param[0] >= 0.5f;
                set_sweep(l, x, smooth(down ? 1.0 - u : u), in != down, param[1], colour);
                break;
            }
            x.kind = (int)kind;
            x.incoming = in;
            x.t = fx_solo_time((int)kind, in, (float)u, param);
            x.p[0] = param[0];
            x.p[1] = param[1];
            l.fx = x;
            break;
        }
    }
}

FrameSpec RenderSession::frame_spec(double t, int W, int H, bool photo) {
    FrameSpec f;
    f.cam = trajectory().at(t);
    f.width = W;
    f.height = H;
    for (int k = 0; k < 3; k++) f.up[k] = _project.up[k];
    for (int k = 0; k < 3; k++) f.background[k] = _project.background[k];
    f.transparent = _project.output.kind != OutputKind::Video &&
                    _project.output.format == ImageFormat::PngAlpha;

    // Which shot, and how far into its transition.
    std::vector<Shot> shots = _project.shots;
    if (shots.empty()) shots.push_back(Shot{});
    const int n = (int)shots.size();
    // Shots name the project's models; the layers are the open ones. With no
    // shots it is the model on screen throughout.
    auto layer_of = [&](int i) {
        if (i < 0 || i >= n) return -1;
        return _project.shots.empty() ? (_sources.empty() ? -1 : 0) : rt(shots[(size_t)i].source);
    };
    const ShotMix m = shot_mix(shots, t, _project.duration());
    const Shot& sh = shots[(size_t)std::max(m.in, 0)];
    const int B = layer_of(m.in), A = layer_of(m.out);
    f.b.source = B;
    f.mix = 1.0f;
    if (m.own) {
        // Leaving its own way: each layer on its own, over its own time.
        f.mode = 3;
        f.a.source = A;
        if (m.in >= 0)
            side_effect(f, f.b, sh.transition, m.u_in, true, sh.param, sh.colour, sh.camera,
                        m.in == 0);
        if (m.out >= 0) {
            const ShotExit& e = shots[(size_t)m.out].exit;
            side_effect(f, f.a, e.transition, m.u_out, false, e.param, e.colour, e.camera,
                        m.out == n - 1);
        }
    } else if (m.u_in < 1.0 && (A < 0 || B < 0)) {
        // Nothing on one side, from the start or into a gap: the one there
        // has the whole change to itself, arriving or leaving.
        f.mode = 3;
        f.a.source = A;
        if (B >= 0)
            side_effect(f, f.b, sh.transition, m.u_in, true, sh.param, sh.colour, sh.camera,
                        m.in == 0);
        else
            side_effect(f, f.a, sh.transition, m.u_in, false, sh.param, sh.colour, sh.camera,
                        false);
    } else if (m.u_in < 1.0) {
        const double u = m.u_in;
        const float su = (float)smooth(u);
        switch (sh.transition) {
            case Transition::Crossfade:
                f.a.source = A;
                f.mix = su;
                break;
            case Transition::Dip:
                for (int k = 0; k < 3; k++) f.tint[0][k] = sh.colour[k];
                if (u < 0.5) { f.b.source = A; f.tint[0][3] = (float)smooth(u * 2.0); }
                else f.tint[0][3] = (float)(1.0 - smooth(u * 2.0 - 1.0));
                break;
            case Transition::Wipe: {
                // The edge travels this way across the picture, y down.
                const double ang = sh.param[0] * kPi / 180.0;
                f.a.source = A;
                f.mask = 1;
                f.mix = (float)u;
                f.wipe_dir[0] = (float)std::cos(ang);
                f.wipe_dir[1] = (float)-std::sin(ang);
                f.soft = std::max(sh.param[1], 0.002f);
                break;
            }
            case Transition::Iris:
                f.a.source = A;
                f.mask = 2;
                f.mix = su;
                f.soft = std::max(sh.param[1], 0.002f);
                break;
            case Transition::Zoom:
                f.a.source = A;
                f.mode = 2;
                f.mix = (float)u;
                f.zoom = std::clamp(sh.param[0], 0.05f, 2.0f);
                break;
            case Transition::Grow:
                f.a.source = A;
                f.mix = (float)smooth(std::min(1.0, u * 1.5));
                f.b.grow = (float)(0.02 + 0.98 * smooth(u));
                f.b.fade_in = (float)std::min(1.0, u * 2.5);
                break;
            default: {
                // In 3D, both in the incoming model's frame -- the outgoing
                // one's when nothing comes in. Until it is read, a crossfade.
                LayerFx x;
                if (!fx_frame(sh.transition, sh.camera, B >= 0 ? B : A, f, x)) {
                    f.a.source = A;
                    f.mix = su;
                    break;
                }
                f.mode = 1;
                f.a.source = A;
                if (sh.transition == Transition::Sweep) {
                    // A level; the new model on one side of it, the old on
                    // the other, the band at the cut lit.
                    const bool down = sh.param[0] >= 0.5f;
                    const double k = smooth(down ? 1.0 - u : u);
                    set_sweep(f.a, x, k, down, sh.param[1], sh.colour);
                    set_sweep(f.b, x, k, !down, sh.param[1], sh.colour);
                    break;
                }
                for (int layer = 0; layer < 2; layer++) {
                    LayerFx& lx = layer ? f.b.fx : f.a.fx;
                    lx = x;
                    lx.kind = (int)sh.transition;
                    lx.incoming = layer == 1;
                    lx.t = (float)u;
                    lx.p[0] = sh.param[0];
                    lx.p[1] = sh.param[1];
                }
                break;
            }
        }
    }

    // Each model as the keys around this moment have it.
    for (LayerSpec* l : {&f.a, &f.b}) {
        if (l->source < 0) continue;
        _project.look_at(project_source_of(l->source), t, trajectory().key_times(), l->style[0],
                         l->style[1], l->style_mix);
        l->variants = l->style_mix > 0.0f ? 2 : 1;
    }

    // The fades, over everything; a photo has none.
    if (!photo) {
        const double T = _project.duration();
        auto fade = [&](const Fade& fd, double into) {
            if (fd.colour == FadeColour::None || fd.seconds <= 1e-6) return;
            const double amount = 1.0 - smooth(into / fd.seconds);
            if (amount <= 0.0) return;
            const float c = fd.colour == FadeColour::White ? 1.0f : 0.0f;
            if (amount > f.tint[1][3]) {
                f.tint[1][0] = f.tint[1][1] = f.tint[1][2] = c;
                f.tint[1][3] = (float)amount;
            }
        };
        fade(_project.fade_in, t - (_project.keys.empty() ? 0.0 : _project.keys.front().time));
        fade(_project.fade_out, T - t);
    }
    return f;
}

void RenderSession::preview_size(float box_w, float box_h, int& W, int& H) const {
    const double aspect = (double)_project.output.width / std::max(_project.output.height, 1);
    double w = std::max(64.0f, box_w), h = w / aspect;
    if (h > box_h && box_h > 32.0f) { h = box_h; w = h * aspect; }
    // Never more than the output, and never more than the scale asks for.
    const double cap = std::max(0.1, (double)_preview_scale) * _project.output.width;
    if (w > cap) { w = cap; h = w / aspect; }
    W = std::max(32, (int)std::lround(w));
    H = std::max(18, (int)std::lround(h));
}

void RenderSession::request_preview() {
    if (!_have_project || _project.keys.empty() || exporting() || _frames.busy()) return;
    if (_frames.source_count() == 0) return;
    int W = _preview_w, H = _preview_h;
    if (W <= 0 || H <= 0) preview_size(px(420.0f), px(240.0f), W, H);
    char key[96];
    std::snprintf(key, sizeof key, "%llu|%.6f|%dx%d", (unsigned long long)_revision,
                  _time, W, H);
    if (key == _preview_key) return;
    _preview_key = key;
    const bool photo = _project.output.kind == OutputKind::Photo;
    _frames.request(frame_spec(_time, W, H, photo));
}


// ===========================================================================
// Once a frame
// ===========================================================================

void RenderSession::poll() {
    if (!_panel) return;
    if (!_have_project) new_project();
    if (_fly_block_key && !ImGui::IsKeyDown((ImGuiKey)_fly_block_key)) _fly_block_key = 0;
    _sel.resize(_project.keys.size(), 0);

    // A click on the pane or the panel stops playback before it lands, so
    // it hits what was under the pointer.
    if (_playing && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
                     ImGui::IsMouseClicked(ImGuiMouseButton_Right))) {
        const ImVec2 m = ImGui::GetIO().MousePos;
        float ix, iy, iw, ih;
        _panel->image_rect(ix, iy, iw, ih);
        const float* r = _panel_rect;
        if ((m.x >= ix && m.y >= iy && m.x < ix + iw && m.y < iy + ih) ||
            (m.x >= r[0] && m.y >= r[1] && m.x < r[0] + r[2] && m.y < r[1] + r[3]))
            _playing = false;
    }
    if (_playing) {
        const double T = _project.duration();
        const double first = _project.keys.empty() ? 0.0 : _project.keys.front().time;
        _time = _play_from + (now_s() - _play_clock);
        if (_time >= T) {
            if (_loop && T > first) {
                _play_from = first;
                _play_clock = now_s();
                _time = first;
            } else {
                _time = T;
                _playing = false;
            }
        }
    }

    if (_flying) {
        FlightSample fs;
        fs.t = _fly_elapsed = now_s() - _fly_t0;
        double target[3];
        if (view_pose(fs.pos, fs.rot, target)) _flight.push_back(fs);
    }

    if (_pick_waiting) {
        float p[3];
        bool hit = false;
        if (_panel->take_pick(p, hit)) {
            _pick_waiting = false;
            if (hit) {
                double s[3] = {p[0], p[1], p[2]}, w[3];
                _w2s.inverse().apply(s, w);
                bool any = false;
                for (uint8_t v : _sel) any = any || v;
                for (int i = 0; i < (int)_project.keys.size(); i++) {
                    if (any && !selected(i)) continue;
                    Keyframe& k = _project.keys[(size_t)i];
                    k.aim = true;
                    for (int d = 0; d < 3; d++) k.target[d] = w[d];
                    k.roll = 0.0;
                    update_aim(k, _project.up);
                }
                project_changed();
            } else {
                note(msg::pick_missed.get());
            }
        }
    }

    if (_job.state != Job::Idle) poll_export();
    else {
        _frames.poll(0.0);
        request_preview();
    }
    const std::string err = _frames.take_error();
    if (!err.empty()) {
        _status = format(msg::render_error, {err});
        _status_err = true;
    }
    commit_history();
}


// ===========================================================================
// Export
// ===========================================================================

// Asks `spirula encode --probe` which codecs this GPU really encodes, and
// how large -- it encodes two frames with each, since a device can list a
// codec and still refuse a session -- and ffmpeg which encoders it has.
void RenderSession::probe_encoder() {
    _encoder_probe = 1;
    _ffmpeg_probe = 1;
    const std::string ffmpeg = _ffmpeg;
    _probe = std::thread([this, ffmpeg] {
        std::atomic<bool> cancel{false};
        std::vector<std::string> lines;
        if (command_exists(ffmpeg))
            run_process({ffmpeg, "-hide_banner", "-encoders"}, "",
                        [&](const std::string& line) { lines.push_back(line); }, cancel);
        _ffmpeg_encoders = parse_ffmpeg_encoders(lines);
        _ffmpeg_probe = 2;
#ifdef SS_TOOL_ENCODE
        int codecs = 0;
        const int code = run_process({app::exe_path(), "encode", "--probe"}, "",
                                     [&](const std::string& line) {
            // "h264 4096 4096": a codec and the largest frame it takes.
            char name[16] = {};
            int w = 0, h = 0;
            if (std::sscanf(line.c_str(), "%15s %d %d", name, &w, &h) < 1) return;
            const int c = std::string(name) == "h264" ? 0 : std::string(name) == "h265" ? 1
                        : std::string(name) == "av1" ? 2 : -1;
            if (c < 0) return;
            codecs |= 1 << c;
            _encoder_max[c][0] = w > 0 ? w : 4096;
            _encoder_max[c][1] = h > 0 ? h : 4096;
        }, cancel);
        _encoder_codecs = codecs;
        _encoder_probe = code == 0 && codecs ? 2 : 3;
#else
        _encoder_probe = 3;
#endif
    });
}

bool RenderSession::builtin_encodes(Codec codec, int width, int height) const {
    // WebM is ffmpeg's; the GPU encoder writes MP4 only.
    if (codec == Codec::Gif || codec == Codec::Av1Webm || _encoder_probe.load() != 2)
        return false;
    const int c = (int)codec;
    return (_encoder_codecs.load() & (1 << c)) && width <= _encoder_max[c][0] &&
           height <= _encoder_max[c][1];
}

Encoder RenderSession::pick_encoder() {
    Encoder e;
    const Output& o = _project.output;
    int W = std::max(16, o.width), H = std::max(16, o.height);
    W += W & 1;
    H += H & 1;
    const bool gpu = !_fallback_tried && builtin_encodes(o.codec, W, H);
    Encoder ff;
    if (command_exists(_ffmpeg)) {
        const std::vector<std::string>& want = ffmpeg_encoders_for(o.codec);
        // Until ffmpeg has said what it has, the usual first choice.
        if (_ffmpeg_probe.load() != 2) {
            if (!want.empty()) ff = {Encoder::Ffmpeg, _ffmpeg, want[0]};
        } else {
            for (const std::string& name : want)
                if (std::find(_ffmpeg_encoders.begin(), _ffmpeg_encoders.end(), name) !=
                    _ffmpeg_encoders.end()) {
                    ff = {Encoder::Ffmpeg, _ffmpeg, name};
                    break;
                }
        }
    }
    // The GPU for H.264 and H.265. Its AV1 predicts only from key frames
    // (src/video/README.md), so an AV1 encoder in ffmpeg makes the smaller file.
    const bool gpu_first = o.codec != Codec::Av1 || ff.kind == Encoder::None;
    if (gpu && gpu_first) {
        e.kind = Encoder::BuiltIn;
        e.exe = app::exe_path();
        return e;
    }
    if (ff.kind != Encoder::None) return ff;
    if (gpu) {
        e.kind = Encoder::BuiltIn;
        e.exe = app::exe_path();
    }
    return e;
}

int RenderSession::frame_count() const {
    if (_project.output.kind == OutputKind::Photo || _project.keys.empty()) return 1;
    const double span = std::max(0.0, _project.duration() - _project.keys.front().time);
    const long n = std::lround(span * std::max(_project.output.fps, 1.0));
    // A loop's last frame would be its first again.
    return (int)std::max(1L, _project.looped() ? n : n + 1);
}

void RenderSession::start_export(bool no_builtin, bool confirmed) {
    if (_job.state != Job::Idle || _project.keys.empty()) return;
    Output& o = _project.output;
    const bool photo = o.kind == OutputKind::Photo;
    if (o.path.empty()) {
        _export_after_pick = _pick != nullptr;
        if (_pick) _pick(Pick::Output, default_project_dir(_sources.empty() ? "" : _sources[0].path),
                         suggested_output_name());
        return;
    }
    const double T = _project.duration();
    if (!photo && T <= _project.keys.front().time) {
        note(msg::need_two_keys.get());
        _status_err = true;
        return;
    }
    // Something already there: asked first, and started again on a yes.
    if (!confirmed) {
        std::error_code ec;
        const fs::path out = fs::u8path(o.path);
        _overwrite_frames = 0;
        if (o.kind == OutputKind::Frames) {
            if (fs::is_directory(out, ec))
                for (const auto& e : fs::directory_iterator(out, ec)) {
                    const std::string name = e.path().filename().string();
                    const std::string ext = e.path().extension().string();
                    if (name.rfind("frame_", 0) == 0 && (ext == ".png" || ext == ".jpg"))
                        _overwrite_frames++;
                }
            _ask_overwrite = _overwrite_frames > 0;
        } else {
            _ask_overwrite = fs::exists(out, ec);
        }
        if (_ask_overwrite) return;
    }
    _playing = false;
    if (_job.finisher.joinable()) _job.finisher.join();
    _job.reset();
    // A preview frame still in flight is not one of the export's.
    _frames.abandon();
    _fallback_tried = no_builtin;
    int W = std::max(16, o.width), H = std::max(16, o.height);
    const bool mp4 = o.kind == OutputKind::Video && o.codec != Codec::Gif;
    if (mp4) { W += W & 1; H += H & 1; }
    _job.photo = photo;
    _job.frames = frame_count();
    _job.path = o.path;
    if (mp4) {
        std::error_code ec;
        const fs::path parent = fs::u8path(o.path).parent_path();
        if (!parent.empty()) fs::create_directories(parent, ec);
        const Encoder e = pick_encoder();
        if (e.kind == Encoder::None) {
            _status = msg::no_encoder.get();
            _status_err = true;
            return;
        }
        _job.builtin = e.kind == Encoder::BuiltIn;
        const bool sphere = !_project.keys.empty() &&
                            _project.lens_at(0).projection == Projection::Equirect;
        _job.sink = open_pipe_sink(encoder_argv(e, W, H, o.fps, o.codec, o.quality, sphere,
                                                o.path), W, H);
    } else if (o.kind == OutputKind::Video) {
        _job.sink = open_gif_sink(o.path, W, H, o.fps, _job.frames, o.quality);
    } else {
        std::string path = o.path;
        if (o.kind == OutputKind::Frames) {
            // A folder: the frames are numbered inside it.
            path = (fs::u8path(o.path) /
                    (o.format == ImageFormat::Jpeg ? "frame_%05d.jpg" : "frame_%05d.png")).string();
        }
        _job.sink = open_image_sink(path, W, H, o.format, o.jpeg_quality);
    }
    _job.state = Job::Preparing;
    _job.started = now_s();
    _status.clear();
    _status_err = false;
    _status_done = false;
}

void RenderSession::cancel_export() {
    if (_job.state == Job::Idle) return;
    if (_job.finisher.joinable()) _job.finisher.join();
    if (_job.sink) _job.sink->cancel();
    if (!_job.photo && _project.output.kind == OutputKind::Video) {
        std::error_code ec;
        fs::remove(fs::u8path(_job.path), ec);
    }
    _job.sink.reset();
    _job.state = Job::Idle;
    _frames.abandon();
    _preview_key.clear();
    note(msg::render_cancelled.get());
}

void RenderSession::poll_export() {
    Job& j = _job;
    const Output& o = _project.output;
    int W = std::max(16, o.width), H = std::max(16, o.height);
    if (o.kind == OutputKind::Video && o.codec != Codec::Gif) { W += W & 1; H += H & 1; }
    if (j.state == Job::Preparing) {
        // Effects that rewrite splats need them read first.
        bool ready = true;
        for (int i = 0; i < _frames.source_count(); i++) ready = _frames.effects_ready(i) && ready;
        if (_frames.busy()) {
            _frames.poll(0.0);
            ready = false;
        }
        if (!ready) return;
        j.state = Job::Rendering;
        j.frame = 0;
    }
    if (j.state == Job::Rendering) {
        // A slice of each window frame, so the window keeps drawing: the
        // renders and the encoder work on their own threads meanwhile.
        const double budget = now_s() + 0.012;
        const double first = _project.keys.front().time;
        auto time_of = [&](int frame) {
            return j.photo ? _time
                           : std::min(first + frame / std::max(o.fps, 1.0), _project.duration());
        };
        do {
            if (!j.unread) {
                if (!_frames.busy() && !j.queued) _frames.request(frame_spec(time_of(j.frame), W, H, j.photo));
                j.queued = false;
                if (!_frames.poll(std::clamp(budget - now_s(), 0.0, 0.004))) continue;
                // The next one goes to the renderers before this one is read
                // back and written, and while the window draws: they overlap.
                if (j.frame + 1 < j.frames) {
                    _frames.request(frame_spec(time_of(j.frame + 1), W, H, j.photo));
                    j.queued = true;
                }
                j.unread = true;
            }
            // An encoder behind: the frame waits in the renderer, not the window.
            if (j.sink->full()) break;
            std::vector<uint8_t> px;
            _frames.read(px, j.sink->channels() == 4);
            j.unread = false;
            if (!j.sink->push(std::move(px))) {
                j.ok = false;
                j.error = j.sink->error();
                j.frame = j.frames;
            } else {
                j.frame++;
            }
            if (j.frame >= j.frames) {
                j.state = Job::Finishing;
                j.finished = false;
                FrameSink* sink = j.sink.get();
                Job* jp = &j;
                j.finisher = std::thread([sink, jp] {
                    const bool ok = sink->finish();
                    if (!ok) {
                        jp->ok = false;
                        if (jp->error.empty()) jp->error = sink->error();
                    }
                    jp->finished = true;
                });
                break;
            }
        } while (now_s() < budget);
    }
    if (j.state == Job::Finishing && j.finished.load()) {
        j.finisher.join();
        j.sink.reset();
        j.state = Job::Idle;
        _preview_key.clear();
        if (!j.ok && j.builtin && command_exists(_ffmpeg)) {
            // The GPU would not do it after all: ffmpeg, from the first frame.
            note(format(msg::encoder_fallback, {j.error.substr(0, j.error.find('\n'))}));
            start_export(true, true);
            return;
        }
        if (j.ok) {
            note(format(msg::render_saved, {j.path}), true);
        } else {
            _status = format(msg::render_failed, {j.error});
            _status_err = true;
            _log.push_back(_status);
        }
    }
}

}  // namespace gui::render

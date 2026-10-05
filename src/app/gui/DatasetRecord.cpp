#include "app/gui/DatasetRecord.h"

#include "data/JsonWrite.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <random>

namespace fs = std::filesystem;

namespace gui {

namespace {

const char* const kStepNames[kNumSteps] = {"frames", "masks", "model", "geometry"};

std::mutex& record_mutex() {
    static std::mutex mu;
    return mu;
}

JsonValue read_root(const std::string& workspace) {
    std::error_code ec;
    const fs::path file = fs::path(workspace) / kDatasetRecordFile;
    if (workspace.empty() || !fs::is_regular_file(file, ec)) return {};
    try {
        JsonValue v = json_parse_file(file.string());
        if (v.is_object()) return v;
    } catch (const std::exception&) {
    }
    return {};
}

// Through a temporary and a rename, so a run killed mid-write leaves the old
// record rather than half of a new one.
void write_root(const std::string& workspace, const JsonValue& root) {
    std::error_code ec;
    const fs::path file = fs::path(workspace) / kDatasetRecordFile;
    fs::create_directories(file.parent_path(), ec);
    JsonWriter w;
    json_write(w, root);
    const fs::path tmp = file.string() + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return;
        f << w.str();
        if (!f) return;
    }
    fs::rename(tmp, file, ec);
    if (ec) fs::remove(tmp, ec);
}

JsonValue str(const std::string& s) {
    JsonValue v;
    v.type = JsonValue::Type::String;
    v.str = s;
    return v;
}

JsonValue num(double d) {
    JsonValue v;
    v.type = JsonValue::Type::Number;
    v.num = d;
    return v;
}

JsonValue boolean(bool b) {
    JsonValue v;
    v.type = JsonValue::Type::Bool;
    v.b = b;
    return v;
}

JsonValue array() {
    JsonValue v;
    v.type = JsonValue::Type::Array;
    return v;
}

JsonValue object() {
    JsonValue v;
    v.type = JsonValue::Type::Object;
    return v;
}

void set(JsonValue& obj, const std::string& key, JsonValue v) {
    for (auto& [k, old] : obj.obj)
        if (k == key) {
            old = std::move(v);
            return;
        }
    obj.obj.emplace_back(key, std::move(v));
}

std::string text(const JsonValue& obj, const char* key) {
    const JsonValue* v = obj.find(key);
    return v ? v->as_string() : std::string();
}

// The step a key of DatasetPreset.cpp's field table feeds, by its spelling.
Step settings_step(const std::string& key) {
    auto starts = [&](const char* p) { return key.rfind(p, 0) == 0; };
    if (starts("mask_") || key == "use_found_masks" || key == "flip_found_masks")
        return Step::Masks;
    if (starts("geometry_")) return Step::Geometry;
    if (starts("sfm_") || starts("colmap_") || starts("image_") || key == "engine_colmap" ||
        key == "point_color_in_image_space")
        return Step::Model;
    return Step::Frames;
}

StepRecord decode_step(const JsonValue& v) {
    StepRecord r;
    if (!v.is_object()) return r;
    r.present = true;
    r.id = text(v, "id");
    r.frames_id = text(v, "frames_id");
    r.masks_id = text(v, "masks_id");
    r.model_id = text(v, "model_id");
    r.complete = v.find("complete") && v.find("complete")->as_bool();
    if (const JsonValue* f = v.find("fields"); f && f->is_array())
        for (const JsonValue& e : f->arr)
            if (e.is_array() && e.arr.size() == 3)
                r.fields.push_back(
                    {e.arr[0].as_string(), e.arr[1].as_string(), e.arr[2].as_string()});
    if (const JsonValue* m = v.find("made"); m && m->is_array())
        for (const JsonValue& e : m->arr) r.made.push_back(e.as_string());
    return r;
}

JsonValue encode_step(const StepRecord& r) {
    JsonValue v = object();
    set(v, "id", str(r.id));
    if (!r.frames_id.empty()) set(v, "frames_id", str(r.frames_id));
    if (!r.masks_id.empty()) set(v, "masks_id", str(r.masks_id));
    if (!r.model_id.empty()) set(v, "model_id", str(r.model_id));
    set(v, "complete", boolean(r.complete));
    JsonValue fields = array();
    for (const StepField& f : r.fields) {
        JsonValue e = array();
        e.arr = {str(f.key), str(f.scope), str(f.value)};
        fields.arr.push_back(std::move(e));
    }
    set(v, "fields", std::move(fields));
    if (!r.made.empty()) {
        JsonValue made = array();
        for (const std::string& m : r.made) made.arr.push_back(str(m));
        set(v, "made", std::move(made));
    }
    return v;
}

}  // namespace

DatasetRecord read_dataset_record(const std::string& workspace) {
    DatasetRecord rec;
    const JsonValue root = read_root(workspace);
    if (!root.is_object()) return rec;
    rec.present = true;
    if (const JsonValue* s = root.find("settings")) rec.settings = *s;
    if (const JsonValue* i = root.find("inputs")) rec.inputs = *i;
    if (const JsonValue* steps = root.find("steps"))
        for (int k = 0; k < kNumSteps; k++)
            if (const JsonValue* s = steps->find(kStepNames[k]))
                rec.steps[k] = decode_step(*s);
    const JsonValue* by_step = root.find("step_settings");
    const JsonValue* inputs_by_step = root.find("step_inputs");
    for (int k = 0; k < kNumSteps; k++)
        if (const JsonValue* i = inputs_by_step ? inputs_by_step->find(kStepNames[k]) : nullptr)
            rec.step_inputs[k] = *i;
    if (by_step && rec.settings.is_object())
        for (auto& [key, value] : rec.settings.obj) {
            const JsonValue* made = by_step->find(kStepNames[(int)settings_step(key)]);
            if (const JsonValue* v = made ? made->find(key) : nullptr) value = *v;
        }
    return rec;
}

void write_record_settings(const std::string& workspace,
                           const std::string& settings_json,
                           const std::string& inputs_json,
                           const bool makes[kNumSteps]) {
    if (workspace.empty()) return;
    std::lock_guard<std::mutex> lk(record_mutex());
    JsonValue root = read_root(workspace);
    if (!root.is_object()) root = object();
    set(root, "version", num(1));
    JsonValue settings, inputs;
    try {
        settings = json_parse(settings_json);
        inputs = json_parse(inputs_json);
    } catch (const std::exception&) {
        return;
    }
    for (const char* slot : {"step_settings", "step_inputs"}) {
        JsonValue by_step = object();
        if (const JsonValue* s = root.find(slot); s && s->is_object()) by_step = *s;
        for (int k = 0; k < kNumSteps; k++)
            if (makes[k])
                set(by_step, kStepNames[k],
                    std::string(slot) == "step_settings" ? settings : inputs);
        set(root, slot, std::move(by_step));
    }
    set(root, "settings", std::move(settings));
    set(root, "inputs", std::move(inputs));
    write_root(workspace, root);
}

std::string new_step_id() {
    static std::mutex mu;
    static std::mt19937_64 rng(std::random_device{}() ^
                               (uint64_t)std::chrono::steady_clock::now()
                                   .time_since_epoch()
                                   .count());
    std::lock_guard<std::mutex> lk(mu);
    char buf[24];
    std::snprintf(buf, sizeof buf, "%016llx", (unsigned long long)rng());
    return buf;
}

void write_step_record(const std::string& workspace, Step step,
                       const StepRecord& rec) {
    if (workspace.empty()) return;
    std::lock_guard<std::mutex> lk(record_mutex());
    JsonValue root = read_root(workspace);
    if (!root.is_object()) root = object();
    set(root, "version", num(1));
    JsonValue steps = object();
    if (const JsonValue* s = root.find("steps"); s && s->is_object()) steps = *s;
    set(steps, kStepNames[(int)step], encode_step(rec));
    set(root, "steps", std::move(steps));
    write_root(workspace, root);
}

std::string encode_record_inputs(const RecordInputs& in) {
    JsonWriter w;
    w.object();
    w.key("rows").array();
    for (const PrepInput& s : in.rows) {
        w.object();
        w.field("path", s.path);
        w.field("is_video", s.is_video);
        w.field("subdir", s.subdir);
        w.field("fps", s.fps);
        w.field("sequential", s.sequential);
        w.field("camera_model", s.camera_model);
        w.field("focal_factor", s.focal_factor);
        w.field("rig", s.rig);
        w.field("rig_dual_fisheye", s.rig_dual_fisheye);
        w.key("subcameras").array();
        for (const SubCamera& sc : s.subcameras) {
            w.object();
            w.field("rel", sc.rel);
            w.field("camera_model", sc.camera_model);
            w.field("focal_factor", sc.focal_factor);
            w.field("rig", sc.rig);
            w.field("rig_dual_fisheye", sc.rig_dual_fisheye);
            w.end();
        }
        w.end();
        w.end();
    }
    w.end();
    w.key("clicks").array();
    for (const MaskClick& c : in.clicks) {
        w.object();
        w.field("x", c.x);
        w.field("y", c.y);
        w.field("positive", c.positive);
        w.field("object", c.object);
        w.field("frame", (long long)c.frame);
        w.field("position", c.position);
        w.field("source", c.source);
        w.field("camera", c.camera);
        w.end();
    }
    w.end();
    w.end();
    return w.str();
}

RecordInputs decode_record_inputs(const JsonValue& v) {
    RecordInputs out;
    if (const JsonValue* rows = v.find("rows"); rows && rows->is_array())
        for (const JsonValue& r : rows->arr) {
            PrepInput s;
            s.path = text(r, "path");
            s.is_video = r.find("is_video") && r.find("is_video")->as_bool();
            s.subdir = text(r, "subdir");
            s.fps = (float)r.get_double("fps", 0.0);
            s.sequential = r.find("sequential") && r.find("sequential")->as_bool();
            s.camera_model = text(r, "camera_model");
            s.focal_factor = (float)r.get_double("focal_factor", 0.0);
            s.rig = (int)r.get_double("rig", 0.0);
            s.rig_dual_fisheye =
                r.find("rig_dual_fisheye") && r.find("rig_dual_fisheye")->as_bool();
            if (const JsonValue* subs = r.find("subcameras"); subs && subs->is_array())
                for (const JsonValue& c : subs->arr) {
                    SubCamera sc;
                    sc.rel = text(c, "rel");
                    sc.camera_model = text(c, "camera_model");
                    sc.focal_factor = (float)c.get_double("focal_factor", 0.0);
                    sc.rig = (int)c.get_double("rig", 0.0);
                    sc.rig_dual_fisheye = c.find("rig_dual_fisheye") &&
                                          c.find("rig_dual_fisheye")->as_bool();
                    s.subcameras.push_back(std::move(sc));
                }
            out.rows.push_back(std::move(s));
        }
    if (const JsonValue* clicks = v.find("clicks"); clicks && clicks->is_array())
        for (const JsonValue& c : clicks->arr) {
            MaskClick k;
            k.x = (float)c.get_double("x", 0.0);
            k.y = (float)c.get_double("y", 0.0);
            k.positive = !c.find("positive") || c.find("positive")->as_bool(true);
            k.object = (int)c.get_double("object", 0.0);
            k.frame = (long long)c.get_double("frame", 0.0);
            k.position = (float)c.get_double("position", 0.0);
            k.source = text(c, "source");
            k.camera = text(c, "camera");
            out.clicks.push_back(std::move(k));
        }
    return out;
}

std::vector<std::string> read_legacy_stamp(const std::string& workspace,
                                           const char* file) {
    std::vector<std::string> args;
    if (workspace.empty()) return args;
    std::ifstream f(fs::path(workspace) / file, std::ios::binary);
    if (!f) return args;
    auto unescape = [](const std::string& s) {
        std::string out;
        for (size_t i = 0; i < s.size(); i++) {
            if (s[i] != '\\' || i + 1 >= s.size()) {
                out += s[i];
                continue;
            }
            const char c = s[++i];
            out += c == 'n' ? '\n' : c == 'r' ? '\r' : c;
        }
        return out;
    };
    for (std::string s; std::getline(f, s);) {
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        args.push_back(unescape(s));
    }
    return args;
}

}  // namespace gui

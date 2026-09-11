// Manifest.cpp -- see Manifest.h.

#include "sfm/core/Manifest.h"

#include "data/Yaml.h"
#include "sfm/core/CameraSetup.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace sfm {

namespace {

[[noreturn]] void bad(const std::string& path, const std::string& why) {
    throw std::runtime_error(path + ": " + why);
}

const JsonValue* want_object(const JsonValue& v, const std::string& path) {
    if (!v.is_object()) bad(path, "a manifest is a mapping of keys to values");
    return &v;
}

std::string str_of(const JsonValue& v, const std::string& path, const char* key) {
    if (v.type == JsonValue::Type::Null) return {};
    if (v.type != JsonValue::Type::String)
        bad(path, std::string(key) + ": expected text");
    return v.str;
}

// Relative to the manifest, so a capture and the file describing it move
// together; absolute paths are left alone.
std::string resolve(const std::string& p, const fs::path& base) {
    if (p.empty()) return p;
    const fs::path f(p);
    return f.is_absolute() ? p : (base / f).lexically_normal().string();
}

}  // namespace

Manifest manifest_read(const std::string& path) {
    JsonValue root;
    try {
        root = yaml_parse_file(path);
    } catch (const std::exception& e) {
        bad(path, e.what());
    }
    want_object(root, path);

    Manifest m;
    m.base_dir = fs::path(path).parent_path().string();
    if (const JsonValue* v = root.find("image_dir"))
        m.image_dir = str_of(*v, path, "image_dir");
    if (const JsonValue* v = root.find("mask_dir"))
        m.mask_dir = str_of(*v, path, "mask_dir");
    if (const JsonValue* v = root.find("mask_flipped")) {
        if (v->type != JsonValue::Type::Bool) bad(path, "mask_flipped: expected true or false");
        m.mask_flipped = v->b;
        m.has_mask_flipped = true;
    }
    if (const JsonValue* v = root.find("camera_mode")) {
        m.camera_mode = str_of(*v, path, "camera_mode");
        CameraMode parsed;
        if (!m.camera_mode.empty() && !parseCameraMode(m.camera_mode, parsed))
            bad(path, "camera_mode: expected single, folder or image");
    }
    if (const JsonValue* v = root.find("image_gamut"))
        m.image_gamut = str_of(*v, path, "image_gamut");
    if (const JsonValue* v = root.find("image_linear")) {
        if (v->type != JsonValue::Type::Bool) bad(path, "image_linear: expected true or false");
        m.image_linear = v->b ? 1 : 0;
    }

    if (const JsonValue* cams = root.find("cameras")) {
        if (!cams->is_array()) bad(path, "cameras: expected a list");
        for (const JsonValue& c : cams->arr) {
            if (!c.is_object()) bad(path, "cameras: each entry is a mapping");
            ManifestCamera mc;
            if (const JsonValue* v = c.find("prefix")) mc.prefix = str_of(*v, path, "prefix");
            if (const JsonValue* v = c.find("model")) {
                mc.model = str_of(*v, path, "model");
                CamModel parsed;
                if (!mc.model.empty() && !parseCamModelName(mc.model, parsed))
                    bad(path, "cameras." + mc.prefix + ".model: unknown camera model '" +
                                  mc.model + "'");
            }
            if (const JsonValue* v = c.find("focal")) {
                if (v->type != JsonValue::Type::Number) bad(path, "focal: expected a number");
                mc.focal = v->num;
            }
            if (const JsonValue* v = c.find("distortion")) {
                if (!v->is_array()) bad(path, "distortion: expected a list of numbers");
                for (const JsonValue& d : v->arr) {
                    if (d.type != JsonValue::Type::Number)
                        bad(path, "distortion: expected a list of numbers");
                    mc.distortion.push_back(d.num);
                }
            }
            m.cameras.push_back(std::move(mc));
        }
    }
    if (const JsonValue* rigs = root.find("rigs")) {
        if (!rigs->is_array()) bad(path, "rigs: expected a list");
        for (const JsonValue& r : rigs->arr) {
            if (!r.is_object()) bad(path, "rigs: each entry is a mapping");
            RigDef d;
            if (const JsonValue* v = r.find("name")) d.name = str_of(*v, path, "name");
            if (const JsonValue* v = r.find("captures")) {
                if (!v->is_array()) bad(path, "rigs: captures: expected a list of prefixes");
                for (const JsonValue& c : v->arr) d.captures.push_back(str_of(c, path, "captures"));
            }
            const JsonValue* mem = r.find("members");
            if (!mem || !mem->is_array()) bad(path, "rigs: each entry lists its members");
            for (const JsonValue& m : mem->arr) {
                RigMemberDef md;
                if (m.type == JsonValue::Type::String) {
                    md.prefix = m.str;
                } else if (m.is_object()) {
                    if (const JsonValue* v = m.find("prefix")) md.prefix = str_of(*v, path, "prefix");
                    auto numbers = [&](const JsonValue& v, size_t n, const char* key,
                                       double* out) {
                        if (!v.is_array() || v.arr.size() != n)
                            bad(path, std::string("rigs: ") + key + ": expected " +
                                          std::to_string(n) + " numbers");
                        for (size_t k = 0; k < n; k++) {
                            if (v.arr[k].type != JsonValue::Type::Number)
                                bad(path, std::string("rigs: ") + key + ": expected numbers");
                            out[k] = v.arr[k].num;
                        }
                    };
                    if (const JsonValue* v = m.find("rotation")) {
                        double q[4];
                        numbers(*v, 4, "rotation", q);
                        md.ext.R = quaternionToRotation({q[0], q[1], q[2], q[3]});
                        md.has_ext = true;
                    }
                    if (const JsonValue* v = m.find("translation")) {
                        double t[3];
                        numbers(*v, 3, "translation", t);
                        md.ext.t = {t[0], t[1], t[2]};
                        md.has_ext = true;
                    }
                    if (const JsonValue* v = m.find("fixed")) {
                        if (v->type != JsonValue::Type::Bool) bad(path, "rigs: fixed: expected true or false");
                        md.ext_fixed = v->b;
                    }
                } else {
                    bad(path, "rigs: a member is a prefix or a mapping");
                }
                while (!md.prefix.empty() && (md.prefix.back() == '/' || md.prefix.back() == '\\'))
                    md.prefix.pop_back();
                if (md.prefix.empty()) bad(path, "rigs: a member needs a prefix");
                d.members.push_back(std::move(md));
            }
            if (d.members.size() < 2) bad(path, "rigs: a rig needs at least two members");
            m.rigs.push_back(std::move(d));
        }
    }
    if (const JsonValue* caps = root.find("captures")) {
        if (!caps->is_array()) bad(path, "captures: expected a list");
        for (const JsonValue& c : caps->arr) {
            if (!c.is_object()) bad(path, "captures: each entry is a mapping");
            ManifestCapture mc;
            if (const JsonValue* v = c.find("prefix")) mc.prefix = str_of(*v, path, "prefix");
            if (const JsonValue* v = c.find("telemetry")) mc.telemetry = str_of(*v, path, "telemetry");
            if (mc.telemetry.empty()) bad(path, "captures: each entry names its telemetry file");
            if (const JsonValue* v = c.find("fps")) {
                if (v->type != JsonValue::Type::Number || v->num < 0) bad(path, "fps: expected a number");
                mc.fps = v->num;
            }
            if (const JsonValue* v = c.find("time_offset")) {
                if (v->type != JsonValue::Type::Number) bad(path, "time_offset: expected a number");
                mc.time_offset = v->num;
            }
            m.captures.push_back(std::move(mc));
        }
    }
    return m;
}

std::string manifest_write(const Manifest& m, bool json) {
    auto text = [](const std::string& s) {
        JsonValue v;
        v.type = JsonValue::Type::String;
        v.str = s;
        return v;
    };
    auto number = [](double d) {
        JsonValue v;
        v.type = JsonValue::Type::Number;
        v.num = d;
        return v;
    };
    auto boolean = [](bool b) {
        JsonValue v;
        v.type = JsonValue::Type::Bool;
        v.b = b;
        return v;
    };

    JsonValue root;
    root.type = JsonValue::Type::Object;
    if (!m.image_dir.empty()) root.obj.emplace_back("image_dir", text(m.image_dir));
    if (!m.mask_dir.empty()) root.obj.emplace_back("mask_dir", text(m.mask_dir));
    if (m.has_mask_flipped) root.obj.emplace_back("mask_flipped", boolean(m.mask_flipped));
    if (!m.camera_mode.empty()) root.obj.emplace_back("camera_mode", text(m.camera_mode));
    if (!m.image_gamut.empty()) root.obj.emplace_back("image_gamut", text(m.image_gamut));
    if (m.image_linear >= 0) root.obj.emplace_back("image_linear", boolean(m.image_linear != 0));
    if (!m.cameras.empty()) {
        JsonValue cams;
        cams.type = JsonValue::Type::Array;
        for (const ManifestCamera& c : m.cameras) {
            JsonValue e;
            e.type = JsonValue::Type::Object;
            e.obj.emplace_back("prefix", text(c.prefix));
            if (!c.model.empty()) e.obj.emplace_back("model", text(c.model));
            if (c.focal > 0) e.obj.emplace_back("focal", number(c.focal));
            if (!c.distortion.empty()) {
                JsonValue d;
                d.type = JsonValue::Type::Array;
                for (double x : c.distortion) d.arr.push_back(number(x));
                e.obj.emplace_back("distortion", std::move(d));
            }
            cams.arr.push_back(std::move(e));
        }
        root.obj.emplace_back("cameras", std::move(cams));
    }
    if (!m.rigs.empty()) {
        JsonValue rigs;
        rigs.type = JsonValue::Type::Array;
        for (const RigDef& d : m.rigs) {
            JsonValue e;
            e.type = JsonValue::Type::Object;
            if (!d.name.empty()) e.obj.emplace_back("name", text(d.name));
            if (!d.captures.empty()) {
                JsonValue caps;
                caps.type = JsonValue::Type::Array;
                for (const std::string& c : d.captures) caps.arr.push_back(text(c));
                e.obj.emplace_back("captures", std::move(caps));
            }
            JsonValue mem;
            mem.type = JsonValue::Type::Array;
            for (const RigMemberDef& md : d.members) {
                if (!md.has_ext) {
                    mem.arr.push_back(text(md.prefix));
                    continue;
                }
                JsonValue o;
                o.type = JsonValue::Type::Object;
                o.obj.emplace_back("prefix", text(md.prefix));
                const Quat q = rotationToQuaternion(md.ext.R);
                JsonValue rot, tr;
                rot.type = tr.type = JsonValue::Type::Array;
                for (double v : q) rot.arr.push_back(number(v));
                for (double v : {md.ext.t.x, md.ext.t.y, md.ext.t.z}) tr.arr.push_back(number(v));
                o.obj.emplace_back("rotation", std::move(rot));
                o.obj.emplace_back("translation", std::move(tr));
                if (!md.ext_fixed) o.obj.emplace_back("fixed", boolean(false));
                mem.arr.push_back(std::move(o));
            }
            e.obj.emplace_back("members", std::move(mem));
            rigs.arr.push_back(std::move(e));
        }
        root.obj.emplace_back("rigs", std::move(rigs));
    }
    if (!m.captures.empty()) {
        JsonValue caps;
        caps.type = JsonValue::Type::Array;
        for (const ManifestCapture& c : m.captures) {
            JsonValue e;
            e.type = JsonValue::Type::Object;
            e.obj.emplace_back("prefix", text(c.prefix));
            e.obj.emplace_back("telemetry", text(c.telemetry));
            if (c.fps > 0) e.obj.emplace_back("fps", number(c.fps));
            if (c.time_offset != 0) e.obj.emplace_back("time_offset", number(c.time_offset));
            caps.arr.push_back(std::move(e));
        }
        root.obj.emplace_back("captures", std::move(caps));
    }
    return json ? json_write(root) : yaml_write(root);
}

std::string manifest_apply(const Manifest& m, SfmConfig& cfg,
                           const std::set<std::string>& seen,
                           std::string& image_dir) {
    const fs::path base(m.base_dir);
    if (!m.image_dir.empty() && image_dir.empty()) image_dir = resolve(m.image_dir, base);
    if (!m.mask_dir.empty() && !seen.count("masks") && !seen.count("mask-dir"))
        cfg.mask_dir = resolve(m.mask_dir, base);
    if (m.has_mask_flipped && !seen.count("flip-mask")) cfg.flip_mask = m.mask_flipped;
    if (!m.camera_mode.empty() && !seen.count("camera-mode")) {
        cfg.camera_mode = m.camera_mode;
        cfg.camera_mode_pinned = true;
    }
    if (!m.image_gamut.empty() && !seen.count("image-gamut")) cfg.image_gamut = m.image_gamut;
    if (m.image_linear >= 0 && !seen.count("image-linear"))
        cfg.image_is_linear = m.image_linear != 0;

    for (const ManifestCamera& c : m.cameras) {
        // The dataset-wide entry is the same thing --camera-model sets, so it
        // goes to the same field rather than becoming an override of everything.
        if (c.prefix.empty()) {
            if (!c.model.empty() && !seen.count("camera-model")) cfg.camera_model = c.model;
            if (c.focal > 0 && !seen.count("focal")) cfg.focal = c.focal;
            if (!c.distortion.empty() && !seen.count("distortion"))
                cfg.camera.extra = c.distortion;
            continue;
        }
        CameraOverride o;
        o.prefix = c.prefix;
        if (!c.model.empty()) {
            CamModel parsed;
            if (!parseCamModelName(c.model, parsed))
                return "unknown camera model '" + c.model + "' for '" + c.prefix + "'";
            o.has_model = true;
            o.model = parsed;
        }
        if (c.focal > 0) {
            o.has_focal = true;
            o.focal = c.focal;
        }
        if (!c.distortion.empty()) {
            o.has_extra = true;
            o.extra = c.distortion;
        }
        // Among equal-length prefixes the earliest entry wins
        // (cameraOverrideFor), and the command line's were pushed while argv
        // was parsed -- so appending here is what lets a flag beat the file.
        cfg.camera.overrides.push_back(o);
    }
    for (const ManifestCapture& c : m.captures)
        cfg.telemetry_inputs.push_back({c.prefix, resolve(c.telemetry, base), c.fps, c.time_offset});
    for (const RigDef& r : m.rigs) cfg.rigs.push_back(r);
    return {};
}

}  // namespace sfm

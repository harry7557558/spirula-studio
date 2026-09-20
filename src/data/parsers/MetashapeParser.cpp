// MetashapeParser.cpp -- Agisoft Metashape dataset reader for DatasetParser.h.
//
// Inputs: a camera-export .xml + a point-export .ply in the dataset dir,
// plus an optional .psx project whose .files zips provide the camera_id ->
// photo-path table used to disambiguate image-filename matching (needed
// when several image subdirectories share basenames, e.g. multi-lens rigs).
// XML via app/Xml.h; zip reading via external/miniz. The parsed cameras are
// converted to a transforms.json-shaped meta and fed through
// parse_nerfstudio_meta, mirroring Python's
// _parser_metashape_data -> _parse_nerfstudio_data(transforms[0]).

#include "data/DatasetParser.h"
#include "i18n/catalog/Data.h"
#include "data/Json.h"
#include "data/Xml.h"

#include "external/miniz.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace dmsg = spirula::i18n::msg::data;
using spirula::i18n::format;

namespace fs = std::filesystem;

namespace {

// ===========================================================================
// JsonValue construction helpers
// ===========================================================================

JsonValue jnum(double v) {
    JsonValue j; j.type = JsonValue::Type::Number; j.num = v; return j;
}
JsonValue jstr(std::string s) {
    JsonValue j; j.type = JsonValue::Type::String; j.str = std::move(s); return j;
}
JsonValue jarr() { JsonValue j; j.type = JsonValue::Type::Array;  return j; }
JsonValue jobj() { JsonValue j; j.type = JsonValue::Type::Object; return j; }

void jset(JsonValue& obj, const char* key, JsonValue v) {
    obj.obj.emplace_back(key, std::move(v));
}


// ===========================================================================
// Camera-label / photo-path -> image-filename matching
// (metashape_utils.py StringMatcher; exact brute force here -- corpora are
// only a few thousand short strings)
// ===========================================================================

std::string norm_slashes(std::string s) {
    std::replace(s.begin(), s.end(), '\\', '/');
    return s;
}

size_t common_suffix_len(const std::string& a, const std::string& b) {
    size_t n = std::min(a.size(), b.size()), i = 0;
    while (i < n && a[a.size()-1-i] == b[b.size()-1-i]) i++;
    return i;
}

// Filenames sharing the longest common suffix with `s` (query_suffix).
std::vector<std::string> match_suffix(const std::vector<std::string>& names,
                                      const std::string& s) {
    std::string q = norm_slashes(s);
    size_t best = 0;
    std::vector<std::string> matches;
    for (const std::string& name : names) {
        size_t l = common_suffix_len(q, name);
        if (l > best) { best = l; matches.clear(); }
        if (l == best && l > 0) matches.push_back(name);
    }
    return matches;
}

// Filenames containing the longest prefix of `s` as a substring
// (query_substring; used for camera labels when no .psx table exists).
std::vector<std::string> match_substring(const std::vector<std::string>& names,
                                         const std::string& s) {
    std::string q = norm_slashes(s);
    for (size_t len = q.size(); len > 0; len--) {
        std::string needle = q.substr(0, len);
        std::vector<std::string> matches;
        for (const std::string& name : names)
            if (name.find(needle) != std::string::npos) matches.push_back(name);
        if (!matches.empty()) return matches;
    }
    return {};
}


// ===========================================================================
// find_metashape_cameras_dict: camera_id -> photo path from the .psx
// project's .files tree (plain XMLs and doc.xml inside zips)
// ===========================================================================

bool cameras_dict_from_xml(const XmlNode& root,
                           std::map<std::string, std::string>& out) {
    const XmlNode* cameras = root.find("cameras");
    if (!cameras) return false;
    std::map<std::string, std::string> d;
    for (const XmlNode* cam : cameras->findall("camera")) {
        const std::string* cid = cam->attr("camera_id");
        const XmlNode* photo = cam->find("photo");
        if (!cid || !photo) continue;
        if (const std::string* path = photo->attr("path")) d[*cid] = *path;
    }
    if (d.empty()) return false;
    out = std::move(d);
    return true;
}

bool ends_with_ci(const std::string& s, const char* suffix) {
    size_t n = std::strlen(suffix);
    if (s.size() < n) return false;
    for (size_t i = 0; i < n; i++)
        if (std::tolower((unsigned char)s[s.size()-n+i]) != suffix[i]) return false;
    return true;
}

bool find_metashape_cameras_dict(const fs::path& root_dir,
                                 std::map<std::string, std::string>& out) {
    if (!fs::is_directory(root_dir)) return false;
    for (const auto& entry : fs::recursive_directory_iterator(root_dir)) {
        if (!entry.is_regular_file()) continue;
        std::string path = entry.path().string();

        if (ends_with_ci(path, ".xml")) {
            try {
                if (cameras_dict_from_xml(xml_parse_file(path), out)) return true;
            } catch (const std::exception&) {}
            continue;
        }

        if (!ends_with_ci(path, ".zip")) continue;
        mz_zip_archive za;
        std::memset(&za, 0, sizeof(za));
        if (!mz_zip_reader_init_file(&za, path.c_str(), 0)) continue;
        int n = (int)mz_zip_reader_get_num_files(&za);
        for (int i = 0; i < n; i++) {
            mz_zip_archive_file_stat st;
            if (!mz_zip_reader_file_stat(&za, i, &st)) continue;
            if (!ends_with_ci(st.m_filename, ".xml")) continue;
            size_t size = 0;
            void* data = mz_zip_reader_extract_to_heap(&za, i, &size, 0);
            if (!data) continue;
            std::string content((const char*)data, size);
            mz_free(data);
            try {
                if (cameras_dict_from_xml(xml_parse(content), out)) {
                    mz_zip_reader_end(&za);
                    return true;
                }
            } catch (const std::exception&) {}
        }
        mz_zip_reader_end(&za);
    }
    return false;
}


// ===========================================================================
// Input-file discovery.
// Relative paths resolve against the dataset dir; empty = the unique
// candidate with that extension in the dataset dir.
// ===========================================================================

fs::path resolve_input(const fs::path& root, const std::string& configured,
                       const char* ext, bool required, const char* flag) {
    if (!configured.empty()) {
        fs::path p(configured);
        if (p.is_relative()) p = root / p;
        if (!fs::exists(p))
            throw std::runtime_error("MetashapeParser: " + p.string() +
                                     " does not exist");
        if (!ends_with_ci(p.string(), ext))
            throw std::runtime_error("MetashapeParser: " + p.string() +
                                     " must have a " + ext + " extension");
        return p;
    }
    std::vector<fs::path> found;
    for (const auto& entry : fs::directory_iterator(root))
        if (entry.is_regular_file() && ends_with_ci(entry.path().string(), ext))
            found.push_back(entry.path());
    if (found.size() > 1)
        throw std::runtime_error(std::string("MetashapeParser: multiple ") + ext +
                                 " files found in dataset dir; specify using --" + flag);
    if (found.empty()) {
        if (required)
            throw std::runtime_error(std::string("MetashapeParser: no ") + ext +
                                     " file found in dataset dir; specify using --" + flag);
        return {};
    }
    std::printf("%s\n",
                format(dmsg::ms_using_file, {ext, found[0].string()}).c_str());
    return found[0];
}


double attr_double(const XmlNode& node, const char* name, double def) {
    const std::string* v = node.attr(name);
    return v ? std::atof(v->c_str()) : def;
}

// _find_param: child element text as double, 0 when absent.
double child_double(const XmlNode& node, const char* tag) {
    const XmlNode* c = node.find(tag);
    return c ? std::atof(c->text.c_str()) : 0.0;
}

std::vector<double> split_doubles(const std::string& text) {
    std::vector<double> out;
    const char* p = text.c_str();
    char* end = nullptr;
    for (;;) {
        double v = std::strtod(p, &end);
        if (end == p) break;
        out.push_back(v);
        p = end;
    }
    return out;
}

std::vector<std::string> split_tokens(const std::string& text) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && std::isspace((unsigned char)text[i])) i++;
        size_t j = i;
        while (j < text.size() && !std::isspace((unsigned char)text[j])) j++;
        if (j > i) out.push_back(text.substr(i, j - i));
        i = j;
    }
    return out;
}

}  // namespace


// ===========================================================================
// parse_metashape_dataset
// ===========================================================================

ParsedDataset parse_metashape_dataset(const std::string& dataset_dir,
                                      const DatasetParserConfig& cfg) {
    fs::path root(dataset_dir);
    if (!fs::is_directory(root))
        throw std::runtime_error("MetashapeParser: dataset dir " + dataset_dir +
                                 " not found");

    fs::path xml_path = resolve_input(root, cfg.metashape_xml, ".xml", true,
                                      "metashape-xml");
    // Optional: an XML alone is poses, which the trainer seeds at random
    // (--random-init) and the viewer draws as frustums.
    fs::path ply_path = resolve_input(root, cfg.metashape_ply, ".ply", false,
                                      "metashape-ply");
    fs::path psx_path = resolve_input(root, cfg.metashape_psx, ".psx", false,
                                      "metashape-psx");

    // ---- Optional .psx camera_id -> photo path table -----------------------
    std::map<std::string, std::string> camera_dict;
    bool have_camera_dict = false;
    if (!psx_path.empty()) {
        XmlNode psx = xml_parse_file(psx_path.string());
        if (const std::string* rel = psx.attr("path")) {
            // path="{projectname}.files/project.zip"; the camera tables live
            // in the .files tree that contains it.
            std::string expanded = *rel;
            const std::string token = "{projectname}";
            if (size_t pos = expanded.find(token); pos != std::string::npos)
                expanded.replace(pos, token.size(), psx_path.stem().string());
            fs::path files_root = (psx_path.parent_path() / expanded).parent_path();
            have_camera_dict = find_metashape_cameras_dict(files_root, camera_dict);
        }
        if (!have_camera_dict)
            std::fprintf(stderr, "%s %s\n", dmsg::word_warning.get(),
                         format(dmsg::ms_no_camera_table,
                                {psx_path.string()}).c_str());
    }

    // ---- Image file list (relative to the dataset dir) ---------------------
    // In lenient (viewer) mode a missing image directory is tolerated; camera
    // labels stand in for filenames below.
    fs::path image_dir = root / cfg.image_dir;
    if (!fs::is_directory(image_dir) && cfg.require_image_files)
        throw std::runtime_error("MetashapeParser: image directory " +
                                 image_dir.string() +
                                 " does not exist; specify using --image-dir");
    std::vector<std::string> image_filenames;
    if (fs::is_directory(image_dir))
        for (const auto& entry : fs::recursive_directory_iterator(image_dir))
            if (entry.is_regular_file())
                image_filenames.push_back(
                    fs::relative(entry.path(), root).generic_string());

    // ---- Camera-export XML (metashape_to_json) ------------------------------
    XmlNode doc = xml_parse_file(xml_path.string());
    if (doc.children.empty())
        throw std::runtime_error("MetashapeParser: empty document in " +
                                 xml_path.string());
    const XmlNode& chunk = doc.children[0];

    const XmlNode* sensors = chunk.find("sensors");
    if (!sensors)
        throw std::runtime_error("MetashapeParser: no sensors found");

    // Sensor id -> transforms.json-style intrinsics fields.
    std::map<std::string, JsonValue> sensor_dict;
    for (const XmlNode* sensor : sensors->iter("sensor")) {
        const std::string* idp = sensor->attr("id");
        if (!idp) continue;
        std::string type = sensor->attr("type") ? *sensor->attr("type") : "";

        // Calibrated sensors only (spherical/cylindrical need none).
        bool intrinsic_free = (type == "spherical" || type == "cylindrical");
        if (!intrinsic_free && !sensor->find("calibration")) continue;

        std::string camera_model;
        if (type == "frame")                camera_model = "PINHOLE";
        else if (type == "fisheye" ||
                 type == "equidistant_fisheye") camera_model = "FISHEYE";
        else if (type == "equisolid_fisheye")   camera_model = "EQUISOLID";
        else if (type == "spherical")           camera_model = "EQUIRECTANGULAR";
        else
            // cylindrical, RPC, ...
            throw std::runtime_error(
                "MetashapeParser: unsupported Metashape sensor type '" + type + "'");

        const XmlNode* resolution = sensor->find("resolution");
        if (!resolution)
            throw std::runtime_error("MetashapeParser: sensor " + *idp +
                                     " has no resolution");
        double w = attr_double(*resolution, "width", 0);
        double h = attr_double(*resolution, "height", 0);

        // First calibration whose class attribute exists and != "initial"
        // (ElementTree calibration[@class!='initial'] semantics).
        const XmlNode* calib = nullptr;
        for (const XmlNode* c : sensor->findall("calibration")) {
            const std::string* cls = c->attr("class");
            if (cls && *cls != "initial") { calib = c; break; }
        }

        JsonValue s = jobj();
        jset(s, "camera_model", jstr(camera_model));
        jset(s, "w", jnum(w));
        jset(s, "h", jnum(h));
        if (!calib) {
            if (!intrinsic_free)
                throw std::runtime_error("MetashapeParser: sensor " + *idp +
                                         " has no adjusted calibration");
            jset(s, "fl_x", jnum(w / 2.0));
            jset(s, "fl_y", jnum(h));
            jset(s, "cx", jnum(w / 2.0));
            jset(s, "cy", jnum(h / 2.0));
        } else {
            const XmlNode* f = calib->find("f");
            if (!f)
                throw std::runtime_error("MetashapeParser: focal length not "
                                         "found for sensor " + *idp);
            double fl = std::atof(f->text.c_str());
            jset(s, "fl_x", jnum(fl));
            jset(s, "fl_y", jnum(fl));
            jset(s, "cx", jnum(child_double(*calib, "cx") + w / 2.0));
            jset(s, "cy", jnum(child_double(*calib, "cy") + h / 2.0));
            jset(s, "k1", jnum(child_double(*calib, "k1")));
            jset(s, "k2", jnum(child_double(*calib, "k2")));
            jset(s, "k3", jnum(child_double(*calib, "k3")));
            jset(s, "k4", jnum(child_double(*calib, "k4")));
            // Metashape p1/p2 are swapped relative to OpenCV; b1/b2 are in
            // pixels, ours are relative to f (metashape_utils.py:259-262).
            jset(s, "p1", jnum(child_double(*calib, "p2")));
            jset(s, "p2", jnum(child_double(*calib, "p1")));
            jset(s, "b1", jnum(child_double(*calib, "b1") / fl));
            jset(s, "b2", jnum(child_double(*calib, "b2") / fl));
            // Metashape's k4 is the 4th RADIAL term, not OpenCV's first
            // rational denominator, so say so rather than let the nerfstudio
            // back end guess from which keys are present.
            jset(s, "camera_distortion", jstr("THIN_PRISM"));
            // PhotoScan <= 1.5 multiplied the tangential terms by
            // (1 + P3 r^2 + P4 r^4); nothing here carries that.
            if (child_double(*calib, "p3") != 0.0 ||
                child_double(*calib, "p4") != 0.0)
                throw std::runtime_error(
                    "MetashapeParser: sensor " + *idp + " has nonzero p3/p4 "
                    "(the pre-1.6 tangential scaling), which no supported "
                    "distortion model carries");
        }
        sensor_dict.emplace(*idp, std::move(s));
    }

    // ---- Components: alignment transforms + camera-id groups ----------------
    std::map<std::string, std::array<double, 16>> component_transforms;
    std::vector<std::pair<std::string, std::vector<std::string>>> component_groups;
    if (const XmlNode* components = chunk.find("components")) {
        for (const XmlNode* component : components->iter("component")) {
            const std::string* cidp = component->attr("id");
            std::string cid = cidp ? *cidp : "";
            if (const XmlNode* transform = component->find("transform")) {
                std::array<double, 16> m = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
                double s = 1.0;
                if (const XmlNode* scale = transform->find("scale"))
                    s = std::atof(scale->text.c_str());
                if (const XmlNode* rotation = transform->find("rotation")) {
                    std::vector<double> r = split_doubles(rotation->text);
                    if (r.size() >= 9)
                        for (int i = 0; i < 3; i++)
                            for (int j = 0; j < 3; j++)
                                m[i*4 + j] = r[i*3 + j];
                }
                for (int i = 0; i < 3; i++)
                    for (int j = 0; j < 3; j++) m[i*4 + j] *= s;
                if (const XmlNode* translation = transform->find("translation")) {
                    std::vector<double> t = split_doubles(translation->text);
                    if (t.size() >= 3)
                        for (int i = 0; i < 3; i++) m[i*4 + 3] = t[i];
                }
                component_transforms[cid] = m;
            }
            // camera_ids live inside <partition>, hence the recursive iter.
            std::vector<std::string> ids;
            for (const XmlNode* ci : component->iter("camera_ids")) {
                std::vector<std::string> toks = split_tokens(ci->text);
                ids.insert(ids.end(), toks.begin(), toks.end());
            }
            if (!ids.empty()) component_groups.emplace_back(cid, std::move(ids));
        }
    }

    // ---- Cameras: filename match + sensor + transform ------------------------
    const XmlNode* cameras = chunk.find("cameras");
    if (!cameras)
        throw std::runtime_error("MetashapeParser: cameras not found in " +
                                 xml_path.string());

    struct ValidCamera {
        std::vector<double> transform;   // 16, row-major
        std::string component_id;
        JsonValue frame;                 // file_path + sensor fields
    };
    std::map<std::string, ValidCamera> valid_cameras;
    std::vector<std::string> valid_order;
    int64_t num_skipped = 0;

    // <camera> elements can be nested inside <group>, hence the recursive iter.
    for (const XmlNode* camera : cameras->iter("camera")) {
        const std::string* cam_id = camera->attr("id");
        if (!cam_id) continue;
        const std::string* label = camera->attr("label");
        std::string label_s = label ? *label : ("camera " + *cam_id);

        std::vector<std::string> matches;
        if (image_filenames.empty() && !cfg.require_image_files) {
            // Lenient mode with no images on disk: the camera label stands in
            // for the filename (poses/frustums only).
            matches = {label_s};
        } else if (have_camera_dict) {
            auto it = camera_dict.find(*cam_id);
            if (it == camera_dict.end()) {
                std::fprintf(stderr, "%s %s\n", dmsg::word_warning.get(),
                             format(dmsg::ms_camera_not_in_table,
                                    {label_s}).c_str());
                num_skipped++;
                continue;
            }
            matches = match_suffix(image_filenames, it->second);
        } else {
            matches = match_substring(image_filenames, label_s);
        }
        if (matches.size() != 1) {
            // Two whole sentences rather than one with a swappable tail:
            // the hint lands in a different place in a verb-final language.
            std::fprintf(stderr, "%s %s\n", dmsg::word_warning.get(),
                         format(have_camera_dict ? dmsg::ms_ambiguous
                                                 : dmsg::ms_ambiguous_psx,
                                {label_s, (long long)matches.size()}).c_str());
            num_skipped++;
            continue;
        }

        const std::string* sensor_id = camera->attr("sensor_id");
        auto sit = sensor_id ? sensor_dict.find(*sensor_id) : sensor_dict.end();
        if (sit == sensor_dict.end()) {
            std::fprintf(stderr, "%s %s\n", dmsg::word_warning.get(),
                         format(dmsg::ms_missing_sensor, {label_s}).c_str());
            num_skipped++;
            continue;
        }

        const XmlNode* transform = camera->find("transform");
        if (!transform) {
            std::fprintf(stderr, "%s %s\n", dmsg::word_warning.get(),
                         format(dmsg::ms_missing_transform, {label_s}).c_str());
            num_skipped++;
            continue;
        }
        std::vector<double> t = split_doubles(transform->text);
        if (t.size() < 16) {
            std::fprintf(stderr, "%s %s\n", dmsg::word_warning.get(),
                         format(dmsg::ms_malformed_transform, {label_s}).c_str());
            num_skipped++;
            continue;
        }

        ValidCamera vc;
        vc.transform = std::move(t);
        if (const std::string* comp = camera->attr("component_id"))
            vc.component_id = *comp;
        vc.frame = jobj();
        jset(vc.frame, "file_path", jstr(matches[0]));
        jset(vc.frame, "camera_id", jnum(std::stod(*sensor_id)));
        for (const auto& [k, v] : sit->second.obj)
            vc.frame.obj.emplace_back(k, v);

        valid_order.push_back(*cam_id);
        valid_cameras.emplace(*cam_id, std::move(vc));
    }

    // ---- Pick the camera group to train on ----------------------------------
    std::vector<std::string> group;
    if (component_groups.empty()) {
        group = valid_order;
    } else {
        size_t best = 0, best_count = 0;
        std::vector<size_t> counts(component_groups.size(), 0);
        for (size_t g = 0; g < component_groups.size(); g++) {
            for (const std::string& id : component_groups[g].second)
                if (valid_cameras.count(id)) counts[g]++;
            if (counts[g] > best_count) { best_count = counts[g]; best = g; }
        }
        // Explicit component selection (viewer component picker) overrides the
        // largest-group default.
        if (cfg.metashape_component >= 0 &&
            (size_t)cfg.metashape_component < component_groups.size()) {
            best = (size_t)cfg.metashape_component;
            best_count = counts[best];
        }
        if (component_groups.size() > 1)
            std::fprintf(stderr, "%s %s\n", dmsg::word_warning.get(),
                         format(dmsg::ms_components,
                                {(long long)component_groups.size(),
                                 (long long)best_count,
                                 (long long)valid_cameras.size()}).c_str());
        // Keep document order within the group.
        std::vector<std::string> ids = component_groups[best].second;
        std::sort(ids.begin(), ids.end());
        for (const std::string& id : valid_order)
            if (std::binary_search(ids.begin(), ids.end(), id)) group.push_back(id);
    }

    // ---- Build the transforms.json-shaped meta -------------------------------
    JsonValue frames = jarr();
    for (const std::string& id : group) {
        ValidCamera& vc = valid_cameras.at(id);
        std::array<double, 16> m;
        std::copy(vc.transform.begin(), vc.transform.begin() + 16, m.begin());

        auto cit = component_transforms.find(vc.component_id);
        if (!vc.component_id.empty() && cit != component_transforms.end()) {
            const std::array<double, 16>& c = cit->second;
            std::array<double, 16> out;
            for (int r = 0; r < 4; r++)
                for (int col = 0; col < 4; col++) {
                    double v = 0.0;
                    for (int k = 0; k < 4; k++) v += c[r*4 + k] * m[k*4 + col];
                    out[r*4 + col] = v;
                }
            m = out;
            // De-scale the rotation block (metashape_utils.py:367).
            double det =
                m[0]*(m[5]*m[10] - m[6]*m[9]) -
                m[1]*(m[4]*m[10] - m[6]*m[8]) +
                m[2]*(m[4]*m[9]  - m[5]*m[8]);
            double s = std::cbrt(det);
            if (s != 0.0)
                for (int r = 0; r < 3; r++)
                    for (int col = 0; col < 3; col++) m[r*4 + col] /= s;
        }

        // Metashape camera frame is OpenCV (+Z forward); ours is OpenGL --
        // negate columns 1 and 2 (metashape_utils.py:373).
        for (int r = 0; r < 4; r++) { m[r*4 + 1] = -m[r*4 + 1]; m[r*4 + 2] = -m[r*4 + 2]; }

        JsonValue tm = jarr();
        for (int r = 0; r < 4; r++) {
            JsonValue row = jarr();
            for (int col = 0; col < 4; col++) row.arr.push_back(jnum(m[r*4 + col]));
            tm.arr.push_back(std::move(row));
        }
        jset(vc.frame, "transform_matrix", std::move(tm));
        frames.arr.push_back(std::move(vc.frame));
    }
    if (frames.arr.empty())
        throw std::runtime_error("MetashapeParser: no usable aligned cameras in " +
                                 xml_path.string());

    JsonValue meta = jobj();
    jset(meta, "frames", std::move(frames));
    if (!ply_path.empty())
        jset(meta, "ply_file_path",
             jstr(fs::relative(ply_path, root).generic_string()));

    if (num_skipped > 0)
        std::printf("%s\n",
                    format(dmsg::ms_skipped, {(long long)num_skipped}).c_str());
    std::printf("%s\n",
                format(dmsg::final_frames,
                       {(long long)meta.find("frames")->arr.size()}).c_str());

    return parse_nerfstudio_meta(meta, dataset_dir, cfg);
}

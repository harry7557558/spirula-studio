// Dataset bridge for the standalone viewer.
//
// Reuses the trainer's battle-tested dataset parsers (compiled from
// src/data/parsers/*.cpp, referenced in-place by the viewer's
// CMakeLists) to load COLMAP / Nerfstudio / Metashape reconstructions in the
// browser. JS mounts the dropped files into Emscripten's in-memory filesystem
// (MEMFS) under /data, then this C ABI enumerates components and parses the one
// the user selects, exposing the point cloud + per-camera intrinsics/poses.
//
// Everything here runs against the virtual FS, so std::filesystem / FILE* work
// exactly as in the native build. Parsers throw std::runtime_error; every entry
// point wraps them and degrades gracefully so a partial drop (only sparse/, only
// a transforms.json, only a pointcloud.ply, ...) still shows what is available.

#include "data/DatasetParser.h"
#include "data/FrustumSize.h"
#include "data/Json.h"
#include "data/Xml.h"

#include <emscripten.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;
#define KEEP EMSCRIPTEN_KEEPALIVE extern "C"

static const char* kRoot = "/data";

// ---------------------------------------------------------------------------
// Parsed result (one dataset component at a time).
// ---------------------------------------------------------------------------
static ParsedDataset g_ds;
static bool          g_loaded = false;
static std::string   g_format;      // "COLMAP" / "Nerfstudio" / "Metashape" / "Point cloud"
static std::string   g_json;        // reused output buffer (enumerate / summary)
static std::string   g_names;       // newline-joined image basenames
static std::string   g_error;       // last parse error ("" = clean load)
static std::vector<float> g_pts;    // the seed cloud as JS reads it, [N, 3]
static float         g_fit[4];      // (cx, cy, cz, radius)
static float         g_pick[3];     // (index, t, perp) for point picking

static void reset_result() {
    g_ds = ParsedDataset();
    g_pts.clear();
    g_pts.shrink_to_fit();
    g_loaded = false;
    g_format.clear();
    g_names.clear();
    g_error.clear();
}

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

// First little-endian uint64 of a binary file (COLMAP element count), or -1.
static int64_t read_count_u64(const fs::path& p) {
    FILE* f = std::fopen(p.string().c_str(), "rb");
    if (!f) return -1;
    uint64_t n = 0;
    size_t got = std::fread(&n, 1, 8, f);
    std::fclose(f);
    if (got != 8) return -1;
    return (int64_t)n;
}

// "# Number of <what>: N" header of a COLMAP text file, or -1.
static int64_t read_count_txt(const fs::path& p) {
    FILE* f = std::fopen(p.string().c_str(), "rb");
    if (!f) return -1;
    char buf[2048];
    size_t got = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    buf[got] = 0;
    const char* m = std::strstr(buf, "# Number of ");
    if (!m) return -1;
    m = std::strchr(m, ':');
    if (!m) return -1;
    return std::atoll(m + 1);
}

// COLMAP element count for either format ("cameras" / "images" / "points3D").
static int64_t colmap_count(const fs::path& dir, const char* base) {
    fs::path bin = dir / (std::string(base) + ".bin");
    std::error_code ec;
    if (fs::exists(bin, ec)) return read_count_u64(bin);
    return read_count_txt(dir / (std::string(base) + ".txt"));
}

static void json_escape(std::string& out, const std::string& s) {
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
}

// path relative to /data (forward slashes), "" for the root itself.
static std::string rel_to_root(const fs::path& p) {
    std::error_code ec;
    fs::path r = fs::relative(p, fs::path(kRoot), ec);
    if (ec) return p.string();
    std::string s = r.generic_string();
    return (s == ".") ? std::string() : s;
}

static std::string basename_of(const std::string& p) {
    return fs::path(p).filename().string();
}

// ---------------------------------------------------------------------------
// Component enumeration -> JSON. Scans /data for every detectable component so
// the UI can offer a picker (COLMAP sparse/0, sparse/1, ...; each Nerfstudio
// transforms.json; each Metashape .xml and its internal <component> chunks).
// ---------------------------------------------------------------------------

struct Component {
    std::string type;    // colmap | nerfstudio | metashape | pointcloud
    std::string label;
    std::string token;   // tab-separated, parsed by ssv_ds_parse
    int64_t     cameras = -1;
    int64_t     points  = -1;
};

static void enum_metashape(const fs::path& xml, std::vector<Component>& out) {
    XmlNode doc;
    try { doc = xml_parse_file(xml.string()); }
    catch (...) { return; }
    if (doc.children.empty()) return;
    const XmlNode& chunk = doc.children[0];
    std::string xrel = rel_to_root(xml);

    // Count cameras per internal <component> group (mirrors MetashapeParser).
    std::vector<std::pair<std::string, size_t>> groups;   // (component id, count)
    if (const XmlNode* comps = chunk.find("components")) {
        for (const XmlNode* c : comps->iter("component")) {
            const std::string* cid = c->attr("id");
            size_t n = 0;
            for (const XmlNode* ci : c->iter("camera_ids")) {
                // rough token count
                const std::string& t = ci->text;
                bool in = false;
                for (char ch : t) {
                    bool ws = (ch==' '||ch=='\t'||ch=='\n'||ch=='\r');
                    if (!ws && !in) { n++; in = true; } else if (ws) in = false;
                }
            }
            groups.emplace_back(cid ? *cid : std::string(), n);
        }
    }
    int64_t total = 0;
    if (const XmlNode* cams = chunk.find("cameras"))
        for (const XmlNode* c : cams->iter("camera")) { (void)c; total++; }

    if (groups.size() <= 1) {
        Component comp;
        comp.type = "metashape";
        comp.label = "Metashape: " + xrel;
        comp.token = std::string("metashape\t") + xrel + "\t-1";
        comp.cameras = total;
        out.push_back(comp);
    } else {
        for (size_t i = 0; i < groups.size(); i++) {
            Component comp;
            comp.type = "metashape";
            comp.label = "Metashape: " + xrel + " — component " +
                         (groups[i].first.empty() ? std::to_string(i) : groups[i].first);
            comp.token = std::string("metashape\t") + xrel + "\t" + std::to_string(i);
            comp.cameras = (int64_t)groups[i].second;
            out.push_back(comp);
        }
    }
}

static std::vector<Component> enumerate_components() {
    std::vector<Component> comps;
    std::vector<fs::path> plys;
    std::error_code ec;
    fs::path root(kRoot);
    if (!fs::exists(root, ec)) return comps;

    // Directories -> COLMAP recon dirs.
    for (auto it = fs::recursive_directory_iterator(
             root, fs::directory_options::skip_permission_denied, ec);
         !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        const fs::path& p = it->path();
        std::error_code fec;
        if (fs::is_directory(p, fec)) {
            auto have = [&](const char* base) {
                return fs::exists(p / (std::string(base) + ".bin"), fec) ||
                       fs::exists(p / (std::string(base) + ".txt"), fec);
            };
            bool cam = have("cameras"), img = have("images"), pts = have("points3D");
            if ((cam && img) || pts) {
                Component comp;
                comp.type = "colmap";
                std::string rel = rel_to_root(p);
                comp.label = "COLMAP: " + (rel.empty() ? std::string(".") : rel);
                comp.token = std::string("colmap\t") + rel;
                if (img) comp.cameras = colmap_count(p, "images");
                if (pts) comp.points = colmap_count(p, "points3D");
                comps.push_back(comp);
            }
        } else if (fs::is_regular_file(p, fec)) {
            std::string name = p.filename().string();
            std::string ext = p.extension().string();
            for (char& c : ext) c = (char)std::tolower((unsigned char)c);
            if (name == "transforms.json") {
                Component comp;
                comp.type = "nerfstudio";
                std::string reldir = rel_to_root(p.parent_path());
                comp.label = "Nerfstudio: " +
                             (reldir.empty() ? std::string("transforms.json")
                                             : reldir + "/transforms.json");
                comp.token = std::string("nerfstudio\t") + reldir;
                try {
                    JsonValue meta = json_parse_file(p.string());
                    if (const JsonValue* fr = meta.find("frames"))
                        comp.cameras = (int64_t)fr->arr.size();
                } catch (...) {}
                comps.push_back(comp);
            } else if (ext == ".xml") {
                enum_metashape(p, comps);
            } else if (ext == ".ply") {
                plys.push_back(p);
            }
        }
    }

    // Bare point clouds are offered only when nothing richer was found, so a
    // full dataset's sparse_pc.ply doesn't clutter the picker.
    if (comps.empty()) {
        for (const fs::path& p : plys) {
            Component comp;
            comp.type = "pointcloud";
            std::string rel = rel_to_root(p);
            comp.label = "Point cloud: " + rel;
            comp.token = std::string("pointcloud\t") + rel;
            comps.push_back(comp);
        }
    }
    return comps;
}

KEEP int ssv_ds_enumerate() {
    std::vector<Component> comps;
    try { comps = enumerate_components(); } catch (...) {}
    g_json = "[";
    for (size_t i = 0; i < comps.size(); i++) {
        if (i) g_json += ',';
        g_json += "{\"type\":\"";  json_escape(g_json, comps[i].type);
        g_json += "\",\"label\":\""; json_escape(g_json, comps[i].label);
        g_json += "\",\"token\":\""; json_escape(g_json, comps[i].token);
        g_json += "\",\"cameras\":" + std::to_string(comps[i].cameras);
        g_json += ",\"points\":" + std::to_string(comps[i].points);
        g_json += '}';
    }
    g_json += ']';
    return (int)comps.size();
}

// ---------------------------------------------------------------------------
// Parse the selected component (tab-separated token). Lenient: on any parser
// failure, salvage whatever can be read (points-only, cameras-only).
// ---------------------------------------------------------------------------

static void split_tabs(const std::string& s, std::vector<std::string>& out) {
    out.clear();
    size_t start = 0;
    while (true) {
        size_t tab = s.find('\t', start);
        if (tab == std::string::npos) { out.push_back(s.substr(start)); break; }
        out.push_back(s.substr(start, tab - start));
        start = tab + 1;
    }
}

static void build_names() {
    g_names.clear();
    for (const std::string& f : g_ds.image_filenames) {
        g_names += basename_of(f);
        g_names += '\n';
    }
}

KEEP int ssv_ds_parse(const char* token_c) {
    reset_result();
    std::string token = token_c ? token_c : "";
    std::vector<std::string> parts;
    split_tabs(token, parts);
    if (parts.empty()) return 0;
    const std::string& type = parts[0];

    DatasetParserConfig cfg;
    cfg.require_image_files = false;   // viewer only needs poses/points

    try {
        if (type == "colmap") {
            std::string rel = parts.size() > 1 ? parts[1] : "";
            cfg.recon_dir = rel;
            try {
                g_ds = parse_colmap_dataset(kRoot, cfg);
            } catch (const std::exception& e) {
                // Salvage a lone points3D file if the full parse failed.
                g_error = e.what();
                fs::path recon = fs::path(kRoot) / rel;
                if (fs::exists(recon / "points3D.bin"))
                    g_ds.points = read_points3D_binary(recon.string());
                else if (fs::exists(recon / "points3D.txt"))
                    g_ds.points = read_points3D_text(recon.string());
                else throw;
            }
            g_format = "COLMAP";
        } else if (type == "nerfstudio") {
            std::string reldir = parts.size() > 1 ? parts[1] : "";
            fs::path dir = reldir.empty() ? fs::path(kRoot) : (fs::path(kRoot) / reldir);
            g_ds = parse_nerfstudio_dataset(dir.string(), cfg);
            g_format = "Nerfstudio";
        } else if (type == "metashape") {
            std::string xrel = parts.size() > 1 ? parts[1] : "";
            int comp = parts.size() > 2 ? std::atoi(parts[2].c_str()) : -1;
            fs::path xml = fs::path(kRoot) / xrel;
            fs::path dir = xml.parent_path();
            cfg.metashape_xml = xml.string();
            cfg.metashape_component = comp;
            // Pick a sibling .ply / .psx if unambiguous (else auto-detect).
            std::error_code ec;
            std::vector<fs::path> plys, psxs;
            for (auto& e : fs::directory_iterator(dir, ec)) {
                std::string ext = e.path().extension().string();
                for (char& c : ext) c = (char)std::tolower((unsigned char)c);
                if (ext == ".ply") plys.push_back(e.path());
                else if (ext == ".psx") psxs.push_back(e.path());
            }
            if (plys.size() == 1) cfg.metashape_ply = plys[0].string();
            if (psxs.size() == 1) cfg.metashape_psx = psxs[0].string();
            g_ds = parse_metashape_dataset(dir.string(), cfg);
            g_format = "Metashape";
        } else if (type == "pointcloud") {
            std::string rel = parts.size() > 1 ? parts[1] : "";
            g_ds.points = read_ply_points((fs::path(kRoot) / rel).string());
            g_format = "Point cloud";
        } else {
            return 0;
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "ssv_ds_parse: %s\n", e.what());
        g_error = e.what();
        // Keep whatever landed in g_ds (partial success is still useful).
    } catch (...) {
        std::fprintf(stderr, "ssv_ds_parse: unknown error\n");
        g_error = "unknown parser error";
    }

    g_ds.num_cameras = (int64_t)g_ds.camera_models.size();
    build_names();
    // The parsers keep the cloud in double; JS wants one Float32Array view of
    // it, and the double copy has no reader left once that exists.
    g_pts.assign(g_ds.points.xyz.begin(), g_ds.points.xyz.end());
    g_ds.points.xyz.clear();
    g_ds.points.xyz.shrink_to_fit();
    g_loaded = (g_ds.num_cameras > 0 || g_pts.size() > 0);
    return g_loaded ? 1 : 0;
}

// ---------------------------------------------------------------------------
// Accessors (typed-array views over the parsed vectors).
// ---------------------------------------------------------------------------
KEEP int      ssv_ds_ok()          { return g_loaded ? 1 : 0; }
KEEP char*    ssv_ds_json()        { return (char*)g_json.c_str(); }
KEEP char*    ssv_ds_last_error()  { return (char*)g_error.c_str(); }

KEEP int      ssv_ds_num_points()  { return (int)(g_pts.size() / 3); }
KEEP float*   ssv_ds_points_xyz()  { return g_pts.empty()? nullptr : g_pts.data(); }
KEEP uint8_t* ssv_ds_points_rgb()  { return g_ds.points.rgb.empty()? nullptr : g_ds.points.rgb.data(); }

KEEP int      ssv_ds_num_cameras() { return (int)g_ds.num_cameras; }
KEEP int32_t* ssv_ds_cam_models()  { return g_ds.camera_models.empty()? nullptr : g_ds.camera_models.data(); }
KEEP float*   ssv_ds_cam_intrins() { return g_ds.intrins.empty()? nullptr : g_ds.intrins.data(); }      // [N,4]
KEEP int32_t* ssv_ds_cam_dist_types() { return g_ds.camera_distortions.empty()? nullptr : g_ds.camera_distortions.data(); }
KEEP float*   ssv_ds_cam_dist()    { return g_ds.dist_coeffs.empty()? nullptr : g_ds.dist_coeffs.data(); } // [N,kCameraDistortionParams]
KEEP float*   ssv_ds_cam_c2w()     { return g_ds.c2w.empty()? nullptr : g_ds.c2w.data(); }               // [N,3,4]
KEEP int32_t* ssv_ds_cam_widths()  { return g_ds.widths.empty()? nullptr : g_ds.widths.data(); }
KEEP int32_t* ssv_ds_cam_heights() { return g_ds.heights.empty()? nullptr : g_ds.heights.data(); }
KEEP char*    ssv_ds_image_names() { return (char*)g_names.c_str(); }

KEEP void     ssv_ds_free()        { reset_result(); }

// ---------------------------------------------------------------------------
// Summary JSON: counts for the info panel.
// ---------------------------------------------------------------------------
KEEP char* ssv_ds_summary_json() {
    int N = (int)g_ds.num_cameras;
    static const char* kModel[4] = {"PINHOLE","FISHEYE","EQUISOLID","EQUIRECTANGULAR"};
    int model_counts[4] = {0,0,0,0};
    for (int m : g_ds.camera_models) if (m >= 0 && m < 4) model_counts[m]++;

    // Distinct intrinsic "groups" (model + resolution + intrinsics), with a
    // representative + image count each.
    struct Group { int model, w, h; float fx, fy, cx, cy; int count; };
    std::vector<Group> groups;
    auto same = [](const Group& g, int m,int w,int h,float fx,float fy,float cx,float cy){
        return g.model==m && g.w==w && g.h==h &&
               std::fabs(g.fx-fx)<1e-3f && std::fabs(g.fy-fy)<1e-3f &&
               std::fabs(g.cx-cx)<1e-3f && std::fabs(g.cy-cy)<1e-3f;
    };
    for (int i = 0; i < N; i++) {
        int m = g_ds.camera_models[i];
        int w = i < (int)g_ds.widths.size() ? g_ds.widths[i] : 0;
        int h = i < (int)g_ds.heights.size() ? g_ds.heights[i] : 0;
        float fx=0,fy=0,cx=0,cy=0;
        if (4*i+3 < (int)g_ds.intrins.size()) {
            fx=g_ds.intrins[4*i]; fy=g_ds.intrins[4*i+1];
            cx=g_ds.intrins[4*i+2]; cy=g_ds.intrins[4*i+3];
        }
        bool found = false;
        for (Group& g : groups) if (same(g,m,w,h,fx,fy,cx,cy)) { g.count++; found=true; break; }
        if (!found) groups.push_back({m,w,h,fx,fy,cx,cy,1});
    }

    g_json = "{\"format\":\"";
    json_escape(g_json, g_format);
    g_json += "\",\"num_images\":" + std::to_string(N);
    g_json += ",\"num_points\":" + std::to_string((long long)(g_pts.size() / 3));
    g_json += ",\"num_groups\":" + std::to_string(groups.size());
    g_json += ",\"models\":{";
    bool first = true;
    for (int m = 0; m < 4; m++) if (model_counts[m] > 0) {
        if (!first) g_json += ',';
        first = false;
        g_json += std::string("\"") + kModel[m] + "\":" + std::to_string(model_counts[m]);
    }
    g_json += "},\"groups\":[";
    for (size_t i = 0; i < groups.size(); i++) {
        const Group& g = groups[i];
        if (i) g_json += ',';
        const char* mn = (g.model>=0 && g.model<4) ? kModel[g.model] : "UNKNOWN";
        g_json += std::string("{\"model\":\"") + mn + "\",\"w\":" + std::to_string(g.w) +
                  ",\"h\":" + std::to_string(g.h) + ",\"count\":" + std::to_string(g.count) + "}";
    }
    g_json += "]}";
    return (char*)g_json.c_str();
}

// ---------------------------------------------------------------------------
// Fit sphere: the dsparse::CenterMode `mode` over points + cameras, and a
// radius about it -> (cx, cy, cz, radius). Mirrors ssv_fit_sphere in
// viewer.cpp.
// ---------------------------------------------------------------------------
KEEP float* ssv_ds_fit_sphere(int mode) {
    std::vector<float> pd, cd;          // squared distances: points / cameras
    const int64_t np = (int64_t)g_pts.size() / 3;
    const int64_t ncam = std::min<int64_t>(g_ds.num_cameras, (int64_t)g_ds.c2w.size() / 12);
    if (np == 0 && ncam == 0) { g_fit[0]=g_fit[1]=g_fit[2]=0.f; g_fit[3]=1.f; return g_fit; }
    {
        std::vector<double> c2w(g_ds.c2w.begin(), g_ds.c2w.begin() + ncam * 12);
        mode = std::max(0, std::min(mode, dsparse::kNumCenterModes - 1));
        const auto hint = dsparse::navigation_hint(c2w.data(), ncam);
        const std::array<double, 3> c = mode == 3 && hint.pivot_focus ? hint.focus : dsparse::scene_center(
            (dsparse::CenterMode)mode, c2w.data(), ncam, g_pts.data(), np, 3, 1 << 20);
        for (int k = 0; k < 3; k++) g_fit[k] = (float)c[k];
    }
    auto quantile = [](std::vector<float>& v, float q)->float{
        if (v.empty()) return 0.f;
        size_t k = (size_t)(q * (v.size()-1));
        std::nth_element(v.begin(), v.begin()+k, v.end());
        return v[k];
    };
    int64_t step = np > (1<<20) ? np / (1<<20) : 1; if (step < 1) step = 1;
    for (int64_t i = 0; i < np; i += step) {
        float dx=g_pts[i*3]-g_fit[0], dy=g_pts[i*3+1]-g_fit[1], dz=g_pts[i*3+2]-g_fit[2];
        pd.push_back(dx*dx+dy*dy+dz*dz);
    }
    for (int64_t i = 0; i < ncam; i++) {
        float dx=g_ds.c2w[12*i+3]-g_fit[0], dy=g_ds.c2w[12*i+7]-g_fit[1], dz=g_ds.c2w[12*i+11]-g_fit[2];
        cd.push_back(dx*dx+dy*dy+dz*dz);
    }
    // Frame the capture volume: points are dense (median), cameras are the
    // outer boundary we still want in view (90th percentile, robust to a stray
    // pose). Radius is the larger so cameras aren't dominated by dense points.
    float rp = pd.empty() ? 0.f : std::sqrt(quantile(pd, 0.5f));
    float rc = cd.empty() ? 0.f : std::sqrt(quantile(cd, 0.9f));
    g_fit[3] = std::max(rp, rc);
    if (!(g_fit[3] > 0.f)) g_fit[3] = 1.f;
    return g_fit;
}

// Base frustum size in the dataset's frame (js/main.js multiplies in the slider).
KEEP float ssv_ds_frustum_size() {
    int64_t n = std::min<int64_t>(g_ds.num_cameras, (int64_t)g_ds.c2w.size() / 12);
    return (float)camhost::frustum_display_size(g_ds.c2w.data(), n);
}

// Nearest point to a ray (model-native frame): returns (index, t, perp) so JS
// can accept the hit against a screen-space threshold. index = -1 if empty.
KEEP float* ssv_ds_pick_point(float ox, float oy, float oz,
                              float dx, float dy, float dz) {
    g_pick[0] = -1.f; g_pick[1] = 0.f; g_pick[2] = 3.4e38f;
    const auto& P = g_pts;
    int64_t np = (int64_t)g_pts.size() / 3;
    float dd = dx*dx + dy*dy + dz*dz; if (dd < 1e-20f) return g_pick;
    float best_score = 3.4e38f;
    for (int64_t i = 0; i < np; i++) {
        float rx=P[i*3]-ox, ry=P[i*3+1]-oy, rz=P[i*3+2]-oz;
        float t = (rx*dx+ry*dy+rz*dz)/dd;
        if (t <= 0.f) continue;
        float px=rx-t*dx, py=ry-t*dy, pz=rz-t*dz;
        float perp = std::sqrt(px*px+py*py+pz*pz);
        float score = perp / t;              // angular distance
        if (score < best_score) {
            best_score = score;
            g_pick[0] = (float)i; g_pick[1] = t; g_pick[2] = perp;
        }
    }
    return g_pick;
}

// 0 fly, 1 orbit about the camera median, 2 orbit about the camera focus.
KEEP int ssv_ds_navigation() {
    const auto hint = dsparse::navigation_hint(g_ds.c2w.data(), g_ds.num_cameras);
    return !hint.orbit ? 0 : hint.pivot_focus ? 2 : 1;
}

KEEP int ssv_ds_num_validation() { return (int)g_ds.val_indices.size(); }
KEEP int32_t* ssv_ds_validation() { return g_ds.val_indices.data(); }

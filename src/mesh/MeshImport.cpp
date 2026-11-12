// MeshImport.cpp -- see MeshImport.h.

#include "core/BufferedFile.h"
#include "mesh/MeshImport.h"

#include "data/Json.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// Declarations only; the implementation is external/stb_image_impl.cpp,
// which both build systems already compile.
#include "external/stb_image.h"

namespace meshing {

namespace {

std::string lower_ext(const std::string& path) {
    const size_t dot = path.find_last_of('.');
    const size_t slash = path.find_last_of("/\\");
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return "";
    std::string e = path.substr(dot);
    for (char& c : e) c = (char)std::tolower((unsigned char)c);
    return e;
}

std::string dir_of(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? std::string(".")
                                      : path.substr(0, slash);
}

std::string join_path(const std::string& dir, const std::string& name) {
    if (name.empty()) return name;
    // An absolute (or drive-qualified) reference stands on its own.
    if (name[0] == '/' || name[0] == '\\' ||
        (name.size() > 2 && name[1] == ':'))
        return name;
    return dir + "/" + name;
}

std::string read_file(const std::string& path, bool binary, bool* ok) {
    spirula::BufferedInputFile f(path, binary ? std::ios::binary : std::ios::in);
    if (!f) { if (ok) *ok = false; return {}; }
    std::ostringstream ss;
    ss << f.rdbuf();
    if (ok) *ok = true;
    return ss.str();
}

// Decode an encoded image (PNG/JPEG) into mesh.texture as RGB8, row 0 = top.
bool load_texture_bytes(const unsigned char* data, size_t n, MeshData& mesh) {
    int w = 0, h = 0, comp = 0;
    unsigned char* px =
        stbi_load_from_memory(data, (int)n, &w, &h, &comp, 3);
    if (!px) return false;
    mesh.tex_width = w;
    mesh.tex_height = h;
    mesh.texture.assign(px, px + (size_t)w * h * 3);
    stbi_image_free(px);
    return true;
}

bool load_texture_file(const std::string& path, MeshData& mesh) {
    bool ok = false;
    std::string bytes = read_file(path, /*binary=*/true, &ok);
    if (!ok || bytes.empty()) return false;
    return load_texture_bytes((const unsigned char*)bytes.data(), bytes.size(),
                              mesh);
}

// Fan-triangulate a polygon given as vertex indices.
void add_polygon(MeshData& mesh, const std::vector<int>& poly) {
    for (size_t k = 2; k < poly.size(); ++k)
        mesh.F.push_back({poly[0], poly[k - 1], poly[k]});
}

// ===========================================================================
// PLY -- ascii and binary either endianness, any property order, float or
// double positions and normals, uchar or float colors with or without alpha,
// s,t / u,v / texture_u,texture_v UVs, polygons fan-triangulated.
// ===========================================================================

struct PlyProp {
    std::string name;
    int type = 0;        // index into kPlyTypes
    bool is_list = false;
    int count_type = 0;  // list length type
};

struct PlyElement {
    std::string name;
    int64_t count = 0;
    std::vector<PlyProp> props;
};

const char* kPlyTypes[] = {"char", "uchar", "short",  "ushort", "int",
                           "uint", "float", "double", "int8",   "uint8",
                           "int16", "uint16", "int32", "uint32", "float32",
                           "float64"};
constexpr int kPlyTypeCount = (int)(sizeof(kPlyTypes) / sizeof(kPlyTypes[0]));

int ply_type_index(const std::string& s) {
    for (int i = 0; i < kPlyTypeCount; ++i)
        if (s == kPlyTypes[i]) return i;
    return -1;
}

// Canonical size in bytes for a type index.
int ply_type_size(int t) {
    switch (t) {
        case 0: case 1: case 8: case 9: return 1;                  // char/uchar
        case 2: case 3: case 10: case 11: return 2;                // short
        case 4: case 5: case 12: case 13: return 4;                // int
        case 6: case 14: return 4;                                 // float
        case 7: case 15: return 8;                                 // double
        default: return 0;
    }
}

bool ply_type_is_float(int t) {
    return t == 6 || t == 7 || t == 14 || t == 15;
}
bool ply_type_is_signed(int t) {
    return t == 0 || t == 2 || t == 4 || t == 8 || t == 10 || t == 12;
}

// Parse the header. Returns the byte offset just past "end_header\n", or 0 on
// failure. `format`: 0 ascii, 1 binary LE, 2 binary BE.
size_t ply_parse_header(const std::string& buf,
                        std::vector<PlyElement>& elements, int& format,
                        std::string& error) {
    std::istringstream in(buf);
    std::string line;
    if (!std::getline(in, line)) { error = "empty file"; return 0; }
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
        line.pop_back();
    if (line != "ply") { error = "not a PLY file"; return 0; }
    format = -1;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();
        std::istringstream ls(line);
        std::string kw;
        ls >> kw;
        if (kw == "comment" || kw == "obj_info") continue;
        if (kw == "format") {
            std::string f;
            ls >> f;
            if (f == "ascii") format = 0;
            else if (f == "binary_little_endian") format = 1;
            else if (f == "binary_big_endian") format = 2;
            else { error = "unknown PLY format '" + f + "'"; return 0; }
        } else if (kw == "element") {
            PlyElement e;
            ls >> e.name >> e.count;
            elements.push_back(e);
        } else if (kw == "property") {
            if (elements.empty()) { error = "property before element"; return 0; }
            std::string t;
            ls >> t;
            PlyProp p;
            if (t == "list") {
                std::string ct, vt;
                ls >> ct >> vt >> p.name;
                p.is_list = true;
                p.count_type = ply_type_index(ct);
                p.type = ply_type_index(vt);
                if (p.count_type < 0 || p.type < 0) {
                    error = "unknown PLY list type";
                    return 0;
                }
            } else {
                p.type = ply_type_index(t);
                ls >> p.name;
                if (p.type < 0) {
                    error = "unknown PLY property type '" + t + "'";
                    return 0;
                }
            }
            elements.back().props.push_back(p);
        } else if (kw == "end_header") {
            if (format < 0) { error = "PLY header has no format line"; return 0; }
            return (size_t)in.tellg();
        }
    }
    error = "PLY header has no end_header";
    return 0;
}

struct BinReader {
    const unsigned char* p;
    const unsigned char* end;
    bool big_endian;
    bool ok = true;

    double read(int type) {
        const int n = ply_type_size(type);
        if (p + n > end) { ok = false; return 0.0; }
        unsigned char tmp[8];
        std::memcpy(tmp, p, n);
        p += n;
        if (big_endian)
            for (int i = 0; i < n / 2; ++i) std::swap(tmp[i], tmp[n - 1 - i]);
        if (ply_type_is_float(type)) {
            if (n == 4) { float v; std::memcpy(&v, tmp, 4); return (double)v; }
            double v; std::memcpy(&v, tmp, 8); return v;
        }
        if (ply_type_is_signed(type)) {
            switch (n) {
                case 1: return (double)(int8_t)tmp[0];
                case 2: { int16_t v; std::memcpy(&v, tmp, 2); return (double)v; }
                default: { int32_t v; std::memcpy(&v, tmp, 4); return (double)v; }
            }
        }
        switch (n) {
            case 1: return (double)tmp[0];
            case 2: { uint16_t v; std::memcpy(&v, tmp, 2); return (double)v; }
            default: { uint32_t v; std::memcpy(&v, tmp, 4); return (double)v; }
        }
    }
};

// Which vertex property each slot comes from; -1 = absent.
struct PlyVertexMap {
    int x = -1, y = -1, z = -1;
    int nx = -1, ny = -1, nz = -1;
    int r = -1, g = -1, b = -1;
    int u = -1, v = -1;
    bool color_is_float = false;
};

PlyVertexMap ply_map_vertex(const PlyElement& e) {
    PlyVertexMap m;
    for (size_t i = 0; i < e.props.size(); ++i) {
        const std::string& n = e.props[i].name;
        const int idx = (int)i;
        if (n == "x") m.x = idx;
        else if (n == "y") m.y = idx;
        else if (n == "z") m.z = idx;
        else if (n == "nx") m.nx = idx;
        else if (n == "ny") m.ny = idx;
        else if (n == "nz") m.nz = idx;
        else if (n == "red" || n == "r" || n == "diffuse_red") m.r = idx;
        else if (n == "green" || n == "g" || n == "diffuse_green") m.g = idx;
        else if (n == "blue" || n == "b" || n == "diffuse_blue") m.b = idx;
        else if (n == "s" || n == "u" || n == "texture_u" || n == "texture_s")
            m.u = idx;
        else if (n == "t" || n == "v" || n == "texture_v" || n == "texture_t")
            m.v = idx;
    }
    if (m.r >= 0) m.color_is_float = ply_type_is_float(e.props[m.r].type);
    return m;
}

bool read_ply(const std::string& path, MeshData& out, std::string& error) {
    bool ok = false;
    const std::string buf = read_file(path, /*binary=*/true, &ok);
    if (!ok) { error = "cannot open " + path; return false; }

    std::vector<PlyElement> elements;
    int format = -1;
    const size_t body = ply_parse_header(buf, elements, format, error);
    if (body == 0) return false;

    const unsigned char* p = (const unsigned char*)buf.data() + body;
    const unsigned char* end = (const unsigned char*)buf.data() + buf.size();
    std::istringstream ascii;
    if (format == 0) ascii.str(buf.substr(body));

    BinReader br{p, end, format == 2};

    auto next_scalar = [&](int type) -> double {
        if (format == 0) {
            double v = 0.0;
            ascii >> v;
            return v;
        }
        return br.read(type);
    };

    for (const PlyElement& e : elements) {
        const bool is_vertex = e.name == "vertex";
        const bool is_face = e.name == "face";
        PlyVertexMap vm;
        if (is_vertex) {
            vm = ply_map_vertex(e);
            if (vm.x < 0 || vm.y < 0 || vm.z < 0) {
                error = "PLY vertex element has no x/y/z";
                return false;
            }
            out.V.reserve((size_t)e.count);
            if (vm.nx >= 0) out.N.reserve((size_t)e.count);
            if (vm.r >= 0) out.C.reserve((size_t)e.count);
            if (vm.u >= 0) out.UV.reserve((size_t)e.count);
        }
        std::vector<double> vals(e.props.size());

        for (int64_t i = 0; i < e.count; ++i) {
            if (is_face) {
                // A face row is the list property plus whatever else the
                // element carries (per-face UVs, material ids -- skipped).
                std::vector<int> poly;
                for (const PlyProp& pr : e.props) {
                    if (pr.is_list) {
                        const int n = (int)next_scalar(pr.count_type);
                        for (int k = 0; k < n; ++k) {
                            double v = next_scalar(pr.type);
                            if (pr.name == "vertex_indices" ||
                                pr.name == "vertex_index")
                                poly.push_back((int)v);
                        }
                    } else {
                        next_scalar(pr.type);
                    }
                }
                if (poly.size() >= 3) add_polygon(out, poly);
                continue;
            }

            for (size_t k = 0; k < e.props.size(); ++k) {
                const PlyProp& pr = e.props[k];
                if (pr.is_list) {
                    const int n = (int)next_scalar(pr.count_type);
                    for (int j = 0; j < n; ++j) next_scalar(pr.type);
                    vals[k] = 0.0;
                } else {
                    vals[k] = next_scalar(pr.type);
                }
            }
            if (!is_vertex) continue;

            out.V.push_back({(float)vals[vm.x], (float)vals[vm.y],
                             (float)vals[vm.z]});
            if (vm.nx >= 0 && vm.ny >= 0 && vm.nz >= 0)
                out.N.push_back({(float)vals[vm.nx], (float)vals[vm.ny],
                                 (float)vals[vm.nz]});
            if (vm.r >= 0 && vm.g >= 0 && vm.b >= 0) {
                const float s = vm.color_is_float ? 255.0f : 1.0f;
                auto q = [&](double v) {
                    float f = (float)v * s;
                    return (unsigned char)std::min(255.0f, std::max(0.0f, f));
                };
                out.C.push_back({q(vals[vm.r]), q(vals[vm.g]), q(vals[vm.b])});
            }
            if (vm.u >= 0 && vm.v >= 0)
                out.UV.push_back({(float)vals[vm.u], (float)vals[vm.v]});
        }
        if (format != 0 && !br.ok) {
            error = "PLY body ended early (truncated file?)";
            return false;
        }
    }

    if (out.V.empty()) { error = "PLY has no vertices"; return false; }
    if (out.F.empty()) { error = "PLY has no faces (not a mesh)"; return false; }
    return true;
}

// ===========================================================================
// OBJ -- v / vn / vt / f in every index form, negative (relative) indices,
// polygons fan-triangulated, the first map_Kd of the .mtl as the texture.
// ===========================================================================

// The first map_Kd of an .mtl, resolved against the .mtl's own directory.
std::string mtl_first_texture(const std::string& mtl_path) {
    bool ok = false;
    const std::string text = read_file(mtl_path, /*binary=*/false, &ok);
    if (!ok) return {};
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string kw;
        ls >> kw;
        if (kw != "map_Kd") continue;
        // Skip any option flags (-bm 1, -o u v w, ...); the last token is the
        // file name, which may contain spaces.
        std::string rest;
        std::getline(ls, rest);
        size_t s = rest.find_first_not_of(" \t");
        if (s == std::string::npos) continue;
        rest = rest.substr(s);
        while (!rest.empty() && rest[0] == '-') {
            const size_t sp = rest.find_first_of(" \t");
            if (sp == std::string::npos) { rest.clear(); break; }
            size_t nx = rest.find_first_not_of(" \t", sp);
            if (nx == std::string::npos) { rest.clear(); break; }
            rest = rest.substr(nx);
            // one flag argument at minimum; the loop re-checks for a '-'
            const size_t sp2 = rest.find_first_of(" \t");
            if (sp2 == std::string::npos) break;
            const size_t nx2 = rest.find_first_not_of(" \t", sp2);
            if (nx2 == std::string::npos) break;
            rest = rest.substr(nx2);
        }
        while (!rest.empty() && (rest.back() == '\r' || rest.back() == ' '))
            rest.pop_back();
        if (!rest.empty()) return join_path(dir_of(mtl_path), rest);
    }
    return {};
}

bool read_obj(const std::string& path, MeshData& out, std::string& error) {
    bool ok = false;
    const std::string text = read_file(path, /*binary=*/false, &ok);
    if (!ok) { error = "cannot open " + path; return false; }

    std::vector<std::array<float, 3>> pos, nrm;
    std::vector<std::array<float, 2>> uv;
    // An OBJ vertex is a (v, vt, vn) triple; the mesh vertex is that triple,
    // so a seam vertex referenced with two different UVs becomes two.
    struct Key { int v, t, n; };
    std::vector<Key> keys;
    // Small open-addressed map from the triple to an output index.
    std::vector<int> table;
    size_t table_mask = 0;
    auto rehash = [&](size_t want) {
        size_t cap = 1024;
        while (cap < want * 2) cap <<= 1;
        table.assign(cap, -1);
        table_mask = cap - 1;
        for (size_t i = 0; i < keys.size(); ++i) {
            const Key& k = keys[i];
            uint64_t h = ((uint64_t)(uint32_t)k.v * 0x9e3779b97f4a7c15ull) ^
                         ((uint64_t)(uint32_t)k.t * 0xc2b2ae3d27d4eb4full) ^
                         ((uint64_t)(uint32_t)k.n * 0x165667b19e3779f9ull);
            size_t s = (size_t)(h & table_mask);
            while (table[s] >= 0) s = (s + 1) & table_mask;
            table[s] = (int)i;
        }
    };
    rehash(1024);

    auto intern = [&](int vi, int ti, int ni) -> int {
        uint64_t h = ((uint64_t)(uint32_t)vi * 0x9e3779b97f4a7c15ull) ^
                     ((uint64_t)(uint32_t)ti * 0xc2b2ae3d27d4eb4full) ^
                     ((uint64_t)(uint32_t)ni * 0x165667b19e3779f9ull);
        size_t s = (size_t)(h & table_mask);
        while (table[s] >= 0) {
            const Key& k = keys[table[s]];
            if (k.v == vi && k.t == ti && k.n == ni) return table[s];
            s = (s + 1) & table_mask;
        }
        const int idx = (int)keys.size();
        keys.push_back({vi, ti, ni});
        out.V.push_back(pos[(size_t)vi]);
        if (ni >= 0 && (size_t)ni < nrm.size()) out.N.push_back(nrm[(size_t)ni]);
        if (ti >= 0 && (size_t)ti < uv.size()) {
            // OBJ v runs bottom-up; MeshData UVs are top-left origin.
            std::array<float, 2> t = uv[(size_t)ti];
            out.UV.push_back({t[0], 1.0f - t[1]});
        }
        if (keys.size() * 2 > table.size()) rehash(keys.size() * 2);
        else table[s] = idx;
        return idx;
    };

    std::string mtllib;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ls(line);
        std::string kw;
        ls >> kw;
        if (kw == "v") {
            float x = 0, y = 0, z = 0;
            ls >> x >> y >> z;
            pos.push_back({x, y, z});
        } else if (kw == "vn") {
            float x = 0, y = 0, z = 0;
            ls >> x >> y >> z;
            nrm.push_back({x, y, z});
        } else if (kw == "vt") {
            float u = 0, v = 0;
            ls >> u >> v;
            uv.push_back({u, v});
        } else if (kw == "mtllib") {
            std::string rest;
            std::getline(ls, rest);
            size_t s = rest.find_first_not_of(" \t");
            if (s != std::string::npos) {
                rest = rest.substr(s);
                while (!rest.empty() && (rest.back() == '\r' || rest.back() == ' '))
                    rest.pop_back();
                mtllib = rest;
            }
        } else if (kw == "f") {
            std::vector<int> poly;
            std::string tok;
            while (ls >> tok) {
                int vi = 0, ti = 0, ni = 0;
                // v, v/vt, v//vn, v/vt/vn
                const size_t s1 = tok.find('/');
                if (s1 == std::string::npos) {
                    vi = std::atoi(tok.c_str());
                } else {
                    vi = std::atoi(tok.substr(0, s1).c_str());
                    const size_t s2 = tok.find('/', s1 + 1);
                    if (s2 == std::string::npos) {
                        ti = std::atoi(tok.substr(s1 + 1).c_str());
                    } else {
                        if (s2 > s1 + 1)
                            ti = std::atoi(tok.substr(s1 + 1, s2 - s1 - 1).c_str());
                        ni = std::atoi(tok.substr(s2 + 1).c_str());
                    }
                }
                auto resolve = [](int i, size_t n) -> int {
                    if (i > 0) return i - 1;
                    if (i < 0) return (int)n + i;
                    return -1;
                };
                const int rv = resolve(vi, pos.size());
                if (rv < 0 || (size_t)rv >= pos.size()) continue;
                poly.push_back(intern(rv, resolve(ti, uv.size()),
                                      resolve(ni, nrm.size())));
            }
            if (poly.size() >= 3) add_polygon(out, poly);
        }
    }

    if (out.V.empty()) { error = "OBJ has no vertices"; return false; }
    if (out.F.empty()) { error = "OBJ has no faces"; return false; }
    // A partial normal/UV set would mis-index; only keep complete ones.
    if (out.N.size() != out.V.size()) out.N.clear();
    if (out.UV.size() != out.V.size()) out.UV.clear();

    if (!mtllib.empty()) {
        const std::string tex =
            mtl_first_texture(join_path(dir_of(path), mtllib));
        if (!tex.empty()) load_texture_file(tex, out);
    }
    return true;
}

// ===========================================================================
// glTF / GLB -- POSITION / NORMAL / COLOR_0 / TEXCOORD_0 / indices in every
// component type the spec allows, every primitive of every mesh merged into
// one with node transforms applied. Draco, sparse accessors, animation: no.
// ===========================================================================

struct GltfBuffers {
    // One blob per glTF buffer (the GLB BIN chunk, or an external .bin, or a
    // decoded data: URI).
    std::vector<std::string> blobs;
};

// Decode the base64 payload of a data: URI.
std::string decode_data_uri(const std::string& uri) {
    const size_t comma = uri.find(',');
    if (comma == std::string::npos) return {};
    const std::string b64 = uri.substr(comma + 1);
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    std::string out;
    out.reserve(b64.size() * 3 / 4);
    int acc = 0, bits = 0;
    for (char c : b64) {
        const int v = val(c);
        if (v < 0) continue;
        acc = (acc << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((char)((acc >> bits) & 0xff));
        }
    }
    return out;
}

int gltf_component_size(int ct) {
    switch (ct) {
        case 5120: case 5121: return 1;   // byte / ubyte
        case 5122: case 5123: return 2;   // short / ushort
        case 5125: case 5126: return 4;   // uint / float
        default: return 0;
    }
}

int gltf_type_count(const std::string& t) {
    if (t == "SCALAR") return 1;
    if (t == "VEC2") return 2;
    if (t == "VEC3") return 3;
    if (t == "VEC4") return 4;
    if (t == "MAT4") return 16;
    return 0;
}

// Read accessor `idx` as doubles, `comps` values per element.
bool gltf_read_accessor(const JsonValue& root, const GltfBuffers& bufs, int idx,
                        std::vector<double>& out, int& comps, bool& normalized,
                        std::string& error) {
    const JsonValue* accs = root.find("accessors");
    if (!accs || !accs->is_array() || idx < 0 ||
        idx >= (int)accs->arr.size()) {
        error = "glTF: accessor index out of range";
        return false;
    }
    const JsonValue& a = accs->arr[(size_t)idx];
    const int ct = (int)a.find("componentType")->as_int(0);
    const int64_t count = a.find("count") ? a.find("count")->as_int(0) : 0;
    const JsonValue* tv = a.find("type");
    comps = gltf_type_count(tv ? tv->as_string() : "");
    normalized = a.find("normalized") && a.find("normalized")->as_bool(false);
    const int csz = gltf_component_size(ct);
    if (!csz || !comps) { error = "glTF: unsupported accessor type"; return false; }
    if (a.has("sparse")) { error = "glTF: sparse accessors are not read"; return false; }

    const JsonValue* bvi = a.find("bufferView");
    if (!bvi) {  // no bufferView == all zeros, per the spec
        out.assign((size_t)count * comps, 0.0);
        return true;
    }
    const JsonValue* bvs = root.find("bufferViews");
    if (!bvs || !bvs->is_array() ||
        bvi->as_int(-1) >= (int64_t)bvs->arr.size()) {
        error = "glTF: bufferView index out of range";
        return false;
    }
    const JsonValue& bv = bvs->arr[(size_t)bvi->as_int(0)];
    const int64_t bidx = bv.find("buffer") ? bv.find("buffer")->as_int(0) : 0;
    if (bidx < 0 || bidx >= (int64_t)bufs.blobs.size()) {
        error = "glTF: buffer index out of range";
        return false;
    }
    const std::string& blob = bufs.blobs[(size_t)bidx];
    const int64_t bv_off = bv.find("byteOffset") ? bv.find("byteOffset")->as_int(0) : 0;
    const int64_t acc_off = a.find("byteOffset") ? a.find("byteOffset")->as_int(0) : 0;
    const int64_t stride =
        bv.find("byteStride") ? bv.find("byteStride")->as_int(0) : 0;
    const int64_t elem = (int64_t)csz * comps;
    const int64_t step = stride > 0 ? stride : elem;
    const int64_t base = bv_off + acc_off;

    if (base + (count > 0 ? (count - 1) * step + elem : 0) > (int64_t)blob.size()) {
        error = "glTF: accessor runs past the end of its buffer";
        return false;
    }
    out.resize((size_t)count * comps);
    const unsigned char* p = (const unsigned char*)blob.data() + base;
    for (int64_t i = 0; i < count; ++i) {
        const unsigned char* e = p + i * step;
        for (int c = 0; c < comps; ++c) {
            const unsigned char* q = e + (int64_t)c * csz;
            double v = 0.0;
            switch (ct) {
                case 5120: { int8_t t; std::memcpy(&t, q, 1);
                             v = normalized ? std::max(t / 127.0, -1.0) : (double)t; break; }
                case 5121: { uint8_t t; std::memcpy(&t, q, 1);
                             v = normalized ? t / 255.0 : (double)t; break; }
                case 5122: { int16_t t; std::memcpy(&t, q, 2);
                             v = normalized ? std::max(t / 32767.0, -1.0) : (double)t; break; }
                case 5123: { uint16_t t; std::memcpy(&t, q, 2);
                             v = normalized ? t / 65535.0 : (double)t; break; }
                case 5125: { uint32_t t; std::memcpy(&t, q, 4); v = (double)t; break; }
                default:   { float t; std::memcpy(&t, q, 4); v = (double)t; break; }
            }
            out[(size_t)i * comps + c] = v;
        }
    }
    return true;
}

// glTF stores COLOR_0 as a LINEAR multiplier; MeshData::C is display sRGB,
// which is what MeshExport linearized on the way out.
unsigned char linear_to_srgb8(double c) {
    c = std::min(1.0, std::max(0.0, c));
    const double s = c <= 0.0031308 ? 12.92 * c
                                    : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
    return (unsigned char)std::lround(std::min(1.0, std::max(0.0, s)) * 255.0);
}

void mul_mat4_point(const double m[16], const float in[3], float out[3]) {
    // column-major (glTF), point (w = 1)
    for (int r = 0; r < 3; ++r)
        out[r] = (float)(m[r] * in[0] + m[4 + r] * in[1] + m[8 + r] * in[2] +
                         m[12 + r]);
}
void mul_mat4_dir(const double m[16], const float in[3], float out[3]) {
    for (int r = 0; r < 3; ++r)
        out[r] = (float)(m[r] * in[0] + m[4 + r] * in[1] + m[8 + r] * in[2]);
}
void mat4_mul(const double a[16], const double b[16], double o[16]) {
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r) {
            double s = 0.0;
            for (int k = 0; k < 4; ++k) s += a[k * 4 + r] * b[c * 4 + k];
            o[c * 4 + r] = s;
        }
}
void node_local_matrix(const JsonValue& n, double m[16]) {
    for (int i = 0; i < 16; ++i) m[i] = (i % 5 == 0) ? 1.0 : 0.0;
    if (const JsonValue* mm = n.find("matrix")) {
        if (mm->is_array() && mm->arr.size() == 16)
            for (int i = 0; i < 16; ++i) m[i] = mm->arr[(size_t)i].as_double(0);
        return;
    }
    double t[3] = {0, 0, 0}, s[3] = {1, 1, 1}, q[4] = {0, 0, 0, 1};
    if (const JsonValue* v = n.find("translation"))
        for (int i = 0; i < 3 && i < (int)v->arr.size(); ++i)
            t[i] = v->arr[(size_t)i].as_double(0);
    if (const JsonValue* v = n.find("scale"))
        for (int i = 0; i < 3 && i < (int)v->arr.size(); ++i)
            s[i] = v->arr[(size_t)i].as_double(1);
    if (const JsonValue* v = n.find("rotation"))
        for (int i = 0; i < 4 && i < (int)v->arr.size(); ++i)
            q[i] = v->arr[(size_t)i].as_double(i == 3 ? 1 : 0);
    const double x = q[0], y = q[1], z = q[2], w = q[3];
    const double r[9] = {
        1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w),
        2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w),
        2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y)};
    for (int c = 0; c < 3; ++c)
        for (int rr = 0; rr < 3; ++rr) m[c * 4 + rr] = r[c * 3 + rr] * s[c];
    m[12] = t[0]; m[13] = t[1]; m[14] = t[2];
}

bool read_gltf(const std::string& path, MeshData& out, std::string& error) {
    const std::string ext = lower_ext(path);
    JsonValue root;
    GltfBuffers bufs;
    std::string glb_bin;

    bool ok = false;
    const std::string raw = read_file(path, /*binary=*/true, &ok);
    if (!ok) { error = "cannot open " + path; return false; }

    if (ext == ".glb") {
        if (raw.size() < 12 || std::memcmp(raw.data(), "glTF", 4) != 0) {
            error = "not a GLB file";
            return false;
        }
        size_t off = 12;
        std::string json_chunk;
        while (off + 8 <= raw.size()) {
            uint32_t len = 0, type = 0;
            std::memcpy(&len, raw.data() + off, 4);
            std::memcpy(&type, raw.data() + off + 4, 4);
            off += 8;
            if (off + len > raw.size()) break;
            if (type == 0x4E4F534Au) json_chunk = raw.substr(off, len);       // JSON
            else if (type == 0x004E4942u) glb_bin = raw.substr(off, len);     // BIN
            off += len;
            off = (off + 3) & ~(size_t)3;
        }
        if (json_chunk.empty()) { error = "GLB has no JSON chunk"; return false; }
        try {
            root = json_parse(json_chunk);
        } catch (const std::exception& e) {
            error = std::string("GLB JSON: ") + e.what();
            return false;
        }
    } else {
        try {
            root = json_parse(raw);
        } catch (const std::exception& e) {
            error = std::string("glTF JSON: ") + e.what();
            return false;
        }
    }

    // ---- buffers ----
    if (const JsonValue* bl = root.find("buffers")) {
        for (const JsonValue& b : bl->arr) {
            const JsonValue* uri = b.find("uri");
            if (!uri || uri->is_null()) {
                bufs.blobs.push_back(glb_bin);   // the GLB BIN chunk
            } else if (uri->as_string().rfind("data:", 0) == 0) {
                bufs.blobs.push_back(decode_data_uri(uri->as_string()));
            } else {
                bool bok = false;
                bufs.blobs.push_back(read_file(
                    join_path(dir_of(path), uri->as_string()), true, &bok));
                if (!bok) {
                    error = "glTF: cannot open buffer " + uri->as_string();
                    return false;
                }
            }
        }
    }

    const JsonValue* meshes = root.find("meshes");
    if (!meshes || !meshes->is_array() || meshes->arr.empty()) {
        error = "glTF has no meshes";
        return false;
    }

    // ---- node transforms: walk the scene, so a mesh under a transformed
    //      node lands where it is drawn. Meshes no node references are still
    //      read (identity), which is what a viewer wants from a stray file.
    const JsonValue* nodes = root.find("nodes");
    std::vector<std::vector<double>> mesh_xform(meshes->arr.size());
    std::vector<bool> mesh_placed(meshes->arr.size(), false);
    if (nodes && nodes->is_array()) {
        struct Item { int node; double m[16]; };
        std::vector<Item> stack;
        double ident[16];
        for (int i = 0; i < 16; ++i) ident[i] = (i % 5 == 0) ? 1.0 : 0.0;
        // Roots: the default scene's nodes, else every node (a flat file).
        std::vector<int> roots;
        const JsonValue* scenes = root.find("scenes");
        const int scene_idx =
            root.find("scene") ? (int)root.find("scene")->as_int(0) : 0;
        if (scenes && scenes->is_array() && scene_idx >= 0 &&
            scene_idx < (int)scenes->arr.size()) {
            if (const JsonValue* ns = scenes->arr[(size_t)scene_idx].find("nodes"))
                for (const JsonValue& v : ns->arr) roots.push_back((int)v.as_int(0));
        }
        if (roots.empty())
            for (size_t i = 0; i < nodes->arr.size(); ++i) roots.push_back((int)i);
        for (int r : roots) {
            Item it{r, {}};
            std::memcpy(it.m, ident, sizeof ident);
            stack.push_back(it);
        }
        // Depth-first; a malformed cyclic file is bounded by the node count.
        size_t guard = nodes->arr.size() * 8 + 64;
        while (!stack.empty() && guard-- > 0) {
            Item it = stack.back();
            stack.pop_back();
            if (it.node < 0 || it.node >= (int)nodes->arr.size()) continue;
            const JsonValue& n = nodes->arr[(size_t)it.node];
            double local[16], world[16];
            node_local_matrix(n, local);
            mat4_mul(it.m, local, world);
            if (const JsonValue* mi = n.find("mesh")) {
                const int m = (int)mi->as_int(-1);
                if (m >= 0 && m < (int)meshes->arr.size() && !mesh_placed[(size_t)m]) {
                    mesh_xform[(size_t)m].assign(world, world + 16);
                    mesh_placed[(size_t)m] = true;
                }
            }
            if (const JsonValue* ch = n.find("children"))
                for (const JsonValue& c : ch->arr) {
                    Item sub{(int)c.as_int(-1), {}};
                    std::memcpy(sub.m, world, sizeof world);
                    stack.push_back(sub);
                }
        }
    }

    int texture_source = -1;

    // ---- primitives ----
    for (size_t mi = 0; mi < meshes->arr.size(); ++mi) {
        const JsonValue* prims = meshes->arr[mi].find("primitives");
        if (!prims || !prims->is_array()) continue;
        const bool xf = mesh_placed[mi];
        const double* M = xf ? mesh_xform[mi].data() : nullptr;

        for (const JsonValue& pr : prims->arr) {
            // Only triangles (mode 4, or absent = 4).
            const JsonValue* mode = pr.find("mode");
            if (mode && mode->as_int(4) != 4) continue;
            const JsonValue* attrs = pr.find("attributes");
            if (!attrs) continue;
            const JsonValue* pos_i = attrs->find("POSITION");
            if (!pos_i) continue;

            std::vector<double> pos;
            int comps = 0;
            bool norm = false;
            if (!gltf_read_accessor(root, bufs, (int)pos_i->as_int(-1), pos,
                                    comps, norm, error))
                return false;
            if (comps != 3) continue;
            const size_t nv = pos.size() / 3;
            const size_t base = out.V.size();

            for (size_t i = 0; i < nv; ++i) {
                float p[3] = {(float)pos[3 * i], (float)pos[3 * i + 1],
                              (float)pos[3 * i + 2]};
                float q[3] = {p[0], p[1], p[2]};
                if (M) mul_mat4_point(M, p, q);
                out.V.push_back({q[0], q[1], q[2]});
            }

            // NORMAL
            if (const JsonValue* ni = attrs->find("NORMAL")) {
                std::vector<double> nn;
                if (gltf_read_accessor(root, bufs, (int)ni->as_int(-1), nn,
                                       comps, norm, error) &&
                    comps == 3 && nn.size() / 3 == nv) {
                    out.N.resize(base, {0.0f, 0.0f, 1.0f});
                    for (size_t i = 0; i < nv; ++i) {
                        float p[3] = {(float)nn[3 * i], (float)nn[3 * i + 1],
                                      (float)nn[3 * i + 2]};
                        float q[3] = {p[0], p[1], p[2]};
                        if (M) mul_mat4_dir(M, p, q);
                        const float len =
                            std::sqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2]);
                        const float s = len > 1e-20f ? 1.0f / len : 0.0f;
                        out.N.push_back({q[0]*s, q[1]*s, q[2]*s});
                    }
                }
            }
            // COLOR_0 (VEC3 or VEC4, linear)
            if (const JsonValue* ci = attrs->find("COLOR_0")) {
                std::vector<double> cc;
                if (gltf_read_accessor(root, bufs, (int)ci->as_int(-1), cc,
                                       comps, norm, error) &&
                    (comps == 3 || comps == 4) &&
                    cc.size() / (size_t)comps == nv) {
                    out.C.resize(base, {255, 255, 255});
                    for (size_t i = 0; i < nv; ++i)
                        out.C.push_back(
                            {linear_to_srgb8(cc[(size_t)comps * i + 0]),
                             linear_to_srgb8(cc[(size_t)comps * i + 1]),
                             linear_to_srgb8(cc[(size_t)comps * i + 2])});
                }
            }
            // TEXCOORD_0
            if (const JsonValue* ui = attrs->find("TEXCOORD_0")) {
                std::vector<double> uu;
                if (gltf_read_accessor(root, bufs, (int)ui->as_int(-1), uu,
                                       comps, norm, error) &&
                    comps == 2 && uu.size() / 2 == nv) {
                    out.UV.resize(base, {0.0f, 0.0f});
                    for (size_t i = 0; i < nv; ++i)
                        out.UV.push_back({(float)uu[2 * i], (float)uu[2 * i + 1]});
                }
            }

            // indices (or an implicit 0,1,2,... sequence)
            if (const JsonValue* ii = pr.find("indices")) {
                std::vector<double> idx;
                if (!gltf_read_accessor(root, bufs, (int)ii->as_int(-1), idx,
                                        comps, norm, error))
                    return false;
                for (size_t t = 0; t + 2 < idx.size(); t += 3)
                    out.F.push_back({(int)(base + (size_t)idx[t]),
                                     (int)(base + (size_t)idx[t + 1]),
                                     (int)(base + (size_t)idx[t + 2])});
            } else {
                for (size_t t = 0; t + 2 < nv; t += 3)
                    out.F.push_back({(int)(base + t), (int)(base + t + 1),
                                     (int)(base + t + 2)});
            }

            // baseColorTexture -> image source (first one wins)
            if (texture_source < 0) {
                const JsonValue* mati = pr.find("material");
                const JsonValue* mats = root.find("materials");
                if (mati && mats && mats->is_array() &&
                    mati->as_int(-1) >= 0 &&
                    mati->as_int(0) < (int64_t)mats->arr.size()) {
                    const JsonValue& mat = mats->arr[(size_t)mati->as_int(0)];
                    const JsonValue* pbr = mat.find("pbrMetallicRoughness");
                    const JsonValue* bct =
                        pbr ? pbr->find("baseColorTexture") : nullptr;
                    const JsonValue* ti = bct ? bct->find("index") : nullptr;
                    const JsonValue* texs = root.find("textures");
                    if (ti && texs && texs->is_array() &&
                        ti->as_int(-1) >= 0 &&
                        ti->as_int(0) < (int64_t)texs->arr.size()) {
                        const JsonValue* src =
                            texs->arr[(size_t)ti->as_int(0)].find("source");
                        if (src) texture_source = (int)src->as_int(-1);
                    }
                }
            }
        }
    }

    if (out.V.empty()) { error = "glTF has no readable geometry"; return false; }
    if (out.F.empty()) { error = "glTF has no triangles"; return false; }
    // Partial per-vertex arrays would mis-index (some primitives had the
    // attribute and some did not): pad or drop, whichever keeps them valid.
    if (!out.N.empty() && out.N.size() != out.V.size()) out.N.clear();
    if (!out.C.empty()) out.C.resize(out.V.size(), {255, 255, 255});
    if (!out.UV.empty()) out.UV.resize(out.V.size(), {0.0f, 0.0f});

    // ---- texture image ----
    const JsonValue* imgs = root.find("images");
    if (texture_source >= 0 && imgs && imgs->is_array() &&
        texture_source < (int)imgs->arr.size()) {
        const JsonValue& im = imgs->arr[(size_t)texture_source];
        if (const JsonValue* uri = im.find("uri")) {
            if (uri->as_string().rfind("data:", 0) == 0) {
                const std::string bytes = decode_data_uri(uri->as_string());
                load_texture_bytes((const unsigned char*)bytes.data(),
                                   bytes.size(), out);
            } else {
                load_texture_file(join_path(dir_of(path), uri->as_string()),
                                  out);
            }
        } else if (const JsonValue* bvi = im.find("bufferView")) {
            const JsonValue* bvs = root.find("bufferViews");
            if (bvs && bvs->is_array() && bvi->as_int(-1) >= 0 &&
                bvi->as_int(0) < (int64_t)bvs->arr.size()) {
                const JsonValue& bv = bvs->arr[(size_t)bvi->as_int(0)];
                const int64_t bidx =
                    bv.find("buffer") ? bv.find("buffer")->as_int(0) : 0;
                const int64_t off =
                    bv.find("byteOffset") ? bv.find("byteOffset")->as_int(0) : 0;
                const int64_t len =
                    bv.find("byteLength") ? bv.find("byteLength")->as_int(0) : 0;
                if (bidx >= 0 && bidx < (int64_t)bufs.blobs.size() &&
                    off + len <= (int64_t)bufs.blobs[(size_t)bidx].size())
                    load_texture_bytes(
                        (const unsigned char*)bufs.blobs[(size_t)bidx].data() + off,
                        (size_t)len, out);
            }
        }
    }
    return true;
}

// ===========================================================================
// STL (binary and ASCII)
//
// Geometry only, and one triangle is three vertices of its own: the file says
// nothing about which of them are the same point, so nothing here welds them.
// ===========================================================================

void stl_add_face(MeshData& out, const float v[9], const float n[3]) {
    const int base = (int)out.V.size();
    for (int i = 0; i < 3; ++i) {
        out.V.push_back({v[3*i], v[3*i+1], v[3*i+2]});
        out.N.push_back({n[0], n[1], n[2]});
    }
    out.F.push_back({base, base + 1, base + 2});
}

bool read_stl(const std::string& path, MeshData& out, std::string& error) {
    bool ok = false;
    const std::string blob = read_file(path, true, &ok);
    if (!ok) { error = "cannot open " + path; return false; }

    // Binary when the triangle count in the header accounts for the whole
    // file: an ASCII STL also starts with "solid", so the size is the test.
    if (blob.size() >= 84) {
        uint32_t n = 0;
        std::memcpy(&n, blob.data() + 80, 4);
        if ((uint64_t)n * 50u + 84u == (uint64_t)blob.size()) {
            out.V.reserve((size_t)n * 3);
            out.F.reserve(n);
            for (uint32_t t = 0; t < n; ++t) {
                const char* p = blob.data() + 84 + (size_t)t * 50;
                float nrm[3], v[9];
                std::memcpy(nrm, p, 12);
                std::memcpy(v, p + 12, 36);
                stl_add_face(out, v, nrm);
            }
            return true;
        }
    }

    std::istringstream in(blob);
    std::string word;
    float nrm[3] = {0, 0, 0}, v[9];
    int got = 0;
    while (in >> word) {
        for (char& c : word) c = (char)std::tolower((unsigned char)c);
        if (word == "facet") {
            if (!(in >> word)) break;               // "normal"
            in >> nrm[0] >> nrm[1] >> nrm[2];
            got = 0;
        } else if (word == "vertex") {
            if (got <= 6) in >> v[got] >> v[got+1] >> v[got+2];
            else { float junk; in >> junk >> junk >> junk; }
            got += 3;
        } else if (word == "endfacet") {
            if (got == 9) stl_add_face(out, v, nrm);
        }
    }
    if (out.F.empty()) { error = path + " holds no STL triangles"; return false; }
    return true;
}

}  // namespace

// ===========================================================================
// Public API
// ===========================================================================

bool is_mesh_path(const std::string& path) {
    const std::string e = lower_ext(path);
    return e == ".ply" || e == ".obj" || e == ".gltf" || e == ".glb" ||
           e == ".stl";
}

bool ply_is_mesh(const std::string& path) {
    if (lower_ext(path) != ".ply") return false;
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    // The header is text and short; 64 KB covers a pathological one.
    std::string head(65536, '\0');
    f.read(&head[0], (std::streamsize)head.size());
    head.resize((size_t)f.gcount());
    const size_t stop = head.find("end_header");
    if (stop == std::string::npos) return false;
    head.resize(stop);
    std::istringstream in(head);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string kw, name;
        int64_t n = 0;
        ls >> kw >> name >> n;
        if (kw == "element" && name == "face" && n > 0) return true;
    }
    return false;
}

bool read_mesh(const std::string& path, MeshData& out, std::string& error) {
    out = MeshData();
    error.clear();
    const std::string e = lower_ext(path);
    bool ok;
    if (e == ".ply") ok = read_ply(path, out, error);
    else if (e == ".obj") ok = read_obj(path, out, error);
    else if (e == ".stl") ok = read_stl(path, out, error);
    else if (e == ".gltf" || e == ".glb") ok = read_gltf(path, out, error);
    else { error = "unsupported mesh format '" + e + "'"; return false; }
    if (!ok) return false;

    // Drop faces that index outside the vertex array rather than letting a
    // malformed file crash a renderer downstream.
    const int nv = (int)out.V.size();
    size_t bad = 0;
    std::vector<std::array<int, 3>> keep;
    keep.reserve(out.F.size());
    for (const auto& f : out.F) {
        if (f[0] < 0 || f[1] < 0 || f[2] < 0 || f[0] >= nv || f[1] >= nv ||
            f[2] >= nv) { ++bad; continue; }
        keep.push_back(f);
    }
    if (bad) out.F.swap(keep);
    if (out.F.empty()) { error = "mesh has no valid triangles"; return false; }
    // Only a complete set is usable per-vertex.
    if (out.N.size() != out.V.size()) out.N.clear();
    if (out.C.size() != out.V.size()) out.C.clear();
    if (out.UV.size() != out.V.size()) out.UV.clear();
    if (out.texture.empty()) { out.tex_width = out.tex_height = 0; }
    return true;
}

void mesh_compute_normals(MeshData& mesh, bool force) {
    if (!force && mesh.N.size() == mesh.V.size()) return;
    mesh.N.assign(mesh.V.size(), {0.0f, 0.0f, 0.0f});
    for (const auto& f : mesh.F) {
        const auto& a = mesh.V[(size_t)f[0]];
        const auto& b = mesh.V[(size_t)f[1]];
        const auto& c = mesh.V[(size_t)f[2]];
        const float e1[3] = {b[0]-a[0], b[1]-a[1], b[2]-a[2]};
        const float e2[3] = {c[0]-a[0], c[1]-a[1], c[2]-a[2]};
        // un-normalized cross product == area-weighted face normal
        const float n[3] = {e1[1]*e2[2] - e1[2]*e2[1],
                            e1[2]*e2[0] - e1[0]*e2[2],
                            e1[0]*e2[1] - e1[1]*e2[0]};
        for (int k = 0; k < 3; ++k)
            for (int d = 0; d < 3; ++d) mesh.N[(size_t)f[k]][d] += n[d];
    }
    for (auto& n : mesh.N) {
        const float len = std::sqrt(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
        if (len > 1e-20f) { n[0] /= len; n[1] /= len; n[2] /= len; }
        else n = {0.0f, 0.0f, 1.0f};
    }
}

}  // namespace meshing

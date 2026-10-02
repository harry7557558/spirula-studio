// SplatPly.cpp -- see SplatPly.h.

#include "checkpoint/SplatPly.h"

#include "checkpoint/SplatTransform.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace spirula {

namespace {

// ---------------------------------------------------------------------------
// Header
// ---------------------------------------------------------------------------

enum class PlyType { F32, F64, I8, U8, I16, U16, I32, U32 };

struct PlyProp {
    std::string name;
    PlyType type = PlyType::F32;
    size_t offset = 0;      // bytes into a binary row
};

struct PlyHeader {
    bool ok = false;
    bool binary_le = false;
    bool ascii = false;
    int64_t num_vertex = 0;
    std::vector<PlyProp> props;
    size_t row_bytes = 0;
    std::streampos data_start = 0;
};

size_t type_size(PlyType t) {
    switch (t) {
        case PlyType::F64: return 8;
        case PlyType::I8: case PlyType::U8: return 1;
        case PlyType::I16: case PlyType::U16: return 2;
        default: return 4;
    }
}

bool parse_type(const std::string& s, PlyType& out) {
    if (s == "float"  || s == "float32") { out = PlyType::F32; return true; }
    if (s == "double" || s == "float64") { out = PlyType::F64; return true; }
    if (s == "char"   || s == "int8")    { out = PlyType::I8;  return true; }
    if (s == "uchar"  || s == "uint8")   { out = PlyType::U8;  return true; }
    if (s == "short"  || s == "int16")   { out = PlyType::I16; return true; }
    if (s == "ushort" || s == "uint16")  { out = PlyType::U16; return true; }
    if (s == "int"    || s == "int32")   { out = PlyType::I32; return true; }
    if (s == "uint"   || s == "uint32")  { out = PlyType::U32; return true; }
    return false;
}

// Reads the header only. A malformed file comes back with ok == false rather
// than throwing, because is_splat_ply() asks about arbitrary files the user
// dropped on the window.
PlyHeader read_header(std::istream& f) {
    PlyHeader h;
    std::string line;
    if (!std::getline(f, line)) return h;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.rfind("ply", 0) != 0) return h;

    bool in_vertex = false;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ss(line);
        std::string tok;
        ss >> tok;
        if (tok == "format") {
            ss >> tok;
            h.binary_le = tok == "binary_little_endian";
            h.ascii = tok == "ascii";
        } else if (tok == "element") {
            std::string name;
            int64_t count = 0;
            ss >> name >> count;
            in_vertex = name == "vertex";
            if (in_vertex) h.num_vertex = count;
        } else if (tok == "property") {
            std::string type, name;
            ss >> type;
            // A list property (faces) has no fixed width; a splat PLY has
            // none, and one on the vertex element makes the row unstridable.
            if (type == "list") {
                if (in_vertex) return h;
                continue;
            }
            ss >> name;
            if (!in_vertex) continue;
            PlyProp p;
            if (!parse_type(type, p.type)) return h;
            p.name = name;
            p.offset = h.row_bytes;
            h.row_bytes += type_size(p.type);
            h.props.push_back(p);
        } else if (tok == "end_header") {
            h.data_start = f.tellg();
            h.ok = h.num_vertex >= 0 && !h.props.empty() && (h.binary_le || h.ascii);
            return h;
        }
    }
    return h;
}

int find_prop(const PlyHeader& h, const std::string& name) {
    for (size_t i = 0; i < h.props.size(); i++)
        if (h.props[i].name == name) return (int)i;
    return -1;
}

bool has_props(const PlyHeader& h, std::initializer_list<const char*> names) {
    for (const char* n : names)
        if (find_prop(h, n) < 0) return false;
    return true;
}

float read_scalar(const uint8_t* row, const PlyProp& p) {
    const uint8_t* q = row + p.offset;
    switch (p.type) {
        case PlyType::F32: { float v;    std::memcpy(&v, q, 4); return v; }
        case PlyType::F64: { double v;   std::memcpy(&v, q, 8); return (float)v; }
        case PlyType::I8:  { int8_t v;   std::memcpy(&v, q, 1); return (float)v; }
        case PlyType::U8:  { uint8_t v;  std::memcpy(&v, q, 1); return (float)v; }
        case PlyType::I16: { int16_t v;  std::memcpy(&v, q, 2); return (float)v; }
        case PlyType::U16: { uint16_t v; std::memcpy(&v, q, 2); return (float)v; }
        case PlyType::I32: { int32_t v;  std::memcpy(&v, q, 4); return (float)v; }
        case PlyType::U32: { uint32_t v; std::memcpy(&v, q, 4); return (float)v; }
    }
    return 0.0f;
}

// The SH degree an f_rest count encodes: 3 * ((deg+1)^2 - 1), or one slot more
// per channel, the pad resumed runs wrote until issue #121. Any other count
// would render nonsense if read.
int sh_degree_from_rest(int n_rest, const std::string& path) {
    if (n_rest == 0) return 0;
    if (n_rest % 3 != 0)
        throw std::runtime_error(path + ": " + std::to_string(n_rest) +
                                 " f_rest properties is not a multiple of 3");
    const int per_channel = n_rest / 3;
    for (int deg = 1; deg <= 4; deg++)
        if (per_channel == (deg + 1) * (deg + 1) - 1 ||
            per_channel == (deg + 1) * (deg + 1)) return deg;
    throw std::runtime_error(path + ": " + std::to_string(per_channel) +
                             " SH coefficients per channel is not a whole degree");
}

}  // namespace


bool is_splat_ply(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    PlyHeader h = read_header(f);
    if (!h.ok) return false;
    return has_props(h, {"x", "y", "z", "opacity", "scale_0", "scale_1", "scale_2",
                         "rot_0", "rot_1", "rot_2", "rot_3",
                         "f_dc_0", "f_dc_1", "f_dc_2"});
}


SplatCloud read_splat_ply(const std::string& path, bool want_sh) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + path);
    PlyHeader h = read_header(f);
    if (!h.ok) throw std::runtime_error(path + ": not a readable PLY");

    auto col = [&](const std::string& name) {
        int i = find_prop(h, name);
        if (i < 0) throw std::runtime_error(path + ": missing property " + name +
                                            " (is this a 3D Gaussian Splatting PLY?)");
        return i;
    };
    const int cx = col("x"), cy = col("y"), cz = col("z");
    const int cq[4] = {col("rot_0"), col("rot_1"), col("rot_2"), col("rot_3")};
    const int cs[3] = {col("scale_0"), col("scale_1"), col("scale_2")};
    const int co = col("opacity");
    const int cd[3] = {col("f_dc_0"), col("f_dc_1"), col("f_dc_2")};

    // f_rest_<i>: red 0..K-1, then green, then blue (channel-major).
    std::vector<int> crest;
    if (want_sh) {
        for (int i = 0;; i++) {
            int c = find_prop(h, "f_rest_" + std::to_string(i));
            if (c < 0) break;
            crest.push_back(c);
        }
    }
    SplatCloud out;
    out.sh_degree = sh_degree_from_rest((int)crest.size(), path);
    const int64_t K = out.dim_sh() - 1;           // coefficients past the DC
    const int64_t K_file = (int64_t)crest.size() / 3;
    const int64_t n = h.num_vertex;
    out.num = n;
    out.means.resize((size_t)n * 3);
    out.quats.resize((size_t)n * 4);
    out.scales.resize((size_t)n * 3);
    out.opacities.resize((size_t)n);
    out.features_dc.resize((size_t)n * 3);
    if (K > 0) out.features_sh.assign((size_t)n * K * 3, 0.0f);

    auto take = [&](int64_t i, const float* v) {
        out.means[i*3+0] = v[cx]; out.means[i*3+1] = v[cy]; out.means[i*3+2] = v[cz];
        for (int k = 0; k < 4; k++) out.quats[i*4+k] = v[cq[k]];
        for (int k = 0; k < 3; k++) out.scales[i*3+k] = v[cs[k]];
        out.opacities[i] = v[co];
        for (int k = 0; k < 3; k++) out.features_dc[i*3+k] = v[cd[k]];
        // Channel-major in the file, coefficient-major in the engine.
        for (int ch = 0; ch < 3 && K > 0; ch++)
            for (int64_t j = 0; j < K; j++)
                out.features_sh[(i * K + j) * 3 + ch] = v[crest[(size_t)(ch * K_file + j)]];
    };

    if (h.ascii) {
        f.clear();
        f.seekg(h.data_start);
        std::vector<float> vals(h.props.size());
        for (int64_t i = 0; i < n; i++) {
            for (size_t k = 0; k < h.props.size(); k++)
                if (!(f >> vals[k]))
                    throw std::runtime_error(path + ": truncated vertex data");
            take(i, vals.data());
        }
        return out;
    }

    f.clear();
    f.seekg(h.data_start);
    // Row-blocked read: a large model is hundreds of MB and one read() per
    // vertex is most of the load time.
    const int64_t kRows = 1 << 14;
    std::vector<uint8_t> raw((size_t)h.row_bytes * kRows);
    std::vector<float> vals(h.props.size());
    for (int64_t row = 0; row < n; row += kRows) {
        const int64_t nr = std::min(kRows, n - row);
        f.read((char*)raw.data(), (std::streamsize)(nr * (int64_t)h.row_bytes));
        if (!f) throw std::runtime_error(path + ": truncated vertex data");
        for (int64_t i = 0; i < nr; i++) {
            const uint8_t* r = raw.data() + (size_t)i * h.row_bytes;
            for (size_t k = 0; k < h.props.size(); k++)
                vals[k] = read_scalar(r, h.props[k]);
            take(row + i, vals.data());
        }
    }
    return out;
}


void write_splat_ply(const SplatCloud& c, const std::string& path,
                     const uint8_t* keep, const SplatTransform* moved) {
    const int64_t K = c.dim_sh() - 1;
    int64_t kept = c.num;
    if (keep) {
        kept = 0;
        for (int64_t i = 0; i < c.num; i++) kept += keep[i] ? 1 : 0;
    }
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    std::ofstream ply(path, std::ios::binary);
    if (!ply) throw std::runtime_error("cannot open for writing: " + path);
    ply << "ply\n";
    ply << "format binary_little_endian 1.0\n";
    ply << "element vertex " << kept << "\n";
    ply << "property float x\nproperty float y\nproperty float z\n";
    ply << "property float nx\nproperty float ny\nproperty float nz\n";
    ply << "property float f_dc_0\nproperty float f_dc_1\nproperty float f_dc_2\n";
    for (int64_t j = 0; j < 3 * K; j++) ply << "property float f_rest_" << j << "\n";
    ply << "property float opacity\n";
    ply << "property float scale_0\nproperty float scale_1\nproperty float scale_2\n";
    ply << "property float rot_0\nproperty float rot_1\nproperty float rot_2\nproperty float rot_3\n";
    ply << "end_header\n";

    const int row_floats = (int)(3 + 3 + 3 + 3 * K + 1 + 3 + 4);
    constexpr int kRowsPerFlush = 1024;
    std::vector<float> buf((size_t)row_floats * kRowsPerFlush);
    int rows = 0;
    auto flush = [&] {
        if (!rows) return;
        ply.write(reinterpret_cast<const char*>(buf.data()),
                  (std::streamsize)rows * row_floats * sizeof(float));
        rows = 0;
    };
    const bool move = moved && !moved->is_identity();
    std::vector<float> sh((size_t)std::max<int64_t>(K, 1) * 3);
    for (int64_t i = 0; i < c.num; i++) {
        if (keep && !keep[i]) continue;
        float mean[3], quat[4], scale[3];
        for (int a = 0; a < 3; a++) mean[a] = c.means[(size_t)i * 3 + a];
        for (int a = 0; a < 3; a++) scale[a] = c.scales[(size_t)i * 3 + a];
        for (int a = 0; a < 4; a++) quat[a] = c.quats[(size_t)i * 4 + a];
        const float* rest = K > 0 ? &c.features_sh[(size_t)i * K * 3] : nullptr;
        if (move) {
            if (K > 0) std::copy(rest, rest + K * 3, sh.begin());
            moved->apply(mean, quat, scale, K > 0 ? sh.data() : nullptr, (int)K);
            rest = sh.data();
        }
        float* row = buf.data() + (size_t)rows * row_floats;
        int p = 0;
        for (int a = 0; a < 3; a++) row[p++] = mean[a];
        row[p++] = 0.0f; row[p++] = 0.0f; row[p++] = 0.0f;   // nx ny nz
        for (int a = 0; a < 3; a++) row[p++] = c.features_dc[(size_t)i * 3 + a];
        // A PLY stores f_rest channel-major; SplatCloud holds it
        // coefficient-major, which is the transposition read_splat_ply undoes.
        for (int ch = 0; ch < 3; ch++)
            for (int64_t j = 0; j < K; j++)
                row[p++] = rest[(size_t)j * 3 + ch];
        row[p++] = c.opacities[(size_t)i];
        for (int a = 0; a < 3; a++) row[p++] = scale[a];
        for (int a = 0; a < 4; a++) row[p++] = quat[a];
        if (++rows == kRowsPerFlush) flush();
    }
    flush();
    ply.flush();
    if (!ply) throw std::runtime_error("write failed: " + path);
}


std::pair<std::string, std::string> find_splat_ply(const std::string& path) {
    const fs::path p(path);
    std::error_code ec;
    if (fs::is_regular_file(p, ec) && p.extension() == ".ply") {
        for (fs::path run = p.parent_path(); !run.empty(); run = run.parent_path()) {
            if (fs::is_regular_file(run / "config.json", ec))
                return {p.string(), run.string()};
            if (run == run.parent_path()) break;
        }
        return {p.string(), p.parent_path().parent_path().string()};
    }
    if (fs::is_regular_file(p / "splat.ply", ec))
        return {(p / "splat.ply").string(), p.parent_path().string()};
    if (fs::is_directory(p, ec)) {
        // A run directory: the newest checkpoint under it that has one.
        std::vector<fs::path> ckpts;
        for (fs::directory_iterator it(p, ec), end; !ec && it != end; it.increment(ec)) {
            const std::string b = it->path().filename().string();
            if (it->is_directory(ec) &&
                (b.rfind("step-", 0) == 0 || b.find(".ckpt") != std::string::npos))
                ckpts.push_back(it->path());
        }
        std::sort(ckpts.begin(), ckpts.end());
        for (auto it = ckpts.rbegin(); it != ckpts.rend(); ++it)
            if (fs::is_regular_file(*it / "splat.ply", ec))
                return {(*it / "splat.ply").string(), p.string()};
    }
    throw std::runtime_error("no splat.ply found under " + path);
}

}  // namespace spirula

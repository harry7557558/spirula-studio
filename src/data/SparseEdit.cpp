// SparseEdit.cpp -- see SparseEdit.h.

#include "data/SparseEdit.h"

#include "data/DatasetParser.h"
#include "data/Json.h"
#include "data/JsonWrite.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>

namespace fs = std::filesystem;

namespace spirula {

namespace {

std::string read_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("cannot read " + p.string());
    return std::string((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
}

void write_file(const fs::path& p, const std::string& body) {
    std::ofstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("cannot write " + p.string());
    f.write(body.data(), (std::streamsize)body.size());
    f.flush();
    if (!f) throw std::runtime_error("write failed: " + p.string());
}

// Once, and only once: a second edit must not overwrite the copy of the file
// as it was before any of them.
void keep_original(const fs::path& p) {
    std::error_code ec;
    const fs::path bak = p.string() + ".orig";
    if (fs::exists(bak, ec) || !fs::exists(p, ec)) return;
    fs::copy_file(p, bak, ec);
}

// Images are matched by the leaf of their path: the reconstruction stores a
// name relative to its own image folder and the parser stores an absolute
// one, and the leaf is the part the two always agree on.
std::string leaf_of(const std::string& path) {
    return fs::path(path).filename().string();
}

std::set<std::string> leaf_set(const std::vector<std::string>& v) {
    std::set<std::string> s;
    for (const std::string& p : v) s.insert(leaf_of(p));
    return s;
}

template <typename T>
T read_le(const char*& p) {
    T v;
    std::memcpy(&v, p, sizeof(T));
    p += sizeof(T);
    return v;
}

template <typename T>
void put_le(std::string& out, T v) {
    out.append(reinterpret_cast<const char*>(&v), sizeof(T));
}

// COLMAP's world-to-camera pose (q as w,x,y,z) under x' = s Q x + u. A camera
// is rigid and cannot carry s, so its frame grows instead: R' = R Q^T,
// t' = s t - R' u. docs/notes/scene-transform.md.
void move_w2c(const Sim3& T, double q[4], double t[3]) {
    double qt[4];
    T.quat(qt);
    // q * conj(qt)
    const double aw = q[0], ax = q[1], ay = q[2], az = q[3];
    const double bw = qt[0], bx = -qt[1], by = -qt[2], bz = -qt[3];
    double r[4] = {aw*bw - ax*bx - ay*by - az*bz,
                   aw*bx + ax*bw + ay*bz - az*by,
                   aw*by - ax*bz + ay*bw + az*bx,
                   aw*bz + ax*by - ay*bx + az*bw};
    double n = std::sqrt(r[0]*r[0] + r[1]*r[1] + r[2]*r[2] + r[3]*r[3]);
    if (!(n > 1e-300)) return;
    if (r[0] < 0) n = -n;
    for (int i = 0; i < 4; i++) r[i] /= n;
    const double w = r[0], x = r[1], y = r[2], z = r[3];
    const double R[9] = {1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w),
                         2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w),
                         2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)};
    for (int i = 0; i < 3; i++)
        t[i] = T.s * t[i] - (R[i*3+0]*T.t[0] + R[i*3+1]*T.t[1] + R[i*3+2]*T.t[2]);
    for (int i = 0; i < 4; i++) q[i] = r[i];
}


// images.bin: a count, then per image an id, a pose, a camera id, a
// null-terminated name and the 2D observations. Rows are copied byte for
// byte, so nothing an edit did not ask about is rewritten.
std::string filter_images_bin(const std::string& src,
                              const std::set<std::string>& drop,
                              std::set<int32_t>& dropped_ids,
                              const Sim3* moved) {
    const char* p = src.data();
    const char* end = src.data() + src.size();
    if ((size_t)(end - p) < sizeof(uint64_t))
        throw std::runtime_error("images.bin is truncated");
    const uint64_t n = read_le<uint64_t>(p);
    std::string out;
    out.reserve(src.size());
    out.resize(sizeof(uint64_t));
    uint64_t kept = 0;
    for (uint64_t i = 0; i < n; i++) {
        const char* row = p;
        if (end - p < 4 + 8 * 7 + 4) throw std::runtime_error("images.bin is truncated");
        const int32_t id = read_le<int32_t>(p);
        p += 8 * 7 + 4;                              // qvec, tvec, camera_id
        const char* name = p;
        while (p < end && *p) p++;
        if (p >= end) throw std::runtime_error("images.bin is truncated");
        const std::string image_name(name, (size_t)(p - name));
        p++;                                          // the terminator
        if ((size_t)(end - p) < sizeof(uint64_t))
            throw std::runtime_error("images.bin is truncated");
        const uint64_t pts = read_le<uint64_t>(p);
        const size_t pt_bytes = (size_t)pts * (8 + 8 + 8);
        if ((size_t)(end - p) < pt_bytes) throw std::runtime_error("images.bin is truncated");
        p += pt_bytes;
        if (drop.count(leaf_of(image_name))) {
            dropped_ids.insert(id);
            continue;
        }
        const size_t at = out.size();
        out.append(row, (size_t)(p - row));
        if (moved) {
            double pose[7];
            std::memcpy(pose, &out[at + 4], sizeof pose);
            move_w2c(*moved, pose, pose + 4);
            std::memcpy(&out[at + 4], pose, sizeof pose);
        }
        kept++;
    }
    std::memcpy(&out[0], &kept, sizeof(uint64_t));
    return out;
}

// frames.bin (COLMAP 3.12+): per frame an id, a rig id, rig_from_world, then
// its data ids. "" when the layout does not account for every byte -- a file
// this cannot read exactly is one it must not rewrite.
std::string move_frames_bin(const std::string& src, const Sim3& moved) {
    const char* p = src.data();
    const char* end = src.data() + src.size();
    if ((size_t)(end - p) < sizeof(uint64_t)) return {};
    const uint64_t n = read_le<uint64_t>(p);
    std::string out = src;
    for (uint64_t i = 0; i < n; i++) {
        if (end - p < 4 + 4 + 8 * 7 + 4) return {};
        p += 8;
        const size_t at = (size_t)(p - src.data());
        double pose[7];
        std::memcpy(pose, p, sizeof pose);
        move_w2c(moved, pose, pose + 4);
        std::memcpy(&out[at], pose, sizeof pose);
        p += sizeof pose;
        const uint32_t ids = read_le<uint32_t>(p);
        const size_t bytes = (size_t)ids * (4 + 4 + 8);
        if ((size_t)(end - p) < bytes) return {};
        p += bytes;
    }
    return p == end ? out : std::string();
}

// points3D.bin: the same, except that a track entry naming a dropped image
// has to go with it, which makes the row a rewrite rather than a copy.
std::string filter_points3d_bin(const std::string& src,
                                const std::vector<uint8_t>& keep,
                                const std::set<int32_t>& dropped_ids,
                                const Sim3* moved) {
    const char* p = src.data();
    const char* end = src.data() + src.size();
    if ((size_t)(end - p) < sizeof(uint64_t))
        throw std::runtime_error("points3D.bin is truncated");
    const uint64_t n = read_le<uint64_t>(p);
    std::string out;
    out.reserve(src.size());
    out.resize(sizeof(uint64_t));
    uint64_t kept = 0;
    for (uint64_t i = 0; i < n; i++) {
        const char* head = p;
        if (end - p < 8 + 24 + 3 + 8 + 8)
            throw std::runtime_error("points3D.bin is truncated");
        p += 8 + 24 + 3 + 8;                          // id, xyz, rgb, error
        const size_t head_bytes = (size_t)(p - head);
        const uint64_t track = read_le<uint64_t>(p);
        const char* track_at = p;
        const size_t track_bytes = (size_t)track * 2 * sizeof(int32_t);
        if ((size_t)(end - p) < track_bytes)
            throw std::runtime_error("points3D.bin is truncated");
        p += track_bytes;
        if (i < keep.size() && !keep[(size_t)i]) continue;
        const size_t at = out.size();
        out.append(head, head_bytes);
        if (moved) {
            double xyz[3], q[3];
            std::memcpy(xyz, &out[at + 8], sizeof xyz);
            moved->apply(xyz, q);
            std::memcpy(&out[at + 8], q, sizeof q);
        }
        if (dropped_ids.empty()) {
            put_le<uint64_t>(out, track);
            out.append(track_at, track_bytes);
        } else {
            std::string live;
            uint64_t n_live = 0;
            for (uint64_t t = 0; t < track; t++) {
                const char* e = track_at + (size_t)t * 2 * sizeof(int32_t);
                int32_t image_id;
                std::memcpy(&image_id, e, sizeof(int32_t));
                if (dropped_ids.count(image_id)) continue;
                live.append(e, 2 * sizeof(int32_t));
                n_live++;
            }
            put_le<uint64_t>(out, n_live);
            out += live;
        }
        kept++;
    }
    std::memcpy(&out[0], &kept, sizeof(uint64_t));
    return out;
}

// The text model, one record per non-comment line -- except images.txt, where
// a record is two lines and the second may be empty.
std::string filter_images_txt(const std::string& src,
                              const std::set<std::string>& drop,
                              std::set<int32_t>& dropped_ids,
                              const Sim3* moved) {
    std::string out;
    out.reserve(src.size());
    size_t pos = 0;
    auto take_line = [&](size_t& at, std::string& line) {
        if (at >= src.size()) return false;
        size_t eol = src.find('\n', at);
        if (eol == std::string::npos) eol = src.size();
        line = src.substr(at, std::min(eol, src.size()) - at);
        at = eol + 1;
        return true;
    };
    std::string line;
    while (pos < src.size()) {
        const size_t start = pos;
        if (!take_line(pos, line)) break;
        size_t b = 0;
        while (b < line.size() && std::isspace((unsigned char)line[b])) b++;
        if (b >= line.size() || line[b] == '#') {
            out.append(src, start, pos - start);
            continue;
        }
        // id qw qx qy qz tx ty tz camera_id name
        const size_t obs_start = pos;
        std::string obs;
        take_line(pos, obs);
        int32_t id = 0;
        char name[1024] = {0};
        double d[7];
        int cam = 0;
        const bool parsed =
            std::sscanf(line.c_str() + b, "%d %lf %lf %lf %lf %lf %lf %lf %d %1023s",
                        &id, &d[0], &d[1], &d[2], &d[3], &d[4], &d[5], &d[6],
                        &cam, name) == 10;
        if (parsed && drop.count(leaf_of(name))) {
            dropped_ids.insert(id);
            continue;
        }
        if (parsed && moved) {
            move_w2c(*moved, d, d + 4);
            char buf[512];
            std::snprintf(buf, sizeof buf,
                          "%d %.17g %.17g %.17g %.17g %.17g %.17g %.17g %d %s\n",
                          id, d[0], d[1], d[2], d[3], d[4], d[5], d[6], cam, name);
            out += buf;
            out.append(src, obs_start, pos - obs_start);
            continue;
        }
        out.append(src, start, pos - start);
    }
    return out;
}

std::string filter_points3d_txt(const std::string& src,
                                const std::vector<uint8_t>& keep,
                                const std::set<int32_t>& dropped_ids,
                                const Sim3* moved) {
    std::string out;
    out.reserve(src.size());
    size_t pos = 0, index = 0;
    while (pos < src.size()) {
        size_t eol = src.find('\n', pos);
        if (eol == std::string::npos) eol = src.size();
        const size_t next = eol + 1;
        size_t b = pos, t = eol;
        while (b < t && (src[b] == ' ' || src[b] == '\t' || src[b] == '\r')) b++;
        const bool record = b < t && src[b] != '#';
        if (!record) {
            out.append(src, pos, std::min(next, src.size()) - pos);
            pos = next;
            continue;
        }
        const bool take = index >= keep.size() || keep[index] != 0;
        index++;
        if (!take) {
            pos = next;
            continue;
        }
        if (dropped_ids.empty() && !moved) {
            out.append(src, pos, std::min(next, src.size()) - pos);
            pos = next;
            continue;
        }
        // Rewrite the row: the first eight fields are the point -- its id,
        // xyz, rgb and error -- and the rest is (image_id, point2D_idx) pairs.
        std::string line = src.substr(b, t - b);
        const char* s = line.c_str();
        char* q = nullptr;
        std::string head;
        double xyz[3] = {0, 0, 0};
        size_t xyz_from = 0, xyz_to = 0;
        for (int f = 0; f < 8; f++) {
            const double v = std::strtod(s, &q);
            if (q == s) break;
            if (f == 1) xyz_from = head.size();
            if (f >= 1 && f <= 3) xyz[f - 1] = v;
            head.append(s, (size_t)(q - s));
            if (f == 3) xyz_to = head.size();
            s = q;
        }
        if (moved && xyz_to > xyz_from) {
            double o[3];
            moved->apply(xyz, o);
            char buf[128];
            std::snprintf(buf, sizeof buf, " %.17g %.17g %.17g", o[0], o[1], o[2]);
            head.replace(xyz_from, xyz_to - xyz_from, buf);
        }
        std::string track;
        while (true) {
            const long a = std::strtol(s, &q, 10);
            if (q == s) break;
            const char* mid = q;
            const long bi = std::strtol(mid, &q, 10);
            if (q == mid) break;
            if (!dropped_ids.count((int32_t)a)) {
                track += " " + std::to_string(a) + " " + std::to_string(bi);
            }
            s = q;
        }
        out += head + track + "\n";
        pos = next;
    }
    return out;
}

std::string nerf_ply_rel(const JsonValue& meta) {
    if (const JsonValue* v = meta.find("ply_file_path")) return v->as_string();
    return {};
}

ColmapPoints3D read_points_of(const std::string& dataset_dir,
                              const std::string& ply_rel) {
    if (ply_rel.empty()) return {};
    return read_ply_points((fs::path(dataset_dir) / ply_rel).string());
}

// The frames a Nerfstudio meta keeps, by the leaf of their file_path.
bool drop_frames(JsonValue& meta, const std::set<std::string>& drop) {
    if (drop.empty()) return false;
    JsonValue* frames = nullptr;
    for (auto& [k, v] : meta.obj)
        if (k == "frames") frames = &v;
    if (!frames || !frames->is_array()) return false;
    std::vector<JsonValue> kept;
    kept.reserve(frames->arr.size());
    for (JsonValue& f : frames->arr) {
        const JsonValue* fp = f.find("file_path");
        if (fp && drop.count(leaf_of(fp->as_string()))) continue;
        kept.push_back(std::move(f));
    }
    const bool changed = kept.size() != frames->arr.size();
    frames->arr = std::move(kept);
    return changed;
}

// A transforms.json holds its poses in the frame applied_transform maps the
// raw one INTO, so the same placement there is the conjugate A T A^-1, and
// applied_transform itself is left alone. docs/notes/scene-transform.md.
Sim3 to_json_frame(const JsonValue& meta, const Sim3& T) {
    const JsonValue* at = meta.find("applied_transform");
    if (!at || !at->is_array() || at->arr.size() < 3) return T;
    double A[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    for (int r = 0; r < 3; r++) {
        const JsonValue& row = at->arr[(size_t)r];
        if (!row.is_array() || row.arr.size() < 4) return T;
        for (int c = 0; c < 4; c++) A[r*4+c] = row.arr[(size_t)c].as_double();
    }
    double Ai[16], M[16] = {0}, tmp[16], out[16];
    dsparse::invert_affine4x4(A, Ai);
    double m34[12];
    T.to_3x4(m34);
    for (int i = 0; i < 12; i++) M[i] = m34[i];
    M[15] = 1.0;
    auto mul = [](const double* a, const double* b, double* o) {
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 4; c++) {
                double v = 0.0;
                for (int k = 0; k < 4; k++) v += a[r*4+k] * b[k*4+c];
                o[r*4+c] = v;
            }
    };
    mul(A, M, tmp);
    mul(tmp, Ai, out);
    return Sim3::from_3x4(out);
}

// Camera-to-world: the position moves with the scene, the axes only turn --
// a transform_matrix whose columns stopped being unit would be a lens.
void move_frames(JsonValue& meta, const Sim3& T) {
    for (auto& [k, frames] : meta.obj) {
        if (k != "frames" || !frames.is_array()) continue;
        for (JsonValue& f : frames.arr)
            for (auto& [fk, tm] : f.obj) {
                if (fk != "transform_matrix" || !tm.is_array() ||
                    tm.arr.size() < 3)
                    continue;
                double m[12];
                bool ok = true;
                for (int r = 0; r < 3 && ok; r++) {
                    ok = tm.arr[(size_t)r].is_array() &&
                         tm.arr[(size_t)r].arr.size() >= 4;
                    for (int c = 0; c < 4 && ok; c++)
                        m[r*4+c] = tm.arr[(size_t)r].arr[(size_t)c].as_double();
                }
                if (!ok) continue;
                double o[12];
                for (int c = 0; c < 3; c++) {
                    const double v[3] = {m[0*4+c], m[1*4+c], m[2*4+c]};
                    double w[3];
                    T.rotate(v, w);
                    for (int r = 0; r < 3; r++) o[r*4+c] = w[r];
                }
                const double pos[3] = {m[3], m[7], m[11]};
                double q[3];
                T.apply(pos, q);
                for (int r = 0; r < 3; r++) o[r*4+3] = q[r];
                for (int r = 0; r < 3; r++)
                    for (int c = 0; c < 4; c++) {
                        JsonValue& cell = tm.arr[(size_t)r].arr[(size_t)c];
                        cell.type = JsonValue::Type::Number;
                        cell.num = o[r*4+c];
                    }
            }
    }
}

void set_string(JsonValue& obj, const char* key, const std::string& value) {
    for (auto& [k, v] : obj.obj)
        if (k == key) {
            v.type = JsonValue::Type::String;
            v.str = value;
            return;
        }
    JsonValue v;
    v.type = JsonValue::Type::String;
    v.str = value;
    obj.obj.emplace_back(key, std::move(v));
}

}  // namespace


SparseFormat sparse_format_of(const std::string& dataset_dir) {
    std::error_code ec;
    if (fs::exists(fs::path(dataset_dir) / "transforms.json", ec))
        return SparseFormat::Nerfstudio;
    if (!find_colmap_model(dataset_dir, "").empty()) return SparseFormat::Colmap;
    for (fs::directory_iterator it(fs::path(dataset_dir), ec), end;
         !ec && it != end; it.increment(ec)) {
        std::string e = it->path().extension().string();
        for (char& c : e) c = (char)std::tolower((unsigned char)c);
        if (e == ".xml") return SparseFormat::Metashape;
    }
    return SparseFormat::None;
}


std::string resolve_sparse_dir(const std::string& path) {
    std::error_code ec;
    fs::path p(path);
    if (p.empty()) return {};
    if (fs::is_regular_file(p, ec)) p = p.parent_path();
    if (!fs::is_directory(p, ec)) return {};

    // A folder of models is not a model: `sparse/` holds `0`, `1`, ...
    if (sparse_format_of(p.string()) == SparseFormat::None) {
        std::vector<fs::path> subs;
        for (fs::directory_iterator it(p, fs::directory_options::skip_permission_denied, ec),
             end; !ec && it != end; it.increment(ec))
            if (it->is_directory(ec)) subs.push_back(it->path());
        std::sort(subs.begin(), subs.end());
        for (const fs::path& s : subs)
            if (sparse_format_of(s.string()) != SparseFormat::None) {
                p = s;
                break;
            }
    }
    if (sparse_format_of(p.string()) == SparseFormat::None) return {};

    // ... and a model folder is not the dataset: strip the conventional tail
    // so the images beside it are found too.
    auto conventional = [](const std::string& leaf) {
        if (leaf == "sparse" || leaf == "colmap") return true;
        if (leaf.empty()) return false;
        for (char c : leaf)
            if (!std::isdigit((unsigned char)c)) return false;
        return true;
    };
    fs::path q = p;
    for (int i = 0; i < 3; i++) {
        if (!conventional(q.filename().string())) break;
        q = q.parent_path();
        if (q.empty()) break;
        if (sparse_format_of(q.string()) != SparseFormat::None) p = q;
    }
    return p.string();
}


SparseStats read_sparse_stats(const std::string& dataset_dir) {
    SparseStats out;
    if (sparse_format_of(dataset_dir) != SparseFormat::Colmap) return out;
    bool text = false;
    const std::string model = find_colmap_model(dataset_dir, "", &text);
    if (model.empty()) return out;
    std::map<int32_t, int32_t> index_of_id;

    if (!text) {
        const std::string im = read_file(fs::path(model) / "images.bin");
        const char* p = im.data();
        const char* end = p + im.size();
        if ((size_t)(end - p) < 8) return out;
        const uint64_t n = read_le<uint64_t>(p);
        for (uint64_t i = 0; i < n; i++) {
            if (end - p < 4 + 8 * 7 + 4) return SparseStats{};
            const int32_t id = read_le<int32_t>(p);
            p += 8 * 7 + 4;
            const char* name = p;
            while (p < end && *p) p++;
            if (p >= end) return SparseStats{};
            index_of_id[id] = (int32_t)out.image_names.size();
            out.image_names.emplace_back(name, (size_t)(p - name));
            p++;
            if ((size_t)(end - p) < 8) return SparseStats{};
            const uint64_t pts = read_le<uint64_t>(p);
            if ((size_t)(end - p) < pts * 24) return SparseStats{};
            int32_t seen = 0;
            for (uint64_t k = 0; k < pts; k++) {
                int64_t point_id;
                std::memcpy(&point_id, p + k * 24 + 16, 8);
                seen += point_id >= 0 ? 1 : 0;
            }
            out.image_points.push_back(seen);
            p += pts * 24;
        }
        const std::string pt = read_file(fs::path(model) / "points3D.bin");
        p = pt.data();
        end = p + pt.size();
        if ((size_t)(end - p) < 8) return SparseStats{};
        const uint64_t m = read_le<uint64_t>(p);
        out.track_beg.push_back(0);
        for (uint64_t i = 0; i < m; i++) {
            if (end - p < 8 + 24 + 3 + 8 + 8) return SparseStats{};
            p += 8 + 24 + 3;
            out.error.push_back((float)read_le<double>(p));
            const uint64_t track = read_le<uint64_t>(p);
            if ((size_t)(end - p) < track * 8) return SparseStats{};
            for (uint64_t t = 0; t < track; t++) {
                int32_t image_id;
                std::memcpy(&image_id, p + t * 8, 4);
                const auto it = index_of_id.find(image_id);
                if (it != index_of_id.end()) out.track_image.push_back(it->second);
            }
            p += track * 8;
            out.track_beg.push_back((int64_t)out.track_image.size());
        }
        return out;
    }

    // The text model: images.txt is two lines a record, points3D.txt one.
    {
        std::ifstream f(fs::path(model) / "images.txt");
        std::string line, obs;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::getline(f, obs);
            int32_t id = 0, cam = 0;
            double d[7];
            char name[1024] = {0};
            if (std::sscanf(line.c_str(), "%d %lf %lf %lf %lf %lf %lf %lf %d %1023s",
                            &id, &d[0], &d[1], &d[2], &d[3], &d[4], &d[5], &d[6],
                            &cam, name) != 10)
                continue;
            index_of_id[id] = (int32_t)out.image_names.size();
            out.image_names.emplace_back(name);
            // x y point3D_id triples; -1 marks a feature that matched nothing.
            int32_t seen = 0;
            const char* q = obs.c_str();
            char* e = nullptr;
            while (true) {
                std::strtod(q, &e);
                if (e == q) break;
                q = e;
                std::strtod(q, &e);
                q = e;
                const long long pid = std::strtoll(q, &e, 10);
                if (e == q) break;
                q = e;
                seen += pid >= 0 ? 1 : 0;
            }
            out.image_points.push_back(seen);
        }
    }
    std::ifstream f(fs::path(model) / "points3D.txt");
    std::string line;
    out.track_beg.push_back(0);
    while (std::getline(f, line)) {
        size_t b = 0;
        while (b < line.size() && std::isspace((unsigned char)line[b])) b++;
        if (b >= line.size() || line[b] == '#') continue;
        const char* q = line.c_str() + b;
        char* e = nullptr;
        double field[8] = {0};
        for (int k = 0; k < 8; k++) {
            field[k] = std::strtod(q, &e);
            q = e;
        }
        out.error.push_back((float)field[7]);
        while (true) {
            const long a = std::strtol(q, &e, 10);
            if (e == q) break;
            q = e;
            std::strtol(q, &e, 10);
            if (e == q) break;
            q = e;
            const auto it = index_of_id.find((int32_t)a);
            if (it != index_of_id.end()) out.track_image.push_back(it->second);
        }
        out.track_beg.push_back((int64_t)out.track_image.size());
    }
    return out;
}

void write_ply_points(const std::string& path, const double* xyz,
                      const uint8_t* rgb, int64_t n, const uint8_t* keep,
                      const Sim3* moved, bool double_xyz) {
    int64_t kept = n;
    if (keep) {
        kept = 0;
        for (int64_t i = 0; i < n; i++) kept += keep[i] ? 1 : 0;
    }
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot write " + path);
    f << "ply\nformat binary_little_endian 1.0\n";
    f << "element vertex " << kept << "\n";
    const char* type = double_xyz ? "double" : "float";
    f << "property " << type << " x\nproperty " << type << " y\nproperty " << type << " z\n";
    f << "property uchar red\nproperty uchar green\nproperty uchar blue\n";
    f << "end_header\n";
    for (int64_t i = 0; i < n; i++) {
        if (keep && !keep[i]) continue;
        double q[3] = {xyz[i * 3], xyz[i * 3 + 1], xyz[i * 3 + 2]};
        if (moved) moved->apply(&xyz[i * 3], q);
        if (double_xyz) {
            f.write(reinterpret_cast<const char*>(q), sizeof q);
        } else {
            const float p[3] = {(float)q[0], (float)q[1], (float)q[2]};
            f.write(reinterpret_cast<const char*>(p), sizeof p);
        }
        const uint8_t c[3] = {rgb ? rgb[i * 3] : (uint8_t)200,
                              rgb ? rgb[i * 3 + 1] : (uint8_t)200,
                              rgb ? rgb[i * 3 + 2] : (uint8_t)200};
        f.write(reinterpret_cast<const char*>(c), sizeof c);
    }
    f.flush();
    if (!f) throw std::runtime_error("write failed: " + path);
}


namespace {

// Read everything an edit of `dataset_dir` starts from.
SparseBaseline load_baseline(const std::string& dataset_dir) {
    SparseBaseline b;
    b.format = sparse_format_of(dataset_dir);
    std::error_code ec;
    switch (b.format) {
        case SparseFormat::Colmap: {
            b.model_dir = find_colmap_model(dataset_dir, "", &b.text);
            if (b.model_dir.empty())
                throw std::runtime_error("no COLMAP points3D under " + dataset_dir);
            const fs::path m(b.model_dir);
            b.images = read_file(m / (b.text ? "images.txt" : "images.bin"));
            b.points = read_file(m / (b.text ? "points3D.txt" : "points3D.bin"));
            if (!b.text && fs::exists(m / "frames.bin", ec))
                b.frames = read_file(m / "frames.bin");
            break;
        }
        case SparseFormat::Nerfstudio: {
            b.meta = read_file(fs::path(dataset_dir) / "transforms.json");
            b.ply_rel = nerf_ply_rel(json_parse(b.meta));
            if (!b.ply_rel.empty() &&
                fs::exists(fs::path(dataset_dir) / b.ply_rel, ec))
                b.cloud = read_ply_points((fs::path(dataset_dir) / b.ply_rel).string());
            break;
        }
        case SparseFormat::Metashape: {
            DatasetParserConfig cfg;
            const JsonValue meta = metashape_meta(dataset_dir, cfg);
            b.cloud = read_points_of(dataset_dir, nerf_ply_rel(meta));
            JsonWriter w;
            json_write(w, meta);
            b.meta = w.str();
            break;
        }
        default:
            throw std::runtime_error("no reconstruction to write back in " +
                                     dataset_dir);
    }
    return b;
}

}  // namespace

std::vector<std::string> sparse_write_filtered(const std::string& dataset_dir,
                                               const SparseKeep& keep,
                                               const Sim3* moved,
                                               SparseBaseline* base) {
    if (moved && moved->is_identity()) moved = nullptr;
    SparseBaseline local;
    if (!base) base = &local;
    if (!base->loaded()) *base = load_baseline(dataset_dir);
    const SparseBaseline& b = *base;

    std::vector<std::string> written;
    const std::set<std::string> drop = leaf_set(keep.drop_images);
    switch (b.format) {
        case SparseFormat::Colmap: {
            const fs::path model(b.model_dir);
            std::set<int32_t> dropped_ids;
            if (!drop.empty() || moved) {
                const fs::path ip = model / (b.text ? "images.txt" : "images.bin");
                const std::string body =
                    b.text ? filter_images_txt(b.images, drop, dropped_ids, moved)
                           : filter_images_bin(b.images, drop, dropped_ids, moved);
                keep_original(ip);
                write_file(ip, body);
                written.push_back(ip.string());
            }
            // COLMAP itself reads rig_from_world in preference to the image's
            // own pose, so a model that has the file has to have it moved.
            if (moved && !b.frames.empty()) {
                const std::string body = move_frames_bin(b.frames, *moved);
                if (!body.empty()) {
                    const fs::path fp = model / "frames.bin";
                    keep_original(fp);
                    write_file(fp, body);
                    written.push_back(fp.string());
                }
            }
            const fs::path pp = model / (b.text ? "points3D.txt" : "points3D.bin");
            keep_original(pp);
            write_file(pp, b.text ? filter_points3d_txt(b.points, keep.points,
                                                        dropped_ids, moved)
                                  : filter_points3d_bin(b.points, keep.points,
                                                        dropped_ids, moved));
            written.push_back(pp.string());
            break;
        }
        case SparseFormat::Nerfstudio:
        case SparseFormat::Metashape: {
            // A Metashape export is not ours to rewrite: the edit lands beside
            // it as the Nerfstudio dataset the parser reads first from then on.
            const bool ours = b.format == SparseFormat::Nerfstudio;
            JsonValue meta = json_parse(b.meta);
            std::string rel = ours ? b.ply_rel : std::string("points3D_edited.ply");
            if (rel.empty()) rel = "points3D.ply";
            Sim3 in_json;
            if (moved) in_json = to_json_frame(meta, *moved);
            const fs::path ply = fs::path(dataset_dir) / rel;
            if (ours) keep_original(ply);
            write_ply_points(ply.string(), b.cloud.xyz.data(),
                             b.cloud.rgb.empty() ? nullptr : b.cloud.rgb.data(),
                             b.cloud.num(), keep.points.data(),
                             moved ? &in_json : nullptr);
            written.push_back(ply.string());
            drop_frames(meta, drop);
            if (moved) move_frames(meta, in_json);
            set_string(meta, "ply_file_path", rel);
            const fs::path meta_path = fs::path(dataset_dir) / "transforms.json";
            if (ours) keep_original(meta_path);
            JsonWriter w;
            json_write(w, meta);
            write_file(meta_path, w.str());
            written.push_back(meta_path.string());
            break;
        }
        default:
            throw std::runtime_error("no reconstruction to write back in " +
                                     dataset_dir);
    }
    return written;
}

std::vector<std::string> sparse_write_copy(const std::string& dataset_dir,
                                           const std::string& out_dir,
                                           const SparseKeep& keep,
                                           const Sim3* moved,
                                           SparseBaseline* loaded) {
    SparseBaseline local;
    if (!loaded) loaded = &local;
    if (!loaded->loaded()) *loaded = load_baseline(dataset_dir);
    const SparseBaseline& base = *loaded;
    if (base.format != SparseFormat::Colmap)
        throw std::runtime_error("only a COLMAP reconstruction can be saved as a copy");
    std::error_code ec;
    const fs::path sparse = fs::path(out_dir) / "sparse";
    if (fs::exists(sparse, ec))
        throw std::runtime_error(sparse.string() + " already exists");
    const fs::path model = sparse / "0";
    fs::create_directories(model, ec);
    if (ec) throw std::runtime_error("cannot create " + model.string());
    const std::string ext = base.text ? ".txt" : ".bin";
    const fs::path images = model / ("images" + ext);
    const fs::path points = model / ("points3D" + ext);
    const fs::path frames = model / "frames.bin";
    for (fs::directory_iterator it(base.model_dir, ec), end; !ec && it != end;
         it.increment(ec)) {
        const fs::path name = it->path().filename();
        if (!it->is_regular_file(ec) || it->path().extension() == ".orig" ||
            name == images.filename() || name == points.filename() ||
            name == frames.filename())
            continue;
        fs::copy_file(it->path(), model / name, ec);
        if (ec) throw std::runtime_error("cannot copy " + it->path().string());
    }
    SparseBaseline b = base;
    b.model_dir = model.string();
    std::vector<std::string> written = sparse_write_filtered(out_dir, keep, moved, &b);
    // Only what the edit changed was written; the rest is the model as found.
    if (!fs::exists(images, ec)) write_file(images, b.images);
    if (!b.frames.empty() && !fs::exists(frames, ec)) write_file(frames, b.frames);
    return written;
}

}  // namespace spirula

// sparse_transform_test -- moving a reconstruction (data/SparseEdit.h) and
// reading it back. For each format the re-parsed cameras and points must be
// the originals under the same similarity, in the frame the PARSER hands
// out -- which for a transforms.json is not the frame the file is written in
// (applied_transform) -- and every point must still project to the pixel it
// did before, which is the one thing a placement may never change.

#include "data/DatasetParser.h"
#include "data/SparseEdit.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using spirula::Sim3;

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

template <typename T>
void put(std::string& o, T v) {
    o.append(reinterpret_cast<const char*>(&v), sizeof v);
}

struct Cam { double q[4], t[3]; std::string name; };

void quat_to_R(const double q[4], double R[9]) {
    const double w = q[0], x = q[1], y = q[2], z = q[3];
    const double M[9] = {1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w),
                         2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w),
                         2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)};
    for (int i = 0; i < 9; i++) R[i] = M[i];
}

void write_colmap(const fs::path& model, const std::vector<Cam>& cams,
                  const std::vector<double>& pts, bool text) {
    fs::create_directories(model);
    if (text) {
        std::ofstream c(model / "cameras.txt");
        c << "# cameras\n1 PINHOLE 640 480 500 500 320 240\n";
        std::ofstream im(model / "images.txt");
        im << "# images\n";
        for (size_t i = 0; i < cams.size(); i++) {
            char buf[512];
            std::snprintf(buf, sizeof buf, "%zu %.17g %.17g %.17g %.17g %.17g %.17g %.17g 1 %s\n",
                          i + 1, cams[i].q[0], cams[i].q[1], cams[i].q[2],
                          cams[i].q[3], cams[i].t[0], cams[i].t[1], cams[i].t[2],
                          cams[i].name.c_str());
            im << buf << "10.5 20.5 " << (i + 1) << "\n";
        }
        std::ofstream p(model / "points3D.txt");
        p << "# points\n";
        for (size_t i = 0; i < pts.size() / 3; i++) {
            char buf[256];
            std::snprintf(buf, sizeof buf, "%zu %.17g %.17g %.17g 10 20 30 0.5 1 0 2 0\n",
                          i + 1, pts[i*3], pts[i*3+1], pts[i*3+2]);
            p << buf;
        }
        return;
    }
    std::string c;
    put<uint64_t>(c, 1);
    put<int32_t>(c, 1); put<int32_t>(c, 1);          // id, PINHOLE
    put<uint64_t>(c, 640); put<uint64_t>(c, 480);
    for (double v : {500.0, 500.0, 320.0, 240.0}) put<double>(c, v);
    std::ofstream(model / "cameras.bin", std::ios::binary) << c;

    std::string im;
    put<uint64_t>(im, cams.size());
    for (size_t i = 0; i < cams.size(); i++) {
        put<int32_t>(im, (int32_t)i + 1);
        for (double v : cams[i].q) put<double>(im, v);
        for (double v : cams[i].t) put<double>(im, v);
        put<int32_t>(im, 1);
        im += cams[i].name;
        im.push_back('\0');
        put<uint64_t>(im, 1);
        put<double>(im, 10.5); put<double>(im, 20.5); put<int64_t>(im, (int64_t)i + 1);
    }
    std::ofstream(model / "images.bin", std::ios::binary) << im;

    std::string p;
    put<uint64_t>(p, pts.size() / 3);
    for (size_t i = 0; i < pts.size() / 3; i++) {
        put<uint64_t>(p, i + 1);
        for (int k = 0; k < 3; k++) put<double>(p, pts[i*3+k]);
        p.push_back(10); p.push_back(20); p.push_back(30);
        put<double>(p, 0.5);
        put<uint64_t>(p, 2);
        put<int32_t>(p, 1); put<int32_t>(p, 0);
        put<int32_t>(p, 2); put<int32_t>(p, 0);
    }
    std::ofstream(model / "points3D.bin", std::ios::binary) << p;
}

void write_nerfstudio(const fs::path& dir, const std::vector<Cam>& cams,
                      const std::vector<double>& pts, const double A[12]) {
    fs::create_directories(dir);
    // json = A * raw, for the poses and for the cloud.
    std::ofstream j(dir / "transforms.json");
    j.precision(17);
    j << "{\n \"camera_model\": \"PINHOLE\", \"w\": 640, \"h\": 480,\n"
         " \"fl_x\": 500, \"fl_y\": 500, \"cx\": 320, \"cy\": 240,\n"
         " \"applied_transform\": [";
    for (int r = 0; r < 3; r++) {
        j << (r ? ", [" : "[");
        for (int c = 0; c < 4; c++) j << (c ? ", " : "") << A[r*4+c];
        j << "]";
    }
    j << "],\n \"ply_file_path\": \"cloud.ply\",\n \"frames\": [\n";
    for (size_t i = 0; i < cams.size(); i++) {
        // COLMAP w2c -> OpenGL c2w in the raw frame, then into the json one.
        double R[9];
        quat_to_R(cams[i].q, R);
        double c2w[12];
        for (int r = 0; r < 3; r++) {
            c2w[r*4+0] = R[0*3+r];
            c2w[r*4+1] = -R[1*3+r];
            c2w[r*4+2] = -R[2*3+r];
            c2w[r*4+3] = -(R[0*3+r]*cams[i].t[0] + R[1*3+r]*cams[i].t[1] +
                           R[2*3+r]*cams[i].t[2]);
        }
        double m[12];
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 4; c++)
                m[r*4+c] = A[r*4+0]*c2w[0*4+c] + A[r*4+1]*c2w[1*4+c] +
                           A[r*4+2]*c2w[2*4+c] + (c == 3 ? A[r*4+3] : 0.0);
        j << "  {\"file_path\": \"images/" << cams[i].name
          << "\", \"transform_matrix\": [";
        for (int r = 0; r < 3; r++) {
            j << "[" << m[r*4] << ", " << m[r*4+1] << ", " << m[r*4+2] << ", "
              << m[r*4+3] << "], ";
        }
        j << "[0, 0, 0, 1]]}" << (i + 1 < cams.size() ? ",\n" : "\n");
    }
    j << " ]\n}\n";
    std::vector<double> q(pts.size());
    for (size_t i = 0; i < pts.size() / 3; i++)
        for (int r = 0; r < 3; r++)
            q[i*3+r] = A[r*4+0]*pts[i*3] + A[r*4+1]*pts[i*3+1] +
                       A[r*4+2]*pts[i*3+2] + A[r*4+3];
    spirula::write_ply_points((dir / "cloud.ply").string(), q.data(), nullptr,
                              (int64_t)q.size() / 3, nullptr);
}

ParsedDataset parse(const fs::path& dir) {
    DatasetParserConfig cfg;
    cfg.require_image_files = false;
    return parse_dataset(dir.string(), cfg, "");
}

// Pixel of point `p` in camera `i` (OpenGL c2w: the camera looks down -z).
bool pixel(const ParsedDataset& ds, int64_t i, const double p[3], double uv[2]) {
    const float* m = &ds.c2w[(size_t)i * 12];
    double d[3], c[3];
    for (int r = 0; r < 3; r++) d[r] = p[r] - m[r*4+3];
    for (int k = 0; k < 3; k++)
        c[k] = m[0*4+k]*d[0] + m[1*4+k]*d[1] + m[2*4+k]*d[2];
    if (c[2] > -1e-9) return false;
    uv[0] = 500.0 * c[0] / -c[2];
    uv[1] = 500.0 * -c[1] / -c[2];
    return true;
}

// Compare `after` with `before` moved by T. Returns the worst errors.
void compare(const ParsedDataset& before, const ParsedDataset& after,
             const Sim3& T, const std::vector<int64_t>& cam_of,
             const std::vector<int64_t>& pt_of, const std::string& tag) {
    double worst_p = 0, worst_c = 0, worst_r = 0, worst_uv = 0;
    bool counts = after.num_cameras == (int64_t)cam_of.size() &&
                  after.points.num() == (int64_t)pt_of.size();
    check(counts, tag + ": the right cameras and points came back");
    if (!counts) return;
    for (size_t k = 0; k < pt_of.size(); k++) {
        double want[3];
        T.apply(&before.points.xyz[(size_t)pt_of[k] * 3], want);
        for (int r = 0; r < 3; r++)
            worst_p = std::max(worst_p,
                               std::fabs(after.points.xyz[k*3+r] - want[r]));
    }
    for (size_t k = 0; k < cam_of.size(); k++) {
        const float* a = &before.c2w[(size_t)cam_of[k] * 12];
        const float* b = &after.c2w[k * 12];
        const double pos[3] = {a[3], a[7], a[11]};
        double want[3];
        T.apply(pos, want);
        for (int r = 0; r < 3; r++)
            worst_c = std::max(worst_c, std::fabs(b[r*4+3] - want[r]));
        for (int c = 0; c < 3; c++) {
            const double v[3] = {a[0*4+c], a[1*4+c], a[2*4+c]};
            double w[3];
            T.rotate(v, w);
            for (int r = 0; r < 3; r++)
                worst_r = std::max(worst_r, std::fabs(b[r*4+c] - w[r]));
        }
        for (size_t j = 0; j < pt_of.size(); j++) {
            double u0[2], u1[2];
            const bool v0 = pixel(before, cam_of[k],
                                  &before.points.xyz[(size_t)pt_of[j] * 3], u0);
            const bool v1 = pixel(after, (int64_t)k, &after.points.xyz[j * 3], u1);
            if (v0 != v1) { worst_uv = 1e9; continue; }
            if (!v0) continue;
            worst_uv = std::max({worst_uv, std::fabs(u0[0] - u1[0]),
                                 std::fabs(u0[1] - u1[1])});
        }
    }
    std::printf("     %s: point %.2e  camera %.2e  axes %.2e  pixel %.2e\n",
                tag.c_str(), worst_p, worst_c, worst_r, worst_uv);
    check(worst_p < 2e-5, tag + ": points moved by T");
    check(worst_c < 2e-5, tag + ": camera centres moved by T");
    check(worst_r < 2e-6, tag + ": camera axes turned, not scaled");
    check(worst_uv < 5e-3, tag + ": every point still lands on its pixel");
}

}  // namespace

int main() {
    std::mt19937 rng(5);
    std::normal_distribution<double> g(0.0, 1.0);
    std::vector<Cam> cams(6);
    for (size_t i = 0; i < cams.size(); i++) {
        double n = 0;
        // Near the identity, so the scene below stays in front of them.
        cams[i].q[0] = 1.0;
        for (int k = 1; k < 4; k++) cams[i].q[k] = 0.15 * g(rng);
        for (double v : cams[i].q) n += v * v;
        for (double& v : cams[i].q) v /= std::sqrt(n);
        for (int k = 0; k < 3; k++) cams[i].t[k] = 0.5 * g(rng);
        cams[i].t[2] += 6.0;
        cams[i].name = "img_" + std::to_string(i) + ".jpg";
    }
    std::vector<double> pts(40 * 3);
    for (double& v : pts) v = g(rng);

    const double axis[3] = {0.6, 0.48, -0.64}, pivot[3] = {0.5, -1.0, 2.0};
    Sim3 T = Sim3::rotation_about(axis, 0.9, pivot);
    T.s = 2.5;
    T.t[0] += 3.0; T.t[1] -= 1.5; T.t[2] += 0.75;
    const double axis2[3] = {0, 0, 1}, origin[3] = {0, 0, 0};
    Sim3 T2 = Sim3::rotation_about(axis2, -0.4, origin);
    T2.s = 0.5;

    const fs::path root = fs::temp_directory_path() / "ss_sparse_transform_test";
    std::error_code ec;
    fs::remove_all(root, ec);

    std::vector<int64_t> all_cams, all_pts;
    for (int64_t i = 0; i < (int64_t)cams.size(); i++) all_cams.push_back(i);
    for (int64_t i = 0; i < (int64_t)pts.size() / 3; i++) all_pts.push_back(i);
    spirula::SparseKeep keep_all;
    keep_all.points.assign(pts.size() / 3, 1);

    for (bool text : {false, true}) {
        const std::string tag = text ? "COLMAP text" : "COLMAP binary";
        const fs::path dir = root / (text ? "colmap_txt" : "colmap_bin");
        write_colmap(dir / "sparse" / "0", cams, pts, text);
        const ParsedDataset before = parse(dir);
        spirula::SparseBaseline base;
        spirula::sparse_write_filtered(dir.string(), keep_all, &T, &base);
        const ParsedDataset after = parse(dir);
        compare(before, after, T, all_cams, all_pts, tag);
        check(!before.edited_in_place && after.edited_in_place,
              tag + ": a saved edit is known for one when it is next opened");

        // A second save of the same session starts from the same baseline:
        // T2 of the original, not T2 of what the first save wrote -- and the
        // keep flags still index the original rows.
        spirula::SparseKeep some = keep_all;
        some.points[3] = some.points[17] = 0;
        some.drop_images.push_back("img_2.jpg");
        spirula::sparse_write_filtered(dir.string(), some, &T2, &base);
        std::vector<int64_t> c2, p2;
        for (int64_t i : all_cams) if (i != 2) c2.push_back(i);
        for (int64_t i : all_pts) if (i != 3 && i != 17) p2.push_back(i);
        compare(before, parse(dir), T2, c2, p2, tag + ", second save");
    }

    {
        // Nerfstudio's own COLMAP convention plus a shift, so a transform that
        // forgot the conjugation cannot pass by luck.
        const double A[12] = {0, 1, 0, 0.7,  1, 0, 0, -1.2,  0, 0, -1, 0.4};
        const fs::path dir = root / "nerfstudio";
        write_nerfstudio(dir, cams, pts, A);
        const ParsedDataset before = parse(dir);
        spirula::SparseBaseline base;
        spirula::sparse_write_filtered(dir.string(), keep_all, &T, &base);
        const ParsedDataset after = parse(dir);
        compare(before, after, T, all_cams, all_pts, "Nerfstudio");
        check(!before.edited_in_place && after.edited_in_place,
              "Nerfstudio: a saved edit is known for one when it is next opened");
        spirula::sparse_write_filtered(dir.string(), keep_all, &T2, &base);
        compare(before, parse(dir), T2, all_cams, all_pts,
                "Nerfstudio, second save");
    }

    {
        // compute_normalized_transform with secondary methods
        // 4 cameras arranged around (0, 0, 5) looking at it
        std::vector<double> c2w(4 * 12, 0.0);
        const double pos[4][3] = {{-2, 0, 0}, {2, 0, 0}, {0, -2, 0}, {0, 2, 0}};
        const double target[3] = {0, 0, 5};
        for (int i = 0; i < 4; i++) {
            // Optical axis -Z points to target - pos
            double dir[3] = {target[0] - pos[i][0], target[1] - pos[i][1], target[2] - pos[i][2]};
            double len = std::sqrt(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]);
            for (int k = 0; k < 3; k++) dir[k] /= len;
            // camera -Z = dir => camera Z = -dir
            double z[3] = {-dir[0], -dir[1], -dir[2]};
            double up_world[3] = {0, 1, 0};
            if (std::abs(z[1]) > 0.9) up_world[0] = 1, up_world[1] = 0;
            // x = up x z
            double x[3] = {up_world[1]*z[2] - up_world[2]*z[1],
                           up_world[2]*z[0] - up_world[0]*z[2],
                           up_world[0]*z[1] - up_world[1]*z[0]};
            double xl = std::sqrt(x[0]*x[0] + x[1]*x[1] + x[2]*x[2]);
            for (int k = 0; k < 3; k++) x[k] /= xl;
            // y = z x x
            double y[3] = {z[1]*x[2] - z[2]*x[1],
                           z[2]*x[0] - z[0]*x[2],
                           z[0]*x[1] - z[1]*x[0]};
            for (int r = 0; r < 3; r++) {
                c2w[i*12 + r*4 + 0] = x[r];
                c2w[i*12 + r*4 + 1] = y[r];
                c2w[i*12 + r*4 + 2] = z[r];
                c2w[i*12 + r*4 + 3] = pos[i][r];
            }
        }

        double T[16], R[9];
        // 1. None orientation, none center, no auto scale
        double s = dsparse::compute_normalized_transform(c2w.data(), 4, T, R, nullptr,
                                                        "none", "none", false);
        check(std::abs(s - 1.0) < 1e-9, "none/none: scale_factor is 1.0 when auto_scale_poses is false");
        bool r_ident = true;
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++)
                if (std::abs(R[r*3 + c] - (r == c ? 1.0 : 0.0)) > 1e-9) r_ident = false;
        check(r_ident, "none orientation produces identity rotation");
        check(std::abs(T[3]) < 1e-9 && std::abs(T[7]) < 1e-9 && std::abs(T[11]) < 1e-9,
              "none center leaves translation at zero");

        // 2. PCA orientation
        s = dsparse::compute_normalized_transform(c2w.data(), 4, T, R, nullptr,
                                                 "pca", "poses", true);
        check(s > 0.0, "pca orientation produces positive scale");
        // Verify R is orthonormal with det = 1
        double detR = R[0]*(R[4]*R[8] - R[5]*R[7]) - R[1]*(R[3]*R[8] - R[5]*R[6]) + R[2]*(R[3]*R[7] - R[4]*R[6]);
        check(std::abs(detR - 1.0) < 1e-6, "pca rotation has determinant 1");

        // 3. Vertical orientation
        s = dsparse::compute_normalized_transform(c2w.data(), 4, T, R, nullptr,
                                                 "vertical", "poses", true);
        detR = R[0]*(R[4]*R[8] - R[5]*R[7]) - R[1]*(R[3]*R[8] - R[5]*R[6]) + R[2]*(R[3]*R[7] - R[4]*R[6]);
        check(std::abs(detR - 1.0) < 1e-6, "vertical rotation has determinant 1");

        // 4. Focus centering
        s = dsparse::compute_normalized_transform(c2w.data(), 4, T, R, nullptr,
                                                 "none", "focus", true);
        // Translation in T: t_norm = s * (-R @ center) = -s * center (since R is I)
        double foc[3] = {-T[3] / s, -T[7] / s, -T[11] / s};
        check(std::abs(foc[0] - target[0]) < 1e-3 &&
              std::abs(foc[1] - target[1]) < 1e-3 &&
              std::abs(foc[2] - target[2]) < 1e-3,
              "focus centering finds camera optical intersection");
    }

    fs::remove_all(root, ec);
    std::printf("%s\n", g_failures ? "FAILED" : "all passed");
    return g_failures ? 1 : 0;
}

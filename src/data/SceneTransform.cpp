// SceneTransform.cpp -- see SceneTransform.h.

#include "data/SceneTransform.h"

#include "data/Json.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace spirula {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = 180.0 / kPi;

// Right-handed rotation about axis `n` (0 = x, 1 = y, 2 = z), row-major.
void axis_rotation(int n, double angle, double R[9]) {
    const double c = std::cos(angle), s = std::sin(angle);
    const int i = (n + 1) % 3, j = (n + 2) % 3;
    std::fill(R, R + 9, 0.0);
    R[n*3 + n] = 1.0;
    R[i*3 + i] = c;  R[i*3 + j] = -s;
    R[j*3 + i] = s;  R[j*3 + j] = c;
}

void mat3_mul(const double A[9], const double B[9], double out[9]) {
    double t[9];
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            t[r*3 + c] = A[r*3]*B[c] + A[r*3+1]*B[3+c] + A[r*3+2]*B[6+c];
    std::copy(t, t + 9, out);
}

int axis_index(char c) {
    switch (c) {
        case 'x': case 'X': return 0;
        case 'y': case 'Y': return 1;
        case 'z': case 'Z': return 2;
    }
    return -1;
}

// ---- JSON text ------------------------------------------------------------

std::string num(double v) {
    if (v == 0.0) v = 0.0;   // no "-0"
    char buf[40];
    std::snprintf(buf, sizeof buf, "%.17g", v);
    return buf;
}

std::string arr(const double* v, int n) {
    std::string s = "[";
    for (int i = 0; i < n; i++) s += (i ? ", " : "") + num(v[i]);
    return s + "]";
}

// Rows of a row-major matrix as nested arrays.
std::string rows(const double* m, int nrow, int ncol) {
    std::string s = "[";
    for (int r = 0; r < nrow; r++)
        s += (r ? ", " : "") + arr(m + r * ncol, ncol);
    return s + "]";
}

std::string transposed(const double* m, int nrow, int ncol) {
    std::string s = "[";
    for (int c = 0; c < ncol; c++)
        for (int r = 0; r < nrow; r++)
            s += ((r || c) ? ", " : "") + num(m[r*ncol + c]);
    return s + "]";
}

std::string quoted(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out + "\"";
}

std::string euler_block(const double R[9], const char* pad) {
    static const char* kOrders[] = {"xyz", "xzy", "yxz", "yzx", "zxy", "zyx"};
    std::string s;
    auto entry = [&](const char* name, const double a[3], bool last) {
        double deg[3] = {a[0]*kDeg, a[1]*kDeg, a[2]*kDeg};
        s += std::string(pad) + "  \"" + name + "\": {\"radians\": " + arr(a, 3) +
             ", \"degrees\": " + arr(deg, 3) + "}" + (last ? "\n" : ",\n");
    };
    s += std::string(pad) + "\"intrinsic\": {\n";
    for (int i = 0; i < 6; i++) {
        double a[3];
        rotation_to_euler_intrinsic(R, kOrders[i], a);
        entry(kOrders[i], a, i == 5);
    }
    s += std::string(pad) + "},\n";
    s += std::string(pad) + "\"extrinsic\": {\n";
    for (int i = 0; i < 6; i++) {
        // Fixed-axis abc is body-axis cba with the angles read backwards.
        char rev[3] = {kOrders[i][2], kOrders[i][1], kOrders[i][0]};
        double a[3], b[3];
        rotation_to_euler_intrinsic(R, rev, a);
        b[0] = a[2]; b[1] = a[1]; b[2] = a[0];
        entry(kOrders[i], b, i == 5);
    }
    s += std::string(pad) + "}\n";
    return s;
}

std::string similarity_block(const SceneTransform& T, const char* pad) {
    const std::string p(pad);
    double q[4];
    rotation_to_quaternion(T.R, q);
    const double qxyzw[4] = {q[1], q[2], q[3], q[0]};

    // Axis-angle from the quaternion; the axis of a zero rotation is +Z by
    // convention (any unit vector would do).
    double angle = 2.0 * std::atan2(std::sqrt(q[1]*q[1] + q[2]*q[2] + q[3]*q[3]), q[0]);
    double axis[3] = {0, 0, 1};
    const double sn = std::sqrt(q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    if (sn > 1e-15) for (int i = 0; i < 3; i++) axis[i] = q[1 + i] / sn;
    const double rotvec[3] = {axis[0]*angle, axis[1]*angle, axis[2]*angle};

    // p_out = s R (p + t_pre)  <=>  t = s R t_pre.
    double t_pre[3];
    for (int r = 0; r < 3; r++)
        t_pre[r] = (T.R[r]*T.t[0] + T.R[3+r]*T.t[1] + T.R[6+r]*T.t[2]) / T.scale;

    double M[16] = {0};
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) M[r*4 + c] = T.scale * T.R[r*3 + c];
        M[r*4 + 3] = T.t[r];
    }
    M[15] = 1.0;

    std::string s;
    s += p + "\"scale\": " + num(T.scale) + ",\n";
    s += p + "\"translation\": " + arr(T.t, 3) + ",\n";
    s += p + "\"translation_before_rotation\": " + arr(t_pre, 3) + ",\n";
    s += p + "\"rotation\": {\n";
    s += p + "  \"matrix_3x3\": " + rows(T.R, 3, 3) + ",\n";
    s += p + "  \"matrix_3x3_flat_row_major\": " + arr(T.R, 9) + ",\n";
    s += p + "  \"matrix_3x3_flat_column_major\": " + transposed(T.R, 3, 3) + ",\n";
    s += p + "  \"quaternion_wxyz\": " + arr(q, 4) + ",\n";
    s += p + "  \"quaternion_xyzw\": " + arr(qxyzw, 4) + ",\n";
    s += p + "  \"axis_angle\": {\"axis\": " + arr(axis, 3) + ", \"angle_rad\": " +
         num(angle) + ", \"angle_deg\": " + num(angle * kDeg) + "},\n";
    s += p + "  \"rotation_vector\": " + arr(rotvec, 3) + ",\n";
    s += p + "  \"euler\": {\n";
    s += euler_block(T.R, (p + "    ").c_str());
    s += p + "  }\n";
    s += p + "},\n";
    s += p + "\"matrix_4x4\": " + rows(M, 4, 4) + ",\n";
    s += p + "\"matrix_4x4_flat_row_major\": " + arr(M, 16) + ",\n";
    s += p + "\"matrix_4x4_flat_column_major\": " + transposed(M, 4, 4) + ",\n";
    s += p + "\"matrix_3x4_flat_row_major\": " + arr(M, 12) + "\n";
    return s;
}

}  // namespace

// ---------------------------------------------------------------------------

SceneTransform SceneTransform::inverse() const {
    SceneTransform inv;
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) inv.R[r*3 + c] = R[c*3 + r];
    inv.scale = 1.0 / scale;
    for (int r = 0; r < 3; r++)
        inv.t[r] = -(inv.R[r*3]*t[0] + inv.R[r*3+1]*t[1] + inv.R[r*3+2]*t[2]) * inv.scale;
    return inv;
}

// Shepperd's method: divide by the largest of the four candidates, which is
// what keeps the result exact for every rotation angle including 180 degrees.
void rotation_to_quaternion(const double R[9], double q[4]) {
    const double tr = R[0] + R[4] + R[8];
    double w, x, y, z;
    if (tr > 0.0) {
        const double s = std::sqrt(tr + 1.0) * 2.0;
        w = 0.25 * s;
        x = (R[7] - R[5]) / s;
        y = (R[2] - R[6]) / s;
        z = (R[3] - R[1]) / s;
    } else if (R[0] > R[4] && R[0] > R[8]) {
        const double s = std::sqrt(1.0 + R[0] - R[4] - R[8]) * 2.0;
        w = (R[7] - R[5]) / s;
        x = 0.25 * s;
        y = (R[1] + R[3]) / s;
        z = (R[2] + R[6]) / s;
    } else if (R[4] > R[8]) {
        const double s = std::sqrt(1.0 + R[4] - R[0] - R[8]) * 2.0;
        w = (R[2] - R[6]) / s;
        x = (R[1] + R[3]) / s;
        y = 0.25 * s;
        z = (R[5] + R[7]) / s;
    } else {
        const double s = std::sqrt(1.0 + R[8] - R[0] - R[4]) * 2.0;
        w = (R[3] - R[1]) / s;
        x = (R[2] + R[6]) / s;
        y = (R[5] + R[7]) / s;
        z = 0.25 * s;
    }
    const double n = std::sqrt(w*w + x*x + y*y + z*z);
    q[0] = w / n; q[1] = x / n; q[2] = y / n; q[3] = z / n;
    if (q[0] < 0.0) for (int i = 0; i < 4; i++) q[i] = -q[i];
}

void euler_intrinsic_to_rotation(const char order[3], const double a[3],
                                 double R[9]) {
    double acc[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1}, step[9];
    for (int k = 0; k < 3; k++) {
        axis_rotation(axis_index(order[k]), a[k], step);
        mat3_mul(acc, step, acc);
    }
    std::copy(acc, acc + 9, R);
}

// For R = R_i(a) R_j(b) R_k(c): R[i][k] = e sin b with e = +1 when ijk is a
// cyclic permutation, and the other two angles fall out of the row and column
// through that entry. At |sin b| = 1 only a +- c is determined; c is set to 0.
void rotation_to_euler_intrinsic(const double R[9], const char order[3],
                                 double out[3]) {
    const int i = axis_index(order[0]), j = axis_index(order[1]),
              k = axis_index(order[2]);
    const double e = ((j == (i + 1) % 3) ? 1.0 : -1.0);
    auto at = [&](int r, int c) { return R[r*3 + c]; };
    const double sb = std::clamp(e * at(i, k), -1.0, 1.0);
    out[1] = std::asin(sb);
    if (std::fabs(sb) < 1.0 - 1e-12) {
        out[0] = std::atan2(-e * at(j, k), at(k, k));
        out[2] = std::atan2(-e * at(i, j), at(i, i));
    } else {
        out[0] = std::atan2(sb * at(j, i), at(j, j));
        out[2] = 0.0;
    }
}

std::string scene_transform_json(const SceneTransform& train_from_world,
                                 const std::string& center_mode,
                                 const double center_world[3]) {
    const SceneTransform world_from_train = train_from_world.inverse();
    std::string s = "{\n";
    s += "  \"format\": \"spirula-scene-transform\",\n";
    s += "  \"version\": 1,\n";
    s += "  \"readme\": [\n";
    const char* lines[] = {
        "p_train = train_from_world.scale * train_from_world.rotation.matrix_3x3 "
        "@ p_world + train_from_world.translation, for a column vector p.",
        "train_from_world is the ACTIVE transform taking a point of the "
        "dataset's own frame (world) into the frame the splats were trained in "
        "(train); read passively, it gives a fixed point's train coordinates "
        "from its world coordinates. world_from_train is its inverse.",
        "matrix_4x4 acts on column vectors [x, y, z, 1]. "
        "matrix_4x4_flat_row_major lists it row by row; "
        "matrix_4x4_flat_column_major column by column, which is also the "
        "row-major layout of its transpose, the matrix that acts on row "
        "vectors [x, y, z, 1] @ M^T.",
        "rotation.matrix_3x3 acts on column vectors. quaternion_wxyz is "
        "(w, x, y, z) and quaternion_xyzw is (x, y, z, w), the same unit "
        "quaternion. axis_angle is a unit axis with a right-handed angle; "
        "rotation_vector is axis * angle_rad.",
        "euler.intrinsic.abc = [a1, a2, a3] means R = R_a(a1) @ R_b(a2) @ "
        "R_c(a3): rotate about the body axis a, then the new b, then the new "
        "c. euler.extrinsic.abc = [a1, a2, a3] means R = R_c(a3) @ R_b(a2) @ "
        "R_a(a1): rotate about the fixed axis a, then b, then c. So "
        "extrinsic.xyz = [roll about X, pitch about Y, yaw about Z] applied in "
        "that order, and intrinsic.zyx = [yaw, pitch, roll]. Every entry is "
        "given in radians and in degrees.",
        "translation is applied AFTER the rotation and scale (p_out = s R p + "
        "t). translation_before_rotation is the equivalent shift applied first "
        "(p_out = s R (p + t_pre)); for train_from_world it is minus "
        "centering.center_world.",
    };
    const int n_lines = (int)(sizeof(lines) / sizeof(lines[0]));
    for (int i = 0; i < n_lines; i++)
        s += "    " + quoted(lines[i]) + (i + 1 < n_lines ? ",\n" : "\n");
    s += "  ],\n";
    s += "  \"centering\": {\"mode\": " + quoted(center_mode) +
         ", \"center_world\": " + arr(center_world, 3) + "},\n";
    s += "  \"train_from_world\": {\n";
    s += similarity_block(train_from_world, "    ");
    s += "  },\n";
    s += "  \"world_from_train\": {\n";
    s += similarity_block(world_from_train, "    ");
    s += "  }\n";
    s += "}\n";
    return s;
}

bool read_scene_transform_json(const std::string& path, SceneTransform& out) {
    JsonValue root;
    try {
        root = json_parse_file(path);
    } catch (const std::exception&) {
        return false;
    }
    const JsonValue* T = root.find("train_from_world");
    if (!T || !T->is_object()) return false;
    const JsonValue* rot = T->find("rotation");
    const JsonValue* R = rot ? rot->find("matrix_3x3_flat_row_major") : nullptr;
    const JsonValue* t = T->find("translation");
    if (!R || !R->is_array() || R->arr.size() != 9 || !t || !t->is_array() || t->arr.size() != 3)
        return false;
    SceneTransform s;
    s.scale = T->get_double("scale", 1.0);
    for (int i = 0; i < 9; i++) s.R[i] = R->arr[(size_t)i].as_double();
    for (int i = 0; i < 3; i++) s.t[i] = t->arr[(size_t)i].as_double();
    if (!(s.scale > 0)) return false;
    out = s;
    return true;
}

}  // namespace spirula

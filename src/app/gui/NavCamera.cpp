// NavCamera.cpp -- see NavCamera.h. Line-for-line port of viewer.html's
// v3 / quat helpers, `cam`, and `Nav`.

#include "app/gui/NavCamera.h"

#include <GLFW/glfw3.h>

#include <cmath>

namespace gui {

namespace {

constexpr float kGamepadDeadzone = 0.1f;

// One pad, triggers rescaled to the browser's [0,1]. GLFW leaves an axis the
// mapping omits at 0 while a real trigger rests at -1, so rest is tracked per
// pad: a device with one trigger mapped would otherwise roll the view forever.
bool read_pad(int jid, GLFWgamepadstate& st, float& lt, float& rt) {
    static float rest[GLFW_JOYSTICK_LAST + 1][2];
    static bool seen[GLFW_JOYSTICK_LAST + 1];
    if (!glfwGetGamepadState(jid, &st)) {
        seen[jid] = false;
        return false;
    }
    const float l = st.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER];
    const float r = st.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER];
    if (!seen[jid]) {
        rest[jid][0] = l;
        rest[jid][1] = r;
        seen[jid] = true;
    }
    rest[jid][0] = std::fmin(rest[jid][0], l);
    rest[jid][1] = std::fmin(rest[jid][1], r);
    lt = 0.5f * (l - rest[jid][0]);
    rt = 0.5f * (r - rest[jid][1]);
    return true;
}

// ---- v3 (viewer.html) -------------------------------------------------------

void v_add(const float a[3], const float b[3], float o[3]) {
    o[0] = a[0]+b[0]; o[1] = a[1]+b[1]; o[2] = a[2]+b[2];
}
void v_sub(const float a[3], const float b[3], float o[3]) {
    o[0] = a[0]-b[0]; o[1] = a[1]-b[1]; o[2] = a[2]-b[2];
}
void v_scale(const float a[3], float s, float o[3]) {
    o[0] = a[0]*s; o[1] = a[1]*s; o[2] = a[2]*s;
}
void v_cross(const float a[3], const float b[3], float o[3]) {
    o[0] = a[1]*b[2] - a[2]*b[1];
    o[1] = a[2]*b[0] - a[0]*b[2];
    o[2] = a[0]*b[1] - a[1]*b[0];
}
float v_len(const float a[3]) {
    return std::sqrt(a[0]*a[0] + a[1]*a[1] + a[2]*a[2]);
}
void v_norm(const float a[3], float o[3]) {
    float l = v_len(a);
    if (l > 1e-12f) { o[0] = a[0]/l; o[1] = a[1]/l; o[2] = a[2]/l; }
    else            { o[0] = 0; o[1] = 0; o[2] = 1; }
}

// ---- quat (viewer.html; [x,y,z,w]) -----------------------------------------

void q_mul(const float a[4], const float b[4], float o[4]) {
    float r[4] = {
        a[3]*b[0] + a[0]*b[3] + a[1]*b[2] - a[2]*b[1],
        a[3]*b[1] - a[0]*b[2] + a[1]*b[3] + a[2]*b[0],
        a[3]*b[2] + a[0]*b[1] - a[1]*b[0] + a[2]*b[3],
        a[3]*b[3] - a[0]*b[0] - a[1]*b[1] - a[2]*b[2],
    };
    o[0] = r[0]; o[1] = r[1]; o[2] = r[2]; o[3] = r[3];
}

void q_from_axis_angle(const float axis[3], float angle, float o[4]) {
    float n[3];
    v_norm(axis, n);
    float s = std::sin(angle / 2);
    o[0] = n[0]*s; o[1] = n[1]*s; o[2] = n[2]*s;
    o[3] = std::cos(angle / 2);
}

void q_norm(float q[4]) {
    float l = std::sqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    if (l > 1e-12f) { q[0] /= l; q[1] /= l; q[2] /= l; q[3] /= l; }
    else            { q[0] = q[1] = q[2] = 0; q[3] = 1; }
}

void q_rot_vec(const float q[4], const float v[3], float o[3]) {
    float x = q[0], y = q[1], z = q[2], w = q[3];
    float tx = 2*(y*v[2] - z*v[1]), ty = 2*(z*v[0] - x*v[2]), tz = 2*(x*v[1] - y*v[0]);
    float r[3] = {
        v[0] + w*tx + y*tz - z*ty,
        v[1] + w*ty + z*tx - x*tz,
        v[2] + w*tz + x*ty - y*tx,
    };
    o[0] = r[0]; o[1] = r[1]; o[2] = r[2];
}

// mat3ToQuat port (column-major 3x3 input).
void mat3_to_quat(const float m[9], float o[4]) {
    float m00 = m[0], m10 = m[1], m20 = m[2];
    float m01 = m[3], m11 = m[4], m21 = m[5];
    float m02 = m[6], m12 = m[7], m22 = m[8];
    float trace = m00 + m11 + m22;
    if (trace > 0) {
        float s = 0.5f / std::sqrt(trace + 1);
        o[0] = (m21 - m12) * s; o[1] = (m02 - m20) * s;
        o[2] = (m10 - m01) * s; o[3] = 0.25f / s;
    } else if (m00 > m11 && m00 > m22) {
        float s = 2 * std::sqrt(1 + m00 - m11 - m22);
        o[0] = 0.25f * s;       o[1] = (m01 + m10) / s;
        o[2] = (m02 + m20) / s; o[3] = (m21 - m12) / s;
    } else if (m11 > m22) {
        float s = 2 * std::sqrt(1 + m11 - m00 - m22);
        o[0] = (m01 + m10) / s; o[1] = 0.25f * s;
        o[2] = (m12 + m21) / s; o[3] = (m02 - m20) / s;
    } else {
        float s = 2 * std::sqrt(1 + m22 - m00 - m11);
        o[0] = (m02 + m20) / s; o[1] = (m12 + m21) / s;
        o[2] = 0.25f * s;       o[3] = (m10 - m01) / s;
    }
}

const float kWorldUp[3] = {0, 0, 1};

}  // namespace


float NavCamera::speed() const { return std::pow(10.0f, speed_exp); }

void NavCamera::c2w(float out[12]) const {
    float x = rot[0], y = rot[1], z = rot[2], w = rot[3];
    float x2 = x*x, y2 = y*y, z2 = z*z;
    out[0] = 1 - 2*(y2 + z2); out[1] = 2*(x*y - z*w); out[2]  = 2*(x*z + y*w); out[3]  = pos[0];
    out[4] = 2*(x*y + z*w);   out[5] = 1 - 2*(x2 + z2); out[6] = 2*(y*z - x*w); out[7]  = pos[1];
    out[8] = 2*(x*z - y*w);   out[9] = 2*(y*z + x*w); out[10] = 1 - 2*(x2 + y2); out[11] = pos[2];
}

void NavCamera::axis_right(float v[3]) const {
    const float e[3] = {1, 0, 0};
    q_rot_vec(rot, e, v);
}
void NavCamera::axis_up(float v[3]) const {
    const float e[3] = {0, 1, 0};
    q_rot_vec(rot, e, v);
}
void NavCamera::axis_forward(float v[3]) const {
    const float e[3] = {0, 0, -1};
    q_rot_vec(rot, e, v);
}

void NavCamera::orbit(float dx, float dy) {
    const float sensitivity = 0.005f;
    float right[3], up[3];
    axis_right(right);
    if (mode == Trackball) axis_up(up);
    else { up[0] = kWorldUp[0]; up[1] = kWorldUp[1]; up[2] = kWorldUp[2]; }
    float qa[4], qb[4], qab[4];
    q_from_axis_angle(up, -dx * sensitivity, qa);
    q_from_axis_angle(right, -dy * sensitivity, qb);
    q_mul(qa, qb, qab);
    q_mul(qab, rot, rot);
    q_norm(rot);
    // Keep camera on the orbit sphere.
    float d[3];
    v_sub(pos, target, d);
    float dist = v_len(d);
    const float back[3] = {0, 0, 1};
    float new_fwd[3];
    q_rot_vec(rot, back, new_fwd);
    v_scale(new_fwd, dist, new_fwd);
    v_add(target, new_fwd, pos);
}

void NavCamera::look(float dx, float dy) {
    const float s = 0.003f;
    float up[3], right[3], qa[4], qb[4];
    axis_up(up);
    axis_right(right);
    q_from_axis_angle(up, -dx * s, qa);
    q_from_axis_angle(right, -dy * s, qb);
    if (mode == Fps) {
        // Constrain roll: yaw around local-y, pitch around (old) local-x.
        const float local_y[3] = {0, 1, 0};
        float qy[4];
        q_from_axis_angle(local_y, -dx * s, qy);
        q_mul(rot, qy, rot);
        q_norm(rot);
        q_mul(qb, rot, rot);
        q_norm(rot);
    } else {
        float qab[4];
        q_mul(qa, qb, qab);
        q_mul(qab, rot, rot);
        q_norm(rot);
    }
}

void NavCamera::pan(float dx, float dy) {
    const float s = speed() * 0.002f;
    float right[3], up[3], d1[3], d2[3], delta[3];
    axis_right(right);
    axis_up(up);
    v_scale(right, -dx * s, d1);
    v_scale(up, dy * s, d2);
    v_add(d1, d2, delta);
    v_add(pos, delta, pos);
    v_add(target, delta, target);
}

void NavCamera::dolly(float delta) {
    const float s = speed() * 0.2f;
    if (mode == Trackball || mode == Turntable) {
        // Scale around the target.
        float d[3];
        v_sub(pos, target, d);
        v_scale(d, std::exp(delta * 0.004f * s), d);
        v_add(target, d, pos);
    } else {
        // Move forward.
        float fwd[3], step[3];
        axis_forward(fwd);
        v_scale(fwd, -delta * 0.004f * s, step);
        v_add(pos, step, pos);
        v_add(target, step, target);
    }
}

void NavCamera::roll(float delta) {
    float fwd[3], q[4];
    axis_forward(fwd);
    q_from_axis_angle(fwd, delta, q);
    q_mul(q, rot, rot);
    q_norm(rot);
}

bool NavCamera::keyboard_tick(float dt, const Keys& k) {
    const float s = speed() * dt * 1.0f;   // translate speed
    const float sr = dt * 1.0f;            // rotate speed
    float fwd[3], right[3], up[3];
    axis_forward(fwd);
    axis_right(right);
    if (mode == Turntable || mode == Fps) {
        up[0] = kWorldUp[0]; up[1] = kWorldUp[1]; up[2] = kWorldUp[2];
    } else {
        axis_up(up);
    }
    float move[3] = {0, 0, 0}, step[3];
    float rotate = 0.0f;
    if (k.w || k.up)    { v_scale(fwd, s, step);    v_add(move, step, move); }
    if (k.s || k.down)  { v_scale(fwd, -s, step);   v_add(move, step, move); }
    if (k.a || k.left)  { v_scale(right, -s, step); v_add(move, step, move); }
    if (k.d || k.right) { v_scale(right, s, step);  v_add(move, step, move); }
    if (mode == Fly) {
        if (k.e) rotate -= sr;
        if (k.q) rotate += sr;
    } else {
        if (k.e) { v_scale(up, s, step);  v_add(move, step, move); }
        if (k.q) { v_scale(up, -s, step); v_add(move, step, move); }
    }
    bool moved = false;
    if (v_len(move) > 1e-12f) {
        v_add(pos, move, pos);
        v_add(target, move, target);
        moved = true;
    }
    if (rotate != 0.0f) {
        roll(rotate);
        moved = true;
    }
    return moved;
}

bool NavCamera::gamepad_tick(float dt) {
    bool moved = false;
    for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; jid++) {
        GLFWgamepadstate st;
        float lt = 0.0f, rt = 0.0f;
        if (!read_pad(jid, st, lt, rt)) continue;
        float lx = st.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
        float ly = st.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];
        float rx = st.axes[GLFW_GAMEPAD_AXIS_RIGHT_X];
        float ry = st.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y];
        float roll_input = lt - rt;
        // Movement (triggers only apply while the left stick is deflected,
        // matching the browser implementation).
        if (std::fabs(lx) > kGamepadDeadzone ||
            std::fabs(ly) > kGamepadDeadzone) {
            const float s = speed() * dt * 1.0f;
            float fwd[3], right[3], move[3], step[3];
            axis_forward(fwd);
            axis_right(right);
            v_scale(right, lx * s, move);
            v_scale(fwd, -ly * s, step);
            v_add(move, step, move);
            float up_step[3];
            v_scale(kWorldUp, (rt - lt) * s, up_step);
            v_add(move, up_step, move);
            v_add(pos, move, pos);
            v_add(target, move, target);
            moved = true;
        }
        // Look
        if (std::fabs(rx) > kGamepadDeadzone ||
            std::fabs(ry) > kGamepadDeadzone) {
            look(rx * dt * 400.0f, ry * dt * 400.0f);
            moved = true;
        }
        // Roll
        if (std::fabs(roll_input) > kGamepadDeadzone) {
            roll(roll_input * dt * 1.0f);
            moved = true;
        }
    }
    return moved;
}

// Stick and trigger input reaches no window event, so the GUI loop cannot
// pace its frame rate on events alone.
bool gamepad_deflected() {
    for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; jid++) {
        GLFWgamepadstate st;
        float lt = 0.0f, rt = 0.0f;
        if (!read_pad(jid, st, lt, rt)) continue;
        if (std::fabs(st.axes[GLFW_GAMEPAD_AXIS_LEFT_X]) > kGamepadDeadzone ||
            std::fabs(st.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]) > kGamepadDeadzone ||
            std::fabs(st.axes[GLFW_GAMEPAD_AXIS_RIGHT_X]) > kGamepadDeadzone ||
            std::fabs(st.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]) > kGamepadDeadzone ||
            std::fabs(lt - rt) > kGamepadDeadzone)
            return true;
    }
    return false;
}

void NavCamera::look_at(const float eye[3], const float tgt[3],
                        const float up_world[3]) {
    float f[3], r[3], u[3], fr[3];
    v_sub(tgt, eye, fr);
    v_norm(fr, f);
    v_cross(f, up_world, r);
    v_norm(r, r);
    v_cross(r, f, u);
    // Columns X=right, Y=up, Z=-forward (OpenGL camera basis).
    float m[9] = {r[0], r[1], r[2], u[0], u[1], u[2], -f[0], -f[1], -f[2]};
    mat3_to_quat(m, rot);
    q_norm(rot);
    pos[0] = eye[0]; pos[1] = eye[1]; pos[2] = eye[2];
    target[0] = tgt[0]; target[1] = tgt[1]; target[2] = tgt[2];
}

}  // namespace gui

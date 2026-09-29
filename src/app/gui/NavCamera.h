#pragma once

// NavCamera -- 1:1 C++ port of the web viewer's camera state + Nav object
// (viewer.html: `cam`, `quat`, `Nav`). Same quaternion camera (OpenGL
// convention c2w, -Z forward), same four navigation modes (Turntable /
// Trackball / First Person / Free Fly), and the same sensitivities, so the
// native viewport feels identical to the browser client:
//   orbit  0.005 rad/px      look   0.003 rad/px
//   pan    speed * 0.002 world-units/px
//   dolly  exp(delta * 0.004 * speed * 0.2) (orbit modes) / forward move
//   keyboard  speed * 1.0 units/s   gamepad look  0.4 rad/s per stick unit
// speed = 10^speed_exp (the browser's Move Speed slider).
//
// Keep edits in sync with viewer.html's Nav -- including its quirks (e.g.
// gamepad triggers only translate while the left stick is deflected).

namespace gui {

struct NavCamera {
    enum Mode { Turntable = 0, Trackball, Fps, Fly };

    float pos[3] = {0, 0, 1};
    float rot[4] = {0, 0, 0, 1};   // (x,y,z,w), camera-to-world rotation
    float target[3] = {0, 0, 0};   // orbit / turntable pivot
    Mode mode = Turntable;
    // What Turntable orbits about and E/Q move along: +Z, as in the browser,
    // unless the scene's own up is known to be another axis. Unit length.
    float world_up[3] = {0, 0, 1};
    float speed_exp = 0.0f;        // Move Speed slider; speed = 10^exp

    float speed() const;

    // Row-major 3x4 camera-to-world (quat.toMatrix3x4 port).
    void c2w(float out[12]) const;
    void axis_right(float v[3]) const;    // cam.right()
    void axis_up(float v[3]) const;       // cam.up()
    void axis_forward(float v[3]) const;  // cam.forward()

    // Nav.* ports; deltas in pixels (mouse) matching the browser.
    void orbit(float dx, float dy);
    void look(float dx, float dy);
    void pan(float dx, float dy);
    void dolly(float delta);               // browser wheel deltaY units
    void roll(float delta);                // radians

    struct Keys {
        bool w = false, a = false, s = false, d = false;
        bool e = false, q = false;
        bool up = false, down = false, left = false, right = false;
    };
    // Nav.keyboardTick; returns true when the camera moved.
    bool keyboard_tick(float dt, const Keys& k);

    // Nav.gamepadTick over all connected GLFW gamepads; returns true when
    // the camera moved.
    bool gamepad_tick(float dt);

    // Place the camera at `eye` looking at `tgt` (sets pos/rot/target).
    void look_at(const float eye[3], const float tgt[3], const float up_world[3]);

    // Carry the camera with a similarity of the world (row-major 3x4
    // [sR | t]), so what moved with it looks exactly as it did.
    void transform(const float S[12]);
    // Turn about the view axis until the camera's right is level with
    // world_up; a camera looking along world_up is left alone.
    void level_roll();
};

// Any connected gamepad deflected past gamepad_tick's deadzone.
bool gamepad_deflected();

// The ImGuiKey (as an int) of the fly key where `us_letter` (one of wasdqe)
// sits on a US board. imgui names a key by what the layout prints on it, and
// the fly keys are places, not letters: on AZERTY, forward is the Z key.
int fly_key(char us_letter);
bool is_fly_key(int imgui_key);

}  // namespace gui

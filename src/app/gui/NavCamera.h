#pragma once

// OpenGL camera (-Z forward, xyzw quaternion) with the web viewer's input
// sensitivities. Turntable navigation keeps the selected world up upright;
// Trackball and Free Fly permit roll.

namespace gui {

struct NavCamera {
    enum Mode { Turntable = 0, Trackball, Fps, Fly };

    float pos[3] = {0, 0, 1};
    float rot[4] = {0, 0, 0, 1};   // (x,y,z,w), camera-to-world rotation
    float target[3] = {0, 0, 0};   // orbit / turntable pivot
    int up_axis = 2;
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
    void rotate_world(const float R[9]);   // row-major world-basis rotation

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

#pragma once

// Driving this GUI from outside it.
//
// A loopback HTTP surface that lists the widgets currently on screen, injects
// mouse/keyboard input at them, and hands back the framebuffer as a PNG. Off
// unless SS_GUI_AUTOMATION is set, and the imgui item hooks it needs stay
// switched off with it, so a normal run pays one never-taken branch per item.
//
// Design, endpoints and the client: docs/notes/gui-automation.md.

#include <functional>
#include <string>
#include <vector>

namespace gui {
namespace automation {

// Reads SS_GUI_AUTOMATION and binds the server. Call after
// ImGui::CreateContext() and before the first frame; a bind failure is
// reported on stderr and leaves automation off.
void arm();
bool armed();

// What /ui/state reports about the application on top of what ImGui knows:
// a JSON object body without the braces, or "" for nothing. Called on the GUI
// thread at the end of every frame; /ui/state serves the last answer.
void set_state_source(std::function<std::string()> f);

// Between glfwPollEvents() and ImGui::NewFrame(): applies one step of the
// queued input. True while anything is still queued, which is what keeps the
// frame loop at its busy rate for the length of a script.
bool begin_frame();

// A drop the client asked for (/ui/drop), taken once after begin_frame() and
// handed on as GLFW hands a real one; empty when none came.
std::vector<std::string> take_drop();

// After the draw data has been rendered, with the GL context current:
// publishes the frame's item table and fills a pending screenshot request.
void end_frame(int fb_w, int fb_h);

void shutdown();

}  // namespace automation
}  // namespace gui

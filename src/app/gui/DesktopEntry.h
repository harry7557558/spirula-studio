#pragma once

// Linux: the desktop entry that gives the window its dock icon. GLFW cannot
// set a Wayland window's icon, so the shell finds it through <app id>.desktop,
// and the release is a bare binary with no installer to put one there.

namespace gui {

// The Wayland app ID and X11 class the window is created with, and the name
// of the entry the shell matches them against.
inline constexpr const char* kDesktopAppId = "spirula-studio";

// Points $XDG_DATA_HOME/applications/<id>.desktop at this binary and writes
// its icon, unless an entry this code did not write is already there. No-op
// off Linux.
void register_desktop_entry();

}  // namespace gui

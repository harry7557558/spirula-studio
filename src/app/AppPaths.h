#pragma once

// Where the app keeps things, and where it finds its own siblings.
//
// Three questions with platform-specific answers that several modules ask,
// answered once: the settings directory, the cache directory (model
// checkpoints, the COLMAP vocabulary tree), and where this executable is --
// which is how the GUI runs a reconstruction, since that is `spirula sfm`, i.e.
// this same binary again.

#include <string>

namespace app {

// Created on first call. Roaming config on Windows, $XDG_CONFIG_HOME on Linux.
std::string config_dir();

// Created on first call. LOCALAPPDATA on Windows, $XDG_CACHE_HOME on Linux.
// Large downloads live here, so it is deliberately not the config directory.
std::string cache_dir();

// The running executable and the directory holding it, resolved once; both ""
// if they cannot be determined. Spawning a tool goes through exe_path()
// (`<exe_path> sfm auto ...`): a PATH lookup could reach a different build.
std::string exe_path();
std::string exe_dir();

// macOS only. A Finder launch inherits launchd's PATH -- /usr/bin:/bin:
// /usr/sbin:/sbin -- so colmap, ffmpeg and python3 are invisible to the bundle
// though a shell finds them. Appends, so an inherited PATH still wins.
void add_desktop_search_paths();

}  // namespace app

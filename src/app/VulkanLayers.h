#pragma once

// Implicit Vulkan layers: DLLs the loader puts into every Vulkan program,
// screen recorders and overlays among them. The few known to crash this one
// are turned off, for this process and every program it starts, through the
// loader's VK_LOADER_LAYERS_DISABLE. SS_VK_KEEP_LAYERS=1 leaves them on.

#include <string>
#include <vector>

namespace app {

// Before the first Vulkan call. Notes the loader and its implicit layers for
// the crash report, and turns off the installed known-bad ones. Returns the
// products whose layer this call turned off (not ones a parent already had).
std::vector<std::string> prepare_vulkan_layers();

// What prepare_vulkan_layers() returned, for the window's log.
const std::vector<std::string>& vulkan_layers_turned_off();

}  // namespace app

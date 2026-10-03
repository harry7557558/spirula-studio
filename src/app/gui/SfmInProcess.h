#pragma once

// Running a reconstruction in this process rather than as a child.
//
// Its own translation unit because sfm/Pipeline.h pulls in the header-only SfM
// module -- 27 s to compile, against SfmRunner.cpp's 3 s -- and nothing else in
// the GUI should pay that. The interface below names no SfM type.
//
// Every SfM Vulkan device is a scoped member of the stage that made it, so the
// GPU is handed back when this returns; the GUI still sequences a run before
// training rather than sharing a VRAM budget with it.

#include "app/gui/SfmProgress.h"

#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace gui {

struct InProcessResult {
    // 0 ok, 2 failed, 3 partial, 4 no metric frame -- `spirula sfm auto`'s own
    // codes, and -1 when the settings could not be read at all.
    int exit_code = 0;
    bool cancelled = false;
    std::string error;   // "" unless the run could not start
};

// `args` is what `spirula sfm auto` would have been typed with, minus the
// program name. `log` sees every line the run produces, formatted as the CLI
// prints it; `on_status` sees where it is, in the shape the child's status.bin
// reader hands back, so one consumer serves both transports.
InProcessResult run_sfm_in_process(
    const std::vector<std::string>& args,
    const std::function<void(const std::string&)>& log,
    const std::function<void(const RunStatus&)>& on_status,
    const std::atomic<bool>& cancel);

// A repair of one finished model (sfm/Repair.h), in GUI terms.
struct RepairRequest {
    std::string workspace, model_dir, output_dir, image_dir;
    std::vector<std::string> replace, add, exclude;
    struct Hint {
        std::string name;
        double R[9];  // world-to-camera, row-major, COLMAP's axes
        double t[3];
    };
    std::vector<Hint> hints;
    bool add_missing = false, audit_all = false, match = true;
    std::string features, matcher;  // "" = the defaults; should be what the run used
};

struct RepairResultLine {
    std::string name, outcome;  // moved | kept | added | failed | removed
    double rot_deg = 0, shift = 0;
};

InProcessResult run_repair_in_process(const RepairRequest& rq,
                                      const std::function<void(const std::string&)>& log,
                                      const std::atomic<bool>& cancel,
                                      std::vector<RepairResultLine>* report);

}  // namespace gui

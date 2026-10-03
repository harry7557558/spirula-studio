#pragma once

// Repairing one finished model in place (Mapper::repair): re-place cameras
// named as wrong, snap hand-placed ones, register missing images. Targets are
// matched against the model first and the new pairs join matches.bin. The
// result keeps the input's frame, so a trained scene or the editor stays aligned.

#include "sfm/SfmConfig.h"
#include "sfm/core/Pose.h"

#include <string>
#include <vector>

namespace sfm {

struct RepairHint {
    std::string name;
    Pose pose;  // world-to-camera, in the input model's frame
};

struct RepairJob {
    std::string workspace;   // holds features/ and matches.bin
    std::string model_dir;   // the cameras.bin / images.bin / points3D.bin to repair
    std::string output_dir;  // may equal model_dir
    // Image names, with or without extension; a full path matches by its tail.
    std::vector<std::string> replace, add;
    std::vector<RepairHint> hints;
    bool add_missing = false;  // every image in matches.bin the model lacks ...
    std::vector<std::string> exclude;  // ... except these
    bool audit_all = false;
    bool match = true;
    SfmConfig cfg;             // finalized by runRepair
};

struct RepairLine {
    std::string name;
    std::string outcome;  // moved | kept | added | failed | removed
    double rot_deg = 0;
    double shift = 0;     // centre displacement over the model's camera spread
};

// 0 on success. Writes the model and `repair.txt` (`outcome rot shift name`
// per row) to output_dir.
int runRepair(RepairJob job, std::vector<RepairLine>* report = nullptr);

// `qw qx qy qz tx ty tz name` per line (images.txt's pose convention); the
// name is the rest of the line, so it may hold spaces.
bool readRepairHints(const std::string& path, std::vector<RepairHint>& out, std::string& err);
bool writeRepairHints(const std::string& path, const std::vector<RepairHint>& hints);
bool readRepairReport(const std::string& path, std::vector<RepairLine>& out);

}  // namespace sfm

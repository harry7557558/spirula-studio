#pragma once

// Which camera poses the files in lidar/ were made for. A model rebuilt, moved
// or scaled after the scan step keeps the old lidar/, and its depth would then
// train in the wrong frame or the wrong units.

#include <string>

namespace scan_stamp {

// The file the parser reads poses from for `format` ("" detects it), or "" when
// there is none or the format has no such file. `seed` is the parser's own.
std::string pose_file(const std::string& dataset, const std::string& format,
                      const std::string& recon_dir, const std::string& seed);

// Records the poses in `poses` as those of lidar/: a scan writer's last step.
void write(const std::string& dataset, const std::string& poses);

// True when lidar/ has a record and each image it shares with `poses` keeps its pose.
bool matches(const std::string& dataset, const std::string& poses);

}  // namespace scan_stamp

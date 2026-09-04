#pragma once

// The flags a reconstruction was built with, left in the workspace beside the
// model they produced. Both runners keep a finished model rather than spending
// an hour rebuilding one -- which is how a dataset somebody else reconstructed
// gets masks and geometry -- and without this they kept one whose camera
// model, quality or masking no longer had anything to do with the panel.
//
// A model with no stamp came from somewhere else and is reused whatever the
// panel says; only the flags say otherwise.

#include <string>
#include <vector>

namespace gui {

// In the workspace root. Not a dataset artifact: no parser looks for it, and
// probe_workspace does not count it as a model.
inline constexpr const char* kReconStampFile = ".spirula-recon";

struct ReconStamp {
    bool present = false;
    std::string engine;              // "builtin" or "colmap"
    std::vector<std::string> args;   // the reconstruction's own flags, in order
};

ReconStamp read_recon_stamp(const std::string& workspace);
void write_recon_stamp(const std::string& workspace, const ReconStamp& st);

// The flag whose value moved since `prior` was written, for the line that says
// why a model is being rebuilt. Empty when nothing moved, and for a workspace
// that carries no stamp.
std::string recon_stamp_change(const ReconStamp& prior, const ReconStamp& now);

}  // namespace gui

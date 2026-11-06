#pragma once
// The FlexGlue checkpoint, from the authors' release. It bundles Meta's DINOv3
// ConvNeXt-B (extractor.backbone.*), so it needs the DINOv3 licence as well as
// FlexGlue's MIT one: one family, registered by app/ModelLicenses.h.

#include "nn/io/Fetch.h"

#include <string>

namespace flexglue {

inline constexpr const char* kLicenseFamily = "flexglue";
inline constexpr nn::FetchFile kCheckpoint{
    "flexglue.pth", "https://github.com/cvg/FlexGlue/releases/download/v0.1/flexglue.pth",
    "d7371f743b8cdc4adf723c2aa05b181587fe4bc177fced139697975dff790068", 567328059ull, nullptr,
    kLicenseFamily};
inline constexpr const char* kFlexGlueTerms =
    "https://github.com/cvg/FlexGlue/blob/c6cbc7a933bb70ab58608e8835d99b6cc4adca1a/LICENSE";

// "flexglue" is the cached checkpoint, fetched if missing; anything else is a
// path to a .pth. Either way the licence is asked for first: every FlexGlue
// checkpoint carries DINOv3's weights, wherever the file came from.
std::string resolve_checkpoint(const std::string& id_or_path);

}  // namespace flexglue

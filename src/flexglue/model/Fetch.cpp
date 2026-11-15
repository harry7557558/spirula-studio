#include "flexglue/model/Fetch.h"

#include "nn/core/Error.h"

#include <filesystem>

namespace flexglue {

std::string resolve_checkpoint(const std::string& id_or_path) {
    nn::require_licenses(kLicenseFamily);
    if (id_or_path.empty() || id_or_path == "flexglue") return nn::ensure_file(kCheckpoint, "flexglue");
    std::error_code ec;
    NN_CHECK(std::filesystem::is_regular_file(id_or_path, ec),
             "FlexGlue checkpoint '%s' is neither \"flexglue\" nor a file that exists",
             id_or_path.c_str());
    return id_or_path;
}

}  // namespace flexglue

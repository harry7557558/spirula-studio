#pragma once
#include "sam/Masking.h"
#include <cstddef>
#include <iosfwd>
#include <optional>
#include <string>

namespace app {
constexpr std::size_t kMaskManifestRecordBytes = 1024 * 1024;
bool read_mask_manifest_record(std::istream& input, std::string& record);
void write_mask_manifest_record(std::ostream& output, const std::string& record);
std::string mask_worker_recipe(const sam::MaskOptions& options,
                               const std::string& gamut,
                               std::optional<bool> linear,
                               const std::string& exposure);
int mask_worker_main(const std::string& device);
}  // namespace app

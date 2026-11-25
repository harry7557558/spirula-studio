#pragma once

#include "app/MeshPreviewBudget.h"
#include "mesh/MeshExport.h"

namespace spirula {

void save_preview_device_free(const std::string& dir, uint64_t bytes);
uint64_t load_preview_device_free(const std::string& dir);
void write_mesh_preview(const std::string& dir, const meshing::MeshData& mesh, int stage,
                        MeshPreviewBudget budget, bool allow_reduction);
int read_mesh_preview(const std::string& dir, int after_stage, meshing::MeshData& mesh, MeshPreviewBudget budget);

}  // namespace spirula

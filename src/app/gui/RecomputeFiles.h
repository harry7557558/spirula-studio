#pragma once

// The files "Recompute Sparse Point Cloud" swaps in a COLMAP model folder:
// points3D.bin and images.bin, the replaced ones kept as *_original
// (docs/notes/fixed-poses.md). Separate from the panel so a test can run it.

#include <filesystem>
#include <string>

namespace gui {

// `made`'s points3D.bin and images.bin in place of `model`'s. The first time,
// `model`'s become *_original; after an earlier recompute, its originals stay.
// "" or what failed.
std::string install_recomputed(const std::filesystem::path& model,
                               const std::filesystem::path& made);
// Whether `model` holds a recompute that restore_original can undo.
bool holds_recompute(const std::filesystem::path& model);
// *_original back in place. "" or what failed.
std::string restore_original(const std::filesystem::path& model);
// A directory and everything in it.
void remove_dir_tree(const std::filesystem::path& dir);

}  // namespace gui

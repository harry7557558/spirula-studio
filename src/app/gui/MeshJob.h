#pragma once

// What to extract and what to write it as -- the value half of meshing, split
// from the process that runs it (MeshRunner.h) so that a preset, a batch row
// and a test can reason about a job without dragging a child process behind.

#include <string>
#include <vector>

namespace gui {

// What the color checkboxes offer, in order. The values are the `--color`
// tokens, and the order is meshing::MeshColorMode's.
inline const char* kMeshColorModes[] = {"none", "vertex", "texture"};
inline constexpr int kNumMeshColorModes = 3;

// The output formats, in the order the checkboxes are drawn. These are the
// `--format` tokens, and the file extensions.
inline const char* kMeshFormats[] = {"ply", "obj", "gltf", "glb", "stl"};
inline constexpr int kNumMeshFormats = 5;

struct MeshJob {
    // A run directory, a step-*.ckpt directory, or a splat .ply.
    std::string checkpoint;
    // The dataset the model was trained on. "" lets the child read it from
    // the run's config.json; `use_data == false` passes --no-data, which
    // meshes from Gaussian densities alone.
    std::string data_dir;
    bool use_data = true;

    // Output base path, without an extension. "" = beside the checkpoint.
    std::string output;

    // Which colors and which formats, as two sets rather than one choice: the
    // mesh is extracted once and written in every combination the pair allows,
    // so a textured GLB to look at and a plain STL to print cost one run.
    bool colors[kNumMeshColorModes] = {false, true, false};
    bool formats[kNumMeshFormats] = {true, false, false, false, false};

    int max_cameras = 0;               // 0 = every camera
    bool tsdf = false;                 // --method tsdf; needs use_data
    std::string ground_masks;          // --ground-masks; needs use_data
    std::string ground_prompt;         // --ground-prompt; replaces ground_masks
    std::string ground_model, ground_detector;   // the pick's files, set at start
    int texture_size = 0;              // 0 = auto (texture mode only)

    // Canonical UUID for native children; empty uses shared precedence and prevents
    // inherited environment or ordinal re-ranking once populated.
    std::string device_uuid;
    // CUDA children receive the separately frozen ordinal; never a Vulkan UUID.
    int cuda_device = -1;

    // ---- advanced ----
    float iso = 0.0f;                  // 0 = the child's default for the path
    int bisection_iters = 3;
    float merge_factor = 1.0f;
    int quality_iters = 3;
    int floater_min_faces = 10;
    bool cull_unseen = true;
    std::string extra_args;            // appended verbatim

    // The file the preview should open when the run finishes: the richest of
    // the outputs below, which is the first of them.
    std::string preview_path() const;
    bool wants_color(int mode) const;
};

// Every mesh file this job asks for, richest first: a baked texture over
// vertex colors over none, a self-contained GLB over the sidecar formats.
// Empty when `output` is unset, or when the two sets have no pair between them.
std::vector<std::string> mesh_job_outputs(const MeshJob& job);

// True when the two sets leave nothing to write -- a texture asked for with
// PLY alone, say. The Create button has nothing to do in that state.
bool mesh_job_writes_nothing(const MeshJob& job);

// The output base a model implies, which is what the picker and a batch row
// both start from: <run>/mesh for a folder, <name>_mesh for a file.
std::string default_mesh_output(const std::string& checkpoint);

}  // namespace gui

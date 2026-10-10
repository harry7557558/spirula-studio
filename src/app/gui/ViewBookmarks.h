#pragma once

// Saved views: five camera poses per dataset, lens included, in a dotted
// .spirula-views.json beside the data. Poses are kept in the dataset's own
// frame, so the trainer and every run of the dataset in the viewer share
// them, and levelling or placing a model does not move one. The ViewportPanel
// members of this feature live here, except the glide (ViewportPanel.cpp).

#include "core/Similarity.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace spirula { class TrainerSession; }

namespace gui {

class ViewportPanel;

inline constexpr int kNumViews = 5;
inline constexpr const char* kViewsFile = ".spirula-views.json";

struct ViewBookmark {
    bool set = false;
    std::string name;              // "" = the default "View N"
    double pos[3] = {0, 0, 0};
    double rot[4] = {1, 0, 0, 0};  // (w, x, y, z) camera-to-world, OpenGL axes
    double target[3] = {0, 0, 0};  // orbit pivot
    int cam_model = 0;             // kViewerCameraModels index
    float fov_deg = 90.0f;         // across the width
    bool ortho = false;
    float aspect = 0.0f;           // image width / height when saved
    int64_t saved_unix = 0;
};

struct ViewBookmarkSet {
    std::string file;              // UTF-8 path of the JSON
    std::array<ViewBookmark, kNumViews> slot;
    std::string error;             // why the last save failed; "" when it did not
    bool save();
};

// The one shared copy for `folder`, read from disk on first use.
std::shared_ptr<ViewBookmarkSet> view_bookmarks(const std::string& folder);

// The trainer's viewport: the dataset folder, through the session's frames.
void bind_session_views(ViewportPanel& panel, const spirula::TrainerSession& s);
// A viewer pane over `file` (opened as `asked`): the dataset of the run it
// came from when its config.json names one, else the folder it is in.
// `file_to_model` maps the file's coordinates into the panel's model frame.
void bind_file_views(ViewportPanel& panel, const std::string& asked,
                     const std::string& file, const spirula::Sim3& file_to_model);

}  // namespace gui

#pragma once

// GeometryPanel -- try depth and normal estimation on one real frame before
// committing a whole capture to it.
//
// The same predictor and the same app::GeometryWarp the run uses, so what is
// on screen is what will be written -- including the split into pinhole faces
// a wide lens gets, which is the part that comes out plausible and wrong when
// the lens was described wrongly. It also answers "how long will this take",
// which at a second an image is the question nobody can afford to guess.
//
// The model lives on the GPU while the panel is open and is dropped when it
// closes; a run that follows should not share VRAM with it.

#include "app/gui/GeometryRunner.h"
#include "app/gui/GlLoader.h"
#include "app/gui/PreviewFrames.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace gui {

class GeometryPanel {
public:
    // One picture the panel can show, already 8-bit RGB. Public because the
    // three that make one -- photo, normals, depth -- are free functions.
    struct Rgb;

    GeometryPanel();
    ~GeometryPanel();

    // `dataset` is preferred over `input` once it holds a reconstruction: its
    // CAMERAS are the only way a preview can be exact. `image_dir` is where
    // its images are -- not under it, for photos read where they lie.
    void open(const std::string& input, bool is_video, const std::string& dataset,
              const std::string& image_dir, const std::string& lens,
              float focal_factor, const std::string& ffmpeg_exe,
              bool force_ffmpeg);
    bool is_open() const { return _open; }
    void close();

    // Draws the window. Call once per frame from the dataset screen. `job` is
    // read, not written: the panel tries the settings, it does not own them.
    void draw(const GeometryJob& job);
    void destroy_gl();

private:
    struct Job;

    void start_job(const GeometryJob& job);
    void upload_preview();

    bool _open = false;
    PreviewSource _src;
    std::string _dataset, _image_dir;
    // The lens the screen names for this input, for the frames that have no
    // camera of their own yet.
    std::string _lens;
    float _focal_factor = 0.0f;

    int  _frame_idx = 0;
    bool _frame_dirty = true;
    bool _needs_run = false;

    // ---- what the worker publishes ----
    std::mutex _mu;
    std::vector<PreviewFrame> _frames;
    bool _frames_ready = false;
    // Already formatted for the screen: which cameras were used, and what one
    // image costs.
    std::string _status, _error, _source_note, _cost_note;
    // Photo, normals, depth. All three are made on every run, so ticking a
    // map on shows it without another forward pass.
    std::unique_ptr<Rgb> _shot[3];
    bool _shot_dirty = true;

    GLuint _tex[3] = {0, 0, 0};
    int _tex_w[3] = {0, 0, 0}, _tex_h[3] = {0, 0, 0};

    std::thread _worker;
    std::atomic<bool> _busy{false};
    std::atomic<bool> _cancel{false};
    std::unique_ptr<Job> _job;   // the predictor, kept warm between runs
};

}  // namespace gui

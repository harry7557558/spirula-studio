#pragma once

// The dataset screen's right-hand column while inputs are being chosen: the
// selected input, shown as what it is. A video plays through its frames and
// takes a clip, a folder is a grid of its photos, a laser scan or a finished
// dataset is a cloud to fly around, and whatever carries a GPS or an IMU has
// it drawn where a wrong reading is plain to see. docs/notes/source-preview.md.

#include "app/Pano360.h"
#include "app/gui/PhotoBrowser.h"
#include "app/gui/PreviewFrames.h"
#include "app/gui/SensorView.h"
#include "app/gui/VideoBrowser.h"
#include "app/gui/ViewportPanel.h"
#include "data/DatasetParser.h"
#include "sfm/core/Exif.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace gui {

struct PreviewInput {
    enum class Kind { None, Video, Photos, Scan };
    Kind kind = Kind::None;
    std::string path;
    // How frames are read, as the run reads them (GuiApp::preview_source).
    PreviewSource src;
    app::Pano360Layout pano;           // a 360 file's packing; invalid for none
    app::Pano360Options pano_orient;   // ... and how the run turns it
    std::string mask_dir;              // masks that came with the photos
    bool mask_flipped = false;         // ... painting what to remove (PrepJob::flip_found_masks)
    float kept_fps = 0.0f;             // the rate a video is extracted at; 0 every frame
    bool clip_locked = false;          // the run has read the clip
    std::string lut;                   // the LUT its frames go through, for Details
};

class SourcePreview {
public:
    SourcePreview() { _video.set_gate(_gate); }
    ~SourcePreview();

    // The input to show, every frame it is selected; cheap when unchanged.
    void show(const PreviewInput& in);
    void clear();
    bool active() const { return _in.kind != PreviewInput::Kind::None; }
    const std::string& path() const { return _in.path; }

    // Whether frames may be decoded on the inference device now; yield_gpu()
    // also waits for a decode in flight.
    void set_gpu(bool allowed, bool have_ffmpeg);
    void yield_gpu();
    // Not on screen this frame: reads nothing new until drawn again.
    void pause() { _video.set_paused(true); }
    // A video playing wants every frame drawn, not the idle rate.
    bool animating() const { return _video.playing(); }

    struct Result {
        bool clip_changed = false;
        bool want_gpu = false;
        bool lens_changed = false;
    };
    // `clip_*` point at the selected video's clip, null for anything else.
    Result draw(float height, double* clip_start, double* clip_end, int* active_lens = nullptr);
    void destroy_gl();

private:
    enum class Tab { Frames, Photos, Sensors, Cloud, Details };
    struct Listing;

    void start_listing();
    void join_listing();
    void draw_header();
    void draw_details();
    void draw_cloud(float height);
    void draw_photo_info(size_t i);
    std::string reload_key(const PreviewInput& in) const;

    PreviewInput _in;
    std::string _key;
    Tab _tab = Tab::Frames;
    bool _tab_chosen = false;

    std::shared_ptr<DecodeGate> _gate = std::make_shared<DecodeGate>();
    PhotoBrowser _photos;
    VideoBrowser _video;
    SensorView _sensors;
    ViewportPanel _view;

    // What the listing thread finds: the photos, a dataset's model, a scan's
    // cloud and images. Handed over whole once it is done.
    std::thread _worker;
    std::atomic<bool> _cancel{false};
    std::mutex _mu;
    std::shared_ptr<Listing> _listing;   // published
    std::shared_ptr<Listing> _shown;     // what the GUI thread is drawing
    bool _cloud_attached = false;
    bool _sensors_asked = false;
    std::atomic<int64_t> _points_read{0};
    // The opened photo's EXIF, read once rather than every frame it is shown.
    std::string _info_path;
    sfm::ExifData _info_exif;
};

}  // namespace gui

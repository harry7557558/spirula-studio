#pragma once

// A video before it is extracted: any frame of it, a strip of frames along it,
// playback at its own speed, and the part of it a run keeps
// (PrepInput::clip_start / clip_end), set by dragging the strip's two handles.
//
// Frames come from a decoder kept open on the GPU (app::FrameCursor), every
// track in step, or where that is not allowed from one ffmpeg run that reads
// all the tracks. One decode at a time: the in-process decoder shares the
// inference device's queues with everything else that runs on it.

#include "app/Pano360.h"
#include "app/gui/Picture.h"
#include "app/gui/PreviewFrames.h"
#include "app/gui/PreviewTexture.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace gui {

struct VideoSummary {
    bool done = false;
    double seconds = 0.0, fps = 0.0;
    long long frames = 0;
    std::vector<std::pair<int, int>> sizes;   // one per video track
    // One per picture shown side by side: a track, or a lens cut from one.
    std::vector<std::pair<int, int>> pictures;
    std::string codec;
    std::string error;
};

// One switch per lens of a two-lens input, named as the folders they are
// written to, as every other list on the screen names them. `active_lens` is
// PrepInput's; true when it changed.
bool draw_lens_switches(int& active_lens, bool locked);

class VideoBrowser {
public:
    VideoBrowser();
    ~VideoBrowser();

    // `src` reads frames as the run would; a valid `pano` adds the stitched
    // view, turned as `orient` asks. A new `key` drops everything loaded.
    void set(const std::string& key, const PreviewSource& src,
             const app::Pano360Layout& pano, const app::Pano360Options& orient);
    void clear();
    const std::string& key() const { return _key; }

    // Who may run the in-process decoder; abort() cuts a decode in flight short.
    void set_gate(std::shared_ptr<DecodeGate> gate) { _gate = std::move(gate); }
    void abort(bool on);
    // Closes the decoder held open on the device. Only with the gate's lock held.
    void drop_decoder();
    // ffmpeg can stand in for the in-process decoder.
    void set_fallback(bool have_ffmpeg) { _have_ffmpeg = have_ffmpeg; }
    // Off screen: what is in hand stays, nothing new is decoded.
    void set_paused(bool paused);

    VideoSummary facts() const;
    double playhead() const { return _playhead; }
    void set_playhead(double seconds);
    int current_frame() const;
    bool take_selection_changed() { return std::exchange(_selection_changed, false); }

    // Playing at the video's own speed, offered only where frames come off the
    // GPU: ffmpeg takes longer to seek than a frame lasts.
    bool can_play() const;
    bool playing() const { return _playing; }
    void play(bool on, double clip_start, double clip_end);
    // Moves a playing playhead on: every GUI frame, whichever tab is shown.
    void tick();

    struct Result {
        bool clip_changed = false;
        bool want_gpu = false;   // the "decode on the GPU" button was pressed
        bool lens_changed = false;
    };
    // `clip_*` are the input's, `locked` once the run has read them;
    // `kept_fps` is the rate it extracts at, 0 every frame. `active_lens`
    // (PrepInput::active_lens) is offered as a switch per lens of two.
    Result draw(float height, double& clip_start, double& clip_end, bool locked,
                float kept_fps, int* active_lens = nullptr);
    void destroy_gl();

private:
    struct Gpu;   // the decoder kept open, where the build has one

    // A picture of one instant: one per stream shown, or the stitched panorama.
    struct Want {
        long long index = -1;
        int side = 0;            // long edge asked for
        bool stitched = false;
    };
    struct Frame {
        long long index = -1;
        int side = 0;
        bool stitched = false;
        std::vector<Picture> pics;
    };

    enum class Decoded { Ok, Failed, NeedsGpu, Aborted };

    void start();
    void stop();
    void work();
    Decoded decode(const PreviewSource& base, const Want& w, bool keyframe, Frame& out,
                   std::string& error);
    long long index_at(double seconds) const;
    int streams(bool stitched) const;
    void take_frames();
    // `active_lens` dims the lens a two-lens file is not extracting.
    void draw_frame(const ImVec2& box, const int* active_lens = nullptr);
    bool draw_timeline(float width, double& clip_start, double& clip_end, bool locked);

    std::string _key;
    PreviewSource _src;
    app::Pano360Layout _pano;
    app::Pano360Options _orient;
    bool _stitched = true;          // 360 files: the panorama, not the tracks

    std::shared_ptr<DecodeGate> _gate = std::make_shared<DecodeGate>();
    std::unique_ptr<Gpu> _gpu;
    std::atomic<bool> _gpu_failed{false};   // the device cannot decode this file
    std::atomic<bool> _have_ffmpeg{true};
    std::atomic<bool> _abort{false};
    std::atomic<bool> _cancel{false};       // the decode in flight is no longer wanted
    std::atomic<bool> _paused{false};

    mutable std::mutex _mu;
    std::condition_variable _cv;
    std::thread _worker;
    bool _quit = false;
    uint64_t _generation = 0;
    VideoSummary _facts;
    VideoSummary _ui_facts;          // the GUI thread's copy, taken once a frame

    static constexpr int kThumbs = 16;
    std::vector<Picture> _thumb_pic[kThumbs];
    long long _thumb_index[kThumbs] = {};
    bool _thumb_fresh[kThumbs] = {};
    int _thumb_state[kThumbs] = {};      // 0 wanted, 1 loading, 2 ready, 3 failed
    std::vector<PreviewTexture> _thumb_tex[kThumbs];

    Want _want;                      // what the GUI shows next
    Want _done;                      // ... the last of it the worker finished
    bool _play = false;              // the worker reads ahead of the playhead
    long long _play_next = -1;
    std::deque<Frame> _ready;        // decoded, not yet on screen
    std::string _frame_error;
    long long _error_index = -1;
    bool _decoding = false;
    bool _decoding_thumb = false;

    std::vector<PreviewTexture> _frame_tex;
    long long _frame_tex_index = -1;
    int _frame_tex_side = 0;
    bool _frame_tex_stitched = false;

    double _playhead = 0.0;
    bool _playing = false;
    bool _selection_changed = false;
    double _play_end = 0.0;
    bool _jumped = false;            // the playhead moved other than by playing
    double _settle_at = 0.0;         // when the playhead last moved, for the exact frame
    int _drag = 0;                   // 0 none, 1 playhead, 2 clip start, 3 clip end
    std::string _start_text, _end_text;
    int _editing = 0;
};

}  // namespace gui

#pragma once

// FilmReel -- the pictures a dataset run produces, browsable while it runs.
//
// Masking is the step whose output a counter cannot describe: "1400 images
// masked" says nothing about whether the prompt caught the thing walking
// through the shot. The reel shows one picture at a time, follows the newest,
// and lets a slider go back to anything the run has already done.
//
// Every frame is registered as it is produced -- two paths -- while only the
// last few pictures are held; going back to a dropped one reloads it from disk
// on the reel's own thread. That bounds what a four-thousand-image capture
// costs without bounding what can be looked at.

#include "app/gui/GlLoader.h"
#include "app/gui/Picture.h"
#include "app/gui/SfmProgress.h"    // KeyPoint2D

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace gui {

// What the reel holds at once. Pictures are pane-sized (Picture.h), so the
// budget is in bytes rather than in frames -- a 4K pane is 24 MB a picture and
// a small one under 2.
inline constexpr size_t kReelBudget = 64u << 20;
inline constexpr int kReelSlots = 12;
// The most points drawn over one picture: eight thousand circles on a
// 900-pixel frame is a grey wash, and each one is a draw command.
inline constexpr size_t kMaxOverlay = 400;

// What to draw on top of a frame: features in the source image's own pixels.
// Empty for the steps that have nothing to overlay.
struct FramePoints {
    const KeyPoint2D* pts = nullptr;
    size_t count = 0;
};

// One picture, and where to find it once it has been dropped.
struct FilmFrame {
    std::string name;          // the caption; the image's name in the dataset
    std::string image_path;
    std::string mask_path;     // "" when there is none
    std::string points_path;   // feature file to overlay; "" for none
    // Set instead of the three above when the frame is a ROW -- the geometry
    // step's photograph, normal map and depth map of one frame, which are
    // only worth anything read together (Picture.h).
    std::vector<PicturePanel> panels;
};

class FilmReel;

// The reels a run feeds as it works, one per step that produces pictures.
// Any may be null: a caller with no screen wants the pipeline without them.
struct RunFilms {
    FilmReel* frames = nullptr;
    FilmReel* masks = nullptr;
    FilmReel* geometry = nullptr;
};

class FilmReel {
public:
    ~FilmReel();

    // ---- producer (worker thread) ----
    // Is a picture wanted for the next frame? False most of the time, which
    // keeps the copy off a decode running at hundreds of frames a second. The
    // frame is registered either way, so the slider still reaches it.
    bool wants(double min_interval_s = 0.12) const;
    // Appends `f`. `rgb` (w*h*3) and `mask` (w*h, 255 = keep) are the pixels
    // the producer already holds; without them the picture is read from
    // `f.image_path` when it is asked for.
    void add(const FilmFrame& f, const uint8_t* rgb = nullptr, int w = 0,
             int h = 0, const uint8_t* mask = nullptr, FramePoints points = {});
    // Appends `f` AND reads its files now, on the calling thread. The reel
    // follows the newest picture it HOLDS (see draw), so a producer that only
    // registers paths never advances the slider.
    void add_loaded(const FilmFrame& f);
    void clear();

    // ---- consumer (UI thread, GL context current) ----
    bool has_frames() const;
    // The slider and the picture it selects, in `height` pixels. False when
    // there is nothing to draw.
    bool draw(float height);
    void destroy_gl();

private:
    struct Slot {
        int index = -1;
        Picture pic;
        // Overlay points as fractions of the picture, already thinned.
        std::vector<KeyPoint2D> pts;
        uint64_t used = 0;       // last touched; the cache evicts the smallest
    };

    // The cache entry for `index`, or null. Caller holds _mu.
    Slot* find(int index);
    // Register `f` at the end, with `pic` as its picture when it has one.
    void append(const FilmFrame& f, Picture&& pic, std::vector<KeyPoint2D>&& pts);
    void put(int index, Picture&& p, std::vector<KeyPoint2D>&& pts);
    void start_loader();
    void stop_loader();
    void loader_loop();
    // Points as fractions of the picture, thinned to kMaxOverlay.
    static std::vector<KeyPoint2D> thin(const KeyPoint2D* pts, size_t count,
                                        int w, int h);

    mutable std::mutex _mu;
    std::vector<FilmFrame> _frames;
    Slot _cache[kReelSlots];
    size_t _bytes = 0;
    uint64_t _clock = 0;
    // Long edge the pane wants, from the last draw: what the producer and the
    // loader size their pictures for.
    int _target = 1024;

    // ---- what the screen is on ----
    int  _shown = -1;            // index the slider selects
    bool _follow = true;         // ... and whether it tracks the newest
    int  _want = -1;             // index the loader is to fetch; -1 = idle
    // The picture in the texture, kept beside it rather than looked up: the
    // slot it came from may be evicted while it is still what is on screen,
    // and a hole would flash on every step of the slider.
    GLuint _tex = 0;
    int  _tex_index = -1;
    int  _tex_w = 0, _tex_h = 0;
    std::vector<KeyPoint2D> _tex_pts;

    std::thread _loader;
    std::condition_variable _cv;
    bool _stop = false;

    // wants() is called far more often than add(); the clock lives here so a
    // caller cannot forget to rate-limit itself.
    mutable std::chrono::steady_clock::time_point _last{};
    mutable bool _ticked = false;
};

// The pictures a child process writes, onto a reel as they appear. The child
// says nothing about them, so the folder is what is watched.
class OutputWatch {
public:
    // A row is a file of `dir` with its photograph under `images` and its depth
    // map under `depths`, either path empty for none. `fresh_only` holds back
    // what `dir` has now until it is written again: an earlier run's output.
    OutputWatch(FilmReel* reel, std::filesystem::path dir,
                std::filesystem::path images, std::filesystem::path depths,
                bool fresh_only = false);

    // One scan of lag before a file is shown: what appeared this tick may
    // still be half written, and a truncated PNG reads as a failure rather
    // than as a race. `flush` gives that up, for the end of the run.
    void poll(bool flush = false);

private:
    std::vector<std::string> listing() const;
    void publish(const std::vector<std::string>& batch);
    std::vector<PicturePanel> row_for(const std::string& map) const;

    FilmReel* _reel;
    std::filesystem::path _dir, _images, _depths;
    std::set<std::string> _seen;
    std::map<std::string, std::filesystem::file_time_type> _before;
    std::vector<std::string> _ripe;
    std::chrono::steady_clock::time_point _last{};
};

}  // namespace gui

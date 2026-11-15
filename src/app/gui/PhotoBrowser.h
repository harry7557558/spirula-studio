#pragma once

// A folder's worth of pictures as a grid of thumbnails, and any one of them at
// the size of the pane: what an input's photographs look like before a run
// reads them. Pictures load on worker threads, the visible ones first, and only
// a bounded number of thumbnails stay on the GPU.

#include "app/gui/Picture.h"
#include "app/gui/PreviewTexture.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace gui {

struct PhotoItem {
    std::string name;      // the caption, relative to the input
    std::string path;      // what the loader reads; its own key for anything else
    std::string mask;      // "" for none
    int w = 0, h = 0;      // stored size; 0 unknown
};

// One item boxed to `max_side` (0 = as stored), its mask tinted in when
// `with_mask`. Runs on a worker thread; false with `error` set.
using PhotoLoader = std::function<bool(const PhotoItem& item, int max_side, bool with_mask,
                                       Picture& out, std::string& error,
                                       const std::atomic<bool>& cancel)>;

class PhotoBrowser {
public:
    ~PhotoBrowser();

    // A new list replaces the old one; the same `key` again keeps what is loaded.
    void set(const std::string& key, std::vector<PhotoItem> items, PhotoLoader load);
    void clear();
    bool empty() const { return _items.empty(); }
    size_t size() const { return _items.size(); }
    const std::vector<PhotoItem>& items() const { return _items; }

    // The single view of item `i`, as a click on the map asks for it.
    void open(size_t i);
    // The item the single view is on, -1 in the grid.
    int opened() const { return _single; }
    int selected() const {
        if (_single >= 0) return _single;
        if (_selected >= 0) return _selected;
        return _big_tex_index;
    }
    bool take_selection_changed() { return std::exchange(_selection_changed, false); }
    void set_mask_preview_open(bool open) { _mask_preview_open = open; }

    // `info`, when set, draws what is known about the opened item under it.
    void draw(float height, const std::function<void(size_t)>& info = nullptr);
    void destroy_gl();

private:
    struct Thumb {
        Picture pic;              // until uploaded
        PreviewTexture tex;
        int state = 0;            // 0 wanted, 1 loading, 2 ready, 3 failed
        uint64_t used = 0;
        double wanted_at = 0.0;
    };

    void start();
    void stop();
    void work();
    void draw_grid(float height);
    void draw_single(float height, const std::function<void(size_t)>& info);
    void forget_pictures();

    std::string _key;
    std::vector<PhotoItem> _items;
    PhotoLoader _load;
    bool _with_mask = true;
    bool _any_mask = false;
    float _cell = 150.0f;          // unscaled
    int _single = -1;
    bool _scroll_to_single = false;
    int _selected = -1;
    bool _selection_changed = false;
    bool _mask_preview_open = false;

    mutable std::mutex _mu;
    std::condition_variable _cv;
    std::map<size_t, Thumb> _thumbs;
    std::vector<std::thread> _workers;
    std::atomic<bool> _cancel{false};
    bool _quit = false;
    uint64_t _generation = 0;      // bumped when the list or the mask switch changes
    uint64_t _clock = 0;
    int _thumb_side = 192;

    // The single view's picture: what is wanted, what came back.
    int _big_want = -1, _big_side = 0;
    int _big_index = -1, _big_made = 0;
    bool _big_busy = false;
    Picture _big_pic;
    bool _big_fresh = false;
    std::string _big_error;
    PreviewTexture _big_tex;
    int _big_tex_index = -1;
};

}  // namespace gui

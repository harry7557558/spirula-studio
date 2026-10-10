#pragma once

// Screenshots of the trainer's 3D view: an engine render at a fixed size, not
// a screen grab, so a live run and a finished PLY compare pixel for pixel.
// An optional info box (steps, splats, VRAM) goes into the image and the pose
// and run as JSON into the file's comment. Settings are global (gui.conf
// screenshot.*); the GuiApp members live in Screenshot.cpp.

#include "app/gui/ViewBookmarks.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace gui {

struct ScreenshotSettings {
    std::string folder = "Screenshots";      // relative = the dataset folder
    std::string name = "{run}_{view}_{n}";
    bool png = false;
    int quality = 95;                        // JPG
    int width = 1920, height = 1080;
    bool overlay = true;
};

// gui.conf lines: true when `k` was one of ours.
bool read_screenshot_setting(ScreenshotSettings& s, const std::string& k,
                             const std::string& v);
void write_screenshot_settings(FILE* f, const ScreenshotSettings& s);

// Everything one file needs, handed to the writer thread whole.
struct ShotJob {
    ScreenshotSettings cfg;
    std::string dataset_dir, dataset, run, view;   // UTF-8
    ViewBookmark pose;                             // dataset frame; set = false without one
    int step = 0, total = 0;
    int64_t splats = 0, cap = 0;
    bool has_vram = false;
    uint64_t vram_used = 0, vram_total = 0;
    int W = 0, H = 0;
    std::vector<uint8_t> rgb;                      // [H, W, 3], top row first
};

// Where `s` puts the files of a run: the folder pattern expanded, a relative
// one under the dataset folder.
std::string screenshot_folder(const ScreenshotSettings& s, const std::string& dataset_dir,
                              const std::string& dataset, const std::string& run);
// Overlay, encode and write; the path written. Throws on failure.
std::string write_screenshot(ShotJob& job);

}  // namespace gui

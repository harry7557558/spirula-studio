#pragma once

// Animated GIF: a palette of its own for every frame (median cut), ordered
// dithering -- which, unlike error diffusion, does not crawl from frame to
// frame -- and LZW. Frames compress independently, so several threads can
// work on them while one writes them out in order.

#include "core/BufferedFile.h"
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace gui::render {

// One frame as the file stores it -- graphic control, descriptor, palette
// and data -- from RGB rows top-down. `colours` 2..256; `delay_cs` is how
// long it shows, hundredths of a second.
std::vector<uint8_t> gif_encode_frame(const uint8_t* rgb, int width, int height,
                                      int colours, int delay_cs);

class GifWriter {
public:
    ~GifWriter();
    // A file that loops forever.
    bool open(const std::string& path, int width, int height);
    bool write(const std::vector<uint8_t>& frame);
    bool close();

private:
    std::unique_ptr<char[]> _io_buffer;
    std::FILE* _f = nullptr;
    bool _ok = false;
};

// Which frames of a video at `fps` a GIF keeps and how long each shows:
// players stretch anything under 2/100 s, so a faster video drops frames.
// Returns 0 for a frame to leave out.
int gif_delay_cs(int frame, int frames, double fps);

}  // namespace gui::render

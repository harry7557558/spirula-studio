#include "data/ImageProbe.h"

#include "core/ExrImage.h"
#include "external/stb_image.h"

bool probe_image_size(const char* path, int* w, int* h) {
    if (!path || !*path) return false;
    if (exr::is_exr(path)) {
        exr::Info info;
        if (!exr::probe(path, info).empty()) return false;
        *w = info.width;
        *h = info.height;
        return true;
    }
    int ch;
    return stbi_info(path, w, h, &ch) != 0;
}

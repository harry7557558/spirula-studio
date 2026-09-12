// FrameLook.cpp -- see app/FrameLook.h.

#include "app/FrameLook.h"

#include "core/ImageOrient.h"

#include <algorithm>

namespace app {

std::vector<std::string> frame_folders(const FrameLook& look, int tracks) {
    std::vector<std::string> out;
    if (look.pano()) {
        for (const Pano360View& v : look.views) out.push_back(v.dir);
        return out;
    }
    if (tracks < 2) return {std::string()};
    for (int t = 0; t < tracks; t++) out.push_back("cam" + std::to_string(t));
    return out;
}

sfm::ExifTransform photo_turn(const std::string& file) {
    return sfm::exifTransform(sfm::exifOrientation(file));
}

sfm::ExifTransform merge_turns(const std::vector<sfm::ExifTransform>& per_track,
                               const std::vector<int>& tracks, bool& mixed) {
    mixed = false;
    sfm::ExifTransform out;
    bool have = false;
    for (size_t i = 0; i < per_track.size(); i++) {
        if (!tracks.empty() &&
            std::find(tracks.begin(), tracks.end(), (int)i) == tracks.end())
            continue;
        if (!have) {
            out.turns_cw = per_track[i].turns_cw;
            have = true;
        } else if (per_track[i].turns_cw != out.turns_cw) {
            mixed = true;
        }
        out.mirror = out.mirror || per_track[i].mirror;
    }
    return out;
}

void turn_normals(const sfm::ExifTransform& t, std::vector<float>& xyz,
                  int& w, int& h) {
    if (t.identity()) return;
    turn_pixels(t, 3, xyz, w, h);
    // The source axes in the turned frame: e_x -> +e_y and e_y -> -e_x for one
    // clockwise quarter turn, so a vector (a, b) becomes (-b, a).
    for (size_t i = 0; i + 2 < xyz.size(); i += 3) {
        float a = xyz[i], b = xyz[i + 1];
        for (int q = 0; q < (t.turns_cw & 3); q++) {
            const float na = -b, nb = a;
            a = na;
            b = nb;
        }
        xyz[i] = t.mirror ? -a : a;
        xyz[i + 1] = b;
    }
}

nn::Image load_upright(const std::string& file, const std::string& gamut,
                       std::optional<bool> is_linear, sfm::ExifTransform& turn) {
    turn = photo_turn(file);
    nn::Image img = nn::load_image(file, gamut, is_linear);
    if (!img.empty())
        turn_pixels(turn, img.channels, img.data, img.width, img.height);
    return img;
}

}  // namespace app

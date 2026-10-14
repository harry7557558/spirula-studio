#pragma once

// Pano360 -- a 360 camera's own frame layout, and the views a dataset wants
// out of it.
//
// One place owns the geometry: what a GoPro's two tracks hold, the plan that
// turns it into equirectangular or a ring of pinhole faces, and the resampler
// both decode paths apply. ffmpeg is asked only to decode and to cut the
// tracks up; its own `v360=eac` insets every face by 2 px, which puts a 4 px
// step across the seam between the two tracks. docs/datasets.md records where
// the layout numbers were measured.

#include <cstdint>
#include <string>
#include <vector>

namespace app {

// What a camera's two tracks hold. A MAX writes one equi-angular cube map
// across both; a MAX 2 writes a whole panorama per track, each padded at the
// sides by the other lens's copy of the far side.
enum class Pano360Packing { Eac, Sphere };

// Eac: a 3x2 cube map, a `strip`-wide overlap inserted at the lens seam down
// the middle of each track's two side faces. Sphere: a whole panorama per
// track, `margin` px of padding a side, the first upright. docs/datasets.md.
struct Pano360Layout {
    Pano360Packing packing = Pano360Packing::Eac;
    int track_w = 0, track_h = 0;
    int face = 0;                 // EAC face side, or the panorama's quarter
    int strip = 0;                // Eac: inserted at each of the two split points
    int margin = 0;               // Sphere: cut off each side of a track
    bool valid() const { return face > 0; }
    bool sphere() const { return packing == Pano360Packing::Sphere; }
    int canvasW() const { return sphere() ? 4 * face : 3 * face; }
    int canvasH() const { return sphere() ? track_h : 2 * face; }
};

// What the camera itself says its two tracks are, when the file says: a GoPro's
// PRJT and the PMOD beside it, as sfm::VideoProjection carries them. Copied
// rather than included so that the geometry here needs nothing off a container.
struct Pano360Meta {
    std::string projection;
    std::vector<uint32_t> mode;
};

// Two equal video tracks their shape, or the camera's own numbers, say are a 360
// packing: a MAX cube map (4096x1344 at 5.6K, 2272x736 at 3K) or a MAX 2
// panorama pair (5952x1920), which only its own PMOD places.
bool pano360_detect(int tracks, int width, int height, const Pano360Meta& meta,
                    Pano360Layout& out);

// The file says it is a 360 packing -- only a .360 names a projection -- and it
// is not one this build can place. For the line that says the tracks are being
// left as they are, instead of quietly treating them as two ordinary lenses.
bool pano360_unsupported(int tracks, int width, int height,
                         const Pano360Meta& meta);

// One column range of a track, and where it lands in the canvas row. Each
// overlap is cut at its middle, which is where the two lenses' copies of the
// seam meet, so a bilinear tap either side of the cut stays on its own lens.
struct Eac360Slice {
    int src_x = 0, dst_x = 0, width = 0;
};
std::vector<Eac360Slice> eac360_slices(const Pano360Layout& l);

// A track pixel's viewing direction, unit, in the frame Pano360View::rot maps
// into. False where the packing stores no single answer -- inside an EAC
// overlap strip, inside a panorama's side padding -- and outside the track.
bool pano360_direction(const Pano360Layout& l, int row, float x, float y,
                       float dir[3]);

enum class Pano360Mode { Off, Faces, Equirect };

struct Pano360Options {
    Pano360Mode mode = Pano360Mode::Off;
    int   size = 0;         // face side, or equirect width; 0 = from the layout
    // How the camera was held, degrees. Applied to the whole sphere, so a
    // 180 roll turns an inverted mount's views the right way up.
    float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
};

// One image per frame. The camera frame is x right, y down, z forward, `rot`
// maps a view-space ray into it, and `fov` is the HORIZONTAL one -- a view
// narrower than it is wide is half a cube face, not a cropped one.
struct Pano360View {
    std::string dir;        // "cam0"...; empty when the plan makes one image
    int   width = 0, height = 0;
    float fov = 0.0f;       // 0 = equirectangular
    float rot[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};   // row major
};

// Faces over an EAC packing gives five views per lens -- the face on its axis
// and the near half of its four sides -- so every view is fed by ONE lens
// (docs/datasets.md). A panorama is stitched already, so it gets a cube of six.
std::vector<Pano360View> pano360_views(const Pano360Layout& l,
                                       const Pano360Options& o);

// What `size` = 0 resolves to: the face side, or the panorama's width.
int pano360_default_size(const Pano360Layout& l, const Pano360Options& o);

// Where each output pixel reads from the canvas. Built once per view and
// reused for every frame; ~8 bytes a pixel.
struct Pano360Remap {
    int width = 0, height = 0;
    std::vector<float> x, y;
};
void pano360_remap(const Pano360Layout& l, const Pano360View& v,
                   Pano360Remap& out);

// Bilinear gather into `out` (width*height*3 bytes) from a tightly packed RGB
// canvas. `threads` <= 0 uses every core.
void pano360_apply(const Pano360Remap& m, const uint8_t* canvas, int canvas_w,
                   int canvas_h, int threads, uint8_t* out);

// The frames laid out as one canvas, tightly packed RGB. `track1` is read only
// where the packing spreads the sphere over both; a panorama pair holds the
// whole of it twice and the second copy stands on its side, so it is ignored.
void pano360_canvas(const Pano360Layout& l, const uint8_t* track0,
                    const uint8_t* track1, uint8_t* out);

// Whether the canvas needs the second track decoded at all.
inline bool pano360_needs_track1(const Pano360Layout& l) { return !l.sphere(); }

// A 360 camera's one-image formats hold both fisheye circles side by side at 2:1
// (an Insta360 .insp, .lrv, .prx) or stacked at 1:2 (a QooCam .mp4), or one lens
// at 1:1. Any other shape takes the nearer of 2:1 and 1:1, with `exact` false.
int packed_lens_count(int width, int height, bool& exact);

// Lens `lens` of `lenses` along the frame's longer side, as its own tightly
// packed image.
void packed_lens_crop(const uint8_t* px, int width, int height, int channels,
                      int lenses, int lens, std::vector<uint8_t>& out,
                      int& out_width, int& out_height);

// A Kandao camera's recording that left the lenses unstitched: the `kfix` box
// its MP4 ends with says so, where the extension is a plain .mp4.
bool kandao_unstitched(const std::string& path);

// Decodes what the canvas needs and cuts it to size, leaving the result on
// `pano360_canvas_pad`. `pre` ("fps=6") runs on the canvas, so ffmpeg never
// scales a frame that selection will throw away.
std::string pano360_graph(const Pano360Layout& l, const std::string& pre);
inline const char* pano360_canvas_pad() { return "canvas"; }

}  // namespace app

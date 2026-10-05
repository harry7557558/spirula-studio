#pragma once

// Turning what a user picked -- videos, folders of photos -- into the input
// list a dataset run takes, and the settings that list itself decides.
//
// It lives here rather than on the dataset screen because a batch row builds
// the same list from the same paths with nobody watching: two answers to
// "which lens is this capture" would be one answer too many.

#include "app/gui/ColmapRunner.h"
#include "app/gui/DatasetPrep.h"
#include "app/gui/SfmRunner.h"

#include <string>
#include <vector>

namespace gui {

// Every input carries a concrete lens, so a list holding a 360 camera and a
// phone cannot end up applying one of them to the other. An Insta360 capture
// splits into one folder per fisheye lens, which the thin-prism model fits.
std::string default_lens(const PrepInput& s);

// A packed video's lenses, from its frame size: two start as a rig of their own.
void set_packed_lenses(PrepInput& s, int width, int height);

// One picked path -> the input it describes, with the defaults its kind wants.
// A folder resolves to the images/ under it and the masks beside them, which
// is what makes the row show the folder that will actually be indexed.
PrepInput make_source(const std::string& path, bool use_found_masks);

// What only a probe can answer: the 360 packing a file carries and how many
// lens tracks it holds. Asked once per input -- an input already probed costs
// nothing.
void probe_sources(std::vector<PrepInput>& sources, const std::string& ffmpeg_exe);

// The sub-folder under images/ each input's frames go into. A lone input keeps
// images/ itself, which is the layout a one-video dataset has always had.
void assign_source_subdirs(std::vector<PrepInput>& sources);

// The camera folders found INSIDE each input, refreshed against what is on
// disk: a capture that arrives split into cam0/, cam1/ is several cameras.
void refresh_subcameras(std::vector<PrepInput>& sources);

// Give the photo folders that look like one rig's lenses (RigGuess.h) a shared
// letter each. `force` replaces the rigs already chosen; otherwise a list
// where any folder is on a rig is left alone, since that was the user's call.
void guess_source_rigs(std::vector<PrepInput>& sources, bool force);

// The output folder this list implies, and whether that folder is the dataset
// itself (a picked images/ has its reconstruction written beside it) rather
// than a fresh one to be created next to the input.
std::string default_workspace(const std::vector<PrepInput>& sources);
// Is `ws` one default_workspace() gives these inputs -- the folder itself or
// one of its _2, _3 -- whatever it holds by now?
bool workspace_named_by(const std::vector<PrepInput>& sources, const std::string& ws);

bool any_pano360(const std::vector<PrepInput>& sources);
void reset_pano_size(const std::vector<PrepInput>& sources,
                     app::Pano360Options& pano);

// The pixel size of an input's frames: the video's own, or the first photo in
// the folder. False when nothing could be measured -- a path being typed, a
// folder with no readable image, ffmpeg missing.
bool source_pixel_size(const PrepInput& s, const std::string& ffmpeg_exe,
                       int& w, int& h);

// True when every input measures 2:1, which is what an equirectangular
// panorama is and nothing else is. False when even one cannot be measured:
// a guessed lens is the one mistake that reconstructs into nothing.
bool sources_look_equirect(const std::vector<PrepInput>& sources,
                           const std::string& ffmpeg_exe);

// What keeps "same as above" (an empty per-input model) honest: the first row
// always holds a real model, a row repeating the one above is emptied, and
// `camera_model` comes back as the first row's -- the dataset-wide one.
void normalize_source_lenses(std::vector<PrepInput>& sources,
                             std::string& camera_model);

// The same for the kept frame rate: "^" is a per-input 0, and the first VIDEO
// row's rate lives in `video_fps` (0 = every frame) rather than in the row,
// because that is what a preset saves and what every batch item starts from.
void normalize_source_fps(std::vector<PrepInput>& sources, float& video_fps);

// One lens for the whole capture, written to every input rather than only the
// first -- the single "Camera / lens" control speaks for all of them.
void apply_lens_to_sources(std::vector<PrepInput>& sources, SfmJob& sfm,
                           const std::string& model);

// The warp decides the lens exactly: a face is a pinhole camera of the field
// of view it was cut at, and a panorama is the spherical model.
void apply_pano_lens(std::vector<PrepInput>& sources, SfmJob& sfm,
                     ColmapJob& colmap);

// The reconstruction settings the LIST decides: a video is a capture in order,
// a dual-lens file is a rig, a 360 file is one camera warped into views. Run
// when the list changes, before any preset is applied over it.
void apply_capture_defaults(std::vector<PrepInput>& sources, SfmJob& sfm,
                            ColmapJob& colmap);

// What a built-in dataset preset has to ask the capture itself: a 360 camera
// writes either two fisheye circles or a 2:1 panorama, and only its frames
// say which -- so it lives here, with the other answers that need a probe.
void dataset_adapt_preset(const std::string& preset,
                          std::vector<PrepInput>& sources, SfmJob& sfm,
                          ColmapJob& colmap, const std::string& ffmpeg_exe);

// ... and the part that must survive a preset applied afterwards: the lens a
// capture is KNOWN to need wins over the one a preset carries, because a
// dual-fisheye clip fitted with a rectilinear model reconstructs into nothing.
void resolve_source_lenses(std::vector<PrepInput>& sources, SfmJob& sfm,
                           ColmapJob& colmap);

}  // namespace gui

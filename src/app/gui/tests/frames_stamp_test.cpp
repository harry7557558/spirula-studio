// frames_stamp -- what a workspace records about the pictures in its images/
// (app/gui/DatasetPrep.h). Changing how a 360 capture is unwrapped used to
// leave the old frames in place, so the run picked up at feature matching and
// reconstructed views nobody had asked for.

#include "app/gui/DatasetPrep.h"

#include <cstdio>
#include <string>

namespace {

int g_failures = 0;

void expect(bool ok, const std::string& what) {
    std::printf("%s  %s\n", ok ? "ok  " : "BAD ", what.c_str());
    if (!ok) g_failures++;
}

gui::PrepJob two_clips() {
    gui::PrepJob job;
    job.workspace = "/tmp/x";
    job.video_fps = 2.0f;
    gui::PrepInput a, b;
    a.path = "/a.360";
    a.is_video = true;
    b.path = "/b.mp4";
    b.is_video = true;
    job.inputs = {a, b};
    return job;
}

void moves(const char* what, void (*edit)(gui::PrepJob&)) {
    gui::PrepJob before = two_clips();
    gui::PrepJob after = two_clips();
    edit(after);
    expect(!gui::recon_stamp_change(gui::frames_stamp(before),
                                    gui::frames_stamp(after)).empty(),
           std::string("a moved ") + what + " makes the frames stale");
}

}  // namespace

int main() {
    const gui::PrepJob job = two_clips();
    expect(gui::recon_stamp_change(gui::frames_stamp(job),
                                   gui::frames_stamp(job)).empty(),
           "an unchanged job matches its own stamp");

    moves("unwrap mode", [](gui::PrepJob& j) {
        j.pano.mode = app::Pano360Mode::Equirect;
    });
    moves("orientation", [](gui::PrepJob& j) { j.pano.roll = 180.0f; });
    moves("rate", [](gui::PrepJob& j) { j.video_fps = 4.0f; });
    moves("per-video rate", [](gui::PrepJob& j) { j.inputs[1].fps = 6.0f; });
    moves("adaptive switch", [](gui::PrepJob& j) { j.adaptive_fps = true; });
    moves("sharpness window", [](gui::PrepJob& j) { j.sharp_window = 5; });
    moves("frame bit depth", [](gui::PrepJob& j) { j.frame_bits = 16; });
    moves("input list", [](gui::PrepJob& j) { j.inputs.pop_back(); });

    // And what does NOT: masking and the reconstruction stamp their own
    // settings, so a prompt must not cost the extraction as well.
    {
        gui::PrepJob after = two_clips();
        after.mask_enable = true;
        after.mask_prompt = "people";
        expect(gui::recon_stamp_change(gui::frames_stamp(job),
                                       gui::frames_stamp(after)).empty(),
               "a masking prompt leaves the frames alone");
    }

    // "The same as above" chains down the list; the first row takes the job's.
    {
        gui::PrepJob j = two_clips();
        j.inputs[0].fps = 6.0f;
        expect(gui::input_fps(j.inputs, j.video_fps, 0) == 6.0f,
               "a row that states a rate uses it");
        expect(gui::input_fps(j.inputs, j.video_fps, 1) == 6.0f,
               "the row below follows it");
        expect(gui::fps_group(j.inputs, 1) == 0,
               "and is in its group");
        j.inputs[1].fps = 1.0f;
        expect(gui::input_fps(j.inputs, j.video_fps, 1) == 1.0f,
               "a row that states its own wins");
        expect(gui::fps_group(j.inputs, 1) == 1, "and opens a group");
        gui::PrepJob plain = two_clips();
        expect(gui::input_fps(plain.inputs, plain.video_fps, 1) == 2.0f,
               "a list that states nothing is all on the dataset's rate");
    }

    // Every frame is a rate of 0 on the job and kFpsEveryFrame on a row, whose
    // own 0 is already "^".
    {
        gui::PrepJob j = two_clips();
        j.video_fps = 0.0f;
        expect(gui::input_fps(j.inputs, j.video_fps, 1) == 0.0f &&
                   gui::every_frame(j, j.inputs[1]),
               "a job at 0 fps keeps every frame of every row");
        expect(gui::all_videos_every_frame(j.inputs, j.video_fps),
               "and says so for the adaptive warning");
        j.inputs[1].fps = 3.0f;
        expect(!gui::every_frame(j, j.inputs[1]) &&
                   !gui::all_videos_every_frame(j.inputs, j.video_fps),
               "a row with a rate of its own is not every frame");

        gui::PrepJob k = two_clips();
        k.inputs[1].fps = gui::kFpsEveryFrame;
        expect(!gui::every_frame(k, k.inputs[0]) && gui::every_frame(k, k.inputs[1]),
               "a row set to every frame leaves the one above alone");
        expect(gui::fps_group(k.inputs, 1) == 1, "and opens a group");
        expect(!gui::all_videos_every_frame(k.inputs, k.video_fps),
               "one row at a rate is enough to keep adaptive meaningful");
    }
    moves("every-frame row", [](gui::PrepJob& j) {
        j.inputs[1].fps = gui::kFpsEveryFrame;
    });

    std::printf(g_failures ? "\nFAILED: %d\n" : "\nall passed\n", g_failures);
    return g_failures ? 1 : 0;
}

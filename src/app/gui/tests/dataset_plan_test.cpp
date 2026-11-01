// dataset_plan -- which steps of a dataset run are reused and which redone
// (app/gui/DatasetPlan.h), against records written into a scratch workspace
// the way a run writes them.

#include "app/LidarDataset.h"
#include "app/gui/DatasetPlan.h"
#include "app/gui/SfmRunner.h"
#include "sfm/core/Resume.h"
#include "dense/Artifact.h"
#include "data/JsonWrite.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using namespace gui;

namespace {

// The blur rule at `px` (0 off), `by_hand` with the pause.
void skip_blurry(PrepJob& job, float px, bool by_hand = false) {
    job.skip_blurry = px > 0.0f;
    job.blur_px = px > 0.0f ? px : job.blur_px;
    job.blur_review = by_hand;
}

int g_failures = 0;

void expect(bool ok, const std::string& what) {
    std::printf("%s  %s\n", ok ? "ok  " : "BAD ", what.c_str());
    if (!ok) g_failures++;
}

void touch(const fs::path& p) {
    fs::create_directories(p.parent_path());
    std::ofstream(p) << "x";
}

// A dual-fisheye clip, the way the panel hands it over once probed.
PrepInput insv() {
    PrepInput in;
    in.path = "/captures/walk.insv";
    in.is_video = true;
    in.video_tracks = 2;
    in.rig = kRigOwn;
    in.camera_model = "thin-prism-fisheye";
    return in;
}

SfmJob video_job(const fs::path& ws) {
    SfmJob j;
    j.prep.workspace = ws.string();
    j.prep.inputs = {insv()};
    j.prep.video_fps = 2.0f;
    j.prep.mask_enable = true;
    j.prep.mask_prompt = "person";
    j.prep.mask_feature_prompt = "sky";
    j.prep.mask_model_path = "/cache/sam3.pt";
    j.prep.mask_detector_path = "/cache/gdino.onnx";
    j.camera_model = "thin-prism-fisheye";
    j.camera_mode = 1;
    j.data_type = 1;
    return j;
}

// What a finished run over `job` leaves: the folders, and the record.
void build(const fs::path& ws, const SfmJob& job) {
    fs::remove_all(ws);
    for (const char* cam : {"cam0", "cam1"})
        for (const char* f : {"00010.jpg", "00020.jpg", "00030.jpg"}) {
            touch(ws / "images" / cam / f);
            touch(ws / "masks" / cam / (std::string(f) + ".png"));
            touch(ws / "feature_masks" / cam / (std::string(f) + ".png"));
        }
    touch(ws / "sparse" / "0" / "cameras.bin");
    StepRecorder rec(ws.string(), DatasetRecord{});
    rec.begin(Step::Frames, frames_fields(job.prep));
    std::vector<PrepCapture> captures;
    for (const PrepInput& in : job.prep.inputs)
        if (in.is_video)
            captures.push_back(
                {in.subdir, fs::absolute(in.path).lexically_normal().generic_string(), 0.0, true});
    rec.finish(Step::Frames, captures);
    rec.begin(Step::Masks, masks_fields(job.prep));
    rec.finish(Step::Masks);
    rec.begin(Step::Model, model_fields(job));
    rec.finish(Step::Model);
    const bool all[kNumSteps] = {true, true, true, true};
    write_record_settings(ws.string(), "{}",
                          encode_record_inputs({job.prep.inputs, job.prep.mask_clicks}), all);
}

DatasetPlan plan(const SfmJob& job, const PlanRequest& req = {}) {
    const WorkspaceState st = probe_workspace(job.prep.workspace, job.prep.inputs);
    const DatasetRecord rec = read_plan_record(job.prep.workspace, job.prep);
    return plan_dataset(plan_job(job), st, rec, req);
}

// The same dataset's images/ dropped back on the panel: the photo folder the
// panel makes of it, with the rows restored from the record.
SfmJob dropped_images(const fs::path& ws, const SfmJob& built) {
    SfmJob j = built;
    PrepInput in;
    in.path = (ws / "images").string();
    in.mask_dir = (ws / "masks").string();
    in.camera_model = "opencv";
    for (const char* cam : {"cam0", "cam1"}) {
        SubCamera sc;
        sc.rel = cam;
        sc.camera_model = "opencv";
        in.subcameras.push_back(sc);
    }
    j.prep.inputs = {in};
    j.prep.video_fps = 5.0f;   // a rate no photo is extracted at
    j.camera_model = "opencv";
    restore_record_inputs(read_dataset_record(ws.string()), j.prep, j.camera_model);
    return j;
}

}  // namespace

int main() {
    const fs::path ws = fs::temp_directory_path() / "spirula_dataset_plan_test";
    const SfmJob made = video_job(ws);

    // ---- which LUT a row's frames go through ---------------------------------
    {
        std::vector<PrepInput> rows(5);
        for (PrepInput& r : rows) r.is_video = true;
        rows[0].lut_detected = "builtin:dji-dlog-m";
        expect(input_lut(rows, 0) == "builtin:dji-dlog-m" && input_lut(rows, 1).empty(),
               "a detected LUT stays with its own file");
        rows[0].lut = "a.cube";
        rows[2].lut_detected = "builtin:gopro-gplog";
        rows[3].lut = kLutNone;
        expect(input_lut(rows, 0) == "a.cube" && input_lut(rows, 1) == "a.cube" &&
                   input_lut(rows, 2) == "builtin:gopro-gplog" && input_lut(rows, 3).empty() &&
                   input_lut(rows, 4).empty(),
               "a choice carries down until a file says otherwise or a row says none");
        SfmJob a = made, b = made;
        b.prep.inputs[0].lut_detected = "builtin:dji-dlog-m";
        const StepFields fb = frames_fields(b.prep);
        bool verbatim = false;
        for (const StepField& f : fb)
            verbatim = verbatim || (f.key == "lut" && f.value == "builtin:dji-dlog-m");
        expect(!(frames_fields(a.prep) == fb) && verbatim,
               "a detected LUT is part of the frames, named as it is");
    }

    // ---- the fields --------------------------------------------------------
    {
        SfmJob a = made, b = made;
        b.prep.video_fps = 3.0f;
        expect(!(frames_fields(a.prep) == frames_fields(b.prep)),
               "a video's rate is part of its frames");
        b = made;
        b.prep.inputs[0].clip_start = 12.5;
        expect(!(frames_fields(a.prep) == frames_fields(b.prep)),
               "a video's clip is part of its frames");
        SfmJob c = b;
        c.prep.inputs[0].clip_end = 40.0;
        expect(!(frames_fields(b.prep) == frames_fields(c.prep)),
               "... and so is where it ends");
        b = made;
        b.prep.mask_prompt = "person; car";
        expect(frames_fields(a.prep) == frames_fields(b.prep),
               "a mask prompt is not part of the frames");
        PrepJob photos;
        photos.workspace = ws.string();
        PrepInput p;
        p.path = "/photos";
        photos.inputs = {p};
        PrepJob faster = photos;
        faster.video_fps = 6.0f;
        faster.photo_import = PhotoImport::Copy;
        expect(frames_fields(photos) == frames_fields(faster),
               "a folder of photos does not depend on the video rate or import mode");
    }

    // ---- the blur rule and a video's frames ---------------------------------
    {
        SfmJob on = made, every = made, every_on = made;
        skip_blurry(on.prep, 8.0f);
        expect(!(frames_fields(made.prep) == frames_fields(on.prep)),
               "video: a blur threshold is part of the frames");
        SfmJob still = on;
        still.prep.similar_bits = 6;
        expect(!(frames_fields(on.prep) == frames_fields(still.prep)),
               "video: so is a similarity threshold");
        SfmJob alone = on;
        alone.prep.blur_neighbour = false;
        expect(!(frames_fields(on.prep) == frames_fields(alone.prep)),
               "video: and whether a skipped frame keeps a neighbour");
        every.prep.video_fps = every_on.prep.video_fps = 0.0f;
        skip_blurry(every_on.prep, 8.0f);
        every_on.prep.similar_bits = 6;
        expect(frames_fields(every.prep) == frames_fields(every_on.prep),
               "video: every frame is every frame, so the rule is not part of those");
    }

    // ---- the blur rule sorts photographs as it does video frames ---------------
    {
        const fs::path pw = ws.string() + "_photos";
        SfmJob off;
        off.prep.workspace = pw.string();
        PrepInput p;
        p.path = "/photos";
        off.prep.inputs = {p};
        off.camera_model = "opencv";
        SfmJob on = off, hand = off, in_place = off;
        skip_blurry(on.prep, 8.0f);
        skip_blurry(hand.prep, 8.0f, true);
        hand.prep.blur_px = on.prep.blur_px;
        skip_blurry(in_place.prep, 8.0f);
        in_place.prep.photo_import = PhotoImport::InPlace;
        expect(!(frames_fields(off.prep) == frames_fields(on.prep)),
               "photos: a blur threshold is part of the frames");
        expect(!(frames_fields(on.prep) == frames_fields(hand.prep)),
               "photos: the same threshold set by hand is another run, one that pauses");
        SfmJob there = off;
        there.prep.photo_import = PhotoImport::InPlace;
        expect(frames_fields(there.prep) == frames_fields(in_place.prep),
               "photos read where they are are never sorted, so the rule is not theirs");

        fs::remove_all(pw);
        touch(pw / "images" / "a.jpg");
        touch(pw / "sparse" / "0" / "cameras.bin");
        StepRecorder rec(pw.string(), DatasetRecord{});
        for (Step s : {Step::Frames, Step::Masks, Step::Model}) {
            rec.begin(s, s == Step::Frames  ? frames_fields(on.prep)
                         : s == Step::Masks ? masks_fields(on.prep)
                                            : model_fields(on));
            rec.finish(s);
        }
        const DatasetPlan same = plan(on);
        expect(same[Step::Frames].act == Act::Reuse && same[Step::Model].act == Act::Reuse,
               "photos: the same threshold keeps the frames and the reconstruction");
        SfmJob tighter = on;
        skip_blurry(tighter.prep, 5.0f);
        const DatasetPlan t = plan(tighter);
        expect(t[Step::Frames].act == Act::Reuse && t[Step::Frames].sort &&
                   t[Step::Model].act == Act::Reuse && !t.ask(),
               "photos: a tighter threshold only leaves more of the same frames out, "
               "so it sorts them where they lie and keeps the reconstruction");
        std::ofstream(pw / kSkippedListFile, std::ios::binary)
            << "# " << blur_rule_text(tighter.prep) << "\n";
        expect(!plan(tighter)[Step::Frames].sort && plan(on)[Step::Frames].sort &&
                   plan(on)[Step::Model].act == Act::Reuse,
               "photos: sorted by it already there is nothing to do, and going back to "
               "the rule they were made with sorts again without making anything");
        fs::remove(pw / kSkippedListFile);
        SfmJob looser = on;
        skip_blurry(looser.prep, 10.0f);
        const DatasetPlan q = plan(looser);
        expect(q[Step::Frames].act == Act::Redo && q[Step::Model].act == Act::Redo && q.ask(),
               "photos: a looser threshold wants frames the dataset never had: made again");
        const DatasetPlan none = plan(off);
        expect(none[Step::Frames].act == Act::Redo && none[Step::Model].act == Act::Redo,
               "photos: so does turning the rule off, which brings the ones left out back");
    }

    // ---- a fresh run over a built dataset -------------------------------------
    build(ws, made);
    {
        SfmJob blur = made;
        skip_blurry(blur.prep, 8.0f);
        const DatasetPlan b = plan(blur);
        expect(b[Step::Frames].act == Act::Reuse && b[Step::Frames].sort &&
                   b[Step::Masks].act == Act::Reuse && b[Step::Model].act == Act::Reuse &&
                   !b.ask(),
               "turning the blur rule on over a dataset made without it sorts the frames "
               "where they lie: nothing is made again and nothing is asked");
        std::ofstream(ws / kSkippedListFile, std::ios::binary)
            << "# " << blur_rule_text(blur.prep) << "\n00020.jpg\n";
        expect(!plan(blur)[Step::Frames].sort, "sorted by that rule already: nothing to do");
        SfmJob tighter = blur;
        skip_blurry(tighter.prep, 5.0f);
        expect(plan(tighter)[Step::Frames].sort && plan(made)[Step::Frames].sort &&
                   plan(made)[Step::Model].act == Act::Reuse,
               "another threshold sorts again, and so does turning the rule off, "
               "which brings the frames back");
        SfmJob faster = blur;
        faster.prep.video_fps = 3.0f;
        const DatasetPlan f = plan(faster);
        expect(f[Step::Frames].act == Act::Redo && !f[Step::Frames].sort && f.ask(),
               "a change that is not the rule's still makes the frames again");
        fs::remove(ws / kSkippedListFile);
    }
    {
        const DatasetPlan p = plan(made);
        expect(p[Step::Frames].act == Act::Reuse && p[Step::Masks].act == Act::Reuse &&
                   p[Step::Model].act == Act::Reuse && !p.ask(),
               "the same settings reuse every step");
        expect(p[Step::Geometry].act == Act::None, "geometry off is not a step");
    }
    {
        const fs::path w = ws.string() + "_ambiguous";
        SfmJob j = video_job(w);
        build(w, j);
        StepRecorder rec(w.string(), read_dataset_record(w.string()));
        rec.begin(Step::Frames, frames_fields(j.prep));
        rec.finish(Step::Frames);
        const DatasetPlan p = plan(j);
        expect(p[Step::Frames].act == Act::Redo && !p[Step::Frames].can_keep && p.ask(),
               "completed video frames without recorded timing must be extracted again");
        PlanRequest keep;
        keep.keep_built = true;
        const DatasetPlan preserved = plan(j, keep);
        expect(!preserved[Step::Frames].can_keep && preserved.ask() &&
                   recorded_captures(w.string(), j.prep).empty(),
               "preserving ambiguous video frames is unavailable and still needs authorization");
        PlanRequest redo;
        redo.redo_frames = true;
        redo.keep_built = true;
        const DatasetPlan authorized = plan(j, redo);
        expect(authorized[Step::Frames].act == Act::Redo && !authorized.ask(),
               "explicit frame rebuilding takes precedence over a retained preservation choice");
        fs::remove_all(w);
    }
    {
        SfmJob scans = made;
        scans.lidar.clouds = {"/scan.e57"};
        scans.lidar.scanner_only = true;
        scans.geometry.enable = true;
        const DatasetPlan p = plan(scans);
        expect(p[Step::Model].act == Act::None && p[Step::Geometry].act == Act::None,
               "a scan's photographs at its poses: no reconstruction, no geometry step");
    }
    {
        SfmJob j = made;
        j.geometry.enable = true;
        const DatasetPlan p = plan(j);
        expect(p[Step::Geometry].act == Act::Run && p[Step::Model].act == Act::Reuse,
               "depth and normals are added without touching the model");
    }

    // ---- the dataset's own images/ dropped back in -----------------------------
    {
        SfmJob j = dropped_images(ws, made);
        j.geometry.enable = true;
        expect(frames_in_dataset(j.prep) && masks_in_dataset(j.prep),
               "the dropped images and masks are the dataset's own");
        expect(j.prep.inputs[0].subcameras[1].camera_model == "thin-prism-fisheye" ||
                   (j.prep.inputs[0].subcameras[1].camera_model.empty() &&
                    j.camera_model == "thin-prism-fisheye"),
               "each camera folder gets the lens the video's row gave it");
        const DatasetPlan p = plan(j);
        expect(p[Step::Frames].act == Act::Reuse && p[Step::Frames].why == Why::InDataset,
               "frames: in the dataset, whatever the video rate says");
        expect(p[Step::Masks].act == Act::Reuse, "masks: reused");
        std::string why;
        for (const FieldChange& c : p[Step::Model].changes)
            why += " " + c.key + "[" + c.scope + "] " + c.was + " -> " + c.now;
        expect(p[Step::Model].act == Act::Reuse && p[Step::Model].changes.empty(),
               "model: the same reconstruction, no change" + why);
        expect(p[Step::Geometry].act == Act::Run && !p.ask(),
               "only depth and normals run, and nothing is asked");
    }

    // ---- a dataset made from two videos, dropped back the same way ---------
    {
        const fs::path two = ws.string() + "_two";
        SfmJob j = video_job(two);
        PrepInput a = insv(), b;
        a.path = "/captures/a.insv";
        a.subdir = "a";
        b.path = "/captures/b.mp4";
        b.subdir = "b";
        b.is_video = true;
        b.video_tracks = 1;
        b.camera_model = "opencv";
        j.prep.inputs = {a, b};
        fs::remove_all(two);
        for (const char* cam : {"a/cam0", "a/cam1", "b"}) {
            touch(two / "images" / cam / "00010.jpg");
            touch(two / "masks" / cam / "00010.png");
            touch(two / "feature_masks" / cam / "00010.png");
        }
        touch(two / "sparse" / "0" / "cameras.bin");
        StepRecorder rec(two.string(), DatasetRecord{});
        for (Step s : {Step::Frames, Step::Masks, Step::Model}) {
            rec.begin(s, s == Step::Frames  ? frames_fields(j.prep)
                         : s == Step::Masks ? masks_fields(j.prep)
                                            : model_fields(j));
            if (s == Step::Frames)
                rec.finish(s, {{a.subdir, fs::absolute(a.path).lexically_normal().generic_string(),
                                 0.0, true},
                                {b.subdir, fs::absolute(b.path).lexically_normal().generic_string(),
                                 0.0, false}});
            else rec.finish(s);
        }
        SfmJob d = j;
        PrepInput in;
        in.path = (two / "images").string();
        in.mask_dir = (two / "masks").string();
        for (const char* cam : {"a/cam0", "a/cam1", "b"}) {
            SubCamera sc;
            sc.rel = cam;
            in.subcameras.push_back(sc);
        }
        d.prep.inputs = {in};
        restore_record_inputs(read_dataset_record(two.string()), d.prep, d.camera_model);
        const DatasetPlan p = plan(d);
        std::string why;
        for (const FieldChange& c : p[Step::Model].changes)
            why += " " + c.key + "[" + c.scope + "] " + c.was + " -> " + c.now;
        expect(p[Step::Model].act == Act::Reuse && p[Step::Model].changes.empty(),
               "two videos' frames dropped back: the same reconstruction" + why);
        fs::remove_all(two);
    }

    // ---- settings that do move ---------------------------------------------
    {
        SfmJob j = made;
        j.quality = 3;
        DatasetPlan p = plan(j);
        expect(p[Step::Model].act == Act::Redo && p[Step::Model].why == Why::Settings &&
                   p.ask(),
               "a quality change rebuilds, and asks first");
        PlanRequest keep;
        keep.keep_built = true;
        p = plan(j, keep);
        expect(p[Step::Model].act == Act::Keep && !p.ask(), "... or keeps it, when told to");
        PlanRequest redo;
        redo.redo_model = true;
        p = plan(made, redo);
        expect(p[Step::Model].act == Act::Redo && !p.ask(),
               "a rebuild that was asked for is not asked about");
    }
    {
        SfmJob j = made;
        j.prep.video_fps = 3.0f;
        j.geometry.enable = true;
        const DatasetPlan p = plan(j);
        expect(p[Step::Frames].act == Act::Redo && p[Step::Masks].act == Act::Redo &&
                   p[Step::Model].act == Act::Redo && p.ask(),
               "a new rate re-extracts, and everything under it follows");
    }
    {
        SfmJob j = made;
        j.prep.mask_dilate_ratio = 0.08f;
        const DatasetPlan p = plan(j);
        expect(p[Step::Masks].act == Act::Redo && p[Step::Masks].why == Why::Settings,
               "a mask setting redoes the masks");
        expect(p[Step::Model].act == Act::Reuse && p[Step::Model].masks_changed &&
                   !p.ask(),
               "... and keeps the reconstruction, noting it");
    }
    {
        SfmJob j = made;
        j.prep.inputs[0].camera_model = "opencv-fisheye";
        j.camera_model = "opencv-fisheye";
        const DatasetPlan p = plan(j);
        expect(p[Step::Model].act == Act::Redo && !p[Step::Model].changes.empty() &&
                   p[Step::Model].changes[0].key == "lens",
               "a lens change rebuilds, and says which folder");
    }

    // ---- what the record restores ------------------------------------------
    {
        const bool all[kNumSteps] = {true, true, true, true};
        const bool geometry_only[kNumSteps] = {false, false, false, true};
        const std::string rows = encode_record_inputs({made.prep.inputs, {}});
        write_record_settings(ws.string(), R"({"sfm_quality": 2, "geometry_max_size": 1064})",
                              rows, all);
        write_record_settings(ws.string(), R"({"sfm_quality": 3, "geometry_max_size": 512})",
                              rows, geometry_only);
        const DatasetRecord rec = read_dataset_record(ws.string());
        expect(rec.settings.get_double("sfm_quality", -1) == 2,
               "a run that kept the reconstruction does not speak for its settings");
        expect(rec.settings.get_double("geometry_max_size", -1) == 512,
               "... while the step it did make does");
    }

    // ---- interrupted steps -----------------------------------------------------
    {
        StepRecorder rec(ws.string(), read_dataset_record(ws.string()));
        rec.begin(Step::Frames, frames_fields(made.prep));
        const DatasetPlan p = plan(made);
        expect(p[Step::Frames].act == Act::Redo && p[Step::Frames].why == Why::Unrecorded,
               "interrupted video frames without timing are extracted again");
        expect(p[Step::Model].act == Act::Redo,
               "... and the model built from the frames before them is not trusted");
    }

    // ---- the reconstruction's own stages ---------------------------------------
    build(ws, made);
    {
        PlanRequest redo;
        redo.redo_model = true;
        DatasetPlan p = plan(made, redo);
        expect(p[ModelPart::Features].act == Act::Run && p[ModelPart::Matching].act == Act::Run &&
                   p[ModelPart::Mapping].act == Act::Redo &&
                   p[ModelPart::Align].act == Act::None,
               "nothing of `spirula sfm`'s to reuse: extract, match, map again");
        touch(ws / "features" / "cam0" / "00010.jpg.bin");
        touch(ws / "matches.bin");
        touch(ws / sfm::resume::kDir / sfm::resume::kExtractSig);
        touch(ws / sfm::resume::kDir / sfm::resume::kMatchSig);
        p = plan(made, redo);
        expect(p[ModelPart::Features].act == Act::Reuse &&
                   p[ModelPart::Matching].act == Act::Reuse &&
                   p[ModelPart::Mapping].act == Act::Redo &&
                   p[ModelPart::Mapping].why == Why::Requested,
               "reconstructing again on the same settings only maps again");
        expect(plan(made)[ModelPart::Mapping].act == Act::None,
               "a reused reconstruction runs none of its stages");
        std::ofstream(ws / sfm::resume::kDir / sfm::resume::kExtractSig)
            << "features=sift\n" << sfm::resume::kSignedImages << "/elsewhere/images\n";
        p = plan(made, redo);
        expect(p[ModelPart::Features].act == Act::Redo &&
                   p[ModelPart::Features].why == Why::Moved,
               "features extracted from images somewhere else are extracted again");
        std::ofstream(ws / sfm::resume::kDir / sfm::resume::kExtractSig)
            << "features=sift\n" << sfm::resume::kSignedImages << (ws / "images").string() << "\n";
        expect(plan(made, redo)[ModelPart::Features].act == Act::Reuse,
               "... and ones from these images are not");

        SfmJob j = made;
        j.features = (int)std::size(kSfmFeatures) - 1;
        p = plan(j);
        expect(p[ModelPart::Features].act == Act::Redo &&
                   p[ModelPart::Features].why == Why::Settings &&
                   p[ModelPart::Matching].why == Why::Features &&
                   p[ModelPart::Mapping].why == Why::Matches,
               "another frontend extracts again, and the stages after it follow");
        j = made;
        j.loop_closure = false;
        p = plan(j);
        expect(p[ModelPart::Features].act == Act::Reuse &&
                   p[ModelPart::Matching].act == Act::Redo &&
                   p[ModelPart::Matching].changes.size() == 1 &&
                   p[ModelPart::Matching].changes[0].key == "loop_closure" &&
                   p[ModelPart::Features].changes.empty(),
               "a pairing setting keeps the features and matches again, naming itself");
        j = made;
        j.prep.inputs[0].rig = kRigNone;
        p = plan(j);
        expect(p[ModelPart::Features].act == Act::Reuse &&
                   p[ModelPart::Matching].act == Act::Redo &&
                   p[ModelPart::Matching].why == Why::Settings,
               "a rig changes the pairs matched, so it matches again");
        j = made;
        j.mapper = 1;
        p = plan(j);
        expect(p[ModelPart::Matching].act == Act::Reuse &&
                   p[ModelPart::Mapping].act == Act::Redo &&
                   p[ModelPart::Mapping].why == Why::Settings,
               "a mapper setting only maps again");
        j = made;
        j.prep.inputs[0].use_gps = false;
        p = plan(j);
        expect(p[ModelPart::Features].act == Act::Reuse &&
                   p[ModelPart::Matching].act == Act::Redo &&
                   p[ModelPart::Matching].changes.size() == 1 &&
                   p[ModelPart::Matching].changes[0].key == "gps",
               "a video's GPS switched off matches again, naming itself");
        j = made;
        j.prep.inputs[0].sequential = true;
        p = plan(j);
        expect(p[ModelPart::Matching].act == Act::Redo &&
                   p[ModelPart::Matching].changes.size() == 1 &&
                   p[ModelPart::Matching].changes[0].key == "sequence" &&
                   p[ModelPart::Matching].changes[0].scope == "cam0,cam1",
               "a video's order, asked for, is its two lenses as one sequence");
        bool switches = false;
        for (const StepField& f : model_fields(made))
            switches = switches || f.key == "sequence" || f.key == "imu" || f.key == "gps";
        expect(!switches, "a video's order is off by default, and sensors left on add no field");
        j = made;
        j.prep.mask_dilate_ratio = 0.08f;
        p = plan(j, redo);
        expect(p[ModelPart::Features].act == Act::Redo &&
                   p[ModelPart::Features].why == Why::Masks &&
                   p[ModelPart::Matching].why == Why::Features,
               "new masks re-extract the images they cover");
        j = made;
        j.prep.video_fps = 3.0f;
        p = plan(j);
        expect(p[ModelPart::Features].why == Why::Frames &&
                   p[ModelPart::Features].lock == Lock::Frames,
               "new frames re-extract every image, and the old points cannot be kept");
    }

    // ---- stages the user keeps or runs against the plan ------------------------
    {
        PlanRequest keep_matches;
        keep_matches.parts[(int)ModelPart::Matching] = PartChoice::Keep;
        SfmJob j = made;
        j.prep.inputs[0].rig = kRigNone;
        DatasetPlan p = plan(j, keep_matches);
        expect(p[ModelPart::Matching].act == Act::Keep &&
                   p[ModelPart::Mapping].act == Act::Redo &&
                   p[ModelPart::Features].act == Act::Reuse,
               "a rig change can keep the matches and only map again");
        expect(p[ModelPart::Mapping].lock == Lock::Always,
               "... and mapping itself is never skipped on its own");

        j = made;
        j.features = 2;
        PlanRequest keep_features;
        keep_features.parts[(int)ModelPart::Features] = PartChoice::Keep;
        p = plan(j, keep_features);
        expect(p[ModelPart::Features].act == Act::Redo &&
                   p[ModelPart::Features].lock == Lock::Frontend,
               "feature points of another type are not kept");
        j = made;
        j.max_features = 1000;
        p = plan(j, keep_features);
        expect(p[ModelPart::Features].act == Act::Keep &&
                   p[ModelPart::Matching].act == Act::Reuse,
               "kept feature points keep the matches over them");

        PlanRequest both = keep_matches;
        p = plan(j, both);
        expect(p[ModelPart::Features].act == Act::Redo &&
                   p[ModelPart::Matching].act == Act::Redo &&
                   p[ModelPart::Matching].lock == Lock::Before,
               "matches over feature points made again are not kept");

        j = made;
        j.prep.inputs[0].camera_model = "opencv-fisheye";
        j.camera_model = "opencv-fisheye";
        p = plan(j, keep_matches);
        expect(p[ModelPart::Matching].act == Act::Redo &&
                   p[ModelPart::Matching].lock == Lock::Lens,
               "matches verified with another lens are not kept");

        PlanRequest force;
        force.redo_model = true;
        force.parts[(int)ModelPart::Matching] = PartChoice::Run;
        p = plan(made, force);
        expect(p[ModelPart::Features].act == Act::Reuse &&
                   p[ModelPart::Matching].act == Act::Redo &&
                   p[ModelPart::Matching].why == Why::Requested &&
                   p[ModelPart::Mapping].why == Why::Matches,
               "a current stage can be made again, and the ones after it follow");
        force.parts[(int)ModelPart::Matching] = PartChoice::Keep;
        force.parts[(int)ModelPart::Features] = PartChoice::Run;
        p = plan(made, force);
        expect(p[ModelPart::Features].act == Act::Redo &&
                   p[ModelPart::Matching].act == Act::Redo &&
                   p[ModelPart::Matching].lock == Lock::Before,
               "... and a stage after one that runs cannot be kept");
    }
    {
        SfmJob j = made;
        j.lidar.clouds = {"/scans/a.e57"};
        DatasetPlan p = plan(j);
        expect(p[Step::Model].act == Act::Reuse && p[ModelPart::Align].act == Act::Run &&
                   p[ModelPart::Features].act == Act::None,
               "scans added to a finished reconstruction: only the alignment runs");
        std::ofstream(ws / "sparse" / "0" / app::lidar::kAlignedMarker)
            << R"({"clouds": ["/scans/a.e57"], "mode": "auto"})";
        expect(plan(j)[ModelPart::Align].act == Act::Reuse,
               "an alignment to the same scans is reused");
        PlanRequest force;
        force.parts[(int)ModelPart::Align] = PartChoice::Run;
        p = plan(j, force);
        expect(p[ModelPart::Align].act == Act::Redo && p[ModelPart::Align].why == Why::Requested,
               "... unless it is asked for again");
        j.lidar.clouds.push_back("/scans/b.e57");
        expect(plan(j)[ModelPart::Align].act == Act::Redo, "... and redone for another scan");
        PlanRequest keep;
        keep.parts[(int)ModelPart::Align] = PartChoice::Keep;
        expect(plan(j, keep)[ModelPart::Align].act == Act::Keep,
               "... or kept, when the user says so");
        keep.redo_model = true;
        p = plan(j, keep);
        expect(p[ModelPart::Align].act == Act::Redo && p[ModelPart::Align].lock == Lock::Before,
               "... but never over a new reconstruction");
        j.lidar.clouds.pop_back();
        j.lidar.in_frame = true;
        expect(plan(j)[ModelPart::Align].act == Act::Redo,
               "... or for a model said to be in the scans' frame already");
        j.lidar.in_frame = false;
        PlanRequest redo;
        redo.redo_model = true;
        p = plan(j, redo);
        expect(p[ModelPart::Align].act == Act::Redo && p[ModelPart::Align].why == Why::Model,
               "a new reconstruction is aligned again");
    }
    build(ws, made);
    {
        SfmJob j = made;
        expect(plan(j)[Step::Dense].act == Act::None, "old settings keep dense processing disabled");
        j.dense.enable = true;
        expect(plan(j)[Step::Dense].act == Act::Run, "enabling dense starts its own step");
        touch(ws / "dense" / "roma.ply");
        expect(spirula::dense::is_dense_seed(ws.string(), (ws / "dense" / "roma.ply").string()) &&
               spirula::dense::is_dense_seed(ws.string(), "dense/roma.ply") &&
               !spirula::dense::is_dense_seed(ws.string(), "other.ply"), "dense seed mask mode recognizes absolute and dataset-relative paths");
        const auto source = ws / "input-to-watch.txt";
        touch(source);
        const std::vector<std::string> watched{source.string()};
        auto write_manifest = [&](int revision = spirula::dense::reconstruction_revision) {
            JsonWriter manifest; manifest.object().field("complete", true).field("cloud_sha256", spirula::sha256_file((ws / "dense" / "roma.ply").string()));
            manifest.field("reconstruction_revision",revision);
            manifest.field("cloud_bytes", 1).field("input_stamp", spirula::dense::input_stamp(watched));
            manifest.field("input_content_stamp",spirula::dense::input_content_stamp(watched));
            manifest.key("input_paths").array().value(source.generic_string()).end();
            manifest.key("statistics").object().field("exported", 1).end().end();
            std::ofstream(ws / "dense" / "manifest.json") << manifest.str();
        };
        write_manifest();
        StepRecorder rec(ws.string(), read_dataset_record(ws.string()));
        rec.begin(Step::Dense, dense_fields(j.dense), {"roma.ply"}); rec.finish(Step::Dense);
        expect(plan(j)[Step::Dense].act == Act::Reuse, "completed dense settings are reused");
        {
            auto legacy = j;
            legacy.dense.config.pairs.reference_coverage = 0;
            StepRecorder old(ws.string(), read_dataset_record(ws.string()));
            old.begin(Step::Dense, dense_fields(legacy.dense), {"roma.ply"}); old.finish(Step::Dense);
            expect(plan(legacy)[Step::Dense].act == Act::Reuse, "a record from before reference coverage stays fresh at coverage 0");
            legacy.dense.config.pairs.reference_coverage = 2;
            expect(plan(legacy)[Step::Dense].act == Act::Redo, "a reference coverage change invalidates dense output");
            StepRecorder again(ws.string(), read_dataset_record(ws.string()));
            again.begin(Step::Dense, dense_fields(j.dense), {"roma.ply"}); again.finish(Step::Dense);
        }
        {
            // Balanced's pairing before neighbours and references measured the surface.
            auto before = j;
            before.dense.config.apply_preset("balanced");
            before.dense.config.pairs.neighbors = 6; before.dense.config.pairs.reference_coverage = 3;
            auto now = j;
            now.dense.config.apply_preset("balanced");
            StepRecorder old(ws.string(), read_dataset_record(ws.string()));
            old.begin(Step::Dense, dense_fields(before.dense), {"roma.ply"}); old.finish(Step::Dense);
            expect(plan(now)[Step::Dense].act == Act::Reuse, "a cloud from a preset's old pairing is not reused under that preset");
            now.dense.config.pairs.neighbors = 4;
            expect(plan(now)[Step::Dense].act == Act::Redo, "a neighbour change after migration did not invalidate dense output");
            StepRecorder again(ws.string(), read_dataset_record(ws.string()));
            again.begin(Step::Dense, dense_fields(j.dense), {"roma.ply"}); again.finish(Step::Dense);
        }
        std::atomic<bool> verification_cancel{false};
        auto verified = plan(j); verify_dense_reuse(verified,ws.string(),verification_cancel);
        expect(verified[Step::Dense].act == Act::Reuse,"dense reuse verifies source contents and cloud checksum");
        write_manifest(spirula::dense::reconstruction_revision - 1);
        expect(spirula::dense::artifact_complete(ws.string()) && plan(j)[Step::Dense].act == Act::Run,
               "an older reconstruction remains loadable but is regenerated by processing");
        write_manifest();
        const auto original_time = fs::last_write_time(source);
        { std::ofstream changed(source,std::ios::binary | std::ios::trunc); changed << "y"; }
        fs::last_write_time(source,original_time);
        verified = plan(j); verify_dense_reuse(verified,ws.string(),verification_cancel);
        expect(verified[Step::Dense].act == Act::Redo,"same-size same-time content edits reject dense reuse");
        write_manifest();
        j.dense.config.matching_space = "source";
        expect(plan(j)[Step::Dense].act == Act::Redo, "source image matching invalidates dense output");
        j.dense.config.matching_space = "rectified";
        j.dense.config.source_reprojection_error = 0.04;
        expect(plan(j)[Step::Dense].act == Act::Redo, "source pixel tolerance participates in saved settings");
        j.dense.config.source_reprojection_error = spirula::dense::DenseConfig{}.source_reprojection_error;
        j.dense.config.use_masks = false;
        expect(plan(j)[Step::Dense].act == Act::Redo, "mask toggle invalidates the dense artifact");
        j.dense.config.use_masks = true;
        j.dense.config.match.precision = spirula::roma::InferencePrecision::Float32;
        expect(plan(j)[Step::Dense].act == Act::Redo, "precision changes invalidate the dense artifact");
        j.dense.config.match.precision = spirula::roma::InferencePrecision::Automatic;
        j.dense.config.sparse_face_pairs = false;
        expect(plan(j)[Step::Dense].act == Act::Redo, "face selection changes invalidate the dense artifact");
        j.dense.config.sparse_face_pairs = true;
        std::ofstream(source, std::ios::app) << "changed";
        expect(plan(j)[Step::Dense].act == Act::Run, "external input changes invalidate the dense artifact");
        write_manifest();
        std::ofstream(ws / "dense" / "roma.ply", std::ios::app) << "truncated replacement";
        expect(plan(j)[Step::Dense].act == Act::Run, "an output with a mismatched size is regenerated");
        touch(ws / "dense" / "roma.ply");
        j.dense.use_for_training = false;
        j.dense.config.image_cache_bytes = 123456789;
        expect(plan(j)[Step::Dense].act == Act::Reuse, "training selection and cache budget do not invalidate dense output");
        j.dense.config.min_overlap = 0.7;
        const auto changed = plan(j);
        expect(changed[Step::Dense].act == Act::Redo && changed[Step::Model].act == Act::Reuse,
               "dense filtering changes do not rerun SfM");
        PlanRequest redo; redo.redo_model = true;
        expect(plan(j, redo)[Step::Dense].why == Why::Model, "reconstruction changes invalidate dense output");
        redo = {}; redo.redo_dense = true;
        expect(plan(j, redo)[Step::Dense].why == Why::Requested && plan(j, redo)[Step::Model].act == Act::Reuse,
               "dense-only redo leaves the camera reconstruction reusable");
        j.dense.enable = false;
        expect(plan(j)[Step::Dense].act == Act::None, "disabling dense keeps its artifact without running it");
    }

    // ---- depth and normals ---------------------------------------------------
    build(ws, made);
    {
        SfmJob j = made;
        j.geometry.enable = true;
        j.geometry.want_normal = true;
        j.geometry.want_depth = false;
        fs::create_directories(ws / "normals");
        touch(ws / "normals" / "cam0" / "00010.png");
        StepRecorder rec(ws.string(), read_dataset_record(ws.string()));
        rec.begin(Step::Geometry, geometry_fields(j.geometry), {"normal"});
        rec.finish(Step::Geometry);
        expect(plan(j)[Step::Geometry].act == Act::Reuse, "maps that are there are reused");
        j.geometry.want_depth = true;
        const DatasetPlan add = plan(j);
        expect(add[Step::Geometry].act == Act::Run && add[Step::Geometry].adds &&
                   add[Step::Geometry].kinds == std::vector<std::string>{"depth"},
               "asking for depth too only adds depth");
        expect(geometry_made(add[Step::Geometry], read_dataset_record(ws.string())).size() == 2,
               "... and the record then holds both");
        j.geometry.max_size = 512;
        expect(plan(j)[Step::Geometry].act == Act::Redo, "a new size redoes them");
        j.geometry.max_size = made.geometry.max_size;
        j.quality = 3;
        PlanRequest redo;
        redo.redo_model = true;
        const DatasetPlan p = plan(j, redo);
        expect(p[Step::Geometry].act == Act::Redo && p[Step::Geometry].why == Why::Model,
               "a rebuilt reconstruction redoes them");
    }

    // ---- no record, and the stamp a workspace from before the record left -----
    {
        fs::remove(ws / kDatasetRecordFile);
        SfmJob j = made;
        j.quality = 3;
        DatasetPlan p = plan(j);
        expect(p[Step::Frames].act == Act::Redo &&
                   p[Step::Frames].why == Why::Unrecorded &&
                   p[Step::Model].act == Act::Redo && p.ask(),
               "unrecorded video frames need known timing before their model is reused");
        std::ofstream(ws / ".spirula-frames")
            << "builtin\n--fps\n2\n--adaptive\n0\n--range\n4\n--sharp\n3\n--sync\n1\n"
               "--max-frames\n100000\n--rotate\n1\n--photos\n0\n--360\n0\n--360-size\n0\n"
               "--360-orient\n0,0,0\n--input\n/captures/walk.insv\n\n0\n";
        p = plan(made);
        expect(p[Step::Frames].act == Act::Redo && p.ask(),
               "a legacy stamp cannot recover missing frame timing");
        j = made;
        j.prep.video_fps = 4.0f;
        p = plan(j);
        expect(p[Step::Frames].act == Act::Redo && p.ask(),
               "... and still notices a new rate");
    }

    // ---- a row's switches, back from the record ------------------------------
    {
        const fs::path w = ws.string() + "_order";
        build(w, video_job(w));
        SfmJob back = video_job(w);
        back.prep.inputs[0].use_imu = false;
        restore_record_inputs(read_dataset_record(w.string()), back.prep, back.camera_model);
        expect(!back.prep.inputs[0].sequential && back.prep.inputs[0].use_imu,
               "a record made with the switches at their defaults gives them back");

        SfmJob ran = video_job(w);
        ran.prep.inputs[0].sequential = true;
        ran.prep.inputs[0].use_gps = false;
        build(w, ran);
        // As a record from before the switch has the row: every video went in
        // order, and only the model's sequence says so.
        SfmJob unset = ran;
        unset.prep.inputs[0].sequential = false;
        const bool all[kNumSteps] = {true, true, true, true};
        write_record_settings(w.string(), "{}", encode_record_inputs({unset.prep.inputs, {}}), all);
        back = video_job(w);
        restore_record_inputs(read_dataset_record(w.string()), back.prep, back.camera_model);
        expect(back.prep.inputs[0].sequential, "a video that ran in order comes back in order");
        expect(!back.prep.inputs[0].use_gps && back.prep.inputs[0].use_imu,
               "... with its sensor switches as they were");
        SfmJob clipped = video_job(w);
        clipped.prep.inputs[0].clip_start = 3.25;
        clipped.prep.inputs[0].clip_end = 61.5;
        build(w, clipped);
        back = video_job(w);
        restore_record_inputs(read_dataset_record(w.string()), back.prep, back.camera_model);
        expect(back.prep.inputs[0].clip_start == 3.25 && back.prep.inputs[0].clip_end == 61.5,
               "a video's clip comes back from the record");
        expect(plan(back)[Step::Frames].act == Act::Reuse, "... and its frames are reused");
        expect(plan(back)[Step::Model].act == Act::Reuse, "... and its model is reused");
        fs::remove_all(w);
    }

    // ---- and what the old stamps still say, back on the panel --------------
    {
        std::ofstream(ws / ".spirula-recon")
            << "builtin\n--quality\nextreme\n--data-type\nvideo\n--camera-model\n"
               "thin-prism-fisheye\n--camera-mode\nfolder\n--mapper\nflat\n--features\n"
               "sift\n--matcher\nbruteforce\n--no-prefilter-sequential\n--manifest\n"
               "image_dir: /x/images\\nrigs:\\n- name: rig\\n  kind: dual-fisheye\\n"
               "  members:\\n  - prefix: cam0\\n  - prefix: cam1\\nsequences:\\n"
               "- members:\\n  - cam0\\n  - cam1\\n\n--metric-gps\nhorizontal\n--masks\n"
               "/x/masks\n";
        SfmJob j;
        bool colmap = true;
        const DatasetRecord old = read_legacy_settings(ws.string(), j, colmap);
        expect(old.present && !colmap && j.quality == 3 && j.data_type == 1 &&
                   j.camera_model == "thin-prism-fisheye" && j.metric_gps == 1 &&
                   !j.prefilter_sequential && j.mask_features && j.prep.video_fps == 2.0f,
               "the old stamps give back the settings they recorded");
        SfmJob d = dropped_images(ws, made);
        restore_record_inputs(old, d.prep, d.camera_model);
        const PrepInput& in = d.prep.inputs[0];
        expect(in.sequential && in.subcameras[0].rig == kRigOwn &&
                   in.subcameras[1].rig_dual_fisheye,
               "... and the camera folders their rig and order");
    }

    // ---- the video behind the frames, for a run that keeps them ---------------
    {
        build(ws, made);
        std::vector<PrepCapture> c = recorded_captures(ws.string(), made.prep);
        // Absolute as the record keeps it: Windows puts the current drive in front.
        const std::string walk = fs::absolute("/captures/walk.insv").lexically_normal().generic_string();
        expect(c.size() == 1 && c[0].path == walk && c[0].subdir.empty() &&
                   c[0].source_indices(),
               "source-indexed extraction retains exact frame timing");
        SfmJob f = made;
        f.prep.force_external_decode = true;
        StepRecorder r(ws.string(), read_dataset_record(ws.string()));
        r.begin(Step::Frames, frames_fields(f.prep));
        r.finish(Step::Frames, {{"", walk, 6.0, false}});
        c = recorded_captures(ws.string(), made.prep);
        expect(c.size() == 1 && c[0].fps == 6.0,
               "legacy resampled timing survives a different decoder choice");
        const fs::path video = fs::temp_directory_path() / "spirula_dataset_plan_test.insv";
        touch(video);
        r.begin(Step::Frames, frames_fields(made.prep));
        r.finish(Step::Frames, {{"", video.string(), 7.5, true}});
        c = recorded_captures(ws.string(), made.prep);
        expect(c.size() == 1 && c[0].fps == 7.5 && c[0].lockstep,
               "what the extraction recorded wins over the fields");
        PrepCapture offset{"", video.string(), 6.0, false};
        offset.time_offset = 12.5;
        r.begin(Step::Frames, frames_fields(made.prep));
        r.finish(Step::Frames, {offset});
        c = recorded_captures(ws.string(), made.prep);
        expect(c.size() == 1 && c[0].time_offset == 12.5,
               "a clip's start, where ffmpeg numbered from, is kept with the frames");
        offset.fps = 0.0;
        r.begin(Step::Frames, frames_fields(made.prep));
        r.finish(Step::Frames, {offset});
        c = recorded_captures(ws.string(), made.prep);
        expect(c.size() == 1 && c[0].fps == 0.0 && !c[0].source_indices() &&
                   c[0].time_offset == 12.5,
               "a full-rate legacy clip cannot treat clip-relative stems as source click frames");
        SfmJob d = dropped_images(ws, made);
        c = captures_behind(d.prep);
        expect(c.size() == 1 && c[0].path == video.string() && c[0].subdir.empty(),
               "the dataset's images/ dropped back in keeps its video");
        d.prep.inputs[0].subdir = "walk";
        c = captures_behind(d.prep);
        expect(c.size() == 1 && c[0].subdir == "walk", "... under the folder it is gathered into");
        fs::remove(video);
        expect(captures_behind(d.prep).empty(), "... while the video is still there");
    }

    // ---- "the same as above" chains down the list; every frame is a rate ------
    {
        PrepJob j;
        PrepInput a, b;
        a.path = "/a.mp4";
        a.is_video = true;
        b = a;
        b.path = "/b.mp4";
        j.inputs = {a, b};
        j.video_fps = 2.0f;
        j.inputs[0].fps = 6.0f;
        expect(input_fps(j.inputs, j.video_fps, 1) == 6.0f && fps_group(j.inputs, 1) == 0,
               "the row below follows a row that states a rate");
        j.inputs[1].fps = kFpsEveryFrame;
        expect(every_frame(j, j.inputs[1]) && !every_frame(j, j.inputs[0]) &&
                   fps_group(j.inputs, 1) == 1,
               "a row set to every frame opens a group of its own");
    }

    fs::remove_all(ws);
    std::printf(g_failures ? "\nFAILED: %d\n" : "\nall passed\n", g_failures);
    return g_failures ? 1 : 0;
}

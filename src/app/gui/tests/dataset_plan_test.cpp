// dataset_plan -- which steps of a dataset run are reused and which redone
// (app/gui/DatasetPlan.h), against records written into a scratch workspace
// the way a run writes them.

#include "app/gui/DatasetPlan.h"
#include "app/gui/SfmRunner.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using namespace gui;

namespace {

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
    rec.finish(Step::Frames);
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

    // ---- the fields --------------------------------------------------------
    {
        SfmJob a = made, b = made;
        b.prep.video_fps = 3.0f;
        expect(!(frames_fields(a.prep) == frames_fields(b.prep)),
               "a video's rate is part of its frames");
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

    // ---- a fresh run over a built dataset -------------------------------------
    build(ws, made);
    {
        const DatasetPlan p = plan(made);
        expect(p[Step::Frames].act == Act::Reuse && p[Step::Masks].act == Act::Reuse &&
                   p[Step::Model].act == Act::Reuse && !p.ask(),
               "the same settings reuse every step");
        expect(p[Step::Geometry].act == Act::None, "geometry off is not a step");
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
            rec.finish(s);
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
        expect(p[Step::Frames].act == Act::Run && p[Step::Frames].why == Why::Resume,
               "frames interrupted on the same settings are finished");
        expect(p[Step::Model].act == Act::Redo,
               "... and the model built from the frames before them is not trusted");
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
        expect(p[Step::Frames].why == Why::Unrecorded && p[Step::Model].why == Why::Unrecorded &&
                   !p.ask(),
               "with no record, what is there is kept");
        std::ofstream(ws / ".spirula-frames")
            << "builtin\n--fps\n2\n--adaptive\n0\n--range\n4\n--sharp\n3\n--sync\n1\n"
               "--max-frames\n100000\n--rotate\n1\n--photos\n0\n--360\n0\n--360-size\n0\n"
               "--360-orient\n0,0,0\n--input\n/captures/walk.insv\n\n0\n";
        p = plan(made);
        expect(p[Step::Frames].act == Act::Reuse && p[Step::Frames].why == Why::None,
               "the old stamp still matches its own frames");
        j = made;
        j.prep.video_fps = 4.0f;
        p = plan(j);
        expect(p[Step::Frames].act == Act::Redo && p.ask(),
               "... and still notices a new rate");
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

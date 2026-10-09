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
        {
            SfmJob spatial = made;
            spatial.pairs = 4;
            const fs::path spatial_ws = ws / "spatial-plan";
            spatial.prep.workspace = spatial_ws.string();
            build(spatial_ws, spatial);
            touch(spatial_ws / "features" / "cam0" / "00010.jpg.bin");
            touch(spatial_ws / "matches.bin");
            touch(spatial_ws / sfm::resume::kDir / sfm::resume::kExtractSig);
            touch(spatial_ws / sfm::resume::kDir / sfm::resume::kMatchSig);
            for (int field = 0; field < 4; ++field) {
                SfmJob changed = spatial;
                if (field == 0) ++changed.block_size;
                if (field == 1) ++changed.block_neighbours;
                if (field == 2) changed.block_radius = 123;
                if (field == 3) ++changed.block_cache_mb;
                const DatasetPlan blocks = plan(changed);
                expect(blocks[ModelPart::Features].act == Act::Reuse &&
                           blocks[ModelPart::Matching].act == Act::Redo &&
                           blocks[ModelPart::Matching].why == Why::Settings,
                       "a spatial block setting keeps features and redoes matching");
            }
        }
        j = made;
        j.mapper = 1;
        p = plan(j);
        expect(p[ModelPart::Matching].act == Act::Reuse &&
                   p[ModelPart::Mapping].act == Act::Redo &&
                   p[ModelPart::Mapping].why == Why::Settings,
               "a mapper setting only maps again");
        j = made;
        j.map_memory_mb = 2048;
        p = plan(j);
        expect(p[ModelPart::Features].act == Act::Reuse &&
                   p[ModelPart::Matching].act == Act::Reuse &&
                   p[ModelPart::Mapping].act == Act::Redo &&
                   p[ModelPart::Mapping].why == Why::Settings,
               "a mapping memory budget reuses features and matches");
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

    // ---- the video behind the frames, for a run that keeps them ---------------
    {
        build(ws, made);
        std::vector<PrepCapture> c = recorded_captures(ws.string(), made.prep);
        // Absolute as the record keeps it: Windows puts the current drive in front.
        const std::string walk = fs::absolute("/captures/walk.insv").lexically_normal().generic_string();
        expect(c.size() == 1 && c[0].path == walk && c[0].subdir.empty(),
               "the frames' fields name the video they were cut from");
        SfmJob f = made;
        f.prep.force_external_decode = true;
        StepRecorder r(ws.string(), read_dataset_record(ws.string()));
        r.begin(Step::Frames, frames_fields(f.prep));
        r.finish(Step::Frames);
        c = recorded_captures(ws.string(), f.prep);
        expect(c.size() == 1 && c[0].fps == 6.0,
               "ffmpeg's stems count candidates at the rate times the window");
        const fs::path video = fs::temp_directory_path() / "spirula_dataset_plan_test.insv";
        touch(video);
        r.begin(Step::Frames, frames_fields(made.prep));
        r.finish(Step::Frames, {{"", video.string(), 7.5, true}});
        c = recorded_captures(ws.string(), made.prep);
        expect(c.size() == 1 && c[0].fps == 7.5 && c[0].lockstep,
               "what the extraction recorded wins over the fields");
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

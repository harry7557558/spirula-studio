#pragma once

// Which steps of a dataset run reuse what the workspace holds and which redo
// it. ONE answer: the panel asks it to say what pressing the button will do,
// and the runner asks it again at each step, so the two cannot disagree.
// The rules and why they are what they are: docs/notes/dataset-rerun.md

#include "app/gui/DatasetRecord.h"
#include "app/gui/GeometryRunner.h"
#include "app/gui/DenseRunner.h"

#include <string>
#include <vector>

namespace gui {

struct SfmJob;
struct ColmapJob;

// The built-in reconstruction's stages: `spirula sfm` keeps features/ and
// matches.bin while the settings that made them read the same, always maps
// again, and `spirula lidar` keeps an alignment made from the same scans.
enum class ModelPart { Features, Matching, Mapping, Align };
inline constexpr int kNumModelParts = 4;

// A stage kept although the plan would make it again, or made again although
// it would be kept. Honoured only where StepPlan::lock allows.
enum class PartChoice { Auto, Keep, Run };

// What the user asked for, as opposed to what the settings imply.
struct PlanRequest {
    bool redo_frames = false, redo_masks = false, redo_model = false;
    bool redo_geometry = false;
    bool redo_dense = false;
    // Keep frames and a reconstruction whose settings differ from the panel's.
    bool keep_built = false;
    PartChoice parts[kNumModelParts] = {};
};

// What each step's output is made with (StepRecord::fields).
StepFields frames_fields(const PrepJob& job);
StepFields masks_fields(const PrepJob& job);
StepFields model_fields(const SfmJob& job);
StepFields model_fields(const ColmapJob& job, const PrepJob& prep);
StepFields geometry_fields(const GeometryJob& job);
StepFields dense_fields(const DenseJob& job);
std::vector<std::string> geometry_kinds(const GeometryJob& job);

// Every input's images are already the dataset's own (a finished dataset's
// images/ dropped back on the panel, or photos read where they are), so there
// is nothing to extract. The masks half asks the same of the masks.
bool frames_in_dataset(const PrepJob& job);
// Some input's frames are the blur rule's to sort: a video cut at a rate, or
// photographs gathered into the dataset.
bool blur_rule_applies(const PrepJob& job);
bool masks_in_dataset(const PrepJob& job);

// One shape for both engines.
struct PlanJob {
    PrepJob prep;
    bool reconstruct = true;   // false: the laser scans' poses place the images
    StepFields model;
    bool mask_features = true;
    GeometryJob geometry;
    // The built-in engine, whose reconstruction is planned stage by stage.
    bool staged = false;
    std::vector<std::string> lidar_clouds;
    bool lidar_in_frame = false;
    DenseJob dense;
};
PlanJob plan_job(const SfmJob& job);
PlanJob plan_job(const ColmapJob& job, const PrepJob& prep);

enum class Act {
    None,    // not part of this run
    Run,     // nothing finished on disk yet (or an interrupted run to finish)
    Reuse,   // on disk and current
    Redo,    // on disk, and replaced
    Keep,    // on disk, settings differ, kept because the user said so
};
enum class Why {
    None,
    Requested,   // a re-do button, "Reconstruct again", the overwrite box
    Settings,    // its settings differ from the ones it was made with
    Frames,      // the frames under it are being replaced
    Model,       // the reconstruction under it is being replaced
    Features,    // the feature points under it are being replaced
    Matches,     // the matches under it are being replaced
    Masks,       // only where the masks it was made with are being replaced
    Moved,       // made from the images in another folder
    Stale,       // made from an earlier output of the step before it
    Resume,      // a run of it was interrupted
    InDataset,   // the input already is the dataset's own
    Unrecorded,  // there is no record of how it was made
};

struct FieldChange {
    std::string key, scope, was, now;   // `was` / `now` empty: absent
};

// Why a stage of the reconstruction can be neither kept nor made again
// against the plan: each is a combination that cannot come out right.
enum class Lock {
    None,
    Nothing,    // nothing finished on disk to keep
    Frames,     // new frames: the old ones' feature points describe nothing
    Frontend,   // feature points of another type than the one chosen
    Before,     // the stage before it is made again
    Lens,       // matches.bin carries the lenses verification used
    Always,     // mapping is the reconstruction being made
};

struct StepPlan {
    Act act = Act::None;
    Why why = Why::None;
    Lock lock = Lock::None;   // a stage of the reconstruction only
    std::vector<FieldChange> changes;
    // Rebuilds something the user did not ask to: confirmed first.
    bool ask = false;
    // Frames, reused: the blur rule is applied to them where they lie first.
    bool sort = false;
    // Model: kept although the masks its feature points avoided have changed.
    bool masks_changed = false;
    // Geometry: the kinds of map this run makes, and whether they are only
    // the kinds not made yet, beside maps that stay.
    std::vector<std::string> kinds;
    bool adds = false;
};

struct DatasetPlan {
    StepPlan steps[kNumSteps];
    // Act::None for a stage the run will not reach.
    StepPlan parts[kNumModelParts];
    StepPlan& operator[](Step s) { return steps[(int)s]; }
    const StepPlan& operator[](Step s) const { return steps[(int)s]; }
    StepPlan& operator[](ModelPart s) { return parts[(int)s]; }
    const StepPlan& operator[](ModelPart s) const { return parts[(int)s]; }
    bool ask() const;
};

void verify_dense_reuse(DatasetPlan& plan, const std::string& dataset, const std::atomic<bool>& cancel);

inline bool makes(Act a) { return a == Act::Run || a == Act::Redo; }

// The workspace's own record, with a frames step read off the stamp a workspace
// from before the record carries.
DatasetRecord read_plan_record(const std::string& workspace, const PrepJob& job);

// `done` fixes every step before `from`: what a running job already did.
DatasetPlan plan_dataset(const PlanJob& job, const WorkspaceState& ws,
                         const DatasetRecord& rec, const PlanRequest& req,
                         const DatasetPlan* done = nullptr,
                         Step from = Step::Frames);

// The input rows' own settings back from `rec`: row by row onto the same
// inputs (clicks too), or onto a dataset's own images/ dropped back in, by the
// camera folders its reconstruction recorded. `camera_model`: the first row's.
void restore_record_inputs(const DatasetRecord& rec, PrepJob& job,
                           std::string& camera_model);

// The videos a workspace's frames were cut from: as recorded when the frames
// were made, or else as their fields (or the legacy stamp) say. `job` fills
// in what a legacy stamp leaves to probing.
std::vector<PrepCapture> recorded_captures(const std::string& workspace, const PrepJob& job);
// Those of every photo input that is a dataset's images/ -- a dataset made
// from a video dropped back in as its frames -- under the input's subdir.
// Only videos that are still there.
std::vector<PrepCapture> captures_behind(const PrepJob& job);

// What a workspace from before the record left in its stamps, onto `job`, with
// its camera folders' lenses, rigs and sequences as a model step for
// restore_record_inputs. `present` is false when it left none.
DatasetRecord read_legacy_settings(const std::string& workspace, SfmJob& job,
                                   bool& colmap);

// ---- what a runner does with the answer ----

// The masks step's answer, as DatasetPrep reads it.
void apply_masks_plan(const StepPlan& s, PrepJob& job);
// The job to run for the geometry step: overwrite when redoing, and only the
// kinds of map the plan names.
GeometryJob geometry_for_plan(GeometryJob g, const StepPlan& s);
// What the record will say exists once the geometry step finishes.
std::vector<std::string> geometry_made(const StepPlan& s, const DatasetRecord& rec);
// The log lines that say why a step is reused, kept or redone.
std::vector<std::string> plan_log_lines(Step step, const StepPlan& s,
                                        const std::string& workspace);
// "key[scope]: was -> now; ...", for a log line.
std::string describe_changes(const std::vector<FieldChange>& changes);

// What a runner writes into the record around a step it runs.
class StepRecorder {
public:
    StepRecorder(std::string workspace, const DatasetRecord& rec);
    // Marks the step started -- an interruption leaves it incomplete -- under
    // a new id that the steps after it are made from.
    void begin(Step s, StepFields fields, std::vector<std::string> made = {});
    // `captures`: the frames step's (StepRecord::captures).
    void finish(Step s, std::vector<PrepCapture> captures = {});
    const std::string& id(Step s) const { return _ids[(int)s]; }

private:
    std::string _ws;
    std::string _ids[kNumSteps];
    StepRecord _open[kNumSteps];
};

}  // namespace gui

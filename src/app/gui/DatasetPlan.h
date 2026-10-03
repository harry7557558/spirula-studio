#pragma once

// Which steps of a dataset run reuse what the workspace holds and which redo
// it. ONE answer: the panel asks it to say what pressing the button will do,
// and the runner asks it again at each step, so the two cannot disagree.
// The rules and why they are what they are: docs/notes/dataset-rerun.md

#include "app/gui/DatasetRecord.h"
#include "app/gui/GeometryRunner.h"

#include <string>
#include <vector>

namespace gui {

struct SfmJob;
struct ColmapJob;

// What the user asked for, as opposed to what the settings imply.
struct PlanRequest {
    bool redo_frames = false, redo_masks = false, redo_model = false;
    bool redo_geometry = false;
    // Keep frames and a reconstruction whose settings differ from the panel's.
    bool keep_built = false;
};

// What each step's output is made with (StepRecord::fields).
StepFields frames_fields(const PrepJob& job);
StepFields masks_fields(const PrepJob& job);
StepFields model_fields(const SfmJob& job);
StepFields model_fields(const ColmapJob& job, const PrepJob& prep);
StepFields geometry_fields(const GeometryJob& job);
std::vector<std::string> geometry_kinds(const GeometryJob& job);

// Every input's images are already the dataset's own (a finished dataset's
// images/ dropped back on the panel, or photos read where they are), so there
// is nothing to extract. The masks half asks the same of the masks.
bool frames_in_dataset(const PrepJob& job);
bool masks_in_dataset(const PrepJob& job);

// One shape for both engines.
struct PlanJob {
    PrepJob prep;
    StepFields model;
    bool mask_features = true;
    GeometryJob geometry;
    // The model step makes kDenseModelDir and leaves the dataset's own model,
    // and so every map's camera, as it was.
    bool keep_cameras = false;
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
    Stale,       // made from an earlier output of the step before it
    Resume,      // a run of it was interrupted
    InDataset,   // the input already is the dataset's own
    Unrecorded,  // there is no record of how it was made
};

struct FieldChange {
    std::string key, scope, was, now;   // `was` / `now` empty: absent
};

struct StepPlan {
    Act act = Act::None;
    Why why = Why::None;
    std::vector<FieldChange> changes;
    // Rebuilds something the user did not ask to: confirmed first.
    bool ask = false;
    // Model: kept although the masks its feature points avoided have changed.
    bool masks_changed = false;
    // Geometry: the kinds of map this run makes, and whether they are only
    // the kinds not made yet, beside maps that stay.
    std::vector<std::string> kinds;
    bool adds = false;
};

struct DatasetPlan {
    StepPlan steps[kNumSteps];
    StepPlan& operator[](Step s) { return steps[(int)s]; }
    const StepPlan& operator[](Step s) const { return steps[(int)s]; }
    bool ask() const;
};

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
    void finish(Step s);
    const std::string& id(Step s) const { return _ids[(int)s]; }

private:
    std::string _ws;
    std::string _ids[kNumSteps];
    StepRecord _open[kNumSteps];
};

}  // namespace gui

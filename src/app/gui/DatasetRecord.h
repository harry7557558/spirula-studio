#pragma once

// What built the dataset in a workspace, kept beside it: the panel as it was
// when a run last started -- restored when the panel is pointed back at the
// folder -- and, per step, the settings that step's output on disk was made
// with, which is what DatasetPlan.h compares against. docs/notes/dataset-rerun.md

#include "app/gui/DatasetPrep.h"
#include "data/Json.h"

#include <string>
#include <vector>

namespace gui {

// In the workspace root, dotted: no parser looks for it.
inline constexpr const char* kDatasetRecordFile = ".spirula-dataset.json";

enum class Step { Frames, Masks, Model, Geometry };
inline constexpr int kNumSteps = 4;

// One setting a step's output depends on. `scope` is what it applies to -- an
// input, a camera folder -- and empty for the whole dataset. `value` is
// compared as text, so it is an identifier and never translated.
struct StepField {
    std::string key, scope, value;
    bool operator==(const StepField& o) const {
        return key == o.key && scope == o.scope && value == o.value;
    }
};
using StepFields = std::vector<StepField>;

struct StepRecord {
    bool present = false;
    // New each time the step writes its output, so a step made from an older
    // output of the one before it can tell (`frames_id` and so on).
    std::string id;
    std::string frames_id, masks_id, model_id;
    // False from the moment the step starts until it finishes.
    bool complete = false;
    StepFields fields;
    // Geometry: the kinds of map it finished, "normal" / "depth".
    std::vector<std::string> made;
};

struct DatasetRecord {
    bool present = false;
    // DatasetPreset's field table, each setting as the run that made its
    // step left it: one that kept the reconstruction does not speak for it.
    JsonValue settings;
    // encode_record_inputs, as the last run started, and as the run that
    // made each step did.
    JsonValue inputs;
    JsonValue step_inputs[kNumSteps];
    StepRecord steps[kNumSteps];
    const StepRecord& step(Step s) const { return steps[(int)s]; }
};

// Never throws: an unreadable file is no record.
DatasetRecord read_dataset_record(const std::string& workspace);

// Each rewrites only its own part of the file. `makes` names the steps the
// run starting now makes, which the settings are then recorded as making.
void write_record_settings(const std::string& workspace,
                           const std::string& settings_json,
                           const std::string& inputs_json,
                           const bool makes[kNumSteps]);
void write_step_record(const std::string& workspace, Step step,
                       const StepRecord& rec);
std::string new_step_id();

// The input rows' own settings (lens, rate, rig, order) -- not what a probe of
// the file will say again -- and the clicks drawn on them.
struct RecordInputs {
    std::vector<PrepInput> rows;
    std::vector<MaskClick> clicks;
};
std::string encode_record_inputs(const RecordInputs& in);
RecordInputs decode_record_inputs(const JsonValue& v);

// A stamp a workspace written before the record carries -- `.spirula-frames`,
// `.spirula-recon` -- as its tokens: the engine or decoder, then the flags and
// their values. Empty if none.
std::vector<std::string> read_legacy_stamp(const std::string& workspace,
                                           const char* file);

}  // namespace gui

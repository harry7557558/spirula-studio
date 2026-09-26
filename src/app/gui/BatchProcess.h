#pragma once

// Batch processing: a list of rows, each of which can build a dataset, train
// it any number of times, and mesh what came out. None of those three has to
// exist yet when the queue is started.
//
// The list is data, not a runner. Driving it is GuiApp::advance_batch() --
// launch a task, wait for the runner that owns that stage, record, launch the
// next -- which reuses the live screens rather than being a second
// implementation of what those runners already are.
//
// Two properties an unattended queue must have: a failed task is an ordinary
// transition (the rest of its row is skipped, the next row still runs), and
// batch_check_row() reports the reasons a row will fail BEFORE anything starts.

#include "app/gui/DatasetPreset.h"
#include "app/gui/MeshPreset.h"
#include "config/TrainConfig.h"
#include "i18n/Message.h"

#include <functional>
#include <string>
#include <vector>

namespace gui {

// The three things a row can do, in the order they happen.
enum class BatchStage { Dataset, Train, Mesh };
inline constexpr int kNumBatchStages = 3;

enum class BatchStatus { Pending, Running, Done, Failed, Skipped, Stopped };

// One thing a pre-flight found. `text` is interface copy with an optional
// {0}; `raw` is engine text (a flag name, a parser message) that stays in
// English -- exactly the ui:: / ui::*Raw split. Exactly one of them is set.
struct BatchIssue {
    const spirula::i18n::Msg* text = nullptr;
    std::string arg;      // fills {0} in `text`
    std::string raw;      // the whole line, untranslated
    bool fatal = false;   // blocks the start; otherwise a warning
    BatchStage stage = BatchStage::Dataset;   // which half of the row it is about
};

// The line to draw. Already translated and formatted, so it goes on screen
// through a ui::*Raw call.
std::string batch_issue_line(const BatchIssue& issue);
bool batch_has_error(const std::vector<BatchIssue>& issues);

// A preset a stage points at: a saved file, or -- with no path -- the built-in
// of that name (kTrainPresets, kDatasetPresets). Meshing has no built-ins, so
// there an empty name is the stock settings.
struct BatchPreset {
    std::string path;
    std::string name;
};

// One training run of a row: which preset, and the three numbers changed often
// enough that a preset per combination would be the wrong shape of work.
struct BatchRun {
    BatchPreset preset{"", "3dgs"};
    // "" is "whatever the preset says". TEXT, because 0 is a legal
    // --sh-degree and so is "unset", and those are different answers.
    std::string cap_max;
    std::string sh_degree;
    std::string iterations;
    // Does the row's Mesh stage cover this run? Off for the big appearance
    // run that would run out of memory meshing, on for the cheap one trained
    // beside it for exactly that purpose.
    bool mesh = true;
};

// What the Mesh stage writes, over whatever its preset says. Both sets are
// empty for "whatever the preset says"; otherwise they are the colors and the
// formats themselves, as bits over kMeshColorModes / kMeshFormats.
struct BatchMeshOptions {
    BatchPreset preset{"", ""};
    int colors = 0;
    int formats = 0;
};

struct BatchRow {
    // ---- what it builds a dataset from ----
    std::vector<std::string> sources;   // videos and folders of photos
    BatchPreset dataset_preset{"", "general"};

    // Where the Dataset stage writes and what the Train stage reads. Left
    // empty on a row that creates one, it is derived from the sources and
    // filled in as that stage finishes.
    std::string dataset;

    // ---- training ----
    // One run each; empty means one run on the built-in default, so a row that
    // just says "train this" needs nothing filled in.
    std::vector<BatchRun> runs;
    std::string output_dir;             // "" = <dataset>/outputs

    // ---- meshing ----
    // The model to mesh. Empty means the runs this row trained, which is what
    // lets a mesh be asked for before the model exists.
    std::string model;
    BatchMeshOptions mesh;

    bool stages[kNumBatchStages] = {false, true, false};
    bool enabled = true;      // kept on the list, left out of this run

    // From the last batch_check_row(); empty until one has run.
    std::vector<BatchIssue> issues;

    bool does(BatchStage s) const { return stages[(int)s]; }
    bool& does(BatchStage s) { return stages[(int)s]; }
};

// One unit of work: one stage of one row. What it reads is resolved when it
// starts rather than when the queue was planned, because for most of them it
// does not exist yet.
struct BatchTask {
    int row = 0;
    BatchStage stage = BatchStage::Train;
    // Which training run this is, and which of them a meshing task follows.
    int variant = 0;
    BatchStatus status = BatchStatus::Pending;

    std::string message;      // engine text (English) when it failed
    std::string result;       // the dataset folder / run folder / mesh file
    // A Dataset task hands these to the row's training runs: a dataset whose
    // photographs are read where they lie names them itself.
    std::string image_dir, mask_dir;
    bool mask_flipped = false;
    int steps = 0;
    // Wall clock, for the progress block: when it was launched, and how long
    // it took once it is over.
    double started_at = 0.0;
    double seconds = 0.0;
};

// The tasks the rows expand into, in the order they will run.
std::vector<BatchTask> batch_plan(const std::vector<BatchRow>& rows);

// How many training runs a row asks for: one per entry, and one when it names
// none at all. batch_run_of() is that implicit run made explicit.
int batch_num_runs(const BatchRow& row);
BatchRun batch_run_of(const BatchRow& row, int variant);
// How many of those runs the Mesh stage covers.
int batch_num_meshes(const BatchRow& row);

// What the queue has taken and what is left. `frac` is how far through the
// running task its own runner says it is (<0 when it cannot say), and
// `elapsed` how long that task has been going.
struct BatchProgress {
    int done = 0, total = 0, running = 0;
    double task_remaining = -1.0;   // the running task, seconds; -1 unknown
    double remaining = -1.0;        // the whole queue, seconds; -1 unknown
};
BatchProgress batch_progress(const std::vector<BatchTask>& tasks, int current,
                             double frac, double elapsed);

// What stands for the summary line inside the command a finished queue runs.
// Braces because the alternatives are punctuation a shell would eat: `<>` is
// redirection, `%..%` and `$..` are variables. gui::command_argv() fills it.
inline constexpr char kBatchMessageToken[] = "{message}";

// What this build and this machine can do, asked once and handed to every
// row's check so that no two rows can get different answers.
struct BatchCapabilities {
    bool device = false;          // a GPU the engine can use
    bool builtin_sfm = false;
    bool colmap = false;
    bool masking = false;         // segmentation is compiled in and usable
    bool geometry = false;        // `spirula geometry` is available
    // Is that checkpoint (and its text detector) already on disk? A batch cannot
    // stop to accept a licence or wait on a 2 GB download, so a missing one is
    // found here.
    std::function<bool(const std::string&, const std::string&)> mask_model_ready;
    // Does it read the text prompt? BiRefNet needs none, and runs without one.
    std::function<bool(const std::string&)> mask_model_prompted;
    std::function<bool(const std::string&)> geometry_model_ready;
};

// Check one row. `all`/`index` are for the checks that are about the list
// rather than the row. Does no GPU work and writes nothing.
std::vector<BatchIssue> batch_check_row(const BatchRow& row,
                                        const std::vector<BatchRow>& all,
                                        int index,
                                        const BatchCapabilities& caps);

// ---- what each stage runs with ------------------------------------------
//
// Each returns false with `error` (English) when a preset file has gone
// missing since the pre-flight -- the one thing that can change under a queue.

// The input list, the settings and the folder a row's Dataset stage needs.
// The capture's own defaults are applied first and the preset over them, so a
// preset decides everything except the lens a capture is KNOWN to need.
bool batch_build_dataset_job(const BatchRow& row, const std::string& ffmpeg_exe,
                             std::vector<PrepInput>& sources,
                             DatasetSettings& settings, std::string& workspace,
                             std::string& error);

// The config a row's `variant`-th training run uses; `dataset` and the two
// folders beside it are what the Train stage resolved. `preset_base` comes
// back as the built-in name to record in the run's config.json.
bool batch_build_train_config(const BatchRow& row, int variant,
                              const std::string& dataset,
                              const std::string& image_dir,
                              const std::string& mask_dir, bool mask_flipped,
                              TrainConfig& cfg, std::string& preset_base,
                              std::string& error);

// The meshing job for one model. `dataset` is what the row resolved; empty
// lets the child read it out of the run's own config.json.
bool batch_build_mesh_job(const BatchRow& row, const std::string& model,
                          const std::string& dataset, MeshJob& job,
                          std::string& error);

// Where a row's dataset goes when the row does not say: derived from the
// sources exactly as the New Dataset screen derives it.
std::string batch_dataset_workspace(const BatchRow& row);

// The list, kept across sessions in <config_dir>/batch.json: losing a queue to
// a crash three hours in is the one failure a user cannot recover from by
// trying again. Neither call throws; an unreadable list comes back empty.
std::vector<BatchRow> load_batch_list();
void save_batch_list(const std::vector<BatchRow>& rows);

}  // namespace gui

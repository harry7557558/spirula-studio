#pragma once

// `spirula sfm` options the panel edits straight off the option table
// (sfm/SfmConfig.h): image size, feature count, pair-selection breadth and the
// selected detector's and matcher's own knobs. An option here needs no member,
// preset key, command-line code or plan field of its own -- SfmJob::options
// holds what differs from the default and this file is every consumer.

#include "app/gui/SfmRunner.h"

#include <string>
#include <vector>

namespace gui {

enum class SfmOptionType { Switch, Integer, Real, Text };

// One row of the table as the panel reads it.
struct SfmOption {
    std::string flag;                 // without the dashes
    SfmOptionType type = SfmOptionType::Text;
    double lo = 0, hi = 0;            // lo >= hi: unbounded
    std::vector<std::string> choices;
    const spirula::i18n::Msg* help = nullptr;
    std::string owner;                // "sift", "lightglue", "prefilter"; "" = shared
    ModelPart part = ModelPart::Features;   // the first stage it reaches
};

// The rows one detector, matcher or pair mode owns, in table order. Empty in a
// build without the reconstruction module.
std::vector<SfmOption> sfm_owned_options(const std::string& owner);
bool find_sfm_option(const std::string& flag, SfmOption& out);

// Which owner's rows the selected detector and matcher read.
std::string sfm_detector_owner(int features);
std::string sfm_matcher_owner(int features, int matcher);
// The selected detector's feature-count flag: their budgets are not comparable.
const char* sfm_feature_count_flag(int features);

// What a row holds with nothing overriding it: the table's default once the
// quality preset and the frontend's own have moved it. Switches read on/off.
std::string sfm_option_default(const SfmReconSettings& s, const std::string& flag);
std::string sfm_option_value(const SfmReconSettings& s, const std::string& flag);
// Stores an override, or drops it when `value` is the default.
void set_sfm_option(SfmReconSettings& s, const std::string& flag, const std::string& value);

// What a run with these choices is given: the valid overrides of a selected
// owner that differ from the default. The command line and the plan's fields
// both read this list, so neither can carry an option the other does not.
SfmOptionMap active_sfm_options(const SfmReconSettings& s);
void append_sfm_option_args(const SfmOptionMap& options, std::vector<std::string>& argv);
// Drops what the table does not know or would refuse.
void sanitize_sfm_options(SfmOptionMap& options);

// An option as a model field of the plan (DatasetPlan.h): its key, and the
// stage a field of that key belongs to. False for a key that is no option.
std::string sfm_option_field(const std::string& flag);
bool sfm_option_part(const std::string& field, ModelPart& part);

}  // namespace gui

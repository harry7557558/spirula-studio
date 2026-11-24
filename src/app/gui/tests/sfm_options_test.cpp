// SfmOptions against the real option table: what the panel shows for each
// quality and frontend, which overrides reach a run, and that every detector
// and matcher row is filed where the panel will find it.

#include "app/gui/SfmOptions.h"
#include "sfm/SfmConfig.h"

#include <cstdio>
#include <set>

using namespace gui;

namespace {

int g_failures = 0;

void expect(bool ok, const std::string& what) {
    std::printf("%s  %s\n", ok ? "ok  " : "BAD ", what.c_str());
    if (!ok) g_failures++;
}

bool has(const std::vector<std::string>& argv, const std::string& a, const std::string& b) {
    for (size_t i = 0; i + 1 < argv.size(); i++)
        if (argv[i] == a && argv[i + 1] == b) return true;
    return false;
}

}  // namespace

int main() {
    // ---- the boxes show what the quality preset picked ----------------------
    {
        SfmReconSettings s;
        expect(sfm_option_default(s, "max-features") == "8192" &&
                   sfm_option_default(s, "max-image-size") == "2400" &&
                   sfm_option_default(s, "prefilter-neighbors") == "32",
               "High with SIFT reads 8192 features, 2400 px, 32 neighbours");
        s.quality = 0;
        expect(sfm_option_default(s, "max-features") == "2048" &&
                   sfm_option_default(s, "max-image-size") == "1000" &&
                   sfm_option_default(s, "prefilter-neighbors") == "16",
               "Fast moves all three");
        s.features = 2;   // ALIKED
        expect(std::string(sfm_feature_count_flag(s.features)) == "aliked-max-features" &&
                   sfm_option_default(s, "aliked-max-features") == "1024" &&
                   sfm_option_default(s, "max-image-size") == "800",
               "a learned detector has its own count and size ladder");
        expect(sfm_option_default(s, "ratio") == "0.92", "... and its own ratio test");
        s.matcher = 1;    // LightGlue
        expect(sfm_option_default(s, "ratio") == "1", "a learned matcher switches it off");
    }

    // ---- an override is what differs from that, and only while it applies ---
    {
        SfmReconSettings s;
        set_sfm_option(s, "max-features", "8192");
        expect(s.options.empty(), "typing the default back stores nothing");
        set_sfm_option(s, "max-features", "4096");
        set_sfm_option(s, "octaves", "5");
        set_sfm_option(s, "prefilter-neighbors", "64");
        expect(active_sfm_options(s).size() == 3, "three overrides reach a run");
        std::vector<std::string> argv;
        append_sfm_option_args(active_sfm_options(s), argv);
        expect(has(argv, "--max-features", "4096") && has(argv, "--octaves", "5") &&
                   has(argv, "--prefilter-neighbors", "64"),
               "... as their flags");
        s.quality = 1;
        expect(active_sfm_options(s).count("max-features") == 0,
               "an override equal to the new quality's value is no override");
        s.quality = 2;
        s.features = 2;
        expect(active_sfm_options(s).count("octaves") == 0 &&
                   active_sfm_options(s).count("max-features") == 0,
               "SIFT's options stay behind when another detector is selected");
        s.pairs = kPairsExhaustive;
        expect(active_sfm_options(s).empty(), "pair selection's stay behind without it");
        s.pairs = kPairsSequential;
        expect(active_sfm_options(s).count("prefilter-neighbors") == 1,
               "... but sequential pairing's loop closure reads them");

        SfmReconSettings sw;
        set_sfm_option(sw, "cross-check", "off");
        argv.clear();
        append_sfm_option_args(active_sfm_options(sw), argv);
        expect(argv == std::vector<std::string>{"--no-cross-check"}, "a switch is a flag");

        SfmOptionMap junk = {{"no-such-flag", "1"}, {"octaves", "99"}, {"octaves-x", "3"},
                             {"edge-threshold", "12"}};
        sanitize_sfm_options(junk);
        expect(junk.size() == 1 && junk.count("edge-threshold"),
               "an unknown flag or an out-of-range value does not survive a preset file");
    }

    // ---- the stage a change reaches -----------------------------------------
    {
        ModelPart part = ModelPart::Mapping;
        expect(sfm_option_part("max_image_size", part) && part == ModelPart::Features,
               "the image size is extraction's");
        expect(sfm_option_part("prefilter_neighbors", part) && part == ModelPart::Matching,
               "pair selection is matching's");
        expect(sfm_option_part("lightglue_min_score", part) && part == ModelPart::Matching,
               "a matcher's own option is matching's");
        expect(!sfm_option_part("lens", part), "a field that is no option is not filed here");
        expect(sfm_option_part("quality", part) && part == ModelPart::Features,
               "the quality preset moves the extraction, so it is filed there");
    }

    // ---- every detector and matcher row has somewhere to be drawn ------------
    {
        const std::set<std::string> owners = {"sift", "aliked", "loma", "bruteforce",
                                              "lightglue", "flexglue", "prefilter"};
        // Rows of these two groups every frontend shares, drawn nowhere per owner.
        const std::set<std::string> shared = {"features", "matcher", "max-matches", "verify",
                                              "min-inliers"};
        bool filed = true;
        for (const sfm::FieldInfo& f : sfm::configFields()) {
            const std::string group = f.group;
            if (group.rfind("features", 0) != 0 && group.rfind("matching", 0) != 0) continue;
            const std::string owner = sfm::fieldOwner(f);
            if (owner.empty() ? shared.count(f.name) == 0 : owners.count(owner) == 0) {
                std::printf("      --%s has no owner the panel draws\n", f.name);
                filed = false;
            }
        }
        expect(filed, "every features/matching row names a detector, matcher or pair mode");
        std::set<std::string> selectable = {"prefilter"};
        for (int features = 0; features < 5; features++)
            for (int matcher = 0; matcher < 3; matcher++) {
                selectable.insert(sfm_detector_owner(features));
                selectable.insert(sfm_matcher_owner(features, matcher));
            }
        expect(selectable == owners, "... and each of those can be selected on the panel");
    }

    if (g_failures) {
        std::printf("sfm_options_test: %d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("sfm_options_test: OK\n");
    return 0;
}

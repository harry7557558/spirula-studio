// The table as the GUI's options editor reads it (describeConfigFields): each
// row's kind matches its field, and every value it shows, passed back the way
// the editor passes an edit, parses to that same value.
#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include "sfm/SfmConfig.h"
#include "sfm/tests/TestMain.h"

using namespace sfm;

static void check(bool condition, const std::string& message, int& fails) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", message.c_str());
    fails++;
}

static const FieldView* rowOf(const std::vector<FieldView>& rows, const char* name) {
    for (const FieldView& f : rows)
        if (f.name == std::string(name)) return &f;
    return nullptr;
}

static int cmdOptionsTableTest(int, char**) {
    int fails = 0;
    SfmConfig cfg;
    cfg.quality = "extreme";
    std::vector<PresetChange> moved;
    check(applyPresets(cfg, {}, moved).empty(), "extreme applies", fails);
    const std::vector<FieldView> rows = describeConfigFields(cfg, CMD_AUTO);

    std::set<std::string> names;
    for (const FieldView& f : rows) {
        check(names.insert(f.name).second, std::string("one row per flag: ") + f.name, fails);
        check(f.tier != Tier::Alias, std::string("no alias offered: ") + f.name, fails);
    }
    const FieldView* ratio = rowOf(rows, "ratio");
    const FieldView* inliers = rowOf(rows, "min-inliers");
    const FieldView* cross = rowOf(rows, "cross-check");
    const FieldView* neighbors = rowOf(rows, "prefilter-neighbors");
    check(ratio && ratio->kind == FieldView::Kind::Real, "ratio is a real", fails);
    check(inliers && inliers->kind == FieldView::Kind::Integer, "min-inliers is an integer", fails);
    check(cross && cross->kind == FieldView::Kind::Switch, "cross-check is a switch", fails);
    check(neighbors && neighbors->value == "48", "the preset's value is what is shown", fails);
    check(!rowOf(rows, "spv-path"), "an extract-only flag is not offered for auto", fails);

    // What the editor would send for each row, unedited, parses back unchanged.
    for (const FieldView& f : rows) {
        if (f.kind == FieldView::Kind::Text && f.value.empty()) continue;
        std::vector<std::string> args = {"sfm"};
        if (f.kind == FieldView::Kind::Switch) {
            args.push_back((f.value == "on" ? "--" : "--no-") + std::string(f.name));
        } else {
            args.push_back("--" + std::string(f.name));
            args.push_back(f.value);
        }
        std::vector<char*> argv;
        for (std::string& a : args) argv.push_back(a.data());
        SfmConfig back;
        std::set<std::string> seen;
        std::string error;
        int i = 1;
        const FieldResult r = setConfigField(back, CMD_AUTO, argv[1], (int)argv.size(), argv.data(),
                                             i, seen, error);
        check(r == FieldResult::Ok, std::string(f.name) + " parses: " + error, fails);
        const std::vector<FieldView> parsed = describeConfigFields(back, CMD_AUTO);
        const FieldView* again = rowOf(parsed, f.name);
        check(again && again->value == f.value, std::string(f.name) + " round-trips: " + f.value +
                                                    " -> " + (again ? again->value : "?"), fails);
    }

    std::printf("%s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, cmdOptionsTableTest); }

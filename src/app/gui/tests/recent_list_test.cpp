// recent_list_test -- the home screen's recent list: order, the per-kind
// cap, gui.conf round trips (and the two keys older builds wrote), and the
// probe that drops what is gone without undoing an add made meanwhile.

#include "app/gui/RecentList.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using gui::ModelContent;
using gui::RecentItem;
using gui::RecentKind;
using gui::RecentList;

namespace {

int g_failed = 0;

void check(bool ok, const std::string& what) {
    std::printf("%-4s %s\n", ok ? "ok" : "FAIL", what.c_str());
    if (!ok) g_failed++;
}

std::string norm(const std::string& p) {
    return fs::u8path(p).lexically_normal().make_preferred().u8string();
}

// What a save and a load in a fresh session would leave.
RecentList round_trip(const RecentList& in) {
    std::FILE* f = std::tmpfile();
    in.write_settings(f);
    std::rewind(f);
    RecentList out;
    char line[2048];
    while (std::fgets(line, sizeof line, f)) {
        std::string s = line;
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        const size_t eq = s.find('=');
        if (eq != std::string::npos) out.read_setting(s.substr(0, eq), s.substr(eq + 1));
    }
    std::fclose(f);
    return out;
}

bool same_items(const RecentList& a, const RecentList& b) {
    if (a.items().size() != b.items().size()) return false;
    for (size_t i = 0; i < a.items().size(); i++) {
        const RecentItem &x = a.items()[i], &y = b.items()[i];
        if (x.kind != y.kind || x.path != y.path || x.time != y.time) return false;
    }
    return true;
}

void wait_for_probe(RecentList& r, bool& removed) {
    removed = false;
    for (int i = 0; i < 500 && r.probing(); i++) {
        removed = r.poll_probe() || removed;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

int g_classified = 0;
ModelContent classify_as_mesh(const std::string&) {
    g_classified++;
    return ModelContent::Mesh;
}

void test_order_and_cap() {
    RecentList r;
    r.add(RecentKind::Dataset, "/data/a", 100);
    r.add(RecentKind::Model, "/data/a/splat.ply", 101);
    r.add(RecentKind::Dataset, "/data/b", 102);
    r.add(RecentKind::Dataset, "/data/a/", 103);
    check(r.items().size() == 3, "a folder with and without its trailing separator is one entry");
    check(r.items()[0].path == norm("/data/a") && r.items()[0].time == 103,
          "adding again moves it to the front with the new time");
    check(r.paths(RecentKind::Dataset).size() == 2 && r.paths(RecentKind::Model).size() == 1,
          "the same path under two kinds is two entries");

    for (int i = 0; i < 30; i++) r.add(RecentKind::Run, "/runs/" + std::to_string(i), 200 + i);
    check(r.paths(RecentKind::Run).size() == RecentList::kPerKind, "each kind keeps kPerKind");
    check(r.paths(RecentKind::Run).back() == norm("/runs/10"), "... and drops its oldest");
    check(r.paths(RecentKind::Dataset).size() == 2, "... without touching the other kinds");

    r.remove(RecentKind::Dataset, "/data/b/");
    check(r.paths(RecentKind::Dataset).size() == 1, "remove matches the normalized path");
    r.clear(RecentKind::Run);
    check(r.paths(RecentKind::Run).empty() && r.items().size() == 2, "clear(kind) leaves the rest");
}

void test_settings() {
    RecentList r;
    r.add(RecentKind::Dataset, "/data/with space", 1700000000);
    r.add(RecentKind::Reconstruction, "/data/ws", 1700000100);
    r.add(RecentKind::Project, "/data/ws/render.json", 1700000200);
    const RecentList back = round_trip(r);
    check(same_items(r, back), "write and read give back the same list, spaces in paths included");

    RecentList legacy;
    legacy.read_setting("recent.model", "1700000300 /m/new.ply");
    legacy.read_setting("recent", "/old/dataset");
    legacy.read_setting("recent_model", "/old/model.ply");
    legacy.read_setting("recent.dataset", "1700000400 /d/newer");
    legacy.read_setting("recent.someday_kind", "1 /x");
    check(legacy.items().size() == 4, "the older keys read, and an unknown kind is skipped");
    check(legacy.items()[0].path == norm("/d/newer") && legacy.items()[1].path == norm("/m/new.ply"),
          "entries with a time sort newest first");
    check(legacy.items()[2].path == norm("/old/dataset") && legacy.items()[2].time == 0 &&
              legacy.items()[3].kind == RecentKind::Model,
          "the untimed ones follow, in file order");
    check(!legacy.read_setting("colmap_exe", "colmap"), "other settings are not ours");

    RecentList odd;
    odd.read_setting("recent.dataset", "/no/time");
    check(odd.items().size() == 1 && odd.items()[0].path == norm("/no/time"),
          "a hand-written line without a time is still a path");
}

void test_probe(const fs::path& dir) {
    fs::create_directories(dir);
    const fs::path here = dir / "here.ply";
    std::ofstream(here) << "ply\n";
    const fs::path gone = dir / "gone";
    const fs::path back = dir / "back";

    RecentList r;
    r.add(RecentKind::Model, here.u8string(), 10);
    r.add(RecentKind::Dataset, gone.u8string(), 11);
    r.add(RecentKind::Dataset, dir.u8string(), 12);
    g_classified = 0;
    bool removed = false;
    r.start_probe(classify_as_mesh);
    wait_for_probe(r, removed);
    check(!r.probing(), "the probe finishes");
    check(removed && r.items().size() == 2, "a path that is gone is dropped");
    check(r.items()[1].content == ModelContent::Mesh && g_classified == 1,
          "a model is classified, and only a model");

    r.start_probe(classify_as_mesh);
    wait_for_probe(r, removed);
    check(!removed && g_classified == 1, "a model already classified is not read again");

    r.add(RecentKind::Run, back.u8string(), 13);
    r.start_probe(classify_as_mesh);
    r.add(RecentKind::Run, back.u8string(), 14);   // re-added while the probe runs
    wait_for_probe(r, removed);
    check(r.paths(RecentKind::Run).size() == 1, "an entry added again during a probe survives it");
    fs::remove_all(dir);
}

}  // namespace

int main() {
    test_order_and_cap();
    test_settings();
    test_probe(fs::temp_directory_path() / "spirula_recent_list_test");
    std::printf("%s\n", g_failed ? "FAILED" : "all passed");
    return g_failed ? 1 : 0;
}

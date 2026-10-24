#include "app/gui/MatchMatrixEditor.h"
#include "app/gui/MatchMatrix.h"
#include "app/gui/PairPreview.h"
#include "app/gui/SfmProgress.h"
#include "sfm/core/Matches.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void expect(bool ok, const std::string& what) {
    std::printf("%s  %s\n", ok ? "ok  " : "BAD ", what.c_str());
    if (!ok) g_failures++;
}

sfm::MatchesDatabase make_test_database() {
    sfm::MatchesDatabase db;
    for (int i = 0; i < 10; i++) {
        sfm::ImageEntry e;
        e.name = "frame_" + std::to_string(i);
        e.num_features = 500;
        db.images.push_back(e);
    }
    // Sequential pairs along diagonal
    for (uint32_t i = 0; i < 9; i++) {
        sfm::TwoViewMatches tvm;
        tvm.image1 = i;
        tvm.image2 = i + 1;
        tvm.config = 1;
        for (uint32_t k = 0; k < 100; k++) tvm.matches.push_back({k, k});
        db.pairs.push_back(std::move(tvm));
    }
    // Corner loop closure (valid: frame 0 <-> frame 9)
    {
        sfm::TwoViewMatches tvm;
        tvm.image1 = 0;
        tvm.image2 = 9;
        tvm.config = 1;
        for (uint32_t k = 0; k < 60; k++) tvm.matches.push_back({k, k});
        db.pairs.push_back(std::move(tvm));
    }
    // False loop closure in the middle (opposite sides of car: frame 2 <-> frame 7)
    {
        sfm::TwoViewMatches tvm;
        tvm.image1 = 2;
        tvm.image2 = 7;
        tvm.config = 1;
        for (uint32_t k = 0; k < 80; k++) tvm.matches.push_back({k, k});
        db.pairs.push_back(std::move(tvm));
    }
    return db;
}

}  // namespace

int main() {
    // 1. MatrixSelection logic
    gui::MatrixSelection sel;
    expect(!sel.active, "selection initially inactive");
    sel.r0 = 2; sel.r1 = 4;
    sel.c0 = 6; sel.c1 = 8;
    sel.active = true;
    sel.symmetric = true;

    expect(sel.contains(2, 7), "selection contains cell in box");
    expect(sel.contains(7, 2), "symmetric selection contains transposed cell");
    expect(!sel.contains(0, 9), "selection excludes corner loop closure");
    expect(!sel.contains(1, 2), "selection excludes sequential match");

    sel.symmetric = false;
    expect(!sel.contains(7, 2), "non-symmetric selection excludes transposed cell");

    // 2. Removal and undo round-trip on matches.bin
    const fs::path tmp_dir = fs::temp_directory_path() / "spirula_matrix_editor_test";
    fs::create_directories(tmp_dir);
    const fs::path matches_path = tmp_dir / "matches.bin";

    sfm::MatchesDatabase db = make_test_database();
    expect(db.pairs.size() == 11, "database initially has 11 pairs");
    sfm::writeMatches(matches_path.string(), db);

    gui::PairMatrix pm;
    int64_t mtime = 0;
    expect(gui::read_pair_matrix_from_matches(matches_path.string(), mtime, pm),
           "read pair matrix from generated matches.bin");
    expect(pm.at(2, 7) == 80, "pair (2, 7) has 80 matches in matrix");
    expect(pm.at(0, 9) == 60, "pair (0, 9) has 60 matches in matrix");

    gui::MatchMatrix mm;
    mm.set(pm);
    gui::PairPreview preview;
    gui::MatchMatrixEditor editor;
    editor.configure(tmp_dir.string(), "", "", matches_path.string());

    // Select false match (2, 7)
    sel.r0 = 2; sel.r1 = 3;
    sel.c0 = 7; sel.c1 = 8;
    sel.symmetric = true;
    sel.active = true;
    editor.set_selection(sel);

    // Remove false match
    bool removed = editor.remove_selected_matches(pm, mm, preview);
    expect(removed, "remove_selected_matches succeeds");
    expect(editor.can_undo(), "undo is available after removal");

    // Verify disk state
    sfm::MatchesDatabase db_after = sfm::readMatches(matches_path.string());
    expect(db_after.pairs.size() == 10, "database now has 10 pairs");
    bool found_false = false, found_corner = false;
    for (const auto& p : db_after.pairs) {
        if ((p.image1 == 2 && p.image2 == 7) || (p.image1 == 7 && p.image2 == 2)) found_false = true;
        if ((p.image1 == 0 && p.image2 == 9) || (p.image1 == 9 && p.image2 == 0)) found_corner = true;
    }
    expect(!found_false, "false loop closure pair (2, 7) was removed");
    expect(found_corner, "valid corner loop closure pair (0, 9) was preserved");
    expect(pm.at(2, 7) == 0, "matrix count for removed pair is now 0");
    expect(pm.at(0, 9) == 60, "matrix count for corner loop closure is still 60");

    // Verify backup created
    expect(fs::exists(tmp_dir / "matches.bin.bak"), "backup matches.bin.bak created");

    // Undo removal
    bool undone = editor.undo_removal(pm, mm, preview);
    expect(undone, "undo succeeds");
    expect(!editor.can_undo(), "undo stack is now empty");

    sfm::MatchesDatabase db_restored = sfm::readMatches(matches_path.string());
    expect(db_restored.pairs.size() == 11, "database restored back to 11 pairs");
    bool found_restored = false;
    for (const auto& p : db_restored.pairs) {
        if ((p.image1 == 2 && p.image2 == 7) || (p.image1 == 7 && p.image2 == 2)) found_restored = true;
    }
    expect(found_restored, "pair (2, 7) restored on disk");
    expect(pm.at(2, 7) == 80, "matrix count for pair (2, 7) restored to 80");

    // 3. PairPreview overlay toggles
    preview.set_show_keypoints(false);
    expect(!preview.show_keypoints(), "keypoints toggle sets false");
    preview.set_show_keypoints(true);
    expect(preview.show_keypoints(), "keypoints toggle sets true");

    preview.set_show_matches(false);
    expect(!preview.show_matches(), "matches toggle sets false");
    preview.set_show_matches(true);
    expect(preview.show_matches(), "matches toggle sets true");

    std::error_code ec;
    fs::remove_all(tmp_dir, ec);

    std::printf("\n%s\n", g_failures == 0 ? "all passed" : "TESTS FAILED");
    return g_failures == 0 ? 0 : 1;
}


// MatchMatrixEditor's file edits without a window: which pairs a selection of
// cells removes (one image per cell, and blocks of images past
// kEditorMatrixBins), the backup, and undo restoring the file byte for byte.

#include "app/gui/MatchMatrixEditor.h"
#include "app/gui/SfmProgress.h"
#include "sfm/core/Matches.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void expect(bool ok, const std::string& what) {
    std::printf("%s  %s\n", ok ? "ok  " : "BAD ", what.c_str());
    if (!ok) g_failures++;
}

std::string slurp(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

void add_pair(sfm::MatchesDatabase& db, uint32_t a, uint32_t b, uint32_t n) {
    sfm::TwoViewMatches p;
    p.image1 = a;
    p.image2 = b;
    p.config = 2;
    std::vector<sfm::FeatureMatch> m;
    for (uint32_t k = 0; k < n; k++) m.push_back({k, k + 1});
    p.matches = sfm::MatchList(std::move(m));
    db.pairs.push_back(std::move(p));
}

sfm::MatchesDatabase chain(uint32_t images) {
    sfm::MatchesDatabase db;
    for (uint32_t i = 0; i < images; i++) db.images.push_back({"frame_" + std::to_string(i), 500});
    for (uint32_t i = 0; i + 1 < images; i++) add_pair(db, i, i + 1, 100);
    return db;
}

bool has_pair(const sfm::MatchesDatabase& db, uint32_t a, uint32_t b) {
    for (const sfm::TwoViewMatches& p : db.pairs)
        if ((p.image1 == a && p.image2 == b) || (p.image1 == b && p.image2 == a)) return true;
    return false;
}

gui::MatrixSelection cells(uint32_t r, uint32_t c) {
    gui::MatrixSelection s;
    s.active = true;
    s.r0 = r;
    s.r1 = r + 1;
    s.c0 = c;
    s.c1 = c + 1;
    return s;
}

// Another run writing the file: a later mtime than the editor's own write.
void rewrite(const fs::path& p, const sfm::MatchesDatabase& db) {
    sfm::writeMatches(p.string(), db);
    fs::last_write_time(p, fs::file_time_type::clock::now() + std::chrono::seconds(5));
}

}  // namespace

int main() {
    const fs::path dir = fs::temp_directory_path() / "spirula_match_matrix_editor_test";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);
    const fs::path path = dir / "matches.bin";
    const fs::path bak = dir / "matches.bin.bak";

    {
        gui::MatrixSelection s = cells(2, 7);
        expect(s.contains(2, 7) && s.contains(7, 2), "a pair is selected in either order");
        expect(!s.contains(0, 9) && !s.contains(1, 2), "other cells are not");
    }

    // One image per cell: the false closure 2-7 goes, the true one 0-9 stays.
    {
        sfm::MatchesDatabase db = chain(10);
        add_pair(db, 0, 9, 60);
        add_pair(db, 7, 2, 80);   // stored in the order pairing chose
        sfm::writeMatches(path.string(), db);
        const std::string original = slurp(path);

        gui::MatchMatrixEditor ed;
        ed.configure(dir.string(), "", "", path.string());
        expect(ed.matrix().bins == 10 && ed.matrix().at(2, 7) == 80, "matrix read at one image per cell");
        ed.select(cells(2, 7));
        expect(ed.remove_selected(), "remove (2, 7)");
        const sfm::MatchesDatabase after = sfm::readMatches(path.string());
        expect(!has_pair(after, 2, 7) && has_pair(after, 0, 9) && after.pairs.size() == 10,
               "only the false closure is gone");
        expect(ed.matrix().at(2, 7) == 0 && ed.matrix().at(0, 9) == 60, "matrix follows the file");
        expect(slurp(bak) == original, "backup is the file before the edit");

        ed.select(cells(0, 1));
        expect(ed.remove_selected(), "remove (0, 1)");
        expect(slurp(bak) == original, "backup is not taken again over an edit");
        expect(ed.undo() && ed.undo() && !ed.can_undo(), "two undos");
        expect(slurp(path) == original, "undo restores the file byte for byte");
        expect(!fs::exists(dir / "matches.bin.tmp"), "no temporary left behind");

        // A run that matched again: the backup is retaken, the history dropped.
        ed.select(cells(0, 1));
        expect(ed.remove_selected() && ed.can_undo(), "remove before a re-run");
        sfm::MatchesDatabase rerun = chain(10);
        add_pair(rerun, 3, 8, 40);
        rewrite(path, rerun);
        expect(!ed.undo() && !ed.can_undo(), "undo refused once another run rewrote the file");
        ed.select(cells(3, 8));
        expect(ed.remove_selected(), "remove from the re-run's file");
        expect(has_pair(sfm::readMatches(bak.string()), 3, 8), "backup retaken from the re-run's file");
    }

    {
        const std::string original = slurp(bak);
        gui::MatchMatrixEditor ed;
        ed.configure(dir.string(), "", "", path.string());
        expect(!ed.can_undo() && ed.can_restore_backup(), "backup available after reopening without undo history");
        expect(ed.restore_backup() && slurp(path) == original, "restore backup byte for byte after reopening");
        expect(slurp(bak) == original, "restore preserves the backup");
        expect(has_pair(sfm::readMatches(path.string()), 3, 8) && ed.matrix().at(3, 8) == 40,
               "restored pairs appear in the file and matrix");
        ed.select(cells(3, 8));
        expect(ed.remove_selected() && ed.can_undo(), "edit after restoring");
        expect(ed.restore_backup() && !ed.can_undo(), "restore clears undo history");

        const std::string before = slurp(path);
        sfm::MatchesDatabase other = chain(10);
        other.images[0].name = "other";
        sfm::writeMatches(bak.string(), other);
        expect(!ed.restore_backup() && slurp(path) == before, "unrelated backup leaves matches untouched");
        std::ofstream(bak, std::ios::binary | std::ios::trunc) << "bad";
        expect(!ed.restore_backup() && slurp(path) == before, "corrupt backup leaves matches untouched");
        fs::remove(bak, ec);
        expect(!ed.can_restore_backup() && !ed.restore_backup() && slurp(path) == before,
               "missing backup leaves matches untouched");
        expect(!fs::exists(dir / "matches.bin.tmp"), "failed restores leave no temporary file");
    }

    // More images than cells: a cell is a block, and the test is on CELLS.
    {
        const uint32_t n = 4100;
        sfm::MatchesDatabase db = chain(n);
        add_pair(db, 100, 3000, 50);   // false closure
        add_pair(db, 49, 1498, 70);    // image numbers equal to that closure's cells
        sfm::writeMatches(path.string(), db);
        fs::remove(bak, ec);

        gui::MatchMatrixEditor ed;
        ed.configure(dir.string(), "", "", path.string());
        const gui::PairMatrix& pm = ed.matrix();
        expect(pm.bins == gui::kEditorMatrixBins && pm.n_images == n, "binned past the cap");
        const uint32_t r = pm.cell_of(3000), c = pm.cell_of(100);
        expect(r == 1498 && c == 49, "cells of the closure");
        ed.select(cells(r, c));
        expect(ed.remove_selected(), "remove the closure's cell");
        const sfm::MatchesDatabase after = sfm::readMatches(path.string());
        expect(!has_pair(after, 100, 3000), "closure removed");
        expect(has_pair(after, 49, 1498), "pair numbered like the cell kept");
        expect(after.pairs.size() == db.pairs.size() - 1, "nothing else removed");
    }

    fs::remove_all(dir, ec);
    std::printf("\n%s\n", g_failures == 0 ? "all passed" : "TESTS FAILED");
    return g_failures == 0 ? 0 : 1;
}

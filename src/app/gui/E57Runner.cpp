// E57Runner.cpp -- see E57Runner.h.

#include "app/gui/E57Runner.h"

#include "app/AppPaths.h"
#include "app/gui/SourceList.h"
#include "app/gui/Subprocess.h"
#include "i18n/Locale.h"
#include "i18n/catalog/Data.h"
#include "i18n/catalog/E57.h"
#include "i18n/catalog/Log.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
namespace E = spirula::i18n::msg::e57;
namespace lmsg = spirula::i18n::msg::log;
using spirula::i18n::format;

namespace gui {

namespace {

int64_t number(const std::string& s) { return std::atoll(s.c_str()); }

std::set<std::string> files_under(const fs::path& dir) {
    std::set<std::string> out;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
        if (it->is_regular_file(ec)) out.insert(it->path().string());
    return out;
}

}  // namespace

E57Runner::~E57Runner() {
    _cancel = true;
    if (_worker.joinable()) _worker.join();
}

void E57Runner::start(const E57Job& job, RunFilms films) {
    if (busy()) return;
    if (_worker.joinable()) _worker.join();
    {
        std::lock_guard<std::mutex> lk(_mu);
        _error.clear();
        _output = job.output;
        _mask_dir.clear();
        _check_warning.clear();
        _log.clear();
    }
    _prog.reset();
    _films = films;
    _taken = {Stage::Frames, Stage::Mapping};
    if (job.depth_maps) _taken.push_back(Stage::Geometry);
    if (job.all_points || job.seed_points > 0) _taken.push_back(Stage::Finishing);
    if (job.masks) _taken.push_back(Stage::Masks);
    _converted = false;
    _cancel = false;
    _state = State::Running;
    _worker = std::thread([this, job] { run(job); });
}

void E57Runner::cancel() { _cancel = true; }

void E57Runner::reset() {
    if (busy()) return;
    if (_worker.joinable()) _worker.join();
    _prog.reset();
    _taken.clear();
    _converted = false;
    std::lock_guard<std::mutex> lk(_mu);
    _error.clear();
    _output.clear();
    _mask_dir.clear();
    _check_warning.clear();
    _state = State::Idle;
}

std::string E57Runner::error() {
    std::lock_guard<std::mutex> lk(_mu);
    return _error;
}
std::string E57Runner::output() {
    std::lock_guard<std::mutex> lk(_mu);
    return _output;
}
std::string E57Runner::mask_dir() {
    std::lock_guard<std::mutex> lk(_mu);
    return _mask_dir;
}
std::string E57Runner::check_warning() {
    std::lock_guard<std::mutex> lk(_mu);
    return _check_warning;
}

void E57Runner::log(const std::string& line) {
    std::lock_guard<std::mutex> lk(_mu);
    _log.push_back(line);
}

std::vector<std::string> E57Runner::drain_log() {
    std::lock_guard<std::mutex> lk(_mu);
    std::vector<std::string> out;
    out.swap(_log);
    return out;
}

// The child's line as its step's: the first one enters it, and a count moves
// its bar. Reading the scans and checking the photos against them share one.
void E57Runner::step(Stage s, const std::string& line, int64_t done, int64_t total) {
    if (_prog.current() != s || _prog.stage(s).status != StageStatus::Running)
        _prog.enter(s, line);
    else
        _prog.detail(s, line);
    if (total > 0) _prog.count(s, done, total);
    else _prog.fraction(s, -1.0f);
}

void E57Runner::note_line(const std::string& line) {
    std::vector<std::string> got;
    auto is = [&](const spirula::i18n::Msg& m) { return spirula::i18n::scan(m, line, got); };
    const bool warning = is(E::check_unsure) || is(E::check_fixed_pinhole) ||
                         is(E::check_fixed_panorama);
    // Images and scans are counted as they start, depth maps once written.
    if (is(E::progress_images) && got.size() == 2)
        step(Stage::Frames, line, number(got[0]) - 1, number(got[1]));
    else if (is(E::progress_points) && got.size() == 2)
        step(Stage::Mapping, line, number(got[0]) - 1, number(got[1]));
    else if (warning || is(E::check_running) || is(E::check_ok) || is(E::check_no_color))
        step(Stage::Mapping, line, 0, 0);
    else if (is(E::progress_depth) && got.size() == 2)
        step(Stage::Geometry, line, number(got[0]), number(got[1]));
    else if (is(E::thinning) || is(E::seed_done) || is(E::seed_all))
        step(Stage::Finishing, line, 0, 0);
    std::lock_guard<std::mutex> lk(_mu);
    if (warning) _check_warning = line;
    // The child's own "error: <what>" is the reason worth showing.
    const std::string word = spirula::i18n::msg::data::word_error.get();
    if (line.rfind(word, 0) == 0) _error = line.substr(std::min(line.size(), word.size() + 1));
}

void E57Runner::run(E57Job job) {
    std::vector<std::string> argv = {
        app::exe_path(), "--lang", spirula::i18n::code(spirula::i18n::current()),
        "e57", job.input, job.output,
        "--points", job.all_points ? std::string("all") : std::to_string(job.seed_points),
        // The screen has already said the folder is not empty.
        "--overwrite",
    };
    if (!job.pinhole) argv.push_back("--no-pinhole");
    if (!job.spherical) argv.push_back("--no-panoramas");
    if (!job.depth_maps) argv.push_back("--no-depth");

    std::string cmd;
    for (const auto& a : argv) cmd += (cmd.empty() ? "$ " : " ") + a;
    log(cmd);

    // An earlier run's files in the folder are only shown once rewritten. On a
    // thread of their own: a scanner's photo is tens of megapixels, and reading
    // one for the reel must not hold up the lines the steps are drawn from.
    const fs::path root(job.output);
    OutputWatch images(_films.frames, root / "images", {}, {}, /*fresh_only=*/true);
    OutputWatch maps(job.depth_maps ? _films.geometry : nullptr, root / "normals",
                     root / "images", root / "depths", /*fresh_only=*/true);
    std::atomic<bool> exited{false};
    std::thread watch([&] {
        while (!exited.load()) {
            images.poll();
            maps.poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    });
    const int rc = run_process(argv, "", [this](const std::string& l) {
        log(l);
        note_line(l);
    }, _cancel);
    exited = true;
    watch.join();
    images.poll(/*flush=*/true);
    maps.poll(/*flush=*/true);
    _prog.finish(rc == 0 ? StageStatus::Done
                         : _cancel.load() ? StageStatus::Skipped : StageStatus::Failed);

    _converted = rc == 0;
    if (rc == 0 && job.masks && !_cancel.load()) {
        // The dataset screen's masking over the images just written, which it
        // reads where they are.
        PrepJob prep = job.prep;
        PrepInput in = make_source(job.output, /*use_found_masks=*/true);
        in.stencil = job.stencil;
        for (MaskClick& c : prep.mask_clicks)
            if (c.source == job.frames) c.source = in.path;
        prep.inputs = {in};
        prep.workspace = job.output;
        prep.photo_import = PhotoImport::InPlace;
        prep.resume = true;
        PrepResult result;
        std::string err;
        const std::set<std::string> scanner_masks = files_under(root / "masks");
        DatasetPrep dp(&_prog, RunFilms{nullptr, _films.masks, nullptr}, _cancel);
        const bool ok = dp.run(prep, result, err);
        _prog.finish(ok ? StageStatus::Done
                        : _cancel.load() ? StageStatus::Skipped : StageStatus::Failed);
        if (!ok) {
            // Unmasked, as the screen then says, rather than masked in part:
            // the trainer finds masks/ on its own, and a re-run starts it over.
            std::error_code ec;
            for (const std::string& f : files_under(root / "masks"))
                if (!scanner_masks.count(f)) fs::remove(f, ec);
        }
        std::lock_guard<std::mutex> lk(_mu);
        if (!ok) {
            _error = err;
            _state = _cancel.load() ? State::Cancelled : State::Failed;
            return;
        }
        _mask_dir = result.mask_dir_cfg;
    }

    std::lock_guard<std::mutex> lk(_mu);
    if (rc == kCancelled || _cancel.load()) {
        _error = lmsg::err_cancelled.get();
        _state = State::Cancelled;
    } else if (rc == kSpawnFailed) {
        _error = format(lmsg::err_spawn_recon, {argv[0]});
        _state = State::Failed;
    } else if (rc != 0) {
        if (_error.empty()) _error = E::failed.get();
        _state = State::Failed;
    } else {
        _state = State::Done;
    }
}

}  // namespace gui

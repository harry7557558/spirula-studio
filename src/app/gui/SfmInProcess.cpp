// SfmInProcess.cpp -- see SfmInProcess.h.

#include "app/gui/SfmInProcess.h"

#ifdef SS_TOOL_SFM
#include "sfm/Pipeline.h"
#include "sfm/core/Cancel.h"
#include "sfm/core/Events.h"
#include "sfm/core/Log.h"
#include "sfm/core/Progress.h"

#include "i18n/catalog/Sfm.h"
#else
#include "i18n/catalog/Log.h"
#endif

namespace gui {

#ifndef SS_TOOL_SFM

// SfmRunner::availability() refuses a run long before this, so reaching it is
// a bug rather than a configuration; it still says which.
InProcessResult run_sfm_in_process(
    const std::vector<std::string>& args,
    const std::function<void(const std::string&)>& log,
    const std::function<void(const RunStatus&)>& on_status,
    const std::atomic<bool>& cancel) {
    (void)args; (void)log; (void)on_status; (void)cancel;
    InProcessResult out;
    out.exit_code = -1;
    out.error = spirula::i18n::msg::log::err_no_sfm_module.get();
    return out;
}

#else

namespace {

// The line the CLI would have printed, so the GUI's log reads the same
// whichever transport ran the job.
//
// The tag column is cached because a sink runs with slog's own lock held and
// slog::prefix() takes it again, which deadlocks (sfm/core/Log.h). The
// language cannot change under a run.
struct LinePrinter {
    std::string tag[7];
    LinePrinter() {
        for (int i = 0; i < 7; i++) tag[i] = sfm::slog::prefix((sfm::slog::Tag)i);
    }
    std::string render(sfm::slog::Tag t, sfm::slog::Level lv,
                       const std::string& text) const {
        namespace M = spirula::i18n::msg::sfm;
        if (lv == sfm::slog::Level::Diag) return text;
        const char* word = lv == sfm::slog::Level::Warning  ? M::word_warning.get()
                           : lv == sfm::slog::Level::Error  ? M::word_error.get()
                                                            : nullptr;
        std::string out = tag[(int)t];
        if (word) { out += word; out += ' '; }
        return out + text;
    }
};

// The same fields status.bin carries, kept from the event stream so the screen
// cannot tell the two apart.
struct StatusFold {
    RunStatus st;
    // False when the event moved nothing a screen shows, which is every pair
    // matching verifies -- and there are hundreds of thousands of those.
    bool apply(const sfm::Event& e) {
        using K = sfm::Event::Kind;
        if (e.kind == K::PairVerified) return false;
        st.stage = (uint32_t)e.stage;
        switch (e.kind) {
            case K::StageBegin: st.done = 0; st.total = e.total; break;
            case K::StageEnd: st.done = st.total; break;
            case K::Progress:
            case K::ImageExtracted: st.done = e.done; st.total = e.total; break;
            case K::ModelUpdated:
                // Not done/total: mapping's bar is events::map_placed, which
                // counts the capture rather than the attempt in hand.
                st.registered = e.registered;
                st.images = e.images;
                st.points = e.points;
                break;
            case K::Result:
                st.finished = true;
                st.partial = e.partial;
                st.metric = e.metric;
                st.registered = e.registered;
                st.images = e.images;
                st.points = e.points;
                st.models = e.models;
                st.mean_reproj = e.mean_reproj;
                break;
            default: break;
        }
        return true;
    }
};

}  // namespace

InProcessResult run_sfm_in_process(
    const std::vector<std::string>& args,
    const std::function<void(const std::string&)>& log,
    const std::function<void(const RunStatus&)>& on_status,
    const std::atomic<bool>& cancel) {
    InProcessResult out;
    sfm::AutoRequest req;
    if (std::string err = sfm::parse_auto_args(args, req); !err.empty()) {
        out.exit_code = -1;
        out.error = err;
        return out;
    }
    if (req.wants_help) {
        out.exit_code = -1;
        out.error = "--help is not a run";
        return out;
    }

    StatusFold fold;
    // Restored when this returns, however it returns, so a second run starts
    // from a clean process (sfm/Pipeline.h).
    sfm::RunContext ctx;
    ctx.set_progress_dir(req.progress_dir);
    ctx.set_cancel(&cancel);
    const LinePrinter printer;
    if (log)
        ctx.set_log([&](sfm::slog::Tag t, sfm::slog::Level lv, const std::string& s) {
            log(printer.render(t, lv, s));
        });
    ctx.set_events([&](const sfm::Event& e) {
        // The child writes the same snapshot from the same stream; this is
        // that writer, for a front end reading it live instead.
        sfm::progress::status(e);
        if (fold.apply(e) && on_status) on_status(fold.st);
    });

    try {
        const sfm::AutoResult r = sfm::run_auto(req.cfg, req.in);
        out.exit_code = r.exit_code;
    } catch (const sfm::Cancelled&) {
        out.cancelled = true;
        out.exit_code = 2;
    } catch (const std::exception& e) {
        out.exit_code = 2;
        out.error = e.what();
    }
    return out;
}

#endif  // SS_TOOL_SFM

}  // namespace gui

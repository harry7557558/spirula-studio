#pragma once

// CompareView -- up to four finished models under one camera, laid out in one
// pane, two, three or a 2x2 grid. The viewer screen and the meshing preview
// are both this. Design: docs/notes/compare-view.md.
//
// It owns the engine while it is open (engine_reset at both ends), so opening
// it is a session-destroying action for GuiApp and nothing else may render
// from the engine meanwhile. GUI thread only.

#include "app/gui/SplatViewer.h"
#include "app/gui/ViewportPanel.h"
#include "app/gui/edit/EditSession.h"
#include "app/gui/edit/MeshDoc.h"
#include "app/gui/render/RenderSession.h"
#include "i18n/Message.h"

#include <atomic>
#include <thread>

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace gui {

class CompareView {
public:
    // Four is where a pane stops being big enough to judge a render in.
    static constexpr int kMaxModels = 4;

    ~CompareView();

    // Take the engine over and show `path` alone.
    void open(const std::string& path);
    // Beside what is already open; ignored when full. `title` names the pane
    // when the model has a role rather than just a filename (the meshing
    // screen's two sides); otherwise the file's own name is used.
    void add(const std::string& path, const spirula::i18n::Msg* title = nullptr);
    // Hand the engine back. Safe when nothing is open.
    void close();

    // Whether the engine is this object's: true from the first add() until
    // close(), which is what the owner keys the viewport handover on -- the
    // last model being closed does not hand the engine back.
    bool holds_engine() const { return _engine_taken; }
    int count() const { return (int)_models.size(); }
    // Which pane holds `path`, or -1. What a screen that offers a list of
    // models to show needs, since the panes are the list.
    int index_of(const std::string& path) const;
    // Show or hide one, leaving the others alone. The removal is deferred to
    // the next poll(), like every other one.
    void set_shown(const std::string& path, bool on,
                   const spirula::i18n::Msg* title = nullptr);
    bool full() const { return count() >= kMaxModels; }
    // Recent paths offered by the "add" menu, refreshed by the owner each
    // frame -- this object has no settings of its own.
    void set_recents(const std::vector<std::string>& r) { _recents = r; }
    // Called when the user asks for a file picker (the owner owns the dialog).
    void set_pick_file(std::function<void()> f) { _pick_file = std::move(f); }
    // The other files written beside `path` by whatever produced it, so an
    // edit can reach them too. The owner knows; this does not.
    void set_siblings_of(
        std::function<std::vector<std::string>(const std::string&)> f) {
        _siblings_of = std::move(f);
    }
    // Told the file an edit was just saved to.
    void set_on_model_saved(std::function<void(const std::string&)> f) { _on_model_saved = std::move(f); }

    // ---- editing (docs/notes/gui-editing-plan.md) ----
    // Open the pane's model for editing, or give it back. One pane at a time:
    // the tools own that viewport's left button while they are there.
    void begin_edit(int index);
    // ... as soon as the first model's loader finishes, which is how a screen
    // hands a model it has only just asked for straight to the editor.
    void edit_first_when_ready() { _edit_when_ready = true; }
    // `reload` false when the panes are going away with the edit.
    void end_edit(bool reload = true);
    int editing() const { return _edit_index; }
    bool edit_busy() const { return _edit_loading.load(); }
    EditSession& edit() { return _edit; }
    // True while an edit has changes that have not been written out.
    bool edit_dirty() const;
    // Ask before those changes are thrown away; `then` runs on a yes. With
    // nothing unsaved it runs straight away.
    void confirm_discard_edits(std::function<void()> then);

    // ---- rendering a photo or a video (docs/notes/render-video.md) ----
    // Offered on the viewer screen, not on the meshing preview.
    void set_render_allowed(bool on) { _render_allowed = on; }
    // Render with this pane's model as the primary one; every other open
    // model is on offer as a source. The editor, when it is open on the same
    // pane, stays open behind it -- what it did shows in the render.
    void begin_render(int index);
    void render_first_when_ready() { _render_when_ready = true; }
    // The same, then with this camera project open in the render.
    void render_project_when_ready(const std::string& path) {
        _render_when_ready = true;
        _render_project = path;
    }
    void end_render(bool switching = false);
    int rendering() const { return _render_index; }
    render::RenderSession& render() { return _render; }
    // Which panel is beside the panes: the editor's, the render's, or none.
    enum class Sidebar { None, Edit, Render };
    Sidebar sidebar() const;
    // Playing back or exporting: the window keeps drawing.
    bool animating() const { return _render.animating(); }

    // Attach panels whose loaders have finished and refresh the placements.
    // Once a frame, before draw().
    void poll();
    // The row above the panes: what is open, the add button, the link.
    void draw_toolbar();
    // Panes showing splats through the engine, for screenshots
    // (defined in Screenshot.cpp).
    struct PaneShot {
        ViewportPanel* panel;
        std::string asked, file;
        int64_t splats;
    };
    std::vector<PaneShot> splat_panes();
    // The panes themselves, `height` tall (0 = all that is left).
    void draw(float height);
    void destroy_gl();
    std::vector<std::string> drain_log();

private:
    struct Model {
        // What was asked for, which is how a caller names a pane again. The
        // viewer's own path() is cleared when it closes.
        std::string path;
        SplatViewer src;
        ViewportPanel panel;
        const spirula::i18n::Msg* title = nullptr;
        int slot = -1;              // engine scene slot
        uint64_t load_id = 0;       // a new one each time the file is read
        uint64_t uid = 0;           // the pane's own, for as long as it is open
        bool attached = false;
        // Placement in the shared frame. `align` puts the model in the FIRST
        // model's frame rather than in its own; the rest is the hand
        // adjustment on top of that.
        bool align = true;
        float offset[3] = {0, 0, 0};
        float euler[3] = {0, 0, 0};   // degrees, applied X then Y then Z
        float scale = 1.0f;
    };

    void take_engine();
    // Read a pane's model from disk again, in place: what a save over the
    // file it was loaded from leaves it needing.
    void reload(int index);
    void remove(int index);
    void move(int index, int dir);
    int  claim_slot();
    void attach(Model& m);
    // Recompute every pane's model->shared similarity from the placements.
    void update_placements();
    // engine_blit_view refuses to run before engine_viewer_init, and the grid
    // it draws belongs to whichever model defines the shared frame.
    void ensure_viewer_overlay();
    // `beside` draws to the right of the pane's image, inside the pane.
    void draw_pane(int index, const ImVec2& size, const std::function<void()>& beside = {});
    void draw_placement_popup(int index);
    // The panel driving the link this frame: the one being dragged (sticky
    // for the length of a drag), else whichever moved.
    ViewportPanel* link_master();

    // One mutex for every panel's render worker: the engine binds one scene
    // at a time, so two panels rendering at once would race over which.
    // Declared before the models, which hold a pointer to it.
    std::mutex _engine_mutex;
    std::vector<std::unique_ptr<Model>> _models;
    bool _engine_taken = false;
    bool _link = true;
    // Whose frame the axes/grid overlay was built for; "" when there is none.
    std::string _overlay_key;
    uint32_t _slots_used = 0;
    uint64_t _loads = 0;
    // Pane-menu actions, applied by the next poll(): a pane cannot remove or
    // reorder itself while its own popup is being drawn inside it.
    int _pending_remove = -1;
    // Asked for by the render, by pane uid: one a frame, since each can move
    // the render to another pane.
    std::vector<uint64_t> _pending_remove_uids;
    int _pending_view = -1;
    bool _pending_view_edit = false;
    uint64_t _uids = 0;
    // The editor's placement copied onto the other panes (EditSession's
    // "move the others with it"), so that they go back when it ends.
    bool _synced = false;
    void sync_placements();
    spirula::Sim3 file_to_norm_of(Model& m);
    int _pending_move = 0;
    int _pending_move_index = -1;
    // Tallest control block across the panes on the last draw; the shorter
    // ones are padded to it so every image comes out the same size.
    float _controls_h = 0.0f;

    // Reading a model a second time is what editing costs: the viewer keeps
    // nothing host-side, and a hundred-megabyte PLY must not block a frame.
    void finish_edit_load();
    // Apply one mesh edit's deletions to every OTHER mesh pane.
    void show_sibling_meshes(int except, const FaceCut& cut);

    // The models as the render sees them, rebuilt once a frame.
    void feed_render();
    // The dataset a run's model was made from, read in the background and
    // moved into the model's own frame: its cameras and lenses are what the
    // render offers to start from. nullptr until it is ready, or when none.
    const ParsedDataset* run_dataset(const std::string& model_file);
    struct RunDataset {
        std::thread worker;
        std::atomic<bool> done{false};
        std::unique_ptr<ParsedDataset> ds;
        double up[3] = {0, 0, 1};
        ~RunDataset() { if (worker.joinable()) worker.join(); }
    };
    std::map<std::string, std::unique_ptr<RunDataset>> _run_datasets;

    EditSession _edit;
    int _edit_index = -1;
    render::RenderSession _render;
    int _render_index = -1;
    bool _render_allowed = false;
    bool _render_when_ready = false;
    std::string _render_project;
    // The render panel is the one showing, when both are open on a pane.
    bool _render_on_top = false;
    // Which edit revision the splats' survivors were copied at.
    uint64_t _alive_rev = 0;
    std::shared_ptr<const std::vector<uint8_t>> _alive;
    bool _edit_when_ready = false;
    std::function<void()> _discard_then;
    bool _ask_discard = false;
    std::thread _edit_worker;
    std::atomic<bool> _edit_loading{false};
    std::unique_ptr<EditDoc> _edit_pending;
    std::string _edit_error;

    std::vector<std::string> _recents;
    std::vector<std::string> _log;
    std::function<void()> _pick_file;
    std::function<std::vector<std::string>(const std::string&)> _siblings_of;
    std::function<void(const std::string&)> _on_model_saved;
};

}  // namespace gui

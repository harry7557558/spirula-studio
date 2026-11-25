#pragma once

#include "app/MeshPreviewBudget.h"
#include "app/gui/ViewportPanel.h"
#include "checkpoint/SplatPly.h"
#include <climits>
#include <future>

namespace gui {
class RunMemoryView;

class MeshMonitor {
public:
    void draw(const std::string& source, const std::string& progress_dir, const RunMemoryView* memory);
    void clear();

private:
    struct Source {
        spirula::SplatCloud cloud;
        int64_t upgrade_count = 0, upgrade_coeffs = 0;
        float center[3]{};
        float unit = 1;
    };
    struct Surface {
        meshing::MeshData mesh;
        int stage = 0;
    };
    static Source load_source(const std::string& path, spirula::MeshPreviewBudget budget, int limit,
                              const std::function<bool()>& cancelled);
    void poll();
    void attach_surface();

    ViewportPanel _view;
    Source _sample;
    Surface _surface;
    std::future<void> _worker;
    std::shared_ptr<Source> _next_source;
    std::shared_ptr<Surface> _next_surface;
    spirula::MeshPreviewBudget _budget{};
    std::string _source, _dir, _error;
    int _splat_limit = INT_MAX;
    double _budget_at = 0, _poll_at = 0;
    bool _requested = false, _show_surface = false, _refresh = false;
};
}  // namespace gui

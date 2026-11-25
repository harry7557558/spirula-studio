#include "app/gui/MeshMonitor.h"
#include "app/MeshPreviewFile.h"
#include "app/gui/RunMemoryView.h"
#include "mesh/MeshImport.h"
#include "backend/api/BackendRuntime.h"
#include "engine/Engine.h"
#include "i18n/catalog/MeshOptions.h"
#include "app/gui/SceneFrame.h"
#include "app/gui/Ui.h"
#include "checkpoint/SplatPly.h"
#include "i18n/catalog/Gui.h"

namespace gui {
namespace msg = spirula::i18n::msg::gui;

namespace {
spirula::MeshPreviewBudget preview_budget(const RunMemoryView* memory) {
    uint64_t free = HardwareSplatRenderer::free_device_bytes();
    if (!free) free = spirula::mesh_preview_device_free(backend::memory_usage());
    auto growth = [](const spirula::ResourceForecast& f) {
        return f.projected ? std::max(0.0, f.peak_mean + f.peak_sigma - f.ours) : 0.0;
    };
    return spirula::mesh_preview_budget(free, memory ? growth(memory->ram()) : 0,
                                             memory ? growth(memory->vram()) : 0);
}
}  // namespace

MeshMonitor::Source MeshMonitor::load_source(const std::string& path, spirula::MeshPreviewBudget budget, int limit,
                                              const std::function<bool()>& cancelled) {
    const auto ply = spirula::find_splat_ply(path).first;
    const auto shape = spirula::splat_ply_shape(ply);
    bool full_color = shape.sh_degree <= 4 && shape.num <= limit &&
                      budget.splat_limit(shape.num, shape.coeffs()) == shape.num;
    int count = std::min(limit, budget.splat_limit(shape.num, full_color ? shape.coeffs() : 0));
    Source source;
    if (limit == INT_MAX && (count < shape.num || (!full_color && shape.coeffs() && shape.sh_degree <= 4))) {
        source.upgrade_count = shape.num;
        source.upgrade_coeffs = shape.sh_degree <= 4 ? shape.coeffs() : 0;
    }
    for (;;) {
        try {
            source.cloud = spirula::read_splat_ply_sample(ply, count, full_color, cancelled);
            break;
        } catch (const std::bad_alloc&) {
            source.upgrade_count = 0;
            if (full_color) full_color = false;
            else if (count > 1) count /= 2;
            else throw;
        }
    }
    float radius;
    scene_extent(source.cloud.means, source.cloud.num, source.center, radius);
    source.unit = view_distance_for(radius);
    return source;
}

void MeshMonitor::clear() {
    if (_source.empty() && _dir.empty()) return;
    _source.clear();
    _dir.clear();
    _error.clear();
    _next_source.reset();
    _next_surface.reset();
    _sample = {};
    _surface = {};
    _requested = _show_surface = false;
    _refresh = true;
    _view.detach();
    _view.destroy_gl();
    _budget_at = _poll_at = 0;
    _splat_limit = INT_MAX;
}

void MeshMonitor::poll() {
    if (_worker.valid()) {
        if (_worker.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
        try {
            _worker.get();
            if (_next_source) {
                _sample = std::move(*_next_source);
                _refresh = true;
            }
            if (_next_surface && _next_surface->stage) {
                if (_surface.mesh.F.empty()) _show_surface = true;
                _surface = std::move(*_next_surface);
                _error.clear();
                _refresh |= _show_surface;
            }
        } catch (const std::exception& e) {
            if (_next_source || _next_surface) _error = e.what();
        }
        _next_source.reset();
        _next_surface.reset();
    }
    if (!_requested && !_source.empty()) {
        _requested = true;
        _next_source = std::make_shared<Source>();
        _worker = std::async(std::launch::async, [path = _source, budget = _budget, limit = _splat_limit,
                            target = std::weak_ptr<Source>(_next_source)] {
            Source source = load_source(path, budget, limit, [&] { return target.expired(); });
            if (auto dest = target.lock()) *dest = std::move(source);
        });
    } else if (!_dir.empty() && ImGui::GetTime() >= _poll_at) {
        _poll_at = ImGui::GetTime() + 1;
        _next_surface = std::make_shared<Surface>();
        _worker = std::async(std::launch::async, [dir = _dir, stage = _surface.stage, budget = _budget,
                            target = std::weak_ptr<Surface>(_next_surface)] {
            Surface surface;
            surface.stage = spirula::read_mesh_preview(dir, stage, surface.mesh, budget);
            if (surface.stage) meshing::mesh_compute_normals(surface.mesh);
            if (auto dest = target.lock()) *dest = std::move(surface);
        });
    }
}

void MeshMonitor::attach_surface() {
    float center[] = {_sample.center[0], _sample.center[1], _sample.center[2]};
    float unit = _sample.unit;
    if (!_sample.cloud.num) {
        std::vector<float> xyz;
        xyz.reserve(_surface.mesh.V.size() * 3);
        for (const auto& v : _surface.mesh.V) xyz.insert(xyz.end(), v.begin(), v.end());
        float radius;
        scene_extent(xyz, _surface.mesh.V.size(), center, radius);
        unit = view_distance_for(radius);
    }
    const float s = 1.0f / unit;
    const float transform[] = {s,0,0,-s*center[0], 0,s,0,-s*center[1], 0,0,s,-s*center[2]};
    _view.attach_preview_mesh(_surface.mesh, transform, _source);
}

void MeshMonitor::draw(const std::string& source, const std::string& dir, const RunMemoryView* memory) {
    if (_source != source) {
        clear();
        _source = source;
    }
    if (_dir != dir) {
        _dir = dir;
        _surface = {};
        _refresh |= _show_surface;
        _show_surface = false;
        _next_surface.reset();
        _budget_at = 0;
        _poll_at = 0;
    }
    if (source.empty() && dir.empty()) {
        ui::CenteredText(msg::mesh_no_model);
        return;
    }
    if (ImGui::GetTime() >= _budget_at) {
        _budget_at = ImGui::GetTime() + 1;
        _budget = preview_budget(memory);
        if (!_show_surface && !_next_source && _sample.upgrade_count &&
            _budget.splat_limit(_sample.upgrade_count, _sample.upgrade_coeffs) == _sample.upgrade_count) {
            _sample.upgrade_count = 0;
            _requested = false;
        }
    }
    const unsigned samples = !_show_surface && engine_stochastic_enabled() ? engine_stochastic_spp() : 0;
    const int pixels = _budget.render_pixels(_show_surface ? 8 : 32 + 12 * samples);
    _view.limit_preview_pixels(std::max(1, pixels));
    if (pixels < 64*64) {
        _next_source.reset();
        _next_surface.reset();
        _sample = {};
        _surface = {};
        if (_view.preview_active() || _view.hardware_active()) {
            _view.detach();
            _view.destroy_gl();
        }
        _requested = _show_surface = false;
        ui::CenteredText(spirula::i18n::msg::meshoption::preview_memory);
        return;
    }
    poll();
    if (!_surface.mesh.F.empty() && _sample.cloud.num > 0) {
        if (ui::RadioButton(msg::mesh_side_splats, !_show_surface) && _show_surface) {
            _show_surface = false;
            _refresh = true;
        }
        ImGui::SameLine();
        if (ui::RadioButton(msg::mesh_preview_surface, _show_surface) && !_show_surface) {
            _show_surface = true;
            _refresh = true;
        }
    } else ui::SeparatorText(_show_surface ? msg::mesh_preview_surface : msg::mesh_side_splats);
    if (_refresh) {
        if (_show_surface) {
            try {
                attach_surface();
            } catch (const std::bad_alloc&) {
                _view.detach();
                _view.destroy_gl();
                _surface.mesh = {};
                _show_surface = false;
                _error = spirula::i18n::msg::meshoption::preview_memory.get();
                return;
            }
        } else if (_sample.cloud.num > 0) {
            try {
                _view.attach_splat_preview(_sample.cloud, _source, _sample.center, _sample.unit);
            } catch (const SplatMemoryError& e) {
                if (_sample.cloud.num > 1) {
                    _splat_limit = int(_sample.cloud.num / 2);
                    _sample = {};
                    _requested = false;
                } else _error = e.what();
            } catch (const std::exception& e) { _error = e.what(); }
        }
        else _view.detach();
        _refresh = false;
    }
    if (_view.preview_active() || _view.hardware_active()) _view.draw(false);
    else if (!_error.empty()) ui::TextDisabledRaw(_error);
    else if (source.empty()) ui::TextDisabled(msg::mesh_no_model);
    else ui::TextDisabled(_worker.valid() ? msg::viewer_loading : msg::viewport_render_failed);
}

}  // namespace gui

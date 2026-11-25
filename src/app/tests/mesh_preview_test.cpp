#include "app/MeshPreviewFile.h"
#include "core/AtomicFile.h"

#include <cmath>
#include <cstdio>

namespace fs = std::filesystem;

int main() {
    const fs::path dir = spirula::reserve_file_directory(fs::temp_directory_path() / "mesh-preview-test");
    const spirula::MeshPreviewBudget ample{1ull << 30, 1ull << 30};
    const spirula::MeshPreviewBudget tight{40u << 20, 8u << 20};
    bool ok = ample.splat_limit(800000, 15) == 800000 && tight.splat_limit(800000, 0) < 800000;
    ok &= ample.render_pixels(32) > 1920*1080 && tight.render_pixels(32) < 960*960;
    for (int side : {4, 400}) {
        meshing::MeshData mesh, preview;
        for (int y = 0; y < side; ++y) {
            for (int x = 0; x < side; ++x) {
                const int i = int(mesh.V.size());
                mesh.V.push_back({float(x), float(y), 0.f});
                if (x && y) {
                    mesh.F.push_back({i-side-1, i-side, i});
                    mesh.F.push_back({i-side-1, i, i-1});
                }
            }
        }
        spirula::write_mesh_preview(dir.string(), mesh, side, ample, false);
        ok &= spirula::read_mesh_preview(dir.string(), 0, preview, ample) == side;
        ok &= mesh.V == preview.V && mesh.F == preview.F;
        ok &= !spirula::read_mesh_preview(dir.string(), side, preview, ample);
        if (side == 4) continue;
        ok &= spirula::read_mesh_preview(dir.string(), 0, preview, tight) == side;
        ok &= preview.F.size() < mesh.F.size() && tight.fits_surface(preview.V.size(), preview.F.size());
        spirula::write_mesh_preview(dir.string(), mesh, side + 1, tight, false);
        ok &= !spirula::read_mesh_preview(dir.string(), side, preview, ample);
        spirula::write_mesh_preview(dir.string(), mesh, side + 2, tight, true);
        ok &= spirula::read_mesh_preview(dir.string(), side, preview, tight) == side + 2;
        // Disconnected sampled faces lose most of the square's area.
        double area = 0;
        for (const auto& f : preview.F) {
            const auto& a = preview.V[f[0]], &b = preview.V[f[1]], &c = preview.V[f[2]];
            area += 0.5 * ((b[0]-a[0])*(c[1]-a[1]) - (b[1]-a[1])*(c[0]-a[0]));
        }
        ok &= area > 0.95 * (side-1) * (side-1);
        ok &= preview.F.size() < mesh.F.size() && preview.V.size() < preview.F.size();
    }
    fs::remove_all(dir);
    std::puts(ok ? "mesh_preview_test: OK" : "mesh_preview_test: FAILED");
    return ok ? 0 : 1;
}

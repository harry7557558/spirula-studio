// MarkerCropBox.cpp -- automatic bounding box (ROI) generation from markers.
#include "data/MarkerCropBox.h"

#include <algorithm>
#include <filesystem>
#include <limits>

namespace fs = std::filesystem;

namespace spirula {

RoiDocument compute_marker_crop_box(const std::vector<Marker3D>& aligned_markers,
                                    const MarkerCropOptions& opt) {
    double xmin = 0.0, xmax = 0.0, ymin = 0.0, ymax = 0.0, zmin = 0.0, zmax = 0.0;

    if (opt.explicit_bounds) {
        xmin = opt.bounds[0]; xmax = opt.bounds[1];
        ymin = opt.bounds[2]; ymax = opt.bounds[3];
        zmin = opt.bounds[4]; zmax = opt.bounds[5];
    } else {
        xmin = std::numeric_limits<double>::infinity();
        xmax = -std::numeric_limits<double>::infinity();
        ymin = std::numeric_limits<double>::infinity();
        ymax = -std::numeric_limits<double>::infinity();

        for (const auto& m : aligned_markers) {
            xmin = std::min(xmin, m.center[0]);
            xmax = std::max(xmax, m.center[0]);
            ymin = std::min(ymin, m.center[1]);
            ymax = std::max(ymax, m.center[1]);
            for (int k = 0; k < 4; k++) {
                xmin = std::min(xmin, m.corners[k][0]);
                xmax = std::max(xmax, m.corners[k][0]);
                ymin = std::min(ymin, m.corners[k][1]);
                ymax = std::max(ymax, m.corners[k][1]);
            }
        }

        if (!std::isfinite(xmin)) {
            xmin = -0.5; xmax = 0.5;
            ymin = -0.5; ymax = 0.5;
        }

        xmin -= opt.pad_xy;
        xmax += opt.pad_xy;
        ymin -= opt.pad_xy;
        ymax += opt.pad_xy;
        zmin = opt.z_min;
        zmax = opt.z_max;
    }

    RoiShape box;
    box.name = "Marker Crop";
    box.kind = RoiShapeKind::Box;
    box.op = RoiOp::Add;
    box.enabled = true;

    box.origin[0] = 0.5 * (xmin + xmax);
    box.origin[1] = 0.5 * (ymin + ymax);
    box.origin[2] = 0.5 * (zmin + zmax);

    box.lo[0] = xmin - box.origin[0]; box.hi[0] = xmax - box.origin[0];
    box.lo[1] = ymin - box.origin[1]; box.hi[1] = ymax - box.origin[1];
    box.lo[2] = zmin - box.origin[2]; box.hi[2] = zmax - box.origin[2];

    RoiDocument doc;
    doc.shapes.push_back(box);
    return doc;
}

bool save_marker_crop_roi(const std::string& dataset_dir,
                          const RoiDocument& doc,
                          const std::string& roi_name,
                          std::string& out_path,
                          std::string& error) {
    try {
        const fs::path dir = fs::path(dataset_dir) / "roi";
        std::error_code ec;
        fs::create_directories(dir, ec);
        if (ec) {
            error = "cannot create directory: " + dir.string();
            return false;
        }

        const std::string fname = roi_name.empty() ? "marker_crop.json"
            : (roi_name.find(".json") != std::string::npos ? roi_name : roi_name + ".json");
        const fs::path file = dir / fname;
        write_roi_file(doc, file.string());
        out_path = file.string();
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

}  // namespace spirula


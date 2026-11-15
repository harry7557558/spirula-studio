#include "app/gui/SfmProgress.h"
#include "sfm/core/Model.h"
#include "sfm/core/Progress.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>

int main() {
    namespace fs = std::filesystem;
    const auto dir = fs::temp_directory_path() / ("spirula-sfm-preview-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir);
    auto require = [](bool ok, const char* message) {
        if (!ok) throw std::runtime_error(message);
    };
    try {
        sfm::progress::set_dir(dir.string());
        sfm::Event event;
        event.kind = sfm::Event::Kind::StageBegin;
        event.stage = sfm::Stage::GlobalInitialize;
        event.total = 10000;
        sfm::progress::status(event);
        gui::RunStatus status;
        int64_t status_stamp = 0;
        require(gui::read_status(dir.string(), status_stamp, status) &&
                status.stage == (uint32_t)sfm::Stage::GlobalInitialize && status.total == 10000,
                "GUI cannot read global initialization status");
        gui::MappingWork work;
        work.initialized(2000, 10000);
        require(work.work() > 0 && work.work() < 0.75, "initialization does not advance mapping");
        const double before = work.work();
        work.placed(0, 100);
        work.initialized(1000, 10000);
        require(work.work() == before, "mapping regresses on a phase transition");
        work.initialized(10000, 10000);
        work.refining();
        require(work.work() == 0.75, "initialization leaves no budget for refinement");

        sfm::Reconstruction rec;
        rec.cameras[1] = sfm::Camera::defaultFor(1, 640, 480, 500);
        for (uint32_t id : {2u, 7u}) {
            sfm::Image im;
            im.id = id;
            im.camera_id = 1;
            im.registered = true;
            im.pose.t = {(double)id, 0, 0};
            rec.images.emplace(id, im);
        }
        sfm::Point3D point;
        point.xyz = {0, 0, 4};
        point.rgb[0] = 200;
        rec.points3D.emplace(0, point);
        sfm::progress::model(rec, true);
        gui::LiveModel model;
        int64_t model_stamp = 0;
        require(gui::read_live_model(dir.string(), model_stamp, model) &&
                model.n_registered == 2 && model.n_points == 1 &&
                model.ds.points.rgb[0] == 200 && std::isfinite(model.ds.train_frame_scale),
                "GUI cannot read positioning preview");
        require(!gui::read_live_model(dir.string(), model_stamp, model), "unchanged preview reloads");
        sfm::progress::set_dir({});
        fs::remove_all(dir);
        std::cout << "PASS global SfM live preview transport and mapping work\n";
        return 0;
    } catch (const std::exception& e) {
        sfm::progress::set_dir({});
        fs::remove_all(dir);
        std::cerr << e.what() << '\n';
        return 1;
    }
}

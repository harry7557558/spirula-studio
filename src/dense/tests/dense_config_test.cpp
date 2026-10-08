#include "dense/DenseConfig.h"

#include <cstdio>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
template<class F> void rejects(F action) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    require(rejected, "invalid dense configuration accepted");
}

}  // namespace

int main() {
    try {
        spirula::dense::DenseConfig config;
        config.validate_run();
        require(config.matching_space == "rectified", "legacy matching space changed");
        const auto original = config;
        config.apply_source_workflow(); config.validate_run();
        require(config.matching_space == "source" && config.pairs.neighbors == 3 && config.samples_per_reference == 10000 &&
                config.reference_fraction == 0.8 && config.source_reprojection_error == 1 &&
                config.geometry.max_reprojection_error == original.geometry.max_reprojection_error &&
                config.match.low_width == original.match.low_width && config.use_masks == original.use_masks &&
                config.geometry.min_source_images == original.geometry.min_source_images, "source setup changed unrelated settings");
        require(config.resolved_geometry().max_reprojection_error == 1, "source tolerance not resolved");
        config.reference_fraction = 0; rejects([&] { config.validate(); });
        config = original;
        require(config.preset == "balanced" && config.matches_preset("balanced") && config.geometry.min_source_images == 3 &&
                config.match.high_width == 0 && config.pairs.reference_coverage > 0, "dense defaults differ");
        for (const char* name : spirula::dense::kDensePresets) {
            auto preset = original;
            preset.geometry.min_source_images = 2; preset.use_masks = false;
            preset.apply_preset(name); preset.validate_run();
            require(preset.matches_preset(name) && preset.geometry.min_source_images == 2 && !preset.use_masks,
                    "preset did not hold its own values or reset unrelated controls");
            preset.pairs.neighbors += 1;
            require(!preset.matches_preset(name), "an edited preset still reads as unedited");
        }
        config.apply_preset("high");
        require(config.match.high_width == 1280 && config.effective_cycle_check() && config.pairs.reference_coverage == 0 &&
                config.stride == 1, "high quality preset is not the full-quality setting");
        config = original;
        config.geometry.min_source_images = 2;
        config.match.memory_budget_bytes = 123456789;
        config.apply_preset("fast");
        require(config.match.low_width == 512 && !config.effective_cycle_check() && config.cycle_check &&
                config.geometry.min_source_images == 2 && config.match.memory_budget_bytes == 123456789, "preset reset unrelated controls");
        config.apply_preset("precise");
        require(config.match.high_width == 1280 && config.effective_cycle_check(), "precise did not restore matching controls");
        config.match.low_width = 336; config.match.low_height = 256;
        config.apply_preset("custom"); config.validate();
        require(config.match.low_width == 336 && config.match.low_height == 256, "custom preset reset resolution");
        auto large = config;
        large.match.low_width = 16384; large.match.low_height = 16;
        large.pairs.neighbors = 4096; large.pairs.sequence_window = 4096;
        large.stride = 32768; large.max_face_size = 32768; large.mask_boundary = 8192;
        large.validate_run();
        large.match.low_width = large.match.low_height = 16384;
        rejects([&] { large.validate(); });
        config.geometry.min_source_images = 3; config.pairs.neighbors = 1;
        config.validate();
        rejects([&] { config.validate_run(); });
        config.pairs.mode = spirula::dense::PairMode::Sequential; config.validate_run();
        config.pairs.mode = spirula::dense::PairMode::Automatic;
        config.geometry.min_source_images = 2; config.validate_run();
        config.geometry.min_source_images = 3; config.pairs.neighbors = 2; config.validate_run();
        config.geometry.min_source_images = 1; rejects([&] { config.validate(); });
        config.geometry.min_source_images = 3; config.min_overlap = 2; rejects([&] { config.validate(); });
        config.min_overlap = 0.5; config.match.high_height = 0; rejects([&] { config.validate(); });
        rejects([&] { config.apply_preset("unknown"); });
        std::printf("PASS dense defaults, editable presets, cycle dependency, resource preservation, validation\n");
        return 0;
    } catch (const std::exception& e) { std::printf("FAIL %s\n", e.what()); return 1; }
}

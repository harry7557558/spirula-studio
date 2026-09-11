// CPU-only checks for native-resolution training memory sizing and refusal.

#include "app/TrainerCore.h"

#include <cstdint>
#include <cstdio>


using namespace spirula;
int main() {
    ParsedDataset ds;
    ds.num_cameras = 1;
    ds.widths = {15520};
    ds.heights = {7760};
    ds.camera_models = {0};
    ds.camera_distortions = {0};
    ds.train_indices = {0};

    PostSplitCameras post;
    post.n_post = 6;
    post.any_warp = true;
    post.K_per_camera = {6};
    post.post_offsets = {0};
    post.post_widths.assign(6, 4482);
    post.post_heights.assign(6, 4482);

    TrainConfig cfg;
    cfg.cap_max = 1000000;
    cfg.max_batch_per_epoch = 1;
    cfg.num_loss_scales = 0;

    const uint64_t gib = 1ull << 30;
    TrainingMemoryEstimate unsplit = estimate_training_memory(
        ds, post, cfg, false, false, false, cfg.cap_max, 0);
    if (!unsplit.dynamic_unknown || unsplit.known_minimum_bytes <= 20 * gib) {
        std::fprintf(stderr, "120 MP known minimum was not preserved\n");
        return 1;
    }

    backend::BudgetSnapshot budget;
    budget.status = backend::BudgetStatus::Available;
    budget.available_bytes = 16 * gib;
    budget.total_bytes = 16 * gib;
    auto failure = training_memory_refusal(budget, unsplit, 0);
    if (!failure || failure->kind != backend::BudgetFailureKind::DriverHeadroom) {
        std::fprintf(stderr, "120 MP workload was not refused before allocation\n");
        return 1;
    }

    budget.available_bytes = unsplit.known_minimum_bytes +
                             kTrainingMemoryReserveBytes;
    if (training_memory_refusal(budget, unsplit, 0)) {
        std::fprintf(stderr, "exact driver boundary was refused\n");
        return 1;
    }
    --budget.available_bytes;
    if (!training_memory_refusal(budget, unsplit, 0)) {
        std::fprintf(stderr, "one-byte driver overflow was accepted\n");
        return 1;
    }

    budget.available_bytes = UINT64_MAX;
    budget.process_bytes = gib;
    uint64_t app_limit = gib + unsplit.known_minimum_bytes;
    if (training_memory_refusal(budget, unsplit, app_limit)) {
        std::fprintf(stderr, "exact application boundary was refused\n");
        return 1;
    }
    failure = training_memory_refusal(budget, unsplit, app_limit - 1);
    if (!failure || failure->kind != backend::BudgetFailureKind::ApplicationLimit) {
        std::fprintf(stderr, "application limit overflow was not typed\n");
        return 1;
    }

    budget.status = backend::BudgetStatus::Unavailable;
    failure = training_memory_refusal(budget, unsplit, 0);
    if (!failure ||
        failure->kind != backend::BudgetFailureKind::TelemetryUnavailable) {
        std::fprintf(stderr, "unavailable telemetry was not preserved\n");
        return 1;
    }

    TrainingMemoryEstimate split = estimate_training_memory(
        ds, post, cfg, false, false, false, cfg.cap_max, 1);
    if (split.known_minimum_bytes >= unsplit.known_minimum_bytes) {
        std::fprintf(stderr, "face-pass cap did not reduce the active minimum\n");
        return 1;
    }

    std::printf("training_memory_preflight: %.2f GiB unsplit, %.2f GiB split; ALL PASSED\n",
                (double)unsplit.known_minimum_bytes / gib,
                (double)split.known_minimum_bytes / gib);
    return 0;
}

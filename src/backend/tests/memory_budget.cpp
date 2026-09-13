// Tests memory budget enforcement under CUDA or Vulkan: limit refusal,
// reservation rollback, credit recovery, and single-winner competition.

#include "backend/api/BackendRuntime.h"
#include "core/SourcePath.h"
#ifdef SS_BACKEND_VULKAN
#include "backend/vulkan/VulkanContext.h"
#endif

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

std::atomic<int> g_fail{0};
std::mutex g_print_mutex;

#define CHECK(cond, ...)                                                       \
    do {                                                                       \
        if (!(cond)) {                                                         \
            {                                                                  \
                std::lock_guard<std::mutex> lock(g_print_mutex);               \
                std::fprintf(stderr, "  FAIL [%s:%d]: ", SS_FILE, __LINE__);   \
                std::fprintf(stderr, __VA_ARGS__);                             \
                std::fprintf(stderr, "\n");                                    \
            }                                                                  \
            g_fail.fetch_add(1, std::memory_order_relaxed);                    \
        }                                                                      \
    } while (0)

constexpr uint64_t k1MiB = 1024 * 1024;
constexpr uint64_t k2MiB = 2 * k1MiB;
constexpr uint64_t k3MiB = 3 * k1MiB;

void test_inactive_semantics() {
    std::printf("Running test_inactive_semantics...\n");
    backend::training_budget_end();
    CHECK(!backend::training_budget_active(), "budget must be inactive initially");

    // An absurdly large request cannot be satisfied.
    const size_t kHugeBytes = ~size_t(0) - k1MiB;

    // Raw device_malloc must return nullptr and not throw or set a budget failure.
    void* ptr = backend::device_malloc(kHugeBytes);
    CHECK(ptr == nullptr, "raw device_malloc of impossible size must return nullptr");

    backend::BudgetFailure failure{};
    CHECK(!backend::consume_budget_failure(failure),
          "no budget failure must be recorded when budget is inactive");

    // device_malloc_checked must throw standard OOM (runtime_error), never BudgetError.
    bool threw_budget = false;
    bool threw_oom = false;
    try {
        backend::device_malloc_checked(kHugeBytes, "test_inactive_huge");
    } catch (const backend::BudgetError&) {
        threw_budget = true;
    } catch (const std::runtime_error&) {
        threw_oom = true;
    } catch (...) {
        CHECK(false, "unexpected exception type thrown");
    }
    CHECK(!threw_budget, "device_malloc_checked must not throw BudgetError when inactive");
    CHECK(threw_oom, "device_malloc_checked must throw runtime_error OOM when inactive");
}

bool check_telemetry_status() {
    backend::BudgetSnapshot snap = backend::budget_snapshot();
    if (snap.status == backend::BudgetStatus::Unavailable) {
        std::printf("Telemetry unavailable (status=Unavailable). Testing distinct refusal...\n");
        backend::training_budget_begin(0, 0);
        CHECK(backend::training_budget_active(), "training budget active");

        void* ptr = backend::device_malloc(k1MiB);
        CHECK(ptr == nullptr, "device_malloc must return nullptr when telemetry unavailable");

        backend::BudgetFailure fail{};
        CHECK(backend::consume_budget_failure(fail),
              "consume_budget_failure must return true on telemetry unavailable");
        CHECK(fail.kind == backend::BudgetFailureKind::TelemetryUnavailable,
              "failure kind must be TelemetryUnavailable, got %d", static_cast<int>(fail.kind));

        bool threw_telemetry_error = false;
        try {
            backend::device_malloc_checked(k1MiB, "test_telemetry_unavail");
        } catch (const backend::BudgetError& err) {
            threw_telemetry_error = true;
            CHECK(err.failure.kind == backend::BudgetFailureKind::TelemetryUnavailable,
                  "BudgetError kind must be TelemetryUnavailable");
        } catch (const std::exception& e) {
            CHECK(false, "unexpected exception on unavailable telemetry: %s", e.what());
        }
        CHECK(threw_telemetry_error, "device_malloc_checked must throw BudgetError");

        backend::training_budget_end();
        CHECK(!backend::training_budget_active(), "training budget ended");
        return false;
    }

    if (snap.status == backend::BudgetStatus::QueryError) {
        std::printf("Telemetry query error (status=QueryError). Testing distinct refusal...\n");
        backend::training_budget_begin(0, 0);
        void* ptr = backend::device_malloc(k1MiB);
        CHECK(ptr == nullptr, "malloc must return nullptr on telemetry query error");

        backend::BudgetFailure fail{};
        CHECK(backend::consume_budget_failure(fail), "consumed failure");
        CHECK(fail.kind == backend::BudgetFailureKind::TelemetryError,
              "failure kind must be TelemetryError");
        backend::training_budget_end();
        return false;
    }

    CHECK(snap.status == backend::BudgetStatus::Available, "telemetry status Available");
    CHECK(snap.total_bytes > 0, "total_bytes > 0");
    CHECK(snap.available_bytes <= snap.total_bytes, "available <= total");
    CHECK(snap.device_index >= 0, "device_index >= 0");
    return true;
}

void test_app_limit_and_rollback() {
    std::printf("Running test_app_limit_and_rollback...\n");
    backend::BudgetSnapshot base = backend::budget_snapshot();
    const uint64_t initial_process = base.process_bytes;
    const uint64_t limit = initial_process + k2MiB;

    backend::training_budget_begin(0, limit);
    CHECK(backend::training_budget_active(), "training budget active");

    backend::BudgetSnapshot active_snap = backend::budget_snapshot();
    CHECK(active_snap.app_limit_bytes == limit, "app_limit_bytes matches requested limit");
    const uint64_t initial_reserved = active_snap.reserved_bytes;
    CHECK(active_snap.process_bytes == initial_process, "process_bytes at baseline");

    // 1. Oversize raw device_malloc: 3 MiB requested against 2 MiB allowance.
    void* p_over = backend::device_malloc(k3MiB);
    CHECK(p_over == nullptr, "oversize device_malloc must return nullptr");

    backend::BudgetFailure fail{};
    CHECK(backend::consume_budget_failure(fail), "consumed budget failure");
    CHECK(fail.kind == backend::BudgetFailureKind::ApplicationLimit,
          "failure kind must be ApplicationLimit, got %d", static_cast<int>(fail.kind));
    CHECK(fail.requested_bytes == k3MiB, "requested_bytes matches 3 MiB");
    CHECK(fail.available_bytes <= limit, "available_bytes bounded by limit");
    CHECK(fail.device_index >= 0, "device_index is valid");

    // Rollback check: failed reservation must not increase reserved_bytes or process_bytes.
    backend::BudgetSnapshot post_fail_snap = backend::budget_snapshot();
    CHECK(post_fail_snap.reserved_bytes == initial_reserved,
          "rollback must restore reserved_bytes on refused raw malloc");
    CHECK(post_fail_snap.process_bytes == initial_process,
          "process_bytes unchanged on refused raw malloc");

    // 2. Oversize device_malloc_checked must throw BudgetError with structured fields.
    bool caught_budget_error = false;
    try {
        backend::device_malloc_checked(k3MiB, "oversize_buf");
    } catch (const backend::BudgetError& err) {
        caught_budget_error = true;
        CHECK(err.failure.kind == backend::BudgetFailureKind::ApplicationLimit,
              "BudgetError carries ApplicationLimit");
        CHECK(err.failure.requested_bytes == k3MiB, "BudgetError carries requested_bytes");
        CHECK(err.what() != nullptr, "BudgetError::what() must not be null");
    } catch (const std::exception& e) {
        CHECK(false, "unexpected exception: %s", e.what());
    }
    CHECK(caught_budget_error, "device_malloc_checked must throw BudgetError");

    // Rollback check: failed checked allocation must also not leak reserved_bytes or process_bytes.
    CHECK(backend::budget_snapshot().reserved_bytes == initial_reserved,
          "rollback must restore reserved_bytes on refused checked malloc");
    CHECK(backend::budget_snapshot().process_bytes == initial_process,
          "process_bytes unchanged on refused checked malloc");

    // 3. To prove rollback did not leak allowance, a valid 2 MiB allocation must succeed now.
    void* p_valid = backend::device_malloc_checked(k2MiB, "valid_post_rollback");
    CHECK(p_valid != nullptr, "allocation within allowance succeeded (no allowance leak)");

    // After commit, reserved_bytes returns to baseline, while process_bytes tracks committed allocation.
    backend::BudgetSnapshot committed_snap = backend::budget_snapshot();
    CHECK(committed_snap.reserved_bytes == initial_reserved,
          "reserved_bytes returns to baseline after commit");
    CHECK(committed_snap.process_bytes >= initial_process + k2MiB,
          "process_bytes tracks committed allocation");

    backend::device_free(p_valid);
    backend::device_synchronize();

    backend::BudgetSnapshot post_free_snap = backend::budget_snapshot();
    CHECK(post_free_snap.reserved_bytes == initial_reserved,
          "reserved_bytes at baseline after valid allocation free");
    CHECK(post_free_snap.process_bytes == initial_process,
          "process_bytes restored after valid allocation free");

    backend::training_budget_end();
    CHECK(!backend::training_budget_active(), "training budget ended");
}

void test_release_and_reallocate() {
    std::printf("Running test_release_and_reallocate...\n");
    backend::BudgetSnapshot base = backend::budget_snapshot();
    const uint64_t initial_process = base.process_bytes;
    const uint64_t limit = initial_process + k2MiB;

    backend::training_budget_begin(0, limit);
    const uint64_t initial_reserved = backend::budget_snapshot().reserved_bytes;

    // Allocate 1.5 MiB (within 2 MiB allowance).
    const uint64_t k1_5MiB = (3 * k1MiB) / 2;
    void* p1 = backend::device_malloc_checked(k1_5MiB, "p1_initial");
    CHECK(p1 != nullptr, "p1 allocation succeeded");

    // After commit, reserved_bytes returns to baseline, process_bytes tracks committed allocation.
    backend::BudgetSnapshot mid_snap = backend::budget_snapshot();
    CHECK(mid_snap.reserved_bytes == initial_reserved,
          "reserved_bytes returns to baseline after commit");
    CHECK(mid_snap.process_bytes >= initial_process + k1_5MiB,
          "process_bytes tracks committed allocation of 1.5 MiB");

    // Second allocation of 1 MiB exceeds remaining 0.5 MiB allowance.
    void* p2 = backend::device_malloc(k1MiB);
    CHECK(p2 == nullptr, "second allocation must fail against remaining allowance");

    backend::BudgetFailure fail{};
    CHECK(backend::consume_budget_failure(fail), "consumed budget failure");
    CHECK(fail.kind == backend::BudgetFailureKind::ApplicationLimit,
          "expected ApplicationLimit failure");

    // Free p1: credit must be reclaimed upon free completion.
    backend::device_free(p1);
    backend::device_synchronize();

    backend::BudgetSnapshot post_free_snap = backend::budget_snapshot();
    CHECK(post_free_snap.reserved_bytes == initial_reserved,
          "reserved_bytes returned to baseline after free");
    CHECK(post_free_snap.process_bytes == initial_process,
          "process_bytes returned to baseline after free");

    // Reallocate 1.5 MiB: fails deterministically if free credit was permanently lost.
    void* p3 = backend::device_malloc_checked(k1_5MiB, "p3_realloc");
    CHECK(p3 != nullptr, "reallocation succeeded (free credit was not permanently lost)");

    backend::BudgetSnapshot realloc_snap = backend::budget_snapshot();
    CHECK(realloc_snap.reserved_bytes == initial_reserved,
          "reserved_bytes at baseline after realloc commit");
    CHECK(realloc_snap.process_bytes >= initial_process + k1_5MiB,
          "process_bytes tracks reallocated allocation");

    backend::device_free(p3);
    backend::device_synchronize();

    CHECK(backend::budget_snapshot().process_bytes == initial_process,
          "process_bytes returned to baseline after final free");
    CHECK(backend::budget_snapshot().reserved_bytes == initial_reserved,
          "reserved_bytes at baseline after final free");

    backend::training_budget_end();
    CHECK(!backend::training_budget_active(), "training budget ended");
}

void test_multithreaded_competition() {
    std::printf("Running test_multithreaded_competition...\n");
    backend::BudgetSnapshot base = backend::budget_snapshot();
    const uint64_t initial_process = base.process_bytes;
    const uint64_t limit = initial_process + k2MiB;

    backend::training_budget_begin(0, limit);
    const uint64_t initial_reserved = backend::budget_snapshot().reserved_bytes;

    std::atomic<int> ready{0};
    std::atomic<bool> go{false};

    void* ptr_a = nullptr;
    void* ptr_b = nullptr;
    bool a_succeeded = false;
    bool b_succeeded = false;
    bool a_caught_budget_error = false;
    bool b_caught_budget_error = false;

    auto worker = [&](void*& out_ptr, bool& out_succeeded, bool& out_caught_err) {
        ready.fetch_add(1, std::memory_order_release);
        while (!go.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        try {
            out_ptr = backend::device_malloc_checked(k2MiB, "competing_thread");
            if (out_ptr) {
                out_succeeded = true;
            }
        } catch (const backend::BudgetError& err) {
            out_caught_err = true;
            CHECK(err.failure.kind == backend::BudgetFailureKind::ApplicationLimit,
                  "competing thread failure kind must be ApplicationLimit");
        } catch (const std::exception& e) {
            CHECK(false, "competing thread unexpected exception: %s", e.what());
        }
    };

    std::thread ta(worker, std::ref(ptr_a), std::ref(a_succeeded), std::ref(a_caught_budget_error));
    std::thread tb(worker, std::ref(ptr_b), std::ref(b_succeeded), std::ref(b_caught_budget_error));

    while (ready.load(std::memory_order_acquire) < 2) {
        std::this_thread::yield();
    }
    go.store(true, std::memory_order_release);

    ta.join();
    tb.join();

    // Deterministically verify no double-spending: exactly one thread must succeed.
    CHECK((a_succeeded ^ b_succeeded),
          "exactly one thread must succeed without double-spending (a=%d, b=%d)",
          a_succeeded ? 1 : 0, b_succeeded ? 1 : 0);
    CHECK((a_caught_budget_error ^ b_caught_budget_error),
          "exactly one thread must catch BudgetError (a=%d, b=%d)",
          a_caught_budget_error ? 1 : 0, b_caught_budget_error ? 1 : 0);

    // After commit, reserved_bytes returns to baseline, while process_bytes tracks the single winner.
    backend::BudgetSnapshot comp_snap = backend::budget_snapshot();
    CHECK(comp_snap.reserved_bytes == initial_reserved,
          "reserved_bytes returns to baseline after winning thread commits");
    CHECK(comp_snap.process_bytes >= initial_process + k2MiB &&
          comp_snap.process_bytes < initial_process + (2 * k2MiB),
          "process_bytes reflects single winning allocation, not double-spend");

    // Clean up winner's allocation.
    if (ptr_a) backend::device_free(ptr_a);
    if (ptr_b) backend::device_free(ptr_b);
    backend::device_synchronize();

    backend::BudgetSnapshot post_comp_snap = backend::budget_snapshot();
    CHECK(post_comp_snap.reserved_bytes == initial_reserved,
          "reserved_bytes returned to baseline after freeing winning thread");
    CHECK(post_comp_snap.process_bytes == initial_process,
          "process_bytes returned to baseline after freeing winning thread");

    backend::training_budget_end();
    CHECK(!backend::training_budget_active(), "training budget ended");
}


}  // namespace

int main() {
    std::printf("== memory_budget unit test ==\n");

#ifdef SS_BACKEND_VULKAN
    CHECK(!backend::vk::context_created(), "snapshot test starts before context creation");
    (void)backend::budget_snapshot();
    CHECK(!backend::vk::context_created(), "inactive snapshot must not create a context");
#endif
    if (backend::device_count() > 0 && backend::device_current() < 0) {
        backend::device_select(0);
    }
    CHECK(backend::device_prepare(), "selected device initializes");
#ifdef SS_BACKEND_VULKAN
    CHECK(backend::vk::context_created(), "device_prepare creates the selected context");
#endif

    test_inactive_semantics();

    if (check_telemetry_status()) {
        test_app_limit_and_rollback();
        test_release_and_reallocate();
        test_multithreaded_competition();
    }

    int failures = g_fail.load(std::memory_order_relaxed);
    if (failures == 0) {
        std::printf("All memory_budget checks passed.\n");
        return 0;
    } else {
        std::fprintf(stderr, "%d check(s) FAILED.\n", failures);
        return 1;
    }
}

// A blocked queue must fail without entering driver teardown (issue #336).

#include "app/gui/Subprocess.h"
#include "backend/api/BackendRuntime.h"
#include "backend/vulkan/VulkanContext.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

static int blocked_child() {
#ifdef _WIN32
    _putenv_s("SS_GPU_STALL_SECONDS", "0.1");
#else
    setenv("SS_GPU_STALL_SECONDS", "0.1", 1);
#endif
    auto& ctx = backend::vk::Context::get();
    if (!ctx.ok()) return 90;
    void* buffer = backend::device_malloc(4096);
    if (!buffer) return 91;
    backend::memset_sync(buffer, 0, 4096);
    backend::device_synchronize();
    if (backend::last_error()) return 91;

    VkSemaphoreTypeCreateInfo type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
    type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    VkSemaphoreCreateInfo create{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    create.pNext = &type;
    VkSemaphore gate = VK_NULL_HANDLE;
    if (vkCreateSemaphore(ctx.device(), &create, nullptr, &gate) != VK_SUCCESS)
        return 92;
    const uint64_t value = 1;
    VkTimelineSemaphoreSubmitInfo timeline{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
    timeline.waitSemaphoreValueCount = 1;
    timeline.pWaitSemaphoreValues = &value;
    const VkPipelineStageFlags stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.pNext = &timeline;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &gate;
    submit.pWaitDstStageMask = &stage;
    VkQueue queue;
    vkGetDeviceQueue(ctx.device(), ctx.queue_family(), 0, &queue);
    if (vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS)
        return 93;

    std::atexit([] { std::_Exit(94); });
    backend::memset_sync(buffer, 0, 4096);
    backend::device_synchronize();
    std::_Exit(95);
}

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--blocked") == 0)
        return blocked_child();
    std::atomic<bool> cancel{false};
    std::string output;
    const auto start = std::chrono::steady_clock::now();
    const int code = gui::run_process(
        {std::filesystem::absolute(argv[0]).string(), "--blocked"}, {},
        [&](const std::string& line) { output += line + '\n'; }, cancel,
        [&] {
            if (std::chrono::steady_clock::now() - start > std::chrono::seconds(10))
                cancel = true;
        });
    const bool passed = code == EXIT_FAILURE && !cancel &&
        output.find("GPU stopped responding") != std::string::npos &&
        output.find("terminating without driver teardown") != std::string::npos;
    std::printf("%s: stalled queue exits with an error without teardown (code %d)\n%s",
                passed ? "PASS" : "FAIL", code, output.c_str());
    return passed ? 0 : 1;
}

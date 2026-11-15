#include "app/InferenceWorker.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
struct Scratch {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("spirula-worker-byte-check-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Scratch() { require(std::filesystem::create_directory(path), "Cannot create exclusive byte-budget scratch"); }
    ~Scratch() { std::error_code ec; std::filesystem::remove_all(path, ec); }
};
}

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--binary-child") {
#ifdef _WIN32
        if (_setmode(_fileno(stdout), _O_TEXT) < 0) return 2;
#endif
        if (!app::worker_begin()) return 2;
        return app::worker_write_frame(std::string("\n\r\x1a\0binary", 10)) ? 0 : 3;
    }
    if (argc == 3 && std::string(argv[1]) == "--child") {
        if (!app::worker_begin()) return 2;
        const std::string mode = argv[2];
        std::string request;
        while (app::worker_read_frame(request)) {
            // Ordinary library stdout must never corrupt the response stream.
            std::printf("worker diagnostic\n");
            std::fflush(stdout);
            if (mode == "die") return 7;
            if (mode.rfind("bounded:", 0) == 0) {
                const std::filesystem::path active = mode.substr(8);
                if (!std::filesystem::create_directory(active)) return 9;
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
                std::filesystem::remove(active);
            }
            if (mode == "hang") std::this_thread::sleep_for(std::chrono::seconds(30));
            if (mode == "slow") std::this_thread::sleep_for(std::chrono::milliseconds(150));
            if (mode == "fast") std::this_thread::sleep_for(std::chrono::milliseconds(5));
            if (!app::worker_write_frame(mode + ":" + request)) return 3;
        }
        return mode == "exit-fail" ? 7 : 0;
    }
    try {
        const std::string exe = std::filesystem::absolute(argv[0]).string();
        auto command = [&](const char* mode) { return std::vector<std::string>{exe, "--child", mode}; };
        std::atomic<bool> cancel{false};
        std::string error;
        {
            gui::ProcessDuplex child;
            require(child.start({exe, "--binary-child"}, {}), "Cannot start binary protocol child");
            // Both the length header and payload contain 0x0A; CRT text mode expands either.
            const std::string expected("\x0a\0\0\0\n\r\x1a\0binary", 14);
            std::string actual(expected.size(), '\0');
            size_t received = 0;
            while (received < actual.size()) {
                const size_t n = child.read(actual.data() + received, actual.size() - received);
                require(n != 0, "Binary protocol frame truncated");
                received += n;
            }
            require(actual == expected, "Binary protocol changed header or payload bytes");
            char extra;
            require(child.read(&extra, 1) == 0, "Binary protocol inserted extra bytes");
            require(child.finish() == 0, "Binary protocol child failed");
        }
        int fast = 0, slow = 0, diagnostics = 0;
        std::vector<bool> accepted(8, false);
        std::vector<app::WorkerTask> tasks;
        for (size_t i = 0; i < accepted.size(); ++i) tasks.push_back({std::to_string(i), 1});
        require(app::run_inference_workers({command("slow"), command("fast")}, tasks, 2, cancel,
            [&](size_t index, const std::string& response, std::string&) {
                require(!accepted[index], "Output accepted more than once");
                require(response == "fast:" + std::to_string(index) || response == "slow:" + std::to_string(index),
                    "Response belongs to wrong task");
                accepted[index] = true;
                if (response.rfind("fast:", 0) == 0) ++fast; else ++slow;
                return true;
            }, [&](const std::string& line) { if (line == "worker diagnostic") ++diagnostics; }, error), error.c_str());
        for (bool value : accepted) require(value, "Required output never accepted");
        require(fast > slow, "Dispatch did not adapt to device speed");
        require(diagnostics == 8, "Diagnostics not drained separately from protocol");
        {
            Scratch scratch;
            const auto bounded = command(("bounded:" + (scratch.path / "active").string()).c_str());
            size_t completed = 0;
            require(app::run_inference_workers({bounded, bounded}, {{"0", 1}, {"1", 1}, {"2", 1}},
                1, cancel, [&](size_t, const std::string&, std::string&) { ++completed; return true; },
                {}, error), "Shared-byte admission allowed simultaneous exclusive tasks");
            require(completed == 3, "Byte-bounded producers did not unblock");
        }
        int published = 0;
        auto accept = [&](size_t, const std::string&, std::string&) { ++published; return true; };
        require(!app::run_inference_workers({command("die")}, {{"0", 1}}, 1, cancel,
            accept, {}, error), "Dead worker reported success");
        require(published == 0, "Worker death published incomplete output");
        require(!app::run_inference_workers({command("exit-fail")}, {{"0", 1}}, 1, cancel,
            accept, {}, error), "Worker failure after acknowledgement reported success");
        require(published == 1, "Exit-failure regression did not accept the completed response");
        require(error == "Inference worker exited unsuccessfully", "Worker exit failure diagnostic lost");
        published = 0;
        require(!app::run_inference_workers({command("fast")}, {{"0", 2}}, 1, cancel,
            accept, {}, error), "Oversized task ignored shared byte bound");
        require(published == 0, "Rejected task published output");
        require(!app::run_inference_workers({command("fast")}, {{std::string(1024 * 1024 + 1, 'x'), 1}},
            1, cancel, accept, {}, error), "Oversized protocol request admitted");
        require(!app::run_inference_workers({command("fast")}, {{"0", 1}}, 1, cancel,
            [](size_t, const std::string&, std::string& why) { why = "required family missing"; return false; },
            {}, error), "Rejected family reported completion");
        require(error == "required family missing", "Output failure diagnostic lost");
        {
            std::atomic<bool> slow_cancel{false}, accepting{false};
            std::atomic<int> busy{0};
            bool canceled_while_busy = false;
            std::vector<bool> slow_accepted(4, false);
            int publications = 0;
            const auto started = std::chrono::steady_clock::now();
            std::thread stop_publication([&] {
                const auto deadline = started + std::chrono::seconds(3);
                while ((!accepting.load() || busy.load() != 2) &&
                    std::chrono::steady_clock::now() < deadline)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                canceled_while_busy = accepting.load() && busy.load() == 2;
                slow_cancel = true;
            });
            const bool ok = app::run_inference_workers(
                {command("fast"), command("hang")}, {{"0", 1}, {"1", 1}, {"2", 1}, {"3", 1}},
                2, slow_cancel,
                [&](size_t index, const std::string& response, std::string&) {
                    require(!slow_accepted[index], "Slow consumer accepted a task twice");
                    require(response == "fast:" + std::to_string(index),
                        "Slow consumer received the wrong task output");
                    slow_accepted[index] = true;
                    accepting = true;
                    while (!slow_cancel.load())
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    // Publication already in progress may finish, but cannot make cancellation succeed.
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    ++publications;
                    accepting = false;
                    return true;
                }, [&](const std::string& line) {
                    if (line == "worker diagnostic") ++busy;
                }, error);
            stop_publication.join();
            require(canceled_while_busy, "Cancellation missed slow acceptance with another worker busy");
            require(!ok && error == "Inference canceled", "Slow publication falsely completed canceled work");
            int acceptance_count = 0;
            for (bool value : slow_accepted) if (value) ++acceptance_count;
            require(acceptance_count == 1 && publications == 1,
                "Cancellation accepted queued work or duplicated slow publication");
            require(std::chrono::steady_clock::now() - started < std::chrono::seconds(5),
                "Slow publication prevented busy-worker cancellation from terminating");
        }
        std::thread stop([&] { std::this_thread::sleep_for(std::chrono::milliseconds(100)); cancel = true; });
        const auto begin = std::chrono::steady_clock::now();
        const bool canceled = app::run_inference_workers({command("hang")}, {{"0", 1}}, 1,
            cancel, accept, {}, error);
        stop.join();
        require(!canceled && published == 0, "Cancellation published output");
        require(std::chrono::steady_clock::now() - begin < std::chrono::seconds(5),
            "Cancellation failed to unblock protocol I/O");
        std::printf("inference worker invariants passed\n");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}

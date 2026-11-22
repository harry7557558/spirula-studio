#include "app/InferenceWorker.h"
#include <array>
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
using namespace std::chrono_literals;

template<class Predicate>
bool wait_until(Predicate ready, std::chrono::milliseconds timeout = 3s) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!ready()) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::sleep_for(1ms);
    }
    return true;
}

std::vector<std::string> worker_command(const std::string& exe, const std::string& mode) {
    return {exe, "--child", mode};
}

void check_pipeline_before_accept(const std::string& exe) {
    Scratch scratch;
    std::atomic<bool> cancel{false};
    std::string error;
    size_t accepted = 0;
    require(app::run_inference_workers({worker_command(exe, "tracked:" + scratch.path.string())},
        {{"0", 4, 1}, {"1", 4, 1}}, 5, cancel,
        [&](size_t index, const std::string&, std::string&) {
            if (index == 0)
                require(wait_until([&] { return std::filesystem::exists(scratch.path / "received-1"); }),
                    "Slow acceptance blocked the next request");
            ++accepted;
            return true;
        }, {}, error, [](size_t, const std::string&, std::string&) { return true; }), error.c_str());
    require(accepted == 2, "Pipelined response was lost or duplicated");
}

void check_serial_accepts(const std::string& exe) {
    std::atomic<bool> cancel{false};
    std::atomic<int> ready{0}, accepting{0};
    std::string error;
    size_t accepted = 0;
    require(app::run_inference_workers({worker_command(exe, "fast"), worker_command(exe, "fast")},
        {{"0", 1}, {"1", 1}}, 2, cancel,
        [&](size_t, const std::string&, std::string&) {
            require(accepting.fetch_add(1) == 0, "Accept callbacks overlapped");
            std::this_thread::sleep_for(30ms);
            ++accepted;
            --accepting;
            return true;
        }, {}, error, [&](size_t, const std::string&, std::string&) {
            ++ready;
            require(wait_until([&] { return ready.load() == 2; }), "Accept contenders never became ready");
            return true;
        }), error.c_str());
    require(accepted == 2, "Serialized acceptance lost a response");
}

void check_concurrent_prepares(const std::string& exe) {
    std::atomic<bool> cancel{false};
    std::atomic<int> preparing{0};
    std::string error;
    require(app::run_inference_workers({worker_command(exe, "fast"), worker_command(exe, "fast")},
        {{"0", 1}, {"1", 1}}, 2, cancel,
        [](size_t, const std::string&, std::string&) { return true; }, {}, error,
        [&](size_t, const std::string&, std::string&) {
            ++preparing;
            require(wait_until([&] { return preparing.load() == 2; }), "Prepare callbacks could not overlap");
            return true;
        }), error.c_str());
}

void check_callback_failures(const std::string& exe) {
    for (int mode = 0; mode < 4; ++mode) {
        std::atomic<bool> cancel{false};
        std::string error;
        size_t accepts = 0, prepares = 0, publications = 0;
        const auto begin = std::chrono::steady_clock::now();
        require(!app::run_inference_workers({worker_command(exe, "hang-next")},
            {{"0", 4, 1}, {"1", 4, 1}, {"2", 4, 1}}, 5, cancel,
            [&](size_t index, const std::string&, std::string& why) {
                require(index == 0, "Failure allowed a later acceptance");
                ++accepts;
                if (mode == 3) throw std::runtime_error("callback failure");
                why = "callback failure";
                return false;
            }, {}, error, [&](size_t index, const std::string&, std::string& why) {
                require(index == 0, "Failure read a queued response");
                ++prepares;
                if (mode == 2) throw std::runtime_error("callback failure");
                if (mode == 0) { why = "callback failure"; return false; }
                return true;
            }, [&](size_t) { ++publications; }), "Callback failure reported success");
        require(error == "callback failure", "First callback failure was overwritten");
        require(prepares == 1 && accepts == (mode == 0 || mode == 2 ? 0u : 1u) && publications == 0,
            "Callback failure accepted or published later work");
        require(std::chrono::steady_clock::now() - begin < 5s, "Failure waited for an outstanding response");
    }
    for (bool throws : {false, true}) {
        std::atomic<bool> cancel{false};
        std::atomic<int> ready{0}, accepts{0};
        std::string error;
        require(!app::run_inference_workers({worker_command(exe, "fast"), worker_command(exe, "fast")},
            {{"0", 1}, {"1", 1}}, 2, cancel,
            [&](size_t, const std::string&, std::string& why) {
                ++accepts;
                if (throws) throw std::runtime_error("acceptance failed");
                why = "acceptance failed";
                return false;
            }, {}, error, [&](size_t, const std::string&, std::string&) {
                ++ready;
                require(wait_until([&] { return ready.load() == 2; }), "Failure contenders never became ready");
                return true;
            }), "Concurrent acceptance failure reported success");
        require(error == "acceptance failed" && accepts.load() == 1,
            "Failed acceptance let a queued peer accept its output");
    }
}

void check_acceptance_budget(const std::string& exe) {
    Scratch scratch;
    std::atomic<bool> cancel{false}, checked{false};
    std::atomic<int> publishing{0};
    std::array<int, 6> accepted{}, published{};
    std::array<std::thread::id, 6> accepting_threads{};
    std::string error;
    const auto command = worker_command(exe, "paired:" + scratch.path.string());
    std::vector<app::WorkerTask> tasks;
    for (size_t i = 0; i < accepted.size(); ++i) tasks.push_back({std::to_string(i), 4, 1});
    require(app::run_inference_workers({command, command}, tasks, 2 * 4 + 1, cancel,
        [&](size_t index, const std::string&, std::string&) {
            ++accepted[index];
            accepting_threads[index] = std::this_thread::get_id();
            return true;
        }, {}, error, [](size_t, const std::string&, std::string&) { return true; },
        [&](size_t index) {
            require(accepting_threads[index] == std::this_thread::get_id(),
                "Publication moved off its accepting thread");
            ++published[index];
            if (index > 1) return;
            ++publishing;
            if (index == 1) {
                require(wait_until([&] { return checked.load(); }), "Budget inspection did not complete");
                return;
            }
            require(wait_until([&] { return publishing.load() == 2; }),
                "Publication held the acceptance or dispatch lock");
            require(wait_until([&] { return std::filesystem::exists(scratch.path / "received-2"); }),
                "Acceptance footprint did not admit a third producer");
            // Two retained acceptance charges must exclude a fourth host-sized task.
            require(!wait_until([&] { return std::filesystem::exists(scratch.path / "received-3"); }, 100ms),
                "Admission released acceptance bytes before publication completed");
            checked = true;
        }), error.c_str());
    for (size_t i = 0; i < accepted.size(); ++i)
        require(accepted[i] == 1 && published[i] == 1, "Acceptance or publication was lost or duplicated");
}

void check_cancel_during_prepare(const std::string& exe) {
    Scratch scratch;
    std::atomic<bool> cancel{false}, preparing{false};
    bool canceled_after_pipeline = false;
    size_t accepted = 0, published = 0;
    std::string error;
    const auto begin = std::chrono::steady_clock::now();
    std::thread stop([&] {
        canceled_after_pipeline = wait_until([&] {
            return preparing.load() && std::filesystem::exists(scratch.path / "received-1");
        });
        cancel = true;
    });
    const bool ok = app::run_inference_workers({worker_command(exe, "tracked:" + scratch.path.string())},
        {{"0", 4, 1}, {"1", 4, 1}}, 5, cancel,
        [&](size_t, const std::string&, std::string&) { ++accepted; return true; }, {}, error,
        [&](size_t, const std::string&, std::string& why) {
            preparing = true;
            require(wait_until([&] { return cancel.load(); }), "Cancellation did not reach slow preparation");
            std::this_thread::sleep_for(50ms);
            why = "preparation interrupted";
            return false;
        }, [&](size_t) { ++published; });
    stop.join();
    require(canceled_after_pipeline, "Cancellation missed pipelined slow preparation");
    require(!ok && error == "Inference canceled" && accepted == 0 && published == 0,
        "Cancellation during preparation published or reported a different result");
    require(std::chrono::steady_clock::now() - begin < 5s, "Slow preparation stranded cancellation");
}

void check_legacy_sequence(const std::string& exe) {
    Scratch scratch;
    std::atomic<bool> cancel{false};
    std::string error;
    require(app::run_inference_workers({worker_command(exe, "tracked:" + scratch.path.string())},
        {{"0", 4, 1}, {"1", 4, 1}}, 20, cancel,
        [&](size_t index, const std::string&, std::string&) {
            if (index == 0)
                require(!wait_until([&] { return std::filesystem::exists(scratch.path / "received-1"); }, 100ms),
                    "Legacy dispatcher sent the next request before acceptance returned");
            return true;
        }, {}, error), error.c_str());
    require(std::filesystem::exists(scratch.path / "received-1"), "Legacy dispatch lost the next request");
}

void check_full_host_default(const std::string& exe) {
    for (uint64_t budget : {5u, 8u}) {
        Scratch scratch;
        std::atomic<bool> cancel{false};
        std::string error;
        app::WorkerTask first{"0", 1};
        first.host_bytes = 4;
        size_t accepted = 0;
        require(app::run_inference_workers({worker_command(exe, "tracked:" + scratch.path.string())},
            {first, {"1", 4}}, budget, cancel,
            [&](size_t, const std::string&, std::string&) { ++accepted; return true; }, {}, error,
            [&](size_t index, const std::string&, std::string&) {
                if (index != 0) return true;
                const auto next_received = [&] { return std::filesystem::exists(scratch.path / "received-1"); };
                if (budget == 5)
                    require(!wait_until(next_received, 100ms), "Zero acceptance charge ignored updated host_bytes");
                else
                    require(wait_until(next_received), "Full-charge preparation did not pipeline when space fit");
                return true;
            }), error.c_str());
        require(accepted == 2, "Full-charge preparation lost an output");
    }
}

void check_publication_failure(const std::string& exe) {
    std::atomic<bool> cancel{false};
    std::string error;
    size_t accepts = 0, publications = 0;
    const auto accept = [&](size_t, const std::string&, std::string&) { ++accepts; return true; };
    const auto prepare = [](size_t, const std::string&, std::string&) { return true; };
    require(!app::run_inference_workers({worker_command(exe, "fast")}, {{"0", 1, 2}}, 2, cancel,
        accept, {}, error, prepare), "Oversized acceptance footprint was admitted");
    require(!app::run_inference_workers({worker_command(exe, "fast")}, {{"0", 1}}, 2, cancel,
        accept, {}, error, {}, [&](size_t) { ++publications; }), "Publication without preparation was admitted");
    require(accepts == 0 && publications == 0, "Invalid callback admission ran callbacks");
    require(!app::run_inference_workers({worker_command(exe, "hang-next")},
        {{"0", 4, 1}, {"1", 4, 1}}, 5, cancel, accept, {}, error, prepare,
        [&](size_t index) {
            require(index == 0, "Publication exception allowed later work");
            ++publications;
            throw std::runtime_error("publication failed");
        }), "Publication exception reported success");
    require(error == "publication failed" && accepts == 1 && publications == 1,
        "Publication exception lost the first failure or duplicated a callback");
}

void check_published_after_peer_failure(const std::string& exe) {
    std::atomic<bool> cancel{false}, accepting{false}, peer_failed{false};
    std::string error;
    size_t publications = 0;
    require(!app::run_inference_workers({worker_command(exe, "fast"), worker_command(exe, "fast")},
        {{"0", 1}, {"1", 1}}, 2, cancel,
        [&](size_t index, const std::string&, std::string&) {
            require(index == 0, "Peer failure allowed another acceptance");
            accepting = true;
            require(wait_until([&] { return peer_failed.load(); }), "Peer preparation never failed");
            std::this_thread::sleep_for(30ms);
            return true;
        }, {}, error, [&](size_t index, const std::string&, std::string& why) {
            if (index == 0) return true;
            require(wait_until([&] { return accepting.load(); }), "First acceptance never began");
            why = "peer preparation failed";
            peer_failed = true;
            return false;
        }, [&](size_t index) {
            require(index == 0, "Unaccepted output was published");
            ++publications;
        }), "Peer preparation failure reported success");
    require(error == "peer preparation failed" && publications == 1,
        "Peer failure skipped publication for an already accepted result");
}
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
            if (mode.rfind("tracked:", 0) == 0 || mode.rfind("paired:", 0) == 0) {
                const bool paired = mode.rfind("paired:", 0) == 0;
                const std::filesystem::path root = mode.substr(paired ? 7 : 8);
                if (!std::filesystem::create_directory(root / ("received-" + request))) return 9;
                if (paired && (request == "0" || request == "1") &&
                    !wait_until([&] { return std::filesystem::exists(root / "received-0") &&
                        std::filesystem::exists(root / "received-1"); })) return 9;
            }
            if (mode == "hang-next" && request != "0") std::this_thread::sleep_for(30s);
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
        check_pipeline_before_accept(exe);
        check_serial_accepts(exe);
        check_concurrent_prepares(exe);
        check_callback_failures(exe);
        check_acceptance_budget(exe);
        check_cancel_during_prepare(exe);
        check_legacy_sequence(exe);
        check_full_host_default(exe);
        check_publication_failure(exe);
        check_published_after_peer_failure(exe);
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

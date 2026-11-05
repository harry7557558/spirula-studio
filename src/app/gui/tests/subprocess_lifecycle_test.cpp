#include "app/gui/Subprocess.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif
#endif

namespace fs = std::filesystem;
using namespace std::chrono_literals;

namespace {
std::string executable;

long process_id() {
#ifdef _WIN32
    return static_cast<long>(GetCurrentProcessId());
#else
    return static_cast<long>(getpid());
#endif
}

template<class Predicate> bool wait_for(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    do {
        if (predicate()) return true;
        std::this_thread::sleep_for(10ms);
    } while (std::chrono::steady_clock::now() < deadline);
    return predicate();
}

void publish(const fs::path& path) {
    std::ofstream file(path);
    file << process_id() << '\n';
}

long read_pid(const fs::path& path) {
    long pid = 0;
    std::ifstream(path) >> pid;
    return pid;
}

// Native spawning keeps the test supervisor independent of the runner under test.
struct NativeProcess {
#ifdef _WIN32
    HANDLE handle = nullptr;
#else
    pid_t pid = -1;
#endif
    int code = -1;
    bool done = false;

    NativeProcess() = default;
    NativeProcess(const NativeProcess&) = delete;
    NativeProcess& operator=(const NativeProcess&) = delete;

    bool start(const std::vector<std::string>& args) {
#ifdef _WIN32
        std::string command;
        for (const auto& arg : args) {
            if (!command.empty()) command += ' ';
            command += '"';
            size_t slashes = 0;
            for (char c : arg) {
                if (c == '\\') { ++slashes; continue; }
                command.append(slashes * (c == '"' ? 2 : 1), '\\');
                if (c == '"') command += '\\';
                command += c;
                slashes = 0;
            }
            command.append(slashes * 2, '\\');
            command += '"';
        }
        STARTUPINFOA startup{};
        startup.cb = sizeof startup;
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        PROCESS_INFORMATION process{};
        if (!CreateProcessA(nullptr, command.data(), nullptr, nullptr, TRUE,
                            CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
            std::fprintf(stderr, "native fixture launch failed: Windows error %lu\n",
                         static_cast<unsigned long>(GetLastError()));
            return false;
        }
        CloseHandle(process.hThread);
        handle = process.hProcess;
        return true;
#else
        std::vector<char*> argv;
        for (const auto& arg : args) argv.push_back(const_cast<char*>(arg.c_str()));
        argv.push_back(nullptr);
        pid = fork();
        if (pid == 0) {
            execv(argv[0], argv.data());
            _exit(127);
        }
        return pid > 0;
#endif
    }

    bool exited() {
        if (done) return true;
#ifdef _WIN32
        if (!handle) return true;
        if (WaitForSingleObject(handle, 0) != WAIT_OBJECT_0) return false;
        DWORD status = 0;
        GetExitCodeProcess(handle, &status);
        code = static_cast<int>(status);
#else
        if (pid <= 0) return true;
        int status = 0;
        if (waitpid(pid, &status, WNOHANG) != pid) return false;
        code = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
#endif
        done = true;
        return true;
    }

    void terminate() {
        if (exited()) return;
#ifdef _WIN32
        TerminateProcess(handle, 99);
#else
        ::kill(pid, SIGKILL);
#endif
        wait_for([&] { return exited(); });
    }

    ~NativeProcess() {
        terminate();
#ifdef _WIN32
        if (handle) CloseHandle(handle);
#endif
    }
};

bool alive(long pid) {
    if (pid <= 0) return false;
#ifdef _WIN32
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
    if (!process) return false;
    bool running = WaitForSingleObject(process, 0) == WAIT_TIMEOUT;
    CloseHandle(process);
    return running;
#else
    // Linux's subreaper lets the supervisor reap orphaned fixture descendants.
    int status = 0;
    if (waitpid(static_cast<pid_t>(pid), &status, WNOHANG) == pid) return false;
    return ::kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
#endif
}

void cleanup(long pid) {
    if (!alive(pid)) return;
#ifdef _WIN32
    HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE,
                                 static_cast<DWORD>(pid));
    if (process) {
        TerminateProcess(process, 99);
        WaitForSingleObject(process, 5000);
        CloseHandle(process);
    }
#else
    ::kill(static_cast<pid_t>(pid), SIGKILL);
    wait_for([&] { return !alive(pid); });
#endif
}

// Exclusive resources prove shutdown without mistaking unreaped zombies for work.
struct FixtureResource {
#ifdef _WIN32
    HANDLE handle = INVALID_HANDLE_VALUE;
#else
    int descriptor = -1;
#endif
    FixtureResource() = default;
    FixtureResource(const FixtureResource&) = delete;
    FixtureResource& operator=(const FixtureResource&) = delete;

    bool acquire(const fs::path& path, bool create = true) {
#ifdef _WIN32
        handle = CreateFileA(path.string().c_str(), GENERIC_READ | GENERIC_WRITE,
                             0, nullptr, create ? OPEN_ALWAYS : OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, nullptr);
        return handle != INVALID_HANDLE_VALUE;
#else
        descriptor = open(path.c_str(), O_RDWR | O_CLOEXEC | (create ? O_CREAT : 0), 0600);
        return descriptor >= 0 && flock(descriptor, LOCK_EX | LOCK_NB) == 0;
#endif
    }

    ~FixtureResource() {
#ifdef _WIN32
        if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
#else
        if (descriptor >= 0) close(descriptor);
#endif
    }
};

bool resources_released(const fs::path& directory) {
    for (const std::string name : {"tree", "leaf", "chain"}) {
        if (name == "chain" && read_pid(directory / "chain") == 0) continue;
        FixtureResource resource;
        if (!resource.acquire(directory / (name + ".lock"), false)) {
#ifdef _WIN32
            const DWORD error = GetLastError();
#endif
            std::fprintf(stderr, "%s resource still owned at API return\n", name.c_str());
#ifdef _WIN32
            std::fprintf(stderr, "Windows error %lu, fixture running=%d\n",
                         static_cast<unsigned long>(error),
                         alive(read_pid(directory / name)) ? 1 : 0);
#endif
            return false;
        }
    }
    return true;
}

int helper(const std::string& mode, const fs::path& directory) {
    FixtureResource resource;
    const std::string resource_name = mode == "leaf" || mode == "leaf-closed" ? "leaf" :
        mode == "chain" ? "chain" : "tree";
    if ((mode == "leaf" || mode == "leaf-closed" || mode == "chain" ||
         mode == "tree" || mode == "orphan" || mode == "closed") &&
        !resource.acquire(directory / (resource_name + ".lock"))) return 93;
    if (mode == "leaf" || mode == "leaf-closed") {
        if (mode == "leaf-closed") {
            std::fclose(stdout);
            std::fclose(stderr);
        }
        publish(directory / "leaf");
    } else if (mode == "chain") {
        publish(directory / "chain");
        std::atomic<bool> cancel{false};
        return gui::run_process({executable, "tree", directory.string()}, "",
            [](const std::string& line) {
                std::puts(line.c_str());
                std::fflush(stdout);
            }, cancel);
    } else if (mode == "tree" || mode == "orphan" || mode == "closed") {
        NativeProcess leaf;
        if (!leaf.start({executable, mode == "closed" ? "leaf-closed" : "leaf",
                         directory.string()})) return 90;
        if (!wait_for([&] { return read_pid(directory / "leaf") > 0; })) return 91;
        publish(directory / "tree");
        std::puts("ready");
        std::fflush(stdout);
        if (mode == "orphan") std::_Exit(0);
        if (mode == "closed") {
            std::fclose(stdout);
            std::fclose(stderr);
            publish(directory / "closed");
        }
        for (;;) std::this_thread::sleep_for(1s);
    } else if (mode == "normal") {
        std::puts("stdout line");
        std::fflush(stdout);
        std::fputs("stderr tail", stderr);
        return 7;
    } else if (mode == "consume") {
        std::string input((std::istreambuf_iterator<char>(std::cin)), {});
        std::puts(input == "payload" ? "accepted" : "incomplete");
        return input == "payload" ? 0 : 9;
    } else if (mode == "produce") {
        std::fputs("payload", stdout);
        std::fputs("diagnostic\n", stderr);
        return 7;
    } else if (mode == "duplex") {
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
#endif
        char message[4];
        while (std::fread(message, 1, sizeof message, stdin) == sizeof message) {
            std::fputs("diagnostic\n", stderr);
            std::fflush(stderr);
            if (std::fwrite(message, 1, sizeof message, stdout) != sizeof message) return 8;
            std::fflush(stdout);
        }
        std::fputs("stderr tail", stderr);
        return 7;
    } else if (mode == "duplex-drain") {
        while (std::getchar() != EOF) {}
        std::string output(2 << 20, 'x');
        return std::fwrite(output.data(), 1, output.size(), stdout) == output.size() ? 0 : 8;
    } else if (mode != "idle") {
        return 92;
    }
    for (;;) std::this_thread::sleep_for(1s);
}

int normal_cases() {
    std::atomic<bool> cancel{false};
    std::vector<std::string> lines;
    const int status = gui::run_process({executable, "normal"}, "",
        [&](const std::string& line) { lines.push_back(line); }, cancel);
    bool stdout_seen = false, stderr_seen = false;
    for (const auto& line : lines) {
        stdout_seen |= line == "stdout line";
        stderr_seen |= line == "stderr tail";
    }
    if (status != 7 || !stdout_seen || !stderr_seen) return 1;
    gui::ProcessPipe pipe;
    bool accepted = false;
    if (!pipe.start({executable, "consume"}, [&](const std::string& line) {
            if (line == "accepted") accepted = true;
        })) return 2;
    if (!pipe.write("payload", 7) || pipe.finish() != 0 || !accepted) return 3;
    gui::ProcessReader reader;
    bool diagnostic = false;
    if (!reader.start({executable, "produce"}, [&](const std::string& line) {
            if (line == "diagnostic") diagnostic = true;
        })) return 4;
    std::string output;
    char bytes[3];
    size_t count = 0;
    while ((count = reader.read(bytes, sizeof bytes))) output.append(bytes, count);
    if (reader.finish() != 7 || output != "payload" || !diagnostic) return 5;
#ifndef _WIN32
    std::signal(SIGPIPE, SIG_DFL);
#endif
    gui::ProcessDuplex duplex;
    std::vector<std::string> diagnostics;
    if (duplex.start({}, {}) || duplex.finish() != gui::kSpawnFailed) return 6;
    if (!duplex.start({executable, "duplex"}, [&](const std::string& line) {
            diagnostics.push_back(line);
        })) return 7;
    const std::string payload("a\0\r\n", 4);
    for (int request = 0; request < 3; ++request) {
        if (!duplex.write(payload.data(), payload.size())) return 8;
        std::string response;
        while (response.size() < payload.size()) {
            count = duplex.read(bytes, sizeof bytes);
            if (!count) return 9;
            response.append(bytes, count);
        }
        if (response != payload) return 10;
    }
    if (duplex.finish() != 7 ||
        diagnostics != std::vector<std::string>{"diagnostic", "diagnostic",
                                                "diagnostic", "stderr tail"}) return 11;
    if (duplex.read(bytes, sizeof bytes) || duplex.write("x", 1)) return 12;
    if (!duplex.start({executable, "duplex-drain"}, {}) || duplex.finish() != 0) return 13;
    if (!duplex.start({executable, "produce"}, {})) return 14;
    output.clear();
    while ((count = duplex.read(bytes, sizeof bytes))) output.append(bytes, count);
    if (output != "payload" || duplex.write("x", 1) || duplex.finish() != 7) return 15;
    if (duplex.start({"spirula-intentionally-missing-duplex-executable"}, {}) &&
        duplex.finish() != gui::kSpawnFailed) return 16;
#ifndef _WIN32
    struct sigaction disposition{};
    if (sigaction(SIGPIPE, nullptr, &disposition) != 0 ||
        disposition.sa_handler != SIG_DFL) return 17;
#endif
    return 0;
}

int runner(const std::string& api, const std::string& action, const fs::path& directory) {
    const bool death = action == "death" || action == "chain-death";
    const std::string mode = action == "exit" ? "orphan" :
        action == "closed" ? "closed" :
        action == "chain-cancel" || action == "chain-death" ? "chain" : "tree";
    const std::vector<std::string> command{executable, mode, directory.string()};
    std::atomic<bool> cancel{false};
    if (api == "run") {
        std::thread closer;
        bool closed_ready = false;
        if (action == "closed") closer = std::thread([&] {
            closed_ready = wait_for([&] { return read_pid(directory / "closed") > 0; });
            cancel.store(true);
        });
        const int result = gui::run_process(command, "", [&](const std::string& line) {
            if (line == "ready" && !death && action != "exit" && action != "closed")
                cancel.store(true);
        }, cancel);
        const bool released = resources_released(directory);
        if (closer.joinable()) closer.join();
        if (action == "closed" && !closed_ready) return 18;
        if (!released) return 19;
        return result == (action == "exit" ? 0 : gui::kCancelled) ? 0 : 10;
    }
    if (api == "duplex" &&
        (action == "blocked" || action == "destructor")) {
        auto process = std::make_unique<gui::ProcessDuplex>();
        if (!process->start(command, {})) return 20;
        if (!wait_for([&] { return read_pid(directory / "tree") > 0; })) return 21;
        if (action == "destructor") {
            process.reset();
            return resources_released(directory) ? 0 : 22;
        }
        std::string ready;
        char bytes[6];
        while (ready.size() < 6) {
            const size_t got = process->read(bytes, 6 - ready.size());
            if (!got) return 23;
            ready.append(bytes, got);
        }
        if (ready != "ready\n") {
#ifdef _WIN32
            // Existing tree fixture writes text-mode CRLF.
            if (ready != "ready\r") return 24;
            if (process->read(bytes, 1) != 1 || bytes[0] != '\n') return 24;
#else
            return 24;
#endif
        }
        std::atomic<int> entered{0}, completed{0};
        size_t got = 1;
        bool wrote = true;
        std::thread reader([&] {
            entered.fetch_add(1);
            got = process->read(bytes, sizeof bytes);
            completed.fetch_add(1);
        });
        const std::string large(4 << 20, 'x');
        std::thread writer([&] {
            entered.fetch_add(1);
            wrote = process->write(large.data(), large.size());
            completed.fetch_add(1);
        });
        const bool started = wait_for([&] { return entered.load() == 2; });
        std::this_thread::sleep_for(50ms);
        const bool blocked = completed.load() == 0;
        process->kill();
        reader.join();
        writer.join();
        const int result = process->finish();
        return started && blocked && got == 0 && !wrote && result != 0 &&
            resources_released(directory) ? 0 : 25;
    }
    auto exercise = [&](auto& process) {
        if (!process.start(command, {})) return 11;
        if (!wait_for([&] { return read_pid(directory / "tree") > 0; })) return 12;
        if (death) {
            for (;;) std::this_thread::sleep_for(1s);
        }
        if (action == "exit") {
            const int result = process.finish();
            return result == 0 && resources_released(directory) ? 0 : 14;
        }
        if (action == "closed" &&
            !wait_for([&] { return read_pid(directory / "closed") > 0; })) return 15;
        process.kill();
        const int result = process.finish();
        return result != 0 && resources_released(directory) ? 0 : 13;
    };
    if (api == "pipe") {
        gui::ProcessPipe process;
        return exercise(process);
    }
    if (api == "duplex") {
        gui::ProcessDuplex process;
        return exercise(process);
    }
    gui::ProcessReader process;
    if (action == "exit") {
        if (!process.start(command, {})) return 16;
        char bytes[32];
        while (process.read(bytes, sizeof bytes)) {}
        const int result = process.finish();
        return result == 0 && resources_released(directory) ? 0 : 17;
    }
    return exercise(process);
}

int suite() {
#ifdef __linux__
    if (prctl(PR_SET_CHILD_SUBREAPER, 1) != 0) {
        std::perror("PR_SET_CHILD_SUBREAPER");
        return 1;
    }
#endif
    const fs::path root = fs::temp_directory_path() /
        ("spirula subprocess " + std::to_string(process_id()));
    if (!fs::create_directory(root)) {
        std::fprintf(stderr, "fixture directory already exists: %s\n", root.string().c_str());
        return 1;
    }
    int failures = 0;
    auto check = [&](bool ok, const std::string& description) {
        std::printf("%s %s\n", ok ? "ok" : "FAIL", description.c_str());
        if (!ok) ++failures;
    };
    NativeProcess unrelated;
    if (!unrelated.start({executable, "idle"})) {
        fs::remove_all(root);
        return 1;
    }
    {
        NativeProcess normal;
        const bool started = normal.start({executable, "check-normal"});
        check(started && wait_for([&] { return normal.exited(); }) && normal.code == 0,
              "normal completion, exact output, stdin EOF and nonzero exit preservation");
    }
    for (const std::string api : {"run", "pipe", "reader", "duplex"}) {
        for (const std::string action : {"cancel", "death", "chain-cancel",
                                         "chain-death", "exit", "closed",
                                         "blocked", "destructor"}) {
            if (api != "duplex" && (action == "blocked" || action == "destructor")) continue;
            const auto directory = root / (api + "-" + action);
            fs::create_directory(directory);
            NativeProcess owner;
            const bool started = owner.start({executable, "runner", api, action,
                                               directory.string()});
            const bool ready = started && wait_for([&] {
                return read_pid(directory / "tree") > 0 && read_pid(directory / "leaf") > 0;
            });
            check(ready, api + " " + action + " descendant readiness");
            const long tree = read_pid(directory / "tree");
            const long leaf = read_pid(directory / "leaf");
            const long chain = read_pid(directory / "chain");
            if (action == "death" || action == "chain-death") {
                check(started && !owner.exited(), api + " owner alive before abrupt death");
                owner.terminate();
            }
            else check(started && wait_for([&] { return owner.exited(); }) && owner.code == 0,
                       api + " " + action + " returns the expected exit status without hanging");
            if (action == "death" || action == "chain-death") {
                check(ready && wait_for([&] {
                    return !alive(tree) && !alive(leaf) && !alive(chain);
                }), api + " " + action + " terminates child and descendant");
            } else {
                check(ready && resources_released(directory),
                      api + " " + action + " releases descendant resources before API return");
            }
            check(!unrelated.exited(), api + " " + action + " preserves unrelated process");
            owner.terminate();
            cleanup(leaf);
            cleanup(tree);
            cleanup(chain);
        }
    }
    fs::remove_all(root);
    return failures == 0 ? 0 : 1;
}
}  // namespace

int main(int argc, char** argv) {
    executable = fs::absolute(argv[0]).string();
    if (argc == 1) return suite();
    const std::string mode = argv[1];
    if (mode == "check-normal") return normal_cases();
    if (mode == "runner" && argc == 5) return runner(argv[2], argv[3], argv[4]);
    return helper(mode, argc > 2 ? fs::path(argv[2]) : fs::path{});
}

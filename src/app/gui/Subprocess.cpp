// Subprocess.cpp -- see Subprocess.h.

#include "app/gui/Subprocess.h"

#include <cctype>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX   // keep windows.h from defining min()/max() macros
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdlib>
#endif

namespace fs = std::filesystem;

namespace gui {

namespace {

// Feed a chunk into the line splitter. Both '\n' and a bare '\r' end a line
// -- ffmpeg/curl emit progress as '\r'-terminated updates that would
// otherwise never surface in the log (and look like a hang).
void emit_lines(std::string& acc, const char* buf, size_t n,
                const std::function<void(const std::string&)>& on_line) {
    acc.append(buf, n);
    size_t pos = 0, nl;
    while ((nl = acc.find_first_of("\r\n", pos)) != std::string::npos) {
        if (acc[nl] == '\r' && nl + 1 >= acc.size())
            break;   // might be the first half of a \r\n; wait for more
        size_t next = nl + 1;
        if (acc[nl] == '\r' && acc[next] == '\n') next++;
        if (nl > pos && on_line) on_line(acc.substr(pos, nl - pos));
        pos = next;
    }
    acc.erase(0, pos);
}

void emit_tail(std::string& acc, const std::function<void(const std::string&)>& on_line) {
    if (!acc.empty() && on_line) on_line(acc);
    acc.clear();
}

}  // namespace


#ifdef _WIN32

// ---------------------------------------------------------------------------
// Windows: CreateProcess + anonymous pipe, PeekNamedPipe polling.
// ---------------------------------------------------------------------------

namespace {

// Quote one argument per MSVCRT rules.
std::string quote_arg(const std::string& a) {
    if (!a.empty() && a.find_first_of(" \t\"") == std::string::npos) return a;
    std::string out = "\"";
    size_t bs = 0;
    for (char c : a) {
        if (c == '\\') { bs++; continue; }
        if (c == '"') { out.append(bs * 2 + 1, '\\'); out += '"'; bs = 0; continue; }
        out.append(bs, '\\'); bs = 0;
        out += c;
    }
    out.append(bs * 2, '\\');
    out += '"';
    return out;
}

}  // namespace

int run_process(const std::vector<std::string>& argv,
                const std::string& cwd,
                const std::function<void(const std::string&)>& on_line,
                const std::atomic<bool>& cancel) {
    if (argv.empty()) return kSpawnFailed;
    std::string cmdline;
    for (size_t i = 0; i < argv.size(); i++)
        cmdline += (i ? " " : "") + quote_arg(argv[i]);

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof sa;
    sa.bInheritHandle = TRUE;
    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return kSpawnFailed;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = wr;
    si.hStdError  = wr;
    si.hStdInput  = INVALID_HANDLE_VALUE;
    PROCESS_INFORMATION pi{};
    std::vector<char> cmd(cmdline.begin(), cmdline.end());
    cmd.push_back('\0');
    BOOL ok = CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW, nullptr,
                             cwd.empty() ? nullptr : cwd.c_str(), &si, &pi);
    CloseHandle(wr);
    if (!ok) { CloseHandle(rd); return kSpawnFailed; }
    CloseHandle(pi.hThread);

    std::string acc;
    char buf[4096];
    bool killed = false;
    for (;;) {
        if (cancel.load() && !killed) {
            TerminateProcess(pi.hProcess, 1);
            killed = true;
        }
        DWORD avail = 0;
        if (!PeekNamedPipe(rd, nullptr, 0, nullptr, &avail, nullptr)) break;
        if (avail > 0) {
            DWORD got = 0;
            if (!ReadFile(rd, buf, (DWORD)std::min<size_t>(sizeof buf, avail), &got, nullptr) || !got)
                break;
            emit_lines(acc, buf, got, on_line);
        } else {
            if (WaitForSingleObject(pi.hProcess, 100) == WAIT_OBJECT_0) {
                // Drain whatever is left.
                while (PeekNamedPipe(rd, nullptr, 0, nullptr, &avail, nullptr) && avail) {
                    DWORD got = 0;
                    if (!ReadFile(rd, buf, (DWORD)std::min<size_t>(sizeof buf, avail), &got, nullptr) || !got)
                        break;
                    emit_lines(acc, buf, got, on_line);
                }
                break;
            }
        }
    }
    emit_tail(acc, on_line);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(rd);
    return killed ? kCancelled : (int)code;
}


struct ProcessPipe::Impl {
    HANDLE process = nullptr;
    HANDLE in_wr = nullptr;
    HANDLE out_rd = nullptr;
    std::thread reader;
    std::function<void(const std::string&)> on_line;
};

bool ProcessPipe::start(const std::vector<std::string>& argv,
                        std::function<void(const std::string&)> on_line) {
    if (argv.empty() || _impl->process) return false;
    std::string cmdline;
    for (size_t i = 0; i < argv.size(); i++)
        cmdline += (i ? " " : "") + quote_arg(argv[i]);
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof sa;
    sa.bInheritHandle = TRUE;
    HANDLE in_rd = nullptr, out_wr = nullptr;
    if (!CreatePipe(&in_rd, &_impl->in_wr, &sa, 1 << 20)) return false;
    if (!CreatePipe(&_impl->out_rd, &out_wr, &sa, 0)) {
        CloseHandle(in_rd);
        CloseHandle(_impl->in_wr);
        _impl->in_wr = nullptr;
        return false;
    }
    SetHandleInformation(_impl->in_wr, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(_impl->out_rd, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOA si{};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = in_rd;
    si.hStdOutput = out_wr;
    si.hStdError = out_wr;
    PROCESS_INFORMATION pi{};
    std::vector<char> cmd(cmdline.begin(), cmdline.end());
    cmd.push_back('\0');
    // An encoder saturates every core; the window showing its progress
    // must still draw, so it runs a notch below.
    const BOOL ok = CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, TRUE,
                                   CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS, nullptr,
                                   nullptr, &si, &pi);
    CloseHandle(in_rd);
    CloseHandle(out_wr);
    if (!ok) {
        CloseHandle(_impl->in_wr);
        CloseHandle(_impl->out_rd);
        _impl->in_wr = _impl->out_rd = nullptr;
        return false;
    }
    CloseHandle(pi.hThread);
    _impl->process = pi.hProcess;
    _impl->on_line = std::move(on_line);
    Impl* im = _impl.get();
    _impl->reader = std::thread([im] {
        std::string acc;
        char buf[4096];
        DWORD got = 0;
        while (ReadFile(im->out_rd, buf, sizeof buf, &got, nullptr) && got)
            emit_lines(acc, buf, got, im->on_line);
        emit_tail(acc, im->on_line);
    });
    return true;
}

bool ProcessPipe::write(const void* data, size_t bytes) {
    if (!_impl->in_wr) return false;
    const char* p = (const char*)data;
    while (bytes > 0) {
        DWORD put = 0;
        const DWORD chunk = (DWORD)std::min<size_t>(bytes, 1 << 24);
        if (!WriteFile(_impl->in_wr, p, chunk, &put, nullptr) || !put) return false;
        p += put;
        bytes -= put;
    }
    return true;
}

int ProcessPipe::finish() {
    if (!_impl->process) return kSpawnFailed;
    if (_impl->in_wr) { CloseHandle(_impl->in_wr); _impl->in_wr = nullptr; }
    WaitForSingleObject(_impl->process, INFINITE);
    if (_impl->reader.joinable()) _impl->reader.join();
    DWORD code = 1;
    GetExitCodeProcess(_impl->process, &code);
    CloseHandle(_impl->process);
    CloseHandle(_impl->out_rd);
    _impl->process = _impl->out_rd = nullptr;
    return (int)code;
}

void ProcessPipe::kill() {
    if (_impl->process) TerminateProcess(_impl->process, 1);
}

ProcessPipe::~ProcessPipe() {
    if (_impl->process) {
        kill();
        finish();
    }
}


struct ProcessReader::Impl {
    HANDLE process = nullptr;
    HANDLE out_rd = nullptr, err_rd = nullptr;
    std::thread reader;
    std::function<void(const std::string&)> on_line;
};

bool ProcessReader::start(const std::vector<std::string>& argv,
                          std::function<void(const std::string&)> on_line) {
    if (argv.empty() || _impl->process) return false;
    std::string cmdline;
    for (size_t i = 0; i < argv.size(); i++)
        cmdline += (i ? " " : "") + quote_arg(argv[i]);
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof sa;
    sa.bInheritHandle = TRUE;
    HANDLE out_wr = nullptr, err_wr = nullptr;
    if (!CreatePipe(&_impl->out_rd, &out_wr, &sa, 1 << 20)) return false;
    if (!CreatePipe(&_impl->err_rd, &err_wr, &sa, 0)) {
        CloseHandle(_impl->out_rd);
        CloseHandle(out_wr);
        _impl->out_rd = nullptr;
        return false;
    }
    SetHandleInformation(_impl->out_rd, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(_impl->err_rd, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOA si{};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = INVALID_HANDLE_VALUE;
    si.hStdOutput = out_wr;
    si.hStdError = err_wr;
    PROCESS_INFORMATION pi{};
    std::vector<char> cmd(cmdline.begin(), cmdline.end());
    cmd.push_back('\0');
    const BOOL ok = CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, TRUE,
                                   CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(out_wr);
    CloseHandle(err_wr);
    if (!ok) {
        CloseHandle(_impl->out_rd);
        CloseHandle(_impl->err_rd);
        _impl->out_rd = _impl->err_rd = nullptr;
        return false;
    }
    CloseHandle(pi.hThread);
    _impl->process = pi.hProcess;
    _impl->on_line = std::move(on_line);
    Impl* im = _impl.get();
    _impl->reader = std::thread([im] {
        std::string acc;
        char buf[4096];
        DWORD got = 0;
        while (ReadFile(im->err_rd, buf, sizeof buf, &got, nullptr) && got)
            emit_lines(acc, buf, got, im->on_line);
        emit_tail(acc, im->on_line);
    });
    return true;
}

size_t ProcessReader::read(void* data, size_t bytes) {
    if (!_impl->out_rd) return 0;
    DWORD got = 0;
    const DWORD want = (DWORD)std::min<size_t>(bytes, 1 << 24);
    if (!ReadFile(_impl->out_rd, data, want, &got, nullptr)) return 0;
    return got;
}

int ProcessReader::finish() {
    if (!_impl->process) return kSpawnFailed;
    // Closed first: a child still writing then fails instead of blocking.
    if (_impl->out_rd) { CloseHandle(_impl->out_rd); _impl->out_rd = nullptr; }
    WaitForSingleObject(_impl->process, INFINITE);
    if (_impl->reader.joinable()) _impl->reader.join();
    DWORD code = 1;
    GetExitCodeProcess(_impl->process, &code);
    CloseHandle(_impl->process);
    CloseHandle(_impl->err_rd);
    _impl->process = _impl->err_rd = nullptr;
    return (int)code;
}

void ProcessReader::kill() {
    if (_impl->process) TerminateProcess(_impl->process, 1);
}

ProcessReader::~ProcessReader() {
    if (_impl->process) {
        kill();
        finish();
    }
}


bool command_exists(const std::string& exe) {
    if (exe.find('\\') != std::string::npos || exe.find('/') != std::string::npos) {
        std::error_code ec;
        return fs::exists(exe, ec);
    }
    char found[MAX_PATH];
    return SearchPathA(nullptr, exe.c_str(), ".exe", MAX_PATH, found, nullptr) > 0;
}

bool open_url(const std::string& url) {
    // Not ShellExecute: it lives in shell32, and this is the only call in the
    // program that would need it. `cmd /c start` reaches the same handler.
    // The empty "" is start's window-title argument -- without it, a quoted
    // URL becomes the title and nothing opens.
    std::string cmd = "cmd /c start \"\" \"" + url + "\"";
    STARTUPINFOA si{};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        return false;
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

#else

// ---------------------------------------------------------------------------
// POSIX: fork/execvp + pipe, poll(2), process-group kill on cancel.
// ---------------------------------------------------------------------------

int run_process(const std::vector<std::string>& argv,
                const std::string& cwd,
                const std::function<void(const std::string&)>& on_line,
                const std::atomic<bool>& cancel) {
    if (argv.empty()) return kSpawnFailed;
    int fds[2];
    if (pipe(fds) != 0) return kSpawnFailed;

    pid_t pid = fork();
    if (pid < 0) { close(fds[0]); close(fds[1]); return kSpawnFailed; }
    if (pid == 0) {
        // Child: own process group so cancel kills helpers too.
        setpgid(0, 0);
        dup2(fds[1], STDOUT_FILENO);
        dup2(fds[1], STDERR_FILENO);
        close(fds[0]);
        close(fds[1]);
        if (!cwd.empty() && chdir(cwd.c_str()) != 0) _exit(127);
        std::vector<char*> args;
        for (const auto& a : argv) args.push_back(const_cast<char*>(a.c_str()));
        args.push_back(nullptr);
        execvp(args[0], args.data());
        _exit(127);
    }
    close(fds[1]);

    std::string acc;
    char buf[4096];
    bool killed = false;
    for (;;) {
        if (cancel.load() && !killed) {
            kill(-pid, SIGKILL);
            killed = true;
        }
        struct pollfd pfd{fds[0], POLLIN, 0};
        int pr = poll(&pfd, 1, 100);
        if (pr > 0) {
            ssize_t got = read(fds[0], buf, sizeof buf);
            if (got <= 0) break;   // EOF or error: child exited
            emit_lines(acc, buf, (size_t)got, on_line);
        } else if (pr < 0 && errno != EINTR) {
            break;
        }
    }
    emit_tail(acc, on_line);
    close(fds[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    if (killed) return kCancelled;
    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        return code == 127 ? kSpawnFailed : code;
    }
    return 1;
}

struct ProcessPipe::Impl {
    pid_t pid = -1;
    int in_fd = -1, out_fd = -1;
    std::thread reader;
    std::function<void(const std::string&)> on_line;
};

bool ProcessPipe::start(const std::vector<std::string>& argv,
                        std::function<void(const std::string&)> on_line) {
    if (argv.empty() || _impl->pid > 0) return false;
    // A child that dies mid-frame must fail the write, not kill this process.
    signal(SIGPIPE, SIG_IGN);
    int in[2], out[2];
    if (pipe(in) != 0) return false;
    if (pipe(out) != 0) { close(in[0]); close(in[1]); return false; }
    const pid_t pid = fork();
    if (pid < 0) {
        close(in[0]); close(in[1]); close(out[0]); close(out[1]);
        return false;
    }
    if (pid == 0) {
        setpgid(0, 0);
        // Below the window, as on Windows.
        if (nice(10) == -1) {}
        dup2(in[0], STDIN_FILENO);
        dup2(out[1], STDOUT_FILENO);
        dup2(out[1], STDERR_FILENO);
        close(in[0]); close(in[1]); close(out[0]); close(out[1]);
        std::vector<char*> args;
        for (const auto& a : argv) args.push_back(const_cast<char*>(a.c_str()));
        args.push_back(nullptr);
        execvp(args[0], args.data());
        _exit(127);
    }
    close(in[0]);
    close(out[1]);
    fcntl(in[1], F_SETFD, FD_CLOEXEC);
    fcntl(out[0], F_SETFD, FD_CLOEXEC);
    _impl->pid = pid;
    _impl->in_fd = in[1];
    _impl->out_fd = out[0];
    _impl->on_line = std::move(on_line);
    Impl* im = _impl.get();
    _impl->reader = std::thread([im] {
        std::string acc;
        char buf[4096];
        for (;;) {
            const ssize_t got = read(im->out_fd, buf, sizeof buf);
            if (got < 0 && errno == EINTR) continue;
            if (got <= 0) break;
            emit_lines(acc, buf, (size_t)got, im->on_line);
        }
        emit_tail(acc, im->on_line);
    });
    return true;
}

bool ProcessPipe::write(const void* data, size_t bytes) {
    if (_impl->in_fd < 0) return false;
    const char* p = (const char*)data;
    while (bytes > 0) {
        const ssize_t put = ::write(_impl->in_fd, p, bytes);
        if (put < 0 && errno == EINTR) continue;
        if (put <= 0) return false;
        p += put;
        bytes -= (size_t)put;
    }
    return true;
}

int ProcessPipe::finish() {
    if (_impl->pid <= 0) return kSpawnFailed;
    if (_impl->in_fd >= 0) { close(_impl->in_fd); _impl->in_fd = -1; }
    int status = 0;
    while (waitpid(_impl->pid, &status, 0) < 0 && errno == EINTR) {}
    if (_impl->reader.joinable()) _impl->reader.join();
    close(_impl->out_fd);
    _impl->out_fd = -1;
    _impl->pid = -1;
    if (WIFEXITED(status)) {
        const int code = WEXITSTATUS(status);
        return code == 127 ? kSpawnFailed : code;
    }
    return 1;
}

void ProcessPipe::kill() {
    if (_impl->pid > 0) ::kill(-_impl->pid, SIGKILL);
}

ProcessPipe::~ProcessPipe() {
    if (_impl->pid > 0) {
        kill();
        finish();
    }
}


struct ProcessReader::Impl {
    pid_t pid = -1;
    int out_fd = -1, err_fd = -1;
    std::thread reader;
    std::function<void(const std::string&)> on_line;
};

bool ProcessReader::start(const std::vector<std::string>& argv,
                          std::function<void(const std::string&)> on_line) {
    if (argv.empty() || _impl->pid > 0) return false;
    int out[2], err[2];
    if (pipe(out) != 0) return false;
    if (pipe(err) != 0) { close(out[0]); close(out[1]); return false; }
    const pid_t pid = fork();
    if (pid < 0) {
        close(out[0]); close(out[1]); close(err[0]); close(err[1]);
        return false;
    }
    if (pid == 0) {
        setpgid(0, 0);
        const int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) dup2(devnull, STDIN_FILENO);
        dup2(out[1], STDOUT_FILENO);
        dup2(err[1], STDERR_FILENO);
        close(out[0]); close(out[1]); close(err[0]); close(err[1]);
        std::vector<char*> args;
        for (const auto& a : argv) args.push_back(const_cast<char*>(a.c_str()));
        args.push_back(nullptr);
        execvp(args[0], args.data());
        _exit(127);
    }
    close(out[1]);
    close(err[1]);
    fcntl(out[0], F_SETFD, FD_CLOEXEC);
    fcntl(err[0], F_SETFD, FD_CLOEXEC);
    _impl->pid = pid;
    _impl->out_fd = out[0];
    _impl->err_fd = err[0];
    _impl->on_line = std::move(on_line);
    Impl* im = _impl.get();
    _impl->reader = std::thread([im] {
        std::string acc;
        char buf[4096];
        for (;;) {
            const ssize_t got = ::read(im->err_fd, buf, sizeof buf);
            if (got < 0 && errno == EINTR) continue;
            if (got <= 0) break;
            emit_lines(acc, buf, (size_t)got, im->on_line);
        }
        emit_tail(acc, im->on_line);
    });
    return true;
}

size_t ProcessReader::read(void* data, size_t bytes) {
    if (_impl->out_fd < 0) return 0;
    for (;;) {
        const ssize_t got = ::read(_impl->out_fd, data, bytes);
        if (got < 0 && errno == EINTR) continue;
        return got > 0 ? (size_t)got : 0;
    }
}

int ProcessReader::finish() {
    if (_impl->pid <= 0) return kSpawnFailed;
    // Closed first: a child still writing then fails instead of blocking.
    if (_impl->out_fd >= 0) { close(_impl->out_fd); _impl->out_fd = -1; }
    int status = 0;
    while (waitpid(_impl->pid, &status, 0) < 0 && errno == EINTR) {}
    if (_impl->reader.joinable()) _impl->reader.join();
    close(_impl->err_fd);
    _impl->err_fd = -1;
    _impl->pid = -1;
    if (WIFEXITED(status)) {
        const int code = WEXITSTATUS(status);
        return code == 127 ? kSpawnFailed : code;
    }
    return 1;
}

void ProcessReader::kill() {
    if (_impl->pid > 0) ::kill(-_impl->pid, SIGKILL);
}

ProcessReader::~ProcessReader() {
    if (_impl->pid > 0) {
        kill();
        finish();
    }
}


bool command_exists(const std::string& exe) {
    if (exe.find('/') != std::string::npos)
        return access(exe.c_str(), X_OK) == 0;
    const char* path = std::getenv("PATH");
    if (!path) return false;
    std::string p = path;
    size_t pos = 0;
    while (pos <= p.size()) {
        size_t colon = p.find(':', pos);
        std::string dir = p.substr(pos, colon == std::string::npos
                                            ? std::string::npos : colon - pos);
        if (!dir.empty() && access((dir + "/" + exe).c_str(), X_OK) == 0)
            return true;
        if (colon == std::string::npos) break;
        pos = colon + 1;
    }
    return false;
}

bool open_url(const std::string& url) {
#ifdef __APPLE__
    const char* openers[] = {"open"};
#else
    // xdg-open covers every desktop that follows the freedesktop spec; the
    // other two are for the sessions that ship one but not it.
    const char* openers[] = {"xdg-open", "gio", "x-www-browser"};
#endif
    for (const char* opener : openers) {
        if (!command_exists(opener)) continue;
        const pid_t pid = fork();
        if (pid < 0) return false;
        if (pid == 0) {
            // Double fork so the browser is reparented to init and this
            // process never has to reap it -- the GUI has no SIGCHLD handler
            // and would otherwise accumulate zombies.
            if (fork() == 0) {
                setsid();
                // Nothing here should write over the GUI's own stdout.
                int devnull = open("/dev/null", O_RDWR);
                if (devnull >= 0) {
                    dup2(devnull, STDIN_FILENO);
                    dup2(devnull, STDOUT_FILENO);
                    dup2(devnull, STDERR_FILENO);
                    if (devnull > STDERR_FILENO) close(devnull);
                }
                if (std::strcmp(opener, "gio") == 0)
                    execlp(opener, opener, "open", url.c_str(), (char*)nullptr);
                else
                    execlp(opener, opener, url.c_str(), (char*)nullptr);
            }
            _exit(0);
        }
        int status = 0;
        waitpid(pid, &status, 0);
        return true;
    }
    return false;
}

#endif

ProcessPipe::ProcessPipe() : _impl(new Impl) {}
ProcessReader::ProcessReader() : _impl(new Impl) {}

// Platform-independent: argv assembly is the same everywhere.
std::vector<std::string> split_args(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    bool in_quote = false, have = false;
    char quote = 0;
    for (size_t i = 0; i < s.size(); i++) {
        const char c = s[i];
        if (in_quote) {
            if (c == quote) in_quote = false;
            else cur += c;
            continue;
        }
        // A line continuation, the way a pasted shell command writes one:
        // neither the backslash nor the break is part of an argument. Any
        // other backslash is a Windows path and stays.
        if (c == '\\') {
            size_t j = i + 1;
            if (j < s.size() && s[j] == '\r') j++;
            if (j < s.size() && s[j] == '\n') { i = j; continue; }
        }
        if (c == '"' || c == '\'') {
            in_quote = true;
            quote = c;
            have = true;
        } else if (std::isspace((unsigned char)c)) {
            if (have) { out.push_back(cur); cur.clear(); have = false; }
        } else {
            cur += c;
            have = true;
        }
    }
    if (have) out.push_back(cur);
    return out;
}


std::string safe_arg(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (unsigned char c : text) {
        // A straight quote ends a JSON string and groups words for a shell;
        // the typographic ones read the same and mean nothing to either.
        if (c == '"') out += "”";
        else if (c == '\'' || c == '`') out += "’";
        // Backslash is JSON's escape character, and a path survives as '/'.
        else if (c == '\\') out += '/';
        else if (c < 0x20 || c == 0x7F) out += ' ';   // would end the line
        else out += (char)c;
    }
    std::string tidy;
    tidy.reserve(out.size());
    for (char c : out)
        if (c != ' ' || (!tidy.empty() && tidy.back() != ' ')) tidy += c;
    while (!tidy.empty() && tidy.back() == ' ') tidy.pop_back();
    return tidy;
}


std::vector<std::string> command_argv(const std::string& command,
                                      const std::string& token,
                                      const std::string& value) {
    std::vector<std::string> argv = split_args(command);
    if (token.empty()) return argv;
    const std::string safe = safe_arg(value);
    for (std::string& arg : argv)
        for (size_t at = arg.find(token); at != std::string::npos;
             at = arg.find(token, at + safe.size()))
            arg.replace(at, token.size(), safe);
    return argv;
}

}  // namespace gui

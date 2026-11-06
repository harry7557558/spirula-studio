// Subprocess.cpp -- see Subprocess.h.

#include "app/gui/Subprocess.h"

#include <cctype>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <thread>
#include <mutex>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX   // keep windows.h from defining min()/max() macros
#endif
#include <windows.h>
#else
#include <cerrno>
#include <climits>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#ifdef __linux__
#include <sys/syscall.h>
#endif
extern char** environ;
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

// The job is assigned atomically at creation: even death during launch cannot
// strand a suspended child. Only this owner holds its non-inheritable handle.
bool create_owned_process(char* command, const char* cwd, STARTUPINFOA& startup,
                          DWORD flags, PROCESS_INFORMATION& process, HANDLE& job) {
    job = CreateJobObjectA(nullptr, nullptr);
    if (!job) return false;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation,
                                 &limits, sizeof limits)) {
        CloseHandle(job);
        job = nullptr;
        return false;
    }
    HANDLE inherited[3];
    size_t count = 0;
    for (HANDLE h : {startup.hStdInput, startup.hStdOutput, startup.hStdError}) {
        if (!h || h == INVALID_HANDLE_VALUE) continue;
        if (std::find(inherited, inherited + count, h) == inherited + count)
            inherited[count++] = h;
    }
    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(nullptr, 2, 0, &bytes);
    std::vector<unsigned char> storage(bytes);
    auto* attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    bool initialized = InitializeProcThreadAttributeList(attributes, 2, 0, &bytes);
    STARTUPINFOEXA extended{};
    extended.StartupInfo = startup;
    extended.StartupInfo.cb = sizeof extended;
    extended.lpAttributeList = attributes;
    bool ok = initialized &&
        UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                  inherited, count * sizeof(HANDLE), nullptr, nullptr) &&
        UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_JOB_LIST,
                                  &job, sizeof job, nullptr, nullptr) &&
        CreateProcessA(nullptr, command, nullptr, nullptr, TRUE,
                       flags | EXTENDED_STARTUPINFO_PRESENT, nullptr, cwd,
                       &extended.StartupInfo, &process);
    if (initialized) DeleteProcThreadAttributeList(attributes);
    if (!ok) {
        CloseHandle(job);
        job = nullptr;
    }
    return ok;
}

struct OwnedJob {
    HANDLE handle = nullptr;
    std::vector<HANDLE> members;
    bool terminating = false;
    bool cleanup_ok = true;
};

bool retain_job_members(OwnedJob& job) {
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    if (!QueryInformationJobObject(job.handle, JobObjectExtendedLimitInformation,
                                   &limits, sizeof limits, nullptr)) return false;
    limits.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
    limits.BasicLimitInformation.ActiveProcessLimit = 0;
    if (!SetInformationJobObject(job.handle, JobObjectExtendedLimitInformation,
                                 &limits, sizeof limits)) return false;
    constexpr size_t header = offsetof(JOBOBJECT_BASIC_PROCESS_ID_LIST, ProcessIdList) /
                              sizeof(ULONG_PTR);
    std::vector<ULONG_PTR> storage(header + 16);
    auto* list = reinterpret_cast<JOBOBJECT_BASIC_PROCESS_ID_LIST*>(storage.data());
    for (;;) {
        const size_t bytes = storage.size() * sizeof(ULONG_PTR);
        if (bytes > MAXDWORD) return false;
        if (QueryInformationJobObject(job.handle, JobObjectBasicProcessIdList,
                                     list, (DWORD)bytes, nullptr)) break;
        if (GetLastError() != ERROR_MORE_DATA) return false;
        storage.resize(header + (size_t)list->NumberOfAssignedProcesses);
        list = reinterpret_cast<JOBOBJECT_BASIC_PROCESS_ID_LIST*>(storage.data());
    }
    job.members.reserve(list->NumberOfProcessIdsInList);
    for (DWORD i = 0; i < list->NumberOfProcessIdsInList; ++i) {
        HANDLE member = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION,
                                    FALSE, (DWORD)list->ProcessIdList[i]);
        if (!member) return false;
        BOOL belongs = FALSE;
        if (!IsProcessInJob(member, job.handle, &belongs) || !belongs) {
            CloseHandle(member);
            return false;
        }
        job.members.push_back(member);
    }
    return true;
}

void terminate_owned_job(OwnedJob& job) {
    if (!job.handle || job.terminating) return;
    job.terminating = true;
    job.cleanup_ok = retain_job_members(job);
    if (!TerminateJobObject(job.handle, 1)) {
        job.cleanup_ok = false;
        CloseHandle(job.handle);
        job.handle = nullptr;
    }
}

void finish_owned_job(OwnedJob& job) {
    if (!job.handle && job.members.empty()) return;
    terminate_owned_job(job);
    const ULONGLONG deadline = GetTickCount64() + 5000;
    for (HANDLE member : job.members) {
        const ULONGLONG now = GetTickCount64();
        const DWORD remaining = now < deadline ? (DWORD)(deadline - now) : 0;
        if (WaitForSingleObject(member, remaining) != WAIT_OBJECT_0)
            job.cleanup_ok = false;
        CloseHandle(member);
    }
    job.members.clear();
    if (job.handle) {
        for (;;) {
            JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
            if (!QueryInformationJobObject(job.handle, JobObjectBasicAccountingInformation,
                                           &accounting, sizeof accounting, nullptr)) {
                job.cleanup_ok = false;
                break;
            }
            if (accounting.ActiveProcesses == 0) break;
            if (GetTickCount64() >= deadline) { job.cleanup_ok = false; break; }
            Sleep(1);
        }
        CloseHandle(job.handle);
        job.handle = nullptr;
    }
}

void close_owned_job(OwnedJob& job, std::mutex& mutex) {
    std::lock_guard<std::mutex> lock(mutex);
    finish_owned_job(job);
}

void read_owned_lines(HANDLE process, OwnedJob& job, std::mutex& mutex,
                      HANDLE pipe, const std::function<void(const std::string&)>& on_line) {
    std::string acc;
    char buf[4096];
    bool exited = false;
    for (;;) {
        if (!exited && WaitForSingleObject(process, 0) == WAIT_OBJECT_0) {
            exited = true;
            close_owned_job(job, mutex);
        }
        DWORD avail = 0, got = 0;
        if (PeekNamedPipe(pipe, nullptr, 0, nullptr, &avail, nullptr) && avail) {
            if (ReadFile(pipe, buf, (DWORD)std::min<size_t>(sizeof buf, avail), &got, nullptr) && got)
                emit_lines(acc, buf, got, on_line);
        } else if (exited) {
            break;
        } else {
            WaitForSingleObject(process, 100);
        }
    }
    emit_tail(acc, on_line);
}

}  // namespace

int run_process(const std::vector<std::string>& argv,
                const std::string& cwd,
                const std::function<void(const std::string&)>& on_line,
                const std::atomic<bool>& cancel,
                const std::function<void()>& on_poll) {
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
    OwnedJob job;
    BOOL ok = create_owned_process(cmd.data(),
                                   cwd.empty() ? nullptr : cwd.c_str(),
                                   si, CREATE_NO_WINDOW, pi, job.handle);
    CloseHandle(wr);
    if (!ok) { CloseHandle(rd); return kSpawnFailed; }
    CloseHandle(pi.hThread);

    std::string acc;
    char buf[4096];
    bool killed = false;
    bool output_open = true;
    bool exited = false;
    for (;;) {
        if (on_poll) on_poll();
        if (cancel.load() && !killed) {
            terminate_owned_job(job);
            killed = true;
        }
        if (!exited && WaitForSingleObject(pi.hProcess, 0) == WAIT_OBJECT_0) {
            exited = true;
            finish_owned_job(job);
        }
        DWORD avail = 0;
        if (output_open && !PeekNamedPipe(rd, nullptr, 0, nullptr, &avail, nullptr))
            output_open = false;
        if (avail > 0) {
            DWORD got = 0;
            if (!ReadFile(rd, buf, (DWORD)std::min<size_t>(sizeof buf, avail), &got, nullptr) || !got)
                output_open = false;
            else
                emit_lines(acc, buf, got, on_line);
        } else if (exited) {
            break;
        } else {
            WaitForSingleObject(pi.hProcess, 100);
        }
    }
    emit_tail(acc, on_line);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    finish_owned_job(job);
    CloseHandle(pi.hProcess);
    CloseHandle(rd);
    return killed ? kCancelled : job.cleanup_ok ? (int)code : 1;
}


struct ProcessPipe::Impl {
    HANDLE process = nullptr;
    OwnedJob job;
    std::mutex job_mutex;
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
    const BOOL ok = create_owned_process(cmd.data(), nullptr, si,
                                         CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS,
                                         pi, _impl->job.handle);
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
    _impl->job.cleanup_ok = true;
    _impl->job.terminating = false;
    _impl->on_line = std::move(on_line);
    Impl* im = _impl.get();
    _impl->reader = std::thread([im] {
        read_owned_lines(im->process, im->job, im->job_mutex, im->out_rd, im->on_line);
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
    // Descendants must release inherited pipe writers before the reader joins.
    close_owned_job(_impl->job, _impl->job_mutex);
    if (_impl->reader.joinable()) _impl->reader.join();
    DWORD code = 1;
    GetExitCodeProcess(_impl->process, &code);
    CloseHandle(_impl->process);
    CloseHandle(_impl->out_rd);
    _impl->process = _impl->out_rd = nullptr;
    return _impl->job.cleanup_ok ? (int)code : 1;
}

void ProcessPipe::kill() {
    std::lock_guard<std::mutex> lock(_impl->job_mutex);
    terminate_owned_job(_impl->job);
}

ProcessPipe::~ProcessPipe() {
    if (_impl->process) {
        kill();
        finish();
    }
}


struct ProcessReader::Impl {
    HANDLE process = nullptr;
    OwnedJob job;
    std::mutex job_mutex;
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
    const BOOL ok = create_owned_process(cmd.data(), nullptr, si,
                                         CREATE_NO_WINDOW, pi, _impl->job.handle);
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
    _impl->job.cleanup_ok = true;
    _impl->job.terminating = false;
    _impl->on_line = std::move(on_line);
    Impl* im = _impl.get();
    _impl->reader = std::thread([im] {
        read_owned_lines(im->process, im->job, im->job_mutex, im->err_rd, im->on_line);
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
    close_owned_job(_impl->job, _impl->job_mutex);
    if (_impl->reader.joinable()) _impl->reader.join();
    DWORD code = 1;
    GetExitCodeProcess(_impl->process, &code);
    CloseHandle(_impl->process);
    CloseHandle(_impl->err_rd);
    _impl->process = _impl->err_rd = nullptr;
    return _impl->job.cleanup_ok ? (int)code : 1;
}

void ProcessReader::kill() {
    std::lock_guard<std::mutex> lock(_impl->job_mutex);
    terminate_owned_job(_impl->job);
}

ProcessReader::~ProcessReader() {
    if (_impl->process) {
        kill();
        finish();
    }
}

struct ProcessDuplex::Impl {
    HANDLE process = nullptr;
    OwnedJob job;
    std::mutex job_mutex;
    HANDLE in_wr = nullptr, out_rd = nullptr, err_rd = nullptr;
    std::thread diagnostics;
    std::atomic<bool> cancelled{false};
};

bool ProcessDuplex::start(const std::vector<std::string>& argv,
                          std::function<void(const std::string&)> on_stderr) {
    if (argv.empty() || _impl->process) return false;
    std::string command;
    for (const auto& arg : argv)
        command += (command.empty() ? "" : " ") + quote_arg(arg);
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof sa;
    sa.bInheritHandle = TRUE;
    HANDLE in_rd = nullptr, out_wr = nullptr, err_wr = nullptr;
    const bool pipes =
        CreatePipe(&in_rd, &_impl->in_wr, &sa, 1 << 20) &&
        CreatePipe(&_impl->out_rd, &out_wr, &sa, 1 << 20) &&
        CreatePipe(&_impl->err_rd, &err_wr, &sa, 0) &&
        SetHandleInformation(_impl->in_wr, HANDLE_FLAG_INHERIT, 0) &&
        SetHandleInformation(_impl->out_rd, HANDLE_FLAG_INHERIT, 0) &&
        SetHandleInformation(_impl->err_rd, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOA startup{};
    startup.cb = sizeof startup;
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = in_rd;
    startup.hStdOutput = out_wr;
    startup.hStdError = err_wr;
    PROCESS_INFORMATION process{};
    const bool started = pipes &&
        create_owned_process(command.data(), nullptr, startup, CREATE_NO_WINDOW,
                             process, _impl->job.handle);
    for (HANDLE handle : {in_rd, out_wr, err_wr})
        if (handle) CloseHandle(handle);
    if (!started) {
        for (HANDLE handle : {_impl->in_wr, _impl->out_rd, _impl->err_rd})
            if (handle) CloseHandle(handle);
        _impl->in_wr = _impl->out_rd = _impl->err_rd = nullptr;
        return false;
    }
    CloseHandle(process.hThread);
    _impl->process = process.hProcess;
    _impl->job.cleanup_ok = true;
    _impl->job.terminating = false;
    _impl->cancelled.store(false);
    Impl* im = _impl.get();
    _impl->diagnostics = std::thread([im, on_stderr = std::move(on_stderr)] {
        read_owned_lines(im->process, im->job, im->job_mutex, im->err_rd, on_stderr);
    });
    return true;
}

bool ProcessDuplex::write(const void* data, size_t bytes) {
    if (!_impl->in_wr || _impl->cancelled.load()) return false;
    const char* cursor = static_cast<const char*>(data);
    while (bytes) {
        DWORD put = 0;
        if (_impl->cancelled.load() ||
            !WriteFile(_impl->in_wr, cursor, (DWORD)std::min<size_t>(bytes, 1 << 24),
                       &put, nullptr) || !put) return false;
        cursor += put;
        bytes -= put;
    }
    return true;
}

size_t ProcessDuplex::read(void* data, size_t bytes) {
    if (!_impl->out_rd || _impl->cancelled.load()) return 0;
    DWORD got = 0;
    return ReadFile(_impl->out_rd, data, (DWORD)std::min<size_t>(bytes, 1 << 24),
                    &got, nullptr) ? got : 0;
}

int ProcessDuplex::finish() {
    if (!_impl->process) return kSpawnFailed;
    CloseHandle(_impl->in_wr);
    _impl->in_wr = nullptr;
    char discard[4096];
    while (read(discard, sizeof discard)) {}
    WaitForSingleObject(_impl->process, INFINITE);
    close_owned_job(_impl->job, _impl->job_mutex);
    if (_impl->diagnostics.joinable()) _impl->diagnostics.join();
    DWORD code = 1;
    GetExitCodeProcess(_impl->process, &code);
    CloseHandle(_impl->process);
    CloseHandle(_impl->out_rd);
    CloseHandle(_impl->err_rd);
    _impl->process = _impl->out_rd = _impl->err_rd = nullptr;
    return _impl->job.cleanup_ok ? (int)code : 1;
}

void ProcessDuplex::kill() {
    _impl->cancelled.store(true);
    std::lock_guard<std::mutex> lock(_impl->job_mutex);
    terminate_owned_job(_impl->job);
}

ProcessDuplex::~ProcessDuplex() {
    kill();
    finish();
}


bool command_exists(const std::string& exe) {
    if (exe.find('\\') != std::string::npos || exe.find('/') != std::string::npos) {
        std::error_code ec;
        return fs::is_regular_file(exe, ec);
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

namespace {

std::mutex spawn_mutex;
std::vector<int> ownership_writers;

constexpr char completion_key[] = "_SPIRULA_SUBPROCESS_COMPLETION_FDS=";

std::vector<int> inherited_completions() {
    std::vector<int> fds;
    const char* text = std::getenv("_SPIRULA_SUBPROCESS_COMPLETION_FDS");
    while (text && *text) {
        char* end = nullptr;
        const long value = std::strtol(text, &end, 10);
        if (end == text || value < 3 || value > INT_MAX) break;
        struct stat info{};
        const int fd = static_cast<int>(value);
        if (fstat(fd, &info) == 0 && S_ISFIFO(info.st_mode) &&
            (fcntl(fd, F_GETFL) & O_ACCMODE) == O_WRONLY)
            fds.push_back(fd);
        text = *end == ',' ? end + 1 : nullptr;
    }
    return fds;
}

struct ExecArgs {
    std::vector<char*> args, environment, shell;
    std::vector<std::string> paths;
    std::vector<int> ancestors = inherited_completions();
    std::string metadata;

    explicit ExecArgs(const std::vector<std::string>& argv) {
        args.reserve(argv.size() + 1);
        for (const auto& arg : argv) args.push_back(const_cast<char*>(arg.c_str()));
        args.push_back(nullptr);
        shell.reserve(argv.size() + 2);
        shell.push_back(const_cast<char*>("/bin/sh"));
        shell.push_back(nullptr);
        for (size_t i = 1; i < argv.size(); ++i) shell.push_back(args[i]);
        shell.push_back(nullptr);
        if (argv[0].find('/') != std::string::npos) {
            paths.push_back(argv[0]);
        } else {
            const char* path = std::getenv("PATH");
            std::string search;
            if (path) search = path;
            else {
                const size_t size = confstr(_CS_PATH, nullptr, 0);
                if (size) {
                    search.resize(size);
                    confstr(_CS_PATH, search.data(), size);
                    search.resize(size - 1);
                }
            }
            size_t begin = 0;
            for (;;) {
                const size_t end = search.find(':', begin);
                const std::string directory = search.substr(begin, end - begin);
                paths.push_back(directory.empty() ? argv[0] : directory + "/" + argv[0]);
                if (end == std::string::npos) break;
                begin = end + 1;
            }
        }
    }

    void prepare(int completion) {
        metadata = completion_key;
        for (int fd : ancestors) metadata += std::to_string(fd) + ",";
        metadata += std::to_string(completion);
        for (char** entry = environ; entry && *entry; ++entry)
            if (std::strncmp(*entry, completion_key, sizeof completion_key - 1) != 0)
                environment.push_back(*entry);
        environment.push_back(metadata.data());
        environment.push_back(nullptr);
    }

    [[noreturn]] void report_failure() {
        const int code = errno;
        const char prefix[] = "cannot run ";
        const char reason[] = ": exec failed, errno ";
        char digits[16];
        char* end = digits + sizeof digits;
        *--end = '\n';
        unsigned value = static_cast<unsigned>(code);
        do { *--end = char('0' + value % 10); value /= 10; } while (value);
        if (::write(STDERR_FILENO, prefix, sizeof prefix - 1) < 0) {}
        if (::write(STDERR_FILENO, args[0], std::strlen(args[0])) < 0) {}
        if (::write(STDERR_FILENO, reason, sizeof reason - 1) < 0) {}
        if (::write(STDERR_FILENO, end, digits + sizeof digits - end) < 0) {}
        _exit(127);
    }

    [[noreturn]] void execute() {
        for (const auto& path : paths) {
            execve(path.c_str(), args.data(), environment.data());
            if (errno == ENOEXEC) {
                shell[1] = const_cast<char*>(path.c_str());
                execve("/bin/sh", shell.data(), environment.data());
                report_failure();
            }
            if (errno != EACCES && errno != ENOENT && errno != ENOTDIR &&
                errno != ESTALE && errno != ENODEV && errno != ETIMEDOUT) break;
        }
        report_failure();
    }
};

bool owned_pipe(int fds[2]) {
    std::lock_guard<std::mutex> lock(spawn_mutex);
    if (pipe(fds) != 0) return false;
    for (int i = 0; i < 2; ++i) {
        int& fd = fds[i];
        if (fd >= STDERR_FILENO + 1) continue;
        const int original = fd;
        fd = fcntl(original, F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
        close(original);
        if (fd < 0) {
            close(fds[0]); close(fds[1]);
            return false;
        }
    }
    if (fcntl(fds[0], F_SETFD, FD_CLOEXEC) == 0 &&
        fcntl(fds[1], F_SETFD, FD_CLOEXEC) == 0) return true;
    close(fds[0]); close(fds[1]);
    return false;
}

struct OwnedChild {
    pid_t guardian = -1;
    int owner = -1, status = -1;

    void cancel() {
        std::lock_guard<std::mutex> lock(spawn_mutex);
        if (owner < 0) return;
        ownership_writers.erase(std::find(ownership_writers.begin(),
                                         ownership_writers.end(), owner));
        close(owner);
        owner = -1;
    }

    int wait() {
        int result = 0;
        ssize_t got;
        do { got = ::read(status, &result, sizeof result); } while (got < 0 && errno == EINTR);
        int ignored = 0;
        while (waitpid(guardian, &ignored, 0) < 0 && errno == EINTR) {}
        cancel();
        close(status);
        status = -1;
        guardian = -1;
        return got == sizeof result ? result : (1 << 8);
    }
};

void close_guardian_fds(const std::vector<int>& keep, int max_fd) {
#if defined(__linux__) && defined(SYS_close_range)
    unsigned begin = 0;
    bool complete = true;
    for (int fd : keep) {
        if (begin < (unsigned)fd &&
            syscall(SYS_close_range, begin, (unsigned)fd - 1, 0) != 0) {
            complete = false;
            break;
        }
        begin = (unsigned)fd + 1;
    }
    if (complete && syscall(SYS_close_range, begin, ~0u, 0) == 0) return;
#endif
    for (int fd = 0; fd < max_fd; ++fd)
        if (!std::binary_search(keep.begin(), keep.end(), fd)) close(fd);
}

bool wait_completion(int fd) {
    char ignored[64];
    ssize_t remaining;
    do { remaining = ::read(fd, ignored, sizeof ignored); }
    while (remaining > 0 || (remaining < 0 && errno == EINTR));
    return remaining == 0;
}

// A guardian outside the work group owns its lifetime. EOF is also delivered
// on SIGKILL of the caller, unlike a signal handler or atexit cleanup.
pid_t fork_owned(OwnedChild& child, ExecArgs& exec) {
    std::unique_lock<std::mutex> lock(spawn_mutex);
    int owner[2], status[2], completion[2];
    if (pipe(owner) != 0) return -1;
    if (pipe(status) != 0) { close(owner[0]); close(owner[1]); return -1; }
    if (pipe(completion) != 0) {
        close(owner[0]); close(owner[1]); close(status[0]); close(status[1]);
        return -1;
    }
    const int original = completion[1];
    completion[1] = fcntl(original, F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
    close(original);
    if (completion[1] < 0 || fcntl(completion[1], F_SETFD, 0) != 0 ||
        fcntl(completion[0], F_SETFD, FD_CLOEXEC) != 0) {
        close(owner[0]); close(owner[1]); close(status[0]); close(status[1]);
        close(completion[0]); close(completion[1]);
        return -1;
    }
    for (int fd : {owner[0], owner[1], status[0], status[1]}) {
        if (fcntl(fd, F_SETFD, FD_CLOEXEC) != 0) {
            close(owner[0]); close(owner[1]); close(status[0]); close(status[1]);
            close(completion[0]); close(completion[1]);
            return -1;
        }
    }
    exec.prepare(completion[1]);
    std::vector<int> keep = exec.ancestors;
    keep.insert(keep.end(), {owner[0], status[1], completion[0]});
    std::sort(keep.begin(), keep.end());
    keep.erase(std::unique(keep.begin(), keep.end()), keep.end());
    const int max_fd = getdtablesize();
    struct sigaction pipe_signal{};
    sigaction(SIGPIPE, nullptr, &pipe_signal);
    struct sigaction child_signal{};
    sigaction(SIGCHLD, nullptr, &child_signal);
    const pid_t guardian = fork();
    if (guardian < 0) {
        close(owner[0]); close(owner[1]); close(status[0]); close(status[1]);
        close(completion[0]); close(completion[1]);
        return -1;
    }
    if (guardian == 0) {
        if (setsid() < 0) _exit(1);
        signal(SIGPIPE, SIG_IGN);
        struct sigaction reap_signal{};
        reap_signal.sa_handler = SIG_DFL;
        sigemptyset(&reap_signal.sa_mask);
        sigaction(SIGCHLD, &reap_signal, nullptr);
        close(owner[1]);
        close(status[0]);
        for (int fd : ownership_writers) close(fd);
        pid_t worker = fork();
        if (worker == 0) {
            sigaction(SIGPIPE, &pipe_signal, nullptr);
            sigaction(SIGCHLD, &child_signal, nullptr);
            close(owner[0]);
            close(status[1]);
            close(completion[0]);
            if (setpgid(0, 0) != 0) _exit(127);
            lock.release();
            return 0;
        }
        close(completion[1]);
        close_guardian_fds(keep, max_fd);
        // Establish the group before reporting readiness to cancellation.
        if (worker > 0 && setpgid(worker, worker) != 0 && getpgid(worker) != worker) {
            ::kill(worker, SIGKILL);
            int ignored;
            while (waitpid(worker, &ignored, 0) < 0 && errno == EINTR) {}
            worker = -1;
        }
        if (::write(status[1], &worker, sizeof worker) != sizeof worker || worker < 0) {
            if (worker > 0) {
                ::kill(-worker, SIGKILL);
                int ignored;
                while (waitpid(worker, &ignored, 0) < 0 && errno == EINTR) {}
            }
            wait_completion(completion[0]);
            _exit(1);
        }
        int result = 0;
        for (;;) {
            siginfo_t info{};
            const int done = waitid(P_PID, worker, &info, WEXITED | WNOHANG | WNOWAIT);
            if (done == 0 && info.si_pid == worker) {
                // Keep the leader unreaped until the group has been killed:
                // its PID must not be recycled into somebody else's group.
                ::kill(-worker, SIGKILL);
                while (waitpid(worker, &result, 0) < 0 && errno == EINTR) {}
                break;
            }
            if (done < 0 && errno != EINTR) {
                if (errno != ECHILD) {
                    ::kill(-worker, SIGKILL);
                    while (waitpid(worker, &result, 0) < 0 && errno == EINTR) {}
                }
                result = 1 << 8;
                break;
            }
            pollfd pfd{owner[0], POLLIN, 0};
            const int ready = poll(&pfd, 1, 50);
            if ((ready > 0 && pfd.revents) || (ready < 0 && errno != EINTR)) {
                ::kill(-worker, SIGKILL);
                while (waitpid(worker, &result, 0) < 0 && errno == EINTR) {}
                break;
            }
        }
        // Nested guardians retain ancestor leases until their own descendants
        // have stopped. EOF is the reclamation barrier, not merely worker exit.
        if (!wait_completion(completion[0])) result = 1 << 8;
        ssize_t written;
        do { written = ::write(status[1], &result, sizeof result); } while (written < 0 && errno == EINTR);
        _exit(written == sizeof result ? 0 : 1);
    }
    close(completion[0]);
    close(completion[1]);
    close(owner[0]);
    close(status[1]);
    pid_t worker = -1;
    ssize_t got;
    do { got = ::read(status[0], &worker, sizeof worker); } while (got < 0 && errno == EINTR);
    if (got != sizeof worker || worker < 0) {
        close(owner[1]); close(status[0]);
        int ignored;
        while (waitpid(guardian, &ignored, 0) < 0 && errno == EINTR) {}
        return -1;
    }
    child.guardian = guardian;
    child.owner = owner[1];
    child.status = status[0];
    ownership_writers.push_back(child.owner);
    return worker;
}

}  // namespace

int run_process(const std::vector<std::string>& argv,
                const std::string& cwd,
                const std::function<void(const std::string&)>& on_line,
                const std::atomic<bool>& cancel,
                const std::function<void()>& on_poll) {
    if (argv.empty()) return kSpawnFailed;
    ExecArgs exec(argv);
    int fds[2];
    if (!owned_pipe(fds)) return kSpawnFailed;

    OwnedChild child;
    pid_t pid = fork_owned(child, exec);
    if (pid < 0) { close(fds[0]); close(fds[1]); return kSpawnFailed; }
    if (pid == 0) {
        dup2(fds[1], STDOUT_FILENO);
        dup2(fds[1], STDERR_FILENO);
        close(fds[0]);
        close(fds[1]);
        if (!cwd.empty() && chdir(cwd.c_str()) != 0) _exit(127);
        exec.execute();
    }
    close(fds[1]);

    std::string acc;
    char buf[4096];
    bool killed = false;
    bool output_open = true;
    for (;;) {
        if (on_poll) on_poll();
        if (cancel.load() && !killed) {
            child.cancel();
            killed = true;
        }
        pollfd fds_to_poll[2]{{output_open ? fds[0] : -1, POLLIN, 0},
                             {child.status, POLLIN, 0}};
        const int pr = poll(fds_to_poll, 2, 100);
        if (pr > 0) {
            if (output_open && fds_to_poll[0].revents) {
                const ssize_t got = read(fds[0], buf, sizeof buf);
                if (got > 0) emit_lines(acc, buf, (size_t)got, on_line);
                else if (got == 0 || errno != EINTR) output_open = false;
            }
            if (!output_open && fds_to_poll[1].revents) break;
        } else if (pr < 0 && errno != EINTR) {
            child.cancel();
            break;
        }
    }
    emit_tail(acc, on_line);
    close(fds[0]);
    const int status = child.wait();
    if (killed) return kCancelled;
    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        return code == 127 ? kSpawnFailed : code;
    }
    return 1;
}

struct ProcessPipe::Impl {
    pid_t pid = -1;
    OwnedChild child;
    int in_fd = -1, out_fd = -1;
    std::thread reader;
    std::function<void(const std::string&)> on_line;
};

bool ProcessPipe::start(const std::vector<std::string>& argv,
                        std::function<void(const std::string&)> on_line) {
    if (argv.empty() || _impl->pid > 0) return false;
    ExecArgs exec(argv);
    // A child that dies mid-frame must fail the write, not kill this process.
    signal(SIGPIPE, SIG_IGN);
    int in[2], out[2];
    if (!owned_pipe(in)) return false;
    if (!owned_pipe(out)) { close(in[0]); close(in[1]); return false; }
    const pid_t pid = fork_owned(_impl->child, exec);
    if (pid < 0) {
        close(in[0]); close(in[1]); close(out[0]); close(out[1]);
        return false;
    }
    if (pid == 0) {
        // Below the window, as on Windows.
        if (nice(10) == -1) {}
        dup2(in[0], STDIN_FILENO);
        dup2(out[1], STDOUT_FILENO);
        dup2(out[1], STDERR_FILENO);
        close(in[0]); close(in[1]); close(out[0]); close(out[1]);
        exec.execute();
    }
    close(in[0]);
    close(out[1]);
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
    const int status = _impl->child.wait();
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
    _impl->child.cancel();
}

ProcessPipe::~ProcessPipe() {
    if (_impl->pid > 0) {
        kill();
        finish();
    }
}


struct ProcessReader::Impl {
    pid_t pid = -1;
    OwnedChild child;
    int out_fd = -1, err_fd = -1;
    std::thread reader;
    std::function<void(const std::string&)> on_line;
};

bool ProcessReader::start(const std::vector<std::string>& argv,
                          std::function<void(const std::string&)> on_line) {
    if (argv.empty() || _impl->pid > 0) return false;
    ExecArgs exec(argv);
    int out[2], err[2];
    if (!owned_pipe(out)) return false;
    if (!owned_pipe(err)) { close(out[0]); close(out[1]); return false; }
    const pid_t pid = fork_owned(_impl->child, exec);
    if (pid < 0) {
        close(out[0]); close(out[1]); close(err[0]); close(err[1]);
        return false;
    }
    if (pid == 0) {
        const int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) dup2(devnull, STDIN_FILENO);
        if (devnull > STDERR_FILENO) close(devnull);
        dup2(out[1], STDOUT_FILENO);
        dup2(err[1], STDERR_FILENO);
        close(out[0]); close(out[1]); close(err[0]); close(err[1]);
        exec.execute();
    }
    close(out[1]);
    close(err[1]);
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
    const int status = _impl->child.wait();
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
    _impl->child.cancel();
}

ProcessReader::~ProcessReader() {
    if (_impl->pid > 0) {
        kill();
        finish();
    }
}

struct ProcessDuplex::Impl {
    pid_t pid = -1;
    OwnedChild child;
    int in_fd = -1, out_fd = -1, err_fd = -1;
    std::thread diagnostics;
    std::atomic<bool> cancelled{false};
};

bool ProcessDuplex::start(const std::vector<std::string>& argv,
                          std::function<void(const std::string&)> on_stderr) {
    if (argv.empty() || _impl->pid > 0) return false;
    ExecArgs exec(argv);
    int in[2], out[2], err[2];
    if (!owned_pipe(in)) return false;
    if (!owned_pipe(out)) { close(in[0]); close(in[1]); return false; }
    if (!owned_pipe(err)) {
        close(in[0]); close(in[1]); close(out[0]); close(out[1]);
        return false;
    }
    const pid_t pid = fork_owned(_impl->child, exec);
    if (pid < 0) {
        close(in[0]); close(in[1]); close(out[0]); close(out[1]);
        close(err[0]); close(err[1]);
        return false;
    }
    if (pid == 0) {
        if (dup2(in[0], STDIN_FILENO) < 0 ||
            dup2(out[1], STDOUT_FILENO) < 0 ||
            dup2(err[1], STDERR_FILENO) < 0) _exit(127);
        close(in[0]); close(in[1]); close(out[0]); close(out[1]);
        close(err[0]); close(err[1]);
        exec.execute();
    }
    close(in[0]); close(out[1]); close(err[1]);
    _impl->pid = pid;
    _impl->in_fd = in[1];
    _impl->out_fd = out[0];
    _impl->err_fd = err[0];
    _impl->cancelled.store(false);
    Impl* im = _impl.get();
    _impl->diagnostics = std::thread([im, on_stderr = std::move(on_stderr)] {
        std::string acc;
        char buffer[4096];
        for (;;) {
            const ssize_t got = ::read(im->err_fd, buffer, sizeof buffer);
            if (got < 0 && errno == EINTR) continue;
            if (got <= 0) break;
            emit_lines(acc, buffer, (size_t)got, on_stderr);
        }
        emit_tail(acc, on_stderr);
    });
    return true;
}

bool ProcessDuplex::write(const void* data, size_t bytes) {
    if (_impl->in_fd < 0 || _impl->cancelled.load()) return false;
    sigset_t blocked, previous, pending;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGPIPE);
    if (pthread_sigmask(SIG_BLOCK, &blocked, &previous) != 0) return false;
    sigpending(&pending);
    const bool already_pending = sigismember(&pending, SIGPIPE) == 1;
    const char* cursor = static_cast<const char*>(data);
    bool success = true;
    while (bytes) {
        if (_impl->cancelled.load()) { success = false; break; }
        const ssize_t put = ::write(_impl->in_fd, cursor,
                                    std::min<size_t>(bytes, 1 << 24));
        if (put < 0 && errno == EINTR) continue;
        if (put <= 0) { success = false; break; }
        cursor += put;
        bytes -= (size_t)put;
    }
    // Consume only a SIGPIPE generated by this write before restoring the mask.
    sigpending(&pending);
    if (!already_pending && sigismember(&pending, SIGPIPE) == 1) {
        int ignored;
        sigwait(&blocked, &ignored);
    }
    pthread_sigmask(SIG_SETMASK, &previous, nullptr);
    return success;
}

size_t ProcessDuplex::read(void* data, size_t bytes) {
    if (_impl->out_fd < 0 || _impl->cancelled.load()) return 0;
    for (;;) {
        const ssize_t got = ::read(_impl->out_fd, data,
                                   std::min<size_t>(bytes, 1 << 24));
        if (got < 0 && errno == EINTR) continue;
        return got > 0 ? (size_t)got : 0;
    }
}

int ProcessDuplex::finish() {
    if (_impl->pid <= 0) return kSpawnFailed;
    close(_impl->in_fd);
    _impl->in_fd = -1;
    char discard[4096];
    while (read(discard, sizeof discard)) {}
    const int status = _impl->child.wait();
    if (_impl->diagnostics.joinable()) _impl->diagnostics.join();
    close(_impl->out_fd); close(_impl->err_fd);
    _impl->out_fd = _impl->err_fd = -1;
    _impl->pid = -1;
    if (WIFEXITED(status)) {
        const int code = WEXITSTATUS(status);
        return code == 127 ? kSpawnFailed : code;
    }
    return 1;
}

void ProcessDuplex::kill() {
    _impl->cancelled.store(true);
    _impl->child.cancel();
}

ProcessDuplex::~ProcessDuplex() {
    kill();
    finish();
}


// A folder passes access(X_OK) too, and then fails to spawn without a word.
static bool runnable(const std::string& path) {
    std::error_code ec;
    return access(path.c_str(), X_OK) == 0 && !fs::is_directory(path, ec);
}

bool command_exists(const std::string& exe) {
    if (exe.find('/') != std::string::npos)
        return runnable(exe);
    const char* path = std::getenv("PATH");
    if (!path) return false;
    std::string p = path;
    size_t pos = 0;
    while (pos <= p.size()) {
        size_t colon = p.find(':', pos);
        std::string dir = p.substr(pos, colon == std::string::npos
                                            ? std::string::npos : colon - pos);
        if (!dir.empty() && runnable(dir + "/" + exe))
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
        std::unique_lock<std::mutex> lock(spawn_mutex);
        const auto completion_fds = inherited_completions();
        const pid_t pid = fork();
        if (pid < 0) return false;
        if (pid == 0) {
            lock.release();
            for (int fd : completion_fds) close(fd);
            for (int fd : ownership_writers) close(fd);
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
        lock.unlock();
        int status = 0;
        waitpid(pid, &status, 0);
        return true;
    }
    return false;
}

#endif

ProcessPipe::ProcessPipe() : _impl(new Impl) {}
ProcessReader::ProcessReader() : _impl(new Impl) {}
ProcessDuplex::ProcessDuplex() : _impl(new Impl) {}

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

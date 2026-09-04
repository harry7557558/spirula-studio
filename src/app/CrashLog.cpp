// CrashLog.cpp -- see CrashLog.h.
//
// No handler here allocates, formats through the CRT or takes a lock: it runs
// on a thread that may already hold the heap lock it died inside, and one that
// deadlocks there writes no report at all.

#include "app/CrashLog.h"

#include "core/Env.h"
#include "i18n/catalog/Brand.h"
#include "i18n/catalog/Gui.h"

#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dbghelp.h>
#else
#include <dlfcn.h>
#include <execinfo.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace app {

namespace {

constexpr int kMaxFrames = 64;
constexpr size_t kBufSize = 32768;
// A report is a couple of KB and nobody reads the hundredth, so the file
// starts over rather than growing without bound.
constexpr long long kMaxLogBytes = 256 * 1024;

char g_path[1024];
char g_note[256];
char g_buf[kBufSize];
size_t g_len;
std::atomic<bool> g_armed{false};
std::atomic<bool> g_dialog{false};
std::atomic_flag g_busy = ATOMIC_FLAG_INIT;

void put(const char* s) {
    while (s && *s && g_len + 1 < kBufSize) g_buf[g_len++] = *s++;
}
void put_ch(char c) { if (g_len + 1 < kBufSize) g_buf[g_len++] = c; }

void put_dec(long long v) {
    if (v < 0) { put_ch('-'); v = -v; }
    char t[24];
    int n = 0;
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) put_ch(t[--n]);
}

void put_hex(unsigned long long v) {
    char t[16];
    int n = 0;
    do { t[n++] = "0123456789abcdef"[v & 15]; v >>= 4; } while (v && n < 16);
    put("0x");
    while (n) put_ch(t[--n]);
}

void put_pad2(int v) {
    put_ch((char)('0' + (v / 10) % 10));
    put_ch((char)('0' + v % 10));
}

// UTC from a Unix timestamp by arithmetic (civil_from_days), because
// localtime/gmtime take a lock and this runs where locks may be held.
void put_utc(long long t) {
    long long days = t / 86400, secs = t % 86400;
    if (secs < 0) { secs += 86400; days--; }
    long long z = days + 719468;
    long long era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    unsigned d = doy - (153 * mp + 2) / 5 + 1;
    unsigned m = mp < 10 ? mp + 3 : mp - 9;
    long long y = (long long)yoe + era * 400 + (m <= 2);
    put_dec(y);
    put_ch('-'); put_pad2((int)m);
    put_ch('-'); put_pad2((int)d);
    put_ch('T'); put_pad2((int)(secs / 3600));
    put_ch(':'); put_pad2((int)(secs / 60 % 60));
    put_ch(':'); put_pad2((int)(secs % 60));
    put("Z");
}

const char* basename_of(const char* p) {
    const char* base = p;
    for (const char* c = p; *c; c++)
        if (*c == '/' || *c == '\\') base = c + 1;
    return base;
}

// ---------------------------------------------------------------------------
// Stack
// ---------------------------------------------------------------------------

#ifdef _WIN32

void put_frame(DWORD64 pc) {
    HANDLE proc = GetCurrentProcess();
    HMODULE mod = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)(uintptr_t)pc, &mod) && mod) {
        char file[MAX_PATH];
        if (GetModuleFileNameA(mod, file, MAX_PATH)) put(basename_of(file));
        put_ch('+');
        put_hex(pc - (DWORD64)(uintptr_t)mod);
    } else {
        put_hex(pc);
    }
    // Absent without a .pdb beside the executable, which a release build has
    // no reason to ship: the module+RVA above is what resolves it later.
    alignas(SYMBOL_INFO) char sym[sizeof(SYMBOL_INFO) + 512];
    SYMBOL_INFO* si = (SYMBOL_INFO*)sym;
    si->SizeOfStruct = sizeof(SYMBOL_INFO);
    si->MaxNameLen = 511;
    DWORD64 off = 0;
    if (SymFromAddr(proc, pc, &off, si)) {
        put("  ");
        put(si->Name);
    }
    IMAGEHLP_LINE64 line{};
    line.SizeOfStruct = sizeof line;
    DWORD line_off = 0;
    if (SymGetLineFromAddr64(proc, pc, &line_off, &line) && line.FileName) {
        put("  (");
        put(basename_of(line.FileName));
        put_ch(':');
        put_dec(line.LineNumber);
        put_ch(')');
    }
}

// StackWalk64 writes through the context it is given, so it gets a copy.
void put_stack(const CONTEXT* from) {
    CONTEXT ctx = *from;
    STACKFRAME64 sf{};
    DWORD machine;
#if defined(_M_X64)
    machine = IMAGE_FILE_MACHINE_AMD64;
    sf.AddrPC.Offset = ctx.Rip;
    sf.AddrFrame.Offset = ctx.Rbp;
    sf.AddrStack.Offset = ctx.Rsp;
#elif defined(_M_ARM64)
    machine = IMAGE_FILE_MACHINE_ARM64;
    sf.AddrPC.Offset = ctx.Pc;
    sf.AddrFrame.Offset = ctx.Fp;
    sf.AddrStack.Offset = ctx.Sp;
#else
    machine = IMAGE_FILE_MACHINE_I386;
    sf.AddrPC.Offset = ctx.Eip;
    sf.AddrFrame.Offset = ctx.Ebp;
    sf.AddrStack.Offset = ctx.Esp;
#endif
    sf.AddrPC.Mode = sf.AddrFrame.Mode = sf.AddrStack.Mode = AddrModeFlat;

    HANDLE proc = GetCurrentProcess();
    for (int i = 0; i < kMaxFrames; i++) {
        if (!StackWalk64(machine, proc, GetCurrentThread(), &sf, &ctx, nullptr,
                         SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
            break;
        if (!sf.AddrPC.Offset) break;
        put("  #");
        put_dec(i);
        put_ch(' ');
        put_frame(sf.AddrPC.Offset);
        put_ch('\n');
    }
}

void put_stack_here() {
    CONTEXT ctx;
    RtlCaptureContext(&ctx);
    put_stack(&ctx);
}

#else

void put_stack_here() {
    void* frames[kMaxFrames];
    const int n = backtrace(frames, kMaxFrames);
    for (int i = 0; i < n; i++) {
        put("  #");
        put_dec(i);
        put_ch(' ');
        Dl_info info;
        if (dladdr(frames[i], &info) && info.dli_fname && info.dli_fbase) {
            put(basename_of(info.dli_fname));
            put_ch('+');
            put_hex((unsigned long long)((const char*)frames[i] -
                                         (const char*)info.dli_fbase));
            if (info.dli_sname) {
                put("  ");
                put(info.dli_sname);
            }
        } else {
            put_hex((unsigned long long)(uintptr_t)frames[i]);
        }
        put_ch('\n');
    }
}

#endif

// ---------------------------------------------------------------------------
// The report
// ---------------------------------------------------------------------------

void begin_report() {
    g_len = 0;
    put("\n=== Spirula Studio crash report ===\n");
    put("time:    ");
    put_utc((long long)std::time(nullptr));
    put("\nversion: ");
#ifdef SS_VERSION
    put(SS_VERSION);
#else
    put("unknown");
#endif
    put("\nbuild:   " __DATE__ " " __TIME__ "\npid:     ");
#ifdef _WIN32
    put_dec(GetCurrentProcessId());
#else
    put_dec(getpid());
#endif
    put_ch('\n');
    if (g_note[0]) {
        put("doing:   ");
        put(g_note);
        put_ch('\n');
    }
}

void end_report() {
    put("Resolve a frame with: addr2line -f -C -e <module> <offset>\n"
        "  (Windows: the offset is an RVA; open it against the matching .pdb)\n");

#ifdef _WIN32
    HANDLE h = CreateFileA(g_path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    LARGE_INTEGER size;
    if (h != INVALID_HANDLE_VALUE && GetFileSizeEx(h, &size) &&
        size.QuadPart > kMaxLogBytes) {
        CloseHandle(h);
        h = CreateFileA(g_path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    }
    if (h != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(h, g_buf, (DWORD)g_len, &written, nullptr);
        CloseHandle(h);
    }
    HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
    if (err && err != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(err, g_buf, (DWORD)g_len, &written, nullptr);
    }

    // The whole point for the window: its console is gone by now (GuiMain), so
    // without this it simply vanishes and the file is never found.
    if (!g_dialog.load()) return;
    wchar_t text[2048];
    int n = MultiByteToWideChar(CP_UTF8, 0,
                                spirula::i18n::msg::gui::crash_report_saved.get(),
                                -1, text, 1024);
    if (n > 0) {
        n--;                                  // the terminator it counted
        text[n++] = L'\n';
        text[n++] = L'\n';
        MultiByteToWideChar(CP_UTF8, 0, g_path, -1, text + n,
                            (int)(2048 - n));
        wchar_t caption[256];
        MultiByteToWideChar(CP_UTF8, 0,
                            spirula::i18n::msg::brand::window_title.get(), -1,
                            caption, 256);
        MessageBoxW(nullptr, text, caption, MB_OK | MB_ICONERROR |
                                                MB_SETFOREGROUND | MB_TOPMOST);
    }
#else
    int fd = ::open(g_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0 && lseek(fd, 0, SEEK_END) > kMaxLogBytes) {
        ::close(fd);
        fd = ::open(g_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    }
    if (fd >= 0) {
        (void)!::write(fd, g_buf, g_len);
        ::close(fd);
    }
    (void)!::write(2, g_buf, g_len);
#endif
}

// One report per process: a fault inside the handler must end the process
// rather than recurse into it.
bool claim() {
    return g_armed.load() && !g_busy.test_and_set();
}

// ---------------------------------------------------------------------------
// Handlers
// ---------------------------------------------------------------------------

void report_uncaught() {
    if (!claim()) return;
    begin_report();
    put("cause:   uncaught exception");
    if (std::current_exception()) {
        try {
            std::rethrow_exception(std::current_exception());
        } catch (const std::exception& e) {
            put(" -- ");
            put(e.what());
        } catch (...) {
            put(" -- not derived from std::exception");
        }
    }
    put("\nstack:\n");
    put_stack_here();
    end_report();
#ifdef _MSC_VER
    // std::terminate goes on to abort(), whose CRT dialog would land on top
    // of the message box end_report() has just shown.
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
}

#ifdef _WIN32

// 0xE06D7363 is an MSVC C++ throw, and this filter -- not std::terminate --
// is where an uncaught one lands, so what() is lost on Windows. Returning
// CONTINUE_SEARCH to reach terminate instead ends the process with no report.
const char* exception_name(DWORD code) {
    switch (code) {
        case 0xE06D7363:                      return "C++ exception";
        case EXCEPTION_ACCESS_VIOLATION:      return "ACCESS_VIOLATION";
        case EXCEPTION_STACK_OVERFLOW:        return "STACK_OVERFLOW";
        case EXCEPTION_ILLEGAL_INSTRUCTION:   return "ILLEGAL_INSTRUCTION";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:    return "INT_DIVIDE_BY_ZERO";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:    return "FLT_DIVIDE_BY_ZERO";
        case EXCEPTION_PRIV_INSTRUCTION:      return "PRIV_INSTRUCTION";
        case EXCEPTION_IN_PAGE_ERROR:         return "IN_PAGE_ERROR";
        case EXCEPTION_DATATYPE_MISALIGNMENT: return "DATATYPE_MISALIGNMENT";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY_BOUNDS_EXCEEDED";
        default:                              return "exception";
    }
}

LONG WINAPI on_exception(EXCEPTION_POINTERS* info) {
    if (!claim() || !info || !info->ExceptionRecord)
        return EXCEPTION_CONTINUE_SEARCH;
    const EXCEPTION_RECORD* r = info->ExceptionRecord;
    begin_report();
    put("cause:   ");
    put(exception_name(r->ExceptionCode));
    put(" (");
    put_hex(r->ExceptionCode);
    put(") at ");
    put_hex((unsigned long long)(uintptr_t)r->ExceptionAddress);
    if (r->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
        r->NumberParameters >= 2) {
        put(r->ExceptionInformation[0] == 1 ? ", writing " : ", reading ");
        put_hex((unsigned long long)r->ExceptionInformation[1]);
    }
    put("\nstack:\n");
    put_stack(info->ContextRecord);
    end_report();
    // Our own dialog has been shown; letting this fall through would put
    // Windows Error Reporting's on top of it.
    return EXCEPTION_EXECUTE_HANDLER;
}

#else

const char* signal_name(int sig) {
    switch (sig) {
        case SIGSEGV: return "SIGSEGV";
        case SIGBUS:  return "SIGBUS";
        case SIGILL:  return "SIGILL";
        case SIGFPE:  return "SIGFPE";
        case SIGABRT: return "SIGABRT";
        default:      return "signal";
    }
}

void on_signal(int sig, siginfo_t* info, void*) {
    if (claim()) {
        begin_report();
        put("cause:   ");
        put(signal_name(sig));
        if (info && (sig == SIGSEGV || sig == SIGBUS)) {
            put(" at ");
            put_hex((unsigned long long)(uintptr_t)info->si_addr);
        }
        put("\nstack:\n");
        put_stack_here();
        end_report();
    }
    // Back to the default disposition so the shell still sees a crash and a
    // core file can be produced.
    signal(sig, SIG_DFL);
    raise(sig);
}

#endif

}  // namespace


void set_crash_dialog(bool on) { g_dialog = on; }


std::string crash_log_path() { return g_path; }


void set_crash_note(const std::string& what) {
    size_t n = 0;
    while (n < what.size() && n + 1 < sizeof g_note) {
        g_note[n] = what[n];
        n++;
    }
    while (n < sizeof g_note) g_note[n++] = '\0';   // always terminated
}


void install_crash_log(const std::string& dir) {
    if (g_armed.load()) return;
    std::string path = dir;
    if (!path.empty() && path.back() != '/' && path.back() != '\\')
        path += '/';
    path += "crash.log";
    if (path.size() + 1 > sizeof g_path) return;
    std::memcpy(g_path, path.c_str(), path.size() + 1);

    std::set_terminate(report_uncaught);

#ifdef _WIN32
    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
    SymInitialize(GetCurrentProcess(), nullptr, TRUE);
    SetUnhandledExceptionFilter(on_exception);
#else
    // backtrace() loads the unwinder on its first call, which allocates; do
    // that now rather than inside the handler.
    void* warm[4];
    (void)backtrace(warm, 4);

    // A stack overflow is the fault most worth a report and the one that
    // leaves no room on the faulting stack to write one.
    static char alt[65536];   // SIGSTKSZ is not a constant on current glibc
    stack_t ss{};
    ss.ss_sp = alt;
    ss.ss_size = sizeof alt;
    sigaltstack(&ss, nullptr);

    struct sigaction sa {};
    sa.sa_sigaction = on_signal;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
    sigemptyset(&sa.sa_mask);
    for (int sig : {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT})
        sigaction(sig, &sa, nullptr);
#endif
    g_armed = true;

    const char* test = spirula::env("CRASH_TEST");
    if (!test) return;
    set_crash_note("SS_CRASH_TEST");
    if (test[0] == 't') throw std::runtime_error("SS_CRASH_TEST");
    *(volatile int*)(uintptr_t)8 = 0;
}

}  // namespace app

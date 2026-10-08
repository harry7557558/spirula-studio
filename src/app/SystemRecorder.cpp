#include "app/SystemRecorder.h"

#include "core/HostMemory.h"

#if defined(_WIN32)
#include <pdh.h>
#include <pdhmsg.h>
#include <tlhelp32.h>
#pragma comment(lib, "pdh.lib")
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace spirula {
namespace {

long long unix_ms() {
    return (long long)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string csv_safe(std::string s) {
    for (char& c : s)
        if (c == ',' || c == '\n' || c == '\r' || c == '"') c = ' ';
    while (!s.empty() && s.back() == ' ') s.pop_back();
    while (!s.empty() && s.front() == ' ') s.erase(s.begin());
    return s;
}

std::string json_str(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        if ((unsigned char)c >= 0x20) out += c;
    }
    return out + "\"";
}

// "" for an unknown value, so the report reads it as missing rather than zero.
std::string num(double v, bool known = true) {
    if (!known) return "";
    char buf[48];
    std::snprintf(buf, sizeof buf, "%.3f", v);
    return buf;
}

#if defined(_WIN32)
std::string utf8(const wchar_t* w) {
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return {};
    std::string s((size_t)n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
}
#endif

// NVML, loaded at run time: a machine without the NVIDIA driver still records
// everything else. Only the handful of calls nvidia-smi's query fields need.
class Nvml {
public:
    ~Nvml() { close(); }

    bool open() {
#if defined(_WIN32)
        HMODULE lib = LoadLibraryW(L"nvml.dll");
        if (!lib) lib = LoadLibraryW(L"C:\\Program Files\\NVIDIA Corporation\\NVSMI\\nvml.dll");
        _lib = lib;
        auto sym = [&](const char* n) { return lib ? (void*)GetProcAddress(lib, n) : nullptr; };
#else
        _lib = dlopen("libnvidia-ml.so.1", RTLD_NOW | RTLD_LOCAL);
        auto sym = [&](const char* n) { return _lib ? dlsym(_lib, n) : nullptr; };
#endif
        if (!_lib) return false;
        _init     = (Ret (*)())sym("nvmlInit_v2");
        _shutdown = (Ret (*)())sym("nvmlShutdown");
        _count    = (Ret (*)(unsigned*))sym("nvmlDeviceGetCount_v2");
        _handle   = (Ret (*)(unsigned, Device*))sym("nvmlDeviceGetHandleByIndex_v2");
        _name     = (Ret (*)(Device, char*, unsigned))sym("nvmlDeviceGetName");
        _util     = (Ret (*)(Device, Utilization*))sym("nvmlDeviceGetUtilizationRates");
        _mem      = (Ret (*)(Device, Memory*))sym("nvmlDeviceGetMemoryInfo");
        _temp     = (Ret (*)(Device, int, unsigned*))sym("nvmlDeviceGetTemperature");
        _power    = (Ret (*)(Device, unsigned*))sym("nvmlDeviceGetPowerUsage");
        _clock    = (Ret (*)(Device, int, unsigned*))sym("nvmlDeviceGetClockInfo");
        _pstate   = (Ret (*)(Device, int*))sym("nvmlDeviceGetPerformanceState");
        _reasons  = (Ret (*)(Device, unsigned long long*))sym("nvmlDeviceGetCurrentClocksThrottleReasons");
        unsigned n = 0;
        if (!_init || !_count || !_handle || _init() != 0) { close(); return false; }
        _live = true;
        if (_count(&n) != 0) n = 0;
        for (unsigned i = 0; i < n; ++i) {
            Device d = nullptr;
            if (_handle(i, &d) != 0) continue;
            char name[96] = {};
            if (!_name || _name(d, name, sizeof name) != 0) std::strcpy(name, "NVIDIA GPU");
            _devices.push_back({i, d, csv_safe(name)});
        }
        if (_devices.empty()) close();
        return !_devices.empty();
    }

    void close() {
        if (_live && _shutdown) _shutdown();
        _live = false;
        _devices.clear();
#if defined(_WIN32)
        if (_lib) FreeLibrary((HMODULE)_lib);
#else
        if (_lib) dlclose(_lib);
#endif
        _lib = nullptr;
    }

    std::vector<std::string> names() const {
        std::vector<std::string> out;
        for (const auto& d : _devices) out.push_back(d.name);
        return out;
    }

    static const char* header() {
        return "unix_ms,index,name,utilization_gpu,utilization_memory,memory_used,memory_total,"
               "temperature_gpu,power_draw,clocks_sm,clocks_mem,pstate,clocks_event_reasons_active\n";
    }

    void sample(std::FILE* f, long long ms) {
        for (const auto& d : _devices) {
            Utilization u{};
            Memory m{};
            unsigned temp = 0, mw = 0, sm = 0, memclk = 0;
            int ps = -1;
            unsigned long long reasons = 0;
            const bool has_u = _util && _util(d.handle, &u) == 0;
            const bool has_m = _mem && _mem(d.handle, &m) == 0;
            const bool has_t = _temp && _temp(d.handle, 0, &temp) == 0;
            const bool has_p = _power && _power(d.handle, &mw) == 0;
            const bool has_sm = _clock && _clock(d.handle, 1, &sm) == 0;
            const bool has_mc = _clock && _clock(d.handle, 2, &memclk) == 0;
            const bool has_ps = _pstate && _pstate(d.handle, &ps) == 0 && ps >= 0;
            const bool has_r = _reasons && _reasons(d.handle, &reasons) == 0;
            char pstate[8] = "", why[24] = "";
            if (has_ps) std::snprintf(pstate, sizeof pstate, "P%d", ps);
            if (has_r) std::snprintf(why, sizeof why, "0x%016llx", reasons);
            const double mib = 1024.0 * 1024.0;
            std::fprintf(f, "%lld,%u,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n", ms, d.index, d.name.c_str(),
                         num(u.gpu, has_u).c_str(), num(u.memory, has_u).c_str(),
                         num((double)m.used / mib, has_m).c_str(), num((double)m.total / mib, has_m).c_str(),
                         num(temp, has_t).c_str(), num(mw / 1000.0, has_p).c_str(),
                         num(sm, has_sm).c_str(), num(memclk, has_mc).c_str(), pstate, why);
        }
    }

private:
    using Ret = int;
    using Device = void*;
    struct Utilization { unsigned gpu, memory; };
    struct Memory { unsigned long long total, free, used; };
    struct Dev { unsigned index; Device handle; std::string name; };

    void* _lib = nullptr;
    bool _live = false;
    std::vector<Dev> _devices;
    Ret (*_init)() = nullptr;
    Ret (*_shutdown)() = nullptr;
    Ret (*_count)(unsigned*) = nullptr;
    Ret (*_handle)(unsigned, Device*) = nullptr;
    Ret (*_name)(Device, char*, unsigned) = nullptr;
    Ret (*_util)(Device, Utilization*) = nullptr;
    Ret (*_mem)(Device, Memory*) = nullptr;
    Ret (*_temp)(Device, int, unsigned*) = nullptr;
    Ret (*_power)(Device, unsigned*) = nullptr;
    Ret (*_clock)(Device, int, unsigned*) = nullptr;
    Ret (*_pstate)(Device, int*) = nullptr;
    Ret (*_reasons)(Device, unsigned long long*) = nullptr;
};

// One row of system.csv; NaN-free, unknown fields stay empty.
struct MachineSample {
    double cpu_total = -1.0;
    std::vector<double> cores;               // -1 = unknown
    double ram_avail = -1.0, ram_committed = -1.0, ram_total = -1.0;
    double disk_read = -1.0, disk_write = -1.0, disk_busy = -1.0;
    int procs = 0;
    double cpu_pct = -1.0, private_bytes = -1.0, ws_bytes = -1.0, threads = -1.0;
    double read_bps = -1.0, write_bps = -1.0;
};

std::string opt(double v) { return num(v, v >= 0.0); }

// ---------------------------------------------------------------------------
// Windows: PDH for the machine and GPU engines, the process API for our tree
// ---------------------------------------------------------------------------
#if defined(_WIN32)
class Platform {
public:
    explicit Platform(int cores) : _cores(cores) {
        if (PdhOpenQueryW(nullptr, 0, &_query) != ERROR_SUCCESS) { _query = nullptr; return; }
        PdhAddEnglishCounterW(_query, L"\\Processor(*)\\% Processor Time", 0, &_cpu);
        PdhAddEnglishCounterW(_query, L"\\PhysicalDisk(_Total)\\Disk Read Bytes/sec", 0, &_disk_read);
        PdhAddEnglishCounterW(_query, L"\\PhysicalDisk(_Total)\\Disk Write Bytes/sec", 0, &_disk_write);
        PdhAddEnglishCounterW(_query, L"\\PhysicalDisk(_Total)\\% Disk Time", 0, &_disk_busy);
    }
    ~Platform() {
        for (auto& [pid, p] : _procs) CloseHandle(p.handle);
        if (_query) PdhCloseQuery(_query);
    }

    // Fills `s`, appends gpu_engines.csv rows to `engines`.
    void sample(MachineSample& s, double interval_s, std::vector<std::string>& engines) {
        track_processes();
        if (_query) PdhCollectQueryData(_query);
        read_cpu(s);
        s.disk_read = scalar(_disk_read);
        s.disk_write = scalar(_disk_write);
        s.disk_busy = scalar(_disk_busy);

        MEMORYSTATUSEX m{};
        m.dwLength = sizeof m;
        if (GlobalMemoryStatusEx(&m)) {
            s.ram_avail = (double)m.ullAvailPhys;
            s.ram_total = (double)m.ullTotalPhys;
            s.ram_committed = (double)(m.ullTotalPageFile - m.ullAvailPageFile);
        }

        double cpu = 0, priv = 0, ws = 0, threads = 0, rd = 0, wr = 0;
        for (auto& [pid, p] : _procs) {
            FILETIME c, e, k, u;
            IO_COUNTERS io{};
            PROCESS_MEMORY_COUNTERS_EX mem{};
            if (GetProcessTimes(p.handle, &c, &e, &k, &u)) {
                const unsigned long long t = ticks(k) + ticks(u);
                if (p.primed) cpu += (double)(t - p.cpu) / (interval_s * 1e7) * 100.0;
                p.cpu = t;
            }
            if (GetProcessIoCounters(p.handle, &io)) {
                if (p.primed) {
                    rd += (double)(io.ReadTransferCount - p.read) / interval_s;
                    wr += (double)(io.WriteTransferCount - p.write) / interval_s;
                }
                p.read = io.ReadTransferCount;
                p.write = io.WriteTransferCount;
            }
            if (GetProcessMemoryInfo(p.handle, (PROCESS_MEMORY_COUNTERS*)&mem, sizeof mem)) {
                priv += (double)mem.PrivateUsage;
                ws += (double)mem.WorkingSetSize;
            }
            threads += p.threads;
            p.primed = true;
            gpu_rows(pid, p, engines);
        }
        s.procs = (int)_procs.size();
        s.cpu_pct = cpu; s.private_bytes = priv; s.ws_bytes = ws; s.threads = threads;
        s.read_bps = rd; s.write_bps = wr;
    }

private:
    struct Proc {
        HANDLE handle = nullptr;
        unsigned long long cpu = 0, read = 0, write = 0;
        double threads = 0;
        bool primed = false;
        PDH_HCOUNTER gpu = nullptr;
        int empty_reads = 0;
    };

    static unsigned long long ticks(const FILETIME& f) {
        return ((unsigned long long)f.dwHighDateTime << 32) | f.dwLowDateTime;
    }

    double scalar(PDH_HCOUNTER c) const {
        PDH_FMT_COUNTERVALUE v{};
        if (!c || PdhGetFormattedCounterValue(c, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, nullptr, &v) != ERROR_SUCCESS ||
            v.CStatus != ERROR_SUCCESS)
            return -1.0;
        return v.doubleValue;
    }

    template <class F>
    static bool for_each_instance(PDH_HCOUNTER c, F&& f) {
        DWORD bytes = 0, count = 0;
        if (!c || PdhGetFormattedCounterArrayW(c, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &bytes, &count, nullptr) !=
                      (PDH_STATUS)PDH_MORE_DATA)
            return false;
        std::vector<unsigned char> buf(bytes);
        auto* items = (PDH_FMT_COUNTERVALUE_ITEM_W*)buf.data();
        if (PdhGetFormattedCounterArrayW(c, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &bytes, &count, items) != ERROR_SUCCESS)
            return false;
        for (DWORD i = 0; i < count; ++i)
            if (items[i].FmtValue.CStatus == ERROR_SUCCESS) f(items[i].szName, items[i].FmtValue.doubleValue);
        return count > 0;
    }

    void read_cpu(MachineSample& s) {
        s.cores.assign((size_t)_cores, -1.0);
        for_each_instance(_cpu, [&](const wchar_t* name, double v) {
            if (std::wcscmp(name, L"_Total") == 0) { s.cpu_total = v; return; }
            wchar_t* end = nullptr;
            const long i = std::wcstol(name, &end, 10);
            if (end != name && *end == 0 && i >= 0 && i < _cores) s.cores[(size_t)i] = v;
        });
    }

    // This process and everything it started: a dense step or an SfM child.
    void track_processes() {
        const DWORD self = GetCurrentProcessId();
        std::map<DWORD, std::pair<DWORD, DWORD>> parent_threads;
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W e{};
            e.dwSize = sizeof e;
            for (BOOL ok = Process32FirstW(snap, &e); ok; ok = Process32NextW(snap, &e))
                parent_threads[e.th32ProcessID] = {e.th32ParentProcessID, e.cntThreads};
            CloseHandle(snap);
        }
        std::set<DWORD> tree{self};
        for (bool grew = true; grew;) {
            grew = false;
            for (const auto& [pid, pt] : parent_threads)
                if (!tree.count(pid) && tree.count(pt.first) && pid != pt.first) { tree.insert(pid); grew = true; }
        }
        for (auto it = _procs.begin(); it != _procs.end();) {
            if (tree.count(it->first)) { ++it; continue; }
            CloseHandle(it->second.handle);
            if (it->second.gpu) PdhRemoveCounter(it->second.gpu);
            it = _procs.erase(it);
        }
        for (DWORD pid : tree) {
            auto it = _procs.find(pid);
            if (it == _procs.end()) {
                HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
                if (!h) continue;
                Proc p;
                p.handle = h;
                add_gpu_counter(pid, p);
                it = _procs.emplace(pid, p).first;
            }
            const auto pt = parent_threads.find(pid);
            it->second.threads = pt != parent_threads.end() ? (double)pt->second.second : 0.0;
        }
    }

    void add_gpu_counter(DWORD pid, Proc& p) {
        if (!_query) return;
        const std::wstring path = L"\\GPU Engine(pid_" + std::to_wstring(pid) + L"_*)\\Utilization Percentage";
        if (PdhAddEnglishCounterW(_query, path.c_str(), 0, &p.gpu) != ERROR_SUCCESS) p.gpu = nullptr;
    }

    // The busiest engine of each type on each adapter. A process that had no
    // GPU engines when its counter was added gets it re-added now and then.
    void gpu_rows(DWORD pid, Proc& p, std::vector<std::string>& out) {
        std::map<std::pair<std::string, std::string>, double> busiest;
        const bool any = for_each_instance(p.gpu, [&](const wchar_t* wname, double v) {
            const std::string name = utf8(wname);
            const size_t l = name.find("luid_"), ph = name.find("_phys"), et = name.find("engtype_");
            if (l == std::string::npos || ph == std::string::npos || et == std::string::npos || ph < l) return;
            if (!(v >= 0.0 && v <= 100.5)) return;   // Windows glitches when an engine's history resets
            auto& b = busiest[{name.substr(l, ph - l), csv_safe(name.substr(et + 8))}];
            b = std::max(b, v);
        });
        for (const auto& [key, v] : busiest)
            out.push_back(std::to_string(pid) + "," + key.first + "," + key.second + "," + num(v));
        if (any) { p.empty_reads = 0; return; }
        if (++p.empty_reads % 10 == 0 && p.gpu) {
            PdhRemoveCounter(p.gpu);
            p.gpu = nullptr;
            add_gpu_counter(pid, p);
        }
    }

    int _cores;
    PDH_HQUERY _query = nullptr;
    PDH_HCOUNTER _cpu = nullptr, _disk_read = nullptr, _disk_write = nullptr, _disk_busy = nullptr;
    std::map<DWORD, Proc> _procs;
};

std::string cpu_name() {
    wchar_t buf[256] = {};
    DWORD bytes = sizeof buf;
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                     L"ProcessorNameString", RRF_RT_REG_SZ, nullptr, buf, &bytes) == ERROR_SUCCESS)
        return utf8(buf);
    return {};
}

std::vector<std::string> gpu_names() {
    std::vector<std::string> out;
    DISPLAY_DEVICEW d{};
    d.cb = sizeof d;
    for (DWORD i = 0; EnumDisplayDevicesW(nullptr, i, &d, 0); ++i) {
        const std::string n = csv_safe(utf8(d.DeviceString));
        if (!n.empty() && std::find(out.begin(), out.end(), n) == out.end()) out.push_back(n);
        d.cb = sizeof d;
    }
    return out;
}

std::string os_name() { return "Windows"; }

// ---------------------------------------------------------------------------
// Linux: /proc. Our own process only; GPU engines come from NVML alone.
// ---------------------------------------------------------------------------
#elif defined(__linux__)
std::string read_file(const char* path) {
    std::ifstream in(path);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// The value after `key` in a "Key:   1234 kB" file, in bytes when `kib`.
double field(const std::string& text, const char* key, bool kib) {
    const size_t at = text.find(key);
    if (at == std::string::npos) return -1.0;
    const double v = std::strtod(text.c_str() + at + std::strlen(key), nullptr);
    return kib ? v * 1024.0 : v;
}

class Platform {
public:
    explicit Platform(int cores) : _cores(cores) {}

    void sample(MachineSample& s, double interval_s, std::vector<std::string>&) {
        read_cpu(s);
        read_disks(s, interval_s);
        const std::string mem = read_file("/proc/meminfo");
        s.ram_total = field(mem, "MemTotal:", true);
        s.ram_avail = field(mem, "MemAvailable:", true);
        s.ram_committed = field(mem, "Committed_AS:", true);

        s.procs = 1;
        const std::string stat = read_file("/proc/self/stat");
        const size_t paren = stat.rfind(')');
        if (paren != std::string::npos) {
            std::istringstream in(stat.substr(paren + 2));
            std::vector<std::string> f;
            for (std::string t; in >> t;) f.push_back(t);
            if (f.size() > 17) {
                const double hz = (double)sysconf(_SC_CLK_TCK);
                const double t = (std::strtod(f[11].c_str(), nullptr) + std::strtod(f[12].c_str(), nullptr)) / hz;
                if (_cpu_s >= 0.0) s.cpu_pct = (t - _cpu_s) / interval_s * 100.0;
                _cpu_s = t;
                s.threads = std::strtod(f[17].c_str(), nullptr);
            }
        }
        const std::string status = read_file("/proc/self/status");
        s.ws_bytes = field(status, "VmRSS:", true);
        s.private_bytes = field(status, "RssAnon:", true);
        const std::string io = read_file("/proc/self/io");
        const double rd = field(io, "read_bytes:", false), wr = field(io, "write_bytes:", false);
        if (rd >= 0.0 && _read >= 0.0) s.read_bps = (rd - _read) / interval_s;
        if (wr >= 0.0 && _write >= 0.0) s.write_bps = (wr - _write) / interval_s;
        _read = rd;
        _write = wr;
    }

private:
    struct Times { double busy = 0, total = 0; };

    void read_cpu(MachineSample& s) {
        s.cores.assign((size_t)_cores, -1.0);
        std::istringstream in(read_file("/proc/stat"));
        for (std::string line; std::getline(in, line);) {
            if (line.compare(0, 3, "cpu") != 0) break;
            std::istringstream ls(line);
            std::string name;
            ls >> name;
            double v[8] = {};
            for (double& x : v) ls >> x;
            Times now;
            for (double x : v) now.total += x;
            now.busy = now.total - v[3] - v[4];
            const int index = name == "cpu" ? -1 : std::atoi(name.c_str() + 3);
            Times& was = _times[index];
            const double dt = now.total - was.total;
            const double pct = dt > 0 && was.total > 0 ? (now.busy - was.busy) / dt * 100.0 : -1.0;
            was = now;
            if (index < 0) s.cpu_total = pct;
            else if (index < _cores) s.cores[(size_t)index] = pct;
        }
    }

    // Whole disks only: partitions, device-mapper and RAID would count twice.
    void read_disks(MachineSample& s, double interval_s) {
        std::istringstream in(read_file("/proc/diskstats"));
        double read = 0, write = 0, busy = 0;
        bool any = false;
        for (std::string line; std::getline(in, line);) {
            std::istringstream ls(line);
            long major = 0, minor = 0;
            std::string name;
            double f[10] = {};
            if (!(ls >> major >> minor >> name)) continue;
            for (double& x : f) ls >> x;
            if (name.rfind("loop", 0) == 0 || name.rfind("ram", 0) == 0 || name.rfind("zram", 0) == 0 ||
                name.rfind("dm-", 0) == 0 || name.rfind("md", 0) == 0 ||
                access(("/sys/block/" + name).c_str(), F_OK) != 0)
                continue;
            any = true;
            Disk& was = _disks[name];
            const Disk now{f[2] * 512.0, f[6] * 512.0, f[9]};
            if (was.ticks_ms >= 0.0) {
                read += (now.read - was.read) / interval_s;
                write += (now.write - was.write) / interval_s;
                busy = std::max(busy, (now.ticks_ms - was.ticks_ms) / (interval_s * 10.0));
            }
            was = now;
        }
        if (any && _disks_primed) { s.disk_read = read; s.disk_write = write; s.disk_busy = busy; }
        _disks_primed = any;
    }

    struct Disk { double read = 0, write = 0, ticks_ms = -1.0; };
    int _cores;
    std::map<int, Times> _times;
    std::map<std::string, Disk> _disks;
    bool _disks_primed = false;
    double _cpu_s = -1.0, _read = -1.0, _write = -1.0;
};

std::string cpu_name() {
    const std::string info = read_file("/proc/cpuinfo");
    const size_t at = info.find("model name");
    if (at == std::string::npos) return {};
    const size_t colon = info.find(':', at), end = info.find('\n', at);
    return colon < end ? csv_safe(info.substr(colon + 1, end - colon - 1)) : std::string{};
}

std::vector<std::string> gpu_names() { return {}; }

std::string os_name() {
    const std::string rel = read_file("/etc/os-release");
    const size_t at = rel.find("PRETTY_NAME=\"");
    if (at == std::string::npos) return "Linux";
    const size_t end = rel.find('"', at + 13);
    return end == std::string::npos ? "Linux" : rel.substr(at + 13, end - at - 13);
}

// ---------------------------------------------------------------------------
// Elsewhere: memory only.
// ---------------------------------------------------------------------------
#else
class Platform {
public:
    explicit Platform(int) {}
    void sample(MachineSample& s, double, std::vector<std::string>&) {
        s.ram_total = (double)spirula::physicalRamBytes();
        s.ram_avail = (double)spirula::availableRamBytes();
        s.procs = 1;
        s.ws_bytes = (double)spirula::processRamBytes();
    }
};
std::string cpu_name() { return {}; }
std::vector<std::string> gpu_names() { return {}; }
std::string os_name() { return "macOS"; }
#endif

}  // namespace

struct SystemRecorder::Impl {
    fs::path dir;
    double interval_s = 1.0;
    int cores = 1;
    std::FILE* system = nullptr;
    std::FILE* engines = nullptr;
    std::FILE* nvidia = nullptr;
    std::thread thread;
    std::mutex mu;
    std::condition_variable cv;
    bool stopping = false;

    ~Impl() {
        for (std::FILE* f : {system, engines, nvidia})
            if (f) std::fclose(f);
    }

    void write_meta(const std::vector<std::string>& gpus) {
        std::ofstream out(dir / "meta.json");
        out << "{\n  \"started_unix_ms\": " << unix_ms() << ",\n  \"interval_s\": " << interval_s
            << ",\n  \"out\": " << json_str(dir.u8string()) << ",\n  \"cpu\": " << json_str(cpu_name())
            << ",\n  \"logical_processors\": " << cores << ",\n  \"ram_bytes\": " << spirula::physicalRamBytes()
            << ",\n  \"gpus\": [";
        for (size_t i = 0; i < gpus.size(); ++i)
            out << (i ? ", " : "") << "{\"name\": " << json_str(gpus[i]) << "}";
        out << "],\n  \"os\": " << json_str(os_name())
            << ",\n  \"spirula\": {\"recorder\": \"built-in\", \"config\": \"../config.json\"}\n}\n";
    }

    void run(Nvml& nvml) {
        Platform platform(cores);
        std::vector<std::string> engine_rows;
        MachineSample primer;
        platform.sample(primer, interval_s, engine_rows);   // counters need a first reading
        auto next = std::chrono::steady_clock::now();
        auto last = next;
        for (;;) {
            next += std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(interval_s));
            {
                std::unique_lock<std::mutex> lk(mu);
                if (cv.wait_until(lk, next, [&] { return stopping; })) break;
            }
            const auto now = std::chrono::steady_clock::now();
            const double dt = std::max(1e-3, std::chrono::duration<double>(now - last).count());
            last = now;
            if (now > next + std::chrono::seconds(5)) next = now;   // a long stall: do not catch up
            const long long ms = unix_ms();
            MachineSample s;
            engine_rows.clear();
            platform.sample(s, dt, engine_rows);
            std::string row = std::to_string(ms) + "," + opt(s.cpu_total);
            for (double c : s.cores) row += "," + opt(c);
            row += "," + opt(s.ram_avail) + "," + opt(s.ram_committed) + "," + opt(s.ram_total) + "," +
                   opt(s.disk_read) + "," + opt(s.disk_write) + "," + opt(s.disk_busy) + "," +
                   std::to_string(s.procs) + "," + opt(s.cpu_pct) + "," + opt(s.private_bytes) + "," +
                   opt(s.ws_bytes) + "," + opt(s.threads) + "," + opt(s.read_bps) + "," + opt(s.write_bps) + "\n";
            std::fputs(row.c_str(), system);
            std::fflush(system);
            for (const std::string& e : engine_rows) std::fprintf(engines, "%lld,%s\n", ms, e.c_str());
            std::fflush(engines);
            if (nvidia) {
                nvml.sample(nvidia, ms);
                std::fflush(nvidia);
            }
        }
    }
};

SystemRecorder::SystemRecorder() = default;
SystemRecorder::~SystemRecorder() { stop(); }

bool SystemRecorder::running() const { return _impl && _impl->thread.joinable(); }

bool SystemRecorder::start(const fs::path& dir, double interval_s) {
    stop();
    auto impl = std::make_unique<Impl>();
    impl->dir = dir;
    impl->interval_s = std::max(0.1, interval_s);
    impl->cores = std::max(1, (int)std::thread::hardware_concurrency());
    std::error_code ec;
    fs::create_directories(dir, ec);
    impl->system = std::fopen((dir / "system.csv").string().c_str(), "w");
    impl->engines = std::fopen((dir / "gpu_engines.csv").string().c_str(), "w");
    if (!impl->system || !impl->engines) return false;
    std::string header = "unix_ms,cpu_total";
    for (int i = 0; i < impl->cores; ++i) header += ",cpu_" + std::to_string(i);
    header += ",ram_avail_bytes,ram_committed_bytes,ram_total_bytes,disk_read_bps,disk_write_bps,disk_busy_pct,"
              "spirula_procs,spirula_cpu_pct,spirula_private_bytes,spirula_ws_bytes,spirula_threads,"
              "spirula_read_bps,spirula_write_bps\n";
    std::fputs(header.c_str(), impl->system);
    std::fputs("unix_ms,pid,luid,engine,util_pct\n", impl->engines);

    // NVML lives on the sampling thread: opened, read and closed there.
    std::vector<std::string> gpus = gpu_names();
    Impl* raw = impl.get();
    impl->thread = std::thread([raw, gpus]() mutable {
        Nvml nvml;
        if (nvml.open()) {
            raw->nvidia = std::fopen((raw->dir / "nvidia.csv").string().c_str(), "w");
            if (raw->nvidia) std::fputs(Nvml::header(), raw->nvidia);
            for (const std::string& n : nvml.names())
                if (std::find(gpus.begin(), gpus.end(), n) == gpus.end()) gpus.push_back(n);
        }
        raw->write_meta(gpus);
        raw->run(nvml);
    });
    _impl = std::move(impl);
    return true;
}

void SystemRecorder::stop() {
    if (!_impl) return;
    {
        std::lock_guard<std::mutex> lk(_impl->mu);
        _impl->stopping = true;
    }
    _impl->cv.notify_all();
    if (_impl->thread.joinable()) _impl->thread.join();
    _impl.reset();
}

}  // namespace spirula

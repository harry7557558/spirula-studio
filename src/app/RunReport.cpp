// RunReport.cpp -- see RunReport.h.

#include "app/RunReport.h"

#include "core/AtomicFile.h"
#include "core/HostMemory.h"
#include "data/Json.h"
#include "data/JsonWrite.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace spirula {

std::filesystem::path run_report_path(const std::filesystem::path& dir) {
    return dir / "memory.json";
}

void write_run_report(const std::filesystem::path& dir, const RunReport& r) {
    JsonWriter json;
    json.object();
    json.field("sequence", (long long)r.sequence).field("phase", r.phase).field("work", r.work);
    json.field("host_process", (long long)r.host_process).field("host_available", (long long)r.host_available);
    json.field("host_total", (long long)r.host_total).field("host_commit_free", (long long)r.host_commit_free);
    json.field("host_peak", (long long)r.host_peak.mean).field("host_peak_sigma", (long long)r.host_peak.sigma);
    json.field("host_peak_ceiling", r.host_peak.ceiling);
    json.field("device_known", r.device_known);
    json.field("device_ours", (long long)r.device_ours).field("device_others", (long long)r.device_others);
    json.field("device_capacity", (long long)r.device_capacity);
    json.field("device_peak", (long long)r.device_peak.mean).field("device_peak_sigma", (long long)r.device_peak.sigma);
    json.field("device_peak_ceiling", r.device_peak.ceiling);
    json.end();
    const auto destination = run_report_path(dir);
    const auto part = destination.string() + ".part";
    {
        std::ofstream out(part, std::ios::binary | std::ios::trunc);
        out << json.str();
        if (!out.flush()) return;
    }
    try { replace_file(part, destination); } catch (const std::exception&) {}   // a reader holds it; the next write retries
}

bool read_run_report(const std::filesystem::path& dir, RunReport& r) {
    std::ifstream in(run_report_path(dir), std::ios::binary);
    if (!in) return false;
    std::stringstream text;
    text << in.rdbuf();
    try {
        const JsonValue json = json_parse(text.str());
        if (!json.is_object()) return false;
        auto bytes = [&](const char* key) {
            const auto* v = json.find(key);
            return v ? (uint64_t)std::max<int64_t>(0, v->as_int()) : 0;
        };
        auto number = [&](const char* key, double def) {
            const auto* v = json.find(key);
            return v ? v->as_double(def) : def;
        };
        RunReport out;
        out.sequence = bytes("sequence");
        if (const auto* phase = json.find("phase")) out.phase = phase->as_string();
        out.work = number("work", -1.0);
        out.host_process = bytes("host_process"); out.host_available = bytes("host_available");
        out.host_total = bytes("host_total"); out.host_commit_free = bytes("host_commit_free");
        auto flag = [&](const char* key) { const auto* v = json.find(key); return v && v->as_bool(); };
        out.host_peak = {number("host_peak", 0.0), number("host_peak_sigma", 0.0), flag("host_peak_ceiling")};
        if (const auto* known = json.find("device_known")) out.device_known = known->as_bool();
        out.device_ours = bytes("device_ours"); out.device_others = bytes("device_others");
        out.device_capacity = bytes("device_capacity");
        out.device_peak = {number("device_peak", 0.0), number("device_peak_sigma", 0.0), flag("device_peak_ceiling")};
        r = std::move(out);
        return true;
    } catch (const std::exception&) { return false; }
}

RunReporter::RunReporter(const std::string& dir, std::function<bool(Device&)> device)
    : _dir(dir), _device(std::move(device)) {
    if (dir.empty()) return;
    std::error_code error;
    std::filesystem::create_directories(_dir, error);
    std::filesystem::remove(run_report_path(_dir), error);
    _thread = std::thread([this] { run(); });
}

void RunReporter::phase(const char* name) {
    std::lock_guard<std::mutex> lock(_mu);
    _r.phase = name;
}

void RunReporter::work(double w) {
    std::lock_guard<std::mutex> lock(_mu);
    _r.work = w;
}

void RunReporter::host_peak(PlannedPeak p) {
    std::lock_guard<std::mutex> lock(_mu);
    _r.host_peak = p;
}

void RunReporter::device(const Device& d) {
    std::lock_guard<std::mutex> lock(_mu);
    _r.device_known = true;
    _r.device_ours = d.ours;
    _r.device_others = d.others;
    _r.device_capacity = d.capacity;
}

void RunReporter::device_peak(PlannedPeak p) {
    std::lock_guard<std::mutex> lock(_mu);
    _r.device_peak = p;
}

void RunReporter::stop() {
    if (!_thread.joinable()) return;
    {
        std::lock_guard<std::mutex> lock(_mu);
        _quit = true;
    }
    _wake.notify_all();
    _thread.join();
}

void RunReporter::run() {
    std::unique_lock<std::mutex> lock(_mu);
    for (;;) {
        _r.host_process = processRamBytes();
        _r.host_available = availableRamBytes();
        _r.host_total = physicalRamBytes();
        _r.host_commit_free = availableCommitBytes();
        if (_device) {
            Device d;
            lock.unlock();
            const bool ok = _device(d);
            lock.lock();
            if (ok) {
                _r.device_known = true;
                _r.device_ours = d.ours;
                _r.device_others = d.others;
                _r.device_capacity = d.capacity;
            }
        }
        ++_r.sequence;
        write_run_report(_dir, _r);
        if (_wake.wait_for(lock, std::chrono::milliseconds(500), [&] { return _quit; })) return;
    }
}

}  // namespace spirula

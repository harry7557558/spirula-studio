// RecentList.cpp -- see RecentList.h.

#include "app/gui/RecentList.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <thread>

namespace fs = std::filesystem;

namespace gui {

namespace {

constexpr char kPrefix[] = "recent.";
const char* const kKindKeys[kNumRecentKinds] = {
    "dataset", "reconstruction", "model", "run", "camera_project"};

// A folder picked and the same folder dropped are one entry. Lexical only:
// asking the disk would block the UI on a share that has gone away.
std::string normalize(const std::string& path) {
    if (path.empty()) return path;
    fs::path p = fs::u8path(path).lexically_normal();
    p.make_preferred();
    if (p.has_relative_path() && p.filename().empty()) p = p.parent_path();
    return p.u8string();
}

bool all_digits(const std::string& s) {
    return !s.empty() &&
           std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
}

}  // namespace

struct RecentList::Probe {
    std::atomic<bool> done{false};
    std::vector<RecentItem> gone;
    std::vector<RecentItem> classified;
};

void RecentList::insert_by_time(RecentItem item) {
    auto at = std::find_if(_items.begin(), _items.end(),
                           [&](const RecentItem& o) { return o.time < item.time; });
    _items.insert(at, std::move(item));
}

void RecentList::add(RecentKind kind, const std::string& path, int64_t now) {
    const std::string p = normalize(path);
    if (p.empty()) return;
    remove(kind, p);
    RecentItem item;
    item.kind = kind;
    item.path = p;
    item.time = now;
    item.seq = _next_seq++;
    _items.insert(_items.begin(), std::move(item));
    size_t n = 0;
    for (auto it = _items.begin(); it != _items.end();) {
        if (it->kind == kind && ++n > kPerKind) it = _items.erase(it);
        else ++it;
    }
}

void RecentList::remove(RecentKind kind, const std::string& path) {
    const std::string p = normalize(path);
    _items.erase(std::remove_if(_items.begin(), _items.end(),
                                [&](const RecentItem& it) {
                                    return it.kind == kind && it.path == p;
                                }),
                 _items.end());
}

void RecentList::clear(RecentKind kind) {
    _items.erase(std::remove_if(_items.begin(), _items.end(),
                                [&](const RecentItem& it) { return it.kind == kind; }),
                 _items.end());
}

void RecentList::clear() { _items.clear(); }

std::vector<std::string> RecentList::paths(RecentKind kind) const {
    std::vector<std::string> out;
    for (const RecentItem& it : _items)
        if (it.kind == kind) out.push_back(it.path);
    return out;
}

bool RecentList::contains(RecentKind kind, const std::string& path) const {
    const std::string p = normalize(path);
    return std::any_of(_items.begin(), _items.end(), [&](const RecentItem& it) {
        return it.kind == kind && it.path == p;
    });
}

bool RecentList::read_setting(const std::string& key, const std::string& value) {
    RecentKind kind;
    std::string path = value;
    int64_t time = 0;
    if (key == "recent") {
        kind = RecentKind::Dataset;
    } else if (key == "recent_model") {
        kind = RecentKind::Model;
    } else if (key.rfind(kPrefix, 0) == 0) {
        const std::string name = key.substr(sizeof kPrefix - 1);
        int k = 0;
        while (k < kNumRecentKinds && name != kKindKeys[k]) k++;
        if (k == kNumRecentKinds) return true;   // a kind from a newer build
        kind = (RecentKind)k;
        const size_t sp = value.find(' ');
        if (sp != std::string::npos && all_digits(value.substr(0, sp))) {
            time = std::strtoll(value.c_str(), nullptr, 10);
            path = value.substr(sp + 1);
        }
    } else {
        return false;
    }
    path = normalize(path);
    if (path.empty()) return true;
    size_t n = 0;
    for (const RecentItem& o : _items) {
        if (o.kind != kind) continue;
        if (o.path == path) return true;
        n++;
    }
    if (n >= kPerKind) return true;
    RecentItem item;
    item.kind = kind;
    item.path = path;
    item.time = time;
    item.seq = _next_seq++;
    insert_by_time(std::move(item));
    return true;
}

void RecentList::write_settings(std::FILE* f) const {
    for (const RecentItem& it : _items)
        std::fprintf(f, "%s%s=%lld %s\n", kPrefix, kKindKeys[(int)it.kind],
                     (long long)it.time, it.path.c_str());
}

// Detached rather than joined: a stat on a share that has gone away blocks
// for the network timeout, and quitting must not wait that out.
void RecentList::start_probe(Classify classify) {
    if (_probe) return;
    auto probe = std::make_shared<Probe>();
    _probe = probe;
    std::thread([probe, items = _items, classify] {
        for (const RecentItem& it : items) {
            std::error_code ec;
            if (fs::status(fs::u8path(it.path), ec).type() == fs::file_type::not_found) {
                probe->gone.push_back(it);
                continue;
            }
            if (it.kind == RecentKind::Model && it.content == ModelContent::Unknown && classify) {
                RecentItem c = it;
                c.content = classify(it.path);
                probe->classified.push_back(std::move(c));
            }
        }
        probe->done = true;
    }).detach();
}

bool RecentList::poll_probe() {
    if (!_probe || !_probe->done.load()) return false;
    const std::shared_ptr<Probe> probe = std::move(_probe);
    _probe.reset();
    // An entry added again since the probe began is newer than its answer.
    auto same = [](const RecentItem& a, const RecentItem& b) {
        return a.kind == b.kind && a.path == b.path && a.seq == b.seq;
    };
    for (const RecentItem& c : probe->classified)
        for (RecentItem& it : _items)
            if (same(it, c)) it.content = c.content;
    const size_t before = _items.size();
    for (const RecentItem& g : probe->gone)
        _items.erase(std::remove_if(_items.begin(), _items.end(),
                                    [&](const RecentItem& it) { return same(it, g); }),
                     _items.end());
    return _items.size() != before;
}

}  // namespace gui

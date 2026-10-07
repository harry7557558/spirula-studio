#pragma once

// What the home screen lists as recent: datasets, reconstructions, models,
// training runs and camera projects, newest first, kept in gui.conf. A probe
// off the UI thread drops what is no longer on disk -- a share that has gone
// away can take a network timeout to say so.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace gui {

// Persisted by name, so the order is free to change.
enum class RecentKind { Dataset, Reconstruction, Model, Run, Project };
inline constexpr int kNumRecentKinds = 5;

// What a model file turned out to hold; a .ply can be any of the three.
enum class ModelContent { Unknown, Splats, Mesh, Points };

struct RecentItem {
    RecentKind kind = RecentKind::Dataset;
    std::string path;
    int64_t time = 0;          // unix seconds; 0 when an older gui.conf did not say
    ModelContent content = ModelContent::Unknown;   // Model only, never persisted
    uint64_t seq = 0;          // bumped on every add, so a probe cannot undo one
};

class RecentList {
public:
    static constexpr size_t kPerKind = 20;

    // To the front, stamped `now`; the oldest of its kind past kPerKind goes.
    void add(RecentKind kind, const std::string& path, int64_t now);
    void remove(RecentKind kind, const std::string& path);
    void clear(RecentKind kind);
    void clear();

    const std::vector<RecentItem>& items() const { return _items; }
    std::vector<std::string> paths(RecentKind kind) const;
    bool contains(RecentKind kind, const std::string& path) const;

    // One gui.conf line, "recent.<kind>=<time> <path>". False when the key is
    // not ours. "recent" and "recent_model", which carried no time, read too.
    bool read_setting(const std::string& key, const std::string& value);
    void write_settings(std::FILE* f) const;

    // `classify` runs on the probe's thread, once per model it has not seen.
    using Classify = ModelContent (*)(const std::string& path);
    void start_probe(Classify classify);
    bool probing() const { return _probe != nullptr; }
    // Applies a finished probe. True when it removed anything.
    bool poll_probe();

private:
    struct Probe;
    void insert_by_time(RecentItem item);
    std::vector<RecentItem> _items;
    uint64_t _next_seq = 1;
    std::shared_ptr<Probe> _probe;
};

}  // namespace gui

#pragma once

// What an interrupted run left behind, and whether the next one may use it.
//
// A stage's leftovers are reusable when the settings that produced them still
// read the same (SfmConfig::stageSignature) -- so the files carry that text and
// a re-run compares before it trusts them. Everything lives in `<workspace>/
// .resume/`, is safe to delete at any point, and is deleted by a front end
// clearing the intermediates.
//
// Verification is the one stage worth resuming part way through: it is the
// longest, and it is a long list of independent pairs. MatchJournal is the
// append-only record of every pair it finished, kept ONLY until matches.bin is
// written.

#include "sfm/core/Matches.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sfm {
namespace resume {

// Under the workspace, beside features/ and matches.bin. Dotted: the workspace
// is the user's folder, and a plain name there could be theirs.
inline constexpr const char* kDir = ".resume";

std::filesystem::path dir(const std::string& workspace);

// A stage's recorded signature, "" when it has none. `store` writes one and
// `forget` removes it, which is how a stage says its output is no longer valid.
std::string recorded(const std::filesystem::path& file);
void store(const std::filesystem::path& file, const std::string& signature);
void forget(const std::filesystem::path& file);

// Everything under `<workspace>/.resume/`.
void clear(const std::string& workspace);

// The pair list matching settled on, so a resumed run need not select it again
// (a fraction of matching, but not a small one on a large capture). False --
// leaving `pairs` alone -- when the file is absent or carries other settings.
bool readPairs(const std::filesystem::path& file, const std::string& signature,
               std::vector<std::pair<uint32_t, uint32_t>>& pairs);
void writePairs(const std::filesystem::path& file, const std::string& signature,
                const std::vector<std::pair<uint32_t, uint32_t>>& pairs);

// One key per unordered image pair.
inline uint64_t pairKey(uint32_t a, uint32_t b) {
    return a < b ? ((uint64_t)a << 32) | b : ((uint64_t)b << 32) | a;
}

// Every pair verification finished, kept or not: one the journal names is never
// verified twice, one absent from it was never reached (or went with the tail
// the writer had not flushed). Appended from the workers, hence the lock.
class MatchJournal {
public:
    ~MatchJournal() { close(); }

    // Open for appending after `resume` read it, or fresh. False leaves the
    // journal disarmed and every call below a no-op.
    bool open(const std::filesystem::path& file, const std::string& signature,
              bool append);
    // `putative` is what the matcher offered before verification, which only
    // this record can say afterwards -- the summary counts it.
    void record(uint32_t a, uint32_t b, int32_t config, uint32_t putative,
                const uint32_t* idx1, const uint32_t* idx2, size_t stride,
                uint32_t count);
    void flush();
    void close();
    bool armed() const { return _armed; }

private:
    void write_locked(bool force);

    std::mutex _mu;
    std::ofstream _f;
    std::string _buf;
    double _flushed_at = 0;
    bool _armed = false;
};

// What a journal holds: the pairs it finished, and the matches of the ones it
// kept. False (leaving both untouched) when the file is absent, was written for
// other settings, or names other images.
bool readJournal(const std::filesystem::path& file, const std::string& signature,
                 const std::vector<ImageEntry>& images,
                 std::unordered_map<uint64_t, TwoViewMatches>& kept,
                 std::vector<uint64_t>& done, uint64_t& putative);

}  // namespace resume
}  // namespace sfm

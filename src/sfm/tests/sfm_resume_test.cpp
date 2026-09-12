// What a re-run may pick up, and what it must not (D76).
//
// The two halves that decide correctness: a stage signature moves exactly when
// a flag that stage reads moves, and the verification journal reads back what
// it wrote -- including from a tail its writer never flushed, which is how
// every killed run leaves it.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "sfm/SfmConfig.h"
#include "sfm/core/Features.h"
#include "sfm/core/Resume.h"
#include "sfm/tests/TestMain.h"

namespace fs = std::filesystem;
using namespace sfm;

static void check(bool condition, const char* message, int& fails) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    fails++;
}

static fs::path tempDir() {
    const fs::path d = fs::temp_directory_path() / "spirula_sfm_resume_selftest";
    std::error_code ec;
    fs::remove_all(d, ec);
    fs::create_directories(d, ec);
    return d;
}

// ---------------------------------------------------------------------------
// Signatures
// ---------------------------------------------------------------------------

static void testSignatures(int& fails) {
    SfmConfig base;
    base.finalize(CMD_AUTO);

    const std::string ext0 = stageSignature(base, CMD_EXTRACT);
    const std::string mat0 = stageSignature(base, CMD_MATCH);

    // A flag the stage reads moves its signature.
    {
        SfmConfig c = base;
        c.sift.max_num_features = base.sift.max_num_features / 2;
        c.finalize(CMD_AUTO);
        check(stageSignature(c, CMD_EXTRACT) != ext0, "--max-features moves extraction", fails);
    }
    {
        SfmConfig c = base;
        c.twoview.min_num_inliers = base.twoview.min_num_inliers + 5;
        c.finalize(CMD_AUTO);
        check(stageSignature(c, CMD_MATCH) != mat0, "--min-inliers moves matching", fails);
        check(stageSignature(c, CMD_EXTRACT) == ext0, "--min-inliers leaves extraction", fails);
    }
    // A per-group lens is no table row and still reaches the camera setup.
    {
        SfmConfig c = base;
        parseCameraOverride("cam0=opencv-fisheye", OverrideKind::Model, c.camera.overrides);
        check(stageSignature(c, CMD_MATCH) != mat0, "a per-group lens moves matching", fails);
    }
    // How fast the stage runs is not what it produces.
    {
        SfmConfig c = base;
        c.threads = 3;
        c.decode_threads = 3;
        c.device = 1;
        c.quiet = true;
        c.finalize(CMD_AUTO);
        check(stageSignature(c, CMD_EXTRACT) == ext0, "runtime flags leave extraction", fails);
        check(stageSignature(c, CMD_MATCH) == mat0, "runtime flags leave matching", fails);
    }
    // Mapping-only flags leave both earlier stages alone, which is the whole
    // point: re-running the mapper must not cost the matching.
    {
        SfmConfig c = base;
        c.mapper.min_tri_angle_deg = 2.0;
        c.mapper_mode = "bottom-up";
        c.finalize(CMD_AUTO);
        check(stageSignature(c, CMD_EXTRACT) == ext0, "mapper flags leave extraction", fails);
        check(stageSignature(c, CMD_MATCH) == mat0, "mapper flags leave matching", fails);
    }
}

// ---------------------------------------------------------------------------
// The journal
// ---------------------------------------------------------------------------

static void testJournal(const fs::path& dir, int& fails) {
    const fs::path file = dir / "matches.part";
    const std::string sig = "extract=1\nmatch=2\n";
    std::vector<ImageEntry> images = {{"a", 100}, {"b", 100}, {"c", 100}};

    // Three pairs: two kept, one that verification refused.
    const uint32_t idx1[] = {1, 4, 9}, idx2[] = {2, 5, 7};
    {
        resume::MatchJournal j;
        check(j.open(file, sig, /*append=*/false), "journal opens", fails);
        j.record(0, 1, 3, 40, idx1, idx2, 4, 3);
        j.record(0, 2, 0, 12, nullptr, nullptr, 4, 0);
        j.record(1, 2, 2, 30, idx1, idx2, 4, 2);
        j.close();
    }

    std::unordered_map<uint64_t, TwoViewMatches> kept;
    std::vector<uint64_t> done;
    uint64_t putative = 0;
    check(resume::readJournal(file, sig, images, kept, done, putative), "journal reads", fails);
    check(done.size() == 3, "every finished pair is named", fails);
    check(kept.size() == 2, "only the kept pairs carry matches", fails);
    check(putative == 82, "the putative count survives", fails);
    const auto it = kept.find(resume::pairKey(0, 1));
    check(it != kept.end() && it->second.config == 3 && it->second.matches.size() == 3 &&
              it->second.matches[2].idx1 == 9 && it->second.matches[2].idx2 == 7,
          "a kept pair round-trips", fails);
    check(kept.count(resume::pairKey(0, 2)) == 0, "a refused pair keeps no matches", fails);

    // Other settings: the file is not this run's.
    std::unordered_map<uint64_t, TwoViewMatches> other_kept;
    std::vector<uint64_t> other_done;
    uint64_t other_putative = 0;
    check(!resume::readJournal(file, sig + "x", images, other_kept, other_done, other_putative),
          "a signature mismatch refuses the journal", fails);

    // The tail a killed writer never finished: everything before it survives.
    {
        const uintmax_t whole = fs::file_size(file);
        std::vector<char> bytes(whole);
        std::ifstream(file, std::ios::binary).read(bytes.data(), (std::streamsize)whole);
        std::ofstream torn(file, std::ios::binary | std::ios::trunc);
        torn.write(bytes.data(), (std::streamsize)whole - 6);
    }
    kept.clear();
    done.clear();
    putative = 0;
    check(resume::readJournal(file, sig, images, kept, done, putative), "a torn journal reads",
          fails);
    check(done.size() == 2 && kept.size() == 1, "a torn record is dropped, not the file", fails);

    // Appending continues the same file rather than starting one.
    {
        resume::MatchJournal j;
        check(j.open(file, sig, /*append=*/true), "journal reopens", fails);
        j.record(1, 2, 2, 30, idx1, idx2, 4, 2);
        j.close();
    }
    kept.clear();
    done.clear();
    putative = 0;
    resume::readJournal(file, sig, images, kept, done, putative);
    check(done.size() == 3 && kept.size() == 2, "appending keeps what was there", fails);
}

// ---------------------------------------------------------------------------
// The pair list and the feature-file probe
// ---------------------------------------------------------------------------

static void testPairsAndFeatures(const fs::path& dir, int& fails) {
    const fs::path file = dir / "pairs.bin";
    const std::vector<std::pair<uint32_t, uint32_t>> pairs = {{0, 1}, {0, 2}, {1, 2}};
    resume::writePairs(file, "sig", pairs);
    std::vector<std::pair<uint32_t, uint32_t>> back;
    check(resume::readPairs(file, "sig", back) && back == pairs, "the pair list round-trips",
          fails);
    back.clear();
    check(!resume::readPairs(file, "other", back) && back.empty(),
          "a signature mismatch refuses the pair list", fails);

    // peekFeatures is what says a feature file may be reused without reading
    // the descriptors back, so a truncated one has to fail it.
    FeatureSet fs_;
    fs_.width = fs_.extract_width = 640;
    fs_.height = fs_.extract_height = 480;
    fs_.dim = 8;
    fs_.dtype = DType::U8;
    fs_.keypoints.resize(50);
    fs_.descriptors.assign(50 * 8, 7);
    const fs::path feat = dir / "one.bin";
    writeFeatures(feat.string(), fs_);
    uint32_t count = 0;
    check(peekFeatures(feat.string(), count) && count == 50, "a whole feature file probes", fails);
    check(!fs::exists(feat.string() + ".part"), "the write leaves no part file", fails);
    {
        const uintmax_t whole = fs::file_size(feat);
        std::vector<char> bytes(whole);
        std::ifstream(feat, std::ios::binary).read(bytes.data(), (std::streamsize)whole);
        std::ofstream torn(feat, std::ios::binary | std::ios::trunc);
        torn.write(bytes.data(), (std::streamsize)whole - 100);
    }
    count = 0;
    check(!peekFeatures(feat.string(), count), "a truncated feature file is refused", fails);
}

static int cmdResumeTest(int, char**) {
    int fails = 0;
    const fs::path dir = tempDir();
    testSignatures(fails);
    testJournal(dir, fails);
    testPairsAndFeatures(dir, fails);
    std::error_code ec;
    fs::remove_all(dir, ec);
    std::printf("%s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, cmdResumeTest); }

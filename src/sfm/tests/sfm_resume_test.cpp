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

#include "core/RelativePath.h"
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
    // A rig's mates are matched too, so the rig is part of what matching made.
    {
        SfmConfig c = base;
        RigDef d;
        parseRigArg("cam0,cam1", d);
        c.rigs.push_back(d);
        const std::string with_rig = stageSignature(c, CMD_MATCH);
        check(with_rig != mat0, "a rig moves matching", fails);
        check(stageSignature(c, CMD_EXTRACT) == ext0, "a rig leaves extraction", fails);
        parseRigArg("dual-fisheye=cam0,cam1", c.rigs[0]);
        check(stageSignature(c, CMD_MATCH) != with_rig, "which way its lenses face moves it",
              fails);
        c.rig_pairs = false;
        SfmConfig off = base;
        off.rig_pairs = false;
        check(stageSignature(c, CMD_MATCH) == stageSignature(off, CMD_MATCH),
              "without rig pairs a rig leaves matching", fails);
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
    // Moving a workspace keeps extraction signatures identical when masks live inside it.
    {
        check(stageSignature(base, CMD_EXTRACT, "/tmp/old/dataset") == ext0 &&
                  stageSignature(base, CMD_EXTRACT, "/mnt/nas/moved/dataset") == ext0,
              "empty masks keep extract signature invariant across moves", fails);
        SfmConfig a = base, b = base;
        a.mask_dir = "/tmp/old/dataset/masks";
        b.mask_dir = "/mnt/nas/moved/dataset/masks";
        check(stageSignature(a, CMD_EXTRACT, "/tmp/old/dataset") ==
                  stageSignature(b, CMD_EXTRACT, "/mnt/nas/moved/dataset"),
              "workspace-relative masks keep extract signature invariant across moves", fails);
        check(spirula::relative_to_dir("/tmp/old/dataset/images/", "/tmp/old/dataset") == "images",
              "relative_to_dir strips trailing slash", fails);
        check(spirula::relative_to_dir("/tmp/old/video.OSV", "/tmp/old/dataset") == "../video.OSV",
              "relative_to_dir writes sibling file with ../", fails);
        check(spirula::relative_to_dir("/tmp/old/videos/a.OSV", "/tmp/old/dataset") ==
                  "../videos/a.OSV",
              "relative_to_dir keeps a file under the dataset's parent relative", fails);
        const fs::path root = fs::temp_directory_path();
        const fs::path video = root / "videos" / "a.OSV";
        check(spirula::relative_to_dir(video.u8string(), (root / "old" / "dataset").u8string()) ==
                  fs::absolute(video).lexically_normal().u8string(),
              "relative_to_dir keeps a file further away absolute", fails);
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
        check(!j.open(file, sig), "journal opens", fails);
        j.record(0, 1, 3, 40, idx1, idx2, 4, 3);
        j.record(0, 2, 0, 12, nullptr, nullptr, 4, 0);
        j.record(1, 2, 2, 30, idx1 + 1, idx2 + 1, 4, 2);
        j.close();
    }

    std::unordered_map<uint64_t, TwoViewMatches> kept;
    std::vector<uint64_t> done;
    uint64_t putative = 0;
    std::error_code error;
    check(resume::readJournal(file, sig, images, kept, done, putative, error).has_value() &&
              !error,
          "journal reads", fails);
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
    check(!resume::readJournal(file, sig + "x", images, other_kept, other_done, other_putative,
                               error) && !error,
          "a signature mismatch refuses the journal", fails);

    // Tear the last record in its payload, then in its header.
    for (int missing : {6, 33}) {
        fs::resize_file(file, fs::file_size(file) - missing);
        const auto append_at = resume::readJournal(file, sig, images, kept, done, putative, error);
        check(append_at.has_value() && !error, "a torn journal reads", fails);
        check(done.size() == 2 && kept.size() == 1, "a torn record is dropped, not the file", fails);
        {
            resume::MatchJournal j;
            check(!j.open(file, sig, append_at), "journal reopens", fails);
            j.record(1, 2, 2, 30, idx1 + 1, idx2 + 1, 4, 2);
        }
        check(resume::readJournal(file, sig, images, kept, done, putative, error).has_value() &&
                  !error,
              "the appended journal reads", fails);
        check(done.size() == 3 && kept.size() == 2, "appending keeps what was there", fails);
        const auto appended = kept.find(resume::pairKey(1, 2));
        check(appended != kept.end() && appended->second.matches.size() == 2 &&
                  appended->second.matches[0].idx1 == 4 && appended->second.matches[0].idx2 == 5 &&
                  appended->second.matches[1].idx1 == 9 && appended->second.matches[1].idx2 == 7,
              "appended match indices round-trip after a torn write", fails);
    }

    {
        resume::MatchJournal j;
        check(!j.open(file, sig, fs::file_size(file)), "complete journal reopens", fails);
        j.record(0, 3, 0, 0, nullptr, nullptr, 4, 0);
    }
    check(!resume::readJournal(file, sig, images, other_kept, other_done, other_putative, error) &&
              error == std::errc::illegal_byte_sequence && other_done.empty(),
          "an invalid complete record is not repairable tail damage", fails);
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
    bool part_left = false;
    for (const auto& e : fs::directory_iterator(dir))
        part_left |= e.path().filename().string().rfind("one.bin.part", 0) == 0;
    check(!part_left, "the write leaves no part file", fails);
    const uintmax_t whole = fs::file_size(feat);
    std::vector<char> bytes(whole);
    std::ifstream(feat, std::ios::binary).read(bytes.data(), (std::streamsize)whole);
    std::ofstream(feat, std::ios::binary | std::ios::trunc)
        .write(bytes.data(), (std::streamsize)whole - 100);
    count = 0;
    check(!peekFeatures(feat.string(), count), "a truncated feature file is refused", fails);
    {
        std::ofstream oversize(feat, std::ios::binary | std::ios::trunc);
        oversize.write(bytes.data(), (std::streamsize)whole);
        oversize.write(bytes.data(), 100);
    }
    count = 0;
    check(!peekFeatures(feat.string(), count), "an oversize feature file is refused", fails);

    // Colors, a camera id and scores exercise every optional v6 section.
    fs_.colors.assign(50 * 3, 9);
    fs_.exif_camera = "cam";
    for (Keypoint& k : fs_.keypoints) k.response = 1.0f;
    writeFeatures(feat.string(), fs_);
    count = 0;
    check(peekFeatures(feat.string(), count) && count == 50,
          "a feature file with every optional section probes", fails);
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

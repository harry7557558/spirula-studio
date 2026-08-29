// ModelCache.cpp -- see ModelCache.h.

#include "app/gui/ModelCache.h"
#include "i18n/catalog/Log.h"

#include "app/AppPaths.h"
#include "app/gui/Subprocess.h"

#include "i18n/catalog/Dataset.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;
namespace dmsg = spirula::i18n::msg::dataset;

namespace gui {

namespace {

// The sam3.cpp conversions, which are byte-compatible with what this tree
// loads. Mirroring Meta's originals rather than converting them ourselves is
// deliberate: one published artifact, one checksum, one place to look when a
// checkpoint misbehaves.
const char* kBaseUrl = "https://huggingface.co/PABannier/sam3.cpp/resolve/main/";

}  // namespace

std::string human_bytes(uint64_t b) {
    char buf[32];
    if (b >= (1ull << 30)) std::snprintf(buf, sizeof buf, "%.1f GB", b / 1073741824.0);
    else                   std::snprintf(buf, sizeof buf, "%.0f MB", b / 1048576.0);
    return buf;
}

const std::vector<ModelEntry>& model_catalog() {
    // Quantized SAM 3 first: it is the only family that understands "mask out
    // the people", it is a third the size of the f16 weights, and the
    // quantization is of the *file* -- everything is dequantized to fp16 on
    // upload, so the accuracy difference is small and the download is not.
    //
    // The four SAM 2.1 sizes are all here because the choice is a real one and
    // nobody can make it from a name: they differ by ~2.5x in speed and the two
    // in the middle are where most captures want to be. The blurbs quote the
    // same measurement for each (one instance, 1080p frames, laptop GPU) so
    // they can actually be compared -- see src/sam/README.md for the table.
    static const std::vector<ModelEntry> kCatalog = {
        {"sam3-q4_0", "sam3-q4_0.ggml",
         &dmsg::model_sam3_label, &dmsg::model_sam3_blurb,
         "sam3", 707ull << 20, true, "sam3"},
        {"sam3-f16", "sam3-f16.ggml",
         &dmsg::model_sam3_f16_label, &dmsg::model_sam3_f16_blurb,
         "sam3", 1884ull << 20, true, "sam3"},
        {"sam2.1-large", "sam2.1_hiera_large_f16.ggml",
         &dmsg::model_sam21_large_label, &dmsg::model_sam21_large_blurb,
         "sam2", 430ull << 20, false, "sam2.1_hiera_large"},
        {"sam2.1-base-plus", "sam2.1_hiera_base_plus_f16.ggml",
         &dmsg::model_sam21_baseplus_label, &dmsg::model_sam21_baseplus_blurb,
         "sam2", 156ull << 20, false, "sam2.1_hiera_base_plus"},
        {"sam2.1-small", "sam2.1_hiera_small_f16.ggml",
         &dmsg::model_sam21_small_label, &dmsg::model_sam21_small_blurb,
         "sam2", 89ull << 20, false, "sam2.1_hiera_small"},
        {"sam2.1-tiny", "sam2.1_hiera_tiny_f16.ggml",
         &dmsg::model_sam21_tiny_label, &dmsg::model_sam21_tiny_blurb,
         "sam2", 76ull << 20, false, "sam2.1_hiera_tiny"},
    };
    return kCatalog;
}

const ModelEntry* find_model(const std::string& id) {
    for (const auto& e : model_catalog())
        if (id == e.id) return &e;
    return nullptr;
}

const LicenseInfo& license_for(const std::string& family) {
    // Written for someone who has not read a licence before. What they need to
    // know is (a) it is not ours, (b) whether they are agreeing to anything
    // beyond the ordinary, and (c) where the actual text is.
    static const LicenseInfo kSam3{
        "sam3", &dmsg::license_sam3_title, &dmsg::license_sam3_summary,
        "https://github.com/facebookresearch/sam3/blob/main/LICENSE", true};
    static const LicenseInfo kSam2{
        "sam2", &dmsg::license_sam2_title, &dmsg::license_sam2_summary,
        "https://github.com/facebookresearch/sam2/blob/main/LICENSE", false};
    return family == "sam2" ? kSam2 : kSam3;
}

std::string model_path(const ModelEntry& e) {
    return (fs::path(app::cache_dir()) / "models" / e.file).string();
}

bool model_is_cached(const ModelEntry& e) {
    return file_is_cached(model_path(e), e.bytes);
}

bool file_is_cached(const std::string& path, uint64_t bytes) {
    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) return false;
    // The catalog sizes are approximate, so this is a floor, not an equality.
    return fs::file_size(path, ec) > bytes / 2;
}

// ---------------------------------------------------------------------------
// Download
// ---------------------------------------------------------------------------

FileDownload::~FileDownload() {
    cancel();
    if (_worker.joinable()) _worker.join();
}

void FileDownload::start(const std::string& url, const std::string& dest,
                         uint64_t expected_bytes) {
    if (_state.load() == State::Running) return;
    if (_worker.joinable()) _worker.join();
    _cancel = false;
    _progress = -1.0f;
    {
        std::lock_guard<std::mutex> lk(_mu);
        _status.clear();
        _path.clear();
    }
    _state = State::Running;
    _worker = std::thread([this, url, dest, expected_bytes] {
        run(url, dest, expected_bytes);
    });
}

void FileDownload::start(const ModelEntry& e) {
    start(std::string(kBaseUrl) + e.file, model_path(e), e.bytes);
}

void FileDownload::cancel() { _cancel = true; }

std::string FileDownload::status() {
    std::lock_guard<std::mutex> lk(_mu);
    return _status;
}

std::string FileDownload::path() {
    std::lock_guard<std::mutex> lk(_mu);
    return _path;
}

std::vector<std::string> FileDownload::drain_log() {
    std::lock_guard<std::mutex> lk(_mu);
    std::vector<std::string> out;
    out.swap(_log);
    return out;
}

void FileDownload::log(const std::string& line) {
    std::lock_guard<std::mutex> lk(_mu);
    _log.push_back(line);
    if (_log.size() > 500) _log.erase(_log.begin(), _log.begin() + 200);
}

void FileDownload::run(std::string url, std::string dest,
                       uint64_t expected_bytes) {
    auto fail = [&](const std::string& why) {
        std::lock_guard<std::mutex> lk(_mu);
        _status = why;
        _state = _cancel.load() ? State::Cancelled : State::Failed;
    };

    const fs::path dst = dest;
    std::error_code ec;
    fs::create_directories(dst.parent_path(), ec);
    if (ec) return fail("cannot create " + dst.parent_path().string());

    if (!command_exists("curl"))
        return fail("curl was not found. Install curl, or download\n" + url +
                    "\nto " + dst.string() + " by hand.");

    fs::path part = dst;
    part += ".part";

    log("Downloading " + dst.filename().string() + " (" +
        human_bytes(expected_bytes) + ")");
    // -C - resumes a partial .part file, so a cancelled or dropped download
    // does not start over. --progress-bar writes "###...  42.0%" to stderr;
    // that percentage is the only progress the GUI needs.
    const int rc = run_process(
        {"curl", "-L", "-f", "--progress-bar", "-C", "-", "-o", part.string(),
         url},
        "",
        [&](const std::string& line) {
            size_t pct = line.find('%');
            if (pct != std::string::npos) {
                size_t s = pct;
                while (s > 0 && (std::isdigit((unsigned char)line[s - 1]) ||
                                 line[s - 1] == '.'))
                    s--;
                if (s < pct) {
                    const float v = std::strtof(line.substr(s, pct - s).c_str(), nullptr);
                    _progress = v / 100.0f;
                    char pct_s[16];
                    std::snprintf(pct_s, sizeof pct_s, "%.0f", v);
                    std::string text = spirula::i18n::format(
                        spirula::i18n::msg::log::download_percent_of,
                        {pct_s, human_bytes(expected_bytes)});
                    std::lock_guard<std::mutex> lk(_mu);
                    _status = std::move(text);
                }
                return;
            }
            log(line);
        },
        _cancel);

    if (rc == kCancelled) {
        // The .part file stays: -C - picks it up if the user tries again.
        return fail("cancelled");
    }
    if (rc != 0) {
        fs::remove(part, ec);
        return fail("download failed (curl exit " + std::to_string(rc) +
                    "); see the log");
    }

    fs::rename(part, dst, ec);
    if (ec) return fail("cannot move the download into place: " + ec.message());

    _progress = 1.0f;
    {
        std::lock_guard<std::mutex> lk(_mu);
        _path = dst.string();
        _status = "ready";
    }
    log("Saved to " + dst.string());
    _state = State::Done;
}

// ---------------------------------------------------------------------------
// Queue
// ---------------------------------------------------------------------------

void DownloadQueue::start(std::vector<PendingDownload> files) {
    if (running()) return;
    _rest = std::move(files);
    pump();
}

void DownloadQueue::pump() {
    if (running()) return;
    // A part that failed or was stopped makes the rest pointless: half a
    // checkpoint is not a checkpoint.
    if (_dl.state() == FileDownload::State::Failed ||
        _dl.state() == FileDownload::State::Cancelled)
        _rest.clear();
    if (_rest.empty()) return;
    const PendingDownload d = _rest.front();
    _rest.erase(_rest.begin());
    _dl.start(d.url, d.dest, d.bytes);
}

void DownloadQueue::cancel() {
    _rest.clear();
    _dl.cancel();
}

}  // namespace gui

// ModelCache.cpp -- see ModelCache.h.

#include "app/gui/ModelCache.h"
#include "i18n/catalog/Log.h"

#include "app/AppPaths.h"
#include "app/gui/Subprocess.h"
#include "core/ModelMirror.h"

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

// The same bytes and cache names gdino::resolve_model and birefnet::resolve_model
// use, so a file this screen downloads is one they verify and load.
// The mirrors are ModelScope repositories carrying identical files.
const ExtraFile kGdinoTiny{
    "grounding-dino-tiny.safetensors",
    "https://huggingface.co/IDEA-Research/grounding-dino-tiny/resolve/main/model.safetensors",
    "https://modelscope.cn/models/IDEA-Research/grounding-dino-tiny/resolve/master/"
    "model.safetensors",
    689359096ull};
const ExtraFile kGdinoBase{
    "grounding-dino-base.safetensors",
    "https://huggingface.co/IDEA-Research/grounding-dino-base/resolve/main/model.safetensors",
    "https://modelscope.cn/models/IDEA-Research/grounding-dino-base/resolve/master/"
    "model.safetensors",
    933400872ull};
const ExtraFile kBertVocab{
    "bert-base-uncased-vocab.txt",
    "https://huggingface.co/IDEA-Research/grounding-dino-tiny/resolve/main/vocab.txt",
    "https://modelscope.cn/models/IDEA-Research/grounding-dino-tiny/resolve/master/vocab.txt",
    231508ull};

std::string cache_file(const char* name) {
    return (fs::path(app::cache_dir()) / "models" / name).string();
}

}  // namespace

std::string human_bytes(uint64_t b) {
    char buf[32];
    if (b >= (1ull << 30)) std::snprintf(buf, sizeof buf, "%.1f GB", b / 1073741824.0);
    else                   std::snprintf(buf, sizeof buf, "%.0f MB", b / 1048576.0);
    return buf;
}

const std::vector<ModelEntry>& model_catalog() {
    // SAM 3 reads words itself, SAM 2.1 through a TextDetector, BiRefNet not at
    // all. Blurbs give speed ratios, not milliseconds, which are one machine's;
    // src/sam/README.md has the table.
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
#ifdef SS_BUILD_SAM
        // No prompt at all, so nothing the Python fallback can run.
        {"birefnet", "birefnet-general.safetensors",
         &dmsg::model_birefnet_label, &dmsg::model_birefnet_blurb,
         "birefnet", 444473596ull, false, "", MaskModelKind::Subject,
         "https://huggingface.co/ZhengPeng7/BiRefNet/resolve/main/model.safetensors",
         "https://modelscope.cn/models/modelscope/BiRefNet/resolve/master/model.safetensors"},
        {"birefnet-lite", "birefnet-lite.safetensors",
         &dmsg::model_birefnet_lite_label, &dmsg::model_birefnet_lite_blurb,
         "birefnet", 177634392ull, false, "", MaskModelKind::Subject,
         "https://huggingface.co/ZhengPeng7/BiRefNet_lite/resolve/main/model.safetensors",
         "https://modelscope.cn/models/1038lab/BiRefNet/resolve/master/"
         "BiRefNet_lite.safetensors"},
#endif
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
    static const LicenseInfo kGdino{
        "gdino", &dmsg::license_gdino_title, &dmsg::license_gdino_summary,
        "https://github.com/IDEA-Research/GroundingDINO/blob/main/LICENSE", false};
    static const LicenseInfo kBirefnet{
        "birefnet", &dmsg::license_birefnet_title, &dmsg::license_birefnet_summary,
        "https://github.com/ZhengPeng7/BiRefNet/blob/main/LICENSE", false};
    if (family == "gdino") return kGdino;
    if (family == "birefnet") return kBirefnet;
    return family == "sam2" ? kSam2 : kSam3;
}

std::string model_path(const ModelEntry& e) {
    return (fs::path(app::cache_dir()) / "models" / e.file).string();
}

std::string detector_path(const TextDetector& d) { return cache_file(d.weights->file); }

bool model_is_cached(const ModelEntry& e) { return file_is_cached(model_path(e), e.bytes); }

bool detector_is_cached(const TextDetector& d) {
    return file_is_cached(detector_path(d), d.weights->bytes) &&
           file_is_cached(cache_file(d.vocab->file), d.vocab->bytes);
}

const std::vector<TextDetector>& text_detectors() {
    static const std::vector<TextDetector> kDetectors = {
        {"gdino-tiny", &dmsg::model_gdino_tiny_label, &dmsg::model_gdino_tiny_blurb,
         &kGdinoTiny, &kBertVocab},
        {"gdino-base", &dmsg::model_gdino_base_label, &dmsg::model_gdino_base_blurb,
         &kGdinoBase, &kBertVocab},
    };
    return kDetectors;
}

const TextDetector* find_detector(const std::string& id) {
    for (const auto& d : text_detectors())
        if (id == d.id) return &d;
    return nullptr;
}

bool takes_detector(const ModelEntry& e) { return e.kind == MaskModelKind::Sam && !e.text_prompts; }

const TextDetector* detector_for(const ModelEntry& e, const std::string& detector_id) {
    return takes_detector(e) ? find_detector(detector_id) : nullptr;
}

uint64_t missing_download_bytes(const ModelEntry& e, const TextDetector* d) {
    uint64_t n = model_is_cached(e) ? 0 : e.bytes;
    if (d && !detector_is_cached(*d)) n += d->weights->bytes + d->vocab->bytes;
    return n;
}

MaskModelFiles cached_mask_model(const std::string& id, const std::string& detector_id) {
    MaskModelFiles m;
    const ModelEntry* e = find_model(id);
    if (!e) return m;
    const TextDetector* d = detector_for(*e, detector_id);
    m.kind = d ? MaskModelKind::Grounded : e->kind;
    m.text = d || (e->kind == MaskModelKind::Sam && e->text_prompts);
    if (!model_is_cached(*e) || (d && !detector_is_cached(*d))) return m;
    m.model = model_path(*e);
    if (d) m.detector = detector_path(*d);
    return m;
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
                         uint64_t expected_bytes, const std::string& mirror) {
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
    std::vector<std::string> urls{url};
    if (!mirror.empty()) urls.push_back(mirror);
    _worker = std::thread([this, urls, dest, expected_bytes] {
        run(urls, dest, expected_bytes);
    });
}

bool FileDownload::start(const ModelEntry& e, const TextDetector* d) {
    if (!model_is_cached(e)) {
        start(e.url ? std::string(e.url) : std::string(kBaseUrl) + e.file, model_path(e),
              e.bytes, e.mirror ? std::string(e.mirror) : spirula::model_mirror_url(e.file));
        return true;
    }
    if (d)
        for (const ExtraFile* x : {d->weights, d->vocab})
            if (!file_is_cached(cache_file(x->file), x->bytes)) {
                start(x->url, cache_file(x->file), x->bytes, x->mirror);
                return true;
            }
    return false;
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

int FileDownload::fetch(const std::string& url, const std::string& part,
                        uint64_t expected_bytes) {
    // -C - resumes a cancelled download. The timeouts make a blocked host fail
    // over to the mirror instead of hanging. --progress-bar's "42.0%" on stderr
    // is the only progress the GUI needs.
    return run_process(
        {"curl", "-L", "-f", "--progress-bar", "-C", "-", "--connect-timeout", "30",
         "--speed-limit", "1024", "--speed-time", "60", "-o", part, url},
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
}

void FileDownload::run(std::vector<std::string> urls, std::string dest,
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
        return fail("curl was not found. Install curl, or download\n" + urls[0] +
                    "\nto " + dst.string() + " by hand.");

    fs::path part = dst;
    part += ".part";

    log("Downloading " + dst.filename().string() + " (" +
        human_bytes(expected_bytes) + ")");
    int rc = 0;
    for (size_t i = 0; i < urls.size(); i++) {
        if (i > 0) {
            log("Download from " + urls[i - 1] + " failed (curl exit " +
                std::to_string(rc) + "); trying " + urls[i]);
            _progress = -1.0f;
        }
        rc = fetch(urls[i], part.string(), expected_bytes);
        // The .part file stays: -C - picks it up if the user tries again.
        if (rc == kCancelled) return fail("cancelled");
        if (rc == 0) break;
        fs::remove(part, ec);
    }
    if (rc != 0)
        return fail("download failed (curl exit " + std::to_string(rc) +
                    "); see the log");

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
    _dl.start(d.url, d.dest, d.bytes, d.mirror);
}

void DownloadQueue::cancel() {
    _rest.clear();
    _dl.cancel();
}

}  // namespace gui

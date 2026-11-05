// ModelCache.cpp -- see ModelCache.h.

#include "app/gui/ModelCache.h"
#include "i18n/catalog/Log.h"

#include "app/AppPaths.h"
#include "app/ModelLicenses.h"
#include "app/gui/Subprocess.h"
#include "core/LicenseConsent.h"
#include "core/LicenseFamilies.h"
#include "core/ModelMirror.h"
#include "core/AtomicFile.h"
#include "core/Sha256.h"
#include "roma/model/Fetch.h"
#include "i18n/catalog/Dense.h"

#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/Geometry.h"

#include <cstdio>
#include <cstdlib>
#include <deque>
#include <filesystem>

namespace fs = std::filesystem;
namespace dmsg = spirula::i18n::msg::dataset;
namespace lic = spirula::license::family;

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
         lic::kSam3, 707ull << 20, true},
        {"sam3-f16", "sam3-f16.ggml",
         &dmsg::model_sam3_f16_label, &dmsg::model_sam3_f16_blurb,
         lic::kSam3, 1884ull << 20, true},
        {"sam2.1-large", "sam2.1_hiera_large_f16.ggml",
         &dmsg::model_sam21_large_label, &dmsg::model_sam21_large_blurb,
         lic::kSam2, 430ull << 20, false},
        {"sam2.1-base-plus", "sam2.1_hiera_base_plus_f16.ggml",
         &dmsg::model_sam21_baseplus_label, &dmsg::model_sam21_baseplus_blurb,
         lic::kSam2, 156ull << 20, false},
        {"sam2.1-small", "sam2.1_hiera_small_f16.ggml",
         &dmsg::model_sam21_small_label, &dmsg::model_sam21_small_blurb,
         lic::kSam2, 89ull << 20, false},
        {"sam2.1-tiny", "sam2.1_hiera_tiny_f16.ggml",
         &dmsg::model_sam21_tiny_label, &dmsg::model_sam21_tiny_blurb,
         lic::kSam2, 76ull << 20, false},
        {"birefnet", "birefnet-general.safetensors",
         &dmsg::model_birefnet_label, &dmsg::model_birefnet_blurb,
         lic::kBirefnet, 444473596ull, false, MaskModelKind::Subject,
         "https://huggingface.co/ZhengPeng7/BiRefNet/resolve/main/model.safetensors",
         "https://modelscope.cn/models/modelscope/BiRefNet/resolve/master/model.safetensors"},
        {"birefnet-lite", "birefnet-lite.safetensors",
         &dmsg::model_birefnet_lite_label, &dmsg::model_birefnet_lite_blurb,
         lic::kBirefnet, 177634392ull, false, MaskModelKind::Subject,
         "https://huggingface.co/ZhengPeng7/BiRefNet_lite/resolve/main/model.safetensors",
         "https://modelscope.cn/models/1038lab/BiRefNet/resolve/master/"
         "BiRefNet_lite.safetensors"},
    };
    return kCatalog;
}

const ModelEntry* find_model(const std::string& id) {
    for (const auto& e : model_catalog())
        if (id == e.id) return &e;
    return nullptr;
}

namespace {

std::deque<LicenseInfo>& license_infos() {
    // Written for someone who has not read a licence before: whose it is, and where
    // the text is. The text itself is the licensor's, embedded (core/LicenseConsent),
    // and every family's dialog shows all of it and wants the same tick.
    static std::deque<LicenseInfo> v = [] {
        std::deque<LicenseInfo> out;
        const struct { const char* fam; const ::spirula::i18n::Msg *title, *summary; } kBuiltin[] = {
            {lic::kSam3, &dmsg::license_sam3_title, &dmsg::license_sam3_summary},
            {lic::kSam2, &dmsg::license_sam2_title, &dmsg::license_sam2_summary},
            {lic::kGdino, &dmsg::license_gdino_title, &dmsg::license_gdino_summary},
            {lic::kBirefnet, &dmsg::license_birefnet_title, &dmsg::license_birefnet_summary}};
        for (const auto& e : kBuiltin) {
            const spirula::license::Terms* t = spirula::license::terms_for(e.fam);
            out.push_back({e.fam, e.title, e.summary, t->url, t->text});
        }
        return out;
    }();
    return v;
}

}  // namespace

void register_license_info(const char* family, const ::spirula::i18n::Msg* title,
                           const ::spirula::i18n::Msg* summary) {
    const spirula::license::Terms* t = spirula::license::terms_for(family);
    if (!t || license_for(family)) return;
    license_infos().push_back({t->family, title, summary, t->url, t->text});
}

const LicenseInfo* license_for(const std::string& family) {
    for (const LicenseInfo& li : license_infos())
        if (family == li.family) return &li;
    return nullptr;
}

bool accept_license(const std::string& family) {
    return spirula::license::record(family);
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

const ModelEntry& dense_model_entry() {
    const auto& source = spirula::roma::kOfficialCheckpoint;
    static const ModelEntry entry{"romav2.0.1", source.file, &spirula::i18n::msg::dense::title,
        &spirula::i18n::msg::dense::terms, source.license_family, source.bytes, false,
        MaskModelKind::Subject, source.url, source.url, source.sha256};
    return entry;
}

void register_dense_license() {
    app::register_model_licenses();
    register_license_info(dense_model_entry().family, &spirula::i18n::msg::dense::license_title,
                          &spirula::i18n::msg::dense::terms);
}

const ProgressClock& FileDownload::clock() {
    const double now = progress_now();
    const State st = _state.load();
    if (st == State::Done) _clock.finish(now);
    else if (st == State::Running && _progress.load() >= 0.0f) _clock.work(now, _progress.load());
    return _clock;
}

void FileDownload::start(const PendingDownload& d) {
    if (_state.load() == State::Running) return;
    if (_worker.joinable()) _worker.join();
    if (const auto missing = spirula::license::missing(d.license_family); !missing.empty()) {
        std::lock_guard<std::mutex> lk(_mu);
        _status = spirula::i18n::format(spirula::i18n::msg::dataset::license_not_accepted_download,
                                        {missing.front()});
        _path.clear();
        _state = State::Failed;
        return;
    }
    _cancel = false;
    _progress = -1.0f;
    _clock.start(progress_now());
    {
        std::lock_guard<std::mutex> lk(_mu);
        _status.clear();
        _path.clear();
    }
    _state = State::Running;
    if (!d.command.empty()) {
        _worker = std::thread([this, d] { run_command(d.command); });
        return;
    }
    std::vector<std::string> urls{d.url};
    if (!d.mirror.empty()) urls.push_back(d.mirror);
    _worker = std::thread([this, urls, d] { run(urls, d.dest, d.bytes, d.sha256); });
}

void FileDownload::run_command(std::vector<std::string> argv) {
    std::string cmd;
    for (const std::string& a : argv) cmd += (cmd.empty() ? "$ " : " ") + a;
    log(cmd);
    std::string last;
    const int rc = run_process(argv, "", [&](const std::string& raw) {
        std::string line = raw;
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) line.pop_back();
        if (line.empty()) return;
        std::vector<std::string> got;
        if (spirula::i18n::scan(spirula::i18n::msg::geometry::log_install_progress, line, got) &&
            got.size() >= 3) {
            const float done = std::strtof(got[1].c_str(), nullptr);
            const float total = std::strtof(got[2].c_str(), nullptr);
            _progress = total > 0.0f ? std::min(done / total, 1.0f) : -1.0f;
            std::lock_guard<std::mutex> lk(_mu);
            _status = line;
            return;
        }
        last = line;
        log(line);
    }, _cancel);
    std::lock_guard<std::mutex> lk(_mu);
    if (rc == kCancelled) {
        _status = "cancelled";
        _state = State::Cancelled;
    } else if (rc != 0) {
        _status = last.empty() ? "install failed; see the log" : last;
        _state = State::Failed;
    } else {
        _progress = 1.0f;
        _status = "ready";
        _state = State::Done;
    }
}

bool FileDownload::start(const ModelEntry& e, const TextDetector* d) {
    if (!model_is_cached(e)) {
        start(PendingDownload{e.url ? std::string(e.url) : std::string(kBaseUrl) + e.file,
                              model_path(e), e.bytes, spirula::mirror_for(e.file, e.mirror),
                              e.family, e.sha256 ? e.sha256 : ""});
        return true;
    }
    if (d)
        for (const ExtraFile* x : {d->weights, d->vocab})
            if (!file_is_cached(cache_file(x->file), x->bytes)) {
                start(PendingDownload{x->url, cache_file(x->file), x->bytes,
                                      spirula::mirror_for(x->file, x->mirror), lic::kGdino});
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
                       uint64_t expected_bytes, std::string sha256) {
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

    if (!sha256.empty() && spirula::sha256_file(part.string()) != sha256) {
        fs::remove(part, ec);
        return fail("checkpoint SHA-256 mismatch");
    }
    if (_cancel.load()) return fail("cancelled");
    try { spirula::replace_file(part, dst); }
    catch (const std::exception& e) { return fail(e.what()); }

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
    if (running() || files.empty()) return;
    // A new request, not the tail of one that failed or was stopped: pump()
    // would drop it for that.
    _rest.assign(files.begin() + 1, files.end());
    _dl.start(files.front());
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
    _dl.start(d);
}

void DownloadQueue::cancel() {
    _rest.clear();
    _dl.cancel();
}

}  // namespace gui

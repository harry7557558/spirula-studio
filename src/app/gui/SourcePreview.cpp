// SourcePreview.cpp -- see SourcePreview.h.

#include "app/gui/SourcePreview.h"

#include "app/E57Dataset.h"
#include "app/FrameLook.h"
#include "app/FrameMask.h"
#include "app/gui/DatasetPrep.h"
#include "app/gui/HeifPhoto.h"
#include "app/gui/Layout.h"
#include "app/gui/SceneFrame.h"
#include "app/gui/Ui.h"
#include "data/PointCloudFile.h"
#include "data/SparseEdit.h"
#include "external/stb_image.h"
#include "i18n/catalog/SourcePreview.h"
#include "sfm/core/Exif.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <map>

namespace fs = std::filesystem;
namespace pmsg = spirula::i18n::msg::preview;
using spirula::i18n::format;

namespace gui {

namespace {

// What a scan preview draws at most: enough for its shape, little enough to
// upload while the screen stays responsive.
constexpr int64_t kCloudPoints = 1500000;

std::string count_text(long long v) {
    std::string out;
    ui::detail::group_digits(std::to_string(v).c_str(), 0, out);
    return out;
}

std::string size_text(int w, int h) { return std::to_string(w) + " × " + std::to_string(h); }

std::string clock_text(double s) {
    if (!(s >= 0.0)) s = 0.0;
    const long long ds = (long long)std::floor(s * 10.0 + 0.5);
    char b[48];
    std::snprintf(b, sizeof b, "%02lld:%02lld:%02lld.%lld", ds / 36000, (ds / 600) % 60,
                  (ds / 10) % 60, ds % 10);
    return b;
}

std::string fixed(double v, int decimals) {
    char b[48];
    std::snprintf(b, sizeof b, "%.*f", decimals, v);
    return b;
}

std::string lower_ext(const fs::path& p) {
    std::string e = p.extension().string();
    for (char& c : e) c = (char)std::tolower((unsigned char)c);
    return e;
}

// The thumbnail a camera stores in its EXIF block (IFD1), when it is big
// enough for `want` and the photo's own shape: a few KB read instead of a
// whole photograph decoded. Stored pixels, as the photo's are.
bool exif_thumbnail(const std::string& path, int want, int img_w, int img_h, int& w, int& h,
                    std::vector<uint8_t>& rgb) {
    const std::vector<uint8_t> seg = sfm::readExifSegment(path);
    if (seg.size() < 6 + 16) return false;
    sfm::detail::TiffReader r;
    r.p = seg.data() + 6;
    r.n = seg.size() - 6;
    if (r.p[0] == 'M' && r.p[1] == 'M') r.le = false;
    else if (r.p[0] != 'I' || r.p[1] != 'I') return false;
    const uint32_t ifd0 = r.u32(4);
    if (ifd0 == 0 || (size_t)ifd0 + 2 > r.n) return false;
    const uint32_t ifd1 = r.u32((size_t)ifd0 + 2 + (size_t)r.u16(ifd0) * 12);
    if (ifd1 == 0 || (size_t)ifd1 + 2 > r.n) return false;
    double off = 0, len = 0, v = 0;
    for (uint16_t i = 0, n = r.u16(ifd1); i < n; i++) {
        const size_t e = (size_t)ifd1 + 2 + (size_t)i * 12;
        const uint16_t tag = r.u16(e);
        if (tag == 0x0201 && sfm::detail::tiffValue(r, e, v)) off = v;
        if (tag == 0x0202 && sfm::detail::tiffValue(r, e, v)) len = v;
    }
    if (!(off > 0) || !(len > 0) || off + len > (double)r.n) return false;
    int c = 0;
    unsigned char* px = stbi_load_from_memory(r.p + (size_t)off, (int)len, &w, &h, &c, 3);
    if (!px) return false;
    // Too small, or letterboxed into a shape the photo does not have.
    const bool usable = std::max(w, h) * 5 >= want * 4 &&
                        (img_w <= 0 || img_h <= 0 ||
                         std::fabs((double)w / h - (double)img_w / img_h) < 0.02 * w / h);
    if (usable) rgb.assign(px, px + (size_t)w * h * 3);
    stbi_image_free(px);
    return usable;
}

bool load_photo_file(const PreviewSource& base, DecodeGate& gate, bool mask_flipped,
                     const PhotoItem& it, int max_side, bool with_mask, Picture& out,
                     std::string& error, const std::atomic<bool>& cancel) {
    // Only a HEIC photo decodes on the device; the rest need not queue for it.
    std::unique_lock<std::mutex> hold;
    PreviewSource src = base;
    if (is_heif_path(it.path)) src = gated(base, gate, hold);
    else src.builtin_decode = false;
    int w = 0, h = 0;
    std::vector<uint8_t> rgb;
    // An .insp's EXIF thumbnail is the whole frame, not the lens kept.
    const bool thumb_sized = max_side > 0 && max_side <= 400 && !with_mask && src.lens < 0;
    if (thumb_sized && exif_thumbnail(it.path, max_side, it.w, it.h, w, h, rgb)) {
        if (!src.photos_as_stored) app::turn_pixels(app::photo_turn(it.path), 3, rgb, w, h);
    } else {
        PreviewFrame f;
        f.path = it.path;
        if (!load_preview_frame(src, f, std::max(src.lens, 0), w, h, rgb, error, cancel))
            return false;
    }
    std::vector<uint8_t> mask;
    if (with_mask && !it.mask.empty()) {
        int mw = 0, mh = 0;
        std::vector<uint8_t> m;
        if (app::load_stencil(it.mask, mw, mh, m)) {
            if (mask_flipped)
                for (uint8_t& b : m) b = (uint8_t)(255 - b);
            if (!src.photos_as_stored) app::turn_pixels(app::photo_turn(it.path), 1, m, mw, mh);
            mask.resize((size_t)w * h);
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                    mask[(size_t)y * w + x] =
                        m[(size_t)std::min(mh - 1, y * mh / h) * mw + std::min(mw - 1, x * mw / w)];
        }
    }
    make_picture(rgb.data(), w, h, mask.empty() ? nullptr : mask.data(), max_side, out);
    return !out.empty();
}

// One E57 file's images, read for the photo grid by every worker in turn:
// the reader keeps one file position.
struct E57Images {
    std::mutex mu;
    std::unique_ptr<spirula::e57::Reader> reader;
    std::vector<spirula::e57::Image> images;
};

bool load_e57_image(const std::shared_ptr<E57Images>& src, const PhotoItem& it, int max_side,
                    Picture& out, std::string& error) {
    const size_t i = (size_t)std::strtoull(it.path.c_str(), nullptr, 10);
    std::vector<uint8_t> blob;
    {
        std::lock_guard<std::mutex> lk(src->mu);
        if (!src->reader || i >= src->images.size()) return false;
        const spirula::e57::Image& im = src->images[i];
        try {
            blob = src->reader->read_blob(im.png.empty() ? im.jpeg : im.png);
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }
    int w = 0, h = 0, c = 0;
    unsigned char* px = stbi_load_from_memory(blob.data(), (int)blob.size(), &w, &h, &c, 3);
    if (!px) return false;
    make_picture(px, w, h, nullptr, max_side, out);
    stbi_image_free(px);
    return !out.empty();
}

// The data's own frame scaled about its median, as SplatViewer frames a
// point file, so the viewport's navigation sees a scene one unit across.
void frame_cloud(ParsedDataset& ds) {
    float center[3], radius;
    scene_extent(ds.points.xyz, ds.points.num(), center, radius);
    const float unit = view_distance_for(radius);
    ds.train_frame_scale = unit;
    ds.train_to_normalized = {unit, 0, 0, center[0], 0, unit, 0, center[1],
                              0, 0, unit, center[2], 0, 0, 0, 1};
}

void add_camera(ParsedDataset& ds, const app::E57Camera& c, const std::string& name) {
    const bool pano = std::string(c.model) == "EQUIRECTANGULAR";
    ds.camera_models.push_back(pano ? 3 : 0);
    ds.camera_distortions.push_back(0);
    ds.image_filenames.push_back(name);
    ds.widths.push_back((int32_t)c.width);
    ds.heights.push_back((int32_t)c.height);
    for (int k = 0; k < 12; k++) ds.c2w.push_back((float)c.c2w[k]);
    for (double v : {c.fx, c.fy, c.cx, c.cy}) ds.intrins.push_back((float)v);
    for (int k = 0; k < 8; k++) ds.dist_coeffs.push_back(0.0f);
    ds.num_cameras++;
}

}  // namespace

// ===========================================================================
// What the listing thread finds
// ===========================================================================

struct SourcePreview::Listing {
    bool done = false;
    std::string error;
    std::vector<PhotoItem> photos;
    PhotoLoader loader;
    std::vector<std::string> files;          // photo files, for the sensor read
    std::map<std::string, int> formats;
    std::vector<std::pair<std::string, int>> sizes;   // most common first
    int masks = 0;
    // A cloud for the 3D tab: a scan's points or a dataset's model.
    bool cloud = false;
    bool z_up = false;
    ParsedDataset ds;
    PostSplitCameras post;
    int64_t cloud_total = 0;
    // A dataset around the photos, with what its model holds.
    std::string dataset;
    std::string model_error;
    int64_t model_cameras = 0, model_points = 0;
    // A scan.
    std::string format;
    bool colour = false;
    int64_t points = 0, stations = 0, images = 0, posed = 0, pinhole = 0, spherical = 0;
};

SourcePreview::~SourcePreview() { join_listing(); }

void SourcePreview::join_listing() {
    _cancel = true;
    if (_worker.joinable()) _worker.join();
    _cancel = false;
}

std::string SourcePreview::reload_key(const PreviewInput& in) const {
    // Anything that changes what the frames look like, or what decodes them, is a new preview.
    std::string k = std::to_string((int)in.kind) + '\n' + in.path + '\n' + in.mask_dir + '\n' +
                    in.src.device + '\n' + std::to_string(in.src.builtin_decode) + '\n' +
                    in.src.look.lut + '\n' +
                    std::to_string(in.src.look.auto_rotate) + '\n' +
                    std::to_string(in.src.look.packed_lenses) + '\n' +
                    in.src.image_gamut + '\n' + in.src.image_exposure + '\n' +
                    std::to_string(in.src.image_is_linear.value_or(false)) +
                    std::to_string(in.src.image_is_linear.has_value());
    // A video draws both lenses whichever is kept; a photo shows the one kept.
    if (in.kind == PreviewInput::Kind::Photos) k += '\n' + std::to_string(in.src.lens);
    if (in.pano.valid())
        k += '\n' + std::to_string(in.pano_orient.yaw) + ',' + std::to_string(in.pano_orient.pitch) +
             ',' + std::to_string(in.pano_orient.roll);
    return k;
}

void SourcePreview::clear() {
    join_listing();
    _photos.clear();
    _video.clear();
    _sensors.clear();
    _view.detach();
    _cloud_attached = false;
    std::lock_guard<std::mutex> lk(_mu);
    _listing.reset();
    _shown.reset();
    _in = PreviewInput{};
    _key.clear();
    _tab_chosen = false;
    _sensors_asked = false;
}

void SourcePreview::destroy_gl() {
    _photos.destroy_gl();
    _video.destroy_gl();
    _view.detach();
    _view.destroy_gl();
    _cloud_attached = false;
}

void SourcePreview::set_gpu(bool allowed, bool have_ffmpeg) {
    _gate->allowed = allowed;
    _video.set_fallback(have_ffmpeg);
}

void SourcePreview::yield_gpu() {
    _gate->allowed = false;
    _video.abort(true);
    {
        // The decode in flight finishes, and the decoder kept open goes.
        std::lock_guard<std::mutex> wait(_gate->busy);
        _video.drop_decoder();
    }
    _video.abort(false);
}

std::optional<PreviewFrame> SourcePreview::selected_frame() const {
    PreviewFrame f;
    if (_in.kind == PreviewInput::Kind::Video) {
        f.index = _video.current_frame();
        return f;
    }
    const int i = _photos.selected();
    if (_in.kind != PreviewInput::Kind::Photos || i < 0 || i >= (int)_photos.size())
        return std::nullopt;
    f.path = _photos.items()[(size_t)i].path;
    return f;
}

void SourcePreview::show(const PreviewInput& in) {
    const std::string key = reload_key(in);
    _in.kept_fps = in.kept_fps;
    _in.clip_locked = in.clip_locked;
    _in.lut = in.lut;
    if (key == _key) return;
    const PreviewInput::Kind was = _in.kind;
    clear();
    _in = in;
    _key = key;
    if (!_tab_chosen || was != in.kind)
        _tab = in.kind == PreviewInput::Kind::Video  ? Tab::Frames
               : in.kind == PreviewInput::Kind::Scan ? Tab::Cloud
                                                      : Tab::Photos;
    if (in.kind == PreviewInput::Kind::None) return;
    if (in.kind == PreviewInput::Kind::Video) {
        _video.set(key, in.src, in.pano, in.pano_orient);
        _sensors.load_video(key, in.path);
        return;
    }
    start_listing();
}

void SourcePreview::start_listing() {
    const PreviewInput in = _in;
    _worker = std::thread([this, in] {
        auto L = std::make_shared<Listing>();
        auto publish = [&](bool done) {
            auto copy = std::make_shared<Listing>(*L);
            copy->done = done;
            std::lock_guard<std::mutex> lk(_mu);
            if (!_cancel.load()) _listing = std::move(copy);
        };
        try {
            if (in.kind == PreviewInput::Kind::Photos) {
                std::map<std::string, int> sizes;
                for (const DatasetPrep::ImageSize& s : DatasetPrep::image_sizes(in.path, in.mask_dir)) {
                    if (_cancel.load()) return;
                    PhotoItem it;
                    it.name = s.name;
                    it.path = (fs::path(in.path) / s.name).string();
                    if (!in.mask_dir.empty()) it.mask = dsparse::find_aux_file(in.mask_dir, s.name, "mask");
                    it.w = s.w;
                    it.h = s.h;
                    L->masks += !it.mask.empty();
                    L->formats[lower_ext(s.name)]++;
                    if (s.w > 0) sizes[size_text(s.w, s.h)]++;
                    L->files.push_back(it.path);
                    L->photos.push_back(std::move(it));
                }
                L->sizes.assign(sizes.begin(), sizes.end());
                std::sort(L->sizes.begin(), L->sizes.end(),
                          [](const auto& a, const auto& b) { return a.second > b.second; });
                const PreviewSource src = in.src;
                const bool mask_flipped = in.mask_flipped;
                const std::shared_ptr<DecodeGate> gate = _gate;
                L->loader = [src, gate, mask_flipped](const PhotoItem& it, int side, bool mask,
                                                      Picture& out, std::string& err,
                                                      const std::atomic<bool>& cancel) {
                    return load_photo_file(src, *gate, mask_flipped, it, side, mask, out, err,
                                           cancel);
                };
                publish(false);

                // A dataset around them: the folder itself, or the one its
                // images/ sits in. Its model is what the 3D tab shows.
                fs::path root = in.path;
                if (!folder_looks_like_dataset(root.string()) && root.filename() == "images")
                    root = root.parent_path();
                const std::string recon = folder_looks_like_dataset(root.string())
                                              ? spirula::resolve_sparse_dir(root.string())
                                              : std::string();
                if (!recon.empty() && !_cancel.load()) {
                    L->dataset = root.string();
                    try {
                        DatasetParserConfig cfg;
                        cfg.require_image_files = false;
                        L->ds = parse_dataset(recon, cfg, "");
                        L->post = bake_post_split(L->ds, false, false);
                        L->cloud = true;
                        L->model_cameras = L->ds.num_cameras;
                        L->model_points = L->ds.points.num();
                        L->cloud_total = L->model_points;
                    } catch (const std::exception& e) {
                        L->model_error = e.what();
                    }
                }
                publish(true);
                return;
            }

            // A laser scan: what its header says, its images for the photo
            // grid, then its points thinned to what a preview draws.
            spirula::cloud::Reader r(in.path);
            const spirula::cloud::Info& info = r.info();
            L->format = info.format == spirula::cloud::Format::E57   ? "E57"
                        : info.format == spirula::cloud::Format::Las ? "LAS"
                                                                     : "PLY";
            L->colour = info.has_color;
            L->points = info.points;
            L->stations = (int64_t)info.stations.size();
            if (spirula::e57::Reader* e = r.e57()) {
                auto images = std::make_shared<E57Images>();
                images->images = e->images();
                images->reader = std::make_unique<spirula::e57::Reader>(in.path);
                for (size_t i = 0; i < images->images.size(); i++) {
                    const spirula::e57::Image& im = images->images[i];
                    L->images++;
                    L->posed += im.has_pose;
                    L->pinhole += im.projection == spirula::e57::Projection::Pinhole;
                    L->spherical += im.projection == spirula::e57::Projection::Spherical;
                    if (im.jpeg.empty() && im.png.empty()) continue;
                    PhotoItem it;
                    it.name = im.name.empty() ? std::to_string(i) : im.name;
                    it.path = std::to_string(i);
                    it.w = (int)im.width;
                    it.h = (int)im.height;
                    L->photos.push_back(std::move(it));
                }
                L->loader = [images](const PhotoItem& it, int side, bool, Picture& out,
                                     std::string& err, const std::atomic<bool>&) {
                    return load_e57_image(images, it, side, out, err);
                };
            }
            publish(false);

            _points_read = 0;
            app::ScanPreview scan = app::read_scan_preview(in.path, kCloudPoints, &_cancel,
                                                           &_points_read);
            if (_cancel.load()) return;
            for (size_t i = 0; i < scan.cameras.size(); i++)
                add_camera(L->ds, scan.cameras[i], scan.names[i]);
            // Where the scanner stood, when no photograph says so already.
            if (L->ds.num_cameras == 0)
                for (const spirula::cloud::Station& st : info.stations) {
                    app::E57Camera c;
                    c.model = "EQUIRECTANGULAR";
                    c.width = 2048;
                    c.height = 1024;
                    c.fx = c.fy = 2048.0 / (2.0 * 3.14159265358979323846);
                    c.cx = 1024.0;
                    c.cy = 512.0;
                    // The station's z-up frame as an OpenGL camera looking along +y.
                    const double cam[9] = {1, 0, 0, 0, 0, -1, 0, 1, 0};
                    for (int i = 0; i < 3; i++) {
                        for (int j = 0; j < 3; j++) {
                            double v = 0;
                            for (int k = 0; k < 3; k++) v += st.R[i * 3 + k] * cam[k * 3 + j];
                            c.c2w[i * 4 + j] = v;
                        }
                        c.c2w[i * 4 + 3] = st.origin[i];
                    }
                    add_camera(L->ds, c, std::string());
                }
            L->ds.points.xyz = std::move(scan.xyz);
            L->ds.points.rgb = std::move(scan.rgb);
            L->cloud_total = scan.streamed;
            // A scanner levels itself: its frame's z is up, nothing to guess.
            L->z_up = true;
            L->ds.gauge_oriented = true;
            frame_cloud(L->ds);
            L->post = bake_post_split(L->ds, false, false);
            L->cloud = true;
            publish(true);
        } catch (const std::exception& e) {
            L->error = e.what();
            publish(true);
        }
    });
}

SourcePreview::Result SourcePreview::draw(float height, double* clip_start, double* clip_end,
                                           int* active_lens) {
    Result r;
    if (!active()) return r;
    {
        std::lock_guard<std::mutex> lk(_mu);
        if (_listing != _shown) {
            const bool had_cloud = _shown && _shown->cloud;
            _shown = _listing;
            if (!had_cloud) _cloud_attached = false;
        }
    }
    const std::shared_ptr<Listing> L = _shown;
    if (L && !L->photos.empty() && _photos.size() != L->photos.size())
        _photos.set(_key, L->photos, L->loader);
    if (L && L->done && _in.kind == PreviewInput::Kind::Photos && !_sensors_asked) {
        _sensors.load_photos(_key, L->files);
        _sensors_asked = true;
    }

    const float y0 = ImGui::GetCursorPosY();
    draw_header();
    const bool video = _in.kind == PreviewInput::Kind::Video;
    if (video) _video.tick();

    // The tabs this kind of input has. Sensors only once something was
    // found: a phone video with none is no worse for the tab's absence.
    std::vector<std::pair<Tab, const spirula::i18n::Msg*>> tabs;
    const std::shared_ptr<const SensorData> sd = _sensors.data();
    const bool sensors =
        sd && sd->error.empty() &&
        (video ? SensorView::reads_video() && (sd->has_gps || sd->has_imu || !sd->exposure.empty())
               : sd->with_gps > 0 || sd->with_attitude > 0 || sd->with_exposure > 0);
    if (video) tabs.push_back({Tab::Frames, &pmsg::tab_frames});
    if (!video && (!L || !L->photos.empty())) tabs.push_back({Tab::Photos, &pmsg::tab_photos});
    if (_in.kind == PreviewInput::Kind::Scan || (L && L->cloud))
        tabs.push_back({Tab::Cloud, &pmsg::tab_cloud});
    if (sensors) tabs.push_back({Tab::Sensors, &pmsg::tab_sensors});
    tabs.push_back({Tab::Details, &pmsg::tab_details});
    if (std::none_of(tabs.begin(), tabs.end(), [&](const auto& t) { return t.first == _tab; }))
        _tab = tabs.front().first;
    for (size_t i = 0; i < tabs.size(); i++) {
        if (i) ImGui::SameLine();
        if (ui::RadioButton(*tabs[i].second, _tab == tabs[i].first)) {
            _tab = tabs[i].first;
            _tab_chosen = true;
        }
    }
    const float h = std::max(px(80.0f), height - (ImGui::GetCursorPosY() - y0));

    if (L && !L->error.empty() && _tab != Tab::Details) {
        ui::TextColoredWrappedRaw(ImVec4(0.95f, 0.45f, 0.40f, 1.0f), L->error);
        return r;
    }
    switch (_tab) {
        case Tab::Frames: {
            double unused_start = 0.0, unused_end = 0.0;
            const VideoBrowser::Result v = _video.draw(
                h, clip_start ? *clip_start : unused_start, clip_end ? *clip_end : unused_end,
                _in.clip_locked || !clip_start, _in.kept_fps, active_lens);
            r.clip_changed = v.clip_changed && clip_start;
            r.want_gpu = v.want_gpu;
            r.lens_changed = v.lens_changed;
            break;
        }
        case Tab::Photos:
            if (!L) ui::TextDisabled(pmsg::listing_photos);
            else if (L->photos.empty()) ui::TextDisabled(pmsg::no_photos);
            else {
                // On the tabs' row, so `h` still holds.
                if (active_lens) {
                    ImGui::SameLine();
                    r.lens_changed = draw_lens_switches(*active_lens, _in.clip_locked);
                }
                _photos.draw(h, [this](size_t i) { draw_photo_info(i); });
            }
            break;
        case Tab::Sensors: {
            // The video's clock runs here too: the playhead walks the track.
            const bool keys = (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) ||
                               ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) &&
                              !ImGui::GetIO().WantTextInput;
            if (video && keys &&
                ImGui::Shortcut(ImGuiKey_Space,
                                ImGuiInputFlags_RouteFocused | ImGuiInputFlags_RouteFromRootWindow) &&
                _video.can_play())
                _video.play(!_video.playing(), clip_start ? *clip_start : 0.0,
                            clip_end ? *clip_end : 0.0);
            const SensorView::Result s =
                _sensors.draw(h, video ? _video.playhead() : -1.0,
                              video && clip_start ? *clip_start : 0.0,
                              video && clip_end ? *clip_end : 0.0);
            if (s.seek >= 0.0 && video) _video.set_playhead(s.seek);
            if (s.open_photo >= 0) {
                _photos.open((size_t)s.open_photo);
                _tab = Tab::Photos;
            }
            break;
        }
        case Tab::Cloud:
            draw_cloud(h);
            break;
        case Tab::Details:
            ImGui::BeginChild("##details", ImVec2(0, h));
            draw_details();
            ImGui::EndChild();
            break;
    }
    return r;
}

void SourcePreview::draw_header() {
    // A dataset's images/ is called after the dataset.
    const fs::path p(_in.path);
    const std::string name = p.filename() == "images" && p.has_parent_path()
                                 ? (p.parent_path().filename() / p.filename()).generic_string()
                                 : p.filename().string();
    ui::TextRaw(elide_middle(name, ImGui::GetContentRegionAvail().x));
    if (ImGui::IsItemHovered()) ui::SetTooltipRaw(_in.path);
    const std::shared_ptr<Listing> L = _shown;
    switch (_in.kind) {
        case PreviewInput::Kind::Video: {
            const VideoSummary f = _video.facts();
            if (!f.done || !f.error.empty()) break;
            const std::string size = f.sizes.empty() ? std::string("?")
                                                     : size_text(f.sizes[0].first, f.sizes[0].second);
            if (_in.pano.valid())
                ui::TextDisabled(pmsg::sum_video_360, {clock_text(f.seconds), fixed(f.fps, 2)});
            else if (f.sizes.size() > 1)
                ui::TextDisabled(pmsg::sum_video_streams,
                                 {clock_text(f.seconds), fixed(f.fps, 2), (long long)f.sizes.size(), size});
            else
                ui::TextDisabled(pmsg::sum_video, {clock_text(f.seconds), fixed(f.fps, 2), size});
            break;
        }
        case PreviewInput::Kind::Photos:
            if (!L) break;
            if (!L->sizes.empty())
                ui::TextDisabled(pmsg::sum_photos,
                                 {count_text((long long)L->photos.size()), L->sizes.front().first});
            if (!L->dataset.empty() && L->cloud)
                ui::TextDisabled(pmsg::sum_dataset,
                                 {count_text(L->model_cameras), count_text(L->model_points)});
            break;
        case PreviewInput::Kind::Scan:
            if (!L || !L->error.empty()) break;
            ui::TextDisabled(pmsg::sum_scan, {L->format, count_text(L->points),
                                              count_text(L->posed)});
            break;
        case PreviewInput::Kind::None:
            break;
    }
}

void SourcePreview::draw_cloud(float height) {
    const std::shared_ptr<Listing> L = _shown;
    if (!L || !L->done) {
        if (_in.kind == PreviewInput::Kind::Scan && L && L->points > 0)
            ui::TextDisabled(pmsg::reading_points_of,
                             {count_text(_points_read.load()), count_text(L->points)});
        else
            ui::TextDisabled(_in.kind == PreviewInput::Kind::Scan ? pmsg::reading_points
                                                                  : pmsg::reading_model);
        return;
    }
    if (!L->cloud) {
        if (!L->model_error.empty()) ui::TextColoredWrappedRaw(ImVec4(0.95f, 0.45f, 0.40f, 1.0f), L->model_error);
        return;
    }
    if (!_cloud_attached) {
        // Before the attach, which frames the first view about this up.
        if (L->z_up) {
            const float up[3] = {0.0f, 0.0f, 1.0f};
            _view.set_nav_up(up);
        }
        _view.attach_preview_data(L->ds, L->post, "source:" + _key, 1.0f, L->ds.num_cameras > 0);
        _cloud_attached = true;
    }
    if (_in.kind == PreviewInput::Kind::Scan)
        ui::TextDisabled(pmsg::points_shown,
                         {count_text(L->ds.points.num()), count_text(L->cloud_total)});
    else
        ui::TextDisabled(pmsg::model_shown,
                         {count_text(L->model_cameras), count_text(L->model_points)});
    const float h = std::max(px(80.0f), height - ImGui::GetTextLineHeightWithSpacing());
    ImGui::BeginChild("##sourcecloud", ImVec2(0, h));
    _view.draw(/*training=*/false);
    ImGui::EndChild();
}

void SourcePreview::draw_photo_info(size_t i) {
    const std::shared_ptr<Listing> L = _shown;
    if (!L || i >= L->photos.size()) return;
    const PhotoItem& it = L->photos[i];
    if (_in.kind == PreviewInput::Kind::Scan) {
        if (it.w > 0) ui::TextDisabledRaw(size_text(it.w, it.h));
        return;
    }
    if (_info_path != it.path) {
        _info_path = it.path;
        _info_exif = sfm::readExif(it.path);
    }
    const sfm::ExifData& e = _info_exif;
    std::string line = it.w > 0 ? size_text(it.w, it.h) : std::string();
    const std::string camera = e.make.empty() ? e.model
                               : e.model.rfind(e.make, 0) == 0 ? e.model : e.make + " " + e.model;
    if (!camera.empty()) line += (line.empty() ? "" : "  ·  ") + camera;
    ui::TextDisabledRaw(line);
    if (e.focal_mm > 0 || e.focal_35mm > 0) {
        if (e.focal_35mm > 0)
            ui::TextDisabled(pmsg::info_focal35, {fixed(e.focal_mm, 1), fixed(e.focal_35mm, 0)});
        else
            ui::TextDisabled(pmsg::info_focal, {fixed(e.focal_mm, 1)});
        if (e.exposure_time > 0) {
            ImGui::SameLine(0.0f, px(16.0f));
            const std::string t = e.exposure_time < 1.0 ? "1/" + fixed(1.0 / e.exposure_time, 0)
                                                        : fixed(e.exposure_time, 1);
            ui::TextDisabled(pmsg::info_exposure, {t, fixed(e.f_number, 1), fixed(e.iso, 0)});
        }
    }
    if (e.has_gps) {
        ui::TextDisabled(pmsg::tip_position, {fixed(e.lat_deg, 6), fixed(e.lon_deg, 6)});
        if (e.has_alt) {
            ImGui::SameLine(0.0f, px(16.0f));
            ui::TextDisabled(pmsg::tip_altitude, {fixed(e.alt_m, 1)});
        }
    }
}

void SourcePreview::draw_details() {
    const std::shared_ptr<Listing> L = _shown;
    if (!ImGui::BeginTable("##detailtable", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
        return;
    auto row = [](const spirula::i18n::Msg& label, const std::string& value) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ui::TextDisabled(label);
        ImGui::TableNextColumn();
        ImGui::PushTextWrapPos(0.0f);
        ui::TextRaw(value);
        ImGui::PopTextWrapPos();
    };
    row(pmsg::d_path, _in.path);
    if (_in.kind == PreviewInput::Kind::Video) {
        const VideoSummary f = _video.facts();
        if (f.done && f.error.empty()) {
            row(pmsg::d_duration, clock_text(f.seconds));
            row(pmsg::d_frame_rate, fixed(f.fps, 3));
            row(pmsg::d_frames, count_text(f.frames));
            std::string streams;
            for (size_t i = 0; i < f.sizes.size(); i++)
                streams += (i ? ", " : "") + size_text(f.sizes[i].first, f.sizes[i].second);
            row(pmsg::d_streams, std::to_string(f.sizes.size()) + ": " + streams);
            if (!f.codec.empty()) row(pmsg::d_codec, f.codec);
        }
        if (_in.pano.valid())
            row(pmsg::d_pano, std::string(_in.pano.sphere() ? "MAX 2" : "EAC") + ", " +
                                  std::to_string(_in.pano.canvasW()) + " × " +
                                  std::to_string(_in.pano.canvasH()));
        row(pmsg::d_lut, _in.lut.empty() ? std::string("-") : _in.lut);
        if (const std::shared_ptr<const SensorData> d = _sensors.data(); d && d->done) {
            if (!d->carrier.empty()) row(pmsg::d_telemetry, d->carrier);
            if (!d->camera.empty()) row(pmsg::d_camera, d->camera);
            if (d->has_gps) {
                row(pmsg::d_gps_fixes, count_text((long long)d->fixes.size()));
                row(pmsg::d_gps_rate, format(pmsg::hz_value, {fixed(d->rate_hz, 1)}));
                row(pmsg::d_gps_path, format(pmsg::scale_m, {fixed(d->path_m, 0)}));
                row(pmsg::d_gps_spread, format(pmsg::scale_m, {fixed(d->spread_m, 1)}));
            }
            if (d->imu_rate_hz > 0.0)
                row(pmsg::d_imu_rate, format(pmsg::hz_value, {fixed(d->imu_rate_hz, 0)}));
        }
    } else if (L) {
        if (_in.kind == PreviewInput::Kind::Photos) {
            row(pmsg::d_photos, count_text((long long)L->photos.size()));
            std::string formats;
            for (const auto& [ext, n] : L->formats)
                formats += (formats.empty() ? "" : ", ") + ext + " " + count_text(n);
            row(pmsg::d_formats, formats);
            std::string sizes;
            for (size_t i = 0; i < L->sizes.size() && i < 4; i++)
                sizes += (i ? ", " : "") + L->sizes[i].first + " (" + count_text(L->sizes[i].second) + ")";
            if (L->sizes.size() > 4) sizes += ", …";
            row(pmsg::d_sizes, sizes);
            row(pmsg::d_masks, _in.mask_dir.empty() ? std::string("-")
                                                     : count_text(L->masks) + "  " + _in.mask_dir);
            if (const std::shared_ptr<const SensorData> d = _sensors.data(); d && d->done) {
                row(pmsg::d_gps_photos, count_text(d->with_gps));
                row(pmsg::d_attitude_photos, count_text(d->with_attitude));
            }
            if (!L->dataset.empty()) {
                row(pmsg::d_dataset, L->dataset);
                if (L->cloud) {
                    row(pmsg::d_model_cameras, count_text(L->model_cameras));
                    row(pmsg::d_model_points, count_text(L->model_points));
                }
                if (!L->model_error.empty()) row(pmsg::d_model_error, L->model_error);
            }
        } else {
            if (!L->error.empty()) row(pmsg::d_error, L->error);
            row(pmsg::d_format, L->format);
            row(pmsg::d_points, count_text(L->points));
            row(pmsg::d_colour, L->colour ? pmsg::d_yes.get() : pmsg::d_no.get());
            row(pmsg::d_stations, count_text(L->stations));
            if (L->format == "E57") {
                row(pmsg::d_images, format(pmsg::d_images_value, {count_text(L->images),
                                                                  count_text(L->posed)}));
                row(pmsg::d_image_kinds, format(pmsg::d_image_kinds_value,
                                                {count_text(L->pinhole), count_text(L->spherical)}));
            }
        }
    }
    ImGui::EndTable();
}

}  // namespace gui

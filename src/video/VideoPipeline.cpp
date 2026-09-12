#include "video/VideoPipeline.h"

#include "video/Common.h"
#include "nn/core/Error.h"
#include "nn/core/Log.h"
#include "video/VideoApi.h"
#include "nn/vk/Context.h"
#include "nn/vk/EmbeddedSpirv.h"
#include "nn/vk/Memory.h"
#include "nn/vk/Stream.h"

#include <algorithm>
#include <cstring>
#include <mutex>

NN_DECLARE_EMBEDDED_MODULES(video)

namespace video {

// ---------------------------------------------------------------------------
// Extension entry points
// ---------------------------------------------------------------------------

// Resolved per context generation, not once per process. The GUI calls
// nn::shutdown() after every dataset job to hand the GPU back, so a second
// video open runs on a second device -- and a table resolved from the first
// one points at loader trampolines that were unmapped with it, which is a
// segfault inside an unnamed frame the moment the first entry point is called.
const VideoApi& video_api() {
    static std::mutex mu;
    static VideoApi   api{};
    static uint64_t   resolved_gen = 0;  // generations start at 1

    std::lock_guard<std::mutex> lock(mu);
    const uint64_t gen = vk::Context::initialized() ? vk::Context::generation() : 0;
    if (gen == resolved_gen) return api;
    resolved_gen = gen;

    api = [] {
        VideoApi a{};
        if (!vk::Context::initialized()) return a;
        vk::Context& ctx = vk::Context::get();
        auto get = [&](const char* n) { return ctx.deviceProc(n); };
        a.createSession = (PFN_vkCreateVideoSessionKHR)get("vkCreateVideoSessionKHR");
        a.destroySession = (PFN_vkDestroyVideoSessionKHR)get("vkDestroyVideoSessionKHR");
        a.getSessionMemoryRequirements = (PFN_vkGetVideoSessionMemoryRequirementsKHR)get(
            "vkGetVideoSessionMemoryRequirementsKHR");
        a.bindSessionMemory =
            (PFN_vkBindVideoSessionMemoryKHR)get("vkBindVideoSessionMemoryKHR");
        a.createParameters =
            (PFN_vkCreateVideoSessionParametersKHR)get("vkCreateVideoSessionParametersKHR");
        a.updateParameters =
            (PFN_vkUpdateVideoSessionParametersKHR)get("vkUpdateVideoSessionParametersKHR");
        a.destroyParameters = (PFN_vkDestroyVideoSessionParametersKHR)get(
            "vkDestroyVideoSessionParametersKHR");
        a.cmdBeginCoding = (PFN_vkCmdBeginVideoCodingKHR)get("vkCmdBeginVideoCodingKHR");
        a.cmdEndCoding = (PFN_vkCmdEndVideoCodingKHR)get("vkCmdEndVideoCodingKHR");
        a.cmdControlCoding =
            (PFN_vkCmdControlVideoCodingKHR)get("vkCmdControlVideoCodingKHR");
        a.cmdDecode = (PFN_vkCmdDecodeVideoKHR)get("vkCmdDecodeVideoKHR");
        // Physical-device queries are instance-level.
        a.getCapabilities = (PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR)vkGetInstanceProcAddr(
            ctx.instance(), "vkGetPhysicalDeviceVideoCapabilitiesKHR");
        a.getFormatProperties =
            (PFN_vkGetPhysicalDeviceVideoFormatPropertiesKHR)vkGetInstanceProcAddr(
                ctx.instance(), "vkGetPhysicalDeviceVideoFormatPropertiesKHR");
        return a;
    }();
    return api;
}

namespace {

// A decoded picture's plane geometry, derived once from the output format.
struct PlaneInfo {
    int  bytes_per_sample = 1;   // per component
    int  shift = 0;              // value = sample >> shift  (X6 / X4 padding)
    int  sub_x = 1, sub_y = 1;   // chroma subsampling
    bool planar = false;         // Cb and Cr in separate planes
    bool valid = false;
};

PlaneInfo describe_format(VkFormat f) {
    PlaneInfo p{};
    p.valid = true;
    switch (f) {
        case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
            p.bytes_per_sample = 1; p.sub_x = 2; p.sub_y = 2; break;
        case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
            p.bytes_per_sample = 1; p.sub_x = 2; p.sub_y = 1; break;
        case VK_FORMAT_G8_B8R8_2PLANE_444_UNORM:
            p.bytes_per_sample = 1; p.sub_x = 1; p.sub_y = 1; break;
        case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
            p.bytes_per_sample = 1; p.sub_x = 2; p.sub_y = 2; p.planar = true; break;
        case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
            p.bytes_per_sample = 1; p.sub_x = 2; p.sub_y = 1; p.planar = true; break;
        case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:
            p.bytes_per_sample = 1; p.sub_x = 1; p.sub_y = 1; p.planar = true; break;
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
            p.bytes_per_sample = 2; p.shift = 6; p.sub_x = 2; p.sub_y = 2; break;
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
            p.bytes_per_sample = 2; p.shift = 6; p.sub_x = 2; p.sub_y = 1; break;
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16:
            p.bytes_per_sample = 2; p.shift = 6; p.sub_x = 1; p.sub_y = 1; break;
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
            p.bytes_per_sample = 2; p.shift = 4; p.sub_x = 2; p.sub_y = 2; break;
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
            p.bytes_per_sample = 2; p.shift = 4; p.sub_x = 2; p.sub_y = 1; break;
        case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
            p.bytes_per_sample = 2; p.sub_x = 2; p.sub_y = 2; break;
        case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
            p.bytes_per_sample = 2; p.sub_x = 2; p.sub_y = 1; break;
        case VK_FORMAT_R8_UNORM:
            p.bytes_per_sample = 1; p.sub_x = 0; p.sub_y = 0; break;  // monochrome
        default:
            p.valid = false;
            break;
    }
    return p;
}

const char* format_name(VkFormat f) {
    switch (f) {
        case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM: return "nv12";
        case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM: return "nv16";
        case VK_FORMAT_G8_B8R8_2PLANE_444_UNORM: return "nv24";
        case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM: return "yuv420p";
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16: return "p010";
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16: return "p210";
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16: return "p410";
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16: return "p012";
        case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM: return "p016";
        default: return "?";
    }
}

// `want` is a preference, not a requirement: a video session's bind
// requirements sometimes exclude every device-local type, and any type the
// driver accepts is better than failing to open the file.
uint32_t find_memory_type(uint32_t bits, VkMemoryPropertyFlags want, bool required = false) {
    const VkPhysicalDeviceMemoryProperties& mp = vk::Context::get().memoryProps();
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
        if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want) return i;
    if (required) return UINT32_MAX;
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
        if (bits & (1u << i)) return i;
    return UINT32_MAX;
}

uint32_t align_up(uint32_t v, uint32_t a) { return a ? ((v + a - 1) / a) * a : v; }
VkDeviceSize align_up64(VkDeviceSize v, VkDeviceSize a) {
    return a ? ((v + a - 1) / a) * a : v;
}

// BT.601 / 709 / 2020 luma coefficients, selected by the stream's
// matrix_coefficients (ITU-T H.273) with the usual resolution-based fallback.
void color_matrix(int matrix_coefficients, bool full_range, int bit_depth, int height,
                  float m[3][4]) {
    double kr = 0.2126, kb = 0.0722;  // BT.709
    switch (matrix_coefficients) {
        case 1: kr = 0.2126; kb = 0.0722; break;                 // BT.709
        case 4: kr = 0.30;   kb = 0.11;   break;                 // FCC
        case 5:
        case 6: kr = 0.299;  kb = 0.114;  break;                 // BT.601
        case 7: kr = 0.212;  kb = 0.087;  break;                 // SMPTE 240M
        case 9:
        case 10: kr = 0.2627; kb = 0.0593; break;                // BT.2020
        default:
            if (height <= 576) { kr = 0.299; kb = 0.114; }       // SD -> BT.601
            else if (height > 1440) { kr = 0.2627; kb = 0.0593; }
            break;
    }
    const double kg = 1.0 - kr - kb;
    const double peak = (double)((1 << bit_depth) - 1);
    // Sample -> normalized.
    const double y_scale = full_range ? 255.0 / peak : 255.0 / (219.0 * (peak + 1) / 256.0);
    const double y_off = full_range ? 0.0 : 16.0 * (peak + 1) / 256.0;
    const double c_scale = full_range ? 255.0 / peak : 255.0 / (224.0 * (peak + 1) / 256.0);
    const double c_off = 128.0 * (peak + 1) / 256.0;

    const double rv = 2.0 * (1.0 - kr);
    const double bu = 2.0 * (1.0 - kb);
    const double gu = -2.0 * kb * (1.0 - kb) / kg;
    const double gv = -2.0 * kr * (1.0 - kr) / kg;

    // R = y + rv*V ; G = y + gu*U + gv*V ; B = y + bu*U
    const double y_s = y_scale, c_s = c_scale;
    m[0][0] = (float)y_s;  m[0][1] = 0.0f;                m[0][2] = (float)(rv * c_s);
    m[1][0] = (float)y_s;  m[1][1] = (float)(gu * c_s);   m[1][2] = (float)(gv * c_s);
    m[2][0] = (float)y_s;  m[2][1] = (float)(bu * c_s);   m[2][2] = 0.0f;
    // Constant column: subtract the black level from Y and the half-scale bias
    // from both chroma axes, so the shader is one dot product per channel.
    for (int i = 0; i < 3; ++i)
        m[i][3] = (float)(-y_s * y_off - (double)m[i][1] * c_off - (double)m[i][2] * c_off);
}

// Shader parameter blocks (must mirror src/video/shaders/video.slang).
struct YuvParams {
    float m0[4], m1[4], m2[4];
    vk::DevicePtr out, luma, cb, cr;
    uint32_t out_w, out_h, src_w, src_h;
    uint32_t luma_stride, chroma_stride, flags, shift;
    float inv_sx, inv_sy;
    uint32_t groups_per_row;
};
struct ThumbParams {
    vk::DevicePtr out, luma;
    uint32_t src_w, src_h, luma_stride, size, flags, shift, groups_per_row;
};
struct VarianceParams {
    vk::DevicePtr out, thumb;
    uint32_t size, slot;
};

constexpr uint32_t kThumbSize = 512;   // matches extract_frames.py
constexpr int      kCbRing = 4;

}  // namespace

// ---------------------------------------------------------------------------

struct Picture {
    VkImage        image = VK_NULL_HANDLE;
    VkDeviceMemory mem = VK_NULL_HANDLE;
    VkImageView    view = VK_NULL_HANDLE;
    VkImageLayout  layout = VK_IMAGE_LAYOUT_UNDEFINED;
    int      refs = 0;
    uint64_t decode_value = 0;   // video timeline value covering its decode
    uint64_t read_value = 0;     // compute timeline value covering our reads
    int64_t  poc = 0;
    double   pts = 0.0;
    int64_t  decode_index = 0;
    int64_t  display_index = 0;
    bool     metric_queued = false;
};

struct VideoPipeline::Impl {
    std::unique_ptr<Demuxer>      demux;
    std::unique_ptr<CodecDecoder> codec;
    TrackInfo    track_info;
    StreamFormat fmt{};

    VkDevice      dev = VK_NULL_HANDLE;
    VkQueue       vqueue = VK_NULL_HANDLE;
    uint32_t      vfamily = UINT32_MAX;
    uint32_t      cfamily = UINT32_MAX;
    VkCommandPool vpool = VK_NULL_HANDLE;
    VkCommandBuffer cbs[kCbRing] = {};
    uint64_t        cb_value[kCbRing] = {};
    int             cb_cur = 0;
    VkSemaphore     vtimeline = VK_NULL_HANDLE;
    uint64_t        vsubmitted = 0;

    VkVideoSessionKHR           session = VK_NULL_HANDLE;
    VkVideoSessionParametersKHR sparams = VK_NULL_HANDLE;
    std::vector<VkDeviceMemory> session_mem;
    VkVideoCapabilitiesKHR      caps{};
    bool need_reset = true;

    VkFormat  out_format = VK_FORMAT_UNDEFINED;
    VkFormat  dpb_format = VK_FORMAT_UNDEFINED;
    VkImageUsageFlags dpb_usage = 0, out_usage = 0;
    bool      coincide = false;   // decode target IS the DPB slot
    PlaneInfo planes{};
    VkExtent2D coded{};

    VkImage        dpb_image = VK_NULL_HANDLE;
    VkDeviceMemory dpb_mem = VK_NULL_HANDLE;
    std::vector<VkImageView> dpb_views;
    bool dpb_ready = false;

    struct BsBuf {
        VkBuffer       buf = VK_NULL_HANDLE;
        VkDeviceMemory mem = VK_NULL_HANDLE;
        void*          map = nullptr;
        VkDeviceSize   size = 0;
        uint64_t       value = 0;
    };
    BsBuf bs[kCbRing];
    int   bs_cur = 0;

    std::vector<Picture> pool;
    std::vector<int>     free_list;
    std::vector<int>     dpb_pin;      // DPB slot -> pool index, or -1

    std::vector<int> ready;            // pool indices awaiting output
    int64_t decoded = 0;
    bool    eos = false;

    // Conversion scratch.
    vk::DevicePtr luma_buf = 0, chroma_buf = 0, chroma2_buf = 0;
    vk::DevicePtr thumb_buf = 0, metric_buf = 0, rgb_buf = 0;
    VkDeviceSize  rgb_capacity = 0;
    std::vector<float> metrics;
    std::vector<int>   metric_pending;

    std::vector<uint8_t>  bitstream;
    std::vector<uint32_t> slice_offsets;
    Packet packet;              // the coded frame(s) currently being served
    bool   more_in_packet = false;

    ~Impl();
    bool createSession(std::string& error);
    bool createImages(int lookahead, std::string& error);
    bool decodeNext(std::string& error);
    bool recordDecode(int pool_idx, const PictureInfo& pi, std::string& error);
    void waitVideo(uint64_t value);
    int  acquirePicture();
    void releasePool(int idx);
    void copyPlanes(int idx);
    bool ensureRgb(VkDeviceSize bytes);
    uint32_t planeFlags() const;
};

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

std::unique_ptr<CodecDecoder> make_h264_decoder();
std::unique_ptr<CodecDecoder> make_h265_decoder();
std::unique_ptr<CodecDecoder> make_av1_decoder();

std::unique_ptr<CodecDecoder> make_codec_decoder(Codec codec) {
    switch (codec) {
        case Codec::H264: return make_h264_decoder();
        case Codec::H265: return make_h265_decoder();
        case Codec::AV1:  return make_av1_decoder();
        default: return nullptr;
    }
}

VideoPipeline::VideoPipeline() : impl_(new Impl()) {}
VideoPipeline::~VideoPipeline() = default;

std::string VideoPipeline::availability() {
    NN_ENSURE_EMBEDDED_MODULES(video);
    try {
        vk::Context& ctx = vk::Context::get();
        if (!ctx.hasVideoDecode()) {
            const std::string why = ctx.videoUnavailableReason();
            return "Vulkan video decode is unavailable on " + ctx.info().name + ": " +
                   (why.empty() ? "no video-decode queue" : why);
        }
        if (!video_api().complete())
            return "the Vulkan loader did not resolve the VK_KHR_video_decode entry points";
        return "";
    } catch (const std::exception& e) {
        return std::string("Vulkan video decode probe failed: ") + e.what();
    }
}

VideoPipeline::Impl::~Impl() {
    if (!dev) return;
    const VideoApi& api = video_api();
    // Submit before destroying, not just wait. A caller that queued a sharpness
    // metric and then released the frame without fetching it leaves a plane
    // copy recorded in the compute stream's open command buffer, referencing
    // these images; vkDeviceWaitIdle does not submit it, and the next flush
    // would then run over destroyed handles. Same rule as Allocator::free --
    // see src/nn/vk/README.md, "The rule that cost a day".
    vk::Stream::get().sync();
    vkDeviceWaitIdle(dev);
    for (auto& p : pool) {
        if (p.view) vkDestroyImageView(dev, p.view, nullptr);
        if (p.image) vkDestroyImage(dev, p.image, nullptr);
        if (p.mem) vkFreeMemory(dev, p.mem, nullptr);
    }
    for (VkImageView v : dpb_views)
        if (v) vkDestroyImageView(dev, v, nullptr);
    if (dpb_image) vkDestroyImage(dev, dpb_image, nullptr);
    if (dpb_mem) vkFreeMemory(dev, dpb_mem, nullptr);
    for (auto& b : bs) {
        if (b.buf) vkDestroyBuffer(dev, b.buf, nullptr);
        if (b.mem) vkFreeMemory(dev, b.mem, nullptr);
    }
    if (sparams && api.destroyParameters) api.destroyParameters(dev, sparams, nullptr);
    if (session && api.destroySession) api.destroySession(dev, session, nullptr);
    for (VkDeviceMemory m : session_mem) vkFreeMemory(dev, m, nullptr);
    if (vtimeline) vkDestroySemaphore(dev, vtimeline, nullptr);
    if (vpool) vkDestroyCommandPool(dev, vpool, nullptr);
    for (vk::DevicePtr p : {luma_buf, chroma_buf, chroma2_buf, thumb_buf, metric_buf, rgb_buf})
        if (p) vk::device_free(p);
}

bool VideoPipeline::open(const std::string& path, int track, int lookahead,
                         std::string& error) {
    NN_ENSURE_EMBEDDED_MODULES(video);
    Impl& s = *impl_;
    const std::string why = availability();
    if (!why.empty()) {
        error = why + ". Decoding needs a Vulkan device with VK_KHR_video_decode_queue.";
        return false;
    }

    s.demux = open_demuxer(path, error);
    if (!s.demux) return false;
    if (track < 0) track = 0;
    if (track >= (int)s.demux->tracks().size()) {
        error = "'" + path + "' has " + std::to_string(s.demux->tracks().size()) +
                " video track(s); track " + std::to_string(track) + " was requested";
        return false;
    }
    if (!s.demux->selectTrack(track, error)) return false;
    s.track_info = s.demux->tracks()[(size_t)track];

    s.codec = make_codec_decoder(s.track_info.codec);
    if (!s.codec) {
        error = "'" + path + "' uses codec '" + codec_name(s.track_info.codec) +
                "', which this build does not support";
        return false;
    }
    if (!s.codec->init(s.track_info, error)) return false;
    s.fmt = s.codec->format();
    if (s.fmt.chroma_format == 0) {
        error = "'" + path + "' is monochrome; only 4:2:0, 4:2:2 and 4:4:4 are supported";
        return false;
    }

    vk::Context& ctx = vk::Context::get();
    if (!ctx.hasVideoCodec(s.codec->operation())) {
        error = std::string("'") + path + "' is " + codec_name(s.track_info.codec) + " " +
                s.fmt.profile_name + ", but " + ctx.info().name +
                " does not advertise that decode operation";
        return false;
    }
    if (!s.createSession(error)) return false;
    if (!s.createImages(lookahead, error)) return false;

    NN_LOG_INFO("[video] %s: track %d, %s %s %dx%d %d-bit, %s, %.2f fps, %lld frames\n",
                  path.c_str(), track, codec_name(s.track_info.codec), s.fmt.profile_name,
                  s.fmt.width, s.fmt.height, s.fmt.bit_depth, format_name(s.out_format),
                  s.track_info.fps, (long long)s.track_info.frame_count);
    return true;
}

bool VideoPipeline::Impl::createSession(std::string& error) {
    vk::Context& ctx = vk::Context::get();
    const VideoApi& api = video_api();
    dev = ctx.device();
    vqueue = ctx.videoQueue();
    vfamily = ctx.videoQueueFamily();
    cfamily = ctx.queueFamily();

    VkVideoProfileInfoKHR profile{VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR};
    profile.pNext = (void*)codec->profileExt();
    profile.videoCodecOperation = codec->operation();
    switch (fmt.chroma_format) {
        case 0: profile.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_MONOCHROME_BIT_KHR; break;
        case 2: profile.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_422_BIT_KHR; break;
        case 3: profile.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_444_BIT_KHR; break;
        default: profile.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR; break;
    }
    auto depth_bit = [](int bd) {
        switch (bd) {
            case 10: return VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR;
            case 12: return VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR;
            default: return VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
        }
    };
    profile.lumaBitDepth = depth_bit(fmt.bit_depth);
    profile.chromaBitDepth = depth_bit(fmt.bit_depth);

    VkVideoDecodeH264CapabilitiesKHR h264caps{
        VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_CAPABILITIES_KHR};
    VkVideoDecodeH265CapabilitiesKHR h265caps{
        VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_CAPABILITIES_KHR};
    VkVideoDecodeAV1CapabilitiesKHR av1caps{
        VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_CAPABILITIES_KHR};
    VkVideoDecodeCapabilitiesKHR dcaps{VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR};
    switch (codec->operation()) {
        case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR: dcaps.pNext = &h264caps; break;
        case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR: dcaps.pNext = &h265caps; break;
        default: dcaps.pNext = &av1caps; break;
    }
    caps = VkVideoCapabilitiesKHR{VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR};
    caps.pNext = &dcaps;
    VkResult r = api.getCapabilities(ctx.physical(), &profile, &caps);
    if (r != VK_SUCCESS) {
        error = std::string(codec_name(track_info.codec)) + " " + fmt.profile_name + " " +
                std::to_string(fmt.bit_depth) + "-bit " +
                (fmt.chroma_format == 2 ? "4:2:2" : fmt.chroma_format == 3 ? "4:4:4" : "4:2:0") +
                " is not a decodable profile on " + ctx.info().name +
                " (vkGetPhysicalDeviceVideoCapabilitiesKHR: " + vk::Context::resultName(r) + ")";
        return false;
    }
    // Two layouts exist. DISTINCT lets the driver write a standalone output
    // image alongside the reference it sets up; COINCIDE (what NVIDIA reports
    // for H.264/H.265) decodes straight into the DPB slot, so the picture has
    // to be copied out before that slot is recycled. Both are implemented; the
    // difference is one vkCmdCopyImage per frame, ~30 us at 1080p.
    coincide = !(dcaps.flags & VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_DISTINCT_BIT_KHR);
    if (coincide && !(dcaps.flags & VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_COINCIDE_BIT_KHR)) {
        error = std::string(ctx.info().name) +
                " reports neither distinct nor coincident decode output images";
        return false;
    }

    coded.width = align_up((uint32_t)fmt.coded_width, caps.pictureAccessGranularity.width);
    coded.height = align_up((uint32_t)fmt.coded_height, caps.pictureAccessGranularity.height);
    if (coded.width > caps.maxCodedExtent.width || coded.height > caps.maxCodedExtent.height) {
        error = "video is " + std::to_string(coded.width) + "x" + std::to_string(coded.height) +
                " but the device decodes at most " +
                std::to_string(caps.maxCodedExtent.width) + "x" +
                std::to_string(caps.maxCodedExtent.height);
        return false;
    }
    fmt.max_dpb_slots = std::min(fmt.max_dpb_slots, caps.maxDpbSlots);
    fmt.max_active_references = std::min(fmt.max_active_references, caps.maxActiveReferencePictures);

    // ---- picture formats ----
    VkVideoProfileListInfoKHR plist{VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR};
    plist.profileCount = 1;
    plist.pProfiles = &profile;
    auto pick_format = [&](VkImageUsageFlags usage, VkFormat& out) {
        VkPhysicalDeviceVideoFormatInfoKHR fi{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_FORMAT_INFO_KHR};
        fi.pNext = &plist;
        fi.imageUsage = usage;
        uint32_t n = 0;
        if (api.getFormatProperties(ctx.physical(), &fi, &n, nullptr) != VK_SUCCESS || n == 0)
            return false;
        std::vector<VkVideoFormatPropertiesKHR> props(
            n, {VK_STRUCTURE_TYPE_VIDEO_FORMAT_PROPERTIES_KHR});
        if (api.getFormatProperties(ctx.physical(), &fi, &n, props.data()) != VK_SUCCESS)
            return false;
        for (const auto& pr : props)
            if (describe_format(pr.format).valid) {
                out = pr.format;
                return true;
            }
        out = props[0].format;
        return describe_format(out).valid;
    };
    dpb_usage = VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR;
    out_usage = VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (coincide) {
        dpb_usage |= VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        out_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    if (!pick_format(dpb_usage, dpb_format)) {
        error = "no usable DPB picture format";
        return false;
    }
    if (!pick_format(out_usage, out_format)) {
        error = "no decode output format this build can read (expected a 2- or 3-plane "
                "YCbCr format)";
        return false;
    }
    // vkCmdCopyImage between multi-planar images requires identical formats.
    if (coincide) out_format = dpb_format;
    planes = describe_format(out_format);

    // ---- session ----
    VkVideoSessionCreateInfoKHR sci{VK_STRUCTURE_TYPE_VIDEO_SESSION_CREATE_INFO_KHR};
    sci.queueFamilyIndex = vfamily;
    sci.pVideoProfile = &profile;
    sci.pictureFormat = out_format;
    sci.maxCodedExtent = coded;
    sci.referencePictureFormat = dpb_format;
    sci.maxDpbSlots = fmt.max_dpb_slots;
    sci.maxActiveReferencePictures = fmt.max_active_references;
    sci.pStdHeaderVersion = &caps.stdHeaderVersion;
    r = api.createSession(dev, &sci, nullptr, &session);
    if (r != VK_SUCCESS) {
        error = std::string("vkCreateVideoSessionKHR failed: ") + vk::Context::resultName(r);
        return false;
    }

    uint32_t n_req = 0;
    api.getSessionMemoryRequirements(dev, session, &n_req, nullptr);
    std::vector<VkVideoSessionMemoryRequirementsKHR> reqs(
        n_req, {VK_STRUCTURE_TYPE_VIDEO_SESSION_MEMORY_REQUIREMENTS_KHR});
    api.getSessionMemoryRequirements(dev, session, &n_req, reqs.data());
    std::vector<VkBindVideoSessionMemoryInfoKHR> binds(n_req);
    for (uint32_t i = 0; i < n_req; ++i) {
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = reqs[i].memoryRequirements.size;
        ai.memoryTypeIndex = find_memory_type(reqs[i].memoryRequirements.memoryTypeBits,
                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (ai.memoryTypeIndex == UINT32_MAX) {
            error = "no device-local memory type for the video session";
            return false;
        }
        VkDeviceMemory mem = VK_NULL_HANDLE;
        if (vkAllocateMemory(dev, &ai, nullptr, &mem) != VK_SUCCESS) {
            error = "out of memory allocating the video session backing store";
            return false;
        }
        session_mem.push_back(mem);
        binds[i] = {VK_STRUCTURE_TYPE_BIND_VIDEO_SESSION_MEMORY_INFO_KHR};
        binds[i].memoryBindIndex = reqs[i].memoryBindIndex;
        binds[i].memory = mem;
        binds[i].memoryOffset = 0;
        binds[i].memorySize = ai.allocationSize;
    }
    if (n_req && api.bindSessionMemory(dev, session, n_req, binds.data()) != VK_SUCCESS) {
        error = "vkBindVideoSessionMemoryKHR failed";
        return false;
    }

    sparams = codec->createParameters(dev, session, error);
    if (!sparams) return false;

    // ---- command pool and timeline on the video queue ----
    VkCommandPoolCreateInfo cpi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpi.queueFamilyIndex = vfamily;
    if (vkCreateCommandPool(dev, &cpi, nullptr, &vpool) != VK_SUCCESS) {
        error = "cannot create a command pool on the video-decode queue family";
        return false;
    }
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.commandPool = vpool;
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = kCbRing;
    vkAllocateCommandBuffers(dev, &cai, cbs);

    VkSemaphoreTypeCreateInfo sti{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
    sti.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    VkSemaphoreCreateInfo semci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    semci.pNext = &sti;
    vkCreateSemaphore(dev, &semci, nullptr, &vtimeline);

    // ---- bitstream ring ----
    for (auto& b : bs) {
        b.size = align_up64(4u << 20, caps.minBitstreamBufferSizeAlignment);
        VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bci.pNext = &plist;
        bci.size = b.size;
        bci.usage = VK_BUFFER_USAGE_VIDEO_DECODE_SRC_BIT_KHR;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(dev, &bci, nullptr, &b.buf) != VK_SUCCESS) {
            error = "cannot create a video bitstream buffer";
            return false;
        }
        VkMemoryRequirements mr{};
        vkGetBufferMemoryRequirements(dev, b.buf, &mr);
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = mr.size;
        ai.memoryTypeIndex = find_memory_type(mr.memoryTypeBits,
                                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                              /*required=*/true);
        if (ai.memoryTypeIndex == UINT32_MAX ||
            vkAllocateMemory(dev, &ai, nullptr, &b.mem) != VK_SUCCESS) {
            error = "no host-visible memory for the video bitstream buffer";
            return false;
        }
        vkBindBufferMemory(dev, b.buf, b.mem, 0);
        vkMapMemory(dev, b.mem, 0, VK_WHOLE_SIZE, 0, &b.map);
    }
    return true;
}

bool VideoPipeline::Impl::createImages(int lookahead, std::string& error) {
    vk::Context& ctx = vk::Context::get();
    VkVideoProfileInfoKHR profile{VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR};
    profile.pNext = (void*)codec->profileExt();
    profile.videoCodecOperation = codec->operation();
    switch (fmt.chroma_format) {
        case 0: profile.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_MONOCHROME_BIT_KHR; break;
        case 2: profile.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_422_BIT_KHR; break;
        case 3: profile.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_444_BIT_KHR; break;
        default: profile.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR; break;
    }
    auto depth_bit = [](int bd) {
        switch (bd) {
            case 10: return VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR;
            case 12: return VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR;
            default: return VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
        }
    };
    profile.lumaBitDepth = depth_bit(fmt.bit_depth);
    profile.chromaBitDepth = depth_bit(fmt.bit_depth);
    VkVideoProfileListInfoKHR plist{VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR};
    plist.profileCount = 1;
    plist.pProfiles = &profile;

    const uint32_t families[2] = {vfamily, cfamily};

    auto create_image = [&](VkFormat format, uint32_t layers, VkImageUsageFlags usage,
                            bool concurrent, VkImage& image, VkDeviceMemory& mem) {
        VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ici.pNext = &plist;
        ici.imageType = VK_IMAGE_TYPE_2D;
        ici.format = format;
        ici.extent = {coded.width, coded.height, 1};
        ici.mipLevels = 1;
        ici.arrayLayers = layers;
        ici.samples = VK_SAMPLE_COUNT_1_BIT;
        ici.tiling = VK_IMAGE_TILING_OPTIMAL;
        ici.usage = usage;
        if (concurrent && vfamily != cfamily) {
            ici.sharingMode = VK_SHARING_MODE_CONCURRENT;
            ici.queueFamilyIndexCount = 2;
            ici.pQueueFamilyIndices = families;
        } else {
            ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }
        ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (vkCreateImage(dev, &ici, nullptr, &image) != VK_SUCCESS) return false;
        VkMemoryRequirements mr{};
        vkGetImageMemoryRequirements(dev, image, &mr);
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = mr.size;
        ai.memoryTypeIndex =
            find_memory_type(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (ai.memoryTypeIndex == UINT32_MAX) return false;
        if (vkAllocateMemory(dev, &ai, nullptr, &mem) != VK_SUCCESS) return false;
        return vkBindImageMemory(dev, image, mem, 0) == VK_SUCCESS;
    };
    auto create_view = [&](VkImage image, VkFormat format, uint32_t layer, VkImageView& view) {
        VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vci.image = image;
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format = format;
        vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, layer, 1};
        return vkCreateImageView(dev, &vci, nullptr, &view) == VK_SUCCESS;
    };

    // The DPB is one array image: a single image with maxDpbSlots layers is
    // valid on every implementation, whereas separate reference images are an
    // optional capability.
    if (!create_image(dpb_format, fmt.max_dpb_slots, dpb_usage, false, dpb_image, dpb_mem)) {
        error = "cannot allocate the video reference picture buffer";
        return false;
    }
    dpb_views.resize(fmt.max_dpb_slots);
    for (uint32_t i = 0; i < fmt.max_dpb_slots; ++i)
        if (!create_view(dpb_image, dpb_format, i, dpb_views[i])) {
            error = "cannot create a DPB image view";
            return false;
        }
    dpb_pin.assign(fmt.max_dpb_slots, -1);

    // Output pictures: enough for the reorder queue, the caller's window, the
    // AV1 no-show frames a later show_existing_frame may re-output, and two in
    // flight.
    const uint32_t n_pool =
        fmt.max_reorder + (uint32_t)std::max(lookahead, 1) + fmt.max_dpb_slots + 4;
    pool.resize(n_pool);
    for (uint32_t i = 0; i < n_pool; ++i) {
        if (!create_image(out_format, 1, out_usage, true, pool[i].image, pool[i].mem) ||
            !create_view(pool[i].image, out_format, 0, pool[i].view)) {
            error = "cannot allocate decode output picture " + std::to_string(i);
            return false;
        }
        free_list.push_back((int)i);
    }
    metrics.assign(n_pool, 0.0f);

    // Plane scratch buffers, sized for the coded picture.
    const VkDeviceSize luma_bytes =
        (VkDeviceSize)coded.width * coded.height * planes.bytes_per_sample;
    const int cw = planes.sub_x ? (int)coded.width / planes.sub_x : 0;
    const int ch = planes.sub_y ? (int)coded.height / planes.sub_y : 0;
    const VkDeviceSize chroma_bytes =
        (VkDeviceSize)cw * ch * planes.bytes_per_sample * (planes.planar ? 1 : 2);
    luma_buf = vk::device_alloc(luma_bytes + 16, "video.luma");
    if (chroma_bytes) {
        chroma_buf = vk::device_alloc(chroma_bytes + 16, "video.chroma");
        if (planes.planar) chroma2_buf = vk::device_alloc(chroma_bytes + 16, "video.chroma2");
    }
    thumb_buf = vk::device_alloc((VkDeviceSize)kThumbSize * kThumbSize * 4, "video.thumb");
    metric_buf = vk::device_alloc((VkDeviceSize)n_pool * 4, "video.metrics");

    NN_LOG_DEBUG("[video] dpb %u slots, %u output pictures, %ux%u coded\n",
                   fmt.max_dpb_slots, n_pool, coded.width, coded.height);
    (void)ctx;
    return true;
}

// ---------------------------------------------------------------------------
// Decode
// ---------------------------------------------------------------------------

void VideoPipeline::Impl::waitVideo(uint64_t value) {
    if (value == 0) return;
    uint64_t v = 0;
    vkGetSemaphoreCounterValue(dev, vtimeline, &v);
    if (v >= value) return;
    VkSemaphoreWaitInfo wi{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
    wi.semaphoreCount = 1;
    wi.pSemaphores = &vtimeline;
    wi.pValues = &value;
    vkWaitSemaphores(dev, &wi, UINT64_MAX);
}

int VideoPipeline::Impl::acquirePicture() {
    if (free_list.empty()) return -1;
    const int idx = free_list.back();
    free_list.pop_back();
    pool[(size_t)idx].refs = 1;
    pool[(size_t)idx].metric_queued = false;
    return idx;
}

void VideoPipeline::Impl::releasePool(int idx) {
    Picture& p = pool[(size_t)idx];
    if (p.refs <= 0) return;   // already free: a double release must not duplicate it
    if (--p.refs > 0) return;
    free_list.push_back(idx);
}

uint32_t VideoPipeline::Impl::planeFlags() const {
    uint32_t f = 0;
    if (planes.bytes_per_sample == 2) f |= 1u << 0;
    if (planes.sub_x == 2) f |= 1u << 1;
    if (planes.sub_y == 2) f |= 1u << 2;
    if (planes.planar) f |= 1u << 3;
    return f;
}

bool VideoPipeline::Impl::recordDecode(int pool_idx, const PictureInfo& pi,
                                       std::string& error) {
    const VideoApi& api = video_api();
    Picture& dst = pool[(size_t)pool_idx];
    if (coincide && pi.setup_slot < 0) {
        error = "coincident decode requires every picture to activate a DPB slot";
        return false;
    }

    // The ring slot's previous decode must have retired before we reset its
    // command buffer, and any conversion still reading this picture must be
    // done before the driver writes it again.
    waitVideo(cb_value[cb_cur]);
    waitVideo(bs[(size_t)bs_cur].value);
    if (dst.read_value) {
        // A conversion may still be reading this picture on the compute queue.
        // The two queues share no timeline, so this one point is a host wait --
        // it only triggers when the caller released a frame without ever
        // fetching its metric, which is rare.
        vk::Stream::get().flush();
        VkSemaphore sem = vk::Stream::get().timeline();
        uint64_t v = 0;
        vkGetSemaphoreCounterValue(dev, sem, &v);
        if (v < dst.read_value) {
            VkSemaphoreWaitInfo wi{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
            wi.semaphoreCount = 1;
            wi.pSemaphores = &sem;
            wi.pValues = &dst.read_value;
            vkWaitSemaphores(dev, &wi, UINT64_MAX);
        }
        dst.read_value = 0;
    }

    // ---- bitstream ----
    Impl::BsBuf& b = bs[(size_t)bs_cur];
    const VkDeviceSize need =
        align_up64(bitstream.size() + caps.minBitstreamBufferSizeAlignment,
                   caps.minBitstreamBufferSizeAlignment);
    if (need > b.size) {
        vkDeviceWaitIdle(dev);
        vkDestroyBuffer(dev, b.buf, nullptr);
        vkFreeMemory(dev, b.mem, nullptr);
        b.buf = VK_NULL_HANDLE;
        b.mem = VK_NULL_HANDLE;
        b.size = need * 2;

        VkVideoProfileInfoKHR profile{VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR};
        profile.pNext = (void*)codec->profileExt();
        profile.videoCodecOperation = codec->operation();
        profile.chromaSubsampling = fmt.chroma_format == 2
                                        ? VK_VIDEO_CHROMA_SUBSAMPLING_422_BIT_KHR
                                        : VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
        profile.lumaBitDepth = fmt.bit_depth == 10 ? VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR
                                                   : VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
        profile.chromaBitDepth = profile.lumaBitDepth;
        VkVideoProfileListInfoKHR plist{VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR};
        plist.profileCount = 1;
        plist.pProfiles = &profile;
        VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bci.pNext = &plist;
        bci.size = b.size;
        bci.usage = VK_BUFFER_USAGE_VIDEO_DECODE_SRC_BIT_KHR;
        if (vkCreateBuffer(dev, &bci, nullptr, &b.buf) != VK_SUCCESS) {
            error = "cannot grow the video bitstream buffer";
            return false;
        }
        VkMemoryRequirements mr{};
        vkGetBufferMemoryRequirements(dev, b.buf, &mr);
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = mr.size;
        ai.memoryTypeIndex = find_memory_type(mr.memoryTypeBits,
                                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                              /*required=*/true);
        if (vkAllocateMemory(dev, &ai, nullptr, &b.mem) != VK_SUCCESS) {
            error = "out of host-visible memory for the video bitstream buffer";
            return false;
        }
        vkBindBufferMemory(dev, b.buf, b.mem, 0);
        vkMapMemory(dev, b.mem, 0, VK_WHOLE_SIZE, 0, &b.map);
    }
    std::memcpy(b.map, bitstream.data(), bitstream.size());
    // Drivers read in whole alignment units; zero the tail so the padding is
    // deterministic rather than whatever the last frame left there.
    std::memset((uint8_t*)b.map + bitstream.size(), 0,
                (size_t)(need - bitstream.size()));

    // ---- record ----
    VkCommandBuffer cb = cbs[cb_cur];
    vkResetCommandBuffer(cb, 0);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &bi);

    const VkImageLayout dst_layout = coincide ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                                              : VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR;
    std::vector<VkImageMemoryBarrier> barriers;
    {
        VkImageMemoryBarrier ib{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        ib.srcAccessMask = 0;
        ib.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
        ib.oldLayout = dst.layout;
        ib.newLayout = dst_layout;
        ib.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        ib.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        ib.image = dst.image;
        ib.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barriers.push_back(ib);
        dst.layout = dst_layout;
    }
    if (!dpb_ready) {
        VkImageMemoryBarrier ib{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        ib.srcAccessMask = 0;
        ib.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        ib.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ib.newLayout = VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR;
        ib.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        ib.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        ib.image = dpb_image;
        ib.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, fmt.max_dpb_slots};
        barriers.push_back(ib);
        dpb_ready = true;
    }
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr,
                         (uint32_t)barriers.size(), barriers.data());

    auto picture_resource = [&](VkImageView view) {
        VkVideoPictureResourceInfoKHR pr{VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR};
        pr.codedOffset = {0, 0};
        pr.codedExtent = coded;
        pr.baseArrayLayer = 0;
        pr.imageViewBinding = view;
        return pr;
    };

    // Resources to bind for the coding scope: every ACTIVE reference slot with
    // its index, plus the reconstructed picture with slotIndex -1. The setup
    // slot is not yet active at this point -- it is the decode operation that
    // activates it -- so binding it by index would be invalid.
    std::vector<VkVideoPictureResourceInfoKHR> begin_res;
    std::vector<VkVideoReferenceSlotInfoKHR>   begin_slots;
    begin_res.reserve(pi.refs.size() + 1);
    begin_slots.reserve(pi.refs.size() + 1);
    for (const auto& r : pi.refs) begin_res.push_back(picture_resource(dpb_views[(size_t)r.slot]));
    if (pi.setup_slot >= 0)
        begin_res.push_back(picture_resource(dpb_views[(size_t)pi.setup_slot]));
    for (size_t i = 0; i < pi.refs.size(); ++i) {
        VkVideoReferenceSlotInfoKHR sl{VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR};
        sl.slotIndex = pi.refs[i].slot;
        sl.pPictureResource = &begin_res[i];
        begin_slots.push_back(sl);
    }
    if (pi.setup_slot >= 0) {
        VkVideoReferenceSlotInfoKHR sl{VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR};
        sl.slotIndex = -1;
        sl.pPictureResource = &begin_res.back();
        begin_slots.push_back(sl);
    }

    VkVideoBeginCodingInfoKHR bci{VK_STRUCTURE_TYPE_VIDEO_BEGIN_CODING_INFO_KHR};
    bci.videoSession = session;
    bci.videoSessionParameters = sparams;
    bci.referenceSlotCount = (uint32_t)begin_slots.size();
    bci.pReferenceSlots = begin_slots.data();
    api.cmdBeginCoding(cb, &bci);

    if (need_reset) {
        VkVideoCodingControlInfoKHR ctl{VK_STRUCTURE_TYPE_VIDEO_CODING_CONTROL_INFO_KHR};
        ctl.flags = VK_VIDEO_CODING_CONTROL_RESET_BIT_KHR;
        api.cmdControlCoding(cb, &ctl);
        need_reset = false;
    }

    // Codec-specific DPB slot info hangs off each reference slot.
    std::vector<VkVideoDecodeH264DpbSlotInfoKHR> h264_slots;
    std::vector<VkVideoDecodeH265DpbSlotInfoKHR> h265_slots;
    std::vector<VkVideoDecodeAV1DpbSlotInfoKHR>  av1_slots;
    std::vector<VkVideoReferenceSlotInfoKHR>     dec_slots;
    const VkVideoCodecOperationFlagBitsKHR op = codec->operation();
    const size_t n_slots = pi.refs.size() + 1;
    h264_slots.resize(n_slots, {VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_DPB_SLOT_INFO_KHR});
    h265_slots.resize(n_slots, {VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_DPB_SLOT_INFO_KHR});
    av1_slots.resize(n_slots, {VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_DPB_SLOT_INFO_KHR});
    auto slot_pnext = [&](size_t i, const void* std_ref) -> const void* {
        switch (op) {
            case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
                h264_slots[i].pStdReferenceInfo =
                    (const StdVideoDecodeH264ReferenceInfo*)std_ref;
                return &h264_slots[i];
            case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
                h265_slots[i].pStdReferenceInfo =
                    (const StdVideoDecodeH265ReferenceInfo*)std_ref;
                return &h265_slots[i];
            default:
                av1_slots[i].pStdReferenceInfo =
                    (const StdVideoDecodeAV1ReferenceInfo*)std_ref;
                return &av1_slots[i];
        }
    };
    dec_slots.reserve(pi.refs.size());
    for (size_t i = 0; i < pi.refs.size(); ++i) {
        VkVideoReferenceSlotInfoKHR sl{VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR};
        sl.pNext = slot_pnext(i, pi.refs[i].std_ref);
        sl.slotIndex = pi.refs[i].slot;
        sl.pPictureResource = &begin_res[i];
        dec_slots.push_back(sl);
    }
    VkVideoReferenceSlotInfoKHR setup{VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR};
    if (pi.setup_slot >= 0) {
        setup.pNext = slot_pnext(pi.refs.size(), pi.setup_std_ref);
        setup.slotIndex = pi.setup_slot;
        setup.pPictureResource = &begin_res.back();
    }

    // In coincident mode the decode target must be the very same resource the
    // setup slot names; the picture is copied out to `dst` after EndCoding.
    VkVideoPictureResourceInfoKHR dst_res =
        coincide ? begin_res.back() : picture_resource(dst.view);
    VkVideoDecodeInfoKHR di{VK_STRUCTURE_TYPE_VIDEO_DECODE_INFO_KHR};
    di.pNext = pi.decode_pnext;
    di.srcBuffer = b.buf;
    di.srcBufferOffset = 0;
    di.srcBufferRange = need;
    di.dstPictureResource = dst_res;
    di.pSetupReferenceSlot = pi.setup_slot >= 0 ? &setup : nullptr;
    di.referenceSlotCount = (uint32_t)dec_slots.size();
    di.pReferenceSlots = dec_slots.data();
    api.cmdDecode(cb, &di);

    VkVideoEndCodingInfoKHR eci{VK_STRUCTURE_TYPE_VIDEO_END_CODING_INFO_KHR};
    api.cmdEndCoding(cb, &eci);

    if (coincide) {
        // Lift the freshly decoded DPB layer out to the pool image, so the
        // picture survives the slot being recycled a few frames from now.
        VkImageMemoryBarrier pre[1]{};
        pre[0] = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        pre[0].srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
        pre[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        pre[0].oldLayout = VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR;
        pre[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        pre[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        pre[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        pre[0].image = dpb_image;
        pre[0].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, (uint32_t)pi.setup_slot, 1};
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, pre);

        // Multi-planar images copy one plane per region.
        VkImageCopy regions[3]{};
        uint32_t n_regions = 0;
        const VkImageAspectFlagBits aspects[3] = {VK_IMAGE_ASPECT_PLANE_0_BIT,
                                                  VK_IMAGE_ASPECT_PLANE_1_BIT,
                                                  VK_IMAGE_ASPECT_PLANE_2_BIT};
        const uint32_t n_planes = planes.planar ? 3u : 2u;
        for (uint32_t pl = 0; pl < n_planes; ++pl) {
            const uint32_t div_x = pl == 0 ? 1u : (uint32_t)planes.sub_x;
            const uint32_t div_y = pl == 0 ? 1u : (uint32_t)planes.sub_y;
            regions[n_regions].srcSubresource = {(VkImageAspectFlags)aspects[pl], 0,
                                                 (uint32_t)pi.setup_slot, 1};
            regions[n_regions].dstSubresource = {(VkImageAspectFlags)aspects[pl], 0, 0, 1};
            regions[n_regions].extent = {coded.width / div_x, coded.height / div_y, 1};
            ++n_regions;
        }
        vkCmdCopyImage(cb, dpb_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst.image,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, n_regions, regions);

        VkImageMemoryBarrier post[1]{};
        post[0] = pre[0];
        post[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        post[0].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        post[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        post[0].newLayout = VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR;
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             post);
    }
    vkEndCommandBuffer(cb);

    const uint64_t signal = ++vsubmitted;
    VkTimelineSemaphoreSubmitInfo tsi{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
    tsi.signalSemaphoreValueCount = 1;
    tsi.pSignalSemaphoreValues = &signal;
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.pNext = &tsi;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &vtimeline;
    const VkResult r = vkQueueSubmit(vqueue, 1, &si, VK_NULL_HANDLE);
    if (r != VK_SUCCESS) {
        error = std::string("video decode submit failed: ") + vk::Context::resultName(r);
        return false;
    }
    cb_value[cb_cur] = signal;
    b.value = signal;
    dst.decode_value = signal;
    cb_cur = (cb_cur + 1) % kCbRing;
    bs_cur = (bs_cur + 1) % kCbRing;
    return true;
}

bool VideoPipeline::Impl::decodeNext(std::string& error) {
    if (!more_in_packet) {
        Packet pkt;
        if (!demux->next(packet, error)) {
            if (!error.empty()) return false;
            eos = true;
            return true;
        }
        (void)pkt;
    }

    PictureInfo pi;
    const uint8_t* data = more_in_packet ? nullptr : packet.data.data();
    const size_t bytes = more_in_packet ? 0 : packet.data.size();
    if (!codec->decodeFrame(data, bytes, track_info.nal_length_size, bitstream, slice_offsets,
                            pi, error))
        return false;
    more_in_packet = pi.more_in_packet;

    Packet& pkt = packet;
    NN_LOG_DEBUG("[dec] pkt %lld poc %lld setup %d refs %zu show %d out %d more %d\n",
                   (long long)packet.index, (long long)pi.poc, pi.setup_slot, pi.refs.size(),
                   pi.show_existing_slot, (int)pi.output, (int)pi.more_in_packet);
    if (pi.show_existing_slot >= 0) {
        const int src = dpb_pin[(size_t)pi.show_existing_slot];
        if (src >= 0) {
            ++pool[(size_t)src].refs;
            pool[(size_t)src].pts = pkt.pts;
            pool[(size_t)src].display_index = pkt.display_index;
            ready.push_back(src);
        }
        codec->commitFrame();
        return true;
    }

    const int idx = acquirePicture();
    if (idx < 0) {
        error = "the decoded picture pool is exhausted; too many frames are held at once";
        return false;
    }
    if (!recordDecode(idx, pi, error)) {
        releasePool(idx);
        return false;
    }
    codec->commitFrame();
    ++decoded;

    Picture& p = pool[(size_t)idx];
    p.poc = pi.poc;
    p.pts = pkt.pts;
    p.decode_index = pkt.index;
    p.display_index = pkt.display_index;

    // Pin the pool image while a DPB slot still refers to it (AV1 replays
    // pictures with show_existing_frame; H.264/H.265 never do).
    if (!pi.live_slots.empty() || pi.setup_slot >= 0) {
        std::vector<char> live(dpb_pin.size(), 0);
        for (int32_t s : pi.live_slots)
            if (s >= 0 && s < (int32_t)dpb_pin.size()) live[(size_t)s] = 1;
        for (size_t s = 0; s < dpb_pin.size(); ++s) {
            if (!live[s] && dpb_pin[s] >= 0) {
                releasePool(dpb_pin[s]);
                dpb_pin[s] = -1;
            }
        }
        if (pi.setup_slot >= 0 && live[(size_t)pi.setup_slot]) {
            if (dpb_pin[(size_t)pi.setup_slot] >= 0)
                releasePool(dpb_pin[(size_t)pi.setup_slot]);
            ++p.refs;
            dpb_pin[(size_t)pi.setup_slot] = idx;
        }
    }

    if (pi.output)
        ready.push_back(idx);
    else
        releasePool(idx);
    return true;
}

// ---------------------------------------------------------------------------
// Frame delivery
// ---------------------------------------------------------------------------

bool VideoPipeline::next(FrameHandle& out, std::string& error) {
    Impl& s = *impl_;
    error.clear();
    while (true) {
        const bool can_pop =
            !s.ready.empty() && (s.eos || s.ready.size() > (size_t)s.fmt.max_reorder);
        if (can_pop) {
            size_t best = 0;
            for (size_t i = 1; i < s.ready.size(); ++i)
                if (s.pool[(size_t)s.ready[i]].poc < s.pool[(size_t)s.ready[best]].poc) best = i;
            const int idx = s.ready[best];
            s.ready.erase(s.ready.begin() + (ptrdiff_t)best);
            out.slot = idx;
            out.index = s.pool[(size_t)idx].display_index;
            out.pts = s.pool[(size_t)idx].pts;
            return true;
        }
        if (s.eos) return false;
        if (!s.decodeNext(error)) return false;
    }
}

bool VideoPipeline::seek(int64_t index, int64_t& landed, std::string& error) {
    Impl& s = *impl_;
    error.clear();
    // Asked before anything is torn down, so a container that cannot seek
    // leaves the pipeline exactly where it was.
    if (!s.demux->seekSync(index, landed, error)) return false;
    // The reorder queue and the DPB hold pictures of the GOP being left, and
    // the codec's reference state describes it; all three go.
    for (int idx : s.ready) s.releasePool(idx);
    s.ready.clear();
    for (int& pin : s.dpb_pin) {
        if (pin >= 0) s.releasePool(pin);
        pin = -1;
    }
    s.codec->flush();
    s.more_in_packet = false;
    s.eos = false;
    return true;
}

void VideoPipeline::release(FrameHandle& h) {
    if (h.slot < 0) return;
    impl_->releasePool(h.slot);
    h.slot = -1;
}

void VideoPipeline::Impl::copyPlanes(int idx) {
    Picture& p = pool[(size_t)idx];
    vk::Stream& stream = vk::Stream::get();
    stream.waitOn(vtimeline, p.decode_value);
    VkCommandBuffer cb = stream.commandBuffer();

    if (p.layout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        VkImageMemoryBarrier ib{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        ib.srcAccessMask = 0;
        ib.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        ib.oldLayout = p.layout;
        ib.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        ib.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        ib.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        ib.image = p.image;
        ib.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &ib);
        p.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    }

    auto copy_plane = [&](VkImageAspectFlagBits aspect, vk::DevicePtr dst, uint32_t w,
                          uint32_t h) {
        if (!dst) return;
        vk::Allocation a;
        VkDeviceSize off = 0;
        if (!vk::Allocator::get().resolve(dst, &a, &off)) return;
        VkBufferImageCopy c{};
        c.bufferOffset = off;
        c.bufferRowLength = w;
        c.bufferImageHeight = h;
        c.imageSubresource = {(VkImageAspectFlags)aspect, 0, 0, 1};
        c.imageOffset = {0, 0, 0};
        c.imageExtent = {w, h, 1};
        vkCmdCopyImageToBuffer(cb, p.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, a.buffer, 1,
                               &c);
    };
    const uint32_t lw = coded.width, lh = coded.height;
    copy_plane(VK_IMAGE_ASPECT_PLANE_0_BIT, luma_buf, lw, lh);
    if (chroma_buf) {
        const uint32_t cw = lw / (uint32_t)planes.sub_x;
        const uint32_t ch = lh / (uint32_t)planes.sub_y;
        copy_plane(VK_IMAGE_ASPECT_PLANE_1_BIT, chroma_buf, cw, ch);
        if (chroma2_buf) copy_plane(VK_IMAGE_ASPECT_PLANE_2_BIT, chroma2_buf, cw, ch);
    }
    stream.barrierNow();
}

void VideoPipeline::queueSharpness(const FrameHandle& h) {
    Impl& s = *impl_;
    if (h.slot < 0) return;
    Picture& p = s.pool[(size_t)h.slot];
    if (p.metric_queued) return;
    p.metric_queued = true;

    s.copyPlanes(h.slot);

    ThumbParams tp{};
    tp.out = s.thumb_buf;
    tp.luma = s.luma_buf;
    tp.src_w = (uint32_t)s.fmt.width;
    tp.src_h = (uint32_t)s.fmt.height;
    tp.luma_stride = s.coded.width;
    tp.size = kThumbSize;
    tp.flags = s.planeFlags();
    tp.shift = (uint32_t)s.planes.shift;
    vk::Stream::get().dispatchFlat("video.luma_thumbnail", {}, (int64_t)kThumbSize * kThumbSize,
                                   256, &tp, sizeof(tp), &tp.groups_per_row);

    VarianceParams vp{};
    vp.out = s.metric_buf;
    vp.thumb = s.thumb_buf;
    vp.size = kThumbSize;
    vp.slot = (uint32_t)h.slot;
    vk::Stream::get().dispatch("video.laplacian_variance", {}, 1, 1, 1, &vp, sizeof(vp));

    p.read_value = vk::Stream::get().lastSubmitted() + 1;
    s.metric_pending.push_back(h.slot);
}

void VideoPipeline::flushSharpness() {
    Impl& s = *impl_;
    if (s.metric_pending.empty()) return;
    vk::Stream::get().download(s.metrics.data(), s.metric_buf,
                               (VkDeviceSize)s.metrics.size() * 4);
    for (int slot : s.metric_pending) s.pool[(size_t)slot].read_value = 0;
    s.metric_pending.clear();
}

float VideoPipeline::sharpness(const FrameHandle& h) const {
    if (h.slot < 0 || (size_t)h.slot >= impl_->metrics.size()) return 0.0f;
    return impl_->metrics[(size_t)h.slot];
}

void VideoPipeline::outputSize(const ConvertOpts& opts, int& w, int& h) const {
    const StreamFormat& f = impl_->fmt;
    int sw = std::max(1, (int)(f.width * opts.scale + 0.5f));
    int sh = std::max(1, (int)(f.height * opts.scale + 0.5f));
    const int rot = ((opts.rotate % 360) + 360) % 360;
    if (rot == 90 || rot == 270) std::swap(sw, sh);
    w = sw;
    h = sh;
}

bool VideoPipeline::Impl::ensureRgb(VkDeviceSize bytes) {
    if (rgb_buf && rgb_capacity >= bytes) return true;
    if (rgb_buf) vk::device_free(rgb_buf);
    rgb_capacity = bytes;
    rgb_buf = vk::device_alloc(bytes, "video.rgb");
    return rgb_buf != 0;
}

bool VideoPipeline::toImage(const FrameHandle& h, const ConvertOpts& opts, nn::Image& out,
                            std::string& error) {
    Impl& s = *impl_;
    if (h.slot < 0) {
        error = "toImage: invalid frame handle";
        return false;
    }
    int ow = 0, oh = 0;
    outputSize(opts, ow, oh);
    const VkDeviceSize quads = ((VkDeviceSize)ow * oh + 3) / 4;
    if (!s.ensureRgb(quads * 12)) {
        error = "out of device memory for the RGB frame buffer";
        return false;
    }

    s.copyPlanes(h.slot);

    float m[3][4];
    color_matrix(s.fmt.matrix_coefficients, s.fmt.full_range, s.fmt.bit_depth, s.fmt.height, m);

    const int rot = ((opts.rotate % 360) + 360) % 360;
    const int sw = (rot == 90 || rot == 270) ? oh : ow;
    const int sh = (rot == 90 || rot == 270) ? ow : oh;

    YuvParams p{};
    for (int i = 0; i < 4; ++i) {
        p.m0[i] = m[0][i];
        p.m1[i] = m[1][i];
        p.m2[i] = m[2][i];
    }
    p.out = s.rgb_buf;
    p.luma = s.luma_buf;
    p.cb = vk::or_fallback(s.chroma_buf);
    p.cr = vk::or_fallback(s.chroma2_buf ? s.chroma2_buf : s.chroma_buf);
    p.out_w = (uint32_t)ow;
    p.out_h = (uint32_t)oh;
    p.src_w = (uint32_t)s.fmt.width;
    p.src_h = (uint32_t)s.fmt.height;
    p.luma_stride = s.coded.width;
    p.chroma_stride = s.planes.sub_x ? s.coded.width / (uint32_t)s.planes.sub_x : 0;
    p.flags = s.planeFlags() | ((uint32_t)(rot / 90) << 4);
    p.shift = (uint32_t)s.planes.shift;
    p.inv_sx = (float)s.fmt.width / (float)sw;
    p.inv_sy = (float)s.fmt.height / (float)sh;
    vk::Stream::get().dispatchFlat("video.yuv_to_rgb", {}, (int64_t)quads, 256, &p, sizeof(p),
                                   &p.groups_per_row);

    out.width = ow;
    out.height = oh;
    out.channels = 3;
    out.data.resize((size_t)ow * oh * 3);
    // The kernel writes whole words, so the last quad may run past the image;
    // download the exact byte count and let the padding stay on device.
    std::vector<uint8_t> tmp((size_t)quads * 12);
    vk::Stream::get().download(tmp.data(), s.rgb_buf, (VkDeviceSize)tmp.size());
    std::memcpy(out.data.data(), tmp.data(), out.data.size());
    s.pool[(size_t)h.slot].read_value = 0;
    return true;
}

const std::vector<TrackInfo>& VideoPipeline::tracks() const { return impl_->demux->tracks(); }
const TrackInfo& VideoPipeline::track() const { return impl_->track_info; }
const StreamFormat& VideoPipeline::format() const { return impl_->fmt; }
int64_t VideoPipeline::decodedCount() const { return impl_->decoded; }

}  // namespace video

// VideoEncoder.cpp -- see VideoEncoder.h.

#include "video/VideoEncoder.h"

#include "nn/vk/Context.h"
#include "nn/vk/EmbeddedSpirv.h"
#include "nn/vk/Memory.h"
#include "nn/vk/Stream.h"
#include "video/Common.h"
#include "video/H265Crop.h"
#include "video/Mp4Writer.h"
#include "video/VideoApi.h"

#include <algorithm>
#include <cmath>
#include <cstring>

NN_DECLARE_EMBEDDED_MODULES(video)

namespace video {

namespace {

uint32_t find_memory(uint32_t bits, VkMemoryPropertyFlags want) {
    const VkPhysicalDeviceMemoryProperties& mp = vk::Context::get().memoryProps();
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
        if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want) return i;
    return UINT32_MAX;
}

uint32_t align_up(uint32_t v, uint32_t a) { return a > 1 ? (v + a - 1) / a * a : v; }

struct Nv12Params {
    vk::DevicePtr rgb, luma, chroma;
    uint32_t width, height, coded_w, coded_h, groups_per_row;
};

}  // namespace


struct VideoEncoder::Impl {
    EncodeOptions o;
    VkDevice dev = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    uint32_t family = 0, cfamily = 0;
    VkExtent2D coded{0, 0};

    bool h265() const { return o.codec == Codec::H265; }
    bool av1() const { return o.codec == Codec::Av1; }
    const char* name() const { return av1() ? "AV1" : h265() ? "H.265" : "H.264"; }
    // The size of the frames, which AV1 codes exactly and H.26x crops to.
    VkExtent2D picture{0, 0};

    VkVideoEncodeH264ProfileInfoKHR h264_profile{VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_PROFILE_INFO_KHR};
    VkVideoEncodeH265ProfileInfoKHR h265_profile{VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_PROFILE_INFO_KHR};
    VkVideoEncodeAV1ProfileInfoKHR av1_profile{VK_STRUCTURE_TYPE_VIDEO_ENCODE_AV1_PROFILE_INFO_KHR};
    VkVideoEncodeUsageInfoKHR usage{VK_STRUCTURE_TYPE_VIDEO_ENCODE_USAGE_INFO_KHR};
    VkVideoProfileInfoKHR profile{VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR};
    VkVideoProfileListInfoKHR plist{VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR};

    VkVideoCapabilitiesKHR caps{VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR};
    VkVideoEncodeCapabilitiesKHR ecaps{VK_STRUCTURE_TYPE_VIDEO_ENCODE_CAPABILITIES_KHR};
    VkVideoEncodeH264CapabilitiesKHR h264caps{VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_CAPABILITIES_KHR};
    VkVideoEncodeH265CapabilitiesKHR h265caps{VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_CAPABILITIES_KHR};
    VkVideoEncodeAV1CapabilitiesKHR av1caps{VK_STRUCTURE_TYPE_VIDEO_ENCODE_AV1_CAPABILITIES_KHR};

    VkVideoSessionKHR session = VK_NULL_HANDLE;
    std::vector<VkDeviceMemory> session_mem;
    VkVideoSessionParametersKHR params = VK_NULL_HANDLE;
    VkFormat src_format = VK_FORMAT_UNDEFINED, dpb_format = VK_FORMAT_UNDEFINED;

    // The DPB: an image per slot where the device allows it, as NVIDIA's own
    // samples do, else one image with a layer per slot.
    VkImage src = VK_NULL_HANDLE, dpb[2] = {};
    VkDeviceMemory src_mem = VK_NULL_HANDLE, dpb_mem[2] = {};
    VkImageView src_view = VK_NULL_HANDLE, dpb_views[2] = {};
    const int nslots = 2;
    VkImageLayout src_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    bool dpb_ready = false;
    bool separate_dpb = false;

    VkBuffer bs = VK_NULL_HANDLE;
    VkDeviceMemory bs_mem = VK_NULL_HANDLE;
    void* bs_map = nullptr;
    VkDeviceSize bs_size = 0;
    VkQueryPool query = VK_NULL_HANDLE;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer cb = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;

    vk::DevicePtr rgb_buf = 0, luma_buf = 0, chroma_buf = 0;

    bool cqp = true;
    int qp_i = 22, qp_p = 24;
    bool rc_set = false;
    VkVideoEncodeRateControlInfoKHR rc{VK_STRUCTURE_TYPE_VIDEO_ENCODE_RATE_CONTROL_INFO_KHR};
    VkVideoEncodeRateControlLayerInfoKHR rc_layer{VK_STRUCTURE_TYPE_VIDEO_ENCODE_RATE_CONTROL_LAYER_INFO_KHR};
    VkVideoEncodeH264RateControlInfoKHR h264_rc{VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_RATE_CONTROL_INFO_KHR};
    VkVideoEncodeH265RateControlInfoKHR h265_rc{VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_RATE_CONTROL_INFO_KHR};
    VkVideoEncodeAV1RateControlInfoKHR av1_rc{VK_STRUCTURE_TYPE_VIDEO_ENCODE_AV1_RATE_CONTROL_INFO_KHR};

    int gop = 60;
    int64_t frame = 0;
    int in_gop = 0;
    uint16_t idr_id = 0;
    // What each DPB slot holds: its frame_num and picture order count.
    uint32_t slot_frame_num[2] = {};
    int32_t slot_poc[2] = {};
    bool slot_idr[2] = {};
    // AV1: the order hint in each of the decoder's eight reference buffers,
    // and which reference name the hardware predicts from.
    uint8_t vbi_hint[8] = {};
    int av1_name = 0;
    StdVideoAV1Level av1_level = STD_VIDEO_AV1_LEVEL_4_0;

    std::vector<uint8_t> headers;

    ~Impl();
    bool create(std::string& error);
    bool create_parameters(std::string& error);
    bool fetch_headers(std::string& error);
    bool upload(const uint8_t* rgb, std::string& error);
    bool record_and_submit(bool idr, std::vector<uint8_t>& out, std::string& error);
};

VideoEncoder::Impl::~Impl() {
    if (!dev) return;
    vkDeviceWaitIdle(dev);
    const VideoApi& api = video_api();
    if (fence) vkDestroyFence(dev, fence, nullptr);
    if (pool) vkDestroyCommandPool(dev, pool, nullptr);
    if (query) vkDestroyQueryPool(dev, query, nullptr);
    if (bs) vkDestroyBuffer(dev, bs, nullptr);
    if (bs_mem) vkFreeMemory(dev, bs_mem, nullptr);
    for (VkImageView v : dpb_views) if (v) vkDestroyImageView(dev, v, nullptr);
    if (src_view) vkDestroyImageView(dev, src_view, nullptr);
    if (src) vkDestroyImage(dev, src, nullptr);
    for (VkImage i : dpb) if (i) vkDestroyImage(dev, i, nullptr);
    if (src_mem) vkFreeMemory(dev, src_mem, nullptr);
    for (VkDeviceMemory m : dpb_mem) if (m) vkFreeMemory(dev, m, nullptr);
    if (params && api.destroyParameters) api.destroyParameters(dev, params, nullptr);
    if (session && api.destroySession) api.destroySession(dev, session, nullptr);
    for (VkDeviceMemory m : session_mem) vkFreeMemory(dev, m, nullptr);
    for (vk::DevicePtr p : {rgb_buf, luma_buf, chroma_buf})
        if (p) vk::device_free(p);
}

bool VideoEncoder::Impl::create(std::string& error) {
    vk::Context& ctx = vk::Context::get();
    const VideoApi& api = video_api();
    if (!ctx.hasVideoEncode()) {
        error = ctx.encodeUnavailableReason();
        return false;
    }
    if (!api.encode_complete()) {
        error = "the driver is missing VK_KHR_video_encode_queue entry points";
        return false;
    }
    const VkVideoCodecOperationFlagBitsKHR op =
        av1() ? VK_VIDEO_CODEC_OPERATION_ENCODE_AV1_BIT_KHR
        : h265() ? VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR
                 : VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR;
    if (!(ctx.encodeCodecs() & op)) {
        error = std::string(name()) + " encode is not supported on " + ctx.info().name;
        return false;
    }
    dev = ctx.device();
    queue = ctx.encodeQueue();
    family = ctx.encodeQueueFamily();
    cfamily = ctx.queueFamily();

    h264_profile.stdProfileIdc = STD_VIDEO_H264_PROFILE_IDC_HIGH;
    h265_profile.stdProfileIdc = STD_VIDEO_H265_PROFILE_IDC_MAIN;
    av1_profile.stdProfile = STD_VIDEO_AV1_PROFILE_MAIN;
    usage.videoUsageHints = VK_VIDEO_ENCODE_USAGE_RECORDING_BIT_KHR;
    usage.videoContentHints = VK_VIDEO_ENCODE_CONTENT_RENDERED_BIT_KHR;
    usage.tuningMode = VK_VIDEO_ENCODE_TUNING_MODE_HIGH_QUALITY_KHR;
    usage.pNext = av1() ? (const void*)&av1_profile
                  : h265() ? (const void*)&h265_profile : (const void*)&h264_profile;
    profile.pNext = &usage;
    profile.videoCodecOperation = op;
    profile.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
    profile.lumaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
    profile.chromaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
    plist.profileCount = 1;
    plist.pProfiles = &profile;

    ecaps.pNext = av1() ? (void*)&av1caps : h265() ? (void*)&h265caps : (void*)&h264caps;
    caps.pNext = &ecaps;
    VkResult r = api.getCapabilities(ctx.physical(), &profile, &caps);
    if (r != VK_SUCCESS) {
        error = std::string("no ") + (av1() ? "AV1 Main" : h265() ? "H.265 Main" : "H.264 High") +
                " encode profile: " + vk::Context::resultName(r);
        return false;
    }

    // Macroblocks for H.264, the smallest coding block for H.265, AV1's 8x8
    // mode-info units.
    const uint32_t block = av1() ? 8u : 16u;
    uint32_t gw = std::max({block, caps.pictureAccessGranularity.width,
                            ecaps.encodeInputPictureGranularity.width});
    uint32_t gh = std::max({block, caps.pictureAccessGranularity.height,
                            ecaps.encodeInputPictureGranularity.height});
    coded.width = std::max(align_up((uint32_t)o.width, gw), caps.minCodedExtent.width);
    coded.height = std::max(align_up((uint32_t)o.height, gh), caps.minCodedExtent.height);
    // The kernel writes 4x2 blocks.
    coded.width = align_up(coded.width, 4);
    // AV1 has no cropping: its frames are the size asked for (even, for
    // 4:2:0), inside pictures padded to what the hardware reads.
    picture = av1() ? VkExtent2D{align_up((uint32_t)o.width, 2), align_up((uint32_t)o.height, 2)}
                  : coded;
    if (coded.width > caps.maxCodedExtent.width || coded.height > caps.maxCodedExtent.height) {
        error = std::to_string(o.width) + "x" + std::to_string(o.height) + " is larger than " +
                std::string(name()) + " encode allows on this device (" +
                std::to_string(caps.maxCodedExtent.width) + "x" +
                std::to_string(caps.maxCodedExtent.height) + ")";
        return false;
    }

    auto pick = [&](VkImageUsageFlags use, VkFormat& out) {
        VkPhysicalDeviceVideoFormatInfoKHR fi{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_FORMAT_INFO_KHR};
        fi.pNext = &plist;
        fi.imageUsage = use;
        uint32_t n = 0;
        if (api.getFormatProperties(ctx.physical(), &fi, &n, nullptr) != VK_SUCCESS || !n)
            return false;
        std::vector<VkVideoFormatPropertiesKHR> props(n, {VK_STRUCTURE_TYPE_VIDEO_FORMAT_PROPERTIES_KHR});
        if (api.getFormatProperties(ctx.physical(), &fi, &n, props.data()) != VK_SUCCESS)
            return false;
        for (const auto& p : props)
            if (p.format == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM) { out = p.format; return true; }
        return false;
    };
    if (!pick(VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR | VK_IMAGE_USAGE_TRANSFER_DST_BIT, src_format) ||
        !pick(VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR, dpb_format)) {
        error = "the encoder does not take 8-bit NV12 pictures";
        return false;
    }

    // ---- session ----
    VkVideoSessionCreateInfoKHR sci{VK_STRUCTURE_TYPE_VIDEO_SESSION_CREATE_INFO_KHR};
    sci.queueFamilyIndex = family;
    sci.pVideoProfile = &profile;
    sci.pictureFormat = src_format;
    sci.maxCodedExtent = coded;
    sci.referencePictureFormat = dpb_format;
    sci.maxDpbSlots = std::min((uint32_t)nslots, caps.maxDpbSlots);
    sci.maxActiveReferencePictures = std::min(1u, caps.maxActiveReferencePictures);
    sci.pStdHeaderVersion = &caps.stdHeaderVersion;
    if (sci.maxDpbSlots < 2 || sci.maxActiveReferencePictures < 1) {
        error = "the encoder cannot hold a reference picture";
        return false;
    }
    r = api.createSession(dev, &sci, nullptr, &session);
    if (r != VK_SUCCESS) {
        error = std::string("vkCreateVideoSessionKHR: ") + vk::Context::resultName(r);
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
        ai.memoryTypeIndex = find_memory(reqs[i].memoryRequirements.memoryTypeBits,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (ai.memoryTypeIndex == UINT32_MAX)
            ai.memoryTypeIndex = find_memory(reqs[i].memoryRequirements.memoryTypeBits, 0);
        VkDeviceMemory mem = VK_NULL_HANDLE;
        if (vkAllocateMemory(dev, &ai, nullptr, &mem) != VK_SUCCESS) {
            error = "out of memory for the video session";
            return false;
        }
        session_mem.push_back(mem);
        binds[i] = {VK_STRUCTURE_TYPE_BIND_VIDEO_SESSION_MEMORY_INFO_KHR};
        binds[i].memoryBindIndex = reqs[i].memoryBindIndex;
        binds[i].memory = mem;
        binds[i].memorySize = ai.allocationSize;
    }
    if (n_req && api.bindSessionMemory(dev, session, n_req, binds.data()) != VK_SUCCESS) {
        error = "vkBindVideoSessionMemoryKHR failed";
        return false;
    }

    // ---- rate control: constant QP where offered ----
    // AV1's quantizer index runs 0..255, about four to an H.265 QP step.
    static const int kQp264[3] = {20, 24, 29}, kQp265[3] = {22, 26, 31}, kQAv1[3] = {70, 100, 140};
    const int q = std::clamp(o.quality, 0, 2);
    qp_i = av1() ? kQAv1[q] : h265() ? kQp265[q] : kQp264[q];
    const int min_qp = av1() ? (int)av1caps.minQIndex : h265() ? h265caps.minQp : h264caps.minQp;
    const int max_qp = av1() ? (int)av1caps.maxQIndex : h265() ? h265caps.maxQp : h264caps.maxQp;
    qp_i = std::clamp(qp_i, min_qp, max_qp);
    qp_p = std::clamp(qp_i + (av1() ? 8 : 2), min_qp, max_qp);
    // The reference name a single-reference frame predicts from: LAST where
    // the hardware allows it.
    av1_name = 0;
    for (int i = 0; i < 7; i++)
        if (av1caps.singleReferenceNameMask & (1u << i)) { av1_name = i; break; }
    gop = std::max(1, (int)std::lround(o.fps * 2.0));
    cqp = (ecaps.rateControlModes & VK_VIDEO_ENCODE_RATE_CONTROL_MODE_DISABLED_BIT_KHR) != 0;
    if (cqp) {
        rc.rateControlMode = VK_VIDEO_ENCODE_RATE_CONTROL_MODE_DISABLED_BIT_KHR;
    } else {
        // Bits per pixel a rendered scene looks clean at, per quality step.
        static const double kBpp[3] = {0.25, 0.12, 0.06};
        const double bitrate = kBpp[q] * o.width * o.height * o.fps;
        rc_layer.averageBitrate = (uint64_t)bitrate;
        rc_layer.maxBitrate = std::min<uint64_t>((uint64_t)(bitrate * 2.0),
                                                 ecaps.maxBitrate ? ecaps.maxBitrate : UINT64_MAX);
        rc_layer.averageBitrate = std::min(rc_layer.averageBitrate, rc_layer.maxBitrate);
        rc_layer.frameRateNumerator = (uint32_t)std::lround(o.fps * 1000.0);
        rc_layer.frameRateDenominator = 1000;
        rc.rateControlMode = VK_VIDEO_ENCODE_RATE_CONTROL_MODE_VBR_BIT_KHR;
        rc.layerCount = 1;
        rc.pLayers = &rc_layer;
        rc.virtualBufferSizeInMs = 1000;
        rc.initialVirtualBufferSizeInMs = 500;
        h264_rc.flags = VK_VIDEO_ENCODE_H264_RATE_CONTROL_REGULAR_GOP_BIT_KHR |
                        VK_VIDEO_ENCODE_H264_RATE_CONTROL_REFERENCE_PATTERN_FLAT_BIT_KHR;
        h264_rc.gopFrameCount = (uint32_t)gop;
        h264_rc.idrPeriod = (uint32_t)gop;
        h264_rc.temporalLayerCount = 1;
        h265_rc.flags = VK_VIDEO_ENCODE_H265_RATE_CONTROL_REGULAR_GOP_BIT_KHR |
                        VK_VIDEO_ENCODE_H265_RATE_CONTROL_REFERENCE_PATTERN_FLAT_BIT_KHR;
        h265_rc.gopFrameCount = (uint32_t)gop;
        h265_rc.idrPeriod = (uint32_t)gop;
        h265_rc.subLayerCount = 1;
        av1_rc.flags = VK_VIDEO_ENCODE_AV1_RATE_CONTROL_REGULAR_GOP_BIT_KHR |
                       VK_VIDEO_ENCODE_AV1_RATE_CONTROL_REFERENCE_PATTERN_FLAT_BIT_KHR;
        av1_rc.gopFrameCount = (uint32_t)gop;
        av1_rc.keyFramePeriod = (uint32_t)gop;
        av1_rc.temporalLayerCount = 1;
        rc.pNext = av1() ? (const void*)&av1_rc
                   : h265() ? (const void*)&h265_rc : (const void*)&h264_rc;
    }

    if (!create_parameters(error)) return false;
    if (!fetch_headers(error)) return false;

    // ---- pictures ----
    separate_dpb = (caps.flags & VK_VIDEO_CAPABILITY_SEPARATE_REFERENCE_IMAGES_BIT_KHR) != 0;
    const uint32_t families[2] = {family, cfamily};
    auto make_image = [&](VkFormat format, uint32_t layers, VkImageUsageFlags use, bool shared,
                          VkImage& image, VkDeviceMemory& mem) {
        VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ici.pNext = &plist;
        ici.imageType = VK_IMAGE_TYPE_2D;
        ici.format = format;
        ici.extent = {coded.width, coded.height, 1};
        ici.mipLevels = 1;
        ici.arrayLayers = layers;
        ici.samples = VK_SAMPLE_COUNT_1_BIT;
        ici.tiling = VK_IMAGE_TILING_OPTIMAL;
        ici.usage = use;
        if (shared && family != cfamily) {
            ici.sharingMode = VK_SHARING_MODE_CONCURRENT;
            ici.queueFamilyIndexCount = 2;
            ici.pQueueFamilyIndices = families;
        }
        if (vkCreateImage(dev, &ici, nullptr, &image) != VK_SUCCESS) return false;
        VkMemoryRequirements mr{};
        vkGetImageMemoryRequirements(dev, image, &mr);
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = mr.size;
        ai.memoryTypeIndex = find_memory(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (ai.memoryTypeIndex == UINT32_MAX ||
            vkAllocateMemory(dev, &ai, nullptr, &mem) != VK_SUCCESS)
            return false;
        return vkBindImageMemory(dev, image, mem, 0) == VK_SUCCESS;
    };
    auto make_view = [&](VkImage image, VkFormat format, uint32_t layer, VkImageView& view) {
        VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vci.image = image;
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format = format;
        vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, layer, 1};
        return vkCreateImageView(dev, &vci, nullptr, &view) == VK_SUCCESS;
    };
    if (!make_image(src_format, 1,
                    VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                    true, src, src_mem) ||
        !make_view(src, src_format, 0, src_view) ||
        ![&] {
            if (separate_dpb) {
                for (int k = 0; k < nslots; k++)
                    if (!make_image(dpb_format, 1, VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR, false,
                                    dpb[k], dpb_mem[k]) ||
                        !make_view(dpb[k], dpb_format, 0, dpb_views[k]))
                        return false;
                return true;
            }
            if (!make_image(dpb_format, (uint32_t)nslots, VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR,
                            false, dpb[0], dpb_mem[0]))
                return false;
            for (int k = 0; k < nslots; k++)
                if (!make_view(dpb[0], dpb_format, (uint32_t)k, dpb_views[k])) return false;
            return true;
        }()) {
        error = "cannot allocate the encoder's pictures";
        return false;
    }

    // ---- bitstream buffer, feedback query, commands ----
    bs_size = (VkDeviceSize)coded.width * coded.height * 3 / 2 + (1 << 20);
    const VkDeviceSize a = std::max<VkDeviceSize>(caps.minBitstreamBufferSizeAlignment, 1);
    bs_size = (bs_size + a - 1) / a * a;
    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.pNext = &plist;
    bci.size = bs_size;
    bci.usage = VK_BUFFER_USAGE_VIDEO_ENCODE_DST_BIT_KHR;
    if (vkCreateBuffer(dev, &bci, nullptr, &bs) != VK_SUCCESS) {
        error = "cannot create the bitstream buffer";
        return false;
    }
    VkMemoryRequirements mr{};
    vkGetBufferMemoryRequirements(dev, bs, &mr);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = find_memory(mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                                            VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
    if (ai.memoryTypeIndex == UINT32_MAX)
        ai.memoryTypeIndex = find_memory(mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (ai.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(dev, &ai, nullptr, &bs_mem) != VK_SUCCESS ||
        vkBindBufferMemory(dev, bs, bs_mem, 0) != VK_SUCCESS ||
        vkMapMemory(dev, bs_mem, 0, VK_WHOLE_SIZE, 0, &bs_map) != VK_SUCCESS) {
        error = "no host-visible memory for the bitstream buffer";
        return false;
    }

    VkQueryPoolVideoEncodeFeedbackCreateInfoKHR fb{
        VK_STRUCTURE_TYPE_QUERY_POOL_VIDEO_ENCODE_FEEDBACK_CREATE_INFO_KHR};
    fb.pNext = &profile;
    fb.encodeFeedbackFlags = VK_VIDEO_ENCODE_FEEDBACK_BITSTREAM_BUFFER_OFFSET_BIT_KHR |
                             VK_VIDEO_ENCODE_FEEDBACK_BITSTREAM_BYTES_WRITTEN_BIT_KHR;
    VkQueryPoolCreateInfo qci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    qci.pNext = &fb;
    qci.queryType = VK_QUERY_TYPE_VIDEO_ENCODE_FEEDBACK_KHR;
    qci.queryCount = 1;
    if (vkCreateQueryPool(dev, &qci, nullptr, &query) != VK_SUCCESS) {
        error = "cannot create the encode feedback query";
        return false;
    }
    VkCommandPoolCreateInfo cpi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpi.queueFamilyIndex = family;
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = 1;
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if (vkCreateCommandPool(dev, &cpi, nullptr, &pool) != VK_SUCCESS ||
        (cai.commandPool = pool, vkAllocateCommandBuffers(dev, &cai, &cb) != VK_SUCCESS) ||
        vkCreateFence(dev, &fci, nullptr, &fence) != VK_SUCCESS) {
        error = "cannot create the encode command buffer";
        return false;
    }

    NN_ENSURE_EMBEDDED_MODULES(video);
    const VkDeviceSize luma = (VkDeviceSize)coded.width * coded.height;
    rgb_buf = vk::device_alloc((VkDeviceSize)o.width * o.height * 3 + 16, "encode.rgb");
    luma_buf = vk::device_alloc(luma, "encode.luma");
    chroma_buf = vk::device_alloc(luma / 2, "encode.chroma");
    return true;
}

// ---------------------------------------------------------------------------
// Parameter sets
// ---------------------------------------------------------------------------

bool VideoEncoder::Impl::create_parameters(std::string& error) {
    const VideoApi& api = video_api();
    VkVideoEncodeQualityLevelInfoKHR ql{VK_STRUCTURE_TYPE_VIDEO_ENCODE_QUALITY_LEVEL_INFO_KHR};
    ql.qualityLevel = 0;
    VkVideoSessionParametersCreateInfoKHR pci{VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_CREATE_INFO_KHR};
    pci.videoSession = session;
    // BT.709 studio range, said once in the stream so no player has to guess.
    const uint8_t kPrimaries = 1, kTransfer = 1, kMatrix = 1;

    if (av1()) {
        StdVideoAV1ColorConfig cc{};
        cc.flags.color_description_present_flag = 1;
        cc.BitDepth = 8;
        cc.subsampling_x = 1;
        cc.subsampling_y = 1;
        cc.color_primaries = STD_VIDEO_AV1_COLOR_PRIMARIES_BT_709;
        cc.transfer_characteristics = STD_VIDEO_AV1_TRANSFER_CHARACTERISTICS_BT_709;
        cc.matrix_coefficients = STD_VIDEO_AV1_MATRIX_COEFFICIENTS_BT_709;
        auto bits_for = [](uint32_t v) { uint32_t b = 1; while ((1u << b) <= v) b++; return b; };
        StdVideoAV1SequenceHeader sh{};
        sh.flags.enable_order_hint = 1;
        sh.flags.use_128x128_superblock =
            (av1caps.superblockSizes & VK_VIDEO_ENCODE_AV1_SUPERBLOCK_SIZE_64_BIT_KHR) ? 0 : 1;
        sh.seq_profile = STD_VIDEO_AV1_PROFILE_MAIN;
        sh.frame_width_bits_minus_1 = (uint8_t)(bits_for(picture.width - 1) - 1);
        sh.frame_height_bits_minus_1 = (uint8_t)(bits_for(picture.height - 1) - 1);
        sh.max_frame_width_minus_1 = (uint16_t)(picture.width - 1);
        sh.max_frame_height_minus_1 = (uint16_t)(picture.height - 1);
        sh.order_hint_bits_minus_1 = 7;
        sh.seq_force_integer_mv = 2;               // SELECT_INTEGER_MV
        sh.seq_force_screen_content_tools = 2;     // SELECT_SCREEN_CONTENT_TOOLS
        sh.pColorConfig = &cc;
        // Level by picture size and sample rate (AV1 spec, Annex A).
        const double px = (double)picture.width * picture.height, rate = px * o.fps;
        av1_level = px <= 2228224.0 ? (rate <= 66846720.0 ? STD_VIDEO_AV1_LEVEL_4_0
                                     : rate <= 133693440.0 ? STD_VIDEO_AV1_LEVEL_4_1
                                                           : STD_VIDEO_AV1_LEVEL_5_1)
                    : px <= 8912896.0 ? (rate <= 267386880.0 ? STD_VIDEO_AV1_LEVEL_5_0
                                         : rate <= 534773760.0 ? STD_VIDEO_AV1_LEVEL_5_1
                                                               : STD_VIDEO_AV1_LEVEL_5_2)
                    : rate <= 1069547520.0 ? STD_VIDEO_AV1_LEVEL_6_0 : STD_VIDEO_AV1_LEVEL_6_1;
        av1_level = std::min(av1_level, av1caps.maxLevel);
        StdVideoEncodeAV1OperatingPointInfo op{};
        op.seq_level_idx = (uint8_t)av1_level;
        VkVideoEncodeAV1SessionParametersCreateInfoKHR a{
            VK_STRUCTURE_TYPE_VIDEO_ENCODE_AV1_SESSION_PARAMETERS_CREATE_INFO_KHR};
        a.pNext = &ql;
        a.pStdSequenceHeader = &sh;
        a.stdOperatingPointCount = 1;
        a.pStdOperatingPoints = &op;
        pci.pNext = &a;
        const VkResult r = api.createParameters(dev, &pci, nullptr, &params);
        if (r != VK_SUCCESS) {
            error = std::string("AV1 sequence header refused: ") + vk::Context::resultName(r);
            return false;
        }
        return true;
    }

    if (!h265()) {
        StdVideoH264SequenceParameterSetVui vui{};
        vui.flags.video_signal_type_present_flag = 1;
        vui.flags.color_description_present_flag = 1;
        vui.video_format = 5;
        vui.colour_primaries = kPrimaries;
        vui.transfer_characteristics = kTransfer;
        vui.matrix_coefficients = kMatrix;
        StdVideoH264SequenceParameterSet sps{};
        sps.flags.direct_8x8_inference_flag = 1;
        sps.flags.frame_mbs_only_flag = 1;
        sps.flags.vui_parameters_present_flag = 1;
        sps.profile_idc = STD_VIDEO_H264_PROFILE_IDC_HIGH;
        const double pixels = (double)o.width * o.height * o.fps;
        sps.level_idc = pixels <= 1920.0 * 1080 * 30 ? STD_VIDEO_H264_LEVEL_IDC_4_1
                        : pixels <= 1920.0 * 1080 * 60 ? STD_VIDEO_H264_LEVEL_IDC_4_2
                        : pixels <= 4096.0 * 2176 * 30 ? STD_VIDEO_H264_LEVEL_IDC_5_1
                                                       : STD_VIDEO_H264_LEVEL_IDC_5_2;
        sps.level_idc = std::min(sps.level_idc, h264caps.maxLevelIdc);
        sps.chroma_format_idc = STD_VIDEO_H264_CHROMA_FORMAT_IDC_420;
        sps.log2_max_frame_num_minus4 = 4;
        sps.pic_order_cnt_type = STD_VIDEO_H264_POC_TYPE_2;
        sps.max_num_ref_frames = 1;
        sps.pic_width_in_mbs_minus1 = coded.width / 16 - 1;
        sps.pic_height_in_map_units_minus1 = coded.height / 16 - 1;
        if ((int)coded.width != o.width || (int)coded.height != o.height) {
            sps.flags.frame_cropping_flag = 1;
            sps.frame_crop_right_offset = (coded.width - (uint32_t)o.width) / 2;
            sps.frame_crop_bottom_offset = (coded.height - (uint32_t)o.height) / 2;
        }
        sps.pSequenceParameterSetVui = &vui;
        StdVideoH264PictureParameterSet pps{};
        pps.flags.entropy_coding_mode_flag = 1;
        pps.flags.transform_8x8_mode_flag =
            (h264caps.stdSyntaxFlags & VK_VIDEO_ENCODE_H264_STD_TRANSFORM_8X8_MODE_FLAG_SET_BIT_KHR) ? 1 : 0;
        pps.flags.deblocking_filter_control_present_flag = 1;
        VkVideoEncodeH264SessionParametersAddInfoKHR add{
            VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_SESSION_PARAMETERS_ADD_INFO_KHR};
        add.stdSPSCount = 1;
        add.pStdSPSs = &sps;
        add.stdPPSCount = 1;
        add.pStdPPSs = &pps;
        VkVideoEncodeH264SessionParametersCreateInfoKHR h{
            VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_SESSION_PARAMETERS_CREATE_INFO_KHR};
        h.pNext = &ql;
        h.maxStdSPSCount = 1;
        h.maxStdPPSCount = 1;
        h.pParametersAddInfo = &add;
        pci.pNext = &h;
        const VkResult r = api.createParameters(dev, &pci, nullptr, &params);
        if (r != VK_SUCCESS) {
            error = std::string("H.264 parameter sets refused: ") + vk::Context::resultName(r);
            return false;
        }
        return true;
    }

    // H.265 Main: 16x16 coding blocks at the smallest -- NVIDIA's hardware
    // never splits below that, and an SPS allowing 8x8 has it leave out split
    // flags a conforming decoder reads (NVDEC played it, ffmpeg did not).
    uint32_t ctb = 16;
    if (h265caps.ctbSizes & VK_VIDEO_ENCODE_H265_CTB_SIZE_32_BIT_KHR) ctb = 32;
    else if (h265caps.ctbSizes & VK_VIDEO_ENCODE_H265_CTB_SIZE_64_BIT_KHR) ctb = 64;
    uint32_t tb_min = 4, tb_max = 32;
    const auto tbs = h265caps.transformBlockSizes;
    if (!(tbs & VK_VIDEO_ENCODE_H265_TRANSFORM_BLOCK_SIZE_4_BIT_KHR)) tb_min = 8;
    if (!(tbs & VK_VIDEO_ENCODE_H265_TRANSFORM_BLOCK_SIZE_32_BIT_KHR)) tb_max = 16;
    tb_max = std::min(tb_max, ctb);
    auto log2u = [](uint32_t v) { uint32_t l = 0; while ((1u << (l + 1)) <= v) l++; return l; };

    StdVideoH265ProfileTierLevel ptl{};
    ptl.flags.general_progressive_source_flag = 1;
    ptl.flags.general_frame_only_constraint_flag = 1;
    ptl.general_profile_idc = STD_VIDEO_H265_PROFILE_IDC_MAIN;
    const double px = (double)o.width * o.height;
    ptl.general_level_idc = px <= 2228224.0 ? STD_VIDEO_H265_LEVEL_IDC_4_1
                            : px <= 8912896.0 ? STD_VIDEO_H265_LEVEL_IDC_5_1
                                              : STD_VIDEO_H265_LEVEL_IDC_6_1;
    ptl.general_level_idc = std::min(ptl.general_level_idc, h265caps.maxLevelIdc);
    StdVideoH265DecPicBufMgr dpbm{};
    dpbm.max_dec_pic_buffering_minus1[0] = 1;
    StdVideoH265VideoParameterSet vps{};
    vps.flags.vps_temporal_id_nesting_flag = 1;
    vps.flags.vps_sub_layer_ordering_info_present_flag = 1;
    vps.pDecPicBufMgr = &dpbm;
    vps.pProfileTierLevel = &ptl;
    StdVideoH265SequenceParameterSetVui vui{};
    vui.flags.video_signal_type_present_flag = 1;
    vui.flags.colour_description_present_flag = 1;
    vui.video_format = 5;
    vui.colour_primaries = kPrimaries;
    vui.transfer_characteristics = kTransfer;
    vui.matrix_coeffs = kMatrix;
    StdVideoH265SequenceParameterSet sps{};
    sps.flags.sps_temporal_id_nesting_flag = 1;
    sps.flags.sps_sub_layer_ordering_info_present_flag = 1;
    sps.flags.sample_adaptive_offset_enabled_flag = 1;
    sps.flags.sps_temporal_mvp_enabled_flag = 1;
    sps.flags.strong_intra_smoothing_enabled_flag = 1;
    sps.flags.vui_parameters_present_flag = 1;
    sps.chroma_format_idc = STD_VIDEO_H265_CHROMA_FORMAT_IDC_420;
    sps.pic_width_in_luma_samples = coded.width;
    sps.pic_height_in_luma_samples = coded.height;
    sps.log2_max_pic_order_cnt_lsb_minus4 = 4;
    ctb = std::max(ctb, 32u);
    sps.log2_min_luma_coding_block_size_minus3 = 1;
    sps.log2_diff_max_min_luma_coding_block_size = (uint8_t)(log2u(ctb) - 4);
    sps.log2_min_luma_transform_block_size_minus2 = (uint8_t)(log2u(tb_min) - 2);
    sps.log2_diff_max_min_luma_transform_block_size = (uint8_t)(log2u(tb_max) - log2u(tb_min));
    sps.max_transform_hierarchy_depth_inter = 3;
    sps.max_transform_hierarchy_depth_intra = 3;
    // AMD can encode the cropped CTB grid but advertise the padded one.
    // Encode the full picture; apply display cropping only to the returned SPS.
    sps.pProfileTierLevel = &ptl;
    sps.pDecPicBufMgr = &dpbm;
    sps.pSequenceParameterSetVui = &vui;
    StdVideoH265PictureParameterSet pps{};
    pps.flags.pps_loop_filter_across_slices_enabled_flag = 1;
    pps.flags.cu_qp_delta_enabled_flag = cqp ? 0 : 1;
    VkVideoEncodeH265SessionParametersAddInfoKHR add{
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_SESSION_PARAMETERS_ADD_INFO_KHR};
    add.stdVPSCount = 1;
    add.pStdVPSs = &vps;
    add.stdSPSCount = 1;
    add.pStdSPSs = &sps;
    add.stdPPSCount = 1;
    add.pStdPPSs = &pps;
    VkVideoEncodeH265SessionParametersCreateInfoKHR h{
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_SESSION_PARAMETERS_CREATE_INFO_KHR};
    h.pNext = &ql;
    h.maxStdVPSCount = 1;
    h.maxStdSPSCount = 1;
    h.maxStdPPSCount = 1;
    h.pParametersAddInfo = &add;
    pci.pNext = &h;
    const VkResult r = api.createParameters(dev, &pci, nullptr, &params);
    if (r != VK_SUCCESS) {
        error = std::string("H.265 parameter sets refused: ") + vk::Context::resultName(r);
        return false;
    }
    return true;
}

// Driver overrides define the coded layout; display cropping must preserve it.
bool VideoEncoder::Impl::fetch_headers(std::string& error) {
    const VideoApi& api = video_api();
    VkVideoEncodeH264SessionParametersGetInfoKHR g264{
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_SESSION_PARAMETERS_GET_INFO_KHR};
    g264.writeStdSPS = VK_TRUE;
    g264.writeStdPPS = VK_TRUE;
    VkVideoEncodeH265SessionParametersGetInfoKHR g265{
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_SESSION_PARAMETERS_GET_INFO_KHR};
    g265.writeStdVPS = VK_TRUE;
    g265.writeStdSPS = VK_TRUE;
    g265.writeStdPPS = VK_TRUE;
    VkVideoEncodeSessionParametersGetInfoKHR gi{
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_SESSION_PARAMETERS_GET_INFO_KHR};
    // AV1 has nothing to choose: the one sequence header comes back.
    gi.pNext = av1() ? nullptr : h265() ? (const void*)&g265 : (const void*)&g264;
    gi.videoSessionParameters = params;
    size_t n = 0;
    if (api.getEncodedParameters(dev, &gi, nullptr, &n, nullptr) != VK_SUCCESS || !n) {
        error = "the driver would not write the parameter sets";
        return false;
    }
    headers.resize(n);
    if (api.getEncodedParameters(dev, &gi, nullptr, &n, headers.data()) != VK_SUCCESS) {
        error = "the driver would not write the parameter sets";
        return false;
    }
    headers.resize(n);
    if (h265()) {
        std::vector<uint8_t> cropped, sps;
        cropped.reserve(headers.size() + 16);
        bool found_sps = false;
        for (const auto& [nal, size] : split_annexb(headers.data(), headers.size())) {
            if (size < 2) {
                error = "the encoder returned a truncated H.265 parameter set";
                return false;
            }
            cropped.insert(cropped.end(), {0, 0, 0, 1});
            if (((nal[0] >> 1) & 0x3f) == 33) {
                if (!crop_h265_sps(nal, size, align_up((uint32_t)o.width, 2),
                                   align_up((uint32_t)o.height, 2), sps, error))
                    return false;
                cropped.insert(cropped.end(), sps.begin(), sps.end());
                found_sps = true;
            } else {
                cropped.insert(cropped.end(), nal, nal + size);
            }
        }
        if (!found_sps) {
            error = "the encoder returned no H.265 SPS";
            return false;
        }
        headers.swap(cropped);
    }
    return true;
}

// ---------------------------------------------------------------------------
// One frame
// ---------------------------------------------------------------------------

// RGB into the device, NV12 out of the kernel, NV12 into the source picture:
// all on the compute queue, which the encode submission then waits for.
bool VideoEncoder::Impl::upload(const uint8_t* rgb, std::string& error) {
    (void)error;
    vk::Stream& stream = vk::Stream::get();
    stream.upload(rgb_buf, rgb, (VkDeviceSize)o.width * o.height * 3);
    Nv12Params p{};
    p.rgb = rgb_buf;
    p.luma = luma_buf;
    p.chroma = chroma_buf;
    p.width = (uint32_t)o.width;
    p.height = (uint32_t)o.height;
    p.coded_w = coded.width;
    p.coded_h = coded.height;
    const int64_t blocks = (int64_t)(coded.width / 4) * (coded.height / 2);
    stream.dispatchFlat("video.rgb_to_nv12", {}, blocks, 256, &p, sizeof(p), &p.groups_per_row);

    VkCommandBuffer c = stream.commandBuffer();
    stream.barrierNow();
    VkImageMemoryBarrier ib{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    ib.srcAccessMask = 0;
    ib.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    ib.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ib.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    ib.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    ib.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    ib.image = src;
    ib.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &ib);
    auto copy_plane = [&](VkImageAspectFlagBits aspect, vk::DevicePtr from, uint32_t w, uint32_t h) {
        vk::Allocation a;
        VkDeviceSize off = 0;
        if (!vk::Allocator::get().resolve(from, &a, &off)) return;
        VkBufferImageCopy r{};
        r.bufferOffset = off;
        r.bufferRowLength = w;
        r.bufferImageHeight = h;
        r.imageSubresource = {(VkImageAspectFlags)aspect, 0, 0, 1};
        r.imageExtent = {w, h, 1};
        vkCmdCopyBufferToImage(c, a.buffer, src, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &r);
    };
    copy_plane(VK_IMAGE_ASPECT_PLANE_0_BIT, luma_buf, coded.width, coded.height);
    copy_plane(VK_IMAGE_ASPECT_PLANE_1_BIT, chroma_buf, coded.width / 2, coded.height / 2);
    ib.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    ib.dstAccessMask = 0;
    ib.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    ib.newLayout = VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR;
    vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &ib);
    stream.flush();
    return true;
}

bool VideoEncoder::Impl::record_and_submit(bool idr, std::vector<uint8_t>& out,
                                           std::string& error) {
    const VideoApi& api = video_api();
    int cur = (int)(frame & 1), ref = 1 - cur;
    // AV1 predicts every P frame from the key frame, kept in slot 0: on
    // NVIDIA 595 a P frame's own reconstruction does not serve as a reference
    // (the next frame drifts, static content too), a key frame's does.
    if (av1()) {
        cur = idr ? 0 : 1;
        ref = 0;
    }
    if (idr) {
        in_gop = 0;
        slot_idr[0] = slot_idr[1] = false;
    }
    const uint32_t frame_num = (uint32_t)in_gop;
    const int32_t poc = 2 * in_gop;

    vkResetCommandBuffer(cb, 0);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &bi);
    vkCmdResetQueryPool(cb, query, 0, 1);
    if (!dpb_ready) {
        VkImageMemoryBarrier ib{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        ib.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ib.newLayout = VK_IMAGE_LAYOUT_VIDEO_ENCODE_DPB_KHR;
        ib.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        ib.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        VkImageMemoryBarrier all[2] = {ib, ib};
        for (int s = 0; s < nslots; s++) {
            all[s].image = separate_dpb ? dpb[s] : dpb[0];
            all[s].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0,
                                       separate_dpb ? 1u : (uint32_t)nslots};
        }
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr,
                             separate_dpb ? (uint32_t)nslots : 1u, all);
        dpb_ready = true;
    }

    VkVideoPictureResourceInfoKHR pic[2];
    for (int s = 0; s < nslots; s++) {
        pic[s] = {VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR};
        pic[s].codedExtent = picture;
        pic[s].imageViewBinding = dpb_views[s];
    }
    VkVideoPictureResourceInfoKHR src_pic{VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR};
    src_pic.codedExtent = picture;
    src_pic.imageViewBinding = src_view;

    // ---- the codec's view of this picture and of its reference ----
    StdVideoEncodeH264ReferenceInfo r264[2]{};
    VkVideoEncodeH264DpbSlotInfoKHR d264[2];
    StdVideoEncodeH265ReferenceInfo r265[2]{};
    VkVideoEncodeH265DpbSlotInfoKHR d265[2];
    StdVideoEncodeAV1ExtensionHeader ext_av1{};
    StdVideoEncodeAV1ReferenceInfo rav1[2]{};
    VkVideoEncodeAV1DpbSlotInfoKHR dav1[2];
    const uint8_t hint = (uint8_t)(in_gop & 0xFF);
    for (int s = 0; s < nslots; s++) {
        const bool is_cur = s == cur;
        rav1[s].RefFrameId = (uint32_t)s;
        rav1[s].frame_type = (is_cur ? idr : slot_idr[s]) ? STD_VIDEO_AV1_FRAME_TYPE_KEY
                                                          : STD_VIDEO_AV1_FRAME_TYPE_INTER;
        rav1[s].OrderHint = is_cur ? hint : (uint8_t)slot_poc[s];
        rav1[s].pExtensionHeader = &ext_av1;
        dav1[s] = {VK_STRUCTURE_TYPE_VIDEO_ENCODE_AV1_DPB_SLOT_INFO_KHR};
        dav1[s].pStdReferenceInfo = &rav1[s];
    }
    for (int s = 0; s < nslots; s++) {
        const bool is_cur = s == cur;
        r264[s].primary_pic_type = (is_cur ? idr : slot_idr[s]) ? STD_VIDEO_H264_PICTURE_TYPE_IDR
                                                                : STD_VIDEO_H264_PICTURE_TYPE_P;
        r264[s].FrameNum = is_cur ? frame_num : slot_frame_num[s];
        r264[s].PicOrderCnt = is_cur ? poc : slot_poc[s];
        d264[s] = {VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_DPB_SLOT_INFO_KHR};
        d264[s].pStdReferenceInfo = &r264[s];
        r265[s].pic_type = (is_cur ? idr : slot_idr[s]) ? STD_VIDEO_H265_PICTURE_TYPE_IDR
                                                        : STD_VIDEO_H265_PICTURE_TYPE_P;
        r265[s].PicOrderCntVal = is_cur ? poc / 2 : slot_poc[s] / 2;
        d265[s] = {VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_DPB_SLOT_INFO_KHR};
        d265[s].pStdReferenceInfo = &r265[s];
    }
    auto dpb_info = [&](int s) {
        return av1() ? (const void*)&dav1[s] : h265() ? (const void*)&d265[s] : (const void*)&d264[s];
    };

    // Bound: the reference, and the picture being reconstructed -- listed
    // with slot -1, since it is the encode that activates its slot.
    VkVideoReferenceSlotInfoKHR bound[2];
    uint32_t n_bound = 0;
    bound[n_bound] = {VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR};
    bound[n_bound].slotIndex = -1;
    bound[n_bound].pPictureResource = &pic[cur];
    n_bound++;
    if (!idr) {
        bound[n_bound] = {VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR};
        bound[n_bound].pNext = dpb_info(ref);
        bound[n_bound].slotIndex = ref;
        bound[n_bound].pPictureResource = &pic[ref];
        n_bound++;
    }
    VkVideoBeginCodingInfoKHR begin{VK_STRUCTURE_TYPE_VIDEO_BEGIN_CODING_INFO_KHR};
    begin.pNext = rc_set ? &rc : nullptr;
    begin.videoSession = session;
    begin.videoSessionParameters = params;
    begin.referenceSlotCount = n_bound;
    begin.pReferenceSlots = bound;
    api.cmdBeginCoding(cb, &begin);

    if (!rc_set) {
        VkVideoEncodeQualityLevelInfoKHR ql{VK_STRUCTURE_TYPE_VIDEO_ENCODE_QUALITY_LEVEL_INFO_KHR};
        ql.pNext = &rc;
        VkVideoCodingControlInfoKHR ctl{VK_STRUCTURE_TYPE_VIDEO_CODING_CONTROL_INFO_KHR};
        ctl.pNext = &ql;
        ctl.flags = VK_VIDEO_CODING_CONTROL_RESET_BIT_KHR |
                    VK_VIDEO_CODING_CONTROL_ENCODE_RATE_CONTROL_BIT_KHR |
                    VK_VIDEO_CODING_CONTROL_ENCODE_QUALITY_LEVEL_BIT_KHR;
        api.cmdControlCoding(cb, &ctl);
        rc_set = true;
    }

    const int qp = idr ? qp_i : qp_p;
    // H.264: the slice and the picture.
    StdVideoEncodeH264SliceHeader sh264{};
    sh264.slice_type = idr ? STD_VIDEO_H264_SLICE_TYPE_I : STD_VIDEO_H264_SLICE_TYPE_P;
    sh264.cabac_init_idc = STD_VIDEO_H264_CABAC_INIT_IDC_0;
    sh264.disable_deblocking_filter_idc = STD_VIDEO_H264_DISABLE_DEBLOCKING_FILTER_IDC_DISABLED;
    VkVideoEncodeH264NaluSliceInfoKHR slice264{VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_NALU_SLICE_INFO_KHR};
    slice264.constantQp = cqp ? qp : 0;
    slice264.pStdSliceHeader = &sh264;
    StdVideoEncodeH264ReferenceListsInfo lists264{};
    std::memset(lists264.RefPicList0, STD_VIDEO_H264_NO_REFERENCE_PICTURE, sizeof lists264.RefPicList0);
    std::memset(lists264.RefPicList1, STD_VIDEO_H264_NO_REFERENCE_PICTURE, sizeof lists264.RefPicList1);
    if (!idr) lists264.RefPicList0[0] = (uint8_t)ref;
    StdVideoEncodeH264PictureInfo p264{};
    p264.flags.IdrPicFlag = idr ? 1 : 0;
    p264.flags.is_reference = 1;
    p264.idr_pic_id = idr_id;
    p264.primary_pic_type = idr ? STD_VIDEO_H264_PICTURE_TYPE_IDR : STD_VIDEO_H264_PICTURE_TYPE_P;
    p264.frame_num = frame_num;
    p264.PicOrderCnt = poc;
    p264.pRefLists = &lists264;
    VkVideoEncodeH264PictureInfoKHR pi264{VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_PICTURE_INFO_KHR};
    pi264.naluSliceEntryCount = 1;
    pi264.pNaluSliceEntries = &slice264;
    pi264.pStdPictureInfo = &p264;

    // H.265: one slice segment, the previous picture as its one reference.
    StdVideoEncodeH265SliceSegmentHeader sh265{};
    sh265.flags.first_slice_segment_in_pic_flag = 1;
    sh265.flags.slice_sao_luma_flag = 1;
    sh265.flags.slice_sao_chroma_flag = 1;
    sh265.flags.collocated_from_l0_flag = 1;
    sh265.flags.slice_loop_filter_across_slices_enabled_flag = 1;
    sh265.slice_type = idr ? STD_VIDEO_H265_SLICE_TYPE_I : STD_VIDEO_H265_SLICE_TYPE_P;
    sh265.MaxNumMergeCand = 5;
    VkVideoEncodeH265NaluSliceSegmentInfoKHR slice265{
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_NALU_SLICE_SEGMENT_INFO_KHR};
    slice265.constantQp = cqp ? qp : 0;
    slice265.pStdSliceSegmentHeader = &sh265;
    StdVideoEncodeH265ReferenceListsInfo lists265{};
    std::memset(lists265.RefPicList0, STD_VIDEO_H265_NO_REFERENCE_PICTURE, sizeof lists265.RefPicList0);
    std::memset(lists265.RefPicList1, STD_VIDEO_H265_NO_REFERENCE_PICTURE, sizeof lists265.RefPicList1);
    if (!idr) lists265.RefPicList0[0] = (uint8_t)ref;
    StdVideoH265ShortTermRefPicSet rps{};
    if (!idr) {
        rps.num_negative_pics = 1;
        rps.delta_poc_s0_minus1[0] = 0;
        rps.used_by_curr_pic_s0_flag = 1;
    }
    StdVideoEncodeH265PictureInfo p265{};
    p265.flags.is_reference = 1;
    p265.flags.IrapPicFlag = idr ? 1 : 0;
    p265.flags.pic_output_flag = 1;
    p265.flags.slice_temporal_mvp_enabled_flag = idr ? 0 : 1;
    p265.pic_type = idr ? STD_VIDEO_H265_PICTURE_TYPE_IDR : STD_VIDEO_H265_PICTURE_TYPE_P;
    p265.PicOrderCntVal = poc / 2;
    p265.pRefLists = &lists265;
    p265.pShortTermRefPicSet = &rps;
    VkVideoEncodeH265PictureInfoKHR pi265{VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_PICTURE_INFO_KHR};
    pi265.naluSliceSegmentEntryCount = 1;
    pi265.pNaluSliceSegmentEntries = &slice265;
    pi265.pStdPictureInfo = &p265;

    // AV1: a key frame fills all eight reference buffers; a P frame refreshes
    // buffer 1, never used, and predicts from buffer 0, the key frame.
    StdVideoAV1Quantization q_av1{};
    q_av1.base_q_idx = (uint8_t)qp;
    StdVideoAV1Segmentation seg_av1{};
    StdVideoAV1LoopFilter lf_av1{};
    static const int8_t kRefDeltas[8] = {1, 0, 0, 0, -1, 0, -1, -1};
    for (int k = 0; k < 8; k++) lf_av1.loop_filter_ref_deltas[k] = kRefDeltas[k];
    lf_av1.update_mode_delta = 1;
    StdVideoAV1CDEF cdef_av1{};
    StdVideoAV1LoopRestoration lr_av1{};
    for (int k = 0; k < 3; k++) {
        lr_av1.FrameRestorationType[k] = STD_VIDEO_AV1_FRAME_RESTORATION_TYPE_NONE;
        lr_av1.LoopRestorationSize[k] = 1;
    }
    StdVideoAV1GlobalMotion gm_av1{};
    StdVideoEncodeAV1PictureInfo pav1{};
    pav1.flags.error_resilient_mode = idr ? 1 : 0;
    pav1.flags.show_frame = 1;
    pav1.flags.showable_frame = idr ? 0 : 1;
    // A render size apart from the frame size, the odd pixel's worth, is
    // written in a way decoders reject: the frame is the size it says.
    pav1.frame_type = idr ? STD_VIDEO_AV1_FRAME_TYPE_KEY : STD_VIDEO_AV1_FRAME_TYPE_INTER;
    pav1.current_frame_id = (uint32_t)cur;
    pav1.order_hint = hint;
    pav1.primary_ref_frame = idr ? STD_VIDEO_AV1_PRIMARY_REF_NONE : (uint8_t)av1_name;
    pav1.refresh_frame_flags = idr ? 0xFF : 0x02;
    pav1.render_width_minus_1 = (uint16_t)(picture.width - 1);
    pav1.render_height_minus_1 = (uint16_t)(picture.height - 1);
    pav1.interpolation_filter = STD_VIDEO_AV1_INTERPOLATION_FILTER_EIGHTTAP;
    pav1.TxMode = STD_VIDEO_AV1_TX_MODE_SELECT;
    for (int k = 0; k < 8; k++) pav1.ref_order_hint[k] = vbi_hint[k];
    for (int k = 0; k < 7; k++) pav1.ref_frame_idx[k] = 0;
    pav1.pQuantization = &q_av1;
    pav1.pSegmentation = &seg_av1;
    pav1.pLoopFilter = &lf_av1;
    pav1.pCDEF = &cdef_av1;
    pav1.pLoopRestoration = &lr_av1;
    pav1.pGlobalMotion = &gm_av1;
    pav1.pExtensionHeader = &ext_av1;
    VkVideoEncodeAV1PictureInfoKHR piav1{VK_STRUCTURE_TYPE_VIDEO_ENCODE_AV1_PICTURE_INFO_KHR};
    piav1.predictionMode = idr ? VK_VIDEO_ENCODE_AV1_PREDICTION_MODE_INTRA_ONLY_KHR
                               : VK_VIDEO_ENCODE_AV1_PREDICTION_MODE_SINGLE_REFERENCE_KHR;
    piav1.rateControlGroup = idr ? VK_VIDEO_ENCODE_AV1_RATE_CONTROL_GROUP_INTRA_KHR
                                 : VK_VIDEO_ENCODE_AV1_RATE_CONTROL_GROUP_PREDICTIVE_KHR;
    piav1.constantQIndex = cqp ? (uint32_t)qp : 0;
    piav1.pStdPictureInfo = &pav1;
    for (int k = 0; k < int(VK_MAX_VIDEO_AV1_REFERENCES_PER_FRAME_KHR); k++)
        piav1.referenceNameSlotIndices[k] = -1;
    if (!idr) piav1.referenceNameSlotIndices[av1_name] = ref;

    VkVideoReferenceSlotInfoKHR setup{VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR};
    setup.pNext = dpb_info(cur);
    setup.slotIndex = cur;
    setup.pPictureResource = &pic[cur];
    VkVideoReferenceSlotInfoKHR refs{VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR};
    refs.pNext = dpb_info(ref);
    refs.slotIndex = ref;
    refs.pPictureResource = &pic[ref];

    VkVideoEncodeInfoKHR ei{VK_STRUCTURE_TYPE_VIDEO_ENCODE_INFO_KHR};
    ei.pNext = av1() ? (const void*)&piav1 : h265() ? (const void*)&pi265 : (const void*)&pi264;
    ei.dstBuffer = bs;
    ei.dstBufferOffset = 0;
    ei.dstBufferRange = bs_size;
    ei.srcPictureResource = src_pic;
    ei.pSetupReferenceSlot = &setup;
    ei.referenceSlotCount = idr ? 0 : 1;
    ei.pReferenceSlots = idr ? nullptr : &refs;

    vkCmdBeginQuery(cb, query, 0, 0);
    api.cmdEncode(cb, &ei);
    vkCmdEndQuery(cb, query, 0);
    VkVideoEndCodingInfoKHR end{VK_STRUCTURE_TYPE_VIDEO_END_CODING_INFO_KHR};
    api.cmdEndCoding(cb, &end);
    vkEndCommandBuffer(cb);

    // Waits for the compute queue's copy into the source picture.
    vk::Stream& stream = vk::Stream::get();
    VkSemaphore wait = stream.timeline();
    const uint64_t wait_value = stream.lastSubmitted();
    VkTimelineSemaphoreSubmitInfo tsi{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
    tsi.waitSemaphoreValueCount = 1;
    tsi.pWaitSemaphoreValues = &wait_value;
    const VkPipelineStageFlags stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.pNext = &tsi;
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &wait;
    si.pWaitDstStageMask = &stage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    vkResetFences(dev, 1, &fence);
    VkResult r = vkQueueSubmit(queue, 1, &si, fence);
    if (r != VK_SUCCESS) {
        error = std::string("encode submit failed: ") + vk::Context::resultName(r);
        return false;
    }
    r = vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX);
    if (r != VK_SUCCESS) {
        error = std::string("encode did not finish: ") + vk::Context::resultName(r);
        return false;
    }
    // 32-bit results: NVIDIA 595 writes these as 32-bit words whatever
    // VK_QUERY_RESULT_64_BIT asks for.
    uint32_t fb[3] = {0, 0, 0};
    r = vkGetQueryPoolResults(dev, query, 0, 1, sizeof fb, fb, sizeof fb,
                              VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_WITH_STATUS_BIT_KHR);
    if (r != VK_SUCCESS || (int32_t)fb[2] <= 0) {
        error = "the encoder reported a failed frame";
        return false;
    }
    const VkDeviceSize off = fb[0], bytes = fb[1];
    if (off + bytes > bs_size) {
        error = "the encoded frame overran its buffer (" + std::to_string(off) + " + " +
                std::to_string(bytes) + " of " + std::to_string(bs_size) + ")";
        return false;
    }
    out.assign((const uint8_t*)bs_map + off, (const uint8_t*)bs_map + off + bytes);

    slot_frame_num[cur] = frame_num;
    slot_poc[cur] = av1() ? hint : poc;
    slot_idr[cur] = idr;
    if (idr) for (uint8_t& h : vbi_hint) h = hint;
    else vbi_hint[1] = hint;
    if (idr) idr_id = (uint16_t)(idr_id + 1);
    in_gop++;
    frame++;
    return true;
}

VideoEncoder::VideoEncoder() = default;
VideoEncoder::~VideoEncoder() = default;

bool VideoEncoder::open(const EncodeOptions& o, std::string& error) {
    _impl = std::make_unique<Impl>();
    _impl->o = o;
    if (o.width < 16 || o.height < 16 || !(o.fps > 0.0)) {
        error = "bad frame size or rate";
        return false;
    }
    try {
        return _impl->create(error);
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

bool VideoEncoder::encode(const uint8_t* rgb, std::vector<uint8_t>& au, bool& sync,
                          std::string& error) {
    Impl& s = *_impl;
    sync = s.in_gop == 0 || s.in_gop >= s.gop;
    try {
        if (!s.upload(rgb, error)) return false;
        return s.record_and_submit(sync, au, error);
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

const std::vector<uint8_t>& VideoEncoder::headers() const { return _impl->headers; }

void VideoEncoder::max_size(int& width, int& height) const {
    width = (int)_impl->caps.maxCodedExtent.width;
    height = (int)_impl->caps.maxCodedExtent.height;
}

}  // namespace video

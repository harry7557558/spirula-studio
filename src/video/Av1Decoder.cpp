// AV1 picture-parameter driver for VK_KHR_video_decode_av1.
//
// AV1 asks far more of the application than H.264 or H.265 do. There are no
// parameter-set NAL units to hand over wholesale: every frame carries an
// uncompressed header describing quantization, segmentation, loop filter, CDEF,
// loop restoration, tiling and global motion, and the driver wants all of it as
// StdVideo structures. So this file contains a complete AV1 frame-header
// parser -- section 5.9 of the specification, essentially verbatim -- plus the
// reference-frame state it depends on (5.9.2's load_previous(), the saved
// per-reference loop-filter deltas, segmentation data and global motion).
//
// Film grain synthesis is deliberately not requested: the frames feed a
// segmentation model, grain is a cosmetic post-process, and asking for it would
// force the distinct-output decode path on drivers that would rather not.

#include "nn/core/Log.h"
#include "video/BitReader.h"
#include "video/CodecDecoder.h"
#include "video/VideoApi.h"

#include <algorithm>
#include <cstring>

namespace video {

namespace {

// Spec constants.
constexpr int kNumRefFrames = 8;
constexpr int kRefsPerFrame = 7;
constexpr int kLastFrame = 1, kGoldenFrame = 4, kBwdRefFrame = 5, kAltRef2Frame = 6,
              kAltRefFrame = 7;
constexpr int kPrimaryRefNone = 7;
constexpr int kMaxSegments = 8, kSegLvlMax = 8, kSegLvlAltQ = 0;
constexpr int kMaxTileCols = 64, kMaxTileRows = 64;
constexpr int kWarpedModelPrecBits = 16;
constexpr int kSelectScreenContentTools = 2, kSelectIntegerMv = 2;
constexpr int kSuperresNum = 8, kSuperresDenomMin = 9, kSuperresDenomBits = 3;
// Global-motion transformation types. StdVideoAV1GlobalMotion::GmType is a
// plain uint8_t -- the video std headers do not name these.
constexpr int kWarpIdentity = 0, kWarpTranslation = 1, kWarpRotZoom = 2, kWarpAffine = 3;

enum ObuType {
    kObuSequenceHeader = 1, kObuTemporalDelimiter = 2, kObuFrameHeader = 3,
    kObuTileGroup = 4, kObuMetadata = 5, kObuFrame = 6, kObuRedundantFrameHeader = 7,
};

const int kSegFeatureBits[kSegLvlMax] = {8, 6, 6, 6, 6, 3, 0, 0};
const int kSegFeatureSigned[kSegLvlMax] = {1, 1, 1, 1, 1, 0, 0, 0};
const int kSegFeatureMax[kSegLvlMax] = {255, 63, 63, 63, 63, 7, 0, 0};
const StdVideoAV1FrameRestorationType kRemapLrType[4] = {
    STD_VIDEO_AV1_FRAME_RESTORATION_TYPE_NONE,
    STD_VIDEO_AV1_FRAME_RESTORATION_TYPE_SWITCHABLE,
    STD_VIDEO_AV1_FRAME_RESTORATION_TYPE_WIENER,
    STD_VIDEO_AV1_FRAME_RESTORATION_TYPE_SGRPROJ};
const int8_t kDefaultRefDeltas[8] = {1, 0, 0, 0, -1, 0, -1, -1};

int clip3(int lo, int hi, int v) { return v < lo ? lo : (v > hi ? hi : v); }

int tile_log2(int blkSize, int target) {
    int k = 0;
    while ((blkSize << k) < target) ++k;
    return k;
}

int inverse_recenter(int r, int v) {
    if (v > 2 * r) return v;
    if (v & 1) return r - ((v + 1) >> 1);
    return r + (v >> 1);
}

// Everything a decoded frame contributes to the frames that follow it.
struct RefState {
    bool valid = false;
    int  frame_id = 0;
    int  upscaled_width = 0, frame_width = 0, frame_height = 0;
    int  render_width = 0, render_height = 0;
    int  mi_cols = 0, mi_rows = 0;
    int  frame_type = 0;
    int  order_hint = 0;
    int  saved_order_hints[kNumRefFrames] = {};
    bool showable = false;
    bool disable_frame_end_update_cdf = false;
    bool segmentation_enabled = false;
    int8_t ref_deltas[8] = {};
    int8_t mode_deltas[2] = {};
    uint8_t feature_enabled[kMaxSegments] = {};
    int16_t feature_data[kMaxSegments][kSegLvlMax] = {};
    int32_t gm_params[kNumRefFrames][6] = {};
    int  dpb_slot = -1;
};

class Av1Decoder final : public CodecDecoder {
public:
    VkVideoCodecOperationFlagBitsKHR operation() const override {
        return VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR;
    }
    const void* profileExt() const override { return &profile_; }
    const StreamFormat& format() const override { return format_; }

    bool init(const TrackInfo& track, std::string& error) override;
    VkVideoSessionParametersKHR createParameters(VkDevice device, VkVideoSessionKHR session,
                                                  std::string& error) override;
    bool decodeFrame(const uint8_t* data, size_t size, int nal_length_size,
                     std::vector<uint8_t>& bitstream, std::vector<uint32_t>& slice_offsets,
                     PictureInfo& out, std::string& error) override;
    void commitFrame() override;
    void flush() override;

private:
    struct ObuInfo { uint32_t type; size_t header; size_t payload; };
    static bool readObu(const uint8_t* p, size_t avail, ObuInfo& out);
    // One coded frame's OBUs: a frame header plus its tile groups, or a single
    // OBU_FRAME that packs both.
    struct FrameUnit { size_t start, end; bool frame_obu; };

    bool parseSequenceHeader(const uint8_t* data, size_t size, std::string& error);
    bool parseFrameHeader(BitReader& br, std::string& error);
    void collectTiles(const uint8_t* tg, size_t size, size_t base_offset);
    // Sub-parsers, named after the spec's syntax functions.
    void frameSize(BitReader& br);
    void superresParams(BitReader& br);
    void computeImageSize();
    void renderSize(BitReader& br);
    void frameSizeWithRefs(BitReader& br);
    void setFrameRefs();
    void tileInfo(BitReader& br);
    void quantizationParams(BitReader& br);
    void segmentationParams(BitReader& br);
    void loopFilterParams(BitReader& br);
    void cdefParams(BitReader& br);
    void lrParams(BitReader& br);
    void globalMotionParams(BitReader& br);
    void readGlobalParam(BitReader& br, int type, int ref, int idx);
    int  decodeSubexp(BitReader& br, int numSyms);
    int  decodeUnsignedSubexpWithRef(BitReader& br, int mx, int r);
    int  decodeSignedSubexpWithRef(BitReader& br, int low, int high, int r);
    void filmGrainParams(BitReader& br);
    void setupPastIndependence();
    void loadPrevious();
    int  getRelativeDist(int a, int b) const;
    int  getQIndex(int segment_id) const;
    int  allocSlot();

    // --- sequence header ---
    StdVideoAV1SequenceHeader seq_{};
    StdVideoAV1ColorConfig    color_{};
    StdVideoAV1TimingInfo     timing_{};
    bool seq_valid_ = false;
    bool decoder_model_info_present_ = false;
    bool equal_picture_interval_ = true;
    int  order_hint_bits_ = 0;
    int  num_planes_ = 3;
    int  buffer_delay_length_ = 0;
    int  operating_points_cnt_ = 1;
    bool initial_display_delay_present_ = false;

    VkVideoDecodeAV1ProfileInfoKHR profile_{
        VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_PROFILE_INFO_KHR, nullptr,
        STD_VIDEO_AV1_PROFILE_MAIN, VK_FALSE};
    StreamFormat format_{};

    // --- current frame header ---
    StdVideoDecodeAV1PictureInfo std_pic_{};
    VkVideoDecodeAV1PictureInfoKHR vk_pic_{};
    StdVideoAV1TileInfo        tile_info_{};
    StdVideoAV1Quantization    quant_{};
    StdVideoAV1Segmentation    seg_{};
    StdVideoAV1LoopFilter      lf_{};
    StdVideoAV1CDEF            cdef_{};
    StdVideoAV1LoopRestoration lr_{};
    StdVideoAV1GlobalMotion    gm_{};
    std::vector<uint16_t> mi_col_starts_, mi_row_starts_, width_in_sbs_, height_in_sbs_;
    std::vector<uint32_t> tile_offsets_, tile_sizes_;

    // Frame-header state, in spec variable names.
    int  frame_type_ = 0;
    bool frame_is_intra_ = true;
    bool show_frame_ = true, showable_frame_ = false, show_existing_frame_ = false;
    int  frame_to_show_map_idx_ = 0;
    bool error_resilient_ = false, disable_cdf_update_ = false;
    bool allow_screen_content_tools_ = false, force_integer_mv_ = false;
    bool frame_size_override_ = false;
    int  current_frame_id_ = 0;
    int  order_hint_ = 0;
    int  primary_ref_frame_ = kPrimaryRefNone;
    int  refresh_frame_flags_ = 0;
    bool allow_high_precision_mv_ = false, use_ref_frame_mvs_ = false, allow_intrabc_ = false;
    bool is_motion_mode_switchable_ = false, disable_frame_end_update_cdf_ = false;
    bool allow_warped_motion_ = false, reduced_tx_set_ = false, reference_select_ = false;
    bool skip_mode_present_ = false, is_filter_switchable_ = false;
    bool frame_refs_short_signaling_ = false;
    bool use_superres_ = false, render_and_frame_size_different_ = false;
    int  coded_denom_ = 0, superres_denom_ = kSuperresNum;
    int  frame_width_ = 0, frame_height_ = 0, upscaled_width_ = 0;
    int  render_width_ = 0, render_height_ = 0;
    int  mi_cols_ = 0, mi_rows_ = 0;
    int  ref_frame_idx_[kRefsPerFrame] = {};
    int  order_hints_[kNumRefFrames] = {};
    int  ref_frame_sign_bias_[kNumRefFrames] = {};
    int  delta_frame_id_[kRefsPerFrame] = {};
    StdVideoAV1InterpolationFilter interp_filter_ =
        STD_VIDEO_AV1_INTERPOLATION_FILTER_EIGHTTAP;
    bool segmentation_enabled_ = false, segmentation_update_map_ = false;
    bool segmentation_temporal_update_ = false, segmentation_update_data_ = false;
    int  base_q_idx_ = 0;
    int  delta_q_y_dc_ = 0, delta_q_u_dc_ = 0, delta_q_u_ac_ = 0;
    int  delta_q_v_dc_ = 0, delta_q_v_ac_ = 0;
    bool delta_q_present_ = false, delta_lf_present_ = false, delta_lf_multi_ = false;
    int  delta_q_res_ = 0, delta_lf_res_ = 0;
    bool coded_lossless_ = false, all_lossless_ = false;
    bool uses_lr_ = false, uses_chroma_lr_ = false;
    int8_t ref_deltas_[8] = {}, mode_deltas_[2] = {};
    int32_t prev_gm_params_[kNumRefFrames][6] = {};
    int  tile_cols_ = 1, tile_rows_ = 1, tile_cols_log2_ = 0, tile_rows_log2_ = 0;
    int  tile_size_bytes_ = 4;
    uint8_t skip_mode_frame_[2] = {};
    bool apply_grain_ = false;

    std::vector<uint8_t>   packet_;
    std::vector<FrameUnit> units_;
    size_t                 next_unit_ = 0;

    // Reference state.
    RefState refs_[kNumRefFrames];
    std::vector<bool> slot_used_;
    StdVideoDecodeAV1ReferenceInfo std_refs_[kNumRefFrames]{};
    StdVideoDecodeAV1ReferenceInfo setup_ref_{};
    int  pending_slot_ = -1;
    bool pending_valid_ = false;
    bool params_dirty_ = false;
};

// ---------------------------------------------------------------------------
// OBU framing (5.3)
// ---------------------------------------------------------------------------

bool Av1Decoder::readObu(const uint8_t* p, size_t avail, ObuInfo& out) {
    if (avail < 1) return false;
    BitReader br(p, avail);
    br.bit();  // obu_forbidden_bit
    out.type = br.u(4);
    const bool ext = br.flag();
    const bool has_size = br.flag();
    br.bit();  // obu_reserved_1bit
    out.header = 1;
    if (ext) {
        br.u(8);
        out.header = 2;
    }
    out.payload = avail - out.header;
    if (has_size) {
        int used = 0;
        const uint64_t sz = br.leb128(&used);
        out.header += (size_t)used;
        if (out.header > avail || sz > avail - out.header) return false;
        out.payload = (size_t)sz;
    }
    return true;
}

// 5.11.1 tile_group_obu(). `base_offset` is where `tg` sits inside the
// bitstream buffer, so the recorded tile offsets are what the driver expects.
void Av1Decoder::collectTiles(const uint8_t* tg, size_t size, size_t base_offset) {
    BitReader br(tg, size);
    const int num_tiles = tile_cols_ * tile_rows_;
    int tg_start = 0, tg_end = num_tiles - 1;
    if (num_tiles > 1 && br.flag()) {
        const int bits = tile_cols_log2_ + tile_rows_log2_;
        tg_start = (int)br.u(bits);
        tg_end = (int)br.u(bits);
    }
    br.byteAlign();
    size_t cursor = br.bitPos() / 8;
    size_t remain = size > cursor ? size - cursor : 0;
    for (int t = tg_start; t <= tg_end && remain > 0; ++t) {
        size_t tile_size;
        if (t == tg_end) {
            tile_size = remain;
        } else {
            if (remain < (size_t)tile_size_bytes_) break;
            uint32_t v = 0;
            for (int b = 0; b < tile_size_bytes_; ++b)
                v |= (uint32_t)tg[cursor + (size_t)b] << (8 * b);
            tile_size = (size_t)v + 1;
            cursor += (size_t)tile_size_bytes_;
            remain -= (size_t)tile_size_bytes_;
            if (tile_size > remain) break;
        }
        tile_offsets_.push_back((uint32_t)(base_offset + cursor));
        tile_sizes_.push_back((uint32_t)tile_size);
        cursor += tile_size;
        remain -= tile_size;
    }
}

// ---------------------------------------------------------------------------
// Sequence header (5.5)
// ---------------------------------------------------------------------------

bool Av1Decoder::parseSequenceHeader(const uint8_t* data, size_t size, std::string& error) {
    BitReader br(data, size);
    StdVideoAV1SequenceHeader s{};
    StdVideoAV1ColorConfig c{};

    s.seq_profile = (StdVideoAV1Profile)br.u(3);
    s.flags.still_picture = br.bit();
    s.flags.reduced_still_picture_header = br.bit();

    if (s.flags.reduced_still_picture_header) {
        br.u(5);  // seq_level_idx[0]
        s.flags.timing_info_present_flag = 0;
        decoder_model_info_present_ = false;
        s.flags.initial_display_delay_present_flag = 0;
        operating_points_cnt_ = 1;
    } else {
        s.flags.timing_info_present_flag = br.bit();
        if (s.flags.timing_info_present_flag) {
            timing_.num_units_in_display_tick = br.u(32);
            timing_.time_scale = br.u(32);
            timing_.flags.equal_picture_interval = br.bit();
            if (timing_.flags.equal_picture_interval)
                timing_.num_ticks_per_picture_minus_1 = br.uvlc();
            equal_picture_interval_ = timing_.flags.equal_picture_interval != 0;
            decoder_model_info_present_ = br.flag();
            if (decoder_model_info_present_) {
                buffer_delay_length_ = (int)br.u(5) + 1;
                br.u(32);  // num_units_in_decoding_tick
                br.u(5);   // buffer_removal_time_length_minus_1
                br.u(5);   // frame_presentation_time_length_minus_1
            }
        } else {
            decoder_model_info_present_ = false;
        }
        s.flags.initial_display_delay_present_flag = br.bit();
        initial_display_delay_present_ = s.flags.initial_display_delay_present_flag != 0;
        operating_points_cnt_ = (int)br.u(5) + 1;
        for (int i = 0; i < operating_points_cnt_; ++i) {
            br.u(12);  // operating_point_idc
            const uint32_t level = br.u(5);
            if (level > 7) br.bit();  // seq_tier
            if (decoder_model_info_present_) {
                if (br.flag()) {  // decoder_model_present_for_this_op
                    br.u(buffer_delay_length_);
                    br.u(buffer_delay_length_);
                    br.bit();
                }
            }
            if (initial_display_delay_present_) {
                if (br.flag()) br.u(4);
            }
        }
    }

    s.frame_width_bits_minus_1 = (uint8_t)br.u(4);
    s.frame_height_bits_minus_1 = (uint8_t)br.u(4);
    s.max_frame_width_minus_1 = (uint16_t)br.u(s.frame_width_bits_minus_1 + 1);
    s.max_frame_height_minus_1 = (uint16_t)br.u(s.frame_height_bits_minus_1 + 1);

    if (!s.flags.reduced_still_picture_header) s.flags.frame_id_numbers_present_flag = br.bit();
    if (s.flags.frame_id_numbers_present_flag) {
        s.delta_frame_id_length_minus_2 = (uint8_t)br.u(4);
        s.additional_frame_id_length_minus_1 = (uint8_t)br.u(3);
    }
    s.flags.use_128x128_superblock = br.bit();
    s.flags.enable_filter_intra = br.bit();
    s.flags.enable_intra_edge_filter = br.bit();

    if (s.flags.reduced_still_picture_header) {
        s.seq_force_screen_content_tools = kSelectScreenContentTools;
        s.seq_force_integer_mv = kSelectIntegerMv;
        s.order_hint_bits_minus_1 = 0;
        order_hint_bits_ = 0;
    } else {
        s.flags.enable_interintra_compound = br.bit();
        s.flags.enable_masked_compound = br.bit();
        s.flags.enable_warped_motion = br.bit();
        s.flags.enable_dual_filter = br.bit();
        s.flags.enable_order_hint = br.bit();
        if (s.flags.enable_order_hint) {
            s.flags.enable_jnt_comp = br.bit();
            s.flags.enable_ref_frame_mvs = br.bit();
        }
        if (br.flag()) {  // seq_choose_screen_content_tools
            s.seq_force_screen_content_tools = kSelectScreenContentTools;
        } else {
            s.seq_force_screen_content_tools = (uint8_t)br.bit();
        }
        if (s.seq_force_screen_content_tools > 0) {
            if (br.flag())  // seq_choose_integer_mv
                s.seq_force_integer_mv = kSelectIntegerMv;
            else
                s.seq_force_integer_mv = (uint8_t)br.bit();
        } else {
            s.seq_force_integer_mv = kSelectIntegerMv;
        }
        if (s.flags.enable_order_hint) {
            s.order_hint_bits_minus_1 = (uint8_t)br.u(3);
            order_hint_bits_ = s.order_hint_bits_minus_1 + 1;
        } else {
            order_hint_bits_ = 0;
        }
    }
    s.flags.enable_superres = br.bit();
    s.flags.enable_cdef = br.bit();
    s.flags.enable_restoration = br.bit();

    // color_config()
    const bool high_bitdepth = br.flag();
    if (s.seq_profile == STD_VIDEO_AV1_PROFILE_PROFESSIONAL && high_bitdepth) {
        c.BitDepth = br.flag() ? 12 : 10;
    } else {
        c.BitDepth = high_bitdepth ? 10 : 8;
    }
    if (s.seq_profile != STD_VIDEO_AV1_PROFILE_HIGH) c.flags.mono_chrome = br.bit();
    num_planes_ = c.flags.mono_chrome ? 1 : 3;
    c.flags.color_description_present_flag = br.bit();
    if (c.flags.color_description_present_flag) {
        c.color_primaries = (StdVideoAV1ColorPrimaries)br.u(8);
        c.transfer_characteristics = (StdVideoAV1TransferCharacteristics)br.u(8);
        c.matrix_coefficients = (StdVideoAV1MatrixCoefficients)br.u(8);
    } else {
        c.color_primaries = STD_VIDEO_AV1_COLOR_PRIMARIES_BT_UNSPECIFIED;
        c.transfer_characteristics = STD_VIDEO_AV1_TRANSFER_CHARACTERISTICS_UNSPECIFIED;
        c.matrix_coefficients = STD_VIDEO_AV1_MATRIX_COEFFICIENTS_UNSPECIFIED;
    }
    if (c.flags.mono_chrome) {
        c.flags.color_range = br.bit();
        c.subsampling_x = 1;
        c.subsampling_y = 1;
        c.chroma_sample_position = STD_VIDEO_AV1_CHROMA_SAMPLE_POSITION_UNKNOWN;
        c.flags.separate_uv_delta_q = 0;
    } else if (c.color_primaries == STD_VIDEO_AV1_COLOR_PRIMARIES_BT_709 &&
               c.transfer_characteristics ==
                   STD_VIDEO_AV1_TRANSFER_CHARACTERISTICS_SRGB &&
               c.matrix_coefficients == STD_VIDEO_AV1_MATRIX_COEFFICIENTS_IDENTITY) {
        c.flags.color_range = 1;
        c.subsampling_x = 0;
        c.subsampling_y = 0;
        c.flags.separate_uv_delta_q = br.bit();
    } else {
        c.flags.color_range = br.bit();
        if (s.seq_profile == STD_VIDEO_AV1_PROFILE_MAIN) {
            c.subsampling_x = 1;
            c.subsampling_y = 1;
        } else if (s.seq_profile == STD_VIDEO_AV1_PROFILE_HIGH) {
            c.subsampling_x = 0;
            c.subsampling_y = 0;
        } else {
            if (c.BitDepth == 12) {
                c.subsampling_x = (uint8_t)br.bit();
                c.subsampling_y = c.subsampling_x ? (uint8_t)br.bit() : 0;
            } else {
                c.subsampling_x = 1;
                c.subsampling_y = 0;
            }
        }
        if (c.subsampling_x && c.subsampling_y)
            c.chroma_sample_position = (StdVideoAV1ChromaSamplePosition)br.u(2);
        c.flags.separate_uv_delta_q = br.bit();
    }
    s.flags.film_grain_params_present = br.bit();

    if (br.overrun()) {
        error = "truncated AV1 sequence header";
        return false;
    }

    seq_ = s;
    color_ = c;
    seq_.pColorConfig = &color_;
    seq_.pTimingInfo = seq_.flags.timing_info_present_flag ? &timing_ : nullptr;
    seq_valid_ = true;
    params_dirty_ = true;

    format_.width = seq_.max_frame_width_minus_1 + 1;
    format_.height = seq_.max_frame_height_minus_1 + 1;
    format_.coded_width = format_.width;
    format_.coded_height = format_.height;
    format_.bit_depth = color_.BitDepth;
    format_.chroma_format = color_.flags.mono_chrome
                                ? 0
                                : (color_.subsampling_x && color_.subsampling_y
                                       ? 1
                                       : (color_.subsampling_x ? 2 : 3));
    format_.max_dpb_slots = kNumRefFrames + 1;
    format_.max_active_references = kRefsPerFrame;
    // AV1 has no reordering: show_frame / show_existing_frame put pictures out
    // in presentation order already.
    format_.max_reorder = 0;
    format_.full_range = color_.flags.color_range != 0;
    format_.matrix_coefficients =
        color_.flags.color_description_present_flag ? (int)color_.matrix_coefficients : 2;
    switch (seq_.seq_profile) {
        case STD_VIDEO_AV1_PROFILE_MAIN: format_.profile_name = "Main"; break;
        case STD_VIDEO_AV1_PROFILE_HIGH: format_.profile_name = "High"; break;
        default: format_.profile_name = "Professional"; break;
    }
    profile_.stdProfile = seq_.seq_profile;
    profile_.filmGrainSupport = VK_FALSE;
    slot_used_.assign(format_.max_dpb_slots, false);
    return true;
}

// ---------------------------------------------------------------------------
// Frame header helpers
// ---------------------------------------------------------------------------

int Av1Decoder::getRelativeDist(int a, int b) const {
    if (!seq_.flags.enable_order_hint) return 0;
    int diff = a - b;
    const int m = 1 << (order_hint_bits_ - 1);
    diff = (diff & (m - 1)) - (diff & m);
    return diff;
}

void Av1Decoder::superresParams(BitReader& br) {
    use_superres_ = seq_.flags.enable_superres ? br.flag() : false;
    superres_denom_ = kSuperresNum;
    coded_denom_ = 0;
    if (use_superres_) {
        coded_denom_ = (int)br.u(kSuperresDenomBits);
        superres_denom_ = coded_denom_ + kSuperresDenomMin;
    }
    upscaled_width_ = frame_width_;
    frame_width_ =
        (upscaled_width_ * kSuperresNum + (superres_denom_ / 2)) / superres_denom_;
}

void Av1Decoder::computeImageSize() {
    mi_cols_ = 2 * ((frame_width_ + 7) >> 3);
    mi_rows_ = 2 * ((frame_height_ + 7) >> 3);
}

void Av1Decoder::frameSize(BitReader& br) {
    if (frame_size_override_) {
        frame_width_ = (int)br.u(seq_.frame_width_bits_minus_1 + 1) + 1;
        frame_height_ = (int)br.u(seq_.frame_height_bits_minus_1 + 1) + 1;
    } else {
        frame_width_ = seq_.max_frame_width_minus_1 + 1;
        frame_height_ = seq_.max_frame_height_minus_1 + 1;
    }
    superresParams(br);
    computeImageSize();
}

void Av1Decoder::renderSize(BitReader& br) {
    render_and_frame_size_different_ = br.flag();
    if (render_and_frame_size_different_) {
        render_width_ = (int)br.u(16) + 1;
        render_height_ = (int)br.u(16) + 1;
    } else {
        render_width_ = upscaled_width_;
        render_height_ = frame_height_;
    }
}

void Av1Decoder::frameSizeWithRefs(BitReader& br) {
    bool found = false;
    for (int i = 0; i < kRefsPerFrame; ++i) {
        if (br.flag()) {
            const RefState& r = refs_[ref_frame_idx_[i]];
            upscaled_width_ = r.upscaled_width;
            frame_width_ = upscaled_width_;
            frame_height_ = r.frame_height;
            render_width_ = r.render_width;
            render_height_ = r.render_height;
            found = true;
            break;
        }
    }
    if (!found) {
        frameSize(br);
        renderSize(br);
    } else {
        superresParams(br);
        computeImageSize();
    }
}

// 7.8 set_frame_refs()
void Av1Decoder::setFrameRefs() {
    // Called with ref_frame_idx_[0] (LAST) and [3] (GOLDEN) already set.
    int used[kNumRefFrames] = {};
    for (int i = 0; i < kRefsPerFrame; ++i)
        if (i != 0 && i != 3) ref_frame_idx_[i] = -1;
    used[ref_frame_idx_[0]] = 1;
    used[ref_frame_idx_[3]] = 1;

    const int cur_hint = 1 << (order_hint_bits_ - 1);
    int shifted[kNumRefFrames];
    for (int i = 0; i < kNumRefFrames; ++i)
        shifted[i] = cur_hint + getRelativeDist(refs_[i].order_hint, order_hint_);

    auto find_latest_backward = [&]() {
        int ref = -1, hint = 0;
        for (int i = 0; i < kNumRefFrames; ++i) {
            if (used[i]) continue;
            if (shifted[i] >= cur_hint && (ref < 0 || shifted[i] >= hint)) {
                ref = i;
                hint = shifted[i];
            }
        }
        return ref;
    };
    auto find_earliest_backward = [&]() {
        int ref = -1, hint = 0;
        for (int i = 0; i < kNumRefFrames; ++i) {
            if (used[i]) continue;
            if (shifted[i] >= cur_hint && (ref < 0 || shifted[i] < hint)) {
                ref = i;
                hint = shifted[i];
            }
        }
        return ref;
    };
    auto find_latest_forward = [&]() {
        int ref = -1, hint = 0;
        for (int i = 0; i < kNumRefFrames; ++i) {
            if (used[i]) continue;
            if (shifted[i] < cur_hint && (ref < 0 || shifted[i] >= hint)) {
                ref = i;
                hint = shifted[i];
            }
        }
        return ref;
    };

    int ref = find_latest_backward();
    if (ref >= 0) { ref_frame_idx_[kAltRefFrame - kLastFrame] = ref; used[ref] = 1; }
    ref = find_earliest_backward();
    if (ref >= 0) { ref_frame_idx_[kBwdRefFrame - kLastFrame] = ref; used[ref] = 1; }
    ref = find_earliest_backward();
    if (ref >= 0) { ref_frame_idx_[kAltRef2Frame - kLastFrame] = ref; used[ref] = 1; }

    const int list[5] = {2, 3, kBwdRefFrame, kAltRef2Frame, kAltRefFrame};  // LAST2, LAST3, ...
    for (int i = 0; i < 5; ++i) {
        const int slot = list[i] - kLastFrame;
        if (ref_frame_idx_[slot] >= 0) continue;
        ref = find_latest_forward();
        if (ref >= 0) { ref_frame_idx_[slot] = ref; used[ref] = 1; }
    }
    int earliest = -1, earliest_hint = 0;
    for (int i = 0; i < kNumRefFrames; ++i)
        if (earliest < 0 || shifted[i] < earliest_hint) {
            earliest = i;
            earliest_hint = shifted[i];
        }
    for (int i = 0; i < kRefsPerFrame; ++i)
        if (ref_frame_idx_[i] < 0) ref_frame_idx_[i] = earliest;
}

void Av1Decoder::tileInfo(BitReader& br) {
    const int sb_shift = seq_.flags.use_128x128_superblock ? 5 : 4;
    const int sb_size = sb_shift + 2;
    const int sb_cols = seq_.flags.use_128x128_superblock ? ((mi_cols_ + 31) >> 5)
                                                          : ((mi_cols_ + 15) >> 4);
    const int sb_rows = seq_.flags.use_128x128_superblock ? ((mi_rows_ + 31) >> 5)
                                                          : ((mi_rows_ + 15) >> 4);
    const int max_tile_width_sb = 4096 >> sb_size;
    int max_tile_area_sb = (4096 * 2304) >> (2 * sb_size);
    const int min_log2_tile_cols = tile_log2(max_tile_width_sb, sb_cols);
    const int max_log2_tile_cols = tile_log2(1, std::min(sb_cols, kMaxTileCols));
    const int max_log2_tile_rows = tile_log2(1, std::min(sb_rows, kMaxTileRows));
    const int min_log2_tiles =
        std::max(min_log2_tile_cols, tile_log2(max_tile_area_sb, sb_rows * sb_cols));

    mi_col_starts_.clear();
    mi_row_starts_.clear();
    width_in_sbs_.clear();
    height_in_sbs_.clear();

    const bool uniform = br.flag();
    tile_info_.flags.uniform_tile_spacing_flag = uniform ? 1 : 0;
    if (uniform) {
        tile_cols_log2_ = min_log2_tile_cols;
        while (tile_cols_log2_ < max_log2_tile_cols) {
            if (!br.flag()) break;
            ++tile_cols_log2_;
        }
        const int tile_width_sb = (sb_cols + (1 << tile_cols_log2_) - 1) >> tile_cols_log2_;
        int i = 0;
        for (int start = 0; start < sb_cols; start += tile_width_sb, ++i) {
            mi_col_starts_.push_back((uint16_t)(start << sb_shift));
            width_in_sbs_.push_back((uint16_t)(std::min(tile_width_sb, sb_cols - start) - 1));
        }
        mi_col_starts_.push_back((uint16_t)mi_cols_);
        tile_cols_ = i;

        const int min_log2_tile_rows = std::max(min_log2_tiles - tile_cols_log2_, 0);
        tile_rows_log2_ = min_log2_tile_rows;
        while (tile_rows_log2_ < max_log2_tile_rows) {
            if (!br.flag()) break;
            ++tile_rows_log2_;
        }
        const int tile_height_sb = (sb_rows + (1 << tile_rows_log2_) - 1) >> tile_rows_log2_;
        i = 0;
        for (int start = 0; start < sb_rows; start += tile_height_sb, ++i) {
            mi_row_starts_.push_back((uint16_t)(start << sb_shift));
            height_in_sbs_.push_back((uint16_t)(std::min(tile_height_sb, sb_rows - start) - 1));
        }
        mi_row_starts_.push_back((uint16_t)mi_rows_);
        tile_rows_ = i;
    } else {
        int widest_tile_sb = 0;
        int start = 0, i = 0;
        for (; start < sb_cols; ++i) {
            mi_col_starts_.push_back((uint16_t)(start << sb_shift));
            const int max_width = std::min(sb_cols - start, max_tile_width_sb);
            const int w = (int)br.ns((uint32_t)max_width) + 1;
            width_in_sbs_.push_back((uint16_t)(w - 1));
            widest_tile_sb = std::max(w, widest_tile_sb);
            start += w;
        }
        mi_col_starts_.push_back((uint16_t)mi_cols_);
        tile_cols_ = i;
        tile_cols_log2_ = tile_log2(1, tile_cols_);

        if (min_log2_tiles > 0)
            max_tile_area_sb = (sb_rows * sb_cols) >> (min_log2_tiles + 1);
        else
            max_tile_area_sb = sb_rows * sb_cols;
        const int max_tile_height_sb =
            std::max(max_tile_area_sb / std::max(widest_tile_sb, 1), 1);

        start = 0;
        i = 0;
        for (; start < sb_rows; ++i) {
            mi_row_starts_.push_back((uint16_t)(start << sb_shift));
            const int max_height = std::min(sb_rows - start, max_tile_height_sb);
            const int h = (int)br.ns((uint32_t)max_height) + 1;
            height_in_sbs_.push_back((uint16_t)(h - 1));
            start += h;
        }
        mi_row_starts_.push_back((uint16_t)mi_rows_);
        tile_rows_ = i;
        tile_rows_log2_ = tile_log2(1, tile_rows_);
    }

    uint16_t context_update_tile_id = 0;
    tile_size_bytes_ = 4;
    if (tile_cols_log2_ > 0 || tile_rows_log2_ > 0) {
        context_update_tile_id = (uint16_t)br.u(tile_rows_log2_ + tile_cols_log2_);
        tile_size_bytes_ = (int)br.u(2) + 1;
    }
    tile_info_.TileCols = (uint8_t)tile_cols_;
    tile_info_.TileRows = (uint8_t)tile_rows_;
    tile_info_.context_update_tile_id = context_update_tile_id;
    tile_info_.tile_size_bytes_minus_1 = (uint8_t)(tile_size_bytes_ - 1);
    tile_info_.pMiColStarts = mi_col_starts_.data();
    tile_info_.pMiRowStarts = mi_row_starts_.data();
    tile_info_.pWidthInSbsMinus1 = width_in_sbs_.data();
    tile_info_.pHeightInSbsMinus1 = height_in_sbs_.data();
}

void Av1Decoder::quantizationParams(BitReader& br) {
    auto read_delta_q = [&]() -> int { return br.flag() ? br.su(7) : 0; };
    quant_ = StdVideoAV1Quantization{};
    base_q_idx_ = (int)br.u(8);
    delta_q_y_dc_ = read_delta_q();
    bool diff_uv_delta = false;
    if (num_planes_ > 1) {
        if (color_.flags.separate_uv_delta_q) diff_uv_delta = br.flag();
        delta_q_u_dc_ = read_delta_q();
        delta_q_u_ac_ = read_delta_q();
        if (diff_uv_delta) {
            delta_q_v_dc_ = read_delta_q();
            delta_q_v_ac_ = read_delta_q();
        } else {
            delta_q_v_dc_ = delta_q_u_dc_;
            delta_q_v_ac_ = delta_q_u_ac_;
        }
    } else {
        delta_q_u_dc_ = delta_q_u_ac_ = delta_q_v_dc_ = delta_q_v_ac_ = 0;
    }
    const bool using_qmatrix = br.flag();
    quant_.flags.using_qmatrix = using_qmatrix ? 1 : 0;
    quant_.flags.diff_uv_delta = diff_uv_delta ? 1 : 0;
    if (using_qmatrix) {
        quant_.qm_y = (uint8_t)br.u(4);
        quant_.qm_u = (uint8_t)br.u(4);
        quant_.qm_v = color_.flags.separate_uv_delta_q ? (uint8_t)br.u(4) : quant_.qm_u;
    }
    quant_.base_q_idx = (uint8_t)base_q_idx_;
    quant_.DeltaQYDc = (int8_t)delta_q_y_dc_;
    quant_.DeltaQUDc = (int8_t)delta_q_u_dc_;
    quant_.DeltaQUAc = (int8_t)delta_q_u_ac_;
    quant_.DeltaQVDc = (int8_t)delta_q_v_dc_;
    quant_.DeltaQVAc = (int8_t)delta_q_v_ac_;
}

void Av1Decoder::segmentationParams(BitReader& br) {
    segmentation_enabled_ = br.flag();
    if (segmentation_enabled_) {
        if (primary_ref_frame_ == kPrimaryRefNone) {
            segmentation_update_map_ = true;
            segmentation_temporal_update_ = false;
            segmentation_update_data_ = true;
        } else {
            segmentation_update_map_ = br.flag();
            segmentation_temporal_update_ =
                segmentation_update_map_ ? br.flag() : false;
            segmentation_update_data_ = br.flag();
        }
        if (segmentation_update_data_) {
            for (int i = 0; i < kMaxSegments; ++i) {
                seg_.FeatureEnabled[i] = 0;
                for (int j = 0; j < kSegLvlMax; ++j) {
                    int clipped = 0;
                    if (br.flag()) {
                        seg_.FeatureEnabled[i] |= (uint8_t)(1u << j);
                        const int bits = kSegFeatureBits[j];
                        const int limit = kSegFeatureMax[j];
                        if (kSegFeatureSigned[j])
                            clipped = clip3(-limit, limit, bits ? br.su(1 + bits) : 0);
                        else
                            clipped = clip3(0, limit, bits ? (int)br.u(bits) : 0);
                    }
                    seg_.FeatureData[i][j] = (int16_t)clipped;
                }
            }
        }
    } else {
        std::memset(&seg_, 0, sizeof(seg_));
    }
}

int Av1Decoder::getQIndex(int segment_id) const {
    if (segmentation_enabled_ && (seg_.FeatureEnabled[segment_id] & (1u << kSegLvlAltQ)))
        return clip3(0, 255, base_q_idx_ + seg_.FeatureData[segment_id][kSegLvlAltQ]);
    return base_q_idx_;
}

void Av1Decoder::loopFilterParams(BitReader& br) {
    lf_ = StdVideoAV1LoopFilter{};
    std::memcpy(lf_.loop_filter_ref_deltas, ref_deltas_, sizeof(ref_deltas_));
    std::memcpy(lf_.loop_filter_mode_deltas, mode_deltas_, sizeof(mode_deltas_));
    if (coded_lossless_ || allow_intrabc_) {
        lf_.loop_filter_level[0] = 0;
        lf_.loop_filter_level[1] = 0;
        std::memcpy(ref_deltas_, kDefaultRefDeltas, sizeof(ref_deltas_));
        mode_deltas_[0] = mode_deltas_[1] = 0;
        std::memcpy(lf_.loop_filter_ref_deltas, ref_deltas_, sizeof(ref_deltas_));
        std::memcpy(lf_.loop_filter_mode_deltas, mode_deltas_, sizeof(mode_deltas_));
        return;
    }
    lf_.loop_filter_level[0] = (uint8_t)br.u(6);
    lf_.loop_filter_level[1] = (uint8_t)br.u(6);
    if (num_planes_ > 1 && (lf_.loop_filter_level[0] || lf_.loop_filter_level[1])) {
        lf_.loop_filter_level[2] = (uint8_t)br.u(6);
        lf_.loop_filter_level[3] = (uint8_t)br.u(6);
    }
    lf_.loop_filter_sharpness = (uint8_t)br.u(3);
    lf_.flags.loop_filter_delta_enabled = br.bit();
    if (lf_.flags.loop_filter_delta_enabled) {
        lf_.flags.loop_filter_delta_update = br.bit();
        if (lf_.flags.loop_filter_delta_update) {
            for (int i = 0; i < 8; ++i)
                if (br.flag()) {
                    lf_.update_ref_delta |= (uint8_t)(1u << i);
                    ref_deltas_[i] = (int8_t)br.su(7);
                }
            for (int i = 0; i < 2; ++i)
                if (br.flag()) {
                    lf_.update_mode_delta |= (uint8_t)(1u << i);
                    mode_deltas_[i] = (int8_t)br.su(7);
                }
        }
    }
    std::memcpy(lf_.loop_filter_ref_deltas, ref_deltas_, sizeof(ref_deltas_));
    std::memcpy(lf_.loop_filter_mode_deltas, mode_deltas_, sizeof(mode_deltas_));
}

void Av1Decoder::cdefParams(BitReader& br) {
    cdef_ = StdVideoAV1CDEF{};
    if (coded_lossless_ || allow_intrabc_ || !seq_.flags.enable_cdef) {
        cdef_.cdef_damping_minus_3 = 0;
        cdef_.cdef_bits = 0;
        return;
    }
    cdef_.cdef_damping_minus_3 = (uint8_t)br.u(2);
    cdef_.cdef_bits = (uint8_t)br.u(2);
    const int n = 1 << cdef_.cdef_bits;
    for (int i = 0; i < n && i < int(STD_VIDEO_AV1_MAX_CDEF_FILTER_STRENGTHS); ++i) {
        // Secondary strengths stay as coded (0..3); the driver applies 3 -> 4.
        cdef_.cdef_y_pri_strength[i] = (uint8_t)br.u(4);
        cdef_.cdef_y_sec_strength[i] = (uint8_t)br.u(2);
        if (num_planes_ > 1) {
            cdef_.cdef_uv_pri_strength[i] = (uint8_t)br.u(4);
            cdef_.cdef_uv_sec_strength[i] = (uint8_t)br.u(2);
        }
    }
}

void Av1Decoder::lrParams(BitReader& br) {
    lr_ = StdVideoAV1LoopRestoration{};
    uses_lr_ = false;
    uses_chroma_lr_ = false;
    if (all_lossless_ || allow_intrabc_ || !seq_.flags.enable_restoration) {
        for (int i = 0; i < int(STD_VIDEO_AV1_MAX_NUM_PLANES); ++i)
            lr_.FrameRestorationType[i] = STD_VIDEO_AV1_FRAME_RESTORATION_TYPE_NONE;
        return;
    }
    for (int i = 0; i < num_planes_ && i < int(STD_VIDEO_AV1_MAX_NUM_PLANES); ++i) {
        const uint32_t t = br.u(2);
        lr_.FrameRestorationType[i] = kRemapLrType[t];
        if (lr_.FrameRestorationType[i] != STD_VIDEO_AV1_FRAME_RESTORATION_TYPE_NONE) {
            uses_lr_ = true;
            if (i > 0) uses_chroma_lr_ = true;
        }
    }
    if (!uses_lr_) return;
    int lr_unit_shift = 0;
    if (seq_.flags.use_128x128_superblock) {
        lr_unit_shift = (int)br.bit() + 1;
    } else {
        lr_unit_shift = (int)br.bit();
        if (lr_unit_shift) lr_unit_shift += (int)br.bit();
    }
    // Vulkan takes log2(unit size) - 5, not pixels: 64 << lr_unit_shift -> 1 + shift.
    lr_.LoopRestorationSize[0] = (uint16_t)(1 + lr_unit_shift);
    int uv_shift = 0;
    if (color_.subsampling_x && color_.subsampling_y && uses_chroma_lr_)
        uv_shift = (int)br.bit();
    lr_.LoopRestorationSize[1] = (uint16_t)(lr_.LoopRestorationSize[0] - uv_shift);
    lr_.LoopRestorationSize[2] = lr_.LoopRestorationSize[1];
}

int Av1Decoder::decodeSubexp(BitReader& br, int numSyms) {
    int i = 0, mk = 0;
    const int k = 3;
    while (true) {
        const int b2 = i ? k + i - 1 : k;
        const int a = 1 << b2;
        if (numSyms <= mk + 3 * a) return (int)br.ns((uint32_t)(numSyms - mk)) + mk;
        if (br.flag()) {
            ++i;
            mk += a;
        } else {
            return (int)br.u(b2) + mk;
        }
        if (i > 24) return mk;  // malformed stream guard
    }
}

int Av1Decoder::decodeUnsignedSubexpWithRef(BitReader& br, int mx, int r) {
    const int v = decodeSubexp(br, mx);
    if ((r << 1) <= mx) return inverse_recenter(r, v);
    return mx - 1 - inverse_recenter(mx - 1 - r, v);
}

int Av1Decoder::decodeSignedSubexpWithRef(BitReader& br, int low, int high, int r) {
    return decodeUnsignedSubexpWithRef(br, high - low, r - low) + low;
}

void Av1Decoder::readGlobalParam(BitReader& br, int type, int ref, int idx) {
    int abs_bits = 12;   // GM_ABS_ALPHA_BITS
    int prec_bits = 15;  // GM_ALPHA_PREC_BITS
    if (idx < 2) {
        if (type == kWarpTranslation) {
            abs_bits = 9 - (allow_high_precision_mv_ ? 0 : 1);
            prec_bits = 3 - (allow_high_precision_mv_ ? 0 : 1);
        } else {
            abs_bits = 12;
            prec_bits = 6;
        }
    }
    const int prec_diff = kWarpedModelPrecBits - prec_bits;
    const int round = ((idx % 3) == 2) ? (1 << kWarpedModelPrecBits) : 0;
    const int sub = ((idx % 3) == 2) ? (1 << prec_bits) : 0;
    const int mx = 1 << abs_bits;
    const int r = (prev_gm_params_[ref][idx] >> prec_diff) - sub;
    gm_.gm_params[ref][idx] =
        (decodeSignedSubexpWithRef(br, -mx, mx + 1, r) << prec_diff) + round;
}

void Av1Decoder::globalMotionParams(BitReader& br) {
    gm_ = StdVideoAV1GlobalMotion{};
    for (int ref = kLastFrame; ref <= kAltRefFrame; ++ref) {
        gm_.GmType[ref] = kWarpIdentity;
        for (int i = 0; i < 6; ++i)
            gm_.gm_params[ref][i] = ((i % 3) == 2) ? (1 << kWarpedModelPrecBits) : 0;
    }
    if (frame_is_intra_) return;
    for (int ref = kLastFrame; ref <= kAltRefFrame; ++ref) {
        int type = kWarpIdentity;
        if (br.flag()) {
            if (br.flag())
                type = kWarpRotZoom;
            else
                type = br.flag() ? kWarpTranslation
                                 : kWarpAffine;
        }
        gm_.GmType[ref] = (uint8_t)type;
        if (type >= kWarpRotZoom) {
            readGlobalParam(br, type, ref, 2);
            readGlobalParam(br, type, ref, 3);
            if (type == kWarpAffine) {
                readGlobalParam(br, type, ref, 4);
                readGlobalParam(br, type, ref, 5);
            } else {
                gm_.gm_params[ref][4] = -gm_.gm_params[ref][3];
                gm_.gm_params[ref][5] = gm_.gm_params[ref][2];
            }
        }
        if (type >= kWarpTranslation) {
            readGlobalParam(br, type, ref, 0);
            readGlobalParam(br, type, ref, 1);
        }
    }
}

void Av1Decoder::filmGrainParams(BitReader& br) {
    apply_grain_ = false;
    if (!seq_.flags.film_grain_params_present || (!show_frame_ && !showable_frame_)) return;
    if (!br.flag()) return;  // apply_grain
    // Parsed but not requested: see the file header comment. The syntax still
    // has to be consumed so nothing downstream reads at the wrong bit offset --
    // except nothing follows, so the walk stops here.
    apply_grain_ = true;
}

void Av1Decoder::setupPastIndependence() {
    std::memset(&seg_, 0, sizeof(seg_));
    for (int ref = 0; ref < kNumRefFrames; ++ref)
        for (int i = 0; i < 6; ++i)
            prev_gm_params_[ref][i] = ((i % 3) == 2) ? (1 << kWarpedModelPrecBits) : 0;
    std::memcpy(ref_deltas_, kDefaultRefDeltas, sizeof(ref_deltas_));
    mode_deltas_[0] = mode_deltas_[1] = 0;
}

void Av1Decoder::loadPrevious() {
    const int prev = ref_frame_idx_[primary_ref_frame_];
    const RefState& r = refs_[prev];
    std::memcpy(prev_gm_params_, r.gm_params, sizeof(prev_gm_params_));
    std::memcpy(ref_deltas_, r.ref_deltas, sizeof(ref_deltas_));
    std::memcpy(mode_deltas_, r.mode_deltas, sizeof(mode_deltas_));
    for (int i = 0; i < kMaxSegments; ++i) {
        seg_.FeatureEnabled[i] = r.feature_enabled[i];
        for (int j = 0; j < kSegLvlMax; ++j) seg_.FeatureData[i][j] = r.feature_data[i][j];
    }
}

// ---------------------------------------------------------------------------
// uncompressed_header() (5.9.2)
// ---------------------------------------------------------------------------

bool Av1Decoder::parseFrameHeader(BitReader& br, std::string& error) {
    if (!seq_valid_) {
        error = "AV1 frame header before any sequence header";
        return false;
    }
    const int id_len = seq_.flags.frame_id_numbers_present_flag
                           ? (seq_.additional_frame_id_length_minus_1 +
                              seq_.delta_frame_id_length_minus_2 + 3)
                           : 0;
    const int all_frames = (1 << kNumRefFrames) - 1;
    show_existing_frame_ = false;
    apply_grain_ = false;

    if (seq_.flags.reduced_still_picture_header) {
        frame_type_ = STD_VIDEO_AV1_FRAME_TYPE_KEY;
        frame_is_intra_ = true;
        show_frame_ = true;
        showable_frame_ = false;
        error_resilient_ = true;
    } else {
        show_existing_frame_ = br.flag();
        if (show_existing_frame_) {
            frame_to_show_map_idx_ = (int)br.u(3);
            if (decoder_model_info_present_ && !equal_picture_interval_) br.u(32);
            refresh_frame_flags_ = 0;
            if (seq_.flags.frame_id_numbers_present_flag) br.u(id_len);
            frame_type_ = refs_[frame_to_show_map_idx_].frame_type;
            if (frame_type_ == STD_VIDEO_AV1_FRAME_TYPE_KEY)
                refresh_frame_flags_ = all_frames;
            return true;
        }
        frame_type_ = (int)br.u(2);
        frame_is_intra_ = (frame_type_ == STD_VIDEO_AV1_FRAME_TYPE_INTRA_ONLY ||
                           frame_type_ == STD_VIDEO_AV1_FRAME_TYPE_KEY);
        show_frame_ = br.flag();
        if (show_frame_ && decoder_model_info_present_ && !equal_picture_interval_) br.u(32);
        if (show_frame_)
            showable_frame_ = frame_type_ != STD_VIDEO_AV1_FRAME_TYPE_KEY;
        else
            showable_frame_ = br.flag();
        if (frame_type_ == STD_VIDEO_AV1_FRAME_TYPE_SWITCH ||
            (frame_type_ == STD_VIDEO_AV1_FRAME_TYPE_KEY && show_frame_))
            error_resilient_ = true;
        else
            error_resilient_ = br.flag();
    }

    if (frame_type_ == STD_VIDEO_AV1_FRAME_TYPE_KEY && show_frame_) {
        for (int i = 0; i < kNumRefFrames; ++i) {
            refs_[i].valid = false;
            refs_[i].order_hint = 0;
        }
        for (int i = 0; i < kRefsPerFrame; ++i) order_hints_[kLastFrame + i] = 0;
    }

    disable_cdf_update_ = br.flag();
    if (seq_.seq_force_screen_content_tools == kSelectScreenContentTools)
        allow_screen_content_tools_ = br.flag();
    else
        allow_screen_content_tools_ = seq_.seq_force_screen_content_tools != 0;
    if (allow_screen_content_tools_) {
        if (seq_.seq_force_integer_mv == kSelectIntegerMv)
            force_integer_mv_ = br.flag();
        else
            force_integer_mv_ = seq_.seq_force_integer_mv != 0;
    } else {
        force_integer_mv_ = false;
    }
    if (frame_is_intra_) force_integer_mv_ = true;

    if (seq_.flags.frame_id_numbers_present_flag)
        current_frame_id_ = (int)br.u(id_len);
    else
        current_frame_id_ = 0;

    if (frame_type_ == STD_VIDEO_AV1_FRAME_TYPE_SWITCH)
        frame_size_override_ = true;
    else if (seq_.flags.reduced_still_picture_header)
        frame_size_override_ = false;
    else
        frame_size_override_ = br.flag();

    order_hint_ = order_hint_bits_ ? (int)br.u(order_hint_bits_) : 0;
    if (frame_is_intra_ || error_resilient_)
        primary_ref_frame_ = kPrimaryRefNone;
    else
        primary_ref_frame_ = (int)br.u(3);

    if (decoder_model_info_present_) {
        if (br.flag()) {  // buffer_removal_time_present_flag
            for (int i = 0; i < operating_points_cnt_; ++i) br.u(32);
        }
    }

    allow_high_precision_mv_ = false;
    use_ref_frame_mvs_ = false;
    allow_intrabc_ = false;
    if (frame_type_ == STD_VIDEO_AV1_FRAME_TYPE_SWITCH ||
        (frame_type_ == STD_VIDEO_AV1_FRAME_TYPE_KEY && show_frame_))
        refresh_frame_flags_ = all_frames;
    else
        refresh_frame_flags_ = (int)br.u(8);

    if (!frame_is_intra_ || refresh_frame_flags_ != all_frames) {
        if (error_resilient_ && seq_.flags.enable_order_hint) {
            for (int i = 0; i < kNumRefFrames; ++i) {
                const int hint = (int)br.u(order_hint_bits_);
                if (hint != refs_[i].order_hint) refs_[i].valid = false;
                refs_[i].order_hint = hint;
            }
        }
    }

    frame_refs_short_signaling_ = false;
    if (frame_is_intra_) {
        frameSize(br);
        renderSize(br);
        if (allow_screen_content_tools_ && upscaled_width_ == frame_width_)
            allow_intrabc_ = br.flag();
    } else {
        if (seq_.flags.enable_order_hint) {
            frame_refs_short_signaling_ = br.flag();
            if (frame_refs_short_signaling_) {
                ref_frame_idx_[0] = (int)br.u(3);
                ref_frame_idx_[kGoldenFrame - kLastFrame] = (int)br.u(3);
                setFrameRefs();
            }
        }
        for (int i = 0; i < kRefsPerFrame; ++i) {
            if (!frame_refs_short_signaling_) ref_frame_idx_[i] = (int)br.u(3);
            if (seq_.flags.frame_id_numbers_present_flag)
                delta_frame_id_[i] = (int)br.u(seq_.delta_frame_id_length_minus_2 + 2) + 1;
        }
        if (frame_size_override_ && !error_resilient_) {
            frameSizeWithRefs(br);
        } else {
            frameSize(br);
            renderSize(br);
        }
        allow_high_precision_mv_ = force_integer_mv_ ? false : br.flag();
        is_filter_switchable_ = br.flag();
        interp_filter_ = is_filter_switchable_
                             ? STD_VIDEO_AV1_INTERPOLATION_FILTER_SWITCHABLE
                             : (StdVideoAV1InterpolationFilter)br.u(2);
        is_motion_mode_switchable_ = br.flag();
        use_ref_frame_mvs_ =
            (error_resilient_ || !seq_.flags.enable_ref_frame_mvs) ? false : br.flag();
        for (int i = 0; i < kRefsPerFrame; ++i) {
            const int ref_frame = kLastFrame + i;
            const int hint = refs_[ref_frame_idx_[i]].order_hint;
            order_hints_[ref_frame] = hint;
            ref_frame_sign_bias_[ref_frame] =
                seq_.flags.enable_order_hint ? (getRelativeDist(hint, order_hint_) > 0 ? 1 : 0)
                                             : 0;
        }
    }

    disable_frame_end_update_cdf_ =
        (seq_.flags.reduced_still_picture_header || disable_cdf_update_) ? true : br.flag();

    if (primary_ref_frame_ == kPrimaryRefNone)
        setupPastIndependence();
    else
        loadPrevious();

    tileInfo(br);
    quantizationParams(br);
    segmentationParams(br);

    delta_q_present_ = false;
    delta_q_res_ = 0;
    if (base_q_idx_ > 0) delta_q_present_ = br.flag();
    if (delta_q_present_) delta_q_res_ = (int)br.u(2);

    delta_lf_present_ = false;
    delta_lf_res_ = 0;
    delta_lf_multi_ = false;
    if (delta_q_present_) {
        if (!allow_intrabc_) delta_lf_present_ = br.flag();
        if (delta_lf_present_) {
            delta_lf_res_ = (int)br.u(2);
            delta_lf_multi_ = br.flag();
        }
    }

    coded_lossless_ = true;
    for (int seg = 0; seg < kMaxSegments; ++seg) {
        const int qindex = getQIndex(seg);
        const bool lossless = qindex == 0 && delta_q_y_dc_ == 0 && delta_q_u_ac_ == 0 &&
                              delta_q_u_dc_ == 0 && delta_q_v_ac_ == 0 && delta_q_v_dc_ == 0;
        if (!lossless) {
            coded_lossless_ = false;
            break;
        }
    }
    all_lossless_ = coded_lossless_ && (frame_width_ == upscaled_width_);

    loopFilterParams(br);
    cdefParams(br);
    lrParams(br);

    // read_tx_mode()
    StdVideoAV1TxMode tx_mode;
    if (coded_lossless_) {
        tx_mode = STD_VIDEO_AV1_TX_MODE_ONLY_4X4;
    } else {
        tx_mode = br.flag() ? STD_VIDEO_AV1_TX_MODE_SELECT : STD_VIDEO_AV1_TX_MODE_LARGEST;
    }

    reference_select_ = frame_is_intra_ ? false : br.flag();

    // skip_mode_params()
    skip_mode_frame_[0] = skip_mode_frame_[1] = 0;
    bool skip_mode_allowed = false;
    if (!frame_is_intra_ && reference_select_ && seq_.flags.enable_order_hint) {
        int forward_idx = -1, backward_idx = -1, forward_hint = 0, backward_hint = 0;
        for (int i = 0; i < kRefsPerFrame; ++i) {
            const int ref_hint = refs_[ref_frame_idx_[i]].order_hint;
            if (getRelativeDist(ref_hint, order_hint_) < 0) {
                if (forward_idx < 0 || getRelativeDist(ref_hint, forward_hint) > 0) {
                    forward_idx = i;
                    forward_hint = ref_hint;
                }
            } else if (getRelativeDist(ref_hint, order_hint_) > 0) {
                if (backward_idx < 0 || getRelativeDist(ref_hint, backward_hint) < 0) {
                    backward_idx = i;
                    backward_hint = ref_hint;
                }
            }
        }
        if (forward_idx >= 0) {
            if (backward_idx >= 0) {
                skip_mode_allowed = true;
                skip_mode_frame_[0] =
                    (uint8_t)(kLastFrame + std::min(forward_idx, backward_idx));
                skip_mode_frame_[1] =
                    (uint8_t)(kLastFrame + std::max(forward_idx, backward_idx));
            } else {
                int second_idx = -1, second_hint = 0;
                for (int i = 0; i < kRefsPerFrame; ++i) {
                    const int ref_hint = refs_[ref_frame_idx_[i]].order_hint;
                    if (getRelativeDist(ref_hint, forward_hint) < 0) {
                        if (second_idx < 0 || getRelativeDist(ref_hint, second_hint) > 0) {
                            second_idx = i;
                            second_hint = ref_hint;
                        }
                    }
                }
                if (second_idx >= 0) {
                    skip_mode_allowed = true;
                    skip_mode_frame_[0] =
                        (uint8_t)(kLastFrame + std::min(forward_idx, second_idx));
                    skip_mode_frame_[1] =
                        (uint8_t)(kLastFrame + std::max(forward_idx, second_idx));
                }
            }
        }
    }
    skip_mode_present_ = skip_mode_allowed ? br.flag() : false;

    allow_warped_motion_ =
        (frame_is_intra_ || error_resilient_ || !seq_.flags.enable_warped_motion)
            ? false
            : br.flag();
    reduced_tx_set_ = br.flag();
    globalMotionParams(br);
    filmGrainParams(br);

    if (br.overrun()) {
        error = "truncated AV1 frame header";
        return false;
    }

    // ---- fill the driver-facing picture info ----
    std_pic_ = StdVideoDecodeAV1PictureInfo{};
    std_pic_.frame_type = (StdVideoAV1FrameType)frame_type_;
    std_pic_.current_frame_id = (uint32_t)current_frame_id_;
    std_pic_.OrderHint = (uint8_t)order_hint_;
    std_pic_.primary_ref_frame = (uint8_t)primary_ref_frame_;
    std_pic_.refresh_frame_flags = (uint8_t)refresh_frame_flags_;
    std_pic_.interpolation_filter = interp_filter_;
    std_pic_.TxMode = tx_mode;
    std_pic_.delta_q_res = (uint8_t)delta_q_res_;
    std_pic_.delta_lf_res = (uint8_t)delta_lf_res_;
    std_pic_.SkipModeFrame[0] = skip_mode_frame_[0];
    std_pic_.SkipModeFrame[1] = skip_mode_frame_[1];
    std_pic_.coded_denom = (uint8_t)coded_denom_;
    for (int i = 0; i < kNumRefFrames; ++i) std_pic_.OrderHints[i] = (uint8_t)order_hints_[i];
    for (int i = 0; i < kRefsPerFrame; ++i)
        std_pic_.expectedFrameId[i] = (uint32_t)delta_frame_id_[i];

    std_pic_.flags.error_resilient_mode = error_resilient_;
    std_pic_.flags.disable_cdf_update = disable_cdf_update_;
    std_pic_.flags.use_superres = use_superres_;
    std_pic_.flags.render_and_frame_size_different = render_and_frame_size_different_;
    std_pic_.flags.allow_screen_content_tools = allow_screen_content_tools_;
    std_pic_.flags.is_filter_switchable = is_filter_switchable_;
    std_pic_.flags.force_integer_mv = force_integer_mv_;
    std_pic_.flags.frame_size_override_flag = frame_size_override_;
    std_pic_.flags.allow_intrabc = allow_intrabc_;
    std_pic_.flags.frame_refs_short_signaling = frame_refs_short_signaling_;
    std_pic_.flags.allow_high_precision_mv = allow_high_precision_mv_;
    std_pic_.flags.is_motion_mode_switchable = is_motion_mode_switchable_;
    std_pic_.flags.use_ref_frame_mvs = use_ref_frame_mvs_;
    std_pic_.flags.disable_frame_end_update_cdf = disable_frame_end_update_cdf_;
    std_pic_.flags.allow_warped_motion = allow_warped_motion_;
    std_pic_.flags.reduced_tx_set = reduced_tx_set_;
    std_pic_.flags.reference_select = reference_select_;
    std_pic_.flags.skip_mode_present = skip_mode_present_;
    std_pic_.flags.delta_q_present = delta_q_present_;
    std_pic_.flags.delta_lf_present = delta_lf_present_;
    std_pic_.flags.delta_lf_multi = delta_lf_multi_;
    std_pic_.flags.segmentation_enabled = segmentation_enabled_;
    std_pic_.flags.segmentation_update_map = segmentation_update_map_;
    std_pic_.flags.segmentation_temporal_update = segmentation_temporal_update_;
    std_pic_.flags.segmentation_update_data = segmentation_update_data_;
    std_pic_.flags.UsesLr = uses_lr_;
    std_pic_.flags.usesChromaLr = uses_chroma_lr_;
    std_pic_.flags.apply_grain = 0;   // film grain synthesis is not requested

    std_pic_.pTileInfo = &tile_info_;
    std_pic_.pQuantization = &quant_;
    std_pic_.pSegmentation = &seg_;
    std_pic_.pLoopFilter = &lf_;
    std_pic_.pCDEF = &cdef_;
    std_pic_.pLoopRestoration = &lr_;
    std_pic_.pGlobalMotion = &gm_;
    std_pic_.pFilmGrain = nullptr;
    return true;
}

// ---------------------------------------------------------------------------
// CodecDecoder
// ---------------------------------------------------------------------------

bool Av1Decoder::init(const TrackInfo& track, std::string& error) {
    format_.width = track.width;
    format_.height = track.height;
    // AV1CodecConfigurationRecord: 4 fixed bytes then configOBUs, which carry
    // the sequence header.
    const std::vector<uint8_t>& cfg = track.codec_config;
    if (cfg.size() > 4) {
        const uint8_t* p = cfg.data() + 4;
        size_t remain = cfg.size() - 4;
        while (remain >= 1) {
            BitReader hb(p, remain);
            hb.bit();
            const uint32_t type = hb.u(4);
            const bool ext = hb.flag();
            const bool has_size = hb.flag();
            hb.bit();
            size_t header = 1;
            if (ext) {
                hb.u(8);
                header = 2;
            }
            size_t payload = remain - header;
            if (has_size) {
                int used = 0;
                const uint64_t sz = hb.leb128(&used);
                header += (size_t)used;
                payload = (size_t)sz;
            }
            if (header + payload > remain) break;
            if (type == kObuSequenceHeader &&
                !parseSequenceHeader(p + header, payload, error))
                return false;
            p += header + payload;
            remain -= header + payload;
        }
    }
    if (!seq_valid_) {
        error = "AV1 track carries no sequence header in its av1C record";
        return false;
    }
    return true;
}

VkVideoSessionParametersKHR Av1Decoder::createParameters(VkDevice device,
                                                          VkVideoSessionKHR session,
                                                          std::string& error) {
    VkVideoDecodeAV1SessionParametersCreateInfoKHR codec{
        VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_SESSION_PARAMETERS_CREATE_INFO_KHR};
    codec.pStdSequenceHeader = &seq_;

    VkVideoSessionParametersCreateInfoKHR ci{
        VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_CREATE_INFO_KHR};
    ci.pNext = &codec;
    ci.videoSession = session;

    VkVideoSessionParametersKHR out = VK_NULL_HANDLE;
    if (video_api().createParameters(device, &ci, nullptr, &out) != VK_SUCCESS) {
        error = "vkCreateVideoSessionParametersKHR (AV1) failed";
        return VK_NULL_HANDLE;
    }
    params_dirty_ = false;
    return out;
}

void Av1Decoder::flush() {
    for (auto& r : refs_) r = RefState{};
    slot_used_.assign(format_.max_dpb_slots, false);
    pending_slot_ = -1;
}

int Av1Decoder::allocSlot() {
    for (size_t i = 0; i < slot_used_.size(); ++i)
        if (!slot_used_[i]) return (int)i;
    return 0;
}

bool Av1Decoder::decodeFrame(const uint8_t* data, size_t size, int nal_length_size,
                             std::vector<uint8_t>& bitstream,
                             std::vector<uint32_t>& slice_offsets, PictureInfo& out,
                             std::string& error) {
    (void)nal_length_size;
    slice_offsets.clear();
    tile_offsets_.clear();
    tile_sizes_.clear();
    out = PictureInfo{};

    // A new packet: split the temporal unit into frame units up front. Only OBU
    // boundaries are needed for that, and doing it in one pass means each
    // frame's header can be parsed later, once the frames before it have
    // updated the reference state it depends on.
    if (data) {
        packet_.assign(data, data + size);
        units_.clear();
        next_unit_ = 0;

        size_t p = 0;
        FrameUnit cur{};
        bool open = false;
        while (p < packet_.size()) {
            ObuInfo obu;
            if (!readObu(packet_.data() + p, packet_.size() - p, obu)) break;
            const size_t total = obu.header + obu.payload;
            if (obu.type == kObuSequenceHeader) {
                if (!parseSequenceHeader(packet_.data() + p + obu.header, obu.payload, error))
                    return false;
            } else if (obu.type == kObuFrameHeader || obu.type == kObuFrame) {
                if (open) units_.push_back(cur);
                cur = FrameUnit{p, p + total, obu.type == kObuFrame};
                open = true;
                if (obu.type == kObuFrame) {
                    units_.push_back(cur);
                    open = false;
                }
            } else if (obu.type == kObuTileGroup && open) {
                cur.end = p + total;
            }
            p += total;
        }
        if (open) units_.push_back(cur);
    }

    if (next_unit_ >= units_.size()) {
        error = "AV1 temporal unit contains no frame header";
        return false;
    }
    const FrameUnit unit = units_[next_unit_++];
    bitstream.assign(packet_.begin() + (ptrdiff_t)unit.start,
                     packet_.begin() + (ptrdiff_t)unit.end);
    out.more_in_packet = next_unit_ < units_.size();

    // Parse the frame header, then walk the unit's tile groups.
    {
        ObuInfo obu;
        readObu(bitstream.data(), bitstream.size(), obu);
        BitReader fb(bitstream.data() + obu.header, obu.payload);
        if (!parseFrameHeader(fb, error)) return false;
        if (!show_existing_frame_) {
            if (unit.frame_obu) {
                fb.byteAlign();
                const size_t tg_off = fb.bitPos() / 8;
                collectTiles(bitstream.data() + obu.header + tg_off, obu.payload - tg_off,
                             obu.header + tg_off);
            } else {
                size_t q = obu.header + obu.payload;
                while (q < bitstream.size()) {
                    ObuInfo t;
                    if (!readObu(bitstream.data() + q, bitstream.size() - q, t)) break;
                    if (t.type == kObuTileGroup)
                        collectTiles(bitstream.data() + q + t.header, t.payload,
                                     q + t.header);
                    q += t.header + t.payload;
                }
            }
        }
    }

    // ---- show_existing_frame: re-output a picture already in the DPB ----
    if (show_existing_frame_) {
        const RefState& r = refs_[frame_to_show_map_idx_];
        out.show_existing_slot = r.dpb_slot;
        out.poc = r.order_hint;
        out.output = true;
        out.setup_slot = -1;
        pending_slot_ = -1;
        pending_valid_ = false;
        for (int i = 0; i < kNumRefFrames; ++i)
            if (refs_[i].valid && refs_[i].dpb_slot >= 0) out.live_slots.push_back(refs_[i].dpb_slot);
        return true;
    }

    if (tile_offsets_.empty()) {
        error = "AV1 frame carries no tile data";
        return false;
    }

    // ---- reference slots ----
    out.refs.clear();
    vk_pic_ = VkVideoDecodeAV1PictureInfoKHR{
        VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_PICTURE_INFO_KHR};
    for (int i = 0; i < int(VK_MAX_VIDEO_AV1_REFERENCES_PER_FRAME_KHR); ++i)
        vk_pic_.referenceNameSlotIndices[i] = -1;
    if (!frame_is_intra_) {
        for (int i = 0; i < kRefsPerFrame; ++i) {
            const RefState& r = refs_[ref_frame_idx_[i]];
            if (!r.valid || r.dpb_slot < 0) continue;
            vk_pic_.referenceNameSlotIndices[i] = r.dpb_slot;
            bool seen = false;
            for (const auto& e : out.refs)
                if (e.slot == r.dpb_slot) { seen = true; break; }
            if (seen) continue;
            StdVideoDecodeAV1ReferenceInfo& info = std_refs_[ref_frame_idx_[i]];
            info = StdVideoDecodeAV1ReferenceInfo{};
            info.frame_type = (uint8_t)r.frame_type;
            info.OrderHint = (uint8_t)r.order_hint;
            info.RefFrameSignBias = (uint8_t)ref_frame_sign_bias_[kLastFrame + i];
            info.flags.disable_frame_end_update_cdf = r.disable_frame_end_update_cdf ? 1 : 0;
            info.flags.segmentation_enabled = r.segmentation_enabled ? 1 : 0;
            for (int j = 0; j < kNumRefFrames; ++j)
                info.SavedOrderHints[j] = (uint8_t)r.saved_order_hints[j];
            out.refs.push_back({r.dpb_slot, &info});
        }
    }

    const int slot = allocSlot();
    pending_slot_ = slot;
    pending_valid_ = true;

    setup_ref_ = StdVideoDecodeAV1ReferenceInfo{};
    setup_ref_.frame_type = (uint8_t)frame_type_;
    setup_ref_.OrderHint = (uint8_t)order_hint_;
    setup_ref_.flags.disable_frame_end_update_cdf = disable_frame_end_update_cdf_ ? 1 : 0;
    setup_ref_.flags.segmentation_enabled = segmentation_enabled_ ? 1 : 0;
    for (int j = 0; j < kNumRefFrames; ++j)
        setup_ref_.SavedOrderHints[j] = (uint8_t)order_hints_[j];

    vk_pic_.pStdPictureInfo = &std_pic_;
    vk_pic_.frameHeaderOffset = 0;   // the unit starts at the frame header OBU
    vk_pic_.tileCount = (uint32_t)tile_offsets_.size();
    vk_pic_.pTileOffsets = tile_offsets_.data();
    vk_pic_.pTileSizes = tile_sizes_.data();

    out.decode_pnext = &vk_pic_;
    out.setup_slot = slot;
    out.setup_std_ref = &setup_ref_;
    out.poc = order_hint_;
    out.output = show_frame_;
    out.params_changed = params_dirty_;
    out.sequence_restart = frame_type_ == STD_VIDEO_AV1_FRAME_TYPE_KEY && show_frame_;

    out.live_slots.push_back(slot);
    for (int i = 0; i < kNumRefFrames; ++i) {
        if (refresh_frame_flags_ & (1 << i)) continue;   // about to be replaced
        if (refs_[i].valid && refs_[i].dpb_slot >= 0)
            out.live_slots.push_back(refs_[i].dpb_slot);
    }
    return true;
}

void Av1Decoder::commitFrame() {
    if (show_existing_frame_) {
        // A shown key frame resets every reference to point at it.
        if (refresh_frame_flags_ == 0xff) {
            const RefState src = refs_[frame_to_show_map_idx_];
            for (int i = 0; i < kNumRefFrames; ++i) refs_[i] = src;
        }
        return;
    }
    if (pending_slot_ < 0 || !pending_valid_) return;

    RefState cur{};
    cur.valid = true;
    cur.frame_id = current_frame_id_;
    cur.upscaled_width = upscaled_width_;
    cur.frame_width = frame_width_;
    cur.frame_height = frame_height_;
    cur.render_width = render_width_;
    cur.render_height = render_height_;
    cur.mi_cols = mi_cols_;
    cur.mi_rows = mi_rows_;
    cur.frame_type = frame_type_;
    cur.order_hint = order_hint_;
    cur.showable = showable_frame_;
    cur.disable_frame_end_update_cdf = disable_frame_end_update_cdf_;
    cur.segmentation_enabled = segmentation_enabled_;
    cur.dpb_slot = pending_slot_;
    for (int i = 0; i < kNumRefFrames; ++i) cur.saved_order_hints[i] = order_hints_[i];
    std::memcpy(cur.ref_deltas, ref_deltas_, sizeof(ref_deltas_));
    std::memcpy(cur.mode_deltas, mode_deltas_, sizeof(mode_deltas_));
    for (int i = 0; i < kMaxSegments; ++i) {
        cur.feature_enabled[i] = seg_.FeatureEnabled[i];
        for (int j = 0; j < kSegLvlMax; ++j) cur.feature_data[i][j] = seg_.FeatureData[i][j];
    }
    std::memcpy(cur.gm_params, gm_.gm_params, sizeof(cur.gm_params));

    for (int i = 0; i < kNumRefFrames; ++i)
        if (refresh_frame_flags_ & (1 << i)) refs_[i] = cur;

    // A DPB slot stays occupied while some reference still names it.
    slot_used_.assign(slot_used_.size(), false);
    for (int i = 0; i < kNumRefFrames; ++i)
        if (refs_[i].valid && refs_[i].dpb_slot >= 0 &&
            (size_t)refs_[i].dpb_slot < slot_used_.size())
            slot_used_[(size_t)refs_[i].dpb_slot] = true;
    pending_slot_ = -1;
}

}  // namespace

std::unique_ptr<CodecDecoder> make_av1_decoder() { return std::make_unique<Av1Decoder>(); }

}  // namespace video

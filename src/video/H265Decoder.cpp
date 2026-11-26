// H.265 / HEVC picture-parameter driver for VK_KHR_video_decode_h265.
//
// Same division of labour as the H.264 path: the driver does the slice decode,
// this file supplies the parameter sets and the reference-picture state. HEVC
// puts more of that state in the bitstream than AVC does -- the reference
// picture set is rebuilt from scratch for every picture rather than mutated --
// so most of what follows is st_ref_pic_set() and the POC bookkeeping around it.
//
// Frame pictures only; field_seq_flag streams are rejected by name.

#include "nn/core/Log.h"
#include "video/BitReader.h"
#include "video/CodecDecoder.h"
#include "video/VideoApi.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace video {

namespace {

constexpr int kMaxDpb = 17;
constexpr int kMaxRps = 16;

// NAL unit types we care about.
enum : int {
    kTrailN = 0, kRaslN = 8, kRaslR = 9,
    kBlaWLp = 16, kIdrWRadl = 19, kIdrNLp = 20, kCraNut = 21, kRsvIrap23 = 23,
    kVps = 32, kSps = 33, kPps = 34,
};

bool is_irap(int t) { return t >= kBlaWLp && t <= kRsvIrap23; }
bool is_idr(int t) { return t == kIdrWRadl || t == kIdrNLp; }
bool is_rasl(int t) { return t == kRaslN || t == kRaslR; }

// Derived reference picture set, in the form 8.3.2 needs.
struct Rps {
    int  num_neg = 0, num_pos = 0;
    int  delta_poc_s0[kMaxRps] = {};   // negative
    int  delta_poc_s1[kMaxRps] = {};   // positive
    bool used_s0[kMaxRps] = {};
    bool used_s1[kMaxRps] = {};
    int  numDeltaPocs() const { return num_neg + num_pos; }
};

struct Sps {
    StdVideoH265SequenceParameterSet    std{};
    StdVideoH265ProfileTierLevel        ptl{};
    StdVideoH265DecPicBufMgr            dpbm{};
    StdVideoH265ScalingLists            scaling{};
    StdVideoH265SequenceParameterSetVui vui{};
    StdVideoH265LongTermRefPicsSps      lt{};
    std::vector<StdVideoH265ShortTermRefPicSet> std_rps;
    std::vector<Rps> rps;              // derived form, for our own DPB work
    uint32_t max_poc_lsb = 256;
    int      width = 0, height = 0;
    uint32_t max_dpb = 6, max_reorder = 0;
    bool     valid = false;
};

struct Pps {
    StdVideoH265PictureParameterSet std{};
    StdVideoH265ScalingLists        scaling{};
    bool valid = false;
};

struct Vps {
    StdVideoH265VideoParameterSet std{};
    StdVideoH265ProfileTierLevel  ptl{};
    StdVideoH265DecPicBufMgr      dpbm{};
    bool valid = false;
};

struct DpbFrame {
    bool used = false;
    bool long_term = false;
    int  poc = 0;
    StdVideoDecodeH265ReferenceInfo std_ref{};
};

StdVideoH265LevelIdc level_from_idc(uint32_t v) {
    // general_level_idc is 30 x the level number.
    switch (v) {
        case 30: return STD_VIDEO_H265_LEVEL_IDC_1_0;
        case 60: return STD_VIDEO_H265_LEVEL_IDC_2_0;
        case 63: return STD_VIDEO_H265_LEVEL_IDC_2_1;
        case 90: return STD_VIDEO_H265_LEVEL_IDC_3_0;
        case 93: return STD_VIDEO_H265_LEVEL_IDC_3_1;
        case 120: return STD_VIDEO_H265_LEVEL_IDC_4_0;
        case 123: return STD_VIDEO_H265_LEVEL_IDC_4_1;
        case 150: return STD_VIDEO_H265_LEVEL_IDC_5_0;
        case 153: return STD_VIDEO_H265_LEVEL_IDC_5_1;
        case 156: return STD_VIDEO_H265_LEVEL_IDC_5_2;
        case 180: return STD_VIDEO_H265_LEVEL_IDC_6_0;
        case 183: return STD_VIDEO_H265_LEVEL_IDC_6_1;
        default: return STD_VIDEO_H265_LEVEL_IDC_6_2;
    }
}

uint32_t ceil_log2(uint32_t v) {
    uint32_t n = 0;
    while ((1u << n) < v) ++n;
    return n;
}

void parse_ptl(BitReader& br, bool profile_present, int max_sub_layers_minus1,
               StdVideoH265ProfileTierLevel& out) {
    if (profile_present) {
        br.u(2);                                   // general_profile_space
        out.flags.general_tier_flag = br.bit();
        out.general_profile_idc = (StdVideoH265ProfileIdc)br.u(5);
        for (int i = 0; i < 32; ++i) br.bit();     // profile_compatibility_flag
        out.flags.general_progressive_source_flag = br.bit();
        out.flags.general_interlaced_source_flag = br.bit();
        out.flags.general_non_packed_constraint_flag = br.bit();
        out.flags.general_frame_only_constraint_flag = br.bit();
        for (int i = 0; i < 43; ++i) br.bit();     // reserved / range-extension flags
        br.bit();                                  // inbld / reserved
    }
    out.general_level_idc = level_from_idc(br.u(8));

    std::vector<uint8_t> sub_profile(max_sub_layers_minus1);
    std::vector<uint8_t> sub_level(max_sub_layers_minus1);
    for (int i = 0; i < max_sub_layers_minus1; ++i) {
        sub_profile[i] = (uint8_t)br.bit();
        sub_level[i] = (uint8_t)br.bit();
    }
    if (max_sub_layers_minus1 > 0)
        for (int i = max_sub_layers_minus1; i < 8; ++i) br.u(2);
    for (int i = 0; i < max_sub_layers_minus1; ++i) {
        if (sub_profile[i]) {
            br.u(2);
            br.bit();
            br.u(5);
            for (int j = 0; j < 32; ++j) br.bit();
            for (int j = 0; j < 4; ++j) br.bit();
            for (int j = 0; j < 43; ++j) br.bit();
            br.bit();
        }
        if (sub_level[i]) br.u(8);
    }
}

void parse_sub_layer_hrd(BitReader& br, uint32_t cpb_cnt, bool sub_pic_present) {
    for (uint32_t i = 0; i < cpb_cnt; ++i) {
        br.ue();
        br.ue();
        if (sub_pic_present) {
            br.ue();
            br.ue();
        }
        br.bit();
    }
}

void parse_hrd(BitReader& br, bool common_inf_present, int max_sub_layers_minus1) {
    bool nal_hrd = false, vcl_hrd = false, sub_pic = false;
    if (common_inf_present) {
        nal_hrd = br.flag();
        vcl_hrd = br.flag();
        if (nal_hrd || vcl_hrd) {
            sub_pic = br.flag();
            if (sub_pic) {
                br.u(8);
                br.u(5);
                br.bit();
                br.u(5);
            }
            br.u(4);
            br.u(4);
            if (sub_pic) br.u(4);
            br.u(5);
            br.u(5);
            br.u(5);
        }
    }
    for (int i = 0; i <= max_sub_layers_minus1; ++i) {
        const bool fixed_pic_rate_general = br.flag();
        bool fixed_pic_rate_within_cvs = fixed_pic_rate_general;
        if (!fixed_pic_rate_general) fixed_pic_rate_within_cvs = br.flag();
        bool low_delay = false;
        if (fixed_pic_rate_within_cvs) br.ue();
        else low_delay = br.flag();
        uint32_t cpb_cnt = 1;
        if (!low_delay) cpb_cnt = br.ue() + 1;
        if (nal_hrd) parse_sub_layer_hrd(br, cpb_cnt, sub_pic);
        if (vcl_hrd) parse_sub_layer_hrd(br, cpb_cnt, sub_pic);
    }
}

void parse_dpb_mgr(BitReader& br, bool per_sublayer, int max_sub_layers_minus1,
                   StdVideoH265DecPicBufMgr& out) {
    const int first = per_sublayer ? 0 : max_sub_layers_minus1;
    for (int i = first; i <= max_sub_layers_minus1; ++i) {
        out.max_dec_pic_buffering_minus1[i] = (uint8_t)br.ue();
        out.max_num_reorder_pics[i] = (uint8_t)br.ue();
        out.max_latency_increase_plus1[i] = br.ue();
    }
    for (int i = 0; i < first; ++i) {
        out.max_dec_pic_buffering_minus1[i] =
            out.max_dec_pic_buffering_minus1[max_sub_layers_minus1];
        out.max_num_reorder_pics[i] = out.max_num_reorder_pics[max_sub_layers_minus1];
        out.max_latency_increase_plus1[i] =
            out.max_latency_increase_plus1[max_sub_layers_minus1];
    }
}

void parse_scaling_list_data(BitReader& br, StdVideoH265ScalingLists& out) {
    for (int size_id = 0; size_id < 4; ++size_id) {
        for (int matrix_id = 0; matrix_id < 6; matrix_id += (size_id == 3) ? 3 : 1) {
            uint8_t* dst = nullptr;
            int coefs = 0;
            switch (size_id) {
                case 0: dst = out.ScalingList4x4[matrix_id]; coefs = 16; break;
                case 1: dst = out.ScalingList8x8[matrix_id]; coefs = 64; break;
                case 2: dst = out.ScalingList16x16[matrix_id]; coefs = 64; break;
                default: dst = out.ScalingList32x32[matrix_id / 3]; coefs = 64; break;
            }
            if (!br.flag()) {
                br.ue();  // scaling_list_pred_matrix_id_delta
                continue;
            }
            int next = 8;
            if (size_id > 1) {
                const int dc = br.se() + 8;
                next = dc;
                if (size_id == 2) out.ScalingListDCCoef16x16[matrix_id] = (uint8_t)dc;
                else out.ScalingListDCCoef32x32[matrix_id / 3] = (uint8_t)dc;
            }
            for (int i = 0; i < coefs; ++i) {
                next = (next + br.se() + 256) % 256;
                dst[i] = (uint8_t)next;
            }
        }
    }
}

// 7.3.7 st_ref_pic_set(). `sets` holds the RPSs already parsed from this SPS,
// which an inter-predicted set refers back to.
bool parse_st_rps(BitReader& br, uint32_t idx, uint32_t num_sets, const std::vector<Rps>& sets,
                  Rps& out, StdVideoH265ShortTermRefPicSet* std_out, int* num_delta_of_ref) {
    out = Rps{};
    if (std_out) *std_out = StdVideoH265ShortTermRefPicSet{};
    if (num_delta_of_ref) *num_delta_of_ref = 0;

    bool inter_pred = false;
    if (idx != 0) inter_pred = br.flag();
    if (inter_pred) {
        uint32_t delta_idx_minus1 = 0;
        if (idx == num_sets) delta_idx_minus1 = br.ue();
        const bool delta_rps_sign = br.flag();
        const uint32_t abs_delta_rps_minus1 = br.ue();
        const int ref_idx = (int)idx - (int)(delta_idx_minus1 + 1);
        if (ref_idx < 0 || ref_idx >= (int)sets.size()) return false;
        const Rps& ref = sets[(size_t)ref_idx];
        if (num_delta_of_ref) *num_delta_of_ref = ref.numDeltaPocs();

        const int n = ref.numDeltaPocs();
        std::vector<uint8_t> used(n + 1, 0), use_delta(n + 1, 1);
        uint16_t used_mask = 0, use_delta_mask = 0;
        for (int j = 0; j <= n; ++j) {
            used[j] = (uint8_t)br.bit();
            if (!used[j]) use_delta[j] = (uint8_t)br.bit();
            if (used[j]) used_mask |= (uint16_t)(1u << j);
            if (use_delta[j]) use_delta_mask |= (uint16_t)(1u << j);
        }
        if (std_out) {
            std_out->flags.inter_ref_pic_set_prediction_flag = 1;
            std_out->flags.delta_rps_sign = delta_rps_sign ? 1 : 0;
            std_out->delta_idx_minus1 = delta_idx_minus1;
            std_out->abs_delta_rps_minus1 = (uint16_t)abs_delta_rps_minus1;
            std_out->used_by_curr_pic_flag = used_mask;
            std_out->use_delta_flag = use_delta_mask;
        }

        const int delta_rps = (1 - 2 * (int)delta_rps_sign) * (int)(abs_delta_rps_minus1 + 1);
        int i = 0;
        for (int j = ref.num_pos - 1; j >= 0; --j) {
            const int d = ref.delta_poc_s1[j] + delta_rps;
            if (d < 0 && use_delta[ref.num_neg + j] && i < kMaxRps) {
                out.delta_poc_s0[i] = d;
                out.used_s0[i++] = used[ref.num_neg + j] != 0;
            }
        }
        if (delta_rps < 0 && use_delta[n] && i < kMaxRps) {
            out.delta_poc_s0[i] = delta_rps;
            out.used_s0[i++] = used[n] != 0;
        }
        for (int j = 0; j < ref.num_neg; ++j) {
            const int d = ref.delta_poc_s0[j] + delta_rps;
            if (d < 0 && use_delta[j] && i < kMaxRps) {
                out.delta_poc_s0[i] = d;
                out.used_s0[i++] = used[j] != 0;
            }
        }
        out.num_neg = i;

        i = 0;
        for (int j = ref.num_neg - 1; j >= 0; --j) {
            const int d = ref.delta_poc_s0[j] + delta_rps;
            if (d > 0 && use_delta[j] && i < kMaxRps) {
                out.delta_poc_s1[i] = d;
                out.used_s1[i++] = used[j] != 0;
            }
        }
        if (delta_rps > 0 && use_delta[n] && i < kMaxRps) {
            out.delta_poc_s1[i] = delta_rps;
            out.used_s1[i++] = used[n] != 0;
        }
        for (int j = 0; j < ref.num_pos; ++j) {
            const int d = ref.delta_poc_s1[j] + delta_rps;
            if (d > 0 && use_delta[ref.num_neg + j] && i < kMaxRps) {
                out.delta_poc_s1[i] = d;
                out.used_s1[i++] = used[ref.num_neg + j] != 0;
            }
        }
        out.num_pos = i;
        return !br.overrun();
    }

    const uint32_t num_neg = br.ue();
    const uint32_t num_pos = br.ue();
    if (num_neg > kMaxRps || num_pos > kMaxRps) return false;
    out.num_neg = (int)num_neg;
    out.num_pos = (int)num_pos;
    uint16_t s0_mask = 0, s1_mask = 0;
    int prev = 0;
    for (uint32_t i = 0; i < num_neg; ++i) {
        const uint32_t d = br.ue();
        prev -= (int)(d + 1);
        out.delta_poc_s0[i] = prev;
        out.used_s0[i] = br.flag();
        if (out.used_s0[i]) s0_mask |= (uint16_t)(1u << i);
        if (std_out) std_out->delta_poc_s0_minus1[i] = (uint16_t)d;
    }
    prev = 0;
    for (uint32_t i = 0; i < num_pos; ++i) {
        const uint32_t d = br.ue();
        prev += (int)(d + 1);
        out.delta_poc_s1[i] = prev;
        out.used_s1[i] = br.flag();
        if (out.used_s1[i]) s1_mask |= (uint16_t)(1u << i);
        if (std_out) std_out->delta_poc_s1_minus1[i] = (uint16_t)d;
    }
    if (std_out) {
        std_out->num_negative_pics = (uint8_t)num_neg;
        std_out->num_positive_pics = (uint8_t)num_pos;
        std_out->used_by_curr_pic_s0_flag = s0_mask;
        std_out->used_by_curr_pic_s1_flag = s1_mask;
    }
    return !br.overrun();
}

// ---------------------------------------------------------------------------

class H265Decoder final : public CodecDecoder {
public:
    VkVideoCodecOperationFlagBitsKHR operation() const override {
        return VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR;
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
    bool parseVps(const uint8_t* nal, size_t size, std::string& error);
    bool parseSps(const uint8_t* nal, size_t size, std::string& error);
    bool parsePps(const uint8_t* nal, size_t size, std::string& error);
    bool parseNalArrays(const std::vector<uint8_t>& cfg, std::string& error);
    void updateFormat(const Sps& sps);
    int  allocSlot();

    std::array<std::unique_ptr<Vps>, 16>  vps_;
    std::array<std::unique_ptr<Sps>, 16>  sps_;
    std::array<std::unique_ptr<Pps>, 64>  pps_;
    std::array<DpbFrame, kMaxDpb>         dpb_{};

    VkVideoDecodeH265ProfileInfoKHR profile_{
        VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PROFILE_INFO_KHR, nullptr,
        STD_VIDEO_H265_PROFILE_IDC_MAIN};
    StreamFormat format_{};

    StdVideoDecodeH265PictureInfo   std_pic_{};
    VkVideoDecodeH265PictureInfoKHR vk_pic_{};
    StdVideoDecodeH265ReferenceInfo setup_ref_{};

    // POC state.
    int  poc_ = 0;
    int  prev_tid0_poc_lsb_ = 0, prev_tid0_poc_msb_ = 0;
    bool no_rasl_output_ = true;   // NoRaslOutputFlag of the most recent IRAP
    bool seen_irap_ = false;

    int  pending_slot_ = -1;
    bool pending_is_ref_ = true;
    bool params_dirty_ = false;
    bool have_sps_ = false;
};

// ---------------------------------------------------------------------------
// Parameter sets
// ---------------------------------------------------------------------------

bool H265Decoder::parseVps(const uint8_t* nal, size_t size, std::string& error) {
    RbspReader br(nal, size, 2);
    auto vps = std::make_unique<Vps>();
    StdVideoH265VideoParameterSet& v = vps->std;

    const uint32_t id = br.u(4);
    if (id >= 16) {
        error = "H.265 VPS id out of range";
        return false;
    }
    v.vps_video_parameter_set_id = (uint8_t)id;
    br.u(2);   // vps_base_layer_internal/available flags
    br.u(6);   // vps_max_layers_minus1
    const int max_sub = (int)br.u(3);
    v.vps_max_sub_layers_minus1 = (uint8_t)max_sub;
    v.flags.vps_temporal_id_nesting_flag = br.bit();
    br.u(16);  // reserved 0xffff
    parse_ptl(br, true, max_sub, vps->ptl);
    const bool sub_layer_ordering = br.flag();
    v.flags.vps_sub_layer_ordering_info_present_flag = sub_layer_ordering ? 1 : 0;
    parse_dpb_mgr(br, sub_layer_ordering, max_sub, vps->dpbm);
    if (br.overrun()) {
        error = "truncated H.265 VPS";
        return false;
    }
    v.pProfileTierLevel = &vps->ptl;
    v.pDecPicBufMgr = &vps->dpbm;
    vps->valid = true;
    vps_[id] = std::move(vps);
    params_dirty_ = true;
    return true;
}

bool H265Decoder::parseSps(const uint8_t* nal, size_t size, std::string& error) {
    RbspReader br(nal, size, 2);
    auto sps = std::make_unique<Sps>();
    StdVideoH265SequenceParameterSet& s = sps->std;

    s.sps_video_parameter_set_id = (uint8_t)br.u(4);
    const int max_sub = (int)br.u(3);
    s.sps_max_sub_layers_minus1 = (uint8_t)max_sub;
    s.flags.sps_temporal_id_nesting_flag = br.bit();
    parse_ptl(br, true, max_sub, sps->ptl);

    const uint32_t id = br.ue();
    if (id >= 16) {
        error = "H.265 SPS id out of range";
        return false;
    }
    s.sps_seq_parameter_set_id = (uint8_t)id;
    const uint32_t chroma = br.ue();
    s.chroma_format_idc = (StdVideoH265ChromaFormatIdc)chroma;
    if (chroma == 3) s.flags.separate_colour_plane_flag = br.bit();
    s.pic_width_in_luma_samples = br.ue();
    s.pic_height_in_luma_samples = br.ue();
    s.flags.conformance_window_flag = br.bit();
    if (s.flags.conformance_window_flag) {
        s.conf_win_left_offset = br.ue();
        s.conf_win_right_offset = br.ue();
        s.conf_win_top_offset = br.ue();
        s.conf_win_bottom_offset = br.ue();
    }
    s.bit_depth_luma_minus8 = (uint8_t)br.ue();
    s.bit_depth_chroma_minus8 = (uint8_t)br.ue();
    s.log2_max_pic_order_cnt_lsb_minus4 = (uint8_t)br.ue();
    const bool sub_layer_ordering = br.flag();
    s.flags.sps_sub_layer_ordering_info_present_flag = sub_layer_ordering ? 1 : 0;
    parse_dpb_mgr(br, sub_layer_ordering, max_sub, sps->dpbm);
    s.log2_min_luma_coding_block_size_minus3 = (uint8_t)br.ue();
    s.log2_diff_max_min_luma_coding_block_size = (uint8_t)br.ue();
    s.log2_min_luma_transform_block_size_minus2 = (uint8_t)br.ue();
    s.log2_diff_max_min_luma_transform_block_size = (uint8_t)br.ue();
    s.max_transform_hierarchy_depth_inter = (uint8_t)br.ue();
    s.max_transform_hierarchy_depth_intra = (uint8_t)br.ue();
    s.flags.scaling_list_enabled_flag = br.bit();
    if (s.flags.scaling_list_enabled_flag) {
        s.flags.sps_scaling_list_data_present_flag = br.bit();
        if (s.flags.sps_scaling_list_data_present_flag) {
            parse_scaling_list_data(br, sps->scaling);
            s.pScalingLists = &sps->scaling;
        }
    }
    s.flags.amp_enabled_flag = br.bit();
    s.flags.sample_adaptive_offset_enabled_flag = br.bit();
    s.flags.pcm_enabled_flag = br.bit();
    if (s.flags.pcm_enabled_flag) {
        s.pcm_sample_bit_depth_luma_minus1 = (uint8_t)br.u(4);
        s.pcm_sample_bit_depth_chroma_minus1 = (uint8_t)br.u(4);
        s.log2_min_pcm_luma_coding_block_size_minus3 = (uint8_t)br.ue();
        s.log2_diff_max_min_pcm_luma_coding_block_size = (uint8_t)br.ue();
        s.flags.pcm_loop_filter_disabled_flag = br.bit();
    }
    const uint32_t num_st_rps = br.ue();
    if (num_st_rps > 64) {
        error = "H.265 SPS declares an implausible number of short-term RPSs";
        return false;
    }
    s.num_short_term_ref_pic_sets = (uint8_t)num_st_rps;
    sps->rps.reserve(num_st_rps);
    sps->std_rps.resize(num_st_rps);
    for (uint32_t i = 0; i < num_st_rps; ++i) {
        Rps r;
        if (!parse_st_rps(br, i, num_st_rps, sps->rps, r, &sps->std_rps[i], nullptr)) {
            error = "malformed H.265 short-term reference picture set";
            return false;
        }
        sps->rps.push_back(r);
    }
    if (num_st_rps) s.pShortTermRefPicSet = sps->std_rps.data();

    s.flags.long_term_ref_pics_present_flag = br.bit();
    if (s.flags.long_term_ref_pics_present_flag) {
        const uint32_t n = br.ue();
        s.num_long_term_ref_pics_sps = (uint8_t)n;
        for (uint32_t i = 0; i < n && i < STD_VIDEO_H265_MAX_LONG_TERM_REF_PICS_SPS; ++i) {
            sps->lt.lt_ref_pic_poc_lsb_sps[i] = br.u(s.log2_max_pic_order_cnt_lsb_minus4 + 4);
            if (br.bit()) sps->lt.used_by_curr_pic_lt_sps_flag |= (1u << i);
        }
        s.pLongTermRefPicsSps = &sps->lt;
    }
    s.flags.sps_temporal_mvp_enabled_flag = br.bit();
    s.flags.strong_intra_smoothing_enabled_flag = br.bit();
    s.flags.vui_parameters_present_flag = br.bit();
    if (s.flags.vui_parameters_present_flag) {
        StdVideoH265SequenceParameterSetVui& v = sps->vui;
        v.flags.aspect_ratio_info_present_flag = br.bit();
        if (v.flags.aspect_ratio_info_present_flag) {
            v.aspect_ratio_idc = (StdVideoH265AspectRatioIdc)br.u(8);
            if (v.aspect_ratio_idc == STD_VIDEO_H265_ASPECT_RATIO_IDC_EXTENDED_SAR) {
                v.sar_width = (uint16_t)br.u(16);
                v.sar_height = (uint16_t)br.u(16);
            }
        }
        v.flags.overscan_info_present_flag = br.bit();
        if (v.flags.overscan_info_present_flag) v.flags.overscan_appropriate_flag = br.bit();
        v.flags.video_signal_type_present_flag = br.bit();
        if (v.flags.video_signal_type_present_flag) {
            v.video_format = (uint8_t)br.u(3);
            v.flags.video_full_range_flag = br.bit();
            v.flags.colour_description_present_flag = br.bit();
            if (v.flags.colour_description_present_flag) {
                v.colour_primaries = (uint8_t)br.u(8);
                v.transfer_characteristics = (uint8_t)br.u(8);
                v.matrix_coeffs = (uint8_t)br.u(8);
            }
        }
        v.flags.chroma_loc_info_present_flag = br.bit();
        if (v.flags.chroma_loc_info_present_flag) {
            v.chroma_sample_loc_type_top_field = (uint8_t)br.ue();
            v.chroma_sample_loc_type_bottom_field = (uint8_t)br.ue();
        }
        v.flags.neutral_chroma_indication_flag = br.bit();
        v.flags.field_seq_flag = br.bit();
        v.flags.frame_field_info_present_flag = br.bit();
        v.flags.default_display_window_flag = br.bit();
        if (v.flags.default_display_window_flag) {
            v.def_disp_win_left_offset = (uint16_t)br.ue();
            v.def_disp_win_right_offset = (uint16_t)br.ue();
            v.def_disp_win_top_offset = (uint16_t)br.ue();
            v.def_disp_win_bottom_offset = (uint16_t)br.ue();
        }
        v.flags.vui_timing_info_present_flag = br.bit();
        if (v.flags.vui_timing_info_present_flag) {
            v.vui_num_units_in_tick = br.u(32);
            v.vui_time_scale = br.u(32);
            v.flags.vui_poc_proportional_to_timing_flag = br.bit();
            if (v.flags.vui_poc_proportional_to_timing_flag)
                v.vui_num_ticks_poc_diff_one_minus1 = br.ue();
            v.flags.vui_hrd_parameters_present_flag = br.bit();
            if (v.flags.vui_hrd_parameters_present_flag) parse_hrd(br, true, max_sub);
        }
        v.flags.bitstream_restriction_flag = br.bit();
        if (v.flags.bitstream_restriction_flag) {
            v.flags.tiles_fixed_structure_flag = br.bit();
            v.flags.motion_vectors_over_pic_boundaries_flag = br.bit();
            v.flags.restricted_ref_pic_lists_flag = br.bit();
            v.min_spatial_segmentation_idc = (uint16_t)br.ue();
            v.max_bytes_per_pic_denom = (uint8_t)br.ue();
            v.max_bits_per_min_cu_denom = (uint8_t)br.ue();
            v.log2_max_mv_length_horizontal = (uint8_t)br.ue();
            v.log2_max_mv_length_vertical = (uint8_t)br.ue();
        }
        if (v.flags.field_seq_flag) {
            error = "H.265 field-sequential (interlaced) streams are not supported";
            return false;
        }
        s.pSequenceParameterSetVui = &sps->vui;
    }

    if (br.overrun()) {
        error = "truncated H.265 SPS";
        return false;
    }
    s.pProfileTierLevel = &sps->ptl;
    s.pDecPicBufMgr = &sps->dpbm;

    sps->max_poc_lsb = 1u << (s.log2_max_pic_order_cnt_lsb_minus4 + 4);
    const int sub_w = (chroma == 1 || chroma == 2) ? 2 : 1;
    const int sub_h = (chroma == 1) ? 2 : 1;
    sps->width = (int)s.pic_width_in_luma_samples -
                 sub_w * (int)(s.conf_win_left_offset + s.conf_win_right_offset);
    sps->height = (int)s.pic_height_in_luma_samples -
                  sub_h * (int)(s.conf_win_top_offset + s.conf_win_bottom_offset);
    sps->max_dpb = (uint32_t)sps->dpbm.max_dec_pic_buffering_minus1[max_sub] + 1u;
    sps->max_reorder = sps->dpbm.max_num_reorder_pics[max_sub];
    sps->valid = true;

    sps_[id] = std::move(sps);
    updateFormat(*sps_[id]);
    have_sps_ = true;
    params_dirty_ = true;
    return true;
}

bool H265Decoder::parsePps(const uint8_t* nal, size_t size, std::string& error) {
    RbspReader br(nal, size, 2);
    auto pps = std::make_unique<Pps>();
    StdVideoH265PictureParameterSet& p = pps->std;

    const uint32_t id = br.ue();
    const uint32_t sps_id = br.ue();
    if (id >= 64 || sps_id >= 16) {
        error = "H.265 PPS/SPS id out of range";
        return false;
    }
    p.pps_pic_parameter_set_id = (uint8_t)id;
    p.pps_seq_parameter_set_id = (uint8_t)sps_id;
    if (sps_[sps_id]) p.sps_video_parameter_set_id = sps_[sps_id]->std.sps_video_parameter_set_id;

    p.flags.dependent_slice_segments_enabled_flag = br.bit();
    p.flags.output_flag_present_flag = br.bit();
    p.num_extra_slice_header_bits = (uint8_t)br.u(3);
    p.flags.sign_data_hiding_enabled_flag = br.bit();
    p.flags.cabac_init_present_flag = br.bit();
    p.num_ref_idx_l0_default_active_minus1 = (uint8_t)br.ue();
    p.num_ref_idx_l1_default_active_minus1 = (uint8_t)br.ue();
    p.init_qp_minus26 = (int8_t)br.se();
    p.flags.constrained_intra_pred_flag = br.bit();
    p.flags.transform_skip_enabled_flag = br.bit();
    p.flags.cu_qp_delta_enabled_flag = br.bit();
    if (p.flags.cu_qp_delta_enabled_flag) p.diff_cu_qp_delta_depth = (uint8_t)br.ue();
    p.pps_cb_qp_offset = (int8_t)br.se();
    p.pps_cr_qp_offset = (int8_t)br.se();
    p.flags.pps_slice_chroma_qp_offsets_present_flag = br.bit();
    p.flags.weighted_pred_flag = br.bit();
    p.flags.weighted_bipred_flag = br.bit();
    p.flags.transquant_bypass_enabled_flag = br.bit();
    p.flags.tiles_enabled_flag = br.bit();
    p.flags.entropy_coding_sync_enabled_flag = br.bit();
    if (p.flags.tiles_enabled_flag) {
        p.num_tile_columns_minus1 = (uint8_t)br.ue();
        p.num_tile_rows_minus1 = (uint8_t)br.ue();
        p.flags.uniform_spacing_flag = br.bit();
        if (!p.flags.uniform_spacing_flag) {
            for (int i = 0; i < p.num_tile_columns_minus1 &&
                            i < int(STD_VIDEO_H265_CHROMA_QP_OFFSET_TILE_COLS_LIST_SIZE); ++i)
                p.column_width_minus1[i] = (uint16_t)br.ue();
            for (int i = 0; i < p.num_tile_rows_minus1 &&
                            i < int(STD_VIDEO_H265_CHROMA_QP_OFFSET_TILE_ROWS_LIST_SIZE); ++i)
                p.row_height_minus1[i] = (uint16_t)br.ue();
        }
        p.flags.loop_filter_across_tiles_enabled_flag = br.bit();
    }
    p.flags.pps_loop_filter_across_slices_enabled_flag = br.bit();
    p.flags.deblocking_filter_control_present_flag = br.bit();
    if (p.flags.deblocking_filter_control_present_flag) {
        p.flags.deblocking_filter_override_enabled_flag = br.bit();
        p.flags.pps_deblocking_filter_disabled_flag = br.bit();
        if (!p.flags.pps_deblocking_filter_disabled_flag) {
            p.pps_beta_offset_div2 = (int8_t)br.se();
            p.pps_tc_offset_div2 = (int8_t)br.se();
        }
    }
    p.flags.pps_scaling_list_data_present_flag = br.bit();
    if (p.flags.pps_scaling_list_data_present_flag) {
        parse_scaling_list_data(br, pps->scaling);
        p.pScalingLists = &pps->scaling;
    }
    p.flags.lists_modification_present_flag = br.bit();
    p.log2_parallel_merge_level_minus2 = (uint8_t)br.ue();
    p.flags.slice_segment_header_extension_present_flag = br.bit();
    p.flags.pps_extension_present_flag = br.bit();
    if (p.flags.pps_extension_present_flag) {
        p.flags.pps_range_extension_flag = br.bit();
        const uint32_t other_ext = br.u(7);
        if (p.flags.pps_range_extension_flag) {
            if (p.flags.transform_skip_enabled_flag)
                p.log2_max_transform_skip_block_size_minus2 = (uint8_t)br.ue();
            p.flags.cross_component_prediction_enabled_flag = br.bit();
            p.flags.chroma_qp_offset_list_enabled_flag = br.bit();
            if (p.flags.chroma_qp_offset_list_enabled_flag) {
                p.diff_cu_chroma_qp_offset_depth = (uint8_t)br.ue();
                p.chroma_qp_offset_list_len_minus1 = (uint8_t)br.ue();
                for (int i = 0; i <= p.chroma_qp_offset_list_len_minus1 &&
                                i < int(STD_VIDEO_H265_CHROMA_QP_OFFSET_LIST_SIZE); ++i) {
                    p.cb_qp_offset_list[i] = (int8_t)br.se();
                    p.cr_qp_offset_list[i] = (int8_t)br.se();
                }
            }
            p.log2_sao_offset_scale_luma = (uint8_t)br.ue();
            p.log2_sao_offset_scale_chroma = (uint8_t)br.ue();
        }
        (void)other_ext;  // SCC and multilayer extensions are not parsed
    }
    if (br.overrun()) {
        error = "truncated H.265 PPS";
        return false;
    }
    pps->valid = true;
    pps_[id] = std::move(pps);
    params_dirty_ = true;
    return true;
}

void H265Decoder::updateFormat(const Sps& sps) {
    format_.width = sps.width;
    format_.height = sps.height;
    format_.coded_width = (int)sps.std.pic_width_in_luma_samples;
    format_.coded_height = (int)sps.std.pic_height_in_luma_samples;
    format_.bit_depth = 8 + sps.std.bit_depth_luma_minus8;
    format_.chroma_format = (int)sps.std.chroma_format_idc;
    format_.max_dpb_slots = std::min<uint32_t>(sps.max_dpb + 1, kMaxDpb);
    format_.max_active_references = std::min<uint32_t>(sps.max_dpb, 16);
    format_.max_reorder = sps.max_reorder;
    format_.full_range = sps.vui.flags.video_full_range_flag != 0;
    format_.matrix_coefficients =
        sps.vui.flags.colour_description_present_flag ? sps.vui.matrix_coeffs : 2;
    profile_.stdProfileIdc = sps.ptl.general_profile_idc;
    switch (sps.ptl.general_profile_idc) {
        case STD_VIDEO_H265_PROFILE_IDC_MAIN: format_.profile_name = "Main"; break;
        case STD_VIDEO_H265_PROFILE_IDC_MAIN_10: format_.profile_name = "Main10"; break;
        case STD_VIDEO_H265_PROFILE_IDC_MAIN_STILL_PICTURE:
            format_.profile_name = "MainStillPicture"; break;
        case STD_VIDEO_H265_PROFILE_IDC_FORMAT_RANGE_EXTENSIONS:
            format_.profile_name = "RExt"; break;
        default: format_.profile_name = "SCC"; break;
    }
}

// ---------------------------------------------------------------------------
// CodecDecoder
// ---------------------------------------------------------------------------

bool H265Decoder::parseNalArrays(const std::vector<uint8_t>& cfg, std::string& error) {
    if (cfg.size() < 23) return true;
    size_t p = 22;
    const int n_arrays = cfg[p++];
    for (int a = 0; a < n_arrays && p + 3 <= cfg.size(); ++a) {
        const int type = cfg[p] & 0x3f;
        ++p;
        const int n_nalus = (cfg[p] << 8) | cfg[p + 1];
        p += 2;
        for (int i = 0; i < n_nalus && p + 2 <= cfg.size(); ++i) {
            const size_t len = ((size_t)cfg[p] << 8) | cfg[p + 1];
            p += 2;
            if (p + len > cfg.size()) return true;
            bool ok = true;
            if (type == kVps) ok = parseVps(cfg.data() + p, len, error);
            else if (type == kSps) ok = parseSps(cfg.data() + p, len, error);
            else if (type == kPps) ok = parsePps(cfg.data() + p, len, error);
            if (!ok) return false;
            p += len;
        }
    }
    return true;
}

bool H265Decoder::init(const TrackInfo& track, std::string& error) {
    format_.width = track.width;
    format_.height = track.height;
    if (!parseNalArrays(track.codec_config, error)) return false;
    if (!have_sps_) {
        error = "H.265 track carries no SPS in its hvcC record; the video session "
                "cannot be sized without one";
        return false;
    }
    return true;
}

VkVideoSessionParametersKHR H265Decoder::createParameters(VkDevice device,
                                                           VkVideoSessionKHR session,
                                                           std::string& error) {
    std::vector<StdVideoH265VideoParameterSet>    vps_list;
    std::vector<StdVideoH265SequenceParameterSet> sps_list;
    std::vector<StdVideoH265PictureParameterSet>  pps_list;
    for (const auto& v : vps_) if (v && v->valid) vps_list.push_back(v->std);
    for (const auto& s : sps_) if (s && s->valid) sps_list.push_back(s->std);
    for (const auto& p : pps_) if (p && p->valid) pps_list.push_back(p->std);

    VkVideoDecodeH265SessionParametersAddInfoKHR add{
        VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_SESSION_PARAMETERS_ADD_INFO_KHR};
    add.stdVPSCount = (uint32_t)vps_list.size();
    add.pStdVPSs = vps_list.data();
    add.stdSPSCount = (uint32_t)sps_list.size();
    add.pStdSPSs = sps_list.data();
    add.stdPPSCount = (uint32_t)pps_list.size();
    add.pStdPPSs = pps_list.data();

    VkVideoDecodeH265SessionParametersCreateInfoKHR codec{
        VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_SESSION_PARAMETERS_CREATE_INFO_KHR};
    codec.maxStdVPSCount = 16;
    codec.maxStdSPSCount = 16;
    codec.maxStdPPSCount = 64;
    codec.pParametersAddInfo = &add;

    VkVideoSessionParametersCreateInfoKHR ci{
        VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_CREATE_INFO_KHR};
    ci.pNext = &codec;
    ci.videoSession = session;

    VkVideoSessionParametersKHR out = VK_NULL_HANDLE;
    if (video_api().createParameters(device, &ci, nullptr, &out) != VK_SUCCESS) {
        error = "vkCreateVideoSessionParametersKHR (H.265) failed";
        return VK_NULL_HANDLE;
    }
    params_dirty_ = false;
    return out;
}

void H265Decoder::flush() {
    for (auto& f : dpb_) f.used = false;
    prev_tid0_poc_lsb_ = prev_tid0_poc_msb_ = 0;
    no_rasl_output_ = true;
    seen_irap_ = false;
    pending_slot_ = -1;
}

int H265Decoder::allocSlot() {
    for (uint32_t i = 0; i < format_.max_dpb_slots; ++i)
        if (!dpb_[i].used) return (int)i;
    return -1;
}

bool H265Decoder::decodeFrame(const uint8_t* data, size_t size, int nal_length_size,
                              std::vector<uint8_t>& bitstream,
                              std::vector<uint32_t>& slice_offsets, PictureInfo& out,
                              std::string& error) {
    bitstream.clear();
    slice_offsets.clear();
    out = PictureInfo{};

    bool header_parsed = false;
    bool have_slice = false;
    int  nal_type = 0;
    int  temporal_id = 0;
    uint32_t pps_id = 0;
    uint32_t poc_lsb = 0;
    bool     short_term_from_sps = true;
    uint32_t st_rps_idx = 0;
    Rps      slice_rps{};
    uint32_t st_bits_in_slice = 0;
    int      num_delta_of_ref = 0;
    bool     pic_output = true;
    // Long-term entries, resolved to POCs once the current PicOrderCntVal is
    // known (delta_poc_msb_cycle_lt is relative to it).
    struct LtEntry { uint32_t lsb; bool used; bool msb_present; int msb_cycle; };
    std::vector<LtEntry> lt_entries;

    size_t p = 0;
    while (p < size) {
        const uint8_t* nal = nullptr;
        size_t nal_size = 0;
        if (nal_length_size > 0) {
            if (p + (size_t)nal_length_size > size) break;
            size_t len = 0;
            for (int i = 0; i < nal_length_size; ++i) len = (len << 8) | data[p + i];
            p += (size_t)nal_length_size;
            if (len == 0 || p + len > size) break;
            nal = data + p;
            nal_size = len;
            p += len;
        } else {
            while (p + 3 <= size && !(data[p] == 0 && data[p + 1] == 0 && data[p + 2] == 1)) ++p;
            if (p + 3 > size) break;
            size_t start = p + 3;
            size_t q = start;
            while (q + 3 <= size && !(data[q] == 0 && data[q + 1] == 0 && data[q + 2] <= 1)) ++q;
            if (q + 3 > size) q = size;
            nal = data + start;
            nal_size = q - start;
            p = q;
        }
        if (nal_size < 2) continue;

        const int type = (nal[0] >> 1) & 0x3f;
        const int tid = (nal[1] & 7) - 1;
        if (type == kVps) {
            if (!parseVps(nal, nal_size, error)) return false;
            continue;
        }
        if (type == kSps) {
            if (!parseSps(nal, nal_size, error)) return false;
            continue;
        }
        if (type == kPps) {
            if (!parsePps(nal, nal_size, error)) return false;
            continue;
        }
        if (type > 31) continue;  // SEI, AUD, filler

        have_slice = true;
        if (!header_parsed) {
            nal_type = type;
            temporal_id = tid;

            RbspReader br(nal, nal_size, 2);
            const bool first_slice = br.flag();
            {
                header_parsed = true;
                if (is_irap(type)) br.bit();  // no_output_of_prior_pics_flag
                (void)first_slice;
                pps_id = br.ue();
                if (pps_id >= 64 || !pps_[pps_id]) {
                    error = "H.265 slice references unknown PPS " + std::to_string(pps_id);
                    return false;
                }
                const Pps& pps = *pps_[pps_id];
                const uint32_t sps_id = pps.std.pps_seq_parameter_set_id;
                if (!sps_[sps_id]) {
                    error = "H.265 slice references unknown SPS " + std::to_string(sps_id);
                    return false;
                }
                const Sps& sps = *sps_[sps_id];

                for (int i = 0; i < pps.std.num_extra_slice_header_bits; ++i) br.bit();
                br.ue();  // slice_type
                if (pps.std.flags.output_flag_present_flag) pic_output = br.flag();
                if (sps.std.flags.separate_colour_plane_flag) br.u(2);

                if (!is_idr(type)) {
                    poc_lsb = br.u(sps.std.log2_max_pic_order_cnt_lsb_minus4 + 4);
                    short_term_from_sps = br.flag();
                    if (!short_term_from_sps) {
                        const size_t bit0 = br.bitPos();
                        if (!parse_st_rps(br, sps.std.num_short_term_ref_pic_sets,
                                          sps.std.num_short_term_ref_pic_sets, sps.rps,
                                          slice_rps, nullptr, &num_delta_of_ref)) {
                            error = "malformed short-term RPS in an H.265 slice header";
                            return false;
                        }
                        st_bits_in_slice = (uint32_t)(br.bitPos() - bit0);
                    } else {
                        st_rps_idx = 0;
                        if (sps.std.num_short_term_ref_pic_sets > 1)
                            st_rps_idx = br.u((int)ceil_log2(sps.std.num_short_term_ref_pic_sets));
                        if (st_rps_idx >= sps.rps.size()) {
                            error = "H.265 slice selects an out-of-range RPS index";
                            return false;
                        }
                        slice_rps = sps.rps[st_rps_idx];
                    }
                    if (sps.std.flags.long_term_ref_pics_present_flag) {
                        uint32_t num_lt_sps = 0;
                        if (sps.std.num_long_term_ref_pics_sps > 0) num_lt_sps = br.ue();
                        const uint32_t num_lt_pics = br.ue();
                        const int lsb_bits = sps.std.log2_max_pic_order_cnt_lsb_minus4 + 4;
                        int prev_msb_cycle = 0;
                        for (uint32_t i = 0; i < num_lt_sps + num_lt_pics && i < 32; ++i) {
                            LtEntry e{};
                            if (i < num_lt_sps) {
                                uint32_t idx = 0;
                                if (sps.std.num_long_term_ref_pics_sps > 1)
                                    idx = br.u((int)ceil_log2(
                                        sps.std.num_long_term_ref_pics_sps));
                                e.lsb = sps.lt.lt_ref_pic_poc_lsb_sps[idx];
                                e.used = ((sps.lt.used_by_curr_pic_lt_sps_flag >> idx) & 1u) != 0;
                            } else {
                                e.lsb = br.u(lsb_bits);
                                e.used = br.flag();
                            }
                            e.msb_present = br.flag();
                            if (e.msb_present) {
                                e.msb_cycle = (int)br.ue() +
                                              ((i == 0 || i == num_lt_sps) ? 0 : prev_msb_cycle);
                                prev_msb_cycle = e.msb_cycle;
                            }
                            lt_entries.push_back(e);
                        }
                    }
                }
                if (br.overrun()) {
                    error = "truncated H.265 slice header";
                    return false;
                }
            }
        }
        slice_offsets.push_back((uint32_t)bitstream.size());
        annexb_append(bitstream, nal, nal_size);
    }

    if (!have_slice || !header_parsed || slice_offsets.empty()) {
        error = "H.265 access unit contains no coded slice";
        return false;
    }
    const Pps& pps = *pps_[pps_id];
    const Sps& sps = *sps_[pps.std.pps_seq_parameter_set_id];

    // ---- picture order count (8.3.1) ----
    const bool idr = is_idr(nal_type);
    const bool irap = is_irap(nal_type);
    // NoRaslOutputFlag: 1 for IDR and BLA always, and for the first CRA in the
    // bitstream. Its RASL pictures reference frames that were never decoded.
    if (irap) {
        no_rasl_output_ = idr || nal_type <= 18 /* BLA_W_LP..BLA_N_LP */ || !seen_irap_;
        seen_irap_ = true;
    }
    int poc_msb = 0;
    if (irap && no_rasl_output_) {
        poc_msb = 0;
        if (idr) poc_lsb = 0;
    } else {
        const int max_lsb = (int)sps.max_poc_lsb;
        const int prev_lsb = prev_tid0_poc_lsb_;
        const int prev_msb = prev_tid0_poc_msb_;
        if ((int)poc_lsb < prev_lsb && (prev_lsb - (int)poc_lsb) >= max_lsb / 2)
            poc_msb = prev_msb + max_lsb;
        else if ((int)poc_lsb > prev_lsb && ((int)poc_lsb - prev_lsb) > max_lsb / 2)
            poc_msb = prev_msb - max_lsb;
        else
            poc_msb = prev_msb;
    }
    poc_ = poc_msb + (int)poc_lsb;
    // prevTid0Pic excludes RASL, RADL and sub-layer non-reference pictures (the
    // even VCL types up to 14).
    const bool sub_layer_non_ref = nal_type <= 14 && (nal_type % 2) == 0;
    if (temporal_id == 0 && !is_rasl(nal_type) && nal_type != 6 && nal_type != 7 &&
        !sub_layer_non_ref) {
        prev_tid0_poc_lsb_ = (int)poc_lsb;
        prev_tid0_poc_msb_ = poc_msb;
    }

    if (irap && no_rasl_output_) {
        for (auto& f : dpb_) f.used = false;
    }
    const bool drop_rasl = is_rasl(nal_type) && no_rasl_output_;

    // ---- reference picture set (8.3.2) ----
    int poc_st_before[kMaxRps], poc_st_after[kMaxRps];
    int n_before = 0, n_after = 0;
    std::vector<int> all_rps_poc;
    for (int i = 0; i < slice_rps.num_neg; ++i) {
        const int poc = poc_ + slice_rps.delta_poc_s0[i];
        all_rps_poc.push_back(poc);
        if (slice_rps.used_s0[i] && n_before < kMaxRps) poc_st_before[n_before++] = poc;
    }
    for (int i = 0; i < slice_rps.num_pos; ++i) {
        const int poc = poc_ + slice_rps.delta_poc_s1[i];
        all_rps_poc.push_back(poc);
        if (slice_rps.used_s1[i] && n_after < kMaxRps) poc_st_after[n_after++] = poc;
    }
    const size_t first_lt = all_rps_poc.size();
    std::vector<int> lt_curr_poc;
    for (const LtEntry& e : lt_entries) {
        int poc = (int)e.lsb;
        if (e.msb_present) {
            poc = poc_ - e.msb_cycle * (int)sps.max_poc_lsb - (int)poc_lsb + (int)e.lsb;
        } else {
            for (uint32_t i = 0; i < format_.max_dpb_slots; ++i)
                if (dpb_[i].used && (dpb_[i].poc & ((int)sps.max_poc_lsb - 1)) == poc) {
                    poc = dpb_[i].poc;
                    break;
                }
        }
        all_rps_poc.push_back(poc);
        if (e.used) lt_curr_poc.push_back(poc);
    }

    // RPS retention includes pictures that this frame does not use.
    for (uint32_t i = 0; i < format_.max_dpb_slots; ++i) {
        DpbFrame& f = dpb_[i];
        if (!f.used) continue;
        const bool short_term = std::find(all_rps_poc.begin(),
                                          all_rps_poc.begin() + first_lt, f.poc) !=
                                all_rps_poc.begin() + first_lt;
        const bool long_term = std::find(all_rps_poc.begin() + first_lt,
                                         all_rps_poc.end(), f.poc) != all_rps_poc.end();
        f.used = short_term || long_term;
        f.long_term = long_term;
    }

    auto slot_for_poc = [&](int poc) -> uint8_t {
        for (uint32_t i = 0; i < format_.max_dpb_slots; ++i)
            if (dpb_[i].used && dpb_[i].poc == poc) return (uint8_t)i;
        return STD_VIDEO_H265_NO_REFERENCE_PICTURE;
    };

    std_pic_ = StdVideoDecodeH265PictureInfo{};
    std_pic_.sps_video_parameter_set_id = sps.std.sps_video_parameter_set_id;
    std_pic_.pps_seq_parameter_set_id = (uint8_t)sps.std.sps_seq_parameter_set_id;
    std_pic_.pps_pic_parameter_set_id = (uint8_t)pps_id;
    std_pic_.PicOrderCntVal = poc_;
    std_pic_.flags.IrapPicFlag = irap ? 1 : 0;
    std_pic_.flags.IdrPicFlag = idr ? 1 : 0;
    std_pic_.flags.IsReference = 1;
    std_pic_.flags.short_term_ref_pic_set_sps_flag = short_term_from_sps ? 1 : 0;
    std_pic_.NumBitsForSTRefPicSetInSlice = (uint16_t)st_bits_in_slice;
    std_pic_.NumDeltaPocsOfRefRpsIdx = (uint8_t)num_delta_of_ref;
    for (int i = 0; i < int(STD_VIDEO_DECODE_H265_REF_PIC_SET_LIST_SIZE); ++i) {
        std_pic_.RefPicSetStCurrBefore[i] = STD_VIDEO_H265_NO_REFERENCE_PICTURE;
        std_pic_.RefPicSetStCurrAfter[i] = STD_VIDEO_H265_NO_REFERENCE_PICTURE;
        std_pic_.RefPicSetLtCurr[i] = STD_VIDEO_H265_NO_REFERENCE_PICTURE;
    }
    for (int i = 0; i < n_before && i < int(STD_VIDEO_DECODE_H265_REF_PIC_SET_LIST_SIZE); ++i)
        std_pic_.RefPicSetStCurrBefore[i] = slot_for_poc(poc_st_before[i]);
    for (int i = 0; i < n_after && i < int(STD_VIDEO_DECODE_H265_REF_PIC_SET_LIST_SIZE); ++i)
        std_pic_.RefPicSetStCurrAfter[i] = slot_for_poc(poc_st_after[i]);
    for (size_t i = 0; i < lt_curr_poc.size() && i < STD_VIDEO_DECODE_H265_REF_PIC_SET_LIST_SIZE;
         ++i)
        std_pic_.RefPicSetLtCurr[i] = slot_for_poc(lt_curr_poc[i]);

    out.refs.clear();
    for (uint32_t i = 0; i < format_.max_dpb_slots; ++i) {
        DpbFrame& f = dpb_[i];
        if (!f.used) continue;
        f.std_ref.PicOrderCntVal = f.poc;
        f.std_ref.flags.used_for_long_term_reference = f.long_term ? 1 : 0;
        out.refs.push_back({(int32_t)i, &f.std_ref});
    }
    if (out.refs.size() > format_.max_active_references) {
        error = "H.265 retained references exceed the video session's active-reference limit";
        return false;
    }

    vk_pic_ = VkVideoDecodeH265PictureInfoKHR{
        VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PICTURE_INFO_KHR};
    vk_pic_.pStdPictureInfo = &std_pic_;
    vk_pic_.sliceSegmentCount = (uint32_t)slice_offsets.size();
    vk_pic_.pSliceSegmentOffsets = slice_offsets.data();

    const int slot = allocSlot();
    if (slot < 0) {
        error = "H.265 reference picture set exceeds the video session's DPB slot limit";
        return false;
    }
    pending_slot_ = slot;
    pending_is_ref_ = true;

    setup_ref_ = StdVideoDecodeH265ReferenceInfo{};
    setup_ref_.PicOrderCntVal = poc_;

    out.decode_pnext = &vk_pic_;
    out.setup_slot = slot;
    out.setup_std_ref = &setup_ref_;
    out.poc = poc_;
    out.output = pic_output && !drop_rasl;
    out.params_changed = params_dirty_;
    out.sequence_restart = irap;
    return true;
}

void H265Decoder::commitFrame() {
    if (pending_slot_ < 0) return;
    DpbFrame& f = dpb_[(size_t)pending_slot_];
    f = DpbFrame{};
    f.used = pending_is_ref_;
    f.poc = poc_;
    f.std_ref.PicOrderCntVal = poc_;
    pending_slot_ = -1;
}

}  // namespace

std::unique_ptr<CodecDecoder> make_h265_decoder() { return std::make_unique<H265Decoder>(); }

}  // namespace video

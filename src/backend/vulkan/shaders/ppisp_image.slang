// PPISP image transform + regularization losses (Vulkan backend), mirroring
// the PPISP kernels in kernels/ppisp/Ppisp.cu. All device math comes from the
// canonical shaders/ppisp.slang (same functions the CUDA build calls through
// generated/ppisp.cuh), so forward values match up to fast-math rounding and
// gradients are the same autodiff.
//
// Spec constants (IDs by declaration order — dispatches must pass ALL):
//   0: kParamType — 0 = Original (36), 1 = RQS (39), 2 = NoCRF (24),
//                   3 = NoCRFNoVig (9)
//   1: kClampOutput — clamp the result to [0,1] (CRF-less layouts only)

#include "atomic_float.slang"

#define CudaDeviceExport ForceInline
#include "shaders/ppisp.slang"
#undef CudaDeviceExport

[SpecializationConstant]
const int kParamType = 0;

[SpecializationConstant]
const int kClampOutput = 0;

static const int kMaxParams = PPISP_RQS_NUM_PARAMS;                 // 39
static const int kMaxRawLosses = int(RawPPISPRegLossIndexRQS::length);  // 23
static const int kNumWeighted = int(PPISPRegLossIndex::length);     // 6

[ForceInline]
int _pp_num_params() {
    if (kParamType == 0) return PPISP_NUM_PARAMS;
    if (kParamType == 1) return PPISP_RQS_NUM_PARAMS;
    if (kParamType == 2) return PPISP_NO_CRF_NUM_PARAMS;
    return PPISP_NO_CRF_NO_VIG_NUM_PARAMS;
}

[ForceInline]
int _pp_num_raw_losses() {
    if (kParamType == 0) return int(RawPPISPRegLossIndex::length);
    if (kParamType == 1) return int(RawPPISPRegLossIndexRQS::length);
    if (kParamType == 2) return int(RawPPISPRegLossIndexNoCRF::length);
    return int(RawPPISPRegLossIndexNoCRFNoVig::length);
}

[ForceInline]
bool _pp_finite(float v) {
    return (asuint(v) & 0x7f800000u) != 0x7f800000u;
}

// ---------------------------------------------------------------------------
// ppisp_forward / ppisp_backward
// ---------------------------------------------------------------------------

struct PpispImageParams {
    float* in_image;      // [B,H,W,3]
    float* ppisp_params;  // [N_cam or B, P]
    float4* intrins;      // [B,4]
    int* cam_indices;     // [B] (has_cam_indices)
    float* out_image;     // fwd: out; bwd: v_out_image
    float* v_in_image;    // bwd
    float* v_ppisp_params;// bwd (pre-zeroed)
    int B, H, W;
    float actual_image_width;
    float actual_image_height;
    int has_cam_indices;
    uint wgs_per_row;  // pixel axis folds across (wg.x, wg.z); wg.y = batch
    int _pad0;
};

[ForceInline]
float3 _pp_apply(float3 pixel, float2 pix_coord, float2 image_center,
                 float2 img_size, float pbuf[kMaxParams]) {
    if (kParamType == 0) {
        float ps[PPISP_NUM_PARAMS];
        [ForceUnroll]
        for (int i = 0; i < PPISP_NUM_PARAMS; i++) ps[i] = pbuf[i];
        return apply_ppisp(pixel, pix_coord, image_center, img_size, ps);
    } else if (kParamType == 1) {
        float ps[PPISP_RQS_NUM_PARAMS];
        [ForceUnroll]
        for (int i = 0; i < PPISP_RQS_NUM_PARAMS; i++) ps[i] = pbuf[i];
        return apply_ppisp_rqs(pixel, pix_coord, image_center, img_size, ps);
    } else if (kParamType == 2) {
        float ps[PPISP_NO_CRF_NUM_PARAMS];
        [ForceUnroll]
        for (int i = 0; i < PPISP_NO_CRF_NUM_PARAMS; i++) ps[i] = pbuf[i];
        return apply_ppisp_no_crf(pixel, pix_coord, image_center, img_size, ps,
                                  kClampOutput != 0);
    } else {
        float ps[PPISP_NO_CRF_NO_VIG_NUM_PARAMS];
        [ForceUnroll]
        for (int i = 0; i < PPISP_NO_CRF_NO_VIG_NUM_PARAMS; i++) ps[i] = pbuf[i];
        return apply_ppisp_no_crf_no_vig(pixel, pix_coord, image_center,
                                         img_size, ps, kClampOutput != 0);
    }
}

[ForceInline]
void _pp_apply_vjp(float3 pixel, float2 pix_coord, float2 image_center,
                   float2 img_size, float pbuf[kMaxParams], float3 v_out,
                   out float3 v_pixel, out float vbuf[kMaxParams]) {
    [ForceUnroll]
    for (int i = 0; i < kMaxParams; i++) vbuf[i] = 0.0f;
    if (kParamType == 0) {
        float ps[PPISP_NUM_PARAMS];
        [ForceUnroll]
        for (int i = 0; i < PPISP_NUM_PARAMS; i++) ps[i] = pbuf[i];
        float vs[PPISP_NUM_PARAMS];
        apply_ppisp_vjp(pixel, pix_coord, image_center, img_size, ps, v_out,
                        v_pixel, vs);
        [ForceUnroll]
        for (int i = 0; i < PPISP_NUM_PARAMS; i++) vbuf[i] = vs[i];
    } else if (kParamType == 1) {
        float ps[PPISP_RQS_NUM_PARAMS];
        [ForceUnroll]
        for (int i = 0; i < PPISP_RQS_NUM_PARAMS; i++) ps[i] = pbuf[i];
        float vs[PPISP_RQS_NUM_PARAMS];
        apply_ppisp_rqs_vjp(pixel, pix_coord, image_center, img_size, ps,
                            v_out, v_pixel, vs);
        [ForceUnroll]
        for (int i = 0; i < PPISP_RQS_NUM_PARAMS; i++) vbuf[i] = vs[i];
    } else if (kParamType == 2) {
        float ps[PPISP_NO_CRF_NUM_PARAMS];
        [ForceUnroll]
        for (int i = 0; i < PPISP_NO_CRF_NUM_PARAMS; i++) ps[i] = pbuf[i];
        float vs[PPISP_NO_CRF_NUM_PARAMS];
        apply_ppisp_no_crf_vjp(pixel, pix_coord, image_center, img_size, ps,
                               kClampOutput != 0, v_out, v_pixel, vs);
        [ForceUnroll]
        for (int i = 0; i < PPISP_NO_CRF_NUM_PARAMS; i++) vbuf[i] = vs[i];
    } else {
        float ps[PPISP_NO_CRF_NO_VIG_NUM_PARAMS];
        [ForceUnroll]
        for (int i = 0; i < PPISP_NO_CRF_NO_VIG_NUM_PARAMS; i++) ps[i] = pbuf[i];
        float vs[PPISP_NO_CRF_NO_VIG_NUM_PARAMS];
        apply_ppisp_no_crf_no_vig_vjp(pixel, pix_coord, image_center, img_size,
                                      ps, kClampOutput != 0, v_out, v_pixel, vs);
        [ForceUnroll]
        for (int i = 0; i < PPISP_NO_CRF_NO_VIG_NUM_PARAMS; i++) vbuf[i] = vs[i];
    }
}

[shader("compute")]
[numthreads(256, 1, 1)]
void ppisp_image_fwd(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                     uniform PpispImageParams p) {
    uint gid = (wg.z * p.wgs_per_row + wg.x) * 256 + tid;
    uint bid = wg.y;
    if (bid >= uint(p.B) || gid >= uint(p.H * p.W)) return;
    uint y = gid / uint(p.W);
    uint x = gid % uint(p.W);

    int p_id = p.has_cam_indices != 0 ? p.cam_indices[bid] : int(bid);
    int np = _pp_num_params();
    float pbuf[kMaxParams];
    for (int i = 0; i < kMaxParams; i++)
        pbuf[i] = i < np ? p.ppisp_params[p_id * np + i] : 0.0f;

    uint pix = (bid * uint(p.H) + y) * uint(p.W) + x;
    float3 pixel = float3(p.in_image[3 * pix], p.in_image[3 * pix + 1],
                          p.in_image[3 * pix + 2]);
    float4 intr = p.intrins[bid];
    float3 out_pixel = _pp_apply(
        pixel, float2(float(x), float(y)), float2(intr.z, intr.w),
        float2(p.actual_image_width, p.actual_image_height), pbuf);

    p.out_image[3 * pix] = out_pixel.x;
    p.out_image[3 * pix + 1] = out_pixel.y;
    p.out_image[3 * pix + 2] = out_pixel.z;
}

groupshared float _pp_red64[64];

[shader("compute")]
[numthreads(64, 1, 1)]
void ppisp_image_bwd(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                     uniform PpispImageParams p) {
    uint gid = (wg.z * p.wgs_per_row + wg.x) * 64 + tid;
    uint bid = wg.y;
    bool inside = (bid < uint(p.B)) && (gid < uint(p.H * p.W));
    uint y = inside ? gid / uint(p.W) : 0u;
    uint x = inside ? gid % uint(p.W) : 0u;

    int p_id = (bid < uint(p.B))
                   ? (p.has_cam_indices != 0 ? p.cam_indices[bid] : int(bid))
                   : 0;
    int np = _pp_num_params();
    float pbuf[kMaxParams];
    for (int i = 0; i < kMaxParams; i++)
        pbuf[i] = i < np ? p.ppisp_params[p_id * np + i] : 0.0f;

    float vbuf[kMaxParams];
    [ForceUnroll]
    for (int i = 0; i < kMaxParams; i++) vbuf[i] = 0.0f;

    if (inside) {
        uint pix = (bid * uint(p.H) + y) * uint(p.W) + x;
        float3 v_out = float3(p.out_image[3 * pix], p.out_image[3 * pix + 1],
                              p.out_image[3 * pix + 2]);
        // Both gradients are linear in the incoming one, so a zero one -- a
        // masked pixel, mostly -- has no transform to differentiate. Not a
        // return: the block reduction below needs every thread.
        float3 v_pixel = float3(0.0f);
        if (any(v_out != float3(0.0f))) {
            float3 pixel = float3(p.in_image[3 * pix], p.in_image[3 * pix + 1],
                                  p.in_image[3 * pix + 2]);
            float4 intr = p.intrins[bid];
            _pp_apply_vjp(pixel, float2(float(x), float(y)),
                          float2(intr.z, intr.w),
                          float2(p.actual_image_width, p.actual_image_height),
                          pbuf, v_out, v_pixel, vbuf);
        }
        p.v_in_image[3 * pix] = v_pixel.x;
        p.v_in_image[3 * pix + 1] = v_pixel.y;
        p.v_in_image[3 * pix + 2] = v_pixel.z;
    }

    // Block-wide per-parameter reduce, one atomic per (block, param).
    for (int i = 0; i < np; i++) {
        float param = _pp_finite(vbuf[i]) ? vbuf[i] : 0.0f;
        GroupMemoryBarrierWithGroupSync();
        _pp_red64[tid] = param;
        GroupMemoryBarrierWithGroupSync();
        [ForceUnroll]
        for (uint s = 32; s > 0; s >>= 1) {
            if (tid < s) _pp_red64[tid] += _pp_red64[tid + s];
            GroupMemoryBarrierWithGroupSync();
        }
        if (tid == 0 && _pp_red64[0] != 0.0f)
            atomic_add_f32(&p.v_ppisp_params[p_id * np + i], _pp_red64[0]);
    }
}

// ---------------------------------------------------------------------------
// Regularization losses
// ---------------------------------------------------------------------------

struct PpispRegParams {
    float* ppisp_params;   // [B, P]
    float* raw_losses;     // [B+1, R] (fwd) / [R] tail row (final kernels)
    float* losses;         // [PPISPRegLossIndex::length]
    float* v_ppisp_params; // raw bwd out
    float loss_weights[kNumWeighted];
    int B;                 // number of images / num_train_images
    int _pad0, _pad1, _pad2;
};

[ForceInline]
void _pp_raw_losses(float pbuf[kMaxParams], out float lbuf[kMaxRawLosses]) {
    [ForceUnroll]
    for (int i = 0; i < kMaxRawLosses; i++) lbuf[i] = 0.0f;
    if (kParamType == 0) {
        float ps[PPISP_NUM_PARAMS];
        [ForceUnroll]
        for (int i = 0; i < PPISP_NUM_PARAMS; i++) ps[i] = pbuf[i];
        float ls[int(RawPPISPRegLossIndex::length)] =
            compute_raw_ppisp_regularization_loss(ps);
        [ForceUnroll]
        for (int i = 0; i < int(RawPPISPRegLossIndex::length); i++)
            lbuf[i] = ls[i];
    } else if (kParamType == 1) {
        float ps[PPISP_RQS_NUM_PARAMS];
        [ForceUnroll]
        for (int i = 0; i < PPISP_RQS_NUM_PARAMS; i++) ps[i] = pbuf[i];
        float ls[int(RawPPISPRegLossIndexRQS::length)] =
            compute_raw_ppisp_rqs_regularization_loss(ps);
        [ForceUnroll]
        for (int i = 0; i < int(RawPPISPRegLossIndexRQS::length); i++)
            lbuf[i] = ls[i];
    } else if (kParamType == 2) {
        float ps[PPISP_NO_CRF_NUM_PARAMS];
        [ForceUnroll]
        for (int i = 0; i < PPISP_NO_CRF_NUM_PARAMS; i++) ps[i] = pbuf[i];
        float ls[int(RawPPISPRegLossIndexNoCRF::length)] =
            compute_raw_ppisp_no_crf_regularization_loss(ps);
        [ForceUnroll]
        for (int i = 0; i < int(RawPPISPRegLossIndexNoCRF::length); i++)
            lbuf[i] = ls[i];
    } else {
        float ps[PPISP_NO_CRF_NO_VIG_NUM_PARAMS];
        [ForceUnroll]
        for (int i = 0; i < PPISP_NO_CRF_NO_VIG_NUM_PARAMS; i++) ps[i] = pbuf[i];
        float ls[int(RawPPISPRegLossIndexNoCRFNoVig::length)] =
            compute_raw_ppisp_no_crf_no_vig_regularization_loss(ps);
        [ForceUnroll]
        for (int i = 0; i < int(RawPPISPRegLossIndexNoCRFNoVig::length); i++)
            lbuf[i] = ls[i];
    }
}

groupshared float _pp_red32[32];

[shader("compute")]
[numthreads(32, 1, 1)]
void ppisp_reg_raw_fwd(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                       uniform PpispRegParams p) {
    uint bid = wg.x * 32 + tid;
    bool inside = bid < uint(p.B);

    int np = _pp_num_params();
    int nr = _pp_num_raw_losses();
    float lbuf[kMaxRawLosses];
    [ForceUnroll]
    for (int i = 0; i < kMaxRawLosses; i++) lbuf[i] = 0.0f;
    if (inside) {
        float pbuf[kMaxParams];
        for (int i = 0; i < kMaxParams; i++)
            pbuf[i] = i < np ? p.ppisp_params[bid * uint(np) + uint(i)] : 0.0f;
        _pp_raw_losses(pbuf, lbuf);
    }

    for (int i = 0; i < nr; i++) {
        float loss = (inside && _pp_finite(lbuf[i])) ? lbuf[i] : 0.0f;
        if (inside) p.raw_losses[bid * uint(nr) + uint(i)] = loss;
        GroupMemoryBarrierWithGroupSync();
        _pp_red32[tid] = loss;
        GroupMemoryBarrierWithGroupSync();
        [ForceUnroll]
        for (uint s = 16; s > 0; s >>= 1) {
            if (tid < s) _pp_red32[tid] += _pp_red32[tid + s];
            GroupMemoryBarrierWithGroupSync();
        }
        if (tid == 0 && _pp_red32[0] != 0.0f)
            atomic_add_f32(&p.raw_losses[uint(p.B) * uint(nr) + uint(i)],
                           _pp_red32[0]);
    }
}

// Single-thread weighted-loss kernels (CUDA launches them <<<1,1>>>).
// `raw_losses` points at the summed tail row; `losses` is the weighted out
// (fwd) or v_losses in (bwd, with v_ppisp_params standing in for the
// v_raw_losses out buffer).

[shader("compute")]
// One logical thread; 32-wide workgroup to satisfy the pinned subgroup size.
[numthreads(32, 1, 1)]
void ppisp_reg_final_fwd(uint tid: SV_GroupThreadID,
                         uniform PpispRegParams p) {
    if (tid != 0) return;
    int nr = _pp_num_raw_losses();
    float w[kNumWeighted];
    [ForceUnroll]
    for (int i = 0; i < kNumWeighted; i++) w[i] = p.loss_weights[i];

    float ls[kNumWeighted];
    if (kParamType == 0) {
        float raw[int(RawPPISPRegLossIndex::length)];
        [ForceUnroll]
        for (int i = 0; i < int(RawPPISPRegLossIndex::length); i++)
            raw[i] = p.raw_losses[i];
        float o[kNumWeighted] =
            compute_ppisp_regularization_loss(raw, p.B, w);
        [ForceUnroll]
        for (int i = 0; i < kNumWeighted; i++) ls[i] = o[i];
    } else if (kParamType == 1) {
        float raw[int(RawPPISPRegLossIndexRQS::length)];
        [ForceUnroll]
        for (int i = 0; i < int(RawPPISPRegLossIndexRQS::length); i++)
            raw[i] = p.raw_losses[i];
        float o[kNumWeighted] =
            compute_ppisp_rqs_regularization_loss(raw, p.B, w);
        [ForceUnroll]
        for (int i = 0; i < kNumWeighted; i++) ls[i] = o[i];
    } else if (kParamType == 2) {
        float raw[int(RawPPISPRegLossIndexNoCRF::length)];
        [ForceUnroll]
        for (int i = 0; i < int(RawPPISPRegLossIndexNoCRF::length); i++)
            raw[i] = p.raw_losses[i];
        float o[kNumWeighted] =
            compute_ppisp_no_crf_regularization_loss(raw, p.B, w);
        [ForceUnroll]
        for (int i = 0; i < kNumWeighted; i++) ls[i] = o[i];
    } else {
        float raw[int(RawPPISPRegLossIndexNoCRFNoVig::length)];
        [ForceUnroll]
        for (int i = 0; i < int(RawPPISPRegLossIndexNoCRFNoVig::length); i++)
            raw[i] = p.raw_losses[i];
        float o[kNumWeighted] =
            compute_ppisp_no_crf_no_vig_regularization_loss(raw, p.B, w);
        [ForceUnroll]
        for (int i = 0; i < kNumWeighted; i++) ls[i] = o[i];
    }
    [ForceUnroll]
    for (int i = 0; i < kNumWeighted; i++) p.losses[i] = ls[i];
}

[shader("compute")]
// One logical thread; 32-wide workgroup to satisfy the pinned subgroup size.
[numthreads(32, 1, 1)]
void ppisp_reg_final_bwd(uint tid: SV_GroupThreadID,
                         uniform PpispRegParams p) {
    if (tid != 0) return;
    // p.raw_losses: summed tail row; p.losses: v_losses (in);
    // p.v_ppisp_params: v_raw_losses (out).
    float w[kNumWeighted];
    float vl[kNumWeighted];
    [ForceUnroll]
    for (int i = 0; i < kNumWeighted; i++) {
        w[i] = p.loss_weights[i];
        vl[i] = p.losses[i];
    }

    if (kParamType == 0) {
        float raw[int(RawPPISPRegLossIndex::length)];
        [ForceUnroll]
        for (int i = 0; i < int(RawPPISPRegLossIndex::length); i++)
            raw[i] = p.raw_losses[i];
        float o[int(RawPPISPRegLossIndex::length)] =
            compute_ppisp_regularization_loss_vjp(raw, p.B, w, vl);
        [ForceUnroll]
        for (int i = 0; i < int(RawPPISPRegLossIndex::length); i++)
            p.v_ppisp_params[i] = o[i];
    } else if (kParamType == 1) {
        float raw[int(RawPPISPRegLossIndexRQS::length)];
        [ForceUnroll]
        for (int i = 0; i < int(RawPPISPRegLossIndexRQS::length); i++)
            raw[i] = p.raw_losses[i];
        float o[int(RawPPISPRegLossIndexRQS::length)] =
            compute_ppisp_rqs_regularization_loss_vjp(raw, p.B, w, vl);
        [ForceUnroll]
        for (int i = 0; i < int(RawPPISPRegLossIndexRQS::length); i++)
            p.v_ppisp_params[i] = o[i];
    } else if (kParamType == 2) {
        float raw[int(RawPPISPRegLossIndexNoCRF::length)];
        [ForceUnroll]
        for (int i = 0; i < int(RawPPISPRegLossIndexNoCRF::length); i++)
            raw[i] = p.raw_losses[i];
        float o[int(RawPPISPRegLossIndexNoCRF::length)] =
            compute_ppisp_no_crf_regularization_loss_vjp(raw, p.B, w, vl);
        [ForceUnroll]
        for (int i = 0; i < int(RawPPISPRegLossIndexNoCRF::length); i++)
            p.v_ppisp_params[i] = o[i];
    } else {
        float raw[int(RawPPISPRegLossIndexNoCRFNoVig::length)];
        [ForceUnroll]
        for (int i = 0; i < int(RawPPISPRegLossIndexNoCRFNoVig::length); i++)
            raw[i] = p.raw_losses[i];
        float o[int(RawPPISPRegLossIndexNoCRFNoVig::length)] =
            compute_ppisp_no_crf_no_vig_regularization_loss_vjp(raw, p.B, w, vl);
        [ForceUnroll]
        for (int i = 0; i < int(RawPPISPRegLossIndexNoCRFNoVig::length); i++)
            p.v_ppisp_params[i] = o[i];
    }
}

[shader("compute")]
[numthreads(32, 1, 1)]
void ppisp_reg_raw_bwd(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                       uniform PpispRegParams p) {
    // p.raw_losses: v_raw_losses (in, [R]); p.v_ppisp_params: out [B, P].
    uint bid = wg.x * 32 + tid;
    if (bid >= uint(p.B)) return;

    int np = _pp_num_params();
    float pbuf[kMaxParams];
    for (int i = 0; i < kMaxParams; i++)
        pbuf[i] = i < np ? p.ppisp_params[bid * uint(np) + uint(i)] : 0.0f;

    float vbuf[kMaxParams];
    [ForceUnroll]
    for (int i = 0; i < kMaxParams; i++) vbuf[i] = 0.0f;

    if (kParamType == 0) {
        float ps[PPISP_NUM_PARAMS];
        [ForceUnroll]
        for (int i = 0; i < PPISP_NUM_PARAMS; i++) ps[i] = pbuf[i];
        float vr[int(RawPPISPRegLossIndex::length)];
        [ForceUnroll]
        for (int i = 0; i < int(RawPPISPRegLossIndex::length); i++)
            vr[i] = p.raw_losses[i];
        float o[PPISP_NUM_PARAMS] =
            compute_raw_ppisp_regularization_loss_vjp(ps, vr);
        [ForceUnroll]
        for (int i = 0; i < PPISP_NUM_PARAMS; i++) vbuf[i] = o[i];
    } else if (kParamType == 1) {
        float ps[PPISP_RQS_NUM_PARAMS];
        [ForceUnroll]
        for (int i = 0; i < PPISP_RQS_NUM_PARAMS; i++) ps[i] = pbuf[i];
        float vr[int(RawPPISPRegLossIndexRQS::length)];
        [ForceUnroll]
        for (int i = 0; i < int(RawPPISPRegLossIndexRQS::length); i++)
            vr[i] = p.raw_losses[i];
        float o[PPISP_RQS_NUM_PARAMS] =
            compute_raw_ppisp_rqs_regularization_loss_vjp(ps, vr);
        [ForceUnroll]
        for (int i = 0; i < PPISP_RQS_NUM_PARAMS; i++) vbuf[i] = o[i];
    } else if (kParamType == 2) {
        float ps[PPISP_NO_CRF_NUM_PARAMS];
        [ForceUnroll]
        for (int i = 0; i < PPISP_NO_CRF_NUM_PARAMS; i++) ps[i] = pbuf[i];
        float vr[int(RawPPISPRegLossIndexNoCRF::length)];
        [ForceUnroll]
        for (int i = 0; i < int(RawPPISPRegLossIndexNoCRF::length); i++)
            vr[i] = p.raw_losses[i];
        float o[PPISP_NO_CRF_NUM_PARAMS] =
            compute_raw_ppisp_no_crf_regularization_loss_vjp(ps, vr);
        [ForceUnroll]
        for (int i = 0; i < PPISP_NO_CRF_NUM_PARAMS; i++) vbuf[i] = o[i];
    } else {
        float ps[PPISP_NO_CRF_NO_VIG_NUM_PARAMS];
        [ForceUnroll]
        for (int i = 0; i < PPISP_NO_CRF_NO_VIG_NUM_PARAMS; i++) ps[i] = pbuf[i];
        float vr[int(RawPPISPRegLossIndexNoCRFNoVig::length)];
        [ForceUnroll]
        for (int i = 0; i < int(RawPPISPRegLossIndexNoCRFNoVig::length); i++)
            vr[i] = p.raw_losses[i];
        float o[PPISP_NO_CRF_NO_VIG_NUM_PARAMS] =
            compute_raw_ppisp_no_crf_no_vig_regularization_loss_vjp(ps, vr);
        [ForceUnroll]
        for (int i = 0; i < PPISP_NO_CRF_NO_VIG_NUM_PARAMS; i++) vbuf[i] = o[i];
    }

    for (int i = 0; i < np; i++) {
        float v = _pp_finite(vbuf[i]) ? vbuf[i] : 0.0f;
        p.v_ppisp_params[bid * uint(np) + uint(i)] = v;
    }
}

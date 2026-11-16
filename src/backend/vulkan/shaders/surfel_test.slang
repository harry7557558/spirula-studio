// The 2DGS surfel evaluation in shaders/projection_utils.slang, one case per
// thread, for backend/vulkan/tests/surfel_grad_test.cpp. Record layouts must
// match the kIn* / kOut* offsets there.

#define CudaDeviceExport ForceInline
#include "shaders/projection_utils.slang"
#undef CudaDeviceExport

static const uint IN_STRIDE = 32;
static const uint OUT_STRIDE = 32;

struct SurfelTestParams {
    float* in;
    float* out;
    uint32_t n;
};

float3 ld3(float* p, uint i) { return float3(p[i], p[i + 1], p[i + 2]); }
void st3(float* p, uint i, float3 v) { p[i] = v.x; p[i + 1] = v.y; p[i + 2] = v.z; }

[shader("compute")]
[numthreads(64, 1, 1)]
void surfel_eval(uint3 tid: SV_DispatchThreadID, uniform SurfelTestParams p) {
    uint k = tid.x;
    if (k >= p.n) return;
    float* in = p.in + k * IN_STRIDE;
    float* out = p.out + k * OUT_STRIDE;

    float3 mean = ld3(in, 0);
    float4 quat = float4(in[3], in[4], in[5], in[6]);
    float3 scale = ld3(in, 7);
    float opacity = in[10];
    float3 rgb = ld3(in, 11);
    float3 ray_o = ld3(in, 14);
    float3 ray_d = ld3(in, 17);
    float2 pix = float2(in[20], in[21]);
    float v_alpha = in[22];
    float3 v_rgb_in = ld3(in, 23);
    float v_depth = in[26];
    float3 v_normal = ld3(in, 27);

    float3x3 M = compute_2dgs_iscl_rot(quat, scale);
    float alpha = evaluate_alpha_2dgs(mean, M, opacity, ray_o, ray_d, pix);
    float3 c; float depth; float3 normal;
    evaluate_color_2dgs(mean, M, opacity, rgb, ray_o, ray_d, pix, c, depth, normal);

    float3 vm_a, vro_a, vrd_a; float3x3 vM_a; float vo_a;
    evaluate_alpha_2dgs_vjp(mean, M, opacity, ray_o, ray_d, pix, v_alpha,
                            vm_a, vM_a, vo_a, vro_a, vrd_a);
    float3 vm_c, vro_c, vrd_c, v_rgb; float3x3 vM_c; float vo_c;
    evaluate_color_2dgs_vjp(mean, M, opacity, rgb, ray_o, ray_d, pix,
                            v_rgb_in, v_depth, v_normal,
                            vm_c, vM_c, vo_c, v_rgb, vro_c, vrd_c);
    float4 v_quat; float3 v_scale;
    compute_2dgs_iscl_rot_vjp(quat, scale, vM_a + vM_c, v_quat, v_scale);

    out[0] = alpha;
    st3(out, 1, c);
    out[4] = depth;
    st3(out, 5, normal);
    st3(out, 8, vm_a + vm_c);
    out[11] = v_quat.x; out[12] = v_quat.y; out[13] = v_quat.z; out[14] = v_quat.w;
    st3(out, 15, v_scale);
    out[18] = vo_a + vo_c;
    st3(out, 19, v_rgb);
    st3(out, 22, vro_a + vro_c);
    st3(out, 25, vrd_a + vrd_c);
}

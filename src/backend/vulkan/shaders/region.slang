// Vulkan entry points for the region test (kernels/densify/RegionWeight.cu);
// the device math is the shared shaders/region.slang.

#define CudaDeviceExport ForceInline
#include "shaders/region.slang"
#undef CudaDeviceExport

struct RegionWeightParams {  // 96 bytes: pushed directly
    float* means;          // [N, 3]
    float* quats;          // [N, 4]  (use_normal gates)
    float* scales;         // [N, 3]
    float4* camera_bvh;
    float4* camera_seeds;
    float4* program;
    float4* field_bvh;
    float4* field_seeds;
    float* weight;         // [N] out
    float inside, outside;
    uint num_camera_nodes, num_prog, num_field_nodes, use_normal;
    uint num_splats, wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void region_weight(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                   uniform RegionWeightParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.num_splats) return;
    float3 pos = float3(p.means[3 * idx], p.means[3 * idx + 1], p.means[3 * idx + 2]);
    float3 n = float3(0.0);
    bool has_n = false;
    if (p.use_normal != 0 && p.num_camera_nodes > 0) {
        uint cam = label_field_nearest(pos, n, false, p.camera_bvh, p.camera_seeds,
                                       p.num_camera_nodes);
        if (cam != 255u) {
            float4 c = p.camera_seeds[cam];
            float4 q = float4(p.quats[4 * idx], p.quats[4 * idx + 1], p.quats[4 * idx + 2],
                              p.quats[4 * idx + 3]);
            float3 s = float3(p.scales[3 * idx], p.scales[3 * idx + 1], p.scales[3 * idx + 2]);
            n = splat_normal(q, s, pos, c.xyz);
            has_n = true;
        }
    }
    bool inside = region_contains(pos, n, has_n, p.program, p.num_prog, p.field_bvh,
                                  p.field_seeds, p.num_field_nodes);
    p.weight[idx] = inside ? p.inside : p.outside;
}

struct ScaleScoreParams {  // 32 bytes: pushed directly
    float* weight;
    float2* score;
    uint num_splats, wgs_per_row;
    uint _pad0, _pad1;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void densify_scale_score(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                         uniform ScaleScoreParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.num_splats) return;
    p.score[idx].x *= p.weight[idx];
}

struct DecayOpacityParams {  // 32 bytes: pushed directly
    float* weight;
    float* opac;
    uint num_splats, wgs_per_row;
    float factor;
    uint _pad0;
};

// Opacity is a logit: sigmoid(o) * factor, back to a logit.
[shader("compute")]
[numthreads(256, 1, 1)]
void decay_opacity(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                   uniform DecayOpacityParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.num_splats || p.weight[idx] >= 1.0) return;
    float q = p.factor / (1.0 + exp(-p.opac[idx]));
    p.opac[idx] = log(q / (1.0 - q));
}

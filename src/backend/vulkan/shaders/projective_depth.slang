#include "shaders/projective_depth.slang"

struct ProjectiveObserveParams {
    int* camera_ids;
    int* gaussian_ids;
    float* gradients;
    int* camera_map;
    float4* centers;
    float3* means;
    float3* scales;
    float4* quats;
    uint* states;
    uint* information;
    uint count, n, packed, stride, offset, wgs_per_row, single_camera, pad;
};

[shader("compute")]
[numthreads(256,1,1)]
void observe(uint3 group: SV_GroupID, uint lane: SV_GroupThreadID, uniform ProjectiveObserveParams p) {
    uint i = (group.y*p.wgs_per_row+group.x)*256+lane;
    if (i >= p.count || p.gradients[i*p.stride+p.offset] == 0) return;
    uint camera = p.packed != 0 ? p.camera_ids[i] : i/p.n;
    uint gaussian = p.packed != 0 ? p.gaussian_ids[i] : i%p.n;
    uint id;
    if (p.single_camera != 0) {
        id = p.states[4*gaussian];
        if (id == 0) p.states[4*gaussian] = asuint(float(p.camera_map[camera]+1));
    } else InterlockedCompareExchange(p.states[4*gaussian],0,asuint(float(p.camera_map[camera]+1)),id);
    if (id == 0) return;
    float information = projective_depth_information(
        p.centers[int(asfloat(id))-1].xyz,p.centers[p.camera_map[camera]].xyz,
        p.means[gaussian],p.scales[gaussian],p.quats[gaussian]);
    if (!(information > 0) || !isfinite(information)) return;
    if (p.single_camera != 0) {
        float previous = asfloat(p.information[gaussian]);
        p.information[gaussian] = asuint(previous > 0 ? .9f*previous+.1f*information : information);
        return;
    }
    uint old;
    InterlockedCompareExchange(p.information[gaussian],0,0,old);
    for (;;) {
        float previous = asfloat(old);
        float next = previous > 0 ? .9f*previous+.1f*information : information;
        uint observed;
        InterlockedCompareExchange(p.information[gaussian],old,asuint(next),observed);
        if (observed == old) break;
        old = observed;
    }
}

// Trivial kernels exercising the Vulkan pipeline layer end to end
// (device addresses in push constants, specialization constants, dispatch +
// barrier ordering). Used by backend/vulkan/tests/vk_pipeline_smoke.cpp and
// kept as the smallest reproducer when debugging the dispatch path.

// Spec constant 0: integer scale applied on top of the uniform `a`.
[SpecializationConstant]
const int kScale = 1;

struct SaxpyParams {
    float* x;
    float* y;
    float a;
    uint32_t n;
};

// y[i] = kScale * (a * x[i]) + y[i]
[shader("compute")]
[numthreads(256, 1, 1)]
void saxpy(uint3 tid: SV_DispatchThreadID, uniform SaxpyParams p) {
    uint i = tid.x;
    if (i >= p.n) return;
    p.y[i] = float(kScale) * (p.a * p.x[i]) + p.y[i];
}

struct IotaParams {
    uint32_t* out;
    uint32_t n;
    uint32_t offset;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void iota_u32(uint3 tid: SV_DispatchThreadID, uniform IotaParams p) {
    uint i = tid.x;
    if (i >= p.n) return;
    p.out[i] = i + p.offset;
}

struct DispatchGridParams {
    uint32_t* out;
    uint32_t nx;
    uint32_t ny;
};

[shader("compute")]
[numthreads(4, 1, 1)]
void dispatch_grid(uint3 wg: SV_GroupID, uint3 tid: SV_DispatchThreadID,
                   uint lane: SV_GroupThreadID, uniform DispatchGridParams p) {
    uint i = ((wg.z * p.ny + wg.y) * p.nx + wg.x) * 4 + lane;
    p.out[i] = 1 + tid.x + 1024 * tid.y + 1048576 * tid.z;
}

[shader("compute")]
[numthreads(4, 1, 1)]
void dispatch_group_only(uint3 wg: SV_GroupID, uint lane: SV_GroupThreadID,
                         uniform DispatchGridParams p) {
    uint i = ((wg.z * p.ny + wg.y) * p.nx + wg.x) * 4 + lane;
    p.out[i] = 1 + wg.x * 4 + lane + 1024 * wg.y + 1048576 * wg.z;
}

// Preserve the original binary-tree sum while finishing inside subgroups.
groupshared float _ppr_scratch[kPpispReduceThreads];

[ForceInline]
float ppisp_reduce_sum(float value, uint tid) {
    GroupMemoryBarrierWithGroupSync();
    _ppr_scratch[tid] = value;
    GroupMemoryBarrierWithGroupSync();
    const uint width = WaveGetLaneCount();
    for (uint s = kPpispReduceThreads / 2; s >= width; s >>= 1) {
        if (tid < s) _ppr_scratch[tid] += _ppr_scratch[tid + s];
        GroupMemoryBarrierWithGroupSync();
    }
    const uint lane = WaveGetLaneIndex();
    float sum = _ppr_scratch[lane];
    for (uint s = min(kPpispReduceThreads / 2, width / 2); s > 0; s >>= 1) {
        float other = WaveReadLaneAt(sum, min(lane + s, width - 1));
        if (lane < s) sum += other;
    }
    return sum;
}

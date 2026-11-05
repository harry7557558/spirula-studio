// Caller must bind no render-depth, distortion or median-depth gradients.
#define SS_RASTER_NODEPTH 1
#include "rasterize_bwd.slang"

[shader("compute")]
[numthreads(32, 1, 1)]
void rasterize_bwd_3dgut_nodepth(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                                uniform Raster3dgutBwdParams* pp) {
    rasterize_bwd_3dgut_body(wg, tid, pp);
}

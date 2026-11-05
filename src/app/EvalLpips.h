#pragma once
// TrainerCore's LpipsHooks (app/TrainerCore.h) on src/lpips/: AlexNet and VGG,
// with the same normalize pairing as reference/python/eval_lpips.py.

namespace spirula {

// Once, at startup, in builds with the inference layer (SS_HAVE_LPIPS).
void install_eval_lpips();

}  // namespace spirula

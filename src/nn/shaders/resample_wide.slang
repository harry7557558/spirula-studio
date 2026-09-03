// resample.slang's kernels with 64-bit buffer addressing, for tensors past 4 GiB.
// Source lives entirely in resample.slang; NN_WIDE only switches the addressing
// shim in _common.slang. This is the copy that declares OpCapability Int64,
// which is why it is a separate module -- see src/nn/README.md rule 7.
#define NN_WIDE 1
#include "resample.slang"

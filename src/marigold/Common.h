#pragma once
// The names marigold/ borrows from the inference layer (src/nn/), as
// moge/Common.h does. Nothing model-specific belongs here.

#include "nn/core/Error.h"
#include "nn/core/Half.h"
#include "nn/core/Log.h"
#include "nn/core/Parallel.h"

namespace nn { namespace vk {} }

namespace marigold {

namespace vk = ::nn::vk;
using ::nn::Error;
using ::nn::fail;
using ::nn::now_ms;
using ::nn::parallel_for;

}  // namespace marigold

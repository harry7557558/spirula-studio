# src/vk — the Vulkan seam

Everything device-facing lives here. Above this line (`src/nn` and up) nothing
mentions a Vulkan handle.

## Device baseline

**Vulkan 1.2 core**, plus exactly two core-1.2 features:

- `bufferDeviceAddress` — every buffer is addressed as a 64-bit pointer, so
  Slang kernels take raw `T*` parameters and a launcher is a plain function of
  `Tensor`s instead of a descriptor-binding ritual.
- `timelineSemaphore` — one queue-level timeline orders every submission.

Nothing else is required. In particular:

| not required | why it would have been, and what we do instead |
|---|---|
| `shaderFloat16` / 16-bit storage | fp16 weights are read through a `uint*` as packed half pairs and unpacked with GLSL.std.450 `UnpackHalf2x16`, which needs only `Shader`. **Slang's `f16tof32` does NOT compile to this** — it emits `OpBitcast %half` + `OpFConvert` and drags `OpCapability Float16`/`Int16` in. `_common.slang` drops to inline SPIR-V for exactly this reason; check `spirv-dis <blob> \| grep OpCapability` after touching it. |
| `shaderInt64` | index arithmetic is u32 throughout. The largest tensor in a SAM 3 forward pass is 288×288×256 = 21 M elements, comfortably inside 2^31. |
| 8-bit storage | u8 masks are written as whole u32 words (four pixels per thread); allocations are 16-byte rounded so the tail word stays in bounds. |
| float atomics | inference has no scatter-accumulate. |

Optional and probed: `VK_EXT_subgroup_size_control` (pins the subgroup width —
Intel/ANV defaults to a *varying* SIMD width, which silently breaks any
`tid / WaveGetLaneCount()` indexing), and the `VK_KHR_video_decode_*` family.

## Memory

Two policies over one raw allocator (`Allocator`: one `VkBuffer` + dedicated
`VkDeviceMemory` per allocation, handed out as a `VkDeviceAddress`, with an
address-range map for copy/fill resolution).

- **`VramPool`** — persistent, key-addressed, grow-only. Weights, PE tables,
  the frame's backbone features, memory-bank slots. Keys are a `PoolSlot` enum
  plus an integer sub-index, so the categorized VRAM report stays readable.
- **`Arena`** — a bump allocator for activations, with `Mark`/`rewind` and an
  RAII `ArenaScope`. Every module forward opens one, so peak VRAM is the
  deepest stage's live set rather than the sum of stages.

**Growing an arena mid-scope is illegal** and asserts: it would invalidate every
outstanding pointer. `reserve()` up front instead.

## The rule that cost a day

`Allocator::free` must **drain the stream**, not just `vkDeviceWaitIdle`.

Commands are recorded into an open command buffer and submitted lazily. A
`vkDeviceWaitIdle` waits for *submitted* work and returns happily with the open
buffer still holding commands that reference the buffer about to be destroyed.
The next submit then fails with `VK_ERROR_DEVICE_LOST`, thousands of dispatches
away from the free that caused it, and it only reproduces when batching is on —
`SS_NN_DEBUG_SYNC=1` submits every dispatch separately and makes the bug vanish.

`set_drain_hook` installs `Stream::sync` for this. The corollary for callers:
**freeing a pool slot on a per-frame path stalls the pipeline**, so the tracker
recycles memory-bank slot indices and keeps the buffers.

The rule generalizes past `Allocator`: anything destroying a Vulkan object the
stream may have recorded against must `Stream::sync()` first, not merely
`vkDeviceWaitIdle`. `VideoPipeline::Impl::~Impl` does exactly this — a caller
that queues a sharpness metric and releases the frame without fetching it
leaves a plane copy in the open command buffer, and destroying those images
under it segfaults in the driver at the next flush.

## Streams and submission

One compute queue. Dispatches and copies accumulate into one command buffer
from a 4-deep ring; a flush ends it and signals the timeline. Between every
recorded command sits a conservative global `COMPUTE|TRANSFER →
COMPUTE|TRANSFER` memory barrier, which reproduces straight-line data
dependence exactly. Per-resource narrowing is a later optimization, to be
applied with a profile.

Flush points: `sync()`, `download()`, a staging- or params-ring wrap, and the
command-buffer ring running dry. Everything else stays in flight — an image
encode issues a few thousand dispatches, and submitting each one separately
costs more than the kernels do.

`waitOn(semaphore, value)` attaches an external timeline wait to the *next*
submission (it flushes first, so work already recorded is not gated by it).
That is how the video decoder, which runs on the video-decode queue family,
hands a decoded picture over: it signals its own timeline, and the plane copy
recorded here waits on that value. `commandBuffer()` and `barrierNow()` are the
matching escape hatch for the few commands this class does not wrap — image
layout transitions and image↔buffer copies in `src/video`.

## Pipelines

One pipeline layout in the process: a push-constant range and no descriptor
sets. A kernel is `"<module>.<entry>"` plus its specialization-constant tuple;
pipelines are created lazily and cached.

**Specialization-constant ids are assigned in declaration order per module**,
not per entry point. A dispatch that only cares about a later axis must still
supply the earlier ones — omitting them silently leaves them at their defaults.
That is why the launchers always pass a module's full `SpecList`.

## Debugging

| | |
|---|---|
| `SS_VK_VALIDATION=1` | Khronos validation layers |
| `SS_NN_DEBUG_SYNC=1` | print entry + grid before each dispatch, sync and report after — bisects a device-lost failure to one kernel |
| `SS_PROFILE=1` | timestamp queries; `Session::printProfile()` prints per-entry GPU time. Note it adds a query pair per dispatch, which at SAM 3's dispatch counts inflates wall time by ~35% — use it for *relative* attribution, and `ssam-cli track`'s own ms/frame line for absolute numbers |
| `test_ops --bench` | GEMM and attention at the exact shapes SAM 3 runs them at, in TFLOP/s — the loop for tuning a kernel without a 30-second end-to-end run |
| `SS_VK_DEVICE=<i\|name\|auto\|uuid:hex>` | native device selection; an explicit `--device` wins |

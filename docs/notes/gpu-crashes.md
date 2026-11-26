# Crashes that are not ours: Vulkan layers and lost devices

Two kinds of report look like a Spirula crash but start outside it: a
third-party Vulkan layer misbehaving inside our process, and a GPU that stops
(`VK_ERROR_DEVICE_LOST`) while it decodes a video. This note covers what the
program does about each and what the crash report says.

## What crash.log records

`app/CrashLog.cpp` writes a stack of `module+RVA` frames. It also writes the
lines that make a report from another machine readable without a reply:

| line | written by | holds |
| --- | --- | --- |
| `vulkan:` | `app/VulkanLayers.cpp` at startup | `vulkan-1.dll`'s file version, and any loader variable that changes which layers or drivers load. Path variables are only said to be set, because their values name user folders. |
| `vk layers:` | the same | each implicit layer registered under `SOFTWARE\Khronos\Vulkan\ImplicitLayers` (HKLM, then HKCU) and turned on there, with its DLL, and `(off)` when it is disabled |
| `gpu (inference)`, `gpu (training)`, `gpu (sfm)` | each Vulkan runtime when it picks its device | model, driver name and version as the driver reports them, Vulkan version |
| `modules:` | the handler, at crash time (Windows) | file names of loaded DLLs outside the Windows folder: overlays, hooks, capture layers, antivirus |

The notes live in `core/CrashNotes.h`. It is header-only because nn, sfm and
the training backend do not link `app/`. The handler only reads fixed buffers,
so it keeps CrashLog's rule of no allocation and no locks. A layer that
registers under a GPU driver's own registry key does not appear in
`vk layers:`. Third-party products register under the Khronos keys, and when
loaded their DLLs show in `modules:` either way.

## Known-bad layers

`kBadLayers` in `app/VulkanLayers.cpp` lists layers that have crashed this
program on users' machines. Each entry is matched by manifest name or DLL file
name:

| layer | product | what it does |
| --- | --- | --- |
| `VK_LAYER_PlayClaw` | PlayClaw 6 (`playclawvk64.dll`) | It does not pass `vkDestroyInstance` down the chain. The loader frees the instance but leaves it in `loader.instances`, so the next `vkCreateDevice` or `vkDestroyDevice` walks freed memory. We create and destroy probe instances, so this fires on any run that opens a second device. |
| `VK_MIRILLIS_LAYER` | Mirillis Action! (`MirillisActionVulkanLayer.dll`) | `ACCESS_VIOLATION` reading 0x3ec inside `nvoglv64.dll`, under this layer, while a video decodes |

When one of them is installed and enabled, `main()` adds its name to
`VK_LOADER_LAYERS_DISABLE` before anything opens Vulkan. It does this through
`_putenv_s`, so the loader sees the change and so do child processes. A tool
says so on stderr; the window says so in its log. `SS_VK_KEEP_LAYERS=1` leaves
every layer on. The loader's own `VK_LOADER_LAYERS_ALLOW` also overrides the
disable.

The filter needs loader 1.3.234 or later. With an older `vulkan-1.dll`, the
layer's own `disable_environment` variable from its manifest is set instead.
PlayClaw's is the generic `DISABLE_LAYER`.

To add a layer, it needs a reproduction with that layer as the cause. Our own
bugs that a layer merely exposes do not belong in the list. Once confirmed,
add a row with its manifest `name` and DLL file name.

Workarounds to give users whose layer is not on the list:

- Start Spirula with `VK_LOADER_LAYERS_DISABLE=~implicit~` set to test
  whether any implicit layer is involved. Then narrow it down to the layer
  name that `vk layers:` shows.
- Set the layer's registry value under `SOFTWARE\Khronos\Vulkan\ImplicitLayers`
  to 1. This turns it off for every program, and the product may set it back
  when it updates.

## A lost device while decoding video

Built-in video decoding runs on the inference device: Vulkan Video, then
`video.yuv_to_rgb`. If the driver faults there, the next submit or wait returns
`VK_ERROR_DEVICE_LOST`. Every `NN_VK_CHECK` and the decode submit then set
`nn::device_lost()`. The flag stays set until `nn::shutdown()`, because every
later call on that device fails too.

- **Preview (`gui/VideoBrowser.cpp`).** It catches the failure, closes the
  decoder and shows the cause and the remedy. Its button turns on "Extract
  frames with ffmpeg". It does not fall back to ffmpeg by itself, because
  ffmpeg is much slower and the user should choose that trade. `GuiApp` logs
  the driver's own message. Once no run is using the device, it calls
  `stop_inference_users()` and `nn::shutdown()`, so the next GPU task opens a
  new device instead of failing on the dead one.
- **Frame extraction (`app/FrameExtract.cpp`).** It ends the run with
  `err_video_gpu_lost`, which names the same option and `--decoder ffmpeg`.
  The run does not continue on ffmpeg. The driver's message goes to the log.
- **Decoder teardown.** `VideoPipeline::Impl::~Impl` drains with
  `Stream::drain()`, not `sync()`, because `sync()` throws on a lost device and
  a destructor cannot.

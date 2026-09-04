<div align="center">

![](assets/banner.png)

# Spirula Studio

![GPLv3 License](https://img.shields.io/badge/License-GPLv3-blue.svg)&nbsp;
![GitHub Releases](https://img.shields.io/github/v/release/harry7557558/spirula-studio)&nbsp;
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-blue)

[**Download**](https://github.com/harry7557558/spirula-studio/releases/) &#8226;
[**Build from Source**](#build-from-source) &#8226;
[**Gallery**](#gallery) &#8226;
[**Web Viewer**](https://harry7557558.github.io/spirula-studio/viewer/)

</div>

Spirula Studio trains 3D Gaussian Splatting models &ndash; from raw photo/video to splat to textured mesh &ndash; in one self-contained binary. No Python/PyTorch, no separate COLMAP install. Runs on NVIDIA, AMD, Intel, and Apple GPUs via Vulkan, trains 10M full-SH Gaussians in 8 GB VRAM, and has native support for fisheye and 360° cameras.

<div align="center">

![Spirula Studio - Open Source 3D Gaussian Splatting Pipeline](https://spirula.studio/assets/video/spirula-studio-hero.webp?md5=0ff79909f5c779509664da20b9f9dd61)

<sup>Dataset credit: [Garage](https://www.kaggle.com/datasets/simonbethke/garage) by Simon Bethke (CC BY-SA 4.0); Flight Systems and Control Lab at UTIAS; MegaDepth-X; Mip-NeRF 360.</sup>

</div>

## Features

- Cross vendor support via Vulkan compute &ndash; Runs on **NVIDIA, AMD, Intel, and Apple** GPUs

- One strategy combining advantages of **MCMC/IGS+/MRNF** &ndash; Sharper results, fewer floaters, from objects to large scenes

- Extreme **VRAM efficiency** with quantized training &ndash; Up to 10 million SH3 Gaussians in 8GB VRAM

- Native **360° camera** and **equirectangular** support &ndash; Load a dataset and train, no undistortion needed

- Modified **Bilateral grid** and **PPISP** for exposure/WB correction &ndash; Improving quality without unwanted color shift or darkening

- Built-in **lightning-fast SfM**, **AI masking**, frame extraction from videos &ndash; No need to wait for COLMAP or run separate scripts

- Depth/normal, meshing, skybox, linear color... And more.

## News

- **August 14, 2026: macOS support** &ndash; Support for training on macOS/Apple Silicon has been validated. The app can now be downloaded from [Releases page](https://github.com/harry7557558/spirula-studio/releases/).

- **August 8, 2026: Multilingual support** &ndash; Multilingual support has been added, available to both GUI and CLI. Supported languages: English, 日本語, 简体中文, 繁體中文, 한국어, Deutsch, Français, Español, Português, Italiano, Nederlands, Русский, Türkçe.

- **August 8, 2026: End-to-end workflow** &ndash; The Vulkan backend now has components to extract frames from video, AI masking, native SfM, meshing, and batch training, accessible from both GUI and CLI.

- **July 22, 2026: Cross-vendor support** &ndash; A Vulkan backend has been added, which works on NVIDIA, AMD, and Intel GPUs.

- **July 12, 2026: GUI** &ndash; A training GUI has been implemented. CLI training will remain accessible.


## Download

Binaries for Windows, Linux, and macOS can be downloaded from [Releases page](https://github.com/harry7557558/spirula-studio/releases/). Simply select the one for your platform, download and unzip, and double click to open the GUI.

If you are training on remote/cloud GPUs, you may use the CLI &ndash; Run `spirula --help` for details. By default, `spirula train` command will serve a viewer on an HTTP port, one you can forward over ssh and view training progress in your web browser.


## Build from source

To build from source, Spirula Studio provides two backends:

- **Vulkan (Recommended):** The cross-platform and cross-vendor option. Most tested. Works on all major GPUs. Faster to build and produces smaller binary.

- **CUDA:** Legacy option for CUDA-capable NVIDIA GPUs.

Both provide the same training and meshing functionality. CUDA backend may be faster or slower than Vulkan depending on GPU driver, with difference generally within a few percents. Vulkan backend can be slightly more VRAM efficient in some cases.

| Backend | GPU/Vendor Support | Platform Support | Dependencies | Additional Features |
|--------|--------|--------|--------|--------|
| Vulkan | NVIDIA, AMD, Intel, Apple Silicon | Windows, Linux, macOS | Vulkan/MoltenVK, CMake/Ninja | Native support for SfM, frame extraction from videos, and AI masking |
| CUDA | Most NVIDIA GPUs | Windows, Linux | CUDA, CMake/Ninja | - |

<details>

<summary>Details for building the Vulkan backend</summary>

<br/>

Make sure you have Vulkan SDK installed. On macOS, MoltenVK is automatically fetched by CMake. Clone the repository and run the commands:

### Windows with MSVC:

```bat
cd spirula-studio\
build_develop.bat -DSS_BUILD_CLI=ON -DSS_BUILD_GUI=ON -DSS_BACKEND=vulkan -DSS_ENABLE_PATENTED=ON
```

If it builds successfully, you get `build\spirula.exe`.

### Windows with GCC/Clang:

```bat
cd spirula-studio\
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release -DSS_BUILD_CLI=ON -DSS_BUILD_GUI=ON -DSS_BACKEND=vulkan -DSS_ENABLE_PATENTED=ON -DCMAKE_MAKE_PROGRAM=Ninja
cmake --build build -j
```

Pass `-DCMAKE_C_COMPILER` and `-DCMAKE_CXX_COMPILER` to the first `cmake` command if needed.

If it builds successfully, you get `build\spirula.exe`.

### Linux:

```bash
cd spirula-studio/
bash build_develop.bash -DSS_BUILD_CLI=ON -DSS_BUILD_GUI=ON -DSS_BACKEND=vulkan -DSS_ENABLE_PATENTED=ON
```

If it builds successfully, you get `build/spirula` binary.

### macOS:

```bash
cd spirula-studio/
bash build_develop.bash -DSS_BUILD_CLI=ON -DSS_BUILD_GUI=ON -DSS_BACKEND=vulkan -DSS_ENABLE_PATENTED=ON
cmake --build build --target macos_app
cmake --build build --target macos_dmg
```

If it builds successfully, you get `build/spirula` binary similar to Linux. Additionally, it wraps that binary in a double-clickable `build/Spirula Studio.app`, as well as disk image `build/Spirula Studio.dmg`. MoltenVK is statically linked by default and will run on a Mac without dependency installed.

### Notes regarding third-party licensing

`-DSS_ENABLE_PATENTED=ON` enables decoding video on the GPU instead of shelling out to ffmpeg (about 15x faster frame extraction, and without need to install ffmpeg). However, AVC/HEVC bitstream parsers carry third-party patent exposure. If you turn this on, you are responsible for ensuring compliance with local patent laws regarding AVC/HEVC playback.

Masking needs a SAM checkpoint, which the GUI downloads on first use and caches. The checkpoints are Meta's models under Meta's licenses &ndash; SAM 2.1 is Apache-2.0, SAM 3 is under Meta's own, non-standard license. They are never bundled, and the GUI shows the terms before fetching anything. On the command line, point `--model` at a file you downloaded yourself.

</details>

<details>

<summary>Details for building the CUDA backend</summary>

<br/>

Make sure you have a recent version of CUDA installed. On Windows, you also need MSVC compiler compatible with your CUDA version. Clone the repository and run the commands:

### Windows:

```bat
cd spirula-studio\
build_develop.bat -DSS_BUILD_CLI=ON -DSS_BUILD_GUI=ON -DSS_BACKEND=cuda
```

If it builds successfully, you get `build\spirula.exe`.

### Linux:

```bash
cd spirula-studio/
bash build_develop.bash -DSS_BUILD_CLI=ON -DSS_BUILD_GUI=ON -DSS_BACKEND=cuda
```

If it builds successfully, you get `build/spirula` binary.

</details>

## Gallery

You can find some professional-quality splats trained by Spirula Studio from [Megascapes Library](https://library.getmegascapes.com/) and their [SuperSplat page](https://superspl.at/user/megascapes).

Some splats created by the author of Spirula Studio can also be found on my [SuperSplat page](https://superspl.at/user?id=harry7557558).

<!-- ![](https://s3-eu-west-1.amazonaws.com/images.playcanvas.com/splat/643dadb5/v1/m.webp)&nbsp;
![](https://s3-eu-west-1.amazonaws.com/images.playcanvas.com/splat/ed448729/v1/m.webp)&nbsp;
![](https://s3-eu-west-1.amazonaws.com/images.playcanvas.com/splat/05e46a4b/v1/m.webp)&nbsp;
![](https://s3-eu-west-1.amazonaws.com/images.playcanvas.com/splat/9dd8696b/v1/m.webp)&nbsp;
![](https://s3-eu-west-1.amazonaws.com/images.playcanvas.com/splat/0bcb61c6/v1/m.webp)&nbsp;
![](https://s3-eu-west-1.amazonaws.com/images.playcanvas.com/splat/cdd6f9a2/v1/m.webp)&nbsp; -->

## Trivia

Spirula Studio (formerly spirulae-splat) is named after the now-inactive project [spirulae](https://github.com/harry7557558/spirulae), which was named after the [deep-ocean cephalopod mollusk](https://en.wikipedia.org/wiki/Spirula).

Spirula Studio is developed and maintained almost entirely by one person. Issues and PRs welcome; response times vary.

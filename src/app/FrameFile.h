#pragma once

namespace app {

// What an image at a JPEG quality becomes: a JPEG, or a JPEG XL or AVIF encoded
// straight from the pixels to at least that JPEG's fidelity.
enum class FrameFile { Jpeg, Jxl, Avif };

inline const char* frame_file_ext(FrameFile f) {
    return f == FrameFile::Jxl ? ".jxl" : f == FrameFile::Avif ? ".avif" : ".jpg";
}

// JPEG XL 97 at effort 3 beat stb's JPEG 95 on SSIMULACRA2 and Butteraugli on
// 2.7K, 4K and 1080p video frames in 17-40% fewer bytes, encoding in 1.4x its
// time (effort 7: 15x, for 10% fewer). 10-bit AVIF needed 93.
inline constexpr int kJxlOverJpegQuality = 2;
inline constexpr int kAvifUnderJpegQuality = 2, kAvifSpeed = 8;
inline constexpr int kJxlEffort = 3;

}  // namespace app

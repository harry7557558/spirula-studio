#pragma once

#include <array>
#include <atomic>
#include <thread>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <vector>

namespace spirula { struct SplatCloud; }

namespace gui {

class SplatMemoryError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Shares projection and fragment shaders with the standalone WebGL viewer.
class HardwareSplatRenderer {
public:
    ~HardwareSplatRenderer();
    static bool available();
    static uint64_t free_device_bytes();
    void load(const std::string& ply, const float center[3], float unit,
              const spirula::SplatCloud* sample = nullptr);
    // The viewer buffers it draws, as the compute view shows them
    // (viewer_buffer_keys); false for a key it has no equivalent of.
    enum class Buffer { Rgb, RgbRaw, Depth, Alpha, DepthNormal, RenderNormal, Sh };
    static bool buffer(const std::string& key, Buffer& out);
    // primitive: 0 3dgs, 1 mip, 2 3dgut, 3 2dgs
    unsigned render(int width, int height, const float view[16], int camera,
                    float fx, float fy, int primitive, int degree,
                    const float gamut[9], int transfer, bool linear,
                    Buffer buffer = Buffer::Rgb);
    void destroy();
    void finish_sort() { if (_sort_thread.joinable()) _sort_thread.join(); }
    bool loaded() const { return _count != 0; }
    // Stochastic (SS_VIEWER_STOCHASTIC): a still view still averaging frames in.
    bool refining() const;
    double sort_seconds() const { return _sort_seconds.load(); }
    size_t attribute_bytes() const { return _bytes; }
private:
    struct Texture {
        unsigned id = 0;
        int width = 0, height = 0, layers = 0;
    };
    Texture allocate(int64_t count, unsigned internal, unsigned format, unsigned type);
    void upload(const Texture& t, int64_t first, int64_t count, int components,
                unsigned format, unsigned type, const void* data);
    bool sort(const float view[16], int camera);
    void sort_work(const float view[16], int camera);
    std::thread _sort_thread;
    std::atomic<bool> _sort_ready{false};
    std::atomic<double> _sort_seconds{0};
    std::array<float, 16> _sorting_view{};
    int _sorting_camera = -1;
    // output: 0 colour, 1 ray depth, 2 normal, composited the same way
    unsigned program(int camera, int primitive, int output);
    // The front-to-back order is drawn in this many runs, a stencil pass marking
    // saturated pixels between two (kSaturatedFs); runs hold at least kMinChunk.
    static constexpr uint32_t kChunks = 16, kMinChunk = 1u << 16;
    std::array<Texture, 11> _textures{};  // ... uSb1, then the colour pass's output
    std::array<unsigned, 60> _programs{};
    std::array<unsigned, 5> _buffers{};  // display of a non-colour buffer, by camera
    unsigned _tone = 0, _saturated = 0, _colors = 0, _colors_sh = 0, _frame_copy = 0, _min_max = 0;
    unsigned _vao = 0, _indices = 0, _stencil = 0;
    unsigned _fbo[7]{}, _color[3]{}, _reduced[2]{};
    bool _stochastic = false;
    int _samples = 1;           // MSAA samples a stochastic draw takes
    uint32_t _frames = 0;
    unsigned _frame_depth = 0;
    int _width = 0, _height = 0, _rest = 0, _degree = 0, _sh_layers = 0, _lobes = 0, _sb_layers = 0;
    uint32_t _count = 0;
    size_t _bytes = 0;
    std::vector<float> _positions;
    std::vector<uint32_t> _keys, _order, _scratch;
    std::array<float, 16> _last_view{};
    std::vector<float> _last_render;
    int _last_camera = -1;
};

}

#pragma once

#include <array>
#include <atomic>
#include <thread>
#include <cstdint>
#include <string>
#include <vector>

namespace gui {

// Shares projection and fragment shaders with the standalone WebGL viewer.
class HardwareSplatRenderer {
public:
    ~HardwareSplatRenderer();
    static bool available();
    void load(const std::string& ply, const float center[3], float unit);
    unsigned render(int width, int height, const float view[16], int camera,
                    float fx, float fy, int primitive, int degree,
                    const float gamut[9], int transfer, bool linear);
    void destroy();
    void finish_sort() { if (_sort_thread.joinable()) _sort_thread.join(); }
    bool loaded() const { return _count != 0; }
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
    unsigned program(int camera, int primitive);
    std::array<Texture, 8> _textures{};
    std::array<unsigned, 15> _programs{};
    unsigned _tone = 0, _vao = 0, _indices = 0;
    unsigned _fbo[2]{}, _color[2]{};
    int _width = 0, _height = 0, _rest = 0, _degree = 0, _sh_layers = 0;
    uint32_t _count = 0;
    size_t _bytes = 0;
    std::vector<float> _positions;
    std::vector<uint32_t> _keys, _order, _scratch;
    std::array<float, 16> _last_view{};
    std::vector<float> _last_render;
    int _last_camera = -1;
};

}

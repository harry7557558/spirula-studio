#include "app/gui/HardwareSplatRenderer.h"
#include "app/gui/HardwareSplatSort.h"
#include "app/gui/GlLoader.h"
#include "checkpoint/SplatPly.h"
#include "core/HostMemory.h"
#include "app_generated/splat_shaders.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>

#ifndef APIENTRY
#define APIENTRY
#endif

namespace gui {
namespace {
constexpr unsigned array_texture = 0x8c1a, rgba16f = 0x881a, rgba32f = 0x8814;
constexpr unsigned rgb16f = 0x881b, rgb8snorm = 0x8f96;
using TexImage = void (APIENTRY *)(GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
using TexSub = void (APIENTRY *)(GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const void*);
TexImage image3;
TexSub sub3;
void (APIENTRY *attrib_i)(GLuint, GLint, GLenum, GLsizei, const void*);
void (APIENTRY *divisor)(GLuint, GLuint);
void (APIENTRY *draw_instances)(GLenum, GLint, GLsizei, GLsizei);
void (APIENTRY *uniform2i)(GLint, GLint, GLint);
void (APIENTRY *matrix3)(GLint, GLsizei, GLboolean, const GLfloat*);
void (APIENTRY *bind_image)(GLuint, GLuint, GLint, GLboolean, GLint, GLenum, GLenum);
void (APIENTRY *memory_barrier)(GLbitfield);

template<class T> void load_fn(T& fn, const char* name) {
    fn = reinterpret_cast<T>(glfwGetProcAddress(name));
    if (!fn) throw std::runtime_error(std::string("OpenGL function unavailable: ") + name);
}
void init() {
    if (!glx::init()) throw std::runtime_error("OpenGL initialization failed");
    load_fn(image3, "glTexImage3D"); load_fn(sub3, "glTexSubImage3D");
    load_fn(attrib_i, "glVertexAttribIPointer"); load_fn(divisor, "glVertexAttribDivisor");
    load_fn(draw_instances, "glDrawArraysInstanced"); load_fn(uniform2i, "glUniform2i");
    load_fn(matrix3, "glUniformMatrix3fv");
    if (!glfwExtensionSupported("GL_ARB_fragment_shader_interlock"))
        throw std::runtime_error("Faithful hardware splats require GL_ARB_fragment_shader_interlock");
    load_fn(bind_image, "glBindImageTexture"); load_fn(memory_barrier, "glMemoryBarrier");
}
void check_gl() {
    const unsigned error = glGetError();
    if (error) throw std::runtime_error(error == GL_OUT_OF_MEMORY
        ? "Insufficient GPU memory for hardware splat rendering"
        : "Hardware splat renderer OpenGL error " + std::to_string(error));
}
std::string shader(const std::string& name) {
    const std::string source(reinterpret_cast<const char*>(kSplatShaders), kSplatShadersSize);
    auto start = source.find("const " + name + " = /* glsl */`");
    if (start == std::string::npos) throw std::runtime_error("Missing shared shader: " + name);
    start = source.find('`', start) + 1;
    const auto end = source.find('`', start);
    std::string result = source.substr(start, end - start);
    size_t p;
    while ((p = result.find("${")) != std::string::npos) {
        const auto e = result.find('}', p);
        result.replace(p, e - p + 1, shader(result.substr(p + 2, e - p - 2)));
    }
    return result;
}
unsigned link_program(const char* vs, const char* fs, int camera = 0, int primitive = 0) {
    unsigned program = glx::CreateProgram();
    try {
        for (auto stage : {std::make_pair(vs, GL_VERTEX_SHADER), std::make_pair(fs, GL_FRAGMENT_SHADER)}) {
            std::string s = shader(stage.first);
            s.replace(0, s.find('\n') + 1, "#version 150\n#define CAM_MODEL " + std::to_string(camera) +
                      "\n#define PRIM " + std::to_string(primitive) + "\n");
            if (stage.second == GL_FRAGMENT_SHADER && std::string(fs).find("SPLAT_FS") == 0) {
                const auto newline = s.find('\n')+1;
                s.insert(newline, "#extension GL_ARB_shader_image_load_store : require\n"
                    "#extension GL_ARB_fragment_shader_interlock : require\n"
                    "layout(pixel_interlock_ordered) in;\n"
                    "layout(rgba32f) coherent uniform image2D uAccum;\n");
                const auto output = s.find("out vec4 fragColor;");
                s.erase(output, std::strlen("out vec4 fragColor;"));
                const std::string write = "fragColor = vec4(vColor.rgb, alpha);";
                const auto at = s.find(write);
                s.replace(at, write.size(), R"(
  beginInvocationInterlockARB();
  ivec2 pixel = ivec2(gl_FragCoord.xy);
  vec4 accum = imageLoad(uAccum, pixel);
  if (accum.a > 0.0) {
    float nextT = accum.a*(1.0-alpha);
    if (nextT > 1e-4) accum = vec4(accum.rgb+vColor.rgb*(alpha*accum.a),nextT);
    else accum.a = -accum.a;
    imageStore(uAccum, pixel, accum);
  }
  endInvocationInterlockARB();
)");
            }
            size_t p;
            while ((p = s.find("precision ")) != std::string::npos) s.erase(p, s.find(';', p) - p + 1);
            if ((p = s.find("layout(location=0) ")) != std::string::npos) s.erase(p, 19);
            unsigned sh = glx::CreateShader(stage.second);
            const char* text = s.c_str(); glx::ShaderSource(sh, 1, &text, nullptr); glx::CompileShader(sh);
            int ok; glx::GetShaderiv(sh, GL_COMPILE_STATUS, &ok);
            if (!ok) {
                char log[4096]; glx::GetShaderInfoLog(sh, sizeof(log), nullptr, log);
                glx::DeleteShader(sh); throw std::runtime_error(log);
            }
            glx::AttachShader(program, sh); glx::DeleteShader(sh);
        }
        glx::BindAttribLocation(program, 0, "aIndex"); glx::LinkProgram(program);
        int ok; glx::GetProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[4096]; glx::GetProgramInfoLog(program, sizeof(log), nullptr, log);
            throw std::runtime_error(log);
        }
        return program;
    } catch (...) { glx::DeleteProgram(program); throw; }
}
}

HardwareSplatRenderer::Texture HardwareSplatRenderer::allocate(
        int64_t count, unsigned internal, unsigned format, unsigned type) {
    Texture t;
    int max_texture; glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture);
    if (max_texture < 2048) throw std::runtime_error("Hardware splats require 2048-wide textures");
    t.width = 2048;
    t.height = int(std::min<int64_t>(2048, std::max<int64_t>(1, (count + 2047) / 2048)));
    t.layers = int((count + int64_t(t.width) * t.height - 1) / (int64_t(t.width) * t.height));
    t.layers = std::max(1, t.layers);
    int limit; glGetIntegerv(0x88ff, &limit);
    if (t.layers > limit) throw std::runtime_error("Splat texture exceeds OpenGL array layer limit");
    glGenTextures(1, &t.id); glBindTexture(array_texture, t.id);
    glTexParameteri(array_texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(array_texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    image3(array_texture, 0, internal, t.width, t.height, t.layers, 0, format, type, nullptr);
    if (const auto error = glGetError()) {
        glDeleteTextures(1, &t.id);
        throw std::runtime_error(error == GL_OUT_OF_MEMORY
            ? "Insufficient GPU memory for hardware splat attributes"
            : "OpenGL texture allocation failed: " + std::to_string(error));
    }
    return t;
}
void HardwareSplatRenderer::upload(const Texture& t, int64_t first, int64_t count,
        int components, unsigned format, unsigned type, const void* data) {
    glBindTexture(array_texture, t.id);
    const auto* bytes = static_cast<const unsigned char*>(data);
    const size_t stride = components * (type == GL_FLOAT ? sizeof(float) : 1);
    while (count) {
        const int x = int(first % t.width), y = int(first / t.width % t.height);
        const int z = int(first / (int64_t(t.width) * t.height));
        const int rows = x == 0 ? int(std::min<int64_t>(count / t.width, t.height - y)) : 0;
        const int width = rows ? t.width : int(std::min<int64_t>(count, t.width - x));
        const int height = std::max(1, rows);
        sub3(array_texture, 0, x, y, z, width, height, 1, format, type, bytes);
        const int64_t n = int64_t(width) * height;
        count -= n; first += n; bytes += size_t(n) * stride;
    }
    check_gl();
}

void HardwareSplatRenderer::load(const std::string& path, const float center[3], float unit) {
    destroy(); init();
    while (glGetError()) {}
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    try {
        spirula::read_splat_ply_chunks(path, 65536, [&](const spirula::SplatCloud& c, int64_t first, int64_t total) {
            if (!first) {
                if (c.sb_lobes || c.sh_degree > 4)
                    throw std::runtime_error("Hardware viewer supports up to SH4, without spherical-beta lobes");
                if (total > INT32_MAX / std::max<int64_t>(1, c.coeffs()))
                    throw std::runtime_error("Splat count exceeds signed OpenGL texture indexing capacity");
                const auto available = spirula::availableRamBytes();
                // Driver texture backing raised measured SH3 RSS to 139 bytes per splat at 100M.
                const uint64_t host_estimate = uint64_t(total)*(88+4*c.coeffs())+(256ull<<20);
                if (available && host_estimate > available * 3 / 4)
                    throw std::runtime_error("Insufficient host memory for hardware viewer positions and sorting");
                _count = uint32_t(total); _degree = c.sh_degree; _rest = int(c.coeffs());
                _positions.resize(size_t(total) * 3);
                _textures[0] = allocate(total, rgba32f, GL_RGBA, GL_FLOAT);
                _textures[1] = allocate(total, rgba16f, GL_RGBA, GL_FLOAT);
                _textures[2] = allocate(total, rgba16f, GL_RGBA, GL_FLOAT);
                _textures[3] = allocate(total, rgb16f, GL_RGB, GL_FLOAT);
                const int64_t texels = std::max<int64_t>(1, total * _rest);
                _sh_layers = int((texels + 4 * 2048LL * 2048 - 1) / (4 * 2048LL * 2048));
                for (int i = 0; i < 4; ++i) {
                    const auto count = std::max<int64_t>(1, std::min<int64_t>(int64_t(_sh_layers)*2048*2048,
                        texels - int64_t(i)*_sh_layers*2048*2048));
                    _textures[4+i] = allocate(count, rgb8snorm, GL_RGB, GL_BYTE);
                }
                _bytes = 0;
                const size_t texel_bytes[] = {16,8,8,6,3,3,3,3};
                for (int i = 0; i < 8; ++i)
                    _bytes += size_t(_textures[i].width)*_textures[i].height*_textures[i].layers*texel_bytes[i];
            }
            std::vector<float> pos(size_t(c.num)*4), rot(pos.size()), scale(pos.size());
            std::vector<int8_t> sh(size_t(c.num)*_rest*3);
            for (int64_t i = 0; i < c.num; ++i) {
                double norm = 0;
                for (int j = 0; j < 4; ++j) {
                    if (!std::isfinite(c.quats[i*4+j])) throw std::runtime_error("Non-finite splat quaternion");
                    norm += double(c.quats[i*4+j])*c.quats[i*4+j];
                }
                for (int j = 0; j < 3; ++j)
                    if (!std::isfinite(c.features_dc[i*3+j]) || std::abs(c.features_dc[i*3+j]) > 65504)
                        throw std::runtime_error("Splat color cannot be represented in hardware viewer");
                if (std::isnan(c.opacities[i])) throw std::runtime_error("Non-finite splat opacity");
                norm = norm > 0 ? 1/std::sqrt(norm) : 0;
                for (int j = 0; j < 3; ++j) {
                    pos[i*4+j] = (c.means[i*3+j]-center[j])/unit;
                    _positions[(first+i)*3+j] = pos[i*4+j];
                    rot[i*4+j] = c.quats[i*4+j+1]*norm;
                    scale[i*4+j] = c.scales[i*3+j]-std::log(unit);
                    if (!std::isfinite(pos[i*4+j]) || !std::isfinite(scale[i*4+j]) ||
                        std::abs(scale[i*4+j]) > 80)
                        throw std::runtime_error("Non-finite or unrepresentable splat geometry");
                }
                rot[i*4+3] = norm ? c.quats[i*4]*norm : 1;
                pos[i*4+3] = 1/(1+std::exp(-c.opacities[i]));
                float max_sh = 0;
                for (int j = 0; j < _rest*3; ++j) {
                    const float value = c.features_sh[i*_rest*3+j];
                    if (!std::isfinite(value) || std::abs(value) > 32768)
                        throw std::runtime_error("SH coefficient cannot be represented in hardware viewer");
                    max_sh = std::max(max_sh, std::abs(value));
                }
                // A power-of-two quantizer scale is exact in the half texture.
                max_sh = max_sh > 0 ? std::exp2(std::ceil(std::log2(max_sh))) : 1;
                scale[i*4+3] = max_sh;
                for (int j = 0; j < _rest*3; ++j)
                    sh[i*_rest*3+j] = int8_t(std::clamp(std::round(127*c.features_sh[i*_rest*3+j]/max_sh), -127.f, 127.f));
            }
            upload(_textures[0], first, c.num, 4, GL_RGBA, GL_FLOAT, pos.data());
            upload(_textures[1], first, c.num, 4, GL_RGBA, GL_FLOAT, rot.data());
            upload(_textures[2], first, c.num, 4, GL_RGBA, GL_FLOAT, scale.data());
            upload(_textures[3], first, c.num, 3, GL_RGB, GL_FLOAT, c.features_dc.data());
            int64_t at = first*_rest, remaining = c.num*_rest;
            const int64_t chunk = int64_t(_sh_layers)*2048*2048;
            const int8_t* p = sh.data();
            while (remaining) {
                const auto n = std::min(remaining, chunk-at%chunk);
                upload(_textures[4+at/chunk], at%chunk, n, 3, GL_RGB, GL_BYTE, p);
                remaining -= n; at += n; p += n*3;
            }
        });
        glx::GenVertexArrays(1, &_vao); glx::BindVertexArray(_vao);
        glx::GenBuffers(1, &_indices); glx::BindBuffer(GL_ARRAY_BUFFER, _indices);
        glx::BufferData(GL_ARRAY_BUFFER, size_t(_count)*4, nullptr, GL_STREAM_DRAW);
        glx::EnableVertexAttribArray(0); attrib_i(0, 1, GL_UNSIGNED_INT, 4, nullptr); divisor(0, 1);
        glx::BindVertexArray(0); check_gl();
    } catch (...) { destroy(); throw; }
}

void HardwareSplatRenderer::sort_work(const float view[16], int camera) {
    const auto begin = std::chrono::steady_clock::now();
    _keys.resize(_count); _order.resize(_count); _scratch.resize(_count);
    sort_splat_depths(_positions.data(),_count,view,camera,_keys.data(),_order.data(),_scratch.data());
    _sort_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
}

bool HardwareSplatRenderer::sort(const float view[16], int camera) {
    auto matches = [&] {
        if (_last_camera != camera) return false;
        const bool planar = camera == 0 || camera == 4;
        const int first = planar ? 8 : 0, last = planar ? 12 : 16;
        return std::equal(_last_view.begin()+first, _last_view.begin()+last, view+first);
    };
    auto upload_order = [&] {
        glx::BindBuffer(GL_ARRAY_BUFFER, _indices);
        auto sub = reinterpret_cast<void (APIENTRY *)(GLenum, std::ptrdiff_t, std::ptrdiff_t, const void*)>(glfwGetProcAddress("glBufferSubData"));
        sub(GL_ARRAY_BUFFER, 0, size_t(_count)*4, _order.data());
    };
    if (_last_camera < 0 || (_count < 1000000 && !_sort_thread.joinable())) {
        sort_work(view, camera); upload_order();
        std::copy(view, view+16, _last_view.begin()); _last_camera = camera;
    }
    if (_sort_thread.joinable() && !_sort_ready.load())
        return matches();
    if (_sort_ready.load()) {
        if (_sort_thread.joinable()) _sort_thread.join();
        upload_order();
        _last_view = _sorting_view; _last_camera = _sorting_camera;
        _sort_ready = false;
    }
    if (matches()) return true;
    std::copy(view, view+16, _sorting_view.begin()); _sorting_camera = camera;
    _sort_ready = false;
    _sort_thread = std::thread([this] {
        sort_work(_sorting_view.data(), _sorting_camera);
        _sort_ready = true;
    });
    return false;
}

unsigned HardwareSplatRenderer::program(int camera, int primitive) {
    static constexpr int models[] = {0,2,3,4,1};
    auto& p = _programs[camera*3+primitive];
    if (!p) p = link_program("SPLAT_VS", primitive == 2 ? "SPLAT_FS_3D" : "SPLAT_FS", models[camera], primitive);
    return p;
}

unsigned HardwareSplatRenderer::render(int W, int H, const float view[16], int camera,
        float fx, float fy, int primitive, int degree, const float gamut[9], int transfer, bool linear) {
    if (!_count) return 0;
    std::vector<float> state(view, view+16);
    state.insert(state.end(), gamut, gamut+9);
    for (float v : {float(W),float(H),float(camera),fx,fy,float(primitive),float(degree),float(transfer),float(linear)})
        state.push_back(v);
    if (state == _last_render && !_sort_ready.load()) return _color[1];
    if (!sort(view, camera)) return _color[1];
    if (!_tone) _tone = link_program("FULLSCREEN_VS", "TONEMAP_FS");
    if (!_fbo[0]) { glx::GenFramebuffers(2, _fbo); glGenTextures(2, _color); }
    if (_width != W || _height != H) {
        _width = _height = 0; _last_render.clear();
        for (int i = 0; i < 2; ++i) {
            glBindTexture(GL_TEXTURE_2D, _color[i]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D, 0, i ? GL_RGBA : rgba32f, W, H, 0, GL_RGBA, GL_FLOAT, nullptr);
            check_gl();
            glx::BindFramebuffer(GL_FRAMEBUFFER, _fbo[i]);
            glx::FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _color[i], 0);
            if (glx::CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                throw std::runtime_error("Incomplete hardware splat framebuffer");
        }
        _width = W; _height = H;
    }
    const unsigned p = program(camera, primitive);
    auto u = [p](const char* name) { return glx::GetUniformLocation(p, name); };
    glx::UseProgram(p); glx::BindFramebuffer(GL_FRAMEBUFFER, _fbo[0]);
    glViewport(0, 0, W, H); glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_SCISSOR_TEST);
    glDrawBuffer(GL_COLOR_ATTACHMENT0); glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_BLEND);
    bind_image(0, _color[0], 0, GL_FALSE, 0, 0x88ba, rgba32f);
    glx::Uniform1i(u("uAccum"), 0);
    glDrawBuffer(GL_NONE);
    static const char* names[] = {"uPos","uRot","uScale","uCol","uSh0","uSh1","uSh2","uSh3"};
    for (int i = 0; i < 8; ++i) {
        glx::ActiveTexture(GL_TEXTURE0+i); glBindTexture(array_texture, _textures[i].id); glx::Uniform1i(u(names[i]), i);
    }
    glx::Uniform1i(u("uSb0"), 0); glx::Uniform1i(u("uSb1"), 0); glx::Uniform1i(u("uSbLobes"), 0);
    glx::Uniform1i(u("uLogScale"), 1);
    glx::Uniform1i(u("uShRest"), _rest); glx::Uniform1i(u("uShChunkLayers"), _sh_layers);
    glx::Uniform1i(u("uShDegree"), degree < 0 ? _degree : std::min(degree, _degree));
    uniform2i(u("uDim"), _textures[0].width, _textures[0].height);
    uniform2i(u("uShDim"), 2048, 2048);
    float r[9], cam[3]{};
    for (int j = 0; j < 3; ++j) for (int k = 0; k < 3; ++k) {
        r[j*3+k] = view[j*4+k]; cam[k] -= view[j*4+k]*view[j*4+3];
    }
    matrix3(u("uViewR"), 1, GL_TRUE, r);
    glx::Uniform3f(u("uCamPos"), cam[0],cam[1],cam[2]);
    glx::Uniform2f(u("uFocal"), fx,fy); glx::Uniform2f(u("uCenter"), W*.5f,H*.5f);
    glx::Uniform2f(u("uViewport"), float(W),float(H));
    glx::Uniform1f(u("uNear"), 0); glx::Uniform1f(u("uOpacityScale"), 1);
    glx::BindVertexArray(_vao); draw_instances(GL_TRIANGLE_STRIP, 0, 4, _count);
    memory_barrier(0x20 | 0x8);
    bind_image(0, 0, 0, GL_FALSE, 0, 0x88ba, rgba32f);
    glx::BindFramebuffer(GL_FRAMEBUFFER, _fbo[1]); glDrawBuffer(GL_COLOR_ATTACHMENT0); glx::UseProgram(_tone);
    auto t = [this](const char* name) { return glx::GetUniformLocation(_tone, name); };
    glx::ActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, _color[0]);
    glx::Uniform1i(t("uHdr"), 0); glx::Uniform1i(t("uTransfer"), transfer);
    glx::Uniform1i(t("uIsLinear"), linear); glx::Uniform1f(t("uExposure"), 1);
    glx::Uniform3f(t("uBackground"), 0,0,0); matrix3(t("uGamut"), 1, GL_TRUE, gamut);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glx::BindVertexArray(0); glx::UseProgram(0); glx::BindFramebuffer(GL_FRAMEBUFFER, 0);
    check_gl(); _last_render = std::move(state); return _color[1];
}

bool HardwareSplatRenderer::available() {
    return glfwGetCurrentContext() && glfwExtensionSupported("GL_ARB_fragment_shader_interlock");
}

HardwareSplatRenderer::~HardwareSplatRenderer() {
    if (_sort_thread.joinable()) _sort_thread.join();
}

void HardwareSplatRenderer::destroy() {
    if (_sort_thread.joinable()) _sort_thread.join();
    const bool context = glfwGetCurrentContext() != nullptr;
    for (auto& t : _textures) { if (context && t.id) glDeleteTextures(1, &t.id); t = {}; }
    for (auto& p : _programs) { if (context && p) glx::DeleteProgram(p); p = 0; }
    if (context && _tone) glx::DeleteProgram(_tone);
    if (context && _indices) glx::DeleteBuffers(1, &_indices);
    if (context && _vao) glx::DeleteVertexArrays(1, &_vao);
    if (context && _fbo[0]) glx::DeleteFramebuffers(2, _fbo);
    if (context && _color[0]) glDeleteTextures(2, _color);
    _fbo[0] = _fbo[1] = _color[0] = _color[1] = 0;
    _last_render.clear(); _sort_ready = false;
    _tone = _indices = _vao = _count = 0; _width = _height = 0; _last_camera = -1;
    std::vector<float>().swap(_positions);
    std::vector<uint32_t>().swap(_keys); std::vector<uint32_t>().swap(_order); std::vector<uint32_t>().swap(_scratch);
}
}

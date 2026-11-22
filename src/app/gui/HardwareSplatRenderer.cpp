#include "app/gui/HardwareSplatRenderer.h"
#include "app/gui/HardwareSplatSort.h"
#include "app/gui/GlLoader.h"
#include "checkpoint/SplatPly.h"
#include "core/HostMemory.h"
#include "core/SphericalBeta.h"
#include "engine/Engine.h"
#include "app_generated/splat_shaders.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <stdexcept>

#ifndef APIENTRY
#define APIENTRY
#endif

namespace gui {
namespace {
constexpr unsigned array_texture = 0x8c1a, rgba16f = 0x881a, rgba32f = 0x8814;
constexpr unsigned rgb16f = 0x881b, rgb8snorm = 0x8f96;
constexpr unsigned depth24_stencil8 = 0x88f0, depth_stencil_attachment = 0x821a;
constexpr unsigned constant_alpha = 0x8003, one_minus_constant_alpha = 0x8004;
constexpr unsigned texture_2d_multisample = 0x9100, rg32f = 0x8230, rg = 0x8227;
// One fragment a splat, in storage order: the colour the splat pass reads
// (SPLAT_COLOR_TEXTURE in shaders.js).
constexpr const char* kColorFs = R"(#version 150
uniform sampler2DArray uPos;
uniform sampler2DArray uScale;
uniform int uLayer;
out vec4 fragColor;
${TEX_FETCH}
${SPLAT_COLOR_GLSL}
void main() {
  int i = (uLayer*uDim.y + int(gl_FragCoord.y))*uDim.x + int(gl_FragCoord.x);
  fragColor = vec4(evalColor(i, fetch1(uPos, i).xyz, fetch1(uScale, i).w), 1.0);
}
)";
// One stochastic frame's samples, averaged, then into the target by a constant
// blend factor.
constexpr const char* kFrameFs = R"(#version 150
uniform sampler2DMS uFrame;
uniform int uSamples;
out vec4 fragColor;
void main() {
  vec4 sum = vec4(0.0);
  for (int s = 0; s < uSamples; s++) sum += texelFetch(uFrame, ivec2(gl_FragCoord.xy), s);
  fragColor = sum / float(uSamples);
}
)";
// A buffer other than colour, shown as engine_blit_view shows compute's
// (RenderWorker.cpp): uMode is the HardwareSplatRenderer::Buffer.
constexpr const char* kBufferFs = R"(#version 150
uniform sampler2D uHdr;
uniform sampler2D uMinMax;
uniform int uMode;
uniform vec2 uFocal;
uniform vec2 uCenter;
out vec4 fragColor;
${PROJ_GLSL}
// colormap_turbo (Visualizer.cu)
vec3 turbo(float t) {
  const vec3 k[8] = vec3[8](vec3(0.14637796, 0.08594198, 0.23431523),
    vec3(2.94711014, 2.06520532, 6.33792818), vec3(-10.15061040, 9.64581379, 9.26380342),
    vec3(-90.46877071, -55.09180383, -203.76964931), vec3(550.44382083, 149.40813531, 604.07329733),
    vec3(-1061.31232675, -245.26823636, -766.82678386), vec3(874.85901369, 202.98596195, 447.99287080),
    vec3(-266.03287948, -63.82163439, -97.24397473));
  vec3 c = vec3(0.0);
  for (int i = 7; i >= 0; i--) c = c*t + k[i];
  return c;
}
// The rasterizers' depth: the weighted one over the coverage.
float depth(vec4 hdr) { return hdr.r/max(1.0 - hdr.a, 1e-10); }
// A pixel's point in compute's camera frame (x right, y down, z forward).
vec3 point(ivec2 p) {
  ivec2 size = textureSize(uHdr, 0);
  bool inside = all(greaterThanEqual(p, ivec2(0))) && all(lessThan(p, size));
  float d = inside ? depth(texelFetch(uHdr, p, 0)) : 0.0;
  vec2 uv = (vec2(p) + 0.5 - uCenter)/uFocal;
#if CAM_MODEL == 1
  return vec3(uv.x, -uv.y, d);
#else
  vec3 r = unprojectCam(uv);
  return d*vec3(r.x, -r.y, -r.z);
#endif
}
// depth_to_normal_forward (points_to_normal); compute's row y - 1 is this y + 1.
vec3 depthNormal(ivec2 p) {
  ivec2 size = textureSize(uHdr, 0);
  if (p.x == 0 || p.y == 0 || p.x == size.x - 1 || p.y == size.y - 1) return vec3(0.0);
  vec3 p0 = point(p - ivec2(1, 0)), p1 = point(p + ivec2(1, 0));
  vec3 p2 = point(p + ivec2(0, 1)), p3 = point(p - ivec2(0, 1));
  if (dot(p0, p0) == 0.0 || dot(p1, p1) == 0.0 || dot(p2, p2) == 0.0 || dot(p3, p3) == 0.0)
    return vec3(0.0);
  vec3 n = cross(p1 - p0, p2 - p3);
  return dot(n, n) != 0.0 ? normalize(n) : n;
}
void main() {
  ivec2 p = ivec2(gl_FragCoord.xy);
  vec4 hdr = texelFetch(uHdr, p, 0);
  vec3 c = hdr.rgb;
  if (uMode == 2) {
    vec2 m = texelFetch(uMinMax, ivec2(0), 0).rg;
    c = turbo(m.y > m.x ? (depth(hdr) - m.x)/(m.y - m.x) : 0.0);
  } else if (uMode == 3) {
    c = vec3(1.0 - hdr.a);
  } else if (uMode == 4) {
    c = 0.5 + 0.5*depthNormal(p);
  } else if (uMode == 5) {
    c = 0.5 + 0.5*hdr.rgb;
  }
  // The blit's rounding: a normal's 0.5 is a tie GL may break the other way.
  fragColor = vec4(floor(255.0*clamp(c, 0.0, 1.0) + 0.5)/255.0, 1.0);
}
)";
// The finite range of the depth (kBufferFs), or of (min, max) pairs, 8 x 8 texels to one.
constexpr const char* kMinMaxFs = R"(#version 150
uniform sampler2D uSrc;
uniform ivec2 uSize;
uniform int uFirst;
out vec4 fragColor;
void main() {
  ivec2 o = 8*ivec2(gl_FragCoord.xy);
  vec2 m = vec2(1e38, -1e38);
  for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) {
    ivec2 p = o + ivec2(x, y);
    if (p.x >= uSize.x || p.y >= uSize.y) continue;
    vec4 t = texelFetch(uSrc, p, 0);
    vec2 v = t.rg;
    if (uFirst != 0) {
      v = vec2(t.r/max(1.0 - t.a, 1e-10));
      if (isnan(v.x) || isinf(v.x)) continue;
    }
    m = vec2(min(m.x, v.x), max(m.y, v.y));
  }
  fragColor = vec4(m, 0.0, 0.0);
}
)";
// Below 1e-4 transmittance compute stops compositing a pixel; between chunks of
// the front-to-back order, a stencil pass marks those pixels and early stencil
// skips their fragments.
constexpr const char* kSaturatedFs = R"(#version 150
uniform sampler2D uHdr;
void main() {
  if (texelFetch(uHdr, ivec2(gl_FragCoord.xy), 0).a >= 1e-4) discard;
}
)";
using TexImage = void (APIENTRY *)(GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
using TexSub = void (APIENTRY *)(GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const void*);
TexImage image3;
TexSub sub3;
void (APIENTRY *attrib_i)(GLuint, GLint, GLenum, GLsizei, const void*);
void (APIENTRY *uniform2i)(GLint, GLint, GLint);
void (APIENTRY *matrix3)(GLint, GLsizei, GLboolean, const GLfloat*);
void (APIENTRY *blend_separate)(GLenum, GLenum, GLenum, GLenum);
void (APIENTRY *buffer_sub)(GLenum, std::ptrdiff_t, std::ptrdiff_t, const void*);
void (APIENTRY *texture_layer)(GLenum, GLenum, GLuint, GLint, GLint);
void (APIENTRY *divisor)(GLuint, GLuint);
void (APIENTRY *draw_instances)(GLenum, GLint, GLsizei, GLsizei);
void (APIENTRY *blend_color)(GLfloat, GLfloat, GLfloat, GLfloat);
void (APIENTRY *uniform1ui)(GLint, GLuint);
void (APIENTRY *tex_multisample)(GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLboolean);
void (APIENTRY *storage_multisample)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);

template<class T> void load_fn(T& fn, const char* name) {
    fn = reinterpret_cast<T>(glfwGetProcAddress(name));
    if (!fn) throw std::runtime_error(std::string("OpenGL function unavailable: ") + name);
}
void init() {
    if (!glx::init()) throw std::runtime_error("OpenGL initialization failed");
    load_fn(image3, "glTexImage3D"); load_fn(sub3, "glTexSubImage3D");
    load_fn(attrib_i, "glVertexAttribIPointer"); load_fn(uniform2i, "glUniform2i");
    load_fn(matrix3, "glUniformMatrix3fv");
    load_fn(blend_separate, "glBlendFuncSeparate"); load_fn(buffer_sub, "glBufferSubData");
    load_fn(texture_layer, "glFramebufferTextureLayer"); load_fn(divisor, "glVertexAttribDivisor");
    load_fn(draw_instances, "glDrawArraysInstanced");
    load_fn(blend_color, "glBlendColor"); load_fn(uniform1ui, "glUniform1ui");
    load_fn(tex_multisample, "glTexImage2DMultisample");
    load_fn(storage_multisample, "glRenderbufferStorageMultisample");
}
void check_gl() {
    const unsigned error = glGetError();
    if (error) throw std::runtime_error(error == GL_OUT_OF_MEMORY
        ? "Insufficient GPU memory for hardware splat rendering"
        : "Hardware splat renderer OpenGL error " + std::to_string(error));
}
std::string shader(const std::string& name);
// Substitutes ${NAME} with the shaders.js source of that name.
std::string expand(std::string text) {
    size_t p;
    while ((p = text.find("${")) != std::string::npos) {
        const auto e = text.find('}', p);
        text.replace(p, e - p + 1, shader(text.substr(p + 2, e - p - 2)));
    }
    return text;
}
std::string shader(const std::string& name) {
    const std::string source(reinterpret_cast<const char*>(kSplatShaders), kSplatShadersSize);
    auto start = source.find("const " + name + " = /* glsl */`");
    if (start == std::string::npos) throw std::runtime_error("Missing shared shader: " + name);
    start = source.find('`', start) + 1;
    const auto end = source.find('`', start);
    return expand(source.substr(start, end - start));
}
// The value a splat fragment composites for HardwareSplatRenderer::program's
// output, in compute's camera frame (x right, y down, z forward).
constexpr const char* kSplatDepth = R"(flat in float vDepth;
// The distance to the mean, or its depth when orthographic (primitive_3dgs).
float splatDepth() {
#if CAM_MODEL == 0
  vec2 uv = (vCenterPx - uCenter)/uFocal;
  return vDepth*sqrt(1.0 + dot(uv, uv));
#else
  return vDepth;
#endif
}
)";
constexpr const char* kSplatNormal = R"(// evaluate_color_3dgs_normal / evaluate_2dgs.
vec3 splatNormal(vec3 rd, float t) {
#if PRIM == 3
  vec3 n = vec3(vG[0][2], vG[1][2], vG[2][2]);
  if (dot(n, rd) > 0.0) n = -n;
#else
  vec3 gro = vGro;
#if CAM_MODEL == 1
  gro += vG*vec3((gl_FragCoord.xy - uCenter)/uFocal, 0.0);
#endif
  vec3 n = -((gro + 2.0*t*(vG*rd))*vG);
  n *= inversesqrt(dot(n, n));
#endif
  return vec3(n.x, -n.y, -n.z);
}
)";
// vs / fs name a shaders.js source, or are GLSL 1.50 themselves. stochastic: a
// fragment survives with probability alpha, depth-tested on the sort key
// (docs/notes/stochastic-splats.md); output: HardwareSplatRenderer::program's.
unsigned link_program(const char* vs, const char* fs, int camera = 0, int primitive = 0,
                      bool stochastic = false, int samples = 1, int output = 0,
                      const char* defines = "") {
    unsigned program = glx::CreateProgram();
    try {
        for (auto stage : {std::make_pair(vs, GL_VERTEX_SHADER), std::make_pair(fs, GL_FRAGMENT_SHADER)}) {
            std::string s = std::strncmp(stage.first, "#version", 8) ? shader(stage.first) : expand(stage.first);
            const bool mask = stochastic && samples > 1 && stage.second == GL_FRAGMENT_SHADER;
            s.replace(0, s.find('\n') + 1, "#version 150\n" +
                      std::string(mask ? "#extension GL_ARB_sample_shading : require\n" : "") +
                      "#define SPLAT_COLOR_TEXTURE 1\n" + defines +
                      std::string(stochastic ? "#define SPLAT_STOCHASTIC 1\n" : "") + "#define CAM_MODEL " +
                      std::to_string(camera) + "\n#define PRIM " + std::to_string(primitive) + "\n");
            if (stage.second == GL_FRAGMENT_SHADER && std::string(fs).find("SPLAT_FS") == 0) {
                const bool eval3d = std::string(fs) == "SPLAT_FS_3D";
                const std::string value = output == 1 ? (eval3d ? "vec3(t, 0.0, 0.0)" : "vec3(splatDepth(), 0.0, 0.0)")
                                        : output == 2 ? (eval3d ? "splatNormal(rd, t)" : "vec3(0.0)") : "vColor.rgb";
                if (output) s.insert(s.find("void main()"), output == 1 ? (eval3d ? "" : kSplatDepth)
                                                                       : (eval3d ? kSplatNormal : ""));
                const std::string write = "fragColor = vec4(vColor.rgb, alpha);";
                const auto at = s.find(write);
                if (stochastic) {
                    // Each MSAA sample of the pixel draws on its own.
                    s.replace(at, write.size(), R"(uint h = pcgHash(uint(gl_FragCoord.x) ^ pcgHash(uint(gl_FragCoord.y) ^
      pcgHash(vIndex ^ pcgHash(uSeed))));
  int mask = 0;
  float u0 = float(h >> 8u) * (1.0/16777216.0);
  for (int k = 0; k < STOCHASTIC_SAMPLES; k++) {
#if STOCHASTIC_STRATIFIED
    float u = fract(u0 + float(k) / float(STOCHASTIC_SAMPLES));
#else
    float u = float(pcgHash(h + uint(k)) >> 8u) * (1.0/16777216.0);
#endif
    if (u < alpha) mask |= 1 << k;
  }
  if (mask == 0) discard;
#if STOCHASTIC_SAMPLES > 1
  gl_SampleMask[0] = mask;
#endif
  fragColor = vec4()" + value + ", 0.0);");
                    const std::string out = "out vec4 fragColor;";
                    s.insert(s.find(out) + out.size(), "\n#define STOCHASTIC_SAMPLES " + std::to_string(samples) +
                             "\n#define STOCHASTIC_STRATIFIED " + (engine_stochastic_stratified() ? "1" : "0") + R"(
flat in uint vIndex;
uniform uint uSeed;
uint pcgHash(uint v) {
  uint state = v * 747796405u + 2891336453u;
  uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
  return (word >> 22u) ^ word;
})");
                } else {
                    // Front to back under (DST_ALPHA, ONE): the target's alpha is the
                    // transmittance, so the colour has to arrive premultiplied.
                    s.replace(at, write.size(), "fragColor = vec4((" + value + ")*alpha, alpha);");
                }
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
            const int64_t row = c.coeffs();
            if (!first) {
                if (c.sh_degree > 4)
                    throw std::runtime_error("Hardware viewer supports up to SH4");
                if (total > INT32_MAX / std::max<int64_t>(1, row))
                    throw std::runtime_error("Splat count exceeds signed OpenGL texture indexing capacity");
                const auto available = spirula::availableRamBytes();
                // Driver texture backing raised measured SH3 RSS to 139 bytes per splat at 100M.
                const uint64_t host_estimate = uint64_t(total)*(88+4*row)+(256ull<<20);
                if (available && host_estimate > available * 3 / 4)
                    throw std::runtime_error("Insufficient host memory for hardware viewer positions and sorting");
                _count = uint32_t(total); _degree = c.sh_degree;
                _rest = c.basis().sh_coeffs(); _lobes = c.sb_lobes;
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
                // Two RGBA16F texels a lobe, as the web viewer stores them.
                const int64_t sb_texels = std::max<int64_t>(1, total * 2 * _lobes);
                _sb_layers = int((sb_texels + 2 * 2048LL * 2048 - 1) / (2 * 2048LL * 2048));
                for (int i = 0; i < 2; ++i) {
                    const auto count = std::max<int64_t>(1, std::min<int64_t>(int64_t(_sb_layers)*2048*2048,
                        sb_texels - int64_t(i)*_sb_layers*2048*2048));
                    _textures[8+i] = allocate(_lobes ? count : 1, rgba16f, GL_RGBA, GL_FLOAT);
                }
                _textures[10] = allocate(total, rgba16f, GL_RGBA, GL_FLOAT);
                _bytes = 0;
                const size_t texel_bytes[] = {16,8,8,6,3,3,3,3,8,8,8};
                for (int i = 0; i < 11; ++i)
                    _bytes += size_t(_textures[i].width)*_textures[i].height*_textures[i].layers*texel_bytes[i];
            }
            std::vector<float> pos(size_t(c.num)*4), rot(pos.size()), scale(pos.size());
            std::vector<int8_t> sh(size_t(c.num)*_rest*3);
            std::vector<float> sb(size_t(c.num)*_lobes*8);
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
                const float* rest = c.features_sh.data() + i*row*3;
                float max_sh = 0;
                for (int j = 0; j < _rest*3; ++j) {
                    if (!std::isfinite(rest[j]) || std::abs(rest[j]) > 32768)
                        throw std::runtime_error("SH coefficient cannot be represented in hardware viewer");
                    max_sh = std::max(max_sh, std::abs(rest[j]));
                }
                // A power-of-two quantizer scale is exact in the half texture.
                max_sh = max_sh > 0 ? std::exp2(std::ceil(std::log2(max_sh))) : 1;
                scale[i*4+3] = max_sh;
                for (int j = 0; j < _rest*3; ++j)
                    sh[i*_rest*3+j] = int8_t(std::clamp(std::round(127*rest[j]/max_sh), -127.f, 127.f));
                // (axis, sharpness), (amplitude, 0) from (r, g, b), (theta, phi, beta).
                for (int l = 0; l < _lobes; ++l) {
                    const float* lobe = rest + (_rest + 2*l)*3;
                    float* t = sb.data() + (i*_lobes + l)*8;
                    double axis[3];
                    spirula::sb::axis(lobe[3], lobe[4], axis);
                    for (int j = 0; j < 3; ++j) {
                        t[j] = float(axis[j]);
                        t[4+j] = float(spirula::sb::softplus(lobe[j]));
                        if (!std::isfinite(t[j]) || !std::isfinite(t[4+j]) || t[4+j] > 65504)
                            throw std::runtime_error("Spherical-beta lobe cannot be represented in hardware viewer");
                    }
                    t[3] = float(std::min(4*std::exp(double(lobe[5])), 65504.0));
                    t[7] = 0;
                    if (!std::isfinite(t[3])) throw std::runtime_error("Non-finite spherical-beta sharpness");
                }
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
            int64_t sb_at = first*2*_lobes, sb_left = c.num*2*_lobes;
            const int64_t sb_chunk = int64_t(_sb_layers)*2048*2048;
            const float* q = sb.data();
            while (sb_left) {
                const auto n = std::min(sb_left, sb_chunk-sb_at%sb_chunk);
                upload(_textures[8+sb_at/sb_chunk], sb_at%sb_chunk, n, 4, GL_RGBA, GL_FLOAT, q);
                sb_left -= n; sb_at += n; q += n*4;
            }
        });
        glx::GenVertexArrays(1, &_vao); glx::BindVertexArray(_vao);
        glx::GenBuffers(1, &_indices); glx::BindBuffer(GL_ARRAY_BUFFER, _indices);
        _stochastic = engine_stochastic_enabled();
        _samples = 1;
        if (_stochastic && glfwExtensionSupported("GL_ARB_sample_shading")) {
            int most = 1, color_most = 1;
            glGetIntegerv(0x8d57, &most); glGetIntegerv(0x910e, &color_most);
            while (_samples * 2 <= int(engine_stochastic_spp()) && _samples * 2 <= std::min(most, color_most))
                _samples *= 2;
        }
        if (_stochastic) {
            std::vector<uint32_t> order(_count);
            std::iota(order.begin(), order.end(), 0u);
            glx::BufferData(GL_ARRAY_BUFFER, size_t(_count)*4, order.data(), GL_STATIC_DRAW);
        } else {
            glx::BufferData(GL_ARRAY_BUFFER, size_t(_count)*4, nullptr, GL_STREAM_DRAW);
        }
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
        buffer_sub(GL_ARRAY_BUFFER, 0, size_t(_count)*4, _order.data());
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

namespace { constexpr int kModels[] = {0,2,3,4,1}; }

unsigned HardwareSplatRenderer::program(int camera, int primitive, int output) {
    auto& p = _programs[(camera*4+primitive)*3+output];
    if (!p) p = link_program("SPLAT_VS", primitive >= 2 ? "SPLAT_FS_3D" : "SPLAT_FS", kModels[camera],
                             primitive, _stochastic, _samples, output);
    return p;
}

bool HardwareSplatRenderer::buffer(const std::string& key, Buffer& out) {
    static const std::pair<const char*, Buffer> keys[] = {
        {"rgb", Buffer::Rgb}, {"rgb_raw", Buffer::RgbRaw}, {"depth", Buffer::Depth},
        {"alpha", Buffer::Alpha}, {"depth_normal", Buffer::DepthNormal},
        {"render_normal", Buffer::RenderNormal}, {"sh", Buffer::Sh}};
    for (const auto& k : keys)
        if (key == k.first) { out = k.second; return true; }
    return false;
}

bool HardwareSplatRenderer::refining() const {
    return _stochastic && _count && _frames < engine_stochastic_frame_target();
}

unsigned HardwareSplatRenderer::render(int W, int H, const float view[16], int camera,
        float fx, float fy, int primitive, int degree, const float gamut[9], int transfer, bool linear,
        Buffer buffer) {
    if (!_count) return 0;
    std::vector<float> state(view, view+16);
    state.insert(state.end(), gamut, gamut+9);
    for (float v : {float(W),float(H),float(camera),fx,fy,float(primitive),float(degree),float(transfer),
                    float(linear),float(buffer)})
        state.push_back(v);
    const bool sh = buffer == Buffer::Sh;
    const int output = buffer == Buffer::Rgb || buffer == Buffer::RgbRaw || sh ? 0
                     : buffer == Buffer::RenderNormal ? 2 : 1;
    if (_stochastic) {
        // No order at all: a still view only averages more frames in.
        if (state != _last_render) _frames = 0;
        else if (!refining()) return _color[1];
    } else {
        if (state == _last_render && !_sort_ready.load()) return _color[1];
        if (!sort(view, camera)) return _color[1];
    }
    if (!_tone) _tone = link_program("FULLSCREEN_VS", "TONEMAP_FS");
    if (_stochastic && !_frame_copy) _frame_copy = link_program("FULLSCREEN_VS", kFrameFs);
    if (!_saturated) _saturated = link_program("FULLSCREEN_VS", kSaturatedFs);
    if (!_colors) _colors = link_program("FULLSCREEN_VS", kColorFs);
    if (sh && !_colors_sh) _colors_sh = link_program("FULLSCREEN_VS", kColorFs, 0, 0, false, 1, 0,
                                                     "#define SPLAT_ZERO_DC 1\n");
    if (output && !_buffers[camera]) _buffers[camera] = link_program("FULLSCREEN_VS", kBufferFs, kModels[camera]);
    if (buffer == Buffer::Depth && !_min_max) _min_max = link_program("FULLSCREEN_VS", kMinMaxFs);
    if (!_fbo[0]) {
        glx::GenFramebuffers(7, _fbo); glGenTextures(3, _color); glGenTextures(2, _reduced);
        glx::GenRenderbuffers(1, &_stencil); glx::GenRenderbuffers(1, &_frame_depth);
    }
    if (_width != W || _height != H) {
        _width = _height = 0; _last_render.clear();
        glx::BindRenderbuffer(GL_RENDERBUFFER, _stencil);
        glx::RenderbufferStorage(GL_RENDERBUFFER, depth24_stencil8, W, H);
        // The accumulator and the display image; a stochastic frame has its own
        // samples of colour and depth.
        for (int i = 0; i < 2; ++i) {
            glBindTexture(GL_TEXTURE_2D, _color[i]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D, 0, i ? GL_RGBA : rgba32f, W, H, 0, GL_RGBA, GL_FLOAT, nullptr);
            check_gl();
        }
        // The depth range's reduction, 8 x 8 to one, ping-ponged.
        for (int i = 0; i < 2; ++i) {
            glBindTexture(GL_TEXTURE_2D, _reduced[i]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D, 0, rg32f, (W + 7)/8, (H + 7)/8, 0, rg, GL_FLOAT, nullptr);
            glx::BindFramebuffer(GL_FRAMEBUFFER, _fbo[5+i]);
            glx::FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _reduced[i], 0);
            glDrawBuffer(GL_COLOR_ATTACHMENT0);
            check_gl();
        }
        if (_stochastic) {
            glBindTexture(texture_2d_multisample, _color[2]);
            tex_multisample(texture_2d_multisample, _samples, rgba16f, W, H, GL_TRUE);
            glx::BindRenderbuffer(GL_RENDERBUFFER, _frame_depth);
            storage_multisample(GL_RENDERBUFFER, _samples, depth24_stencil8, W, H);
            check_gl();
        }
        for (int i : {0, 1, 2, 4}) {
            if (i == 4 && !_stochastic) continue;
            glx::BindFramebuffer(GL_FRAMEBUFFER, _fbo[i]);
            const unsigned color = i == 4 ? _color[2] : i < 2 ? _color[i] : 0;
            if (color) glx::FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                                 i == 4 ? texture_2d_multisample : GL_TEXTURE_2D, color, 0);
            if (i != 1) glx::FramebufferRenderbuffer(GL_FRAMEBUFFER, depth_stencil_attachment, GL_RENDERBUFFER,
                                                     i == 4 ? _frame_depth : _stencil);
            glDrawBuffer(color ? GL_COLOR_ATTACHMENT0 : GL_NONE);
            if (glx::CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                throw std::runtime_error("Incomplete hardware splat framebuffer");
        }
        _width = W; _height = H;
    }
    float r[9], cam[3]{};
    for (int j = 0; j < 3; ++j) for (int k = 0; k < 3; ++k) {
        r[j*3+k] = view[j*4+k]; cam[k] -= view[j*4+k]*view[j*4+3];
    }
    static const char* names[] = {"uPos","uRot","uScale","uCol","uSh0","uSh1","uSh2","uSh3","uSb0","uSb1","uColor"};
    for (int i = 0; i < 11; ++i) {
        glx::ActiveTexture(GL_TEXTURE0+i); glBindTexture(array_texture, _textures[i].id);
    }
    auto bind_attributes = [&](unsigned program) {
        auto u = [program](const char* name) { return glx::GetUniformLocation(program, name); };
        for (int i = 0; i < 11; ++i) glx::Uniform1i(u(names[i]), i);
        glx::Uniform1i(u("uSbLobes"), _lobes); glx::Uniform1i(u("uSbChunkLayers"), std::max(_sb_layers, 1));
        uniform2i(u("uSbDim"), 2048, 2048);
        glx::Uniform1i(u("uShRest"), _rest); glx::Uniform1i(u("uShChunkLayers"), _sh_layers);
        glx::Uniform1i(u("uShDegree"), degree < 0 ? _degree : std::min(degree, _degree));
        uniform2i(u("uDim"), _textures[0].width, _textures[0].height);
        uniform2i(u("uShDim"), 2048, 2048);
        glx::Uniform3f(u("uCamPos"), cam[0],cam[1],cam[2]);
    };
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND); glDisable(GL_STENCIL_TEST);
    // The colour pass renders into uColor, which it does not sample; sh is
    // the colour without its DC term.
    glx::BindVertexArray(_vao);
    if (output == 0) {
        const unsigned colors_program = sh ? _colors_sh : _colors;
        glx::UseProgram(colors_program); bind_attributes(colors_program);
        glx::BindFramebuffer(GL_FRAMEBUFFER, _fbo[3]);
        const Texture& colors = _textures[10];
        for (int layer = 0; layer < colors.layers; ++layer) {
            const int64_t left = int64_t(_count) - int64_t(layer)*colors.width*colors.height;
            texture_layer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colors.id, 0, layer);
            glx::Uniform1i(glx::GetUniformLocation(colors_program, "uLayer"), layer);
            glViewport(0, 0, colors.width, int(std::min<int64_t>(colors.height, (left + colors.width - 1)/colors.width)));
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
    }
    const unsigned p = program(camera, primitive, output);
    auto u = [p](const char* name) { return glx::GetUniformLocation(p, name); };
    glx::UseProgram(p); bind_attributes(p); glx::BindFramebuffer(GL_FRAMEBUFFER, _fbo[0]);
    glViewport(0, 0, W, H);
    glx::Uniform1i(u("uLogScale"), 1);
    matrix3(u("uViewR"), 1, GL_TRUE, r);
    glx::Uniform2f(u("uFocal"), fx,fy); glx::Uniform2f(u("uCenter"), W*.5f,H*.5f);
    glx::Uniform2f(u("uViewport"), float(W),float(H));
    glx::Uniform1f(u("uNear"), 0); glx::Uniform1f(u("uOpacityScale"), 1);
    glx::BindBuffer(GL_ARRAY_BUFFER, _indices);
    if (_stochastic) {
        // Each sample: the nearest surviving fragment of every splat, in storage
        // order; then the frame joins the running average with weight 1/(n+1).
        const uint32_t draws = std::max<uint32_t>(1, engine_stochastic_spp() / _samples);
        attrib_i(0, 1, GL_UNSIGNED_INT, 4, nullptr);
        for (uint32_t draw = 0; draw < draws; ++draw) {
            const uint32_t n = _frames * draws + draw;
            glx::UseProgram(p);
            uniform1ui(u("uSeed"), n);
            glx::BindFramebuffer(GL_FRAMEBUFFER, _fbo[4]);
            glClearColor(0, 0, 0, 1); glClearDepth(1); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glDepthMask(GL_TRUE);
            draw_instances(GL_TRIANGLE_STRIP, 0, 4, _count);
            glDisable(GL_DEPTH_TEST);
            glx::BindFramebuffer(GL_FRAMEBUFFER, _fbo[0]); glx::UseProgram(_frame_copy);
            glx::Uniform1i(glx::GetUniformLocation(_frame_copy, "uFrame"), 15);
            glx::Uniform1i(glx::GetUniformLocation(_frame_copy, "uSamples"), _samples);
            glx::ActiveTexture(GL_TEXTURE0+15); glBindTexture(texture_2d_multisample, _color[2]);
            glEnable(GL_BLEND); blend_color(0, 0, 0, 1.0f / float(n + 1));
            blend_separate(constant_alpha, one_minus_constant_alpha,
                           constant_alpha, one_minus_constant_alpha);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glDisable(GL_BLEND);
            glBindTexture(texture_2d_multisample, 0);
        }
        ++_frames;
    } else {
        glClearColor(0, 0, 0, 1); glClearStencil(0); glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        // Front to back: C += T a c, T *= 1 - a, with T in the target's alpha.
        glEnable(GL_BLEND); blend_separate(GL_DST_ALPHA, GL_ONE, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_STENCIL_TEST);
        const uint32_t chunk = std::max<uint32_t>(kMinChunk, (_count + kChunks - 1) / kChunks);
        for (uint32_t first = 0; first < _count; first += chunk) {
            if (first) {
                glDisable(GL_BLEND);
                glx::BindFramebuffer(GL_FRAMEBUFFER, _fbo[2]); glx::UseProgram(_saturated);
                glx::Uniform1i(glx::GetUniformLocation(_saturated, "uHdr"), 15);
                glx::ActiveTexture(GL_TEXTURE0+15); glBindTexture(GL_TEXTURE_2D, _color[0]);
                glStencilFunc(GL_ALWAYS, 1, 0xff); glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
                glDrawArrays(GL_TRIANGLES, 0, 3);
                glBindTexture(GL_TEXTURE_2D, 0);
                glx::BindFramebuffer(GL_FRAMEBUFFER, _fbo[0]); glx::UseProgram(p); glEnable(GL_BLEND);
            }
            glStencilFunc(GL_EQUAL, 0, 0xff); glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
            attrib_i(0, 1, GL_UNSIGNED_INT, 4, reinterpret_cast<const void*>(size_t(first)*4));
            draw_instances(GL_TRIANGLE_STRIP, 0, 4, std::min(chunk, _count - first));
        }
        glDisable(GL_STENCIL_TEST); glDisable(GL_BLEND);
    }
    unsigned range = 0;
    if (buffer == Buffer::Depth) {
        glx::UseProgram(_min_max);
        glx::Uniform1i(glx::GetUniformLocation(_min_max, "uSrc"), 15);
        glx::ActiveTexture(GL_TEXTURE0+15);
        int w = W, h = H;
        for (int pass = 0; pass == 0 || w > 1 || h > 1; ++pass) {
            glBindTexture(GL_TEXTURE_2D, pass ? _reduced[(pass-1)%2] : _color[0]);
            glx::BindFramebuffer(GL_FRAMEBUFFER, _fbo[5+pass%2]);
            uniform2i(glx::GetUniformLocation(_min_max, "uSize"), w, h);
            glx::Uniform1i(glx::GetUniformLocation(_min_max, "uFirst"), pass == 0);
            w = (w + 7)/8; h = (h + 7)/8;
            glViewport(0, 0, w, h);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            range = _reduced[pass%2];
        }
        glViewport(0, 0, W, H);
    }
    glx::BindFramebuffer(GL_FRAMEBUFFER, _fbo[1]);
    glx::ActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, _color[0]);
    if (buffer == Buffer::Rgb) {
        glx::UseProgram(_tone);
        auto t = [this](const char* name) { return glx::GetUniformLocation(_tone, name); };
        glx::Uniform1i(t("uHdr"), 0); glx::Uniform1i(t("uTransfer"), transfer);
        glx::Uniform1i(t("uIsLinear"), linear); glx::Uniform1f(t("uExposure"), 1);
        glx::Uniform3f(t("uBackground"), 0,0,0); matrix3(t("uGamut"), 1, GL_TRUE, gamut);
    } else {
        const unsigned shown = _buffers[camera];
        glx::UseProgram(shown);
        auto t = [shown](const char* name) { return glx::GetUniformLocation(shown, name); };
        glx::Uniform1i(t("uHdr"), 0); glx::Uniform1i(t("uMinMax"), 1);
        glx::Uniform1i(t("uMode"), int(buffer));
        glx::Uniform2f(t("uFocal"), fx, fy); glx::Uniform2f(t("uCenter"), W*.5f, H*.5f);
        glx::ActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, range);
    }
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glx::BindVertexArray(0); glx::UseProgram(0); glx::BindFramebuffer(GL_FRAMEBUFFER, 0);
    check_gl(); _last_render = std::move(state); return _color[1];
}

bool HardwareSplatRenderer::available() {
    return glfwGetCurrentContext() != nullptr;
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
    if (context && _saturated) glx::DeleteProgram(_saturated);
    if (context && _colors) glx::DeleteProgram(_colors);
    if (context && _colors_sh) glx::DeleteProgram(_colors_sh);
    if (context && _min_max) glx::DeleteProgram(_min_max);
    for (auto& b : _buffers) { if (context && b) glx::DeleteProgram(b); b = 0; }
    if (context && _frame_copy) glx::DeleteProgram(_frame_copy);
    if (context && _stencil) glx::DeleteRenderbuffers(1, &_stencil);
    if (context && _frame_depth) glx::DeleteRenderbuffers(1, &_frame_depth);
    if (context && _indices) glx::DeleteBuffers(1, &_indices);
    if (context && _vao) glx::DeleteVertexArrays(1, &_vao);
    if (context && _fbo[0]) glx::DeleteFramebuffers(7, _fbo);
    if (context && _color[0]) glDeleteTextures(3, _color);
    if (context && _reduced[0]) glDeleteTextures(2, _reduced);
    for (auto& f : _fbo) f = 0;
    for (auto& c : _color) c = 0;
    for (auto& r : _reduced) r = 0;
    _last_render.clear(); _sort_ready = false;
    _tone = _saturated = _colors = _colors_sh = _min_max = _frame_copy = _stencil = _frame_depth = _indices = _vao = _count = 0; _width = _height = 0; _last_camera = -1;
    _rest = _lobes = _sb_layers = 0; _frames = 0;
    std::vector<float>().swap(_positions);
    std::vector<uint32_t>().swap(_keys); std::vector<uint32_t>().swap(_order); std::vector<uint32_t>().swap(_scratch);
}
}

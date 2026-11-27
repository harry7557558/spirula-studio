#include "app/gui/HardwareSplatGpuSort.h"
#include "app/gui/GlLoader.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <type_traits>

#ifndef APIENTRY
#define APIENTRY
#endif

namespace gui {
namespace {
constexpr unsigned storage_buffer = 0x90d2, compute_shader = 0x91b9, dynamic_copy = 0x88ea;
constexpr unsigned storage_barrier = 0x2000, attrib_barrier = 0x1;
constexpr unsigned max_storage_block = 0x90de, array_texture = 0x8c1a;
constexpr uint32_t kTile = 2048;  // keys a workgroup, kTile/256 a thread

void (APIENTRY *dispatch)(GLuint, GLuint, GLuint);
void (APIENTRY *memory_barrier)(GLbitfield);
void (APIENTRY *bind_base)(GLenum, GLuint, GLuint);
void (APIENTRY *uniform1ui)(GLint, GLuint);
void (APIENTRY *get_integer64)(GLenum, int64_t*);

// Per call: WGL entry points belong to the context current when fetched.
bool load_functions() {
    auto load = [](auto& fn, const char* name) {
        fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(glfwGetProcAddress(name));
        return fn != nullptr;
    };
    return glx::init() && load(dispatch, "glDispatchCompute") && load(memory_barrier, "glMemoryBarrier") &&
           load(bind_base, "glBindBufferBase") && load(uniform1ui, "glUniform1ui") &&
           load(get_integer64, "glGetInteger64v");
}

constexpr const char* kCommon = R"(
layout(local_size_x = 256) in;
const uint kTile = TILE, kItems = kTile/256u;
uniform uint uCount;
uniform uint uBlocks;
uniform uint uShift;
// Workgroups past the last tile run the barriers and touch no memory.
uint tileIndex() { return gl_WorkGroupID.y*gl_NumWorkGroups.x + gl_WorkGroupID.x; }
)";
// Each tile's digit histogram, digit-major; the first pass also writes the keys.
constexpr const char* kCountCs = R"(
layout(std430, binding = 0) buffer Keys { uint keys[]; };
layout(std430, binding = 2) writeonly buffer Counts { uint counts[]; };
#ifdef KEYS
uniform sampler2DArray uPos;
uniform vec4 uView[3];
uniform int uCamera;
// splat_depth_key, operation for operation.
uint depthKey(uint i) {
  ivec3 size = textureSize(uPos, 0);
  uint w = uint(size.x), h = uint(size.y);
  vec3 p = texelFetch(uPos, ivec3(int(i % w), int(i / w % h), int(i / (w*h))), 0).xyz;
  precise float v0 = uView[0].x*p.x + uView[0].y*p.y + uView[0].z*p.z + uView[0].w;
  precise float v1 = uView[1].x*p.x + uView[1].y*p.y + uView[1].z*p.z + uView[1].w;
  precise float v2 = uView[2].x*p.x + uView[2].y*p.y + uView[2].z*p.z + uView[2].w;
  precise float d = -v2;
  if (uCamera == 3) d = v0*v0 + v1*v1 + v2*v2;
  if (uCamera == 1 || uCamera == 2) {
    precise float r2 = v0*v0 + v1*v1, z2 = v2*v2;
    d = z2*z2 + r2*r2*(1.0/512.0);
  }
  if (isnan(d) || isinf(d)) d = -3.40282347e38;
  uint bits = floatBitsToUint(d);
  return bits ^ ((bits & 0x80000000u) != 0u ? 0xffffffffu : 0x80000000u);
}
#endif
shared uint histogram[256];
void main() {
  uint t = gl_LocalInvocationID.x, block = tileIndex();
  histogram[t] = 0u;
  barrier();
  if (block < uBlocks) for (uint k = 0u; k < kItems; ++k) {
    uint i = block*kTile + k*256u + t;
    if (i >= uCount) break;
#ifdef KEYS
    uint key = depthKey(i);
    keys[i] = key;
#else
    uint key = keys[i];
#endif
    atomicAdd(histogram[(key >> uShift) & 255u], 1u);
  }
  barrier();
  if (block < uBlocks) counts[t*uBlocks + block] = histogram[t];
}
)";
// One workgroup a digit: each tile's count becomes the digit's count in earlier
// tiles, and the digit's total goes after the table.
constexpr const char* kScanCs = R"(
layout(std430, binding = 2) buffer Counts { uint counts[]; };
shared uint partial[256];
void main() {
  uint t = gl_LocalInvocationID.x, row = gl_WorkGroupID.x*uBlocks, carry = 0u;
  for (uint first = 0u; first < uBlocks; first += 1024u) {
    uint v[4], sum = 0u;
    for (uint k = 0u; k < 4u; ++k) {
      uint b = first + 4u*t + k;
      v[k] = b < uBlocks ? counts[row + b] : 0u;
      sum += v[k];
    }
    partial[t] = sum;
    barrier();
    for (uint o = 1u; o < 256u; o <<= 1u) {
      uint x = t >= o ? partial[t - o] : 0u;
      barrier();
      partial[t] += x;
      barrier();
    }
    uint run = carry + partial[t] - sum;
    for (uint k = 0u; k < 4u; ++k) {
      uint b = first + 4u*t + k;
      if (b < uBlocks) counts[row + b] = run;
      run += v[k];
    }
    carry += partial[255];
    barrier();
  }
  if (t == 0u) counts[256u*uBlocks + gl_WorkGroupID.x] = carry;
}
)";
// The tile sorts itself stably by the digit in shared memory, then writes each
// digit's run contiguously after that digit's keys from earlier tiles.
constexpr const char* kScatterCs = R"(
layout(std430, binding = 0) readonly buffer KeysIn { uint keysIn[]; };
layout(std430, binding = 1) readonly buffer ValuesIn { uint valuesIn[]; };
layout(std430, binding = 2) readonly buffer Counts { uint counts[]; };
layout(std430, binding = 3) writeonly buffer KeysOut { uint keysOut[]; };
layout(std430, binding = 4) writeonly buffer ValuesOut { uint valuesOut[]; };
// The tile's keys then values, or 16 nibble counters a thread padded one word in 16.
shared uint tile[max(2u*kTile, 17u*256u)];
shared uint partial[256], digitStart[256], tileStart[256];
uint padded(uint i) { return i + (i >> 4u); }
uint exclusiveSum(uint v, uint t) {
  partial[t] = v;
  barrier();
  for (uint o = 1u; o < 256u; o <<= 1u) {
    uint x = t >= o ? partial[t - o] : 0u;
    barrier();
    partial[t] += x;
    barrier();
  }
  uint r = partial[t] - v;
  barrier();
  return r;
}
void main() {
  uint t = gl_LocalInvocationID.x, block = tileIndex();
  bool live = block < uBlocks;
  uint first = block*kTile, n = live ? min(kTile, uCount - first) : 0u;
  uint base = exclusiveSum(live ? counts[256u*uBlocks + t] : 0u, t);
  if (live) digitStart[t] = base + counts[t*uBlocks + block];
  // Padding takes the largest key, so it sorts after every real key.
  for (uint k = 0u; k < kItems; ++k) {
    uint j = k*256u + t;
    tile[j] = j < n ? keysIn[first + j] : 0xffffffffu;
#ifdef FIRST
    tile[kTile + j] = first + j;
#else
    tile[kTile + j] = j < n ? valuesIn[first + j] : 0u;
#endif
  }
  barrier();
  // Two stable counting sorts, low nibble first; a thread ranks kItems consecutive keys.
  for (uint nibble = 0u; nibble < 2u; ++nibble) {
    uint shift = uShift + 4u*nibble, key[kItems], value[kItems], rank[kItems];
    for (uint k = 0u; k < kItems; ++k) {
      key[k] = tile[t*kItems + k];
      value[k] = tile[kTile + t*kItems + k];
    }
    barrier();
    for (uint c = 0u; c < 17u; ++c) tile[17u*t + c] = 0u;
    barrier();
    for (uint k = 0u; k < kItems; ++k) {
      uint at = padded(((key[k] >> shift) & 15u)*256u + t);
      rank[k] = tile[at];
      tile[at] = rank[k] + 1u;
    }
    barrier();
    // Digit-major exclusive sum: thread t holds counters 16t..16t+15, at 17t.
    uint sum = 0u;
    for (uint c = 0u; c < 16u; ++c) sum += tile[17u*t + c];
    uint run = exclusiveSum(sum, t);
    for (uint c = 0u; c < 16u; ++c) {
      uint v = tile[17u*t + c];
      tile[17u*t + c] = run;
      run += v;
    }
    barrier();
    for (uint k = 0u; k < kItems; ++k) rank[k] += tile[padded(((key[k] >> shift) & 15u)*256u + t)];
    barrier();
    for (uint k = 0u; k < kItems; ++k) {
      tile[rank[k]] = key[k];
      tile[kTile + rank[k]] = value[k];
    }
    barrier();
  }
  for (uint k = 0u; k < kItems; ++k) {
    uint j = k*256u + t, d = (tile[j] >> uShift) & 255u;
    if (j == 0u || ((tile[j - 1u] >> uShift) & 255u) != d) tileStart[d] = j;
  }
  barrier();
  if (live) for (uint k = 0u; k < kItems; ++k) {
    uint j = k*256u + t;
    if (j >= n) break;
    uint key = tile[j], d = (key >> uShift) & 255u, at = digitStart[d] + j - tileStart[d];
#ifndef LAST
    keysOut[at] = key;
#endif
    valuesOut[at] = tile[kTile + j];
  }
}
)";

unsigned compile(const char* body, const char* defines) {
    const std::string source = "#version 430\n#define TILE " + std::to_string(kTile) + "u\n" + defines + kCommon + body;
    const unsigned shader = glx::CreateShader(compute_shader);
    const char* text = source.c_str();
    glx::ShaderSource(shader, 1, &text, nullptr); glx::CompileShader(shader);
    int ok = 0; glx::GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    char log[4096] = "";
    if (!ok) {
        glx::GetShaderInfoLog(shader, sizeof(log), nullptr, log);
        glx::DeleteShader(shader);
        throw std::runtime_error(std::string("GPU sort shader: ") + log);
    }
    const unsigned program = glx::CreateProgram();
    glx::AttachShader(program, shader); glx::LinkProgram(program); glx::DeleteShader(shader);
    glx::GetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        glx::GetProgramInfoLog(program, sizeof(log), nullptr, log);
        glx::DeleteProgram(program);
        throw std::runtime_error(std::string("GPU sort program: ") + log);
    }
    return program;
}
}

bool HardwareSplatGpuSort::supported(uint32_t count) {
    while (glGetError()) {}
    GLint major = 0, minor = 0;
    glGetIntegerv(0x821b, &major); glGetIntegerv(0x821c, &minor);
    if (glGetError() || major < 4 || (major == 4 && minor < 3) || !load_functions()) return false;
    int64_t block = 0;
    get_integer64(max_storage_block, &block);
    return !glGetError() && block >= int64_t(count)*4;
}

uint64_t HardwareSplatGpuSort::device_bytes(uint32_t count) {
    return uint64_t(count)*12 + (uint64_t(count + kTile - 1)/kTile*256 + 256)*4;
}

bool HardwareSplatGpuSort::allocate(uint32_t count) {
    destroy();
    if (!count || !supported(count)) return false;
    while (glGetError()) {}
    try {
        const char* defines[] = {"#define KEYS 1\n", "", "", "#define FIRST 1\n", "", "#define LAST 1\n"};
        const char* bodies[] = {kCountCs, kCountCs, kScanCs, kScatterCs, kScatterCs, kScatterCs};
        for (int i = 0; i < 6; ++i) _programs[i] = compile(bodies[i], defines[i]);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        destroy(); return false;
    }
    _blocks = (count + kTile - 1)/kTile;
    glx::GenBuffers(2, _keys); glx::GenBuffers(1, &_values); glx::GenBuffers(1, &_counts);
    for (unsigned b : {_keys[0], _keys[1], _values, _counts}) {
        glx::BindBuffer(storage_buffer, b);
        glx::BufferData(storage_buffer, b == _counts ? (size_t(_blocks)*256 + 256)*4 : size_t(count)*4,
                        nullptr, dynamic_copy);
    }
    glx::BindBuffer(storage_buffer, 0);
    if (glGetError()) { destroy(); return false; }
    _count = count;
    return true;
}

void HardwareSplatGpuSort::sort(unsigned positions, const float view[16], int camera, unsigned order) {
    if (!_count) return;
    // Up to 32768 workgroups a row; GL guarantees 65535.
    const uint32_t columns = std::min<uint32_t>(_blocks, 32768), rows = (_blocks + columns - 1)/columns;
    auto use = [&](unsigned program, uint32_t shift) {
        glx::UseProgram(program);
        uniform1ui(glx::GetUniformLocation(program, "uCount"), _count);
        uniform1ui(glx::GetUniformLocation(program, "uBlocks"), _blocks);
        uniform1ui(glx::GetUniformLocation(program, "uShift"), shift);
    };
    auto buffers = [&](unsigned keys_in, unsigned values_in, unsigned keys_out, unsigned values_out) {
        const unsigned b[] = {keys_in, values_in, _counts, keys_out, values_out};
        for (unsigned i = 0; i < 5; ++i) bind_base(storage_buffer, i, b[i]);
    };
    // The previous frame's draw read `order`.
    memory_barrier(storage_barrier | attrib_barrier);
    use(_programs[0], 0);
    for (int r = 0; r < 3; ++r)
        glx::Uniform4f(glx::GetUniformLocation(_programs[0], ("uView[" + std::to_string(r) + "]").c_str()),
                       view[4*r], view[4*r+1], view[4*r+2], view[4*r+3]);
    glx::Uniform1i(glx::GetUniformLocation(_programs[0], "uCamera"), camera);
    glx::Uniform1i(glx::GetUniformLocation(_programs[0], "uPos"), 0);
    glx::ActiveTexture(GL_TEXTURE0); glBindTexture(array_texture, positions);
    for (int pass = 0; pass < 4; ++pass) {
        // Keys alternate K0, K1; values start as the index, then V1, order, V1, order.
        const unsigned keys_in = _keys[pass % 2], keys_out = pass == 3 ? 0 : _keys[(pass + 1) % 2];
        const unsigned values_in = pass == 0 ? 0 : pass == 2 ? order : _values;
        const unsigned values_out = pass % 2 ? order : _values;
        buffers(keys_in, values_in, keys_out, values_out);
        if (pass) use(_programs[1], 8*pass);
        dispatch(columns, rows, 1); memory_barrier(storage_barrier);
        use(_programs[2], 8*pass);
        dispatch(256, 1, 1); memory_barrier(storage_barrier);
        use(_programs[pass == 0 ? 3 : pass == 3 ? 5 : 4], 8*pass);
        dispatch(columns, rows, 1); memory_barrier(storage_barrier);
    }
    memory_barrier(attrib_barrier);
    for (unsigned i = 0; i < 5; ++i) bind_base(storage_buffer, i, 0);
    glx::UseProgram(0);
}

void HardwareSplatGpuSort::destroy() {
    if (glfwGetCurrentContext()) {
        for (auto& p : _programs) if (p) glx::DeleteProgram(p);
        for (unsigned b : {_keys[0], _keys[1], _values, _counts}) if (b) glx::DeleteBuffers(1, &b);
    }
    for (auto& p : _programs) p = 0;
    _keys[0] = _keys[1] = _values = _counts = 0;
    _count = _blocks = 0;
}

}

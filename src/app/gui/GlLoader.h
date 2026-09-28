#pragma once

// Minimal OpenGL 3.2-core function loader for the GUI's own rendering
// (dataset preview). GL 1.1 entry points come from the system library the
// app already links; everything newer is fetched via glfwGetProcAddress and
// namespaced under glx:: so nothing collides with system GL headers.

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX   // keep windows.h from defining min()/max() macros
#endif
#include <windows.h>
#include <GL/gl.h>
#elif defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <cstddef>
#include <cstdint>

// Post-1.1 enums (guarded; system glext.h may already define them).
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER          0x8892
#endif
#ifndef GL_ELEMENT_ARRAY_BUFFER
#define GL_ELEMENT_ARRAY_BUFFER  0x8893
#endif
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW           0x88E4
#endif
#ifndef GL_STREAM_DRAW
#define GL_STREAM_DRAW           0x88E0
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER         0x8B31
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER       0x8B30
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS        0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS           0x8B82
#endif
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER           0x8D40
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0     0x8CE0
#endif
#ifndef GL_DEPTH_ATTACHMENT
#define GL_DEPTH_ATTACHMENT      0x8D00
#endif
#ifndef GL_RENDERBUFFER
#define GL_RENDERBUFFER          0x8D41
#endif
#ifndef GL_DEPTH_COMPONENT24
#define GL_DEPTH_COMPONENT24     0x81A6
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE  0x8CD5
#endif
#ifndef GL_PROGRAM_POINT_SIZE
#define GL_PROGRAM_POINT_SIZE    0x8642
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE         0x812F
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0              0x84C0
#endif

namespace glx {

using glSizeiptr = std::ptrdiff_t;

// Load all pointers below via glfwGetProcAddress. Returns false if any is
// missing. Idempotent; call with a current GL context.
bool init();

extern GLuint (*CreateShader)(GLenum type);
extern void (*ShaderSource)(GLuint shader, GLsizei count, const char* const* string, const GLint* length);
extern void (*CompileShader)(GLuint shader);
extern void (*GetShaderiv)(GLuint shader, GLenum pname, GLint* params);
extern void (*GetShaderInfoLog)(GLuint shader, GLsizei bufSize, GLsizei* length, char* infoLog);
extern GLuint (*CreateProgram)();
extern void (*AttachShader)(GLuint program, GLuint shader);
extern void (*LinkProgram)(GLuint program);
extern void (*GetProgramiv)(GLuint program, GLenum pname, GLint* params);
extern void (*GetProgramInfoLog)(GLuint program, GLsizei bufSize, GLsizei* length, char* infoLog);
extern void (*DeleteShader)(GLuint shader);
extern void (*DeleteProgram)(GLuint program);
extern void (*UseProgram)(GLuint program);
extern GLint (*GetUniformLocation)(GLuint program, const char* name);
extern void (*UniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
extern void (*Uniform1f)(GLint location, GLfloat v0);
extern void (*Uniform1i)(GLint location, GLint v0);
extern void (*Uniform2f)(GLint location, GLfloat v0, GLfloat v1);
extern void (*Uniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
extern void (*Uniform3f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
extern void (*Uniform1fv)(GLint location, GLsizei count, const GLfloat* value);
extern void (*GenBuffers)(GLsizei n, GLuint* buffers);
extern void (*BindBuffer)(GLenum target, GLuint buffer);
extern void (*BufferData)(GLenum target, glSizeiptr size, const void* data, GLenum usage);
extern void (*DeleteBuffers)(GLsizei n, const GLuint* buffers);
extern void (*GenVertexArrays)(GLsizei n, GLuint* arrays);
extern void (*BindVertexArray)(GLuint array);
extern void (*DeleteVertexArrays)(GLsizei n, const GLuint* arrays);
extern void (*BindAttribLocation)(GLuint program, GLuint index, const char* name);
extern void (*EnableVertexAttribArray)(GLuint index);
extern void (*VertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
extern void (*GenFramebuffers)(GLsizei n, GLuint* framebuffers);
extern void (*BindFramebuffer)(GLenum target, GLuint framebuffer);
extern void (*FramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
extern void (*FramebufferRenderbuffer)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
extern GLenum (*CheckFramebufferStatus)(GLenum target);
extern void (*DeleteFramebuffers)(GLsizei n, const GLuint* framebuffers);
extern void (*GenRenderbuffers)(GLsizei n, GLuint* renderbuffers);
extern void (*BindRenderbuffer)(GLenum target, GLuint renderbuffer);
extern void (*RenderbufferStorage)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
extern void (*DeleteRenderbuffers)(GLsizei n, const GLuint* renderbuffers);
// GL 1.3; the Linux OpenGL ABI only guarantees 1.2, so it is loaded like the
// rest rather than called directly.
extern void (*ActiveTexture)(GLenum texture);

}  // namespace glx

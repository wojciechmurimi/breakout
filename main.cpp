#include <exception>
#include <memory>
#include <string_view>
#include <unordered_map>
#define IMGUI_IMPL_OPENGL_ES3 1

#include "glad/include/glad/gles2.h"

#include <GLFW/glfw3.h>
// #include <glm/gtc/bitfield.hpp>
#include <assimp/material.h>
#include <assimp/matrix4x4.h>
#include <assimp/mesh.h>
#include <assimp/types.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>
#include <stb/stb_image.h>

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <math.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <vector>

// Dear ImGui: standalone example application for GLFW + OpenGL 3, using
// programmable pipeline (GLFW is a cross-platform general purpose library for
// handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation,
// etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/
// folder).
// - Introduction, links and more at the top of imgui.cpp

//=======
#include "./imgui/imgui.h"
//=======
#include "./imgui/backends/imgui_impl_glfw.h"
//=======
#include "./imgui/backends/imgui_impl_opengl3.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
// #include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to
// maximize ease of testing and compatibility with old VS compilers. To link
// with VS2010-era libraries, VS2015+ requires linking with
// legacy_stdio_definitions.lib, which we do using this pragma. Your own project
// should not be affected, as you are likely to link with a newer binary of GLFW
// that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) &&                                 \
    !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

// This example can also compile and run with Emscripten! See
// 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

void glCheckError_(const char *file, int line);
static void glfw_error_callback(int error, const char *description) {
  fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

struct PtrWrap {
  void *ptr;
  PtrWrap(const PtrWrap &) = delete;
  PtrWrap(PtrWrap &&other) noexcept {
    ptr = other.ptr;
    other.ptr = nullptr;
  }

  PtrWrap &operator=(const PtrWrap &) = delete;
  PtrWrap &operator=(PtrWrap &&) = delete;

  PtrWrap(size_t size) noexcept : ptr(malloc(size)) { assert(ptr); }
  ~PtrWrap() noexcept {
    if (ptr)
      free(ptr);
  }
};

static PtrWrap readFile(const char *path) {
  FILE *f = fopen(path, "r");
  assert(f != nullptr);
  fseek(f, 0, SEEK_END);
  size_t len = ftell(f);
  fseek(f, 0, SEEK_SET);
  size_t r = 0;
  PtrWrap pw(len + 1);
  char *buf = (char *)pw.ptr;
  for (; r < len;) {
    size_t n = fread(buf + r, 1, len - r, f);
    assert(ferror(f) == 0);
    r += n;
  }
  buf[len] = '\0';
  return pw;
}

struct Vao {
  Vao() noexcept {
    id = 0;
    glGenVertexArrays(1, &id);
    assert(id != 0);
  }

  Vao(const Vao &other) = delete;
  Vao &operator=(const Vao &other) = delete;

  Vao(Vao &&other) = delete;
  Vao &operator=(Vao &&other) = delete;

  void bind() const noexcept { glBindVertexArray(id); }

  void unbind() const noexcept { glBindVertexArray(0); }

  inline GLuint getId() const noexcept { return id; }

  ~Vao() noexcept { glDeleteVertexArrays(1, &id); }

private:
  GLuint id;
};

struct Vbo {
  Vbo() noexcept : attribCount(0), attribOffsets(0), size(0) {
    id = 0;
    glGenBuffers(1, &id);
    assert(id != 0);
  }

  Vbo(size_t size_bytes) noexcept
      : attribCount(0), attribOffsets(0), size(size_bytes) {
    id = 0;
    glGenBuffers(1, &id);
    assert(id != 0);
    bind();
    bufferData(nullptr, size_bytes, GL_DYNAMIC_DRAW);
    glCheckError_(__FILE__, __LINE__);
  }

  Vbo(const Vbo &other) = delete;
  Vbo &operator=(const Vbo &other) = delete;

  Vbo(const Vbo &&other) = delete;
  Vbo &operator=(const Vbo &&other) = delete;

  template <typename T>
  void bufferStaticData(T *data, size_t count) const noexcept {
    bufferData(data, count * sizeof(T), GL_STATIC_DRAW);
  }

  template <typename T>
  void bufferDynamicData(T *data, size_t count) const noexcept {
    bufferData(data, count * sizeof(T), GL_DYNAMIC_DRAW);
  }

  template <typename T>
  void bufferSubData(T *data, GLsizeiptr count,
                     GLintptr offt = 0) const noexcept {
    bind();
    glCheckError_(__FILE__, __LINE__);
    glBufferSubData(GL_ARRAY_BUFFER, offt, count * sizeof(T), data);
    glCheckError_(__FILE__, __LINE__);
  }

  void pushFloatAttribute(GLint count, GLsizei stride) noexcept {
    glVertexAttribPointer(attribCount, count, GL_FLOAT, GL_FALSE, stride,
                          (void *)attribOffsets);
    glEnableVertexAttribArray(attribCount);
    attribOffsets += count * 4;
    attribCount += 1;
  }

  void bind() const noexcept {
    glBindBuffer(GL_ARRAY_BUFFER, id);
    glCheckError_(__FILE__, __LINE__);
  }

  ~Vbo() noexcept {
    if (id != 0)
      glDeleteBuffers(1, &id);
    id = 0;
  }

private:
  void bufferData(void *data, GLsizeiptr len, GLenum usage) const noexcept {
    glBufferData(GL_ARRAY_BUFFER, len, data, usage);
  }

  GLuint id;
  size_t size;
  size_t attribOffsets;
  size_t attribCount;
};

struct Shader {
  Shader(const Shader &other) = delete;
  Shader &operator=(const Shader &other) = delete;

  Shader(Shader &&other) {
    id = other.id;
    other.id = 0;
  }
  Shader &operator=(Shader &&other) = delete;

  Shader() = default;

  Shader(const char *vertexShaderTxt, const char *fragmentShaderTxt) noexcept
      : id(0) {
    GLuint vs = compileShader(vertexShaderTxt, GL_VERTEX_SHADER);
    GLuint fs = compileShader(fragmentShaderTxt, GL_FRAGMENT_SHADER);

    id = glCreateProgram();
    assert(id != 0);

    int success;
    char infoLog[512];
    glAttachShader(id, vs);
    glAttachShader(id, fs);
    glLinkProgram(id);
    glGetProgramiv(id, GL_LINK_STATUS, &success);
    if (!success) {
      glGetProgramInfoLog(id, 512, NULL, infoLog);
      std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n"
                << infoLog << std::endl;
    }

    assert(success);

    glDeleteShader(vs);
    glDeleteShader(fs);
  }

  Shader(GLuint vs, GLuint fs) noexcept : id(0) {
    id = glCreateProgram();
    assert(id != 0);

    int success;
    char infoLog[512];
    glAttachShader(id, vs);
    glAttachShader(id, fs);
    glLinkProgram(id);
    glGetProgramiv(id, GL_LINK_STATUS, &success);
    if (!success) {
      glGetProgramInfoLog(id, 512, NULL, infoLog);
      std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n"
                << infoLog << std::endl;
    }

    assert(success);

    glDeleteShader(vs);
    glDeleteShader(fs);
  }

  static GLuint compileShader(const char *buf, GLenum type) noexcept {
    int success;
    char infoLog[512];
    GLuint id = glCreateShader(type);
    assert(id != 0);
    glShaderSource(id, 1, &buf, nullptr);
    glCompileShader(id);
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (!success) {
      glGetShaderInfoLog(id, 512, NULL, infoLog);
      std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n"
                << infoLog << std::endl;
    }
    return id;
  }

  static Shader fromFile(const char *vsPath, const char *fsPath) {
    const char *files[] = {vsPath, fsPath};
    GLuint shaders[] = {0, 0};
    GLuint shader_types[] = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER};

    for (int i = 0; i < 2; i++) {
      FILE *f = fopen(files[i], "r");
      assert(f != nullptr);
      fseek(f, 0, SEEK_END);
      size_t len = ftell(f);
      fseek(f, 0, SEEK_SET);
      size_t r = 0;

      char *buf = new char[len + 1];
      for (; r < len;) {
        size_t n = fread(buf + r, 1, len - r, f);
        assert(ferror(f) == 0);
        r += n;
      }
      buf[len] = 0;

      shaders[i] = compileShader(buf, shader_types[i]);

      fclose(f);
      delete[] buf;
    }

    return Shader(shaders[0], shaders[1]);
  }

  Shader &bind() noexcept {
    glUseProgram(id);
    return *this;
  }

  void unbind() const noexcept { glUseProgram(0); }

  void setInt(const char *name, int value) const noexcept {
    GLint loc = glGetUniformLocation(id, name);
    if (loc != -1) {
      glUniform1i(loc, value);
    } else {
      printf("Warning: Uniform %s not found.\n", name);
    }
  }

  void setFloat(const char *name, float value) const noexcept {
    GLint loc = glGetUniformLocation(id, name);
    if (loc != -1) {
      glUniform1f(loc, value);
    } else {
      printf("Warning: Uniform %s not found.\n", name);
    }
  }

  void setVec2(const char *name, glm::vec2 value) const noexcept {
    GLint loc = glGetUniformLocation(id, name);
    if (loc != -1) {
      glUniform2f(loc, value.x, value.y);
    } else {
      printf("Warning: Uniform %s not found.\n", name);
    }
  }

  void setVec3(const char *name, glm::vec3 value) const noexcept {
    GLint loc = glGetUniformLocation(id, name);
    if (loc != -1) {
      glUniform3f(loc, value.x, value.y, value.z);
    } else {
      printf("Warning: Uniform %s not found.\n", name);
    }
  }

  void setVec4(const char *name, glm::vec4 value) const noexcept {
    GLint loc = glGetUniformLocation(id, name);
    if (loc != -1) {
      glUniform4f(loc, value.x, value.y, value.z, value.w);
    } else {
      printf("Warning: Uniform %s not found.\n", name);
    }
  }

  void setMat3(const char *name, glm::mat3 value) const noexcept {
    GLint loc = glGetUniformLocation(id, name);
    if (loc != -1) {
      glUniformMatrix3fv(loc, 1, GL_FALSE, glm::value_ptr(value));
    } else {
      printf("Warning: Uniform %s not found.\n", name);
    }
  }

  void setMat4(const char *name, glm::mat4 value) const noexcept {
    GLint loc = glGetUniformLocation(id, name);
    if (loc != -1) {
      glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(value));
    } else {
      printf("Warning: Uniform %s not found.\n", name);
    }
  }

  GLuint getId() const noexcept { return id; }

  ~Shader() noexcept {
    if (id != 0)
      glDeleteProgram(id);
  }

private:
  GLuint id;
};

struct Texture {
  Texture(const Texture &other) = delete;
  Texture &operator=(const Texture &other) = delete;

  Texture() : id(0) {}

  Texture(Texture &&other) {
    id = other.id;
    kind = other.kind;

    other.id = 0;
  }
  Texture &operator=(Texture &&other) = delete;

  Texture(GLenum kind) noexcept : id(0), kind(kind) {
    glGenTextures(1, &id);
    assert(id != 0);
    setMagFilter(GL_LINEAR);
    setMinFilter(GL_LINEAR);
    setWrapS(GL_CLAMP_TO_EDGE);
    setWrapT(GL_CLAMP_TO_EDGE);
    setWrapR(GL_CLAMP_TO_EDGE);
  }

  Texture(GLenum kind, GLsizei width, GLsizei height,
          GLint internalFormat = GL_RGB, GLint format = GL_RGB,
          const unsigned char *data = nullptr, bool mipmap = true) noexcept
      : id(0), kind(kind) {
    glGenTextures(1, &id);
    assert(id != 0);
    setMagFilter(GL_LINEAR);
    setMinFilter(GL_LINEAR);
    setWrapS(GL_CLAMP_TO_EDGE);
    setWrapT(GL_CLAMP_TO_EDGE);
    setWrapR(GL_CLAMP_TO_EDGE);
    bind();
    buffer2DImageU8(width, height, internalFormat, format, data, mipmap);
  }

  void bind(GLint unit = 0) const noexcept {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(kind, id);
  }

  void unbind() const noexcept { glBindTexture(kind, 0); }

  void buffer2DImageU8(GLsizei width, GLsizei height, GLint internalFormat,
                       GLint format, const unsigned char *data,
                       bool mipmap = true,
                       GLenum type = GL_UNSIGNED_BYTE) const noexcept {
    glTexImage2D(kind, 0, internalFormat, width, height, 0, format, type, data);
    if (mipmap) {
      generateMipMap();
    }
  }

  static Texture depth24Stencil8(GLsizei width, GLsizei height) noexcept {
    Texture tx(GL_TEXTURE_2D);
    tx.bind();
    tx.buffer2DImageU8(width, height, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL,
                       nullptr, false, GL_UNSIGNED_INT_24_8);
    return tx;
  }

  static Texture fromFile2D(const char *path, GLint format,
                            GLint internalFormat) {
    Texture tx(GL_TEXTURE_2D);
    tx.bind();

    int width, height, nrChannels;
    unsigned char *data = stbi_load(path, &width, &height, &nrChannels, 0);
    if (data) {
      tx.buffer2DImageU8(width, height, internalFormat, format, data);
    } else {
      std::cout << "Failed to load texture" << std::endl;
    }
    assert(data);
    stbi_image_free(data);

    // tx.setMagFilter(GL_LINEAR);
    // tx.setMinFilter(GL_LINEAR);

    // tx.setWrapR(GL_CLAMP_TO_EDGE);
    // tx.setWrapS(GL_CLAMP_TO_EDGE);

    return tx;
  }

  // • GL_REPEAT: The default behavior for textures. Repeats the texture
  //   image.
  // • GL_MIRRORED_REPEAT: Same as GL_REPEAT but mirrors the image with each
  // repeat. • GL_CLAMP_TO_EDGE: Clamps the coordinates between 0 and 1. The
  // result is that higher coordinates become clamped to the edge, resulting in
  // a stretched edge pattern. • GL_CLAMP_TO_BORDER: Coordinates outside the
  // range are now given a user-specified border color. private:
  void setWrapS(GLint param) const noexcept {
    glTexParameteri(kind, GL_TEXTURE_WRAP_S, param);
  }
  void setWrapT(GLint param) const noexcept {
    glTexParameteri(kind, GL_TEXTURE_WRAP_T, param);
  }
  void setWrapR(GLint param) const noexcept {
    glTexParameteri(kind, GL_TEXTURE_WRAP_R, param);
  }
  // GL_NEAREST (also known as nearest neighbor or point filtering) is the
  // default texture filtering method of OpenGL. When set to GL_NEAREST, OpenGL
  // selects the texel that center is closest to the texture coordinate
  //
  // GL_LINEAR (also known as (bi)linear filtering) takes an interpolated value
  // from the texture coordinate’s neighboring texels, approximating a color
  // between the texels.
  //
  // • GL_NEAREST_MIPMAP_NEAREST: takes the nearest mipmap
  // to match the pixel size and uses nearest neighbor interpolation for texture
  // sampling.
  //
  // • GL_LINEAR_MIPMAP_NEAREST: takes the nearest mipmap level and
  // samples that level using linear interpolation.
  //
  // • GL_NEAREST_MIPMAP_LINEAR:
  // linearly interpolates between the two mipmaps that most closely match the
  // size of a pixel and samples the interpolated level via nearest neighbor
  // interpolation.
  // • GL_LINEAR_MIPMAP_LINEAR: linearly interpolates between the two closest
  // mipmaps and samples the interpolated level via linear interpolation.
  void setMinFilter(GLint param) const noexcept {
    glTexParameteri(kind, GL_TEXTURE_MIN_FILTER, param);
  }
  void setMagFilter(GLint param) const noexcept {
    glTexParameteri(kind, GL_TEXTURE_MAG_FILTER, param);
  }

  GLuint getId() const noexcept { return id; }

  void generateMipMap() const noexcept { glGenerateMipmap(kind); }

  ~Texture() {
    if (id != 0) {
      glDeleteTextures(1, &id);
    }
  }

private:
  GLuint id;
  GLenum kind;
};

struct RenderBuffer {
  RenderBuffer(const RenderBuffer &other) = delete;
  RenderBuffer &operator=(const RenderBuffer &other) = delete;
  RenderBuffer(RenderBuffer &&other) noexcept {
    id = other.id;
    other.id = 0;
  }
  RenderBuffer &operator=(RenderBuffer &&other) = delete;

  RenderBuffer() noexcept : id(0) {
    glGenRenderbuffers(1, &id);
    assert(id != 0);
  }

  void allocStorage(GLsizei width, GLsizei height,
                    GLenum internalFormat = GL_RGB) noexcept {
    glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, width, height);
  }

  void allocStorageMultisample(GLsizei samples, GLsizei width, GLsizei height,
                               GLenum internalFormat = GL_RGB8) noexcept {
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, internalFormat,
                                     width, height);
    glCheckError_(__FILE__, __LINE__);
  }

  static RenderBuffer multisample(GLsizei samples, GLsizei width,
                                  GLsizei height,
                                  GLenum internalFormat = GL_RGB8) {
    RenderBuffer r;
    r.bind();
    r.allocStorageMultisample(samples, width, height);
    return r;
  }

  static RenderBuffer singlesample(GLsizei width, GLsizei height,
                                   GLenum internalFormat = GL_RGB) {
    RenderBuffer r;
    r.bind();
    r.allocStorage(width, height);
    return r;
  }

  void bind() const noexcept { glBindRenderbuffer(GL_RENDERBUFFER, id); }
  void unbind() const noexcept { glBindRenderbuffer(GL_RENDERBUFFER, 0); }

  GLuint getId() const noexcept { return id; }

  ~RenderBuffer() noexcept {
    if (id != 0)
      glDeleteRenderbuffers(1, &id);
  }

private:
  GLuint id;
};

enum FrameBufferKind {
  R = GL_READ_FRAMEBUFFER,
  W = GL_DRAW_FRAMEBUFFER,
  RW = GL_FRAMEBUFFER
};

struct FrameBuffer {
  FrameBuffer(const FrameBuffer &other) = delete;
  FrameBuffer &operator=(const FrameBuffer &other) = delete;

  FrameBuffer(FrameBuffer &&other);
  FrameBuffer &operator=(FrameBuffer &&other) = delete;

  FrameBuffer() : id(0), kind(FrameBufferKind::RW) {
    glGenFramebuffers(1, &id);
    assert(id != 0);
  }

  FrameBuffer(FrameBufferKind kind) : id(0), kind(kind) {
    glGenFramebuffers(1, &id);
    assert(id != 0);
  }

  const char *getError() const noexcept {
    bind();
    const char *error = "UNKNOWN";
    switch (glCheckFramebufferStatus(GL_FRAMEBUFFER)) {
    case GL_FRAMEBUFFER_COMPLETE:
      error = "GL_FRAMEBUFFER_COMPLETE";
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
      error = "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
      error = "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS";
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
      error = "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
      error = "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
      break;
    case GL_FRAMEBUFFER_UNDEFINED:
      error = "GL_FRAMEBUFFER_UNDEFINED";
      break;
    case GL_FRAMEBUFFER_UNSUPPORTED:
      error = "GL_FRAMEBUFFER_UNSUPPORTED";
      break;
    }
    return error;
  }

  bool isComplete() const noexcept {
    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
  }

  void bind() const noexcept { glBindFramebuffer(kind, id); }
  void unbind() const noexcept { glBindFramebuffer(kind, 0); }

  void attachColorTexture2D(Texture &tx, unsigned int pos = 0) const noexcept {
    tx.bind();
    glFramebufferTexture2D(kind, GL_COLOR_ATTACHMENT0 + pos, GL_TEXTURE_2D,
                           tx.getId(), 0);
  }

  void attachDepth24Stensil8(Texture &tx) const noexcept {
    tx.bind();
    glFramebufferTexture2D(kind, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                           tx.getId(), 0);
  }

  void attachColorRenderBuffer(RenderBuffer &rb,
                               unsigned int pos = 0) noexcept {
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + pos,
                              GL_RENDERBUFFER, rb.getId());
  }

  void attachDepthStensilRenderBuffer(RenderBuffer &rb) noexcept {
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, rb.getId());
  }

  void blit(GLuint destId, GLint width, GLint height) const noexcept {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, id);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destId);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
  }

  GLuint getId() const noexcept { return id; }

  ~FrameBuffer() {
    if (id != 0)
      glDeleteFramebuffers(1, &id);
  }

private:
  GLuint id;
  FrameBufferKind kind = FrameBufferKind::RW;
};

struct CstrHash {
  size_t operator()(const char *a) const {
    return std::hash<std::string_view>{}(a);
  }
};
struct CstrEq {
  bool operator()(const char *a, const char *b) const {
    return strcmp(a, b) == 0;
  }
};

struct ResourceManager {
  static Shader &loadShader(const char *name, const char *vsPath,
                            const char *fsPath) {
    shaders.try_emplace(name, Shader::fromFile(vsPath, fsPath));
    return shaders.at(name);
  }

  static Shader &getShader(const char *name) { return shaders.at(name); }

  static Texture &loadTexture2D(const char *name, const char *path,
                                GLint format, GLint internalFormat) {
    textures.try_emplace(name,
                         Texture::fromFile2D(path, format, internalFormat));
    return textures.at(name);
  }

  static Texture &getTexture(const char *name) { return textures.at(name); }

private:
  inline static std::unordered_map<const char *, Shader, CstrHash, CstrEq>
      shaders;
  inline static std::unordered_map<const char *, Texture, CstrHash, CstrEq>
      textures;
};

struct SpriteRenderer {
  SpriteRenderer(const SpriteRenderer &other) = delete;
  SpriteRenderer &operator=(const SpriteRenderer &other) = delete;

  SpriteRenderer(SpriteRenderer &&other) = delete;
  SpriteRenderer &operator=(SpriteRenderer &&other) = delete;

  SpriteRenderer(Shader &shader) noexcept : shader(shader) {
    vao.bind();
    vbo.bind();
    float vertices[] = {// pos // tex
                        0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f,
                        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
                        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f};

    vbo.bufferStaticData<float>(vertices, sizeof(vertices) / sizeof(float));
    vbo.pushFloatAttribute(4, 4 * 4);
  }

  void draw(Texture *texture, glm::vec2 position,
            glm::vec2 size = glm::vec2(10.0f, 10.0f), float rotate = 0.0f,
            glm::vec3 color = glm::vec3(1.0)) {
    vao.bind();
    shader.bind();

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(position, 0.0f));
    model = glm::translate(model, glm::vec3(0.5 * size.x, 0.5 * size.y, 0.0));
    model = glm::rotate(model, glm::radians(rotate), glm::vec3(0.0, 0.0, 1.0));
    model = glm::translate(model, glm::vec3(-0.5 * size.x, -0.5 * size.y, 0.0));
    model = glm::scale(model, glm::vec3(size, 1.0f));
    shader.setMat4("model", model);
    shader.setVec3("spriteColor", color);

    texture->bind(0);

    glDrawArrays(GL_TRIANGLES, 0, 6);
  }

private:
  Vao vao;
  Vbo vbo;
  Shader &shader;
};

struct GameObject {
  GameObject(glm::vec2 pos, glm::vec2 size, glm::vec3 color, Texture *texture,
             glm::vec2 velocity = glm::vec2(0.0f), bool solid = true) noexcept
      : position(pos), size(size), color(color), texture(texture),
        velocity(velocity), solid(solid) {}

  GameObject() = default;

  GameObject(const GameObject &other) = delete;
  GameObject &operator=(const GameObject &other) = delete;

  GameObject(GameObject &&other) : texture(other.texture) {
    position = other.position;
    size = other.size;
    color = other.color;
    velocity = other.velocity;

    solid = other.solid;
    destroyed = other.destroyed;
  }
  GameObject &operator=(GameObject &&other) {
    texture = other.texture;

    position = other.position;
    size = other.size;
    color = other.color;
    velocity = other.velocity;

    solid = other.solid;
    destroyed = other.destroyed;

    return *this;
  }

  glm::vec2 position;
  glm::vec2 size;
  Texture *texture;
  glm::vec3 color;
  glm::vec2 velocity;

  bool solid = false;
  bool destroyed = false;

  void draw(SpriteRenderer &renderer) const noexcept {
    if (!destroyed)
      renderer.draw(texture, position, size, 0.0f, color);
  }
};

struct Ball : GameObject {
  bool stuck, sticky, passThrough;
  float radius;

  Ball(glm::vec2 pos, float radius, Texture *texture,
       glm::vec2 velocity = glm::vec2(0.0f))
      : GameObject(pos, glm::vec2(radius * 2, radius * 2), glm::vec3(1.0),
                   texture, velocity, false),
        stuck(true), sticky(false), passThrough(false), radius(radius) {}

  glm::vec2 move(float dt, unsigned int window_width) {
    // if not stuck to player board
    if (!stuck) {
      // move the ball
      position += velocity * dt;
      // then check if outside window bounds and if so, reverse velocity and
      // restore at correct position
      if (position.x <= 0.0f) {
        velocity.x = -velocity.x;
        position.x = 0.0f;
      } else if (position.x + size.x >= window_width) {
        velocity.x = -velocity.x;
        position.x = window_width - this->size.x;
      }
      if (position.y <= 0.0f) {
        velocity.y = -velocity.y;
        position.y = 0.0f;
      }
    }
    return position;
  }
};

template <size_t W> struct LevelRow {
  unsigned char blocks[W];
};

// template <size_t W, size_t H>
struct GameLevel {
  GameLevel() = default;
  GameLevel(size_t W, size_t H, unsigned int lvlWidth = 16,
            unsigned int lvlHeight = 8) {
    float unit_width = static_cast<float>(W) / lvlWidth;
    float unit_height = static_cast<float>(H) / lvlHeight;

    for (size_t i = 0; i < lvlHeight; i++) {
      // LevelRow<W> row;
      for (size_t j = 0; j < lvlWidth; j++) {
        glm::vec3 color = glm::vec3(1.0f);
        ;
        const char *texture_name = "block";
        bool solid = false;

        switch (rand() % 6) {
        case 0:
          texture_name = "block_solid";
          solid = true;
          color = glm::vec3(0.8f, 0.8f, 0.7f);
          break;
        case 1:
          break;
        case 2:
          color = glm::vec3(0.2f, 0.6f, 1.0f);
          break;
        case 3:
          color = glm::vec3(0.0f, 0.7f, 0.0f);
          break;
        case 4:
          color = glm::vec3(0.8f, 0.8f, 0.4f);
          break;
        case 5:
          color = glm::vec3(1.0f, 0.5f, 0.0f);
          break;
        }
        glm::vec2 pos(unit_width * j, unit_height * i);
        glm::vec2 size(unit_width, unit_height);
        bricks.push_back(GameObject(pos, size, color,
                                    &ResourceManager::getTexture(texture_name),
                                    glm::vec2(0.0), solid));
      }
    }
  }

  GameLevel(const char *path, size_t W, size_t H) {
    PtrWrap pw = readFile(path);
    const char *buf = (const char *)pw.ptr;
    size_t i = 0, j = 0;
    size_t lvlHeight = 0;
    size_t lvlWidth = 0;
    for (i = 0; buf[i + j] != '\0'; i++) {
      for (j = 0; buf[i + j] != '\n'; j++)
        ;
      if (i == 0) {
        lvlWidth = j;
      } else {
        assert(lvlWidth == j);
      }
      i += j;
      j = 0;
      lvlHeight += 1;
    }

    float unit_width = static_cast<float>(W) / lvlWidth;
    float unit_height = static_cast<float>(H) / lvlHeight;

    i = 0, j = 0;
    size_t k;
    for (k = 0; buf[k + j] != '\0'; k++) {
      for (j = 0; buf[k + j] != '\n' && buf[k + j] != '\0'; j++) {
        glm::vec3 color = glm::vec3(1.0f);
        const char *texture_name = "block";
        bool solid = false;
        switch (buf[k + j] - '0') {
        case '\n' - '0':
          break;
        case 0:
          continue;
          break;
        case 1:
          texture_name = "block_solid";
          solid = true;
          color = glm::vec3(0.8f, 0.8f, 0.7f);
          break;
        case 2:
          color = glm::vec3(0.2f, 0.6f, 1.0f);
          break;
        case 3:
          color = glm::vec3(0.0f, 0.7f, 0.0f);
          break;
        case 4:
          color = glm::vec3(0.8f, 0.8f, 0.4f);
          break;
        case 5:
          color = glm::vec3(1.0f, 0.5f, 0.0f);
          break;
        default:
          assert(false);
        }
        glm::vec2 pos(unit_width * j, unit_height * i);
        glm::vec2 size(unit_width, unit_height);
        bricks.push_back(GameObject(pos, size, color,
                                    &ResourceManager::getTexture(texture_name),
                                    glm::vec2(0.0), solid));
      }
      k += j;
      i += 1;
    }
  }

  void draw(SpriteRenderer &renderer) const noexcept {
    for (auto &brick : bricks) {
      brick.draw(renderer);
    }
  }

  bool isCompleted() {
    for (GameObject &tile : bricks)
      if (!tile.solid && !tile.destroyed)
        return false;
    return true;
  }
  std::vector<GameObject> bricks;
};

enum GameState { GAME_ACTIVE, GAME_MENU, GAME_WIN };
enum Direction { UP, RIGHT, DOWN, LEFT };

Direction VectorDirection(glm::vec2 target) {
  glm::vec2 compass[] = {
      glm::vec2(0.0f, 1.0f),  // up
      glm::vec2(1.0f, 0.0f),  // right
      glm::vec2(0.0f, -1.0f), // down
      glm::vec2(-1.0f, 0.0f)  // left
  };
  float max = 0.0f;
  unsigned int best_match = -1;
  for (unsigned int i = 0; i < 4; i++) {
    float dot_product = glm::dot(glm::normalize(target), compass[i]);
    if (dot_product > max) {
      max = dot_product;
      best_match = i;
    }
  }
  return (Direction)best_match;
}

typedef std::tuple<bool, Direction, glm::vec2> Collision;

Collision CheckCollision(Ball &one, GameObject &two) // AABB - Circle
{
  // get center point circle first
  glm::vec2 center(one.position + one.radius);
  // calculate AABB info (center, half-extents)
  glm::vec2 aabb_half_extents(two.size.x / 2.0f, two.size.y / 2.0f);
  glm::vec2 aabb_center(two.position.x + aabb_half_extents.x,
                        two.position.y + aabb_half_extents.y);
  // get difference vector between both centers
  glm::vec2 difference = center - aabb_center;
  glm::vec2 clamped =
      glm::clamp(difference, -aabb_half_extents, aabb_half_extents);
  // add clamped value to AABB_center and get the value closest to circle
  glm::vec2 closest = aabb_center + clamped;
  // vector between center circle and closest point AABB
  difference = closest - center;

  if (glm::length(difference) <= one.radius)
    return std::make_tuple(true, VectorDirection(difference), difference);
  else
    return std::make_tuple(false, UP, glm::vec2(0.0f, 0.0f));
}

bool CheckCollision(GameObject &one, GameObject &two) // AABB - AABB
{
  // collision x-axis?
  bool collisionX = one.position.x + one.size.x >= two.position.x &&
                    two.position.x + two.size.x >= one.position.x;
  // collision y-axis?
  bool collisionY = one.position.y + one.size.y >= two.position.y &&
                    two.position.y + two.size.y >= one.position.y;
  // collision only if on both axes
  return collisionX && collisionY;
}

struct Particle {
  glm::vec2 position, velocity;
  glm::vec4 color;
  float life;

  Particle() : position(0.0f), velocity(0.0f), color(1.0f), life(0.0f) {}
};

struct ParticleGen {
  std::vector<Particle> particles;
  Vao vao;
  Vbo vbo;
  unsigned int lastUsedParticle = 0;
  Shader &shader;
  Texture &texture;

  ParticleGen(Shader &shader, Texture &texture, size_t amount)
      : shader(shader), texture(texture) {
    for (size_t i = 0; i < amount; i++) {
      particles.push_back(Particle());
    }
    vao.bind();
    vbo.bind();
    float vertices[] = {// pos // tex
                        0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f,
                        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
                        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f};

    vbo.bufferStaticData<float>(vertices, sizeof(vertices) / sizeof(float));
    vbo.pushFloatAttribute(4, 4 * 4);
    shader.bind();
  }

  void update(float dt, GameObject &object, glm::vec2 offset) {
    unsigned int nr_new_particles = 2;
    // add new particles
    for (unsigned int i = 0; i < nr_new_particles; ++i) {
      int unusedParticle = FirstUnusedParticle();
      RespawnParticle(particles[unusedParticle], object, offset);
    }
    // update all particles
    for (unsigned int i = 0; i < particles.size(); ++i) {
      Particle &p = particles[i];
      p.life -= dt;        // reduce life
      if (p.life > 0.0f) { // particle is alive, thus update
        p.position -= p.velocity * dt;
        p.color.a -= dt * 2.5f;
      }
    }
  }

  void RespawnParticle(Particle &particle, GameObject &object,
                       glm::vec2 offset) {
    float random = ((rand() % 100) - 50) / 10.0f;
    float rColor = 0.5f + ((rand() % 100) / 100.0f);
    particle.position = object.position + random + offset;
    particle.color = glm::vec4(rColor, rColor, rColor, 1.0f);
    particle.life = 1.0f;
    particle.velocity = object.velocity * 0.1f;
  }

  void draw() {
    vao.bind();
    shader.bind();
    texture.bind(0);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    for (auto &p : particles) {
      if (p.life > 0.0f) {
        shader.setVec2("offset", p.position);
        shader.setVec4("color", p.color);
        glDrawArrays(GL_TRIANGLES, 0, 6);
      }
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  }

  void reset() {
    for (auto &p : particles) {
      p = Particle();
    }
  }

  unsigned int FirstUnusedParticle() {
    // search from last used particle, often returns almost instantly
    for (unsigned int i = lastUsedParticle; i < particles.size(); ++i) {
      if (particles[i].life <= 0.0f) {
        lastUsedParticle = i;
        return i;
      }
    }
    // otherwise, do a linear search
    for (unsigned int i = 0; i < lastUsedParticle; ++i) {
      if (particles[i].life <= 0.0f) {
        lastUsedParticle = i;
        return i;
      }
    }
    // override first particle if all others are alive
    lastUsedParticle = 0;
    return 0;
  }
};

struct PostProcessor {
  PostProcessor(const PostProcessor &other) = delete;
  PostProcessor &operator=(const PostProcessor &other) = delete;

  PostProcessor(PostProcessor &&other);
  PostProcessor &operator=(PostProcessor &&other) = delete;

  PostProcessor(Shader &shader, GLsizei width, GLsizei height) noexcept
      : width(width), height(height), shader(shader),
        msaa(RenderBuffer::multisample(4, width, height)),
        normal_color(Texture(GL_TEXTURE_2D, width, height)) {
    vao.bind();
    vbo.bind();
    shader.bind();
    shader.setInt("scene", 0);
    float offset = 1.0f / 300.0f;
    float offsets[9][2] = {
        {-offset, offset},  // top-left
        {0.0f, offset},     // top-center
        {offset, offset},   // top-right
        {-offset, 0.0f},    // center-left
        {0.0f, 0.0f},       // center-center
        {offset, 0.0f},     // center - right
        {-offset, -offset}, // bottom-left
        {0.0f, -offset},    // bottom-center
        {offset, -offset}   // bottom-right
    };
    glUniform2fv(glGetUniformLocation(shader.getId(), "offsets"), 9,
                 (float *)offsets);
    int edge_kernel[9] = {-1, -1, -1, -1, 8, -1, -1, -1, -1};
    glUniform1iv(glGetUniformLocation(shader.getId(), "edge_kernel"), 9,
                 edge_kernel);
    float blur_kernel[9] = {1.0f / 16.0f, 2.0f / 16.0f, 1.0f / 16.0f,
                            2.0f / 16.0f, 4.0f / 16.0f, 2.0f / 16.0f,
                            1.0f / 16.0f, 2.0f / 16.0f, 1.0f / 16.0f};
    glUniform1fv(glGetUniformLocation(shader.getId(), "blur_kernel"), 9,
                 blur_kernel);

    fbo.bind();
    msaa.bind();

    fbo.attachColorRenderBuffer(msaa);

    assert(fbo.isComplete());
    msaa.unbind();
    fbo.unbind();

    normal.bind();
    normal_color.bind();

    normal.attachColorTexture2D(normal_color);

    assert(normal.isComplete());
    normal_color.unbind();
    normal.unbind();

    float vertices[] = {// pos        // tex
                        -1.0f, -1.0f, 0.0f,  0.0f, 1.0f, 1.0f,
                        1.0f,  1.0f,  -1.0f, 1.0f, 0.0f, 1.0f,

                        -1.0f, -1.0f, 0.0f,  0.0f, 1.0f, -1.0f,
                        1.0f,  0.0f,  1.0f,  1.0f, 1.0f, 1.0f};
    vbo.bufferStaticData<float>(vertices, sizeof(vertices) / sizeof(float));
    vbo.pushFloatAttribute(4, 4 * 4);
  }

  void beginRender() noexcept { fbo.bind(); }

  void endRender() noexcept {
    fbo.blit(normal.getId(), width, height);
    fbo.unbind();
  }

  void render(float time) noexcept {
    vao.bind();
    vbo.bind();
    shader.bind();

    shader.setFloat("time", time);
    shader.setInt("confuse", confuse);
    shader.setInt("chaos", chaos);
    shader.setInt("shake", shake);

    normal_color.bind();
    glDrawArrays(GL_TRIANGLES, 0, 6);
  }

  void reset() noexcept {
    confuse = false;
    chaos = false;
    shake = false;
  }

  bool confuse = false;
  bool chaos = false;
  bool shake = false;

private:
  GLsizei width, height;
  Shader &shader;
  Vao vao;
  Vbo vbo;
  FrameBuffer fbo;
  RenderBuffer msaa;
  FrameBuffer normal;
  Texture normal_color;
};

struct Character {
  Texture tx;           // ID handle of the glyph texture
  glm::ivec2 Size;      // Size of glyph
  glm::ivec2 Bearing;   // Offset from baseline to left/top of glyph
  unsigned int Advance; // Offset to advance to next glyph

  Character() = default;

  Character(const Character &) = delete;
  Character(Character &&other) : tx(std::move(other.tx)) {
    Size = other.Size;
    Bearing = other.Bearing;
    Advance = other.Advance;
  }

  Character &operator=(const Character &) = delete;
  Character &operator=(Character &&other) = delete;

  Character(Texture tx, glm::ivec2 size, glm::ivec2 bearing,
            unsigned int advance)
      : tx(std::move(tx)), Size(size), Bearing(bearing), Advance(advance) {}

  void print() const noexcept {
    printf("CHAR: Size(%d, %d), Bearing(%d, %d), Advance(%d)\n", Size.x, Size.y,
           Bearing.x, Bearing.y, Advance);
  }
};

struct FontError : std::exception {
  const char *msg;
  explicit FontError(const char *msg = "") : msg(msg) {}
  const char *what() const noexcept override { return msg; }
};

struct Font {
  std::unordered_map<char, Character> characters;
  Font(const Font &) = delete;
  Font(Font &&) = delete;

  Font &operator=(const Font &) = delete;
  Font &operator=(Font &&) = delete;

  Font(const char *path, Shader &s) : vao(), vbo(6 * 4 * 4), shader(s) {
    assert(path == nullptr);
    FT_Library ft;
    if (FT_Init_FreeType(&ft) != 0) {
      throw FontError("init");
    }
    FT_Face face;
    if (FT_New_Face(ft, path, 0, &face) != 0) {
      throw FontError("face");
    }

    if (FT_Set_Pixel_Sizes(face, 0, 48) != 0) {
      throw FontError("pixel_size");
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // no byte-alignment restriction

    for (unsigned char c = 0; c < 128; c++) {
      if (FT_Load_Char(face, c, FT_LOAD_RENDER) != 0) {
        throw FontError("load");
      }
      characters.emplace(
          c,
          Character(
              Texture(GL_TEXTURE_2D, face->glyph->bitmap.width,
                      face->glyph->bitmap.rows, GL_RED, GL_RED,
                      face->glyph->bitmap.buffer, false),
              glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
              glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
              face->glyph->advance.x));
    }
    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    vao.bind();
    vbo.bind();
    vbo.pushFloatAttribute(4, 4 * 4);
    by = characters['H'].Bearing.y;
  }

  void drawText(std::string_view text, float x, float y, float scale,
                glm::vec3 color = glm::vec3(0.3, 0.7f, 0.9f)) {
    vao.bind();
    vbo.bind();

    shader.bind();
    shader.setVec3("textColor", color);
    // fgetc(stdin);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (size_t i = 0; i < text.size(); i++) {
      Character &ch = characters[text[i]];
      float xpos = x + ch.Bearing.x * scale;
      float ypos = y + (by - ch.Bearing.y) * scale;
      float w = ch.Size.x * scale;
      float h = ch.Size.y * scale;
      float vertices[6][4] = {
          {xpos, ypos + h, 0.0f, 1.0f},     {xpos + w, ypos, 1.0f, 0.0f},
          {xpos, ypos, 0.0f, 0.0f},         {xpos, ypos + h, 0.0f, 1.0f},
          {xpos + w, ypos + h, 1.0f, 1.0f}, {xpos + w, ypos, 1.0f, 0.0f}};
      vbo.bufferSubData<float>((float *)vertices,
                               sizeof(vertices) / sizeof(float));
      ch.tx.bind();
      glDrawArrays(GL_TRIANGLES, 0, 6);
      x += (ch.Advance >> 6) * scale; // bitshift by 6 (2^6 = 64)
    }

    glCheckError_(__FILE__, __LINE__);
  }

private:
  Shader &shader;
  Vao vao;
  Vbo vbo;
  float by;
};

struct Game {
  const glm::vec2 PLAYER_SIZE = glm::vec2(100.0f, 20.0f);
  const float PLAYER_VELOCITY = (500.0f);
  // Initial velocity of the Ball
  const glm::vec2 INITIAL_BALL_VELOCITY = glm::vec2(100.0f, -350.0f);
  // Radius of the ball object
  const float BALL_RADIUS = 12.5f;

  GameState State;
  bool Keys[1024];
  bool KeysProcessed[1024];
  unsigned int Width, Height;
  SpriteRenderer *renderer;
  GameLevel levels[4];
  GameObject *player;
  Ball *ball;
  ParticleGen *particles;
  PostProcessor *effects;
  float ShakeTime = 0.0f;
  Font *font;
  unsigned int Lives;
  size_t level;
  // constructor/destructor
  Game() = delete;
  Game(unsigned int width, unsigned int height) noexcept
      : Width(width), Height(height), State(GAME_MENU), Lives(3), level(0) {}
  ~Game() noexcept {

  };
  // initialize game state (load all shaders/textures/levels)
  void Init() {
    memset(&Keys, 0, sizeof Keys);
    memset(&KeysProcessed, 0, sizeof KeysProcessed);

    ResourceManager::loadShader("sprite", "shaders/sprite_v.glsl",
                                "shaders/sprite_f.glsl");
    ResourceManager::loadShader("particle", "shaders/particle_v.glsl",
                                "shaders/particle_f.glsl");
    ResourceManager::loadShader("postprocessing",
                                "shaders/postprocessing_v.glsl",
                                "shaders/postprocessing_f.glsl");
    ResourceManager::loadShader("font", "shaders/font_v.glsl",
                                "shaders/font_f.glsl");
    glm::mat4 proj = glm::ortho(0.0f, static_cast<float>(Width),
                                static_cast<float>(Height), 0.0f, -1.0f, 1.0f);
    ResourceManager::getShader("sprite").bind().setInt("image", 0);
    ResourceManager::getShader("sprite").setMat4("projection", proj);

    ResourceManager::getShader("font").bind().setInt("text", 0);
    ResourceManager::getShader("font").setMat4("projection", proj);

    ResourceManager::getShader("particle").bind().setInt("sprite", 0);
    ResourceManager::getShader("particle").setMat4("projection", proj);

    renderer = new SpriteRenderer(ResourceManager::getShader("sprite"));

    ResourceManager::loadTexture2D("face", "resources/textures/awesomeface.png",
                                   GL_RGBA, GL_RGBA);
    ResourceManager::loadTexture2D(
        "background", "resources/textures/background.jpg", GL_RGB, GL_RGB);
    ResourceManager::loadTexture2D("block", "resources/textures/block.png",
                                   GL_RGB, GL_RGB);
    ResourceManager::loadTexture2D(
        "block_solid", "resources/textures/block_solid.png", GL_RGB, GL_RGB);
    Texture &paddle = ResourceManager::loadTexture2D("paddle",
                                                     "resources/textures/"
                                                     "paddle.png",
                                                     GL_RGBA, GL_RGBA);
    ResourceManager::loadTexture2D(
        "particle", "resources/textures/particle.png", GL_RGBA, GL_RGBA);
    //===powerup
    ResourceManager::loadTexture2D(
        "chaos", "resources/textures/powerup_chaos.png", GL_RGBA, GL_RGBA);

    ResourceManager::loadTexture2D("increase",
                                   "resources/textures/powerup_increase.png",
                                   GL_RGBA, GL_RGBA);

    ResourceManager::loadTexture2D(
        "confuse", "resources/textures/powerup_confuse.png", GL_RGBA, GL_RGBA);

    ResourceManager::loadTexture2D("passthrough",
                                   "resources/textures/powerup_passthrough.png",
                                   GL_RGBA, GL_RGBA);

    ResourceManager::loadTexture2D(
        "speed", "resources/textures/powerup_speed.png", GL_RGBA, GL_RGBA);

    ResourceManager::loadTexture2D(
        "sticky", "resources/textures/powerup_sticky.png", GL_RGBA, GL_RGBA);

    levels[0] = GameLevel("resources/levels/one.lvl", Width, Height / 2);
    levels[1] = GameLevel("resources/levels/two.lvl", Width, Height / 2);
    levels[2] = GameLevel("resources/levels/three.lvl", Width, Height / 2);
    levels[3] = GameLevel("resources/levels/four.lvl", Width, Height / 2);

    glm::vec2 playerPos =
        glm::vec2(Width / 2.0f - PLAYER_SIZE.x / 2.0f, Height - PLAYER_SIZE.y);
    player = new GameObject(playerPos, PLAYER_SIZE, glm::vec3(1.0), &paddle,
                            glm::vec2(PLAYER_VELOCITY, 0));

    glm::vec2 ballPos =
        playerPos +
        glm::vec2(PLAYER_SIZE.x / 2.0f - BALL_RADIUS, -BALL_RADIUS * 2.0f);
    ball = new Ball(ballPos, BALL_RADIUS, &ResourceManager::getTexture("face"),
                    INITIAL_BALL_VELOCITY);

    particles = new ParticleGen(ResourceManager::getShader("particle"),
                                ResourceManager::getTexture("particle"), 500);

    effects = new PostProcessor(ResourceManager::getShader("postprocessing"),
                                Width, Height);
    try {
      font = new Font("resources/fonts/"
                      "Antonio-Bold.ttf",
                      ResourceManager::getShader("font"));
    } catch (FontError &fe) {
      printf("Load font error: %s\n", fe.what());
      abort();
    }
  }

  static bool ShouldSpawn(unsigned int chance) noexcept {
    unsigned int random = rand() % chance;
    return random == 0;
  }

  // game loop
  void ProcessInput(float dt) {
    if (State == GAME_ACTIVE) {
      float velocity = player->velocity.x * dt;
      // move playerboard
      if (Keys[GLFW_KEY_A] || Keys[GLFW_KEY_LEFT]) {
        if (player->position.x >= 0.0f) {
          player->position.x -= velocity;
          if (ball->stuck)
            ball->position.x -= velocity;
        }
      }
      if (Keys[GLFW_KEY_D] || Keys[GLFW_KEY_RIGHT]) {
        if (player->position.x <= Width - player->size.x) {
          player->position.x += velocity;
          if (ball->stuck)
            ball->position.x += velocity;
        }
      }
      if (Keys[GLFW_KEY_SPACE])
        ball->stuck = false;
    }

    if (State == GAME_MENU) {
      if (Keys[GLFW_KEY_ENTER] && !KeysProcessed[GLFW_KEY_ENTER]) {
        State = GAME_ACTIVE;
        KeysProcessed[GLFW_KEY_ENTER] = true;
      }
      if (Keys[GLFW_KEY_W] && !KeysProcessed[GLFW_KEY_W]) {
        level = (level + 1) % 4;
        KeysProcessed[GLFW_KEY_W] = true;
      }
      if (Keys[GLFW_KEY_S] && !KeysProcessed[GLFW_KEY_S]) {
        if (level > 0)
          --level;
        else
          level = 3;
        KeysProcessed[GLFW_KEY_S] = true;
      }
    }

    if (State == GAME_WIN) {
      if (Keys[GLFW_KEY_ENTER]) {
        KeysProcessed[GLFW_KEY_ENTER] = true;
        effects->chaos = false;
        State = GAME_MENU;
      }
    }
  }

  void resetPlayer() {
    delete player;
    delete ball;

    glm::vec2 playerPos =
        glm::vec2(Width / 2.0f - PLAYER_SIZE.x / 2.0f, Height - PLAYER_SIZE.y);
    player = new GameObject(playerPos, PLAYER_SIZE, glm::vec3(1.0),
                            &ResourceManager::getTexture("paddle"),
                            glm::vec2(PLAYER_VELOCITY, 0));

    glm::vec2 ballPos =
        playerPos +
        glm::vec2(PLAYER_SIZE.x / 2.0f - BALL_RADIUS, -BALL_RADIUS * 2.0f);
    ball = new Ball(ballPos, BALL_RADIUS, &ResourceManager::getTexture("face"),
                    INITIAL_BALL_VELOCITY);
    particles->reset();
  }

  void resetLevel() {
    for (auto &brick : levels[level].bricks) {
      brick.destroyed = false;
    }
    resetPlayer();
    effects->reset();
    Lives = 3;
  }

  void Update(float dt) {
    ball->move(dt, Width);
    doCollisions();

    if (State == GAME_ACTIVE && levels[level].isCompleted()) {
      resetLevel();
      resetPlayer();
      effects->chaos = true;
      State = GAME_WIN;
    }

    if (ball->position.y >= Height) // did ball reach bottom edge?
    {
      --Lives;
      // did the player lose all his lives? : Game over
      if (Lives == 0) {
        resetLevel();
        State = GAME_MENU;
      }
      resetPlayer();
    }

    if (!ball->stuck)
      particles->update(dt, *ball, glm::vec2(ball->radius / 2.0f));

    if (ShakeTime > 0.0f) {
      ShakeTime -= dt;
      if (ShakeTime <= 0.0f)
        effects->shake = false;
    }
  }

  void Render() {
    float color = cos(glfwGetTime());
    if (State == GAME_ACTIVE || State == GAME_MENU || State == GAME_WIN) {
      effects->beginRender();
      renderer->draw(
          // font->characters['B'].tx,
          &ResourceManager::getTexture("background"), glm::vec2(0.0f, 0.0f),
          glm::vec2(Width, Height), 0.0f);
      levels[level].draw(*renderer);
      player->draw(*renderer);
      particles->draw();
      ball->draw(*renderer);
      // if (State == GAME_ACTIVE) {
      char buf[32];
      sprintf(buf, "Lives: %d", Lives);
      font->drawText(buf, 5.0f, 5.0f, 0.5f, glm::vec3(color));
      // }
      effects->endRender();
      effects->render(glfwGetTime());
    }
    if (State == GAME_MENU) {
      font->drawText("Press ENTER to start", 250.0f, Height / 2.0 + 20.0, 1.0f,
                     glm::vec3(1.0 - color));
      font->drawText("Press W or S to select level", 300.0f,
                     Height / 2.0 + 70.0f, 0.5f, glm::vec3(color));
    }

    if (State == GAME_WIN) {
      font->drawText("You WON!!!", 320.0, Height / 2.0, 1.0,
                     glm::vec3(0.0, 1.0, 0.0));
      font->drawText("Press ENTER to retry or ESC to quit", 130.0,
                     Height / 2.0 + 50, 1.0, glm::vec3(1.0, 1.0, 0.0));
    }
  }

  void doCollisions() {
    for (GameObject &box : levels[level].bricks) {
      if (!box.destroyed && !(ball->passThrough && box.solid)) {
        Collision collision = CheckCollision(*ball, box);
        if (std::get<0>(collision)) // if collision is true
        {
          // destroy block if not solid
          if (!box.solid) {
            box.destroyed = true;
          } else { // if block is solid, enable shake effect
            ShakeTime = 0.05f;
            effects->shake = true;
          }
          // collision resolution
          Direction dir = std::get<1>(collision);
          glm::vec2 diff_vector = std::get<2>(collision);
          if (dir == LEFT || dir == RIGHT) // horizontal collision
          {
            ball->velocity.x = -ball->velocity.x; // reverse
            // relocate
            float penetration = ball->radius - std::abs(diff_vector.x);
            if (dir == LEFT)
              ball->position.x += penetration; // move right
            else
              ball->position.x -= penetration; // move left;
          } else                               // vertical collision
          {
            ball->velocity.y = -ball->velocity.y; // reverse
            // relocate
            float penetration = ball->radius - std::abs(diff_vector.y);
            if (dir == UP)
              ball->position.y -= penetration; // move up
            else
              ball->position.y += penetration; // move down
          }
        }

        Collision result = CheckCollision(*ball, *player);
        if (!ball->stuck && std::get<0>(result)) {
          // check where it hit the board, and change velocity
          float centerBoard = player->position.x + player->size.x / 2.0f;
          float distance = (ball->position.x + ball->radius) - centerBoard;
          float percentage = distance / (player->size.x / 2.0f);
          // then move accordingly
          float strength = 2.0f;
          glm::vec2 oldVelocity = ball->velocity;
          ball->velocity.x = INITIAL_BALL_VELOCITY.x * percentage * strength;
          // ball->velocity.y = -ball->velocity.y;
          ball->velocity.y = -1.0f * abs(ball->velocity.y);
          ball->velocity =
              glm::normalize(ball->velocity) * glm::length(oldVelocity);

          ball->stuck = ball->sticky;
        }
      }
    }
  }
};

void key_callback(GLFWwindow *window, int key, int scancode, int action,
                  int mode);
void framebuffer_size_callback(GLFWwindow *window, int width, int height);

// Main code
int main(int, char **) {
  srand(time(nullptr));

  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit())
    return 1;

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
  // GL ES 2.0 + GLSL 100 (WebGL 1.0)
  const char *glsl_version = "#version 100";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(IMGUI_IMPL_OPENGL_ES3)
  // GL ES 3.0 + GLSL 300 es (WebGL 2.0)
  const char *glsl_version = "#version 300 es";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
  // GL 3.2 + GLSL 150
  const char *glsl_version = "#version 150";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE,
                 GLFW_OPENGL_CORE_PROFILE);            // 3.2+ only
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Required on Mac
#else
  // GL 3.0 + GLSL 130
  const char *glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  // // 3.2+ only glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  // // 3.0+ only
#endif

  // Create window with graphics context
  float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(
      glfwGetPrimaryMonitor()); // Valid on GLFW 3.3+ only
  GLFWwindow *window =
      glfwCreateWindow((int)(800 * main_scale), (int)(600 * main_scale),
                       "Dear ImGui GLFW+OpenGL3 example", nullptr, nullptr);
  if (window == nullptr)
    return 1;
  glfwMakeContextCurrent(window);
  // glfwSwapInterval(1); // Enable vsync

  if (!gladLoadGLES2((GLADloadfunc)glfwGetProcAddress)) {
    std::cout << "Failed to initialize GLAD" << std::endl;
    return -1;
  }
  // ResourceManager::loadShader("font", "shaders/font_v.glsl",
  //                             "shaders/font_f.glsl");
  // Font f("resources/fonts/"
  //        "Antonio-Bold.ttf",
  //        ResourceManager::getShader("font"));
  // printf("Chars = %ld\n", f.characters.size());
  // return 0;

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  ImGui::StyleColorsLight();

  // Setup scaling
  ImGuiStyle &style = ImGui::GetStyle();
  style.ScaleAllSizes(
      main_scale); // Bake a fixed style scale. (until we have a solution
                   // for dynamic style scaling, changing this requires
                   // resetting Style + calling this again)
  style.FontScaleDpi = main_scale; // Set initial font scale. (in docking
                                   // branch: using io.ConfigDpiScaleFonts=true
                                   // automatically overrides this for every
                                   // window depending on the current monitor)
  style.WindowRounding = 10.0;

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
  ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
#endif
  ImGui_ImplOpenGL3_Init(glsl_version);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  // Our state
  ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.00f);
  int display_w, display_h;
  glfwGetFramebufferSize(window, &display_w, &display_h);
  glViewport(0, 0, display_w, display_h);

  Game game(display_w, display_h);
  game.Init();

  glfwSetWindowUserPointer(window, &game);

  glfwSetKeyCallback(window, key_callback);
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

  // Main loop
#ifdef __EMSCRIPTEN__
  // For an Emscripten build we are disabling file-system access, so let's
  // not attempt to do a fopen() of the imgui.ini file. You may manually
  // call LoadIniSettingsFromMemory() to load settings from your own
  // storage.
  io.IniFilename = nullptr;
  EMSCRIPTEN_MAINLOOP_BEGIN
#else
  while (!glfwWindowShouldClose(window))
#endif
  {
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags
    // to tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input
    // data to your main application, or clear/overwrite your copy of the
    // mouse data.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard
    // input data to your main application, or clear/overwrite your copy
    // of the keyboard data. Generally you may always pass all inputs to
    // dear imgui, and hide them from your application based on those two
    // flags.
    glfwPollEvents();
    if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
      ImGui_ImplGlfw_Sleep(10);
      continue;
    }
    game.ProcessInput(ImGui::GetIO().DeltaTime);
    game.Update(ImGui::GetIO().DeltaTime);

    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w,
                 clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);

    game.Render();

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Rendering
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }
#ifdef __EMSCRIPTEN__
  EMSCRIPTEN_MAINLOOP_END;
#endif

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  // glfwTerminate();

  return 0;
}

// aarch64-linux-android34-clang++ imgui_impl_opengl3.o imgui_draw.o
// imgui_tables.o imgui_widgets.o imgui.o imgui_demo.o
// imgui_impl_android.o main.o -o combined.o -landroid -lEGL -lGLESv3
// -llog

void key_callback(GLFWwindow *window, int key, int scancode, int action,
                  int mode) {
  Game *game = reinterpret_cast<Game *>(glfwGetWindowUserPointer(window));
  // when a user presses the escape key, we set the WindowShouldClose
  // property to true, closing the application
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);
  if (key >= 0 && key < 1024) {
    if (action == GLFW_PRESS) {
      game->Keys[key] = true;
    } else if (action == GLFW_RELEASE) {
      game->Keys[key] = false;
      game->KeysProcessed[key] = false;
    }
  }
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  // make sure the viewport matches the new window dimensions; note that
  // width and height will be significantly larger than specified on
  // retina displays.
  glViewport(0, 0, width, height);
}

void glCheckError_(const char *file, int line) {
  GLenum errorCode = GL_NO_ERROR;
  while ((errorCode = glGetError()) != GL_NO_ERROR) {
    const char *error = "\0";
    switch (errorCode) {
    case GL_INVALID_ENUM:
      error = "INVALID_ENUM";
      break;
    case GL_INVALID_VALUE:
      error = "INVALID_VALUE";
      break;
    case GL_INVALID_OPERATION:
      error = "INVALID_OPERATION";
      break;
    // case GL_STACK_OVERFLOW:
    //   error = "STACK_OVERFLOW";
    //   break;
    // case GL_STACK_UNDERFLOW:
    //   error = "STACK_UNDERFLOW";
    //   break;
    case GL_OUT_OF_MEMORY:
      error = "OUT_OF_MEMORY";
      break;
    case GL_INVALID_FRAMEBUFFER_OPERATION:
      error = "INVALID_FRAMEBUFFER_OPERATION";
      break;
    }
    printf("%s:%d -> Error: %s\n", file, line, error);
    assert(false);
  }
}
#define glCheckError() glCheckError_(__FILE__, __LINE__)

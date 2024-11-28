#pragma once

#include <array>
#include <glad/gl.h>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace mapapp {
template <class TraitType> struct gl_wrapper {
  GLuint handler;
  gl_wrapper(GLuint handler = 0) : handler{handler} {}
  gl_wrapper(const gl_wrapper &) = delete;
  auto operator=(const gl_wrapper &) = delete;
  gl_wrapper(gl_wrapper &&other) : handler{std::exchange(other.handler, 0)} {}

  gl_wrapper &operator=(gl_wrapper &&other) {
    reset();
    handler = std::exchange(other.handler, 0);
    return *this;
  }
  ~gl_wrapper() { reset(); }
  void reset() {
    if (handler) {
      TraitType{}.destroy(handler);
      handler = 0;
    }
  }
  operator GLuint() const { return handler; }

  static gl_wrapper create(auto... args) {
    auto h = TraitType{}.create(args...);
    if (h)
      return h;
    throw std::runtime_error(
        "failed to create GL object (GL function returns 0)");
  }
};

namespace detail {
struct va_wrapper {
  auto create() {
    GLuint h;
    return glGenVertexArrays(1, &h), h;
  }

  auto destroy(auto h) { glDeleteVertexArrays(1, &h); }
};

struct b_wrapper {
  auto create() {
    GLuint h;
    return glGenBuffers(1, &h), h;
  }

  auto destroy(auto h) { glDeleteBuffers(1, &h); }
};

struct p_wrapper {
  auto create() { return glCreateProgram(); }

  auto destroy(auto h) { glDeleteProgram(h); }
};

struct s_wrapper {
  auto create(auto... args) { return glCreateShader(args...); }

  auto destroy(auto h) { glDeleteShader(h); }
};

struct q_wrapper {
  auto create() {
    GLuint h;
    return glGenQueries(1, &h), h;
  }

  auto destroy(auto h) { glDeleteQueries(1, &h); }
};

struct t_wrapper {
  auto create() {
    GLuint h;
    return glGenTextures(1, &h), h;
  }

  auto destroy(auto h) { glDeleteTextures(1, &h); }
};
} // namespace detail

using vertex_array = gl_wrapper<detail::va_wrapper>;
using buffer = gl_wrapper<detail::b_wrapper>;
using shader = gl_wrapper<detail::p_wrapper>;
using shader_object = gl_wrapper<detail::s_wrapper>;
using query = gl_wrapper<detail::q_wrapper>;
using texture = gl_wrapper<detail::t_wrapper>;

shader load_vf_shader(std::array<std::string_view, 2> source);

} // namespace mapapp

#pragma once

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <functional>
#include <glad/glad.h>
#include <glm/vec2.hpp>
#include <memory>
#include <optional>

#include "nk.h"

namespace mapapp {
struct glfw_instance {
  bool init = false;
  glfw_instance();
  ~glfw_instance();
};

namespace detail {
struct glfw_window_deleter {
  void operator()(GLFWwindow *window);
};
} // namespace detail

using glfw_window = std::unique_ptr<GLFWwindow, detail::glfw_window_deleter>;

struct nk_glfw_wrapper {
  nk_glfw glfw{0};
  nk_context *ctx;
  nk_glfw_wrapper(GLFWwindow *window);
  ~nk_glfw_wrapper();
};

class graphics_context {
public:
  static constexpr glm::ivec2 default_viewport{640, 360};
  graphics_context();
  graphics_context(const graphics_context &) = delete;
  graphics_context(graphics_context &&) = delete;
  auto operator=(const graphics_context &) = delete;
  auto operator=(graphics_context &&) = delete;

  operator bool() const;
  glm::ivec2 begin();
  void end();
  auto ui() { return nk->ctx; }

  bool key(int code);
  glm::dvec2 scroll_delta();
  glm::dvec2 cursor_pos();

  std::function<void(glm::ivec2)> framebuf_changed_cb;
  std::function<void(int, int, int)> key_cb;
  std::function<void(glm::dvec2)> scroll_cb;

// private:
  std::optional<glfw_instance> inst;
  glfw_window window;
  std::optional<nk_glfw_wrapper> nk;
  glm::dvec2 scroll{0.0, 0.0};
};

} // namespace mapapp

#include "graphics_context.hpp"
#include "noto.h"
#include <GLFW/glfw3.h>
#include <fmt/core.h>
#include <glm/vec2.hpp>

namespace mapapp {
glfw_instance::glfw_instance() {
  if (!glfwInit()) {
    fmt::println("ERROR: Unable to initialize GLFW");
    std::exit(1);
  }
}

glfw_instance::~glfw_instance() { glfwTerminate(); }

void detail::glfw_window_deleter::operator()(GLFWwindow *window) {
  glfwDestroyWindow(window);
}

nk_glfw_wrapper::nk_glfw_wrapper(GLFWwindow *window) {
  ctx = nk_glfw3_init(&glfw, window, NK_GLFW3_INSTALL_CALLBACKS);
}

nk_glfw_wrapper::~nk_glfw_wrapper() { nk_glfw3_shutdown(&glfw); }

void invoke_if_nn(auto &&fn, auto &&...args) {
  if (fn)
    std::forward<decltype(fn)>(fn)(std::forward<decltype(args)>(args)...);
}

graphics_context::graphics_context() {
  inst.emplace();
  glfwDefaultWindowHints();
  glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
  glfwWindowHint(GLFW_SAMPLES, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  window.reset(glfwCreateWindow(default_viewport.x, default_viewport.y,
                                "Map App", nullptr, nullptr));
  if (!window) {
    fmt::println("ERROR: Unable to create window");
    std::exit(1);
  }
  glfwMakeContextCurrent(window.get());
  if (!gladLoadGL(glfwGetProcAddress)) {
    fmt::println("ERROR: Unable to load OpenGL function pointers");
    std::exit(1);
  }
  glfwSwapInterval(1);

  nk.emplace(window.get());
  struct nk_font_atlas *atlas;
  nk_glfw3_font_stash_begin(&nk->glfw, &atlas);
  static const nk_rune vn_glyph_range[] = {
      0x0020, 0x00FF, // Basic ASCII
      0x00C0, 0x00C3, // À, Á, Â, Ã
      0x00C8, 0x00CA, // È, É, Ê
      0x00CC, 0x00CD, // Ì, Í
      0x00D2, 0x00D5, // Ò, Ó, Ô, Õ
      0x00D9, 0x00DA, // Ù, Ú
      0x00DD, 0x00DD, // Ý
      0x00E0, 0x00E3, // à, á, â, ã
      0x00E8, 0x00EA, // è, é, ê
      0x00EC, 0x00ED, // ì, í
      0x00F2, 0x00F5, // ò, ó, ô, õ
      0x00F9, 0x00FA, // ù, ú
      0x00FD, 0x00FD, // ý
      0x0102, 0x0103, // Ă, ă
      0x0110, 0x0111, // Đ, đ
      0x0128, 0x0129, // Ĩ, ĩ
      0x0168, 0x0169, // Ũ, ũ
      0x01A0, 0x01A1, // Ơ, ơ
      0x01AF, 0x01B0, // Ư, ư
      0x1EA0, 0x1EF9, // Vietnamese-specific accented letters
      0};

  struct nk_font_config config = nk_font_config(16.0);
  config.range = vn_glyph_range;
  auto font = nk_font_atlas_add_from_memory(atlas, noto_ttf, noto_ttf_len,
                                            16.0f, &config);
  nk_glfw3_font_stash_end(&nk->glfw);
  nk_style_set_font(nk->ctx, &font->handle);
  nk->glfw.user_ptr = this;
  static auto self = [](GLFWwindow *w) {
    return static_cast<graphics_context *>(
        static_cast<nk_glfw *>(glfwGetWindowUserPointer(w))->user_ptr);
  };
  // glfwSetFramebufferSizeCallback(
  //     window.get(), [](GLFWwindow *w, int width, int height) {
  //       invoke_if_nn(self(w)->framebuf_changed_cb, glm::ivec2{width,
  //       height});
  //     });
  glfwSetKeyCallback(window.get(),
                     [](GLFWwindow *w, int key, int _, int action, int mods) {
                       invoke_if_nn(self(w)->key_cb, key, action, mods);
                     });
  glfwSetScrollCallback(window.get(),
                        [](GLFWwindow *w, double xoff, double yoff) {
                          self(w)->scroll = {xoff, yoff};
                          nk_glfw3_scroll_callback(w, xoff, yoff);
                        });
}

graphics_context::operator bool() const {
  return !glfwWindowShouldClose(window.get());
}

glm::ivec2 graphics_context::begin() {
  glfwPollEvents();
  nk_glfw3_new_frame(&nk->glfw);
  glm::ivec2 size;
  glfwGetFramebufferSize(window.get(), &size.x, &size.y);
  glViewport(0, 0, size.x, size.y);
  return size;
}

void graphics_context::end() {
  nk_glfw3_render(&nk->glfw, NK_ANTI_ALIASING_ON, 512 << 10, 128 << 10);
  glfwSwapBuffers(window.get());
  scroll = {0.0, 0.0};
}

bool graphics_context::key(int code) {
  return glfwGetKey(window.get(), code) == GLFW_PRESS;
}

glm::dvec2 graphics_context::scroll_delta() { return scroll; }

glm::dvec2 graphics_context::cursor_pos() {
  glm::dvec2 pos;
  glfwGetCursorPos(window.get(), &pos.x, &pos.y);
  return pos;
}

} // namespace mapapp

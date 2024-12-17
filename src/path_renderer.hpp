#pragma once

#include "camera.hpp"
#include "gl.hpp"
#include "graphics_context.hpp"
#include <cstddef>
#include <glad/glad.h>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <vector>
namespace mapapp {
struct path {
  int offset, count;
  std::vector<glm::vec2> vertices;
  nk_colorf color;
  bool strip;
};

struct path_renderer {
  vertex_array vao;
  buffer vbo;
  shader shd;
  std::vector<path> paths;
  GLint loc_translation, loc_scale, loc_color;
  float line_width = 4.0f;

  path_renderer();

  std::size_t add_path(auto &&vertices, nk_colorf color, bool strip = true) {
    auto index = paths.size();
    auto &path = paths.emplace_back();
    path.color = color;
    path.vertices = vertices;
    path.strip = strip;
    update_vertex_buffer();
    return index;
  }

  void render(const camera::transform &transform,
              const std::vector<std::size_t> &paths);
  void update_color(std::size_t index, nk_colorf color);
  void update_line_width(float line_width);
  void reset();

private:
  void update_vertex_buffer();
};
} // namespace mapapp

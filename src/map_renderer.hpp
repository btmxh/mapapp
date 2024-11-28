#pragma once

#include "camera.hpp"
#include "gl.hpp"
#include "map_loader.hpp"
#include <vector>

namespace mapapp {
struct map_renderer {
  vertex_array vao;
  buffer vbo;
  shader shd;
  std::vector<int> offsets, counts;
  GLint loc_translation, loc_scale;

  map_renderer(const map_loader &map);

  void render(const camera::transform &transform);
};
} // namespace mapapp

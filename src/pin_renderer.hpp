#pragma once

#include "gl.hpp"

#include <glm/vec2.hpp>

namespace mapapp {
struct pin_renderer {
  enum class type : std::size_t {
    SOURCE = 0,
    DEST = 1,
    COUNT,
  };

  std::array<texture, static_cast<std::size_t>(type::COUNT)> textures;
  vertex_array vao;
  shader shd;
  GLint loc_bounds;

  pin_renderer();

  void render(type typ, glm::vec2 position, glm::ivec2 viewport);
};
} // namespace mapapp

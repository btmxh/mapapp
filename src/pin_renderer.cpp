#include "pin_renderer.hpp"
#include "gl.hpp"
#include "pin_icons.h"
#include <glad/glad.h>
#include <glm/vec4.hpp>

namespace mapapp {
pin_renderer::pin_renderer() {
  auto init_texture = [&](texture &t, int width, int height,
                          const void *pixels) {
    t = texture::create();
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  };

  vao = vertex_array::create();
  shd = load_vf_shader({R"(
#version 330
out vec2 vf_tex_coords;
const vec2 tex_coords[4] = vec2[](
  vec2(0.0, 0.0),
  vec2(1.0, 0.0),
  vec2(0.0, 1.0),
  vec2(1.0, 1.0)
);
uniform vec4 bounds;
void main() {
  vf_tex_coords = tex_coords[gl_VertexID];
  gl_Position = vec4(mix(bounds.xy, bounds.zw, vf_tex_coords), 0.0, 1.0);
}
  )",
                           R"(
#version 330
in vec2 vf_tex_coords;
out vec4 color;
uniform sampler2D tex;
void main() {
  color = texture(tex, vf_tex_coords);
}
    )"});
  init_texture(textures[0], pin_src_raw_width, pin_src_raw_height, pin_src_raw);
  init_texture(textures[1], pin_dst_raw_width, pin_dst_raw_height, pin_dst_raw);
  glUseProgram(shd);
  glUniform1i(glGetUniformLocation(shd, "tex"), 0);
  loc_bounds = glGetUniformLocation(shd, "bounds");
}

void pin_renderer::render(type typ, glm::vec2 position, glm::ivec2 viewport) {
  static const glm::vec2 hotspot{32 / 64.0f, 56 / 64.0f};
  glm::vec2 size{pin_src_raw_width, pin_src_raw_height}; // doesn't matter
  size = size / glm::vec2{viewport};
  position += size * hotspot;
  glm::vec4 bounds{position, position - size};
  glUseProgram(shd);
  glBindVertexArray(vao);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, textures[static_cast<std::size_t>(typ)]);
  glUniform4fv(loc_bounds, 1, &bounds[0]);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}
} // namespace mapapp

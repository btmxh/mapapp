#include "path_renderer.hpp"

#include "pl2d.hpp"
#include <span>

namespace mapapp {
path_renderer::path_renderer() {
  vao = mapapp::vertex_array::create();
  vbo = mapapp::buffer::create();
  shd = mapapp::load_vf_shader(std::array<std::string_view, 2>{R"(
#version 330
in vec2 pos;
uniform vec2 translation;
uniform vec2 scale;
void main() {
  gl_Position = vec4(scale * (pos + translation), 0.0, 1.0);
}
)",
                                                               R"(
#version 330
out vec4 color;
uniform vec4 u_color;
void main() {
  color = u_color;
}
)"});

  loc_translation = glGetUniformLocation(shd, "translation");
  loc_scale = glGetUniformLocation(shd, "scale");
  loc_color = glGetUniformLocation(shd, "u_color");
  glBindAttribLocation(shd, 0, "pos");

  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, false, sizeof(glm::vec2), nullptr);
}

void path_renderer::render(const camera::transform &transform,
                           const std::vector<std::size_t> &path_indices) {
  if (path_indices.empty()) {
    return;
  }
  glUseProgram(shd);
  glUniform2fv(loc_translation, 1, &transform.translation[0]);
  glUniform2fv(loc_scale, 1, &transform.scale[0]);
  glBindVertexArray(vao);
  for (auto i : path_indices) {
    glUniform4fv(loc_color, 1, &paths[i].color.r);
    glDrawArrays(GL_TRIANGLES, paths[i].offset, paths[i].count);
  }
}

void path_renderer::update_color(std::size_t index, nk_colorf color) {
  paths[index].color = color;
}

void path_renderer::reset() { paths.clear(); }

void path_renderer::update_line_width(float line_width) {
  this->line_width = line_width;
  update_vertex_buffer();
}

void path_renderer::update_vertex_buffer() {
  std::vector<glm::vec2> vertices;

  using pl2d = crushedpixel::Polyline2D;
  for (auto &path : paths) {
    path.offset = vertices.size();
    if (path.strip) {
      pl2d::create<glm::vec2>(
          std::back_insert_iterator{vertices}, path.vertices, line_width,
          pl2d::JointStyle::ROUND, pl2d::EndCapStyle::ROUND, false);
    } else {
      for (std::size_t i = 0; i < path.vertices.size(); i += 2) {
        pl2d::create<glm::vec2>(std::back_insert_iterator{vertices},
                                std::span<glm::vec2>{&path.vertices[i], 2},
                                line_width, pl2d::JointStyle::ROUND,
                                pl2d::EndCapStyle::ROUND, false);
      }
    }
    path.count = vertices.size() - path.offset;
  }

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertices[0]),
               vertices.data(), GL_DYNAMIC_DRAW);
}
} // namespace mapapp

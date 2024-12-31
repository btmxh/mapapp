#include "map_renderer.hpp"
#include "pl2d.hpp"
#include <fmt/base.h>
#include <fmt/core.h>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <mapbox/earcut.hpp>
#include <random>

namespace mapbox {
namespace util {

template <> struct nth<0, glm::vec2> {
  inline static auto get(const glm::vec2 &t) { return t.x; };
};
template <> struct nth<1, glm::vec2> {
  inline static auto get(const glm::vec2 &t) { return t.y; };
};

} // namespace util
} // namespace mapbox

namespace mapapp {

struct vertex {
  glm::vec2 pos;
  glm::u8vec4 color;
};

map_renderer::map_renderer(const map_loader &loader) {
  vao = mapapp::vertex_array::create();
  vbo = mapapp::buffer::create();
  shd = mapapp::load_vf_shader(std::array<std::string_view, 2>{R"(
#version 330
in vec2 pos;
in vec4 color;
out vec4 vf_color;
uniform vec2 translation;
uniform vec2 scale;
void main() {
  gl_Position = vec4(scale * (pos + translation), 0.0, 1.0);
  vf_color = color;
}
)",
                                                               R"(
#version 330
in vec4 vf_color;
out vec4 color;
void main() {
  color = vf_color;
}
)"});

  loc_translation = glGetUniformLocation(shd, "translation");
  loc_scale = glGetUniformLocation(shd, "scale");
  glBindAttribLocation(shd, 0, "pos");
  glBindAttribLocation(shd, 1, "color");

  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);

  auto get_render_attribs = [&](const auto &way) {
    if constexpr (std::is_same_v<std::decay_t<decltype(way)>,
                                 mapapp::highway>) {
      static const float default_thickness = 8.0;
      switch (way.h_kind) {
        using enum mapapp::highway::kind;
        using result_t = std::pair<glm::u8vec4, float>;
      case MOTORWAY:
        return result_t{{0xd2, 0x82, 0x90, 0xff}, default_thickness * 2.0f};
      case TRUNK:
        return result_t{{0xe3, 0xad, 0x9b, 0xff}, default_thickness * 1.0f};
      case PRIMARY:
        return result_t{{0xe4, 0xc1, 0x91, 0xff}, default_thickness * 1.0f};
      case SECONDARY:
        return result_t{{0xdd, 0xe1, 0xa8, 0xff}, default_thickness * 1.0f};
      case TERTIARY:
        return result_t{{0xff, 0xff, 0xff, 0xff}, default_thickness * 1.0f};
      default:
        return result_t{{0xff, 0xff, 0xff, 0xff}, default_thickness * 0.5f};
      }
    } else if constexpr (std::is_same_v<std::decay_t<decltype(way)>,
                                        mapapp::structure>) {
      static auto colors = []() {
        static std::minstd_rand rng;
        static std::uniform_int_distribution<int> range{128, 255};
        std::array<glm::u8vec4,
                   static_cast<std::size_t>(mapapp::structure::kind::NUM_KINDS)>
            colors;
        for (auto &col : colors) {
          for (int i = 0; i < 4; ++i) {
            col[i] = range(rng);
          }
        }
        return colors;
      }();

      return colors[static_cast<std::size_t>(way.s_kind)];
    }
  };

  std::vector<vertex> vertex_buffer_data;
  for (const auto &[id, highway] : loader.highways) {
    std::vector<glm::vec2> input;
    input.reserve(highway.nodes.size());
    for (const auto node_id : highway.nodes) {
      input.push_back(loader.nodes.at(node_id).position);
    }
    auto [color, thickness] = get_render_attribs(highway);
    auto result = pl2d::create(input, thickness, pl2d::JointStyle::ROUND,
                               pl2d::EndCapStyle::ROUND, true);
    offsets.push_back(vertex_buffer_data.size());
    counts.push_back(result.size());
    vertex_buffer_data.reserve(vertex_buffer_data.size() + result.size());
    for (const auto pos : result) {
      vertex_buffer_data.emplace_back(pos, color);
    }
  }
  for (const auto &[id, structure] : loader.structures) {
    std::array<std::vector<glm::vec2>, 1> input;
    input[0].reserve(structure.nodes.size());
    for (const auto node_id : structure.nodes) {
      input[0].push_back(loader.nodes.at(node_id).position);
    }
    auto result = mapbox::earcut(input);
    auto color = get_render_attribs(structure);
    // offsets.push_back(vertex_buffer_data.size());
    // counts.push_back(result.size());
    vertex_buffer_data.reserve(vertex_buffer_data.size() + result.size());
    for (auto idx : result) {
      vertex_buffer_data.emplace_back(input[0][idx]);
    }
  }
  // fmt::println("VBO size: {} (bytes)",
  //              vertex_buffer_data.size() * sizeof(vertex_buffer_data[0]));
  glBufferData(GL_ARRAY_BUFFER,
               vertex_buffer_data.size() * sizeof(vertex_buffer_data[0]),
               vertex_buffer_data.data(), GL_STATIC_DRAW);

  auto ptr_cast = [](auto &&value) {
    return reinterpret_cast<const void *>(static_cast<std::uintptr_t>(value));
  };
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, false, sizeof(vertex),
                        ptr_cast(offsetof(vertex, pos)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, true, sizeof(vertex),
                        ptr_cast(offsetof(vertex, color)));
}

void map_renderer::render(const camera::transform &transform) {
  glUseProgram(shd);
  glUniform2fv(loc_translation, 1, &transform.translation[0]);
  glUniform2fv(loc_scale, 1, &transform.scale[0]);
  glBindVertexArray(vao);
  glMultiDrawArrays(GL_TRIANGLES, offsets.data(), counts.data(),
                    offsets.size());
}
} // namespace mapapp

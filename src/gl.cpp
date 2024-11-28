#include "gl.hpp"
#include <fmt/core.h>
#include <stdexcept>
#include <vector>
namespace mapapp {

shader load_vf_shader(std::array<std::string_view, 2> source) {
  std::array types{GL_VERTEX_SHADER, GL_FRAGMENT_SHADER};
  auto program = shader::create();
  std::vector<shader_object> objs;
  for (int i = 0; i < source.size(); ++i) {
    auto &shader = objs.emplace_back(shader_object::create(types[i]));
    glAttachShader(program, shader);
    auto data = source[i].data();
    auto len = static_cast<GLint>(source[i].size());
    glShaderSource(shader, 1, &data, &len);
    glCompileShader(shader);

    if (GLint j; glGetShaderiv(shader, GL_COMPILE_STATUS, &j), !j) {
      char buffer[256];
      glGetShaderInfoLog(shader, sizeof buffer, nullptr, buffer);
      fmt::println("Shader compilation error: {}", buffer);
      throw std::runtime_error{"error compiling shader"};
    }
  }

  glLinkProgram(program);
  if (GLint i; glGetProgramiv(program, GL_LINK_STATUS, &i), !i) {
    char buffer[256];
    glGetProgramInfoLog(program, sizeof buffer, nullptr, buffer);
    fmt::println("Program link error: {}", buffer);
    throw std::runtime_error{"error linking shader"};
  }

  for (const auto &shader : objs) {
    glDetachShader(program, shader);
  }

  return program;
}

} // namespace mapapp

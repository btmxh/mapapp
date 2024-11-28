#pragma once

#include "map_loader.hpp"
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <vector>

namespace mapapp {
struct camera {
  glm::vec2 center;
  float scale_pp;
  glm::vec2 min_center, max_center;
  float min_scale_pp, max_scale_pp, step_scale_pp;

  static camera from_data(const auto &nodes, glm::ivec2 initial_viewport) {
    std::vector<float> x, y;
    x.reserve(nodes.size()), y.reserve(nodes.size());
    for (const auto &node : nodes)
      x.push_back(node.position.x), y.push_back(node.position.y);
    std::sort(x.begin(), x.end());
    std::sort(y.begin(), y.end());
    auto get_percentile = [&](const auto &cont, auto p) {
      return cont[std::clamp<std::size_t>(std::size(cont) * p, 0,
                                          std::size(cont))];
    };
    static const float outlier_factor = 0.3;
    auto left = get_percentile(x, outlier_factor);
    auto cx = get_percentile(x, 0.5);
    auto right = get_percentile(x, 1.0f - outlier_factor);
    auto up = get_percentile(y, outlier_factor);
    auto cy = get_percentile(y, 0.5);
    auto down = get_percentile(y, 1.0f - outlier_factor);

    glm::vec2 window{right - left, down - up};

    camera c;
    c.center = {cx, cy};
    c.scale_pp =
        std::min(initial_viewport.x / window.x, initial_viewport.y / window.y);
    c.step_scale_pp = c.scale_pp * 0.1f;
    c.min_scale_pp = c.scale_pp * 0.5f;
    c.max_scale_pp = c.scale_pp * 5.0f;
    c.min_center = c.center - window * 1.0f;
    c.max_center = c.center + window * 1.0f;
    return c;
  }

  struct transform {
    // apply scale after translation
    glm::vec2 translation, scale = {1.0f, 1.0f};

    glm::vec2 map(glm::vec2 x) { return (x + translation) * scale; }

    glm::vec2 unmap(glm::vec2 x) { return x / scale - translation; }
  };

  transform update(glm::ivec2 viewport_size) {
    transform t;
    t.translation = -center;
    t.scale = scale_pp * 2.0f / glm::vec2{viewport_size};
    return t;
  }

  void update_scale_pp(float step) {
    scale_pp =
        std::clamp(scale_pp + step * step_scale_pp, min_scale_pp, max_scale_pp);
  }

  void update_target(glm::vec2 delta) {
    center = glm::clamp(center + delta, min_center, max_center);
  }
};
} // namespace mapapp

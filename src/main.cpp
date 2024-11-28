#include "camera.hpp"
#include "gpu_timer.hpp"
#include "graphics_context.hpp"
#include "map_loader.hpp"
#include "map_renderer.hpp"
#include "nk.h"
#include "path_renderer.hpp"
#include "pathfind.hpp"
#include "pin_renderer.hpp"
#include <GLFW/glfw3.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fmt/ranges.h>
#include <future>
#include <glad/gl.h>
#include <glm/ext/vector_double2.hpp>
#include <glm/ext/vector_uint4_sized.hpp>
#include <mapbox/earcut.hpp>
#include <mutex>
#include <nanoflann.hpp>
#include <optional>
#include <ranges>
#include <thread>
#include <utility>

struct algo_state {
  const char *short_name, *long_name;
  nk_bool enabled = true;
  nk_colorf path_color;
  using time_pt = std::chrono::time_point<std::chrono::high_resolution_clock>;
  std::optional<time_pt> begin;
  std::jthread thread;
  std::mutex result_mtx;
  std::optional<mapapp::pathfind_result> result;
  std::optional<int> path_renderer_index;

  algo_state(const char *short_name, const char *long_name, nk_colorf color)
      : short_name{short_name}, long_name{long_name}, path_color{color} {}

  void reset() {
    thread = {}; // stop currently running thread
    begin = std::nullopt;
    result = std::nullopt; // locking mutex not necessary as the thread is
                           // already stopped
    path_renderer_index.reset();
  }

  static auto now() { return std::chrono::high_resolution_clock::now(); }

  auto use_result(auto fn) {
    std::scoped_lock lock{result_mtx};
    return fn(result,
              std::chrono::duration<double>(
                  (result.has_value() ? result->finish_time : now()) - *begin));
  }

  void run(int index, const mapapp::osm_graph &graph,
           mapapp::osm_graph::index_t start, mapapp::osm_graph::index_t end) {
    std::promise<time_pt> begin_p;
    thread = std::jthread{[&](std::stop_token token, int index) {
                            begin_p.set_value(now());
                            auto result = mapapp::algorithms[index](
                                token, graph, start, end);
                            result.finish_time = now();
                            std::scoped_lock lock{result_mtx};
                            this->result.emplace(std::move(result));
                          },
                          index};
    begin.emplace(begin_p.get_future().get());
  }
};

auto fmt_time(std::chrono::duration<double> time) {
  auto seconds = time.count();
  if (seconds < 1e-3) {
    return fmt::format("{:.2f}µs", seconds * 1e6);
  } else if (seconds < 1.0) {
    return fmt::format("{:.2f}ms", seconds * 1e3);
  } else {
    return fmt::format("{:.2f}s", seconds);
  }
}

auto fmt_bytes(std::size_t bytes) {
  if (bytes >= std::size_t{1} << 20) {
    return fmt::format("{:.2f}MiB", bytes / (1024.0 * 1024.0));
  } else if (bytes >= std::size_t{1} << 10) {
    return fmt::format("{:.2f}KiB", bytes / 1024.0);
  } else {
    return fmt::format("{}B", bytes);
  }
}

auto fmt_dist(double meters) {
  if (meters >= 1000.0) {
    return fmt::format("{:.2f}km", meters * 1e-3);
  } else {
    return fmt::format("{:.2f}m", meters);
  }
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fmt::println("Cách sử dụng: {} [đường dẫn tới file .pbf]", argv[0]);
    std::exit(1);
  }

  std::vector<double> x, y;

  mapapp::map_loader loader;
  loader.load(argv[1]);
  const auto normalize_offset = loader.normalize_node_positions();
  mapapp::osm_graph graph{loader};

  mapapp::graphics_context gc;
  mapapp::map_renderer map_renderer{loader};
  mapapp::path_renderer path_renderer;
  mapapp::pin_renderer pin_renderer;
  // renders the path from to cursor position to the nearest node
  mapapp::path_renderer to_node_path_renderer;

  auto cam =
      mapapp::camera::from_data(loader.nodes | std::views::values,
                                mapapp::graphics_context::default_viewport);

  std::array algos{
      algo_state{"DFS", "DFS (Tìm kiếm theo chiều sâu)", {0.2, 0.6, 0.8, 0.5}},
      algo_state{"BFS", "BFS (Tìm kiếm theo chiều rộng)", {0.4, 0.8, 0.2, 0.5}},
      algo_state{"GBeFS",
                 "GBeFS (Tìm kiếm theo lựa chọn tốt nhất)",
                 {0.9, 0.7, 0.2, 0.5}},
      algo_state{
          "UCS", "UCS (Tìm kiếm với chi phí cực tiểu)", {0.8, 0.4, 0.2, 0.5}},
      algo_state{"A*", "A*", {0.6, 0.2, 0.8, 0.5}},
  };

  std::optional<mapapp::osm_graph::index_t> start, end;
  glm::vec2 start_pos, end_pos;

  enum class PickPointState {
    Pending,
    PickStart,
    PickEnd,
  };

  PickPointState state = PickPointState::Pending;

  auto reset_state = [&]() {
    state = PickPointState::Pending;
    start = end = std::nullopt;
    for (auto &algo : algos) {
      algo.reset();
    }
  };

  std::chrono::duration<double> last_cpu_time{0.0};
  mapapp::gpu_timer gpu_timer;

  while (gc) {
    auto cpu_start = std::chrono::high_resolution_clock::now();

    gpu_timer.begin();
    auto viewport = gc.begin();
    auto transform = cam.update(viewport);

    for (auto &algo : algos) {
      std::scoped_lock lock{algo.result_mtx};
      if (algo.path_renderer_index.has_value() || !algo.result.has_value() ||
          std::isnan(algo.result->distance)) {
        continue;
      }

      std::vector<glm::vec2> positions;
      for (auto node_index : algo.result->path) {
        positions.push_back(graph.nodes[node_index].position);
      }
      algo.path_renderer_index =
          path_renderer.add_path(std::move(positions), algo.path_color);
    }

    auto mouse_pos = gc.cursor_pos();
    mouse_pos = mouse_pos / glm::dvec2{viewport} * 2.0 - 1.0;
    mouse_pos.y *= -1;
    auto pos = glm::dvec2{transform.unmap(mouse_pos)};
    auto nearest_node = graph.nn_query(pos);

    auto ctx = gc.ui();
    if (nk_begin(ctx, "Project AI", nk_rect(50, 50, 450, 400),
                 NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE |
                     NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE)) {
      nk_layout_row_dynamic(ctx, 20, 1);
      if (nk_button_label(ctx, "Tìm đường")) {
        reset_state();
        state = PickPointState::PickStart;
      }

      if (state == PickPointState::PickStart) {
        nk_label(ctx, "Chọn điểm đầu", NK_TEXT_CENTERED);
      } else if (state == PickPointState::PickEnd) {
        nk_label(ctx, "Chọn điểm cuối", NK_TEXT_CENTERED);
      }

      for (int i = 0; i < algos.size(); ++i) {
        auto &algo = algos[i];
        bool enabled = algo.enabled;
        nk_layout_row_template_begin(ctx, 20);
        nk_layout_row_template_push_dynamic(ctx);
        if (enabled) {
          nk_layout_row_template_push_static(ctx, 100);
        }
        nk_layout_row_template_end(ctx);

        if (nk_checkbox_label(ctx, algo.long_name, &algo.enabled)) {
          if (algo.enabled && !algo.begin && start.has_value() &&
              end.has_value()) {
            algo.run(i, graph, *start, *end);
          }
        }
        if (enabled) {
          if (nk_combo_begin_color(ctx, nk_rgb_cf(algo.path_color),
                                   nk_vec2(200, 400))) {
            nk_layout_row_dynamic(ctx, 120, 1);
            auto &col = algo.path_color;
            col = nk_color_picker(ctx, col, NK_RGBA);
            nk_layout_row_dynamic(ctx, 25, 1);
            col.r = nk_propertyf(ctx, "#R:", 0, col.r, 1.0f, 0.01f, 0.005f);
            col.g = nk_propertyf(ctx, "#G:", 0, col.g, 1.0f, 0.01f, 0.005f);
            col.b = nk_propertyf(ctx, "#B:", 0, col.b, 1.0f, 0.01f, 0.005f);
            col.a = nk_propertyf(ctx, "#A:", 0, col.a, 1.0f, 0.01f, 0.005f);
            if (algo.path_renderer_index.has_value()) {
              path_renderer.update_color(*algo.path_renderer_index, col);
            }
            nk_combo_end(ctx);
          }
        }
      }

      nk_layout_row_dynamic(ctx, 20, 1);
      if (std::any_of(algos.begin(), algos.end(), [](const auto &algo) {
            return algo.begin && algo.enabled;
          })) {

        auto new_line_width =
            nk_propertyf(ctx, "Độ rộng đường đi:", 1.0f,
                         path_renderer.line_width, 16.0f, 0.1f, 0.05f);
        if (new_line_width != path_renderer.line_width) {
          path_renderer.update_line_width(new_line_width);
        }

        nk_label(ctx, "Kết quả tìm đường", NK_TEXT_CENTERED);
      }
      for (auto &algo : algos) {
        if (!algo.begin || !algo.enabled) {
          continue;
        }

        auto msg = algo.use_result([&](const auto &result, auto cpu_time) {
          auto time_str = fmt_time(cpu_time);
          if (result.has_value()) {
            if (std::isnan(result->distance)) {
              return fmt::format(
                  "{}: không tìm thấy đường (tổng thời gian {}, bộ nhớ {}/{})",
                  algo.short_name, fmt_time(cpu_time),
                  fmt_bytes(result->mem_stat.max_allocated),
                  fmt_bytes(result->mem_stat.total_allocated));
            }
            return fmt::format(
                "{}: khoảng cách {} (tổng thời gian {}, bộ nhớ {}/{})",
                algo.short_name, fmt_dist(result->distance), fmt_time(cpu_time),
                fmt_bytes(result->mem_stat.max_allocated),
                fmt_bytes(result->mem_stat.total_allocated));
          } else {
            return fmt::format("{}: đang thực hiện (tổng thời gian {})",
                               algo.short_name, time_str);
          }
        });
        nk_label(ctx, msg.data(), NK_TEXT_LEFT);
      }

      if (nk_tree_push(ctx, NK_TREE_TAB, "Thông tin gỡ lỗi", NK_MINIMIZED)) {
        nk_layout_row_dynamic(ctx, 20, 1);
        auto msg = fmt::format("start_node: {} ({})",
                               start ? graph.nodes[*start].id : -1,
                               start.value_or(-1));
        nk_label(ctx, msg.c_str(), NK_TEXT_LEFT);
        msg = fmt::format("end_node: {} ({})", end ? graph.nodes[*end].id : -1,
                          end.value_or(-1));
        nk_label(ctx, msg.c_str(), NK_TEXT_LEFT);
        msg = fmt::format("nearest_node: {} ({})", graph.nodes[nearest_node].id,
                          nearest_node);
        nk_label(ctx, msg.c_str(), NK_TEXT_LEFT);
        msg = fmt::format("last_cpu_time: {}", fmt_time(last_cpu_time));
        nk_label(ctx, msg.c_str(), NK_TEXT_LEFT);
        msg = fmt::format("last_gpu_time: {}", fmt_time(gpu_timer.last_time));
        nk_label(ctx, msg.c_str(), NK_TEXT_LEFT);
        nk_tree_pop(ctx);
      }
    }

    nk_end(ctx);

    if (!nk_item_is_any_active(ctx)) {
      const auto &input = ctx->input;

      if (gc.key(GLFW_KEY_LEFT_CONTROL) || gc.key(GLFW_KEY_RIGHT_CONTROL)) {
        cam.update_scale_pp(gc.scroll_delta().y);
      } else {
        glm::vec2 delta{gc.scroll_delta()};
        if (gc.key(GLFW_KEY_LEFT_SHIFT) || gc.key(GLFW_KEY_RIGHT_SHIFT)) {
          std::swap(delta.x, delta.y);
          delta.x *= -1;
        }
        cam.update_target(delta / cam.scale_pp * 50.0f);
      }

      if (state != PickPointState::Pending) {
        const auto &lmb = input.mouse.buttons[NK_BUTTON_LEFT];
        if (lmb.clicked && !lmb.down) {
          if (state == PickPointState::PickStart) {
            state = PickPointState::PickEnd;
            start = nearest_node;
            start_pos = pos;
          } else {
            state = PickPointState::Pending;
            end = nearest_node;
            end_pos = pos;

            for (int i = 0; i < algos.size(); ++i) {
              if (algos[i].enabled) {
                algos[i].run(i, graph, *start, *end);
              }
            }
          }
        }
      }

      if (glfwGetKey(gc.window.get(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        state = PickPointState::Pending;
      }
    }

    glClearColor(0.8, 0.8, 0.8, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    map_renderer.render(transform);
    std::vector<std::size_t> paths;
    for (const auto &algo : algos) {
      if (algo.path_renderer_index.has_value() && algo.enabled) {
        paths.push_back(*algo.path_renderer_index);
      }
    }
    path_renderer.render(transform, paths);

    {
      using enum mapapp::pin_renderer::type;
      if (!nk_item_is_any_active(ctx)) {
        if (state != PickPointState::Pending) {
          pin_renderer.render(state == PickPointState::PickStart ? SOURCE
                                                                 : DEST,
                              mouse_pos, viewport);
        }
      }

      to_node_path_renderer.reset();
      std::vector<std::size_t> paths;
      auto make_to_node_path = [&](glm::vec2 pos,
                                   mapapp::osm_graph::index_t node_idx) {
        static const float segment_length = 4.0f;
        std::vector<glm::vec2> vertices;
        auto to_node_vector = graph.nodes[node_idx].position - pos;
        auto length = glm::length(to_node_vector);
        auto adv = to_node_vector / length;
        int cnt = 0;
        for (float t = 0.0; length - t >= 0.01f; t += segment_length * 4) {
          auto from = pos + adv * t;
          auto to = pos + adv * std::min(t + segment_length, length);
          vertices.push_back(from);
          vertices.push_back(to);
        }

        paths.push_back(to_node_path_renderer.add_path(
            std::move(vertices), {0.0f, 0.0f, 0.0f, 0.5f}, false));
      };
      if (start.has_value()) {
        make_to_node_path(start_pos, *start);
      }
      if (end.has_value()) {
        make_to_node_path(end_pos, *end);
      }
      if (state != PickPointState::Pending) {
        make_to_node_path(pos, nearest_node);
      }

      to_node_path_renderer.render(transform, paths);

      if (start.has_value()) {
        pin_renderer.render(SOURCE, transform.map(start_pos), viewport);
      }
      if (end.has_value()) {
        pin_renderer.render(DEST, transform.map(end_pos), viewport);
      }
    }

    last_cpu_time = std::chrono::duration<double>(
        std::chrono::high_resolution_clock::now() - cpu_start);

    gc.end();
    gpu_timer.end();
  }
}

#pragma once

#include "map_loader.hpp"
#include <chrono>
#include <fmt/base.h>
#include <glm/vec2.hpp>
#include <map>
#include <nanoflann.hpp>
#include <osmium/osm/location.hpp>
#include <stop_token>
namespace mapapp {

struct osm_graph {
  using index_t = std::size_t;

  struct node_vertex {
    osmium::Location location;
    id_t id;
    glm::vec2 position;
    std::vector<std::pair<double, index_t>> adj;
  };

  struct node_vector : public std::vector<node_vertex> {
    using std::vector<node_vertex>::vector;

    size_t kdtree_get_point_count() const { return size(); }
    double kdtree_get_pt(const std::size_t idx, int dim) const {
      return at(idx).position[dim];
    }

    template <class BBOX> bool kdtree_get_bbox(BBOX &bb) const { return false; }
  };

  std::map<id_t, index_t> node_index_map;
  node_vector nodes;

  struct adaptor_t {};

  nanoflann::KDTreeSingleIndexAdaptor<
      nanoflann::L2_Simple_Adaptor<double, decltype(nodes)>, decltype(nodes), 2,
      std::size_t>
      nn_tree;

  osm_graph(const map_loader &map);
  osm_graph(const osm_graph &) = delete;
  auto operator=(const osm_graph &) = delete;
  osm_graph(osm_graph &&) = delete;
  auto operator=(osm_graph &&) = delete;

  index_t nn_query(glm::dvec2 pos);
};
struct memory_statistics {
  std::size_t total_allocated = 0;
  std::size_t max_allocated = 0;
  std::size_t cur_allocated = 0;

  void alloc(std::size_t size);
  void free(std::size_t size);
};

struct pathfind_result {
  std::vector<osm_graph::index_t> path;
  double distance = NAN;
  std::chrono::high_resolution_clock::time_point finish_time;
  memory_statistics mem_stat;

  // check if path found
  operator bool() { return !std::isnan(distance); }
};

using pathfind_algo = pathfind_result(std::stop_token token,
                                      const osm_graph &graph, id_t start,
                                      id_t end);
pathfind_algo dfs, bfs, befs, ucs, a_star;

constexpr std::array<pathfind_algo *, 5> algorithms{dfs, bfs, befs, ucs,
                                                    a_star};

} // namespace mapapp

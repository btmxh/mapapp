#include "pathfind.hpp"
#include "map_loader.hpp"
#include "spherical.hpp"
#include <chrono>
#include <cstddef>
#include <deque>
#include <fmt/base.h>
#include <fmt/xchar.h>
#include <memory>
#include <optional>
#include <set>

namespace mapapp {
osm_graph::osm_graph(const map_loader &map) : nn_tree{2, nodes} {
  index_t index = 0;
  std::set<id_t> highway_nodes;
  for (const auto &[_, way] : map.highways) {
    highway_nodes.insert(way.nodes.begin(), way.nodes.end());
  }
  for (auto id : highway_nodes) {
    const auto &node = map.nodes.at(id);
    node_index_map.emplace(id, index++);
    auto &n = nodes.emplace_back();
    n.id = id;
    n.location = node.location;
    n.position = node.position;
  }
  for (const auto &[_, way] : map.highways) {
    index_t prev = -1;
    for (const auto node : way.nodes) {
      auto cur = node_index_map[node];
      if (prev != static_cast<index_t>(-1)) {
        auto prev_loc = nodes[prev].location;
        auto cur_loc = nodes[cur].location;
        auto distance = spherical_distance(prev_loc, cur_loc);
        nodes[prev].adj.emplace_back(distance, cur);
        if(!way.oneway) {
          nodes[cur].adj.emplace_back(distance, prev);
        }
      }
      prev = cur;
    }
  }
  for (auto &node : nodes) {
    std::sort(node.adj.begin(), node.adj.end());
  }

  nn_tree.buildIndex();
}

osm_graph::index_t osm_graph::nn_query(glm::dvec2 pos) {
  auto out_index = static_cast<std::size_t>(-1);
  double dummy;
  nn_tree.knnSearch(&pos[0], 1, &out_index, &dummy);
  return out_index;
}

void memory_statistics::alloc(std::size_t size) {
  total_allocated += size;
  cur_allocated += size;
  max_allocated = std::max(max_allocated, cur_allocated);
}

void memory_statistics::free(std::size_t size) {
  assert(size <= cur_allocated);
  cur_allocated -= size;
}

template <class T> struct tracking_allocator {
  using value_type = T;

  memory_statistics *stats;
  std::allocator<T> base;

  tracking_allocator(memory_statistics *stats) : stats{stats} {}
  tracking_allocator(const tracking_allocator &other) : stats{other.stats} {}
  template <class U>
  tracking_allocator(const tracking_allocator<U> &other) : stats{other.stats} {}

  [[nodiscard]] T *allocate(std::size_t n, const void *hint = 0) {
    auto ptr = base.allocate(n);
    stats->alloc(n * sizeof(T));
    return ptr;
  }

  void deallocate(T *p, std::size_t n) {
    base.deallocate(p, n);
    stats->free(n * sizeof(T));
  }
};

template <class T> using pf_vector = std::vector<T, tracking_allocator<T>>;

template <class K, class V>
using pf_map =
    std::map<K, V, std::less<K>,
             tracking_allocator<typename std::map<K, V>::value_type>>;

template <class K>
using pf_set = std::set<K, std::less<K>,
                        tracking_allocator<typename std::set<K>::value_type>>;

template <class T> using pf_deque = std::deque<T, tracking_allocator<T>>;

inline void construct_path(pathfind_result &result, std::stop_token token,
                           const auto &parent, const auto &graph, auto start,
                           auto end) {
  result.distance = 0;
  result.path.push_back(end);

  if (start != end) {
    auto u = end;
    do {
      if (token.stop_requested()) {
        result.distance = NAN;
        result.path.clear();
        return;
      }
      auto parent_u = parent.at(u);
      result.distance += spherical_distance(graph.nodes[u].location,
                                            graph.nodes[parent_u].location);
      u = parent_u;
      result.path.push_back(u);
    } while (u != start);
  }
};

pathfind_result dfs(std::stop_token token, const osm_graph &graph, id_t start,
                    id_t end) {
  pathfind_result result;
  using index_t = osm_graph::index_t;

  pf_vector<std::pair<index_t, std::size_t>> stack{&result.mem_stat};
  pf_set<index_t> visited{&result.mem_stat};

  stack.emplace_back(start, 0);
  visited.insert(start);

  while (!stack.empty() && !token.stop_requested()) {
    auto &back = stack.back();
    auto cur_node = back.first;
    auto index = back.second;

    const auto &adj = graph.nodes[back.first].adj;
    if (cur_node == end) {
      result.distance = 0.0;
      for (const auto &[i, _] : stack) {
        if (token.stop_requested()) {
          result.distance = NAN;
          result.path.clear();
          goto end;
        }

        if (!result.path.empty()) {
          result.distance +=
              spherical_distance(graph.nodes[result.path.back()].location,
                                 graph.nodes[i].location);
        }

        result.path.push_back(i);
      }
      break;
    }

    // skip visited nodes
    while (index < adj.size() && visited.contains(adj[index].second)) {
      ++index;
      if (token.stop_requested()) {
        goto end;
      }
    }

    if (index >= adj.size()) {
      stack.pop_back();
    } else {
      ++back.second;
      visited.insert(cur_node);
      // note: emplace_back must be done after all modifications to `back`
      stack.emplace_back(adj[index].second, 0);
    }
  }

end:
  return result;
}

pathfind_result bfs(std::stop_token token, const osm_graph &graph, id_t start,
                    id_t end) {
  pathfind_result result;
  using index_t = osm_graph::index_t;

  pf_deque<index_t> queue{&result.mem_stat};
  pf_map<index_t, index_t> parent{&result.mem_stat};

  queue.push_back(start);
  parent[start] = static_cast<osm_graph::index_t>(-1);

  while (!queue.empty() && !token.stop_requested()) {
    auto cur_node = queue.front();
    queue.pop_front();

    if (cur_node == end) {
      construct_path(result, token, parent, graph, start, end);
      break;
    }

    for (const auto [_, next_node] : graph.nodes[cur_node].adj) {
      if (token.stop_requested()) {
        return result;
      }

      if (!parent.contains(next_node)) {
        parent.emplace(next_node, cur_node);
        queue.push_back(next_node);
      }
    }
  }

  result.finish_time = std::chrono::high_resolution_clock::now();
  return result;
}

template <class K, class V> struct pf_priority_queue {
  pf_vector<std::pair<K, V>> heap;
  pf_map<K, V> index_map;

  pf_priority_queue(memory_statistics *mem) : heap{mem}, index_map{mem} {}

  void swap(std::size_t i, std::size_t j) {
    std::swap(heap[i], heap[j]);
    index_map[heap[i].first] = i;
    index_map[heap[j].first] = j;
  }

  void check_heap() {
    for (std::size_t i = 0; i < heap.size(); ++i) {
      for (std::size_t j = 2 * i + 1; j < 2 * i + 2 && j < heap.size(); ++j) {
        assert(heap[i].second <= heap[j].second);
      }
    }
  }

  void swap_parent(std::size_t i) {
    if (i == 0)
      return;
    auto p_i = (i - 1) / 2;
    if (heap[i].second < heap[p_i].second) {
      swap(i, p_i);
      swap_parent(p_i);
    }
  }

  void swap_child(std::size_t i) {
    auto c1 = i * 2 + 1;
    auto c2 = i * 2 + 2;

    auto priority = heap[i].second;
    std::optional<std::size_t> child;
    if (c1 < heap.size() && heap[c1].second < priority) {
      child.emplace(c1);
      priority = heap[c1].second;
    }
    if (c2 < heap.size() && heap[c2].second < priority) {
      child.emplace(c2);
      priority = heap[c2].second;
    }

    if (child.has_value()) {
      swap(i, *child);
      swap_child(*child);
    }
  }

  void insert(auto key, auto value) {
    auto index = heap.size();
    heap.emplace_back(key, value);
    index_map[key] = index;
    swap_parent(index);
    check_heap();
  }

  std::optional<std::pair<K, V>> extract_min() {
    if (heap.empty()) {
      return std::nullopt;
    }

    swap(0, heap.size() - 1);
    auto pair = heap.back();
    heap.pop_back();
    swap_child(0);
    index_map.erase(pair.first);
    check_heap();
    return pair;
  }

  bool decrease_key(auto key, auto value) {
    auto it = index_map.find(key);
    if (it == index_map.end()) {
      insert(key, value);
      return true;
    }

    auto index = it->second;
    auto &old_value = heap[index].second;
    if (old_value < value) {
      return false;
    }

    old_value = value;
    swap_parent(index);
    check_heap();
    return true;
  }

  bool has_key(auto key) { return index_map.contains(key); }

  auto operator[](auto key) { return heap[index_map.at(key)].second; }
};

auto heuristic(const osm_graph &graph, osm_graph::index_t start,
               osm_graph::index_t end) {
  return spherical_distance(graph.nodes[end].location,
                            graph.nodes[start].location);
}

pathfind_result befs(std::stop_token token, const osm_graph &graph, id_t start,
                     id_t end) {
  pathfind_result result;
  using index_t = osm_graph::index_t;

  pf_priority_queue<index_t, double> queue{&result.mem_stat};
  pf_map<index_t, index_t> parent{&result.mem_stat};
  pf_map<index_t, double> dist_so_far{&result.mem_stat};

  queue.insert(start, dist_so_far[start] = 0.0);
  parent[start] = static_cast<index_t>(-1);

  for (std::optional<std::pair<index_t, double>> cur;
       cur = queue.extract_min(), cur.has_value() && !token.stop_requested();) {
    auto [cur_node, est_dist] = *cur;
    if (cur_node == end) {
      construct_path(result, token, parent, graph, start, end);
      break;
    }

    for (const auto [_, next_node] : graph.nodes[cur_node].adj) {
      if (!parent.contains(next_node)) {
        parent[next_node] = cur_node;
        queue.insert(next_node, heuristic(graph, next_node, end));
      }
    }
  }

end:
  return result;
}

pathfind_result heuristic_search(std::stop_token token, const osm_graph &graph,
                                 id_t start, id_t end, auto heuristic) {
  pathfind_result result;
  using index_t = osm_graph::index_t;

  pf_priority_queue<index_t, double> queue{&result.mem_stat};
  pf_map<index_t, index_t> parent{&result.mem_stat};
  pf_map<index_t, double> dist_so_far{&result.mem_stat};

  queue.insert(start, heuristic(graph, start, end));
  parent[start] = static_cast<index_t>(-1);
  dist_so_far[start] = 0.0;

  for (std::optional<std::pair<index_t, double>> cur;
       cur = queue.extract_min(), cur.has_value() && !token.stop_requested();) {
    auto [cur_node, est_dist] = *cur;
    if (cur_node == end) {
      construct_path(result, token, parent, graph, start, end);
      break;
    }

    for (const auto [weight, next_node] : graph.nodes[cur_node].adj) {
      if (token.stop_requested()) {
        goto end;
      }

      auto dist_so_far_next_node = dist_so_far[cur_node] + weight;
      if (!dist_so_far.contains(next_node) ||
          dist_so_far[next_node] > dist_so_far_next_node) {
        dist_so_far[next_node] = dist_so_far_next_node;
        parent[next_node] = cur_node;
        queue.decrease_key(next_node, dist_so_far_next_node +
                                          heuristic(graph, next_node, end));
      }
    }
  }

end:
  return result;
}

pathfind_result ucs(std::stop_token token, const osm_graph &graph, id_t start,
                    id_t end) {
  return heuristic_search(token, graph, start, end,
                          [](auto &&...) { return 0; });
}

pathfind_result a_star(std::stop_token token, const osm_graph &graph,
                       id_t start, id_t end) {
  return heuristic_search(token, graph, start, end, heuristic);
}

} // namespace mapapp

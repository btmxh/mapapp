#include "map_loader.hpp"
#include "spherical.hpp"

#include <fmt/base.h>
#include <glm/vec4.hpp>
#include <numeric>
#include <cstdint>
#include <osmium/io/pbf_input.hpp>
#include <osmium/io/reader.hpp>
#include <osmium/osm/location.hpp>
#include <osmium/osm/way.hpp>
#include <osmium/visitor.hpp>

namespace mapapp {
void map_loader::load(const char *path) {
  osmium::io::Reader reader{osmium::io::File{path}};
  osmium::apply(reader, *this);
}

void map_loader::node(const osmium::Node &node) {
  auto &n = insert(nodes, node);
  n.location = node.location();
  n.position = project_spherical(n.location);
}

void map_loader::way(const osmium::Way &way) {
  if (way.tags().has_key("highway")) {
    highway(way);
  } else {
    structure(way);
  }
}

void map_loader::highway(const osmium::Way &way) {
  auto &w = insert(highways, way);
  w.nodes.resize(way.nodes().size());
  w.oneway = way.get_value_by_key("oneway", "no") == std::string_view{"yes"};
  std::transform(way.nodes().begin(), way.nodes().end(), w.nodes.begin(),
                 [&](const auto &n) { return n.ref(); });
  std::string_view highway_type = way.get_value_by_key("highway");
  if (highway_type == "motorway") {
    w.h_kind = highway::kind::MOTORWAY;
  } else if (highway_type == "trunk") {
    w.h_kind = highway::kind::TRUNK;
  } else if (highway_type == "primary") {
    w.h_kind = highway::kind::PRIMARY;
  } else if (highway_type == "secondary") {
    w.h_kind = highway::kind::SECONDARY;
  } else if (highway_type == "tertiary") {
    w.h_kind = highway::kind::TERTIARY;
  } else {
    w.h_kind = highway::kind::OTHER;
  }
}

void map_loader::structure(const osmium::Way &way) {
  auto &s = insert(structures, way);
  s.nodes.resize(way.nodes().size());
  std::transform(way.nodes().begin(), way.nodes().end(), s.nodes.begin(),
                 [&](const auto &n) { return n.ref(); });
  const auto &tags = way.tags();
  if (tags.has_key("building") || tags.has_key("building:part") ||
      tags.has_key("landuse")) {
    s.s_kind = structure::kind::BUILDING;
  } else if (tags.has_key("natural")) {
    s.s_kind = structure::kind::NATURAL;
  } else if (tags.has_key("amenity")) {
    s.s_kind = structure::kind::AMENITY;
  } else if (tags.has_key("leisure")) {
    s.s_kind = structure::kind::LEISURE;
  } else if (tags.has_key("memorial")) {
    s.s_kind = structure::kind::MEMORIAL;
  } else {
    structures.erase(way.id());
  }
}

glm::dvec2 map_loader::normalize_node_positions() {
  glm::dvec2 avg;
  double count = 0.0;
  for (const auto &[_, node] : nodes) {
    avg = (avg * static_cast<double>(count) + node.position) /
          static_cast<double>(count + 1);
    ++count;
  }
  for (auto &[_, node] : nodes) {
    node.position -= avg;
  }

  return avg;
}

} // namespace mapapp

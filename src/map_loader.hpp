#pragma once

#include <glm/vec2.hpp>
#include <map>
#include <osmium/handler.hpp>
#include <osmium/osm/location.hpp>
#include <osmium/osm/object.hpp>
#include <osmium/osm/types.hpp>
#include <osmium/osm/way.hpp>
#include <string>
#include <vector>

namespace mapapp {
using id_t = osmium::object_id_type;

struct osm_entity {
  id_t id;
  std::string name;
  float z_coord;
};

struct node : public osm_entity {
  glm::dvec2 position;
  osmium::Location location;
};

struct way : public osm_entity {
  std::vector<id_t> nodes;
  float thickness;
};

struct highway : public way {
  enum class kind {
    MOTORWAY,
    TRUNK,
    PRIMARY,
    SECONDARY,
    TERTIARY,
    OTHER,
  };
  kind h_kind;
  bool oneway;
};

struct structure : public way {
  enum class kind {
    BUILDING,
    NATURAL,
    AMENITY,
    LEISURE,
    MEMORIAL,
    NUM_KINDS,
  };
  kind s_kind;
};

class map_loader : public osmium::handler::Handler {
public:
  void load(const char *path);

  void node(const osmium::Node &node);
  void way(const osmium::Way &way);

  void highway(const osmium::Way &way);
  void structure(const osmium::Way &way);

  glm::dvec2 normalize_node_positions();

  template <class T> using MapType = std::map<id_t, T>;
  MapType<struct node> nodes;
  MapType<struct highway> highways;
  MapType<struct structure> structures;

private:
  decltype(auto) insert(auto &map, const osmium::OSMObject &obj) {
    auto &value = map[obj.id()];
    value.id = obj.id();
    value.name = obj.get_value_by_key("name", "");
    value.z_coord = 0.0;
    return value;
  }
};
} // namespace mapapp

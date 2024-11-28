#pragma once

#include <glm/vec2.hpp>
#include <osmium/geom/coordinates.hpp>
#include <osmium/geom/haversine.hpp>
#include <osmium/geom/mercator_projection.hpp>
#include <osmium/osm/location.hpp>

namespace mapapp {
inline glm::dvec2 project_spherical(osmium::Location location) {
  auto coords = osmium::geom::MercatorProjection{}(location);
  return {coords.x, coords.y};
}

inline osmium::Location unproject_spherical(glm::dvec2 pos) {
  auto loc = osmium::geom::mercator_to_lonlat(
      osmium::geom::Coordinates{pos.x, pos.y});
  return {loc.x, loc.y};
}

inline float spherical_distance(osmium::Location loc1, osmium::Location loc2) {
  return osmium::geom::haversine::distance(loc1, loc2);
}

} // namespace mapapp

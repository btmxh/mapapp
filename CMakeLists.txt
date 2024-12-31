cmake_minimum_required(VERSION 3.14)

project(mapapp C CXX)

add_subdirectory(third_party)

file(
  GLOB_RECURSE mapapp_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

add_executable(mapapp "${mapapp_SOURCES}")
target_compile_features(mapapp PRIVATE cxx_std_20)
target_compile_definitions(mapapp PUBLIC NOMINMAX)
target_link_libraries(
  mapapp
  PRIVATE glfw
          fmt::fmt
          glad
          nanoflann::nanoflann
          libosmium
          ZLIB::ZLIB
          earcut_hpp::earcut_hpp
          Polyline2D
          glm::glm
          protozero
          nuklear)

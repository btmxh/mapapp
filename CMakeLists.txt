cmake_minimum_required(VERSION 3.14)

project(mapapp C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(glad REQUIRED)
find_package(Osmium REQUIRED)
find_package(fmt REQUIRED)
find_package(ZLIB REQUIRED)
find_package(glm REQUIRED)

file(
  GLOB_RECURSE mapapp_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

add_executable(mapapp "${mapapp_SOURCES}")
target_compile_features(mapapp PRIVATE cxx_std_20)
target_include_directories(mapapp PRIVATE ${OSMIUM_INCLUDE_DIRS})
target_link_libraries(mapapp PRIVATE glfw fmt::fmt ZLIB::ZLIB glad)

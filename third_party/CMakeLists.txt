set(EARCUT_BUILD_TESTS
    OFF
    CACHE BOOL "")
set(EARCUT_BUILD_BENCH
    OFF
    CACHE BOOL "")
set(EARCUT_BUILD_VIZ
    OFF
    CACHE BOOL "")
set(BUILD_TESTING
    OFF
    CACHE BOOL "")
set(BUILD_EXAMPLES
    OFF
    CACHE BOOL "")
set(NANOFLANN_BUILD_TESTS
    OFF
    CACHE BOOL "")
set(NANOFLANN_BUILD_EXAMPLES
    OFF
    CACHE BOOL "")
set(GLFW_INSTALL
    OFF
    CACHE BOOL "")

add_subdirectory(Polyline2D)
add_subdirectory(earcut.hpp)
add_subdirectory(fmt)
add_subdirectory(glfw)
add_subdirectory(glad-46-core-allexts)
add_subdirectory(glm)
add_subdirectory(nanoflann)

add_library(libosmium INTERFACE)
target_include_directories(
  libosmium INTERFACE "${CMAKE_CURRENT_LIST_DIR}/libosmium/include")
add_library(protozero INTERFACE)
target_include_directories(
  protozero INTERFACE "${CMAKE_CURRENT_LIST_DIR}/protozero/include")
add_library(nuklear INTERFACE)
target_include_directories(nuklear
                           INTERFACE "${CMAKE_CURRENT_LIST_DIR}/nuklear")

include(FetchContent)
FetchContent_Declare(
  zlib-cmake URL https://github.com/jimmy-park/zlib-cmake/archive/main.tar.gz)
FetchContent_MakeAvailable(zlib-cmake)

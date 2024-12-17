#pragma once

#include "gl.hpp"
#include <chrono>
#include <glad/glad.h>

namespace mapapp {
struct gpu_timer {
  query q;
  std::chrono::duration<double> last_time{0};
  bool measuring = false, waiting = false;

  gpu_timer();
  void begin();
  void end();
};
} // namespace mapapp

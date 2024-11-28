#include "gpu_timer.hpp"

namespace mapapp {
gpu_timer::gpu_timer() : q{query::create()} {}

void gpu_timer::begin() {
  if (waiting) {
    GLint avail;
    glGetQueryiv(GL_TIME_ELAPSED, GL_QUERY_RESULT_AVAILABLE, &avail);
    if (avail) {
      GLint result;
      glGetQueryiv(GL_TIME_ELAPSED, GL_QUERY_RESULT, &result);
      last_time = std::chrono::duration<double>{result * 1e-9};
    } else {
      return;
    }
  }

  glBeginQuery(GL_TIME_ELAPSED, q);
  measuring = waiting = true;
}

void gpu_timer::end() {
  if (measuring) {
    glEndQuery(GL_TIME_ELAPSED);
  }
}
} // namespace mapapp

#pragma once

#include "Chrono.hpp"
#include "Config.hpp"

struct GLFWwindow;

namespace dnm {
class Camera;

class Imgui {
 public:
  explicit Imgui(Config* config, GLFWwindow* window);
  ~Imgui();

  void logicFrame(TimeSpan dt, Camera* camera);

  private:
  Config* m_config;
};
}  // namespace dnm
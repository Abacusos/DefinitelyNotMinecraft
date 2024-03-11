#pragma once

#include <Core/Chrono.hpp>
#include <Core/Config.hpp>

struct GLFWwindow;

namespace dnm {
class Camera;

class Imgui {
 public:
  explicit Imgui(Config* config, GLFWwindow* window);
  ~Imgui();

  void logicFrame(TimeSpan dt, const Camera* camera) const;

  private:
  Config* m_config;
};
}  // namespace dnm
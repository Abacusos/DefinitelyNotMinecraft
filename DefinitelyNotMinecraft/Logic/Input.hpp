#pragma once

#include "Chrono.hpp"

struct GLFWwindow;

namespace dnm {
class Camera;
class BlockWorld;

class Input {
 public:
  explicit Input(Camera* camera, GLFWwindow* window, BlockWorld* world);
  void processInput(GLFWwindow* window, TimeSpan dt);

 private:
  GLFWwindow* m_window;
  Camera* m_camera;
  BlockWorld* m_world;

  float m_lastX = 0.0f;
  float m_lastY = 0.0f;
  bool m_firstMouse = true;
};
}  // namespace dnm
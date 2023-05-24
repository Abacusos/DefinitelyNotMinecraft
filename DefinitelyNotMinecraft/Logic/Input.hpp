#pragma once

#include "Chrono.hpp"

struct GLFWwindow;

namespace dnm {
class Camera;

class Input {
 public:
  Input(Camera* camera, GLFWwindow* window);
  void processInput(GLFWwindow* window, TimeSpan dt);

 private:
  GLFWwindow* window;
  Camera* camera;

  float lastX = 0.0f;
  float lastY = 0.0f;
  bool firstMouse = true;
};
}  // namespace dnm
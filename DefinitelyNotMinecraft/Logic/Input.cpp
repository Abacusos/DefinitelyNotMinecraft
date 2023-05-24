#include "Input.hpp"

#include <vulkan/vulkan_raii.hpp>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "Camera.hpp"

namespace dnm {

Input::Input(Camera* camera, GLFWwindow* window)
    : camera{camera}, window{window} {
  glfwSetWindowUserPointer(window, this);

  // tell GLFW to capture our mouse
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

void Input::processInput(GLFWwindow* window, TimeSpan dt) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    camera->processKeyboard(Camera::CameraMovement::FORWARD, dt);
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    camera->processKeyboard(Camera::CameraMovement::BACKWARD, dt);
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    camera->processKeyboard(Camera::CameraMovement::LEFT, dt);
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    camera->processKeyboard(Camera::CameraMovement::RIGHT, dt);

  double xpos = 0.0f;
  double ypos = 0.0f;

  glfwGetCursorPos(window, &xpos, &ypos);

  if (firstMouse) {
    lastX = xpos;
    lastY = ypos;
    firstMouse = false;
  }

  float xoffset = xpos - lastX;
  float yoffset =
      lastY - ypos;  // reversed since y-coordinates go from bottom to top

  lastX = xpos;
  lastY = ypos;

  int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
  if (state == GLFW_PRESS) {
    camera->processMouseMovement(xoffset, yoffset);
  }
}

}  // namespace dnm
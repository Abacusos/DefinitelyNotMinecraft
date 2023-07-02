#include "Input.hpp"

#include <vulkan/vulkan_raii.hpp>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "BlockWorld.hpp"
#include "Camera.hpp"

namespace dnm {

Input::Input(Camera* camera, GLFWwindow* window, BlockWorld* world)
    : m_camera{camera}, m_window{window}, m_world{world} {
  glfwSetWindowUserPointer(window, this);

  // tell GLFW to capture our mouse
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

void Input::processInput(GLFWwindow* window, TimeSpan dt) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    m_camera->processKeyboard(Camera::CameraMovement::FORWARD, dt);
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    m_camera->processKeyboard(Camera::CameraMovement::BACKWARD, dt);
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    m_camera->processKeyboard(Camera::CameraMovement::LEFT, dt);
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    m_camera->processKeyboard(Camera::CameraMovement::RIGHT, dt);

  double xpos = 0.0f;
  double ypos = 0.0f;

  glfwGetCursorPos(window, &xpos, &ypos);

  if (m_firstMouse) {
    m_lastX = xpos;
    m_lastY = ypos;
    m_firstMouse = false;
  }

  float xoffset = xpos - m_lastX;
  float yoffset =
      m_lastY - ypos;  // reversed since y-coordinates go from bottom to top

  m_lastX = xpos;
  m_lastY = ypos;

  int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
  if (state == GLFW_PRESS) {
    m_camera->processMouseMovement(xoffset, yoffset);
  }

  
  if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
    m_world->modifyFirstTracedBlock(m_camera->getPosition(), m_camera->getRotation(),
                                  BlockWorld::BlockAction::Destroy);
  if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
    m_world->modifyFirstTracedBlock(m_camera->getPosition(), m_camera->getRotation(),
                                  BlockWorld::BlockAction::Add);
}

}  // namespace dnm
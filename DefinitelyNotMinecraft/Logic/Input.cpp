#include "Input.hpp"

#include <vulkan/vulkan_raii.hpp>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "BlockWorld.hpp"
#include "Camera.hpp"
#include "Config.hpp"
#include "GizmoRenderingModule.hpp"

namespace dnm {

Input::Input(Camera* camera, GLFWwindow* window, BlockWorld* world,
             Config* config, GizmoRenderingModule* gizmoRendering)
    : m_camera{camera},
      m_window{window},
      m_world{world},
      m_config{config},
      m_gizmoRendering{gizmoRendering} {
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
    m_config->insertionMode = static_cast<u32>(BlockWorld::BlockAction::Add);
  if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
    m_config->insertionMode =
        static_cast<u32>(BlockWorld::BlockAction::Destroy);

  auto positionBlock = m_world->getFirstTracedBlock(m_camera->getPosition(),
                                                    m_camera->getRotation());
  if (positionBlock) {
    glm::vec3 position{
        positionBlock->chunkIndex.x * BlockWorld::chunkLocalSize +
            positionBlock->positionWithinChunk.x,
        positionBlock->positionWithinChunk.y,
        positionBlock->chunkIndex.y * BlockWorld::chunkLocalSize +
            positionBlock->positionWithinChunk.z};

    // slightly push them outwards to make them easier to see
    const glm::vec3 umm = position + glm::vec3{-0.51f, 0.51f, -0.51f};
    const glm::vec3 ump = position + glm::vec3{-0.51f, 0.51f, 0.51f};
    const glm::vec3 upp = position + glm::vec3{0.51f, 0.51f, 0.51f};
    const glm::vec3 upm = position + glm::vec3{0.51f, 0.51f, -0.51f};
    const glm::vec3 lmm = position + glm::vec3{-0.51f, -0.51f, -0.51f};
    const glm::vec3 lmp = position + glm::vec3{-0.51f, -0.51f, 0.51f};
    const glm::vec3 lpp = position + glm::vec3{0.51f, -0.51f, 0.51f};
    const glm::vec3 lpm = position + glm::vec3{0.51f, -0.51f, -0.51f};

    const glm::vec3 red{1.0f, 0.0f, 0.0f};

    std::array<GizmoRenderingModule::VertexGizmo, 24u> vertices{
        GizmoRenderingModule::VertexGizmo{umm, red},
        GizmoRenderingModule::VertexGizmo{ump, red},
        GizmoRenderingModule::VertexGizmo{ump, red},
        GizmoRenderingModule::VertexGizmo{upp, red},
        GizmoRenderingModule::VertexGizmo{upp, red},
        GizmoRenderingModule::VertexGizmo{upm, red},
        GizmoRenderingModule::VertexGizmo{upm, red},
        GizmoRenderingModule::VertexGizmo{umm, red},

        GizmoRenderingModule::VertexGizmo{lmm, red},
        GizmoRenderingModule::VertexGizmo{lmp, red},
        GizmoRenderingModule::VertexGizmo{lmp, red},
        GizmoRenderingModule::VertexGizmo{lpp, red},
        GizmoRenderingModule::VertexGizmo{lpp, red},
        GizmoRenderingModule::VertexGizmo{lpm, red},
        GizmoRenderingModule::VertexGizmo{lpm, red},
        GizmoRenderingModule::VertexGizmo{lmm, red},

        GizmoRenderingModule::VertexGizmo{umm, red},
        GizmoRenderingModule::VertexGizmo{lmm, red},
        GizmoRenderingModule::VertexGizmo{ump, red},
        GizmoRenderingModule::VertexGizmo{lmp, red},
        GizmoRenderingModule::VertexGizmo{upp, red},
        GizmoRenderingModule::VertexGizmo{lpp, red},
        GizmoRenderingModule::VertexGizmo{upm, red},
        GizmoRenderingModule::VertexGizmo{lpm, red},
    };

    m_gizmoRendering->drawLines(vertices);
  }

  if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
    m_world->modifyFirstTracedBlock(positionBlock);
}

}  // namespace dnm
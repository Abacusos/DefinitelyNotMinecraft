#include <Logic/Input.hpp>

#include <vulkan/vulkan_raii.hpp>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <Logic/BlockWorld.hpp>
#include <Logic/Camera.hpp>
#include <Core/Config.hpp>
#include <RenderingModule/GizmoRenderingModule.hpp>
#include <Core/Profiler.hpp>

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
ZoneScoped;
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
  if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
    m_camera->processKeyboard(Camera::CameraMovement::DOWN, dt);
  if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
    m_camera->processKeyboard(Camera::CameraMovement::UP, dt);

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

  auto pos = m_camera->getPosition();
  auto fwd = m_camera->getForward();
  const v3 p{0.01f, 0.0f, 0.0f};
  const v3 pfwd = p + fwd * 3.0f;
  const v3 r{1.0f, 0.0f, 0.0f};
  std::array v{
      GizmoRenderingModule::VertexGizmo{p, r},
      GizmoRenderingModule::VertexGizmo{pfwd, r}};
  m_gizmoRendering->drawLines(v);

  auto positionBlock = m_world->getFirstTracedBlock(m_camera);

  if (positionBlock) {
    v3 position{
        positionBlock->chunkIndex.x * BlockWorld::chunkLocalSize +
            positionBlock->positionWithinChunk.x,
        positionBlock->positionWithinChunk.y,
        positionBlock->chunkIndex.y * BlockWorld::chunkLocalSize +
            positionBlock->positionWithinChunk.z};

    // slightly push them outwards to make them easier to see
    const v3 umm = position + v3{-0.51f, 0.51f, -0.51f};
    const v3 ump = position + v3{-0.51f, 0.51f, 0.51f};
    const v3 upp = position + v3{0.51f, 0.51f, 0.51f};
    const v3 upm = position + v3{0.51f, 0.51f, -0.51f};
    const v3 lmm = position + v3{-0.51f, -0.51f, -0.51f};
    const v3 lmp = position + v3{-0.51f, -0.51f, 0.51f};
    const v3 lpp = position + v3{0.51f, -0.51f, 0.51f};
    const v3 lpm = position + v3{0.51f, -0.51f, -0.51f};

    constexpr v3 red{1.0f, 0.0f, 0.0f};

    std::array vertices{
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

  if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS)
    m_config->disableImgui = true;

  if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS)
    m_config->disableImgui = false;
 }

}  // namespace dnm
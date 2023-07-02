#include "Camera.hpp"

#include <iostream>

namespace dnm {

namespace {

glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

struct BufferContent {
  glm::mat4x4 viewMatrix;
  glm::vec4 cameraPosition;
};
}

Camera::Camera(Config* config, Renderer* renderer) : m_config{config} {
  m_viewBuffer = renderer->createBuffer(sizeof(BufferContent),
                                        vk::BufferUsageFlagBits::eUniformBuffer,
                                        "View Matrix");
  m_viewBufferRegistration =
      renderer->registerRAIIBuffer(GlobalBuffers::CameraView, m_viewBuffer);
}

void Camera::processKeyboard(CameraMovement direction, TimeSpan deltaTime) {
  if (direction == Camera::CameraMovement::FORWARD) m_forward = 1.0f;
  if (direction == Camera::CameraMovement::BACKWARD) m_forward = -1.0f;
  if (direction == Camera::CameraMovement::LEFT) m_side = -1.0f;
  if (direction == Camera::CameraMovement::RIGHT) m_side = 1.0f;

  m_dirty = true;
}

void Camera::processMouseMovement(float xoffset, float yoffset,
                                  bool constrainPitch) {
  m_accumulatedXChange += xoffset;
  m_accumulatedYChange += yoffset;
  m_dirty = true;
}

bool Camera::update(TimeSpan deltaTime) {
  if (!m_dirty) {
    return false;
  }

  float moveSpeed = deltaTime.count() * m_movementSpeed * 2.0f;
  float rotSpeed = deltaTime.count() * m_rotationSpeed * 50.0f;

  m_rotation.y += m_accumulatedXChange * rotSpeed;
  m_rotation.x += m_accumulatedYChange * rotSpeed;

  if (m_rotation.x > 89.0f) {
    m_rotation.x = 89.0f;
  }
  if (m_rotation.x < -89.0f) {
    m_rotation.x = -89.0f;
  }

  glm::vec3 direction;
  direction.x = cos(glm::radians(m_rotation.y)) * cos(glm::radians(m_rotation.x));
  direction.y = -sin(glm::radians(m_rotation.x));
  direction.z = sin(glm::radians(m_rotation.y)) * cos(glm::radians(m_rotation.x));
  glm::vec3 cameraFront = glm::normalize(direction);

  m_position += (moveSpeed * cameraFront) * m_forward;
  m_position +=
      (glm::normalize(glm::cross(cameraFront, cameraUp)) * rotSpeed) * m_side;

  BufferContent content;
  content.viewMatrix = glm::lookAt(m_position, m_position + cameraFront, cameraUp);
  content.cameraPosition = glm::vec4(m_position, 1.0f);

  copyToDevice(m_viewBuffer.deviceMemory,
               std::span<const BufferContent>(&content, 1u));

  m_dirty = false;
  m_accumulatedXChange = 0.0f;
  m_accumulatedYChange = 0.0f;

  m_forward = 0.0f;
  m_side = 0.0f;

  return true;
}

}  // namespace dnm
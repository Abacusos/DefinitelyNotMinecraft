#include "Camera.hpp"

namespace dnm {

Camera::Camera(Config* config, Renderer* renderer) : m_config{config} {
  m_viewBuffer = renderer->createBuffer(sizeof(glm::mat4x4),
                                        vk::BufferUsageFlagBits::eUniformBuffer,
                                        "View Matrix");
  m_viewBufferRegistration =
      renderer->registerRAIIBuffer(GlobalBuffers::CameraView, m_viewBuffer);
}

void Camera::processKeyboard(CameraMovement direction, TimeSpan deltaTime) {
  glm::vec3 camFront;
  camFront.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
  camFront.y = sin(glm::radians(rotation.x));
  camFront.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
  camFront = glm::normalize(camFront);

  float moveSpeed = deltaTime.count() * movementSpeed;

  if (direction == Camera::CameraMovement::FORWARD)
    position += camFront * moveSpeed;
  if (direction == Camera::CameraMovement::BACKWARD)
    position -= camFront * moveSpeed;
  if (direction == Camera::CameraMovement::LEFT)
    position -=
        glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) *
        moveSpeed;
  if (direction == Camera::CameraMovement::RIGHT)
    position +=
        glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) *
        moveSpeed;

  dirty = true;
}

void Camera::processMouseMovement(float xoffset, float yoffset,
                                  bool constrainPitch) {
  accumulatedXChange += xoffset;
  accumulatedYChange += yoffset;
  dirty = true;
}

bool Camera::update(TimeSpan deltaTime) {
  if (!dirty) {
    return false;
  }

  float moveSpeed = deltaTime.count() * movementSpeed * 2.0f;
  float rotSpeed = deltaTime.count() * rotationSpeed * 50.0f;

  rotation.y += accumulatedXChange * rotSpeed;
  rotation.x += accumulatedYChange * rotSpeed;

  glm::mat4 rotM = glm::mat4(1.0f);
  glm::mat4 transM;

  rotM =
      glm::rotate(rotM, glm::radians(rotation.x * (m_config->flipY ? -1.0f : 1.0f)), glm::vec3(1.0f, 0.0f, 0.0f));
  rotM =
      glm::rotate(rotM, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
  rotM =
      glm::rotate(rotM, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

  glm::vec3 translation = position;
  if (m_config->flipY) {
    translation.y *= -1.0f;
  }
  transM = glm::translate(glm::mat4(1.0f), translation);

  view = rotM * transM;

  viewPosition = glm::vec4(position, 0.0f) * glm::vec4(-1.0f, 1.0f, -1.0f, 1.0f);

  copyToDevice(m_viewBuffer.deviceMemory, std::span<const glm::mat4x4>(&view, 1u));

  dirty = false;
  accumulatedXChange = 0.0f;
  accumulatedYChange = 0.0f;

  return true;
}

}  // namespace dnm
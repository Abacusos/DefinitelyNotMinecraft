#include <Logic/Camera.hpp>

#include <Core/Formatter.hpp>

namespace dnm {

namespace {
struct BufferContent {
  m4 viewMatrix;
  v4 cameraPosition;
};
}  // namespace

Camera::Camera(Config* config, Renderer* renderer) : m_config{config} {
  m_viewBuffer = renderer->createBuffer(sizeof(BufferContent),
                                        vk::BufferUsageFlagBits::eUniformBuffer,
                                        "View Matrix");
  m_viewBufferRegistration =
      renderer->registerRAIIBuffer(GlobalBuffers::CameraView, m_viewBuffer);

  updateViewMatrix();
}

void Camera::processKeyboard(CameraMovement direction, TimeSpan deltaTime) {
  if (direction == CameraMovement::FORWARD) m_forward = 1.0f;
  if (direction == CameraMovement::BACKWARD) m_forward = -1.0f;
  if (direction == CameraMovement::LEFT) m_side = -1.0f;
  if (direction == CameraMovement::RIGHT) m_side = 1.0f;
  if (direction == CameraMovement::DOWN) m_up = -1.0f;
  if (direction == CameraMovement::UP) m_up = 1.0f;

  m_dirty = true;
}

void Camera::processMouseMovement(float xoffset, float yoffset) {
  m_accumulatedXChange += xoffset / 1920;
  m_accumulatedYChange += yoffset / 1078;
  m_dirty = true;
}

bool Camera::update(TimeSpan deltaTime) {
  ZoneScoped;
  if (!m_dirty) {
    return false;
  }

  m_rotation.y += -m_accumulatedXChange * m_rotationSpeed;
  m_rotation.x += m_accumulatedYChange * m_rotationSpeed;
  if (m_rotation.x > 89.0f) {
    m_rotation.x = 89.0f;
  }
  if (m_rotation.x < -89.0f) { 
    m_rotation.x = -89.0f;
  }

  float moveSpeed = deltaTime.count() * m_movementSpeed;

  const v3 forward = getForward();
  const v3 up = getUp();
  const v3 right = getRight();

  m_position += forward * moveSpeed * m_forward;
  m_position += up * moveSpeed * m_up;
  m_position += right * moveSpeed * m_side;

  updateViewMatrix();

  BufferContent content;
  content.viewMatrix = m_viewMatrix;
  content.cameraPosition = v4(m_position, 1.0f);

  copyToDevice(m_viewBuffer.deviceMemory,
               std::span<const BufferContent>(&content, 1u));

  m_dirty = false;
  m_accumulatedXChange = 0.0f;
  m_accumulatedYChange = 0.0f;

  m_forward = 0.0f;
  m_side = 0.0f;
  m_up = 0.0f;

  return true;
}

v3 Camera::getForward() const {
  // The camera is facing at -negative z, so negate here
  return -normalize(v3(m_cameraTransform[2]));
}

v3 Camera::getUp() const { return normalize(v3(m_cameraTransform[1])); }

v3 Camera::getRight() const { return normalize(v3(m_cameraTransform[0])); }

v3 Camera::getPosition() const { return m_position; }

m4 Camera::getViewMatrix() const { return m_viewMatrix; }

void Camera::updateViewMatrix() {
  m_cameraTransform = mat4_cast(
      glm::quat(v3(glm::radians(m_rotation.x), glm::radians(m_rotation.y),
                   glm::radians(m_rotation.z))));

  m4 translation = glm::identity<m4>();
  translation = translate(translation, m_position);
  m_cameraTransform = translation * m_cameraTransform;
  m_viewMatrix = inverse(m_cameraTransform);
}

}  // namespace dnm
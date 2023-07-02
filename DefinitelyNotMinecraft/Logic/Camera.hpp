#pragma once

#include "Chrono.hpp"
#include "Renderer.hpp"
#include "GLMInclude.hpp"
#include "Config.hpp"

namespace dnm {

class Camera {
 public:

  enum class CameraMovement { FORWARD, BACKWARD, LEFT, RIGHT };

  explicit Camera(Config* config, Renderer* renderer);

  void processKeyboard(CameraMovement direction, TimeSpan deltaTime);

  void processMouseMovement(float xoffset, float yoffset,
                            bool constrainPitch = true);

  bool update(TimeSpan deltaTime);

  glm::vec3 getRotation() const { return m_rotation; }
  glm::vec3 getPosition() const { return m_position; }

 private:
  Config* m_config;
  // Moving the camera away from 0 0 to not show the problems with world generation into negative direction
  glm::vec3 m_position = glm::vec3(500.0f, 140.0f, 500.0f);
  glm::vec3 m_rotation = glm::vec3(30.0f, 30.0f, 0.0f);

  float m_rotationSpeed = 0.001f;
  float m_movementSpeed = 0.01f;

  bool m_dirty = true;
  float m_accumulatedXChange = 0.0f;
  float m_accumulatedYChange = 0.0f;
  float m_forward = 0.0f;
  float m_side = 0.0f;

  BufferData m_viewBuffer{nullptr};
  std::unique_ptr<BufferRegistration> m_viewBufferRegistration{nullptr};
};
}  // namespace dnm
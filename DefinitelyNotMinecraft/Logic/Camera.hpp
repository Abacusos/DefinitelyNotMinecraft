#pragma once

#include "Chrono.hpp"
#include "Renderer.hpp"
#include "GLMInclude.hpp"
#include "Config.hpp"

namespace dnm {

class Camera {
 public:

  enum class CameraMovement { FORWARD, BACKWARD, LEFT, RIGHT };

  Camera(Config* config, Renderer* renderer);

  void processKeyboard(CameraMovement direction, TimeSpan deltaTime);

  void processMouseMovement(float xoffset, float yoffset,
                            bool constrainPitch = true);

  bool update(TimeSpan deltaTime);

  glm::vec3 getRotation() const { return rotation; }
  glm::vec3 getPosition() const { return viewPosition; }

 private:
  Config* m_config;
  glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
  glm::vec3 rotation = glm::vec3(0.0f, 135.0f, 0.0f);
  glm::vec4 viewPosition = glm::vec4();

  float rotationSpeed = 0.001f;
  float movementSpeed = 0.01f;

  glm::mat4 view;

  bool dirty = true;
  float accumulatedXChange = 0.0f;
  float accumulatedYChange = 0.0f;

  BufferData m_viewBuffer{nullptr};
  std::unique_ptr<BufferRegistration> m_viewBufferRegistration{nullptr};
};
}  // namespace dnm
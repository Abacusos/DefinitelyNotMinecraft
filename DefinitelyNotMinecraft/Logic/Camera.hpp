#pragma once

#include <Core/Chrono.hpp>
#include <Core/Config.hpp>
#include <Core/GLMInclude.hpp>

#include <Rendering/Renderer.hpp>

namespace dnm
{
class Camera {
    public:
    enum class CameraMovement
    {
        FORWARD,
        BACKWARD,
        LEFT,
        RIGHT,
        UP,
        DOWN
    };

    explicit Camera(Config* config, Renderer* renderer);

    void processKeyboard(CameraMovement direction, TimeSpan deltaTime);

    void processMouseMovement(float xoffset, float yoffset);

    bool update(TimeSpan deltaTime);

    v3 getForward() const;
    v3 getUp() const;
    v3 getRight() const;
    v3 getPosition() const;
    m4 getViewMatrix() const;

    private:
    void updateViewMatrix();

    private:
    Config* m_config;
    v3      m_position = v3(500.0f, 140.0f, 500.0f);
    v3      m_rotation = v3(0.0f, 0.0f, 0.0f);
    m4      m_cameraTransform;
    m4      m_viewMatrix;

    float m_rotationSpeed = 30.0f;
    float m_movementSpeed = 0.01f;

    bool  m_dirty              = true;
    float m_accumulatedXChange = 0.0f;
    float m_accumulatedYChange = 0.0f;
    float m_forward            = 0.0f;
    float m_side               = 0.0f;
    float m_up                 = 0.0f;

    BufferData                          m_viewBuffer {nullptr};
    std::unique_ptr<BufferRegistration> m_viewBufferRegistration {nullptr};
};
}   // namespace dnm

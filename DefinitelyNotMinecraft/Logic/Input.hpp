#pragma once

#include <Core/Chrono.hpp>

struct GLFWwindow;

namespace dnm
{
    struct GizmoData;
    class Camera;
    class BlockWorld;
    struct Config;

    class Input
    {
    public:
        explicit Input(Camera* camera, GLFWwindow* window, BlockWorld* world,
                       Config* config, GizmoData* gizmoData);
        void processInput(GLFWwindow* window, TimeSpan dt);

    private:
        GLFWwindow* m_window;
        Camera* m_camera;
        BlockWorld* m_world;
        Config* m_config;
        GizmoData* m_gizmoData;

        float m_lastX = 0.0f;
        float m_lastY = 0.0f;
        bool m_firstMouse = true;
    };
} // namespace dnm

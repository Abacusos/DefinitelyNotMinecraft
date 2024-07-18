#pragma once

#include <Core/Chrono.hpp>

struct GLFWwindow;

namespace dnm
{
    class Camera;
    class BlockWorld;
    struct Config;
    class GizmoRenderingModule;

    class Input
    {
    public:
        explicit Input(Camera* camera, GLFWwindow* window, BlockWorld* world,
                       Config* config, GizmoRenderingModule* gizmoRendering);
        void processInput(GLFWwindow* window, TimeSpan dt);

    private:
        GLFWwindow* m_window;
        Camera* m_camera;
        BlockWorld* m_world;
        Config* m_config;
        GizmoRenderingModule* m_gizmoRendering;

        float m_lastX = 0.0f;
        float m_lastY = 0.0f;
        bool m_firstMouse = true;
    };
} // namespace dnm

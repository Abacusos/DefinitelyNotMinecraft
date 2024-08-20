#include "Logic/Imgui.hpp"

#include "imgui_impl_glfw.h"

#include <Core/Profiler.hpp>

#include <Logic/Camera.hpp>

namespace dnm
{
Imgui::Imgui(Config* config, GLFWwindow* window) : m_config {config} {
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(window, true);
}

Imgui::~Imgui() {
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void Imgui::logicFrame(TimeSpan dt, const Camera* camera) const {
    ZoneScoped;
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();

    if (m_config->disableImgui) {
        ImGui::Begin("Definitely not minecraft");

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

        const auto position = camera->getPosition();
        ImGui::Text("Camera position %.1f %.1f %.1f", position.x, position.y, position.z);
        const auto rotation = camera->getForward();
        ImGui::Text("Camera forward %.1f %.1f %.1f", rotation.x, rotation.y, rotation.z);

        ImGui::Text("Modification mode %d", m_config->insertionMode);

        ImGui::Checkbox("Generate draw calls every frame", &m_config->everyFrameGenerateDrawCalls);

        ImGui::Checkbox("Culling enabled", &m_config->cullingEnabled);

        ImGui::Checkbox("Limit frames", &m_config->limitFrames);

        ImGui::InputFloat3("Looking at", &m_config->lookingAt.x);

        ImGui::BeginGroup();

        ImGui::InputInt("Light Count", &m_config->lightCount);
        ImGui::InputFloat("Ambient Strength", &m_config->ambientStrength);
        ImGui::InputFloat("Specular Strength", &m_config->specularStrength);

        ImGui::InputFloat3("Light Color", &m_config->lightColor.x);
        ImGui::InputFloat3("Light Pos", &m_config->lightPosition.x);
        ImGui::InputFloat3("Light Color2", &m_config->lightColor2.x);
        ImGui::InputFloat3("Light Pos2", &m_config->lightPosition2.x);
        ImGui::InputFloat3("Light Color3", &m_config->lightColor3.x);
        ImGui::InputFloat3("Light Pos3", &m_config->lightPosition3.x);

        ImGui::Checkbox("Update Light Buffer", &m_config->updateLight);

        ImGui::EndGroup();

        ImGui::End();
    }
}
}   // namespace dnm

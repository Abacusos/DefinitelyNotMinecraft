#include "Imgui.hpp"

#include "Camera.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"

namespace dnm {
Imgui::Imgui(Config* config, GLFWwindow* window) : m_config{config} {
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

void Imgui::logicFrame(TimeSpan dt, Camera* camera) {
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  ImGuiIO& io = ImGui::GetIO();

  ImGui::Begin("Definitely not minecraft");

  ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
              1000.0f / io.Framerate, io.Framerate);

  auto position = camera->getPosition();
  ImGui::Text("Camera position %.1f %.1f %.1f", position.x, position.y,
              position.z);
  auto rotation = camera->getRotation();
  ImGui::Text("Camera rotation %.1f %.1f %.1f", rotation.x, rotation.y,
              rotation.z);

  ImGui::Checkbox("Use Inverse Depth (broken)", &m_config->useInverseDepth);
  ImGui::Checkbox("FlipY", &m_config->flipY);
  ImGui::Checkbox("Recreate SwapChain", &m_config->recreateSwapChain);
  ImGui::End();
}
}  // namespace dnm
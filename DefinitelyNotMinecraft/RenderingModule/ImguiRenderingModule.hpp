#pragma once

#include <Rendering/Renderer.hpp>
#include <Core/Config.hpp>

namespace dnm {

class ImguiRenderingModule {
 public:
  explicit ImguiRenderingModule(Config* config, Renderer* renderer);
  ~ImguiRenderingModule();

  void drawFrame(const vk::raii::Framebuffer& frameBuffer, TimeSpan dt, const vk::raii::Semaphore& previousRenderStepFinished) const;

  vk::raii::Semaphore& getRenderingFinishedSemaphore();

 private:
  void recreatePipeline();

  void uploadFontTexture();

 private:
  Config* m_config;
  Renderer* m_renderer;

  dnm::TextureData m_fontTexture{nullptr};

  vk::raii::RenderPass m_renderPass{nullptr};
  vk::raii::DescriptorSetLayout m_descriptorSetLayout{nullptr};
  vk::raii::PipelineLayout m_pipelineLayout{nullptr};

  vk::raii::DescriptorPool m_descriptorPool{nullptr};
  vk::raii::DescriptorSet m_descriptorSet{nullptr};
  
  vk::raii::CommandBuffer m_commandBuffer{nullptr};
  
  vk::raii::Semaphore m_renderingFinished{nullptr};

};
}  // namespace dnm

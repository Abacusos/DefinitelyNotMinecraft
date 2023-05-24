#pragma once

#include "BlockWorld.hpp"
#include "Renderer.hpp"
#include "ShaderWatcher.hpp"
#include "Config.hpp"

namespace dnm {

class BlockRenderingModule {
 public:
  explicit BlockRenderingModule(Config* config, Renderer* renderer, ShaderWatcher* watcher,
                                BlockWorld* blockWorld);

  void drawFrame(const vk::raii::Framebuffer& frameBuffer, TimeSpan dt,
                 bool cameraMoved);

  vk::raii::Semaphore& getRenderingFinishedSemaphore();

 private:
  void recreatePipeline();
  bool recompileShadersIfNecessary();

 private:
  Config* m_config;
  Renderer* m_renderer;
  ShaderWatcher* m_watcher;
  BlockWorld* m_blockWorld;

  dnm::Shader m_vertexShaderModule{nullptr};
  dnm::Shader m_fragmentShaderModule{nullptr};
  dnm::Shader m_drawCallGenerationComputeModule{nullptr};

  dnm::BufferData m_vertexBuffer{nullptr};
  dnm::BufferData m_transformBuffer{nullptr};
  dnm::BufferData m_drawCommandBuffer{nullptr};
  dnm::BufferData m_worldDataBuffer{nullptr};
  dnm::BufferData m_blockTypeBuffer{nullptr};
  dnm::BufferData m_chunkConstantsBuffer{nullptr};
  dnm::TextureData m_textureData{nullptr};

  vk::raii::DescriptorSetLayout m_descriptorSetLayout{nullptr};
  vk::raii::PipelineLayout m_pipelineLayout{nullptr};

  vk::raii::DescriptorPool m_descriptorPool{nullptr};
  vk::raii::DescriptorSet m_descriptorSet{nullptr};

  vk::raii::Pipeline m_graphicsPipeline{nullptr};
  vk::raii::Pipeline m_computePipeline{nullptr};

  vk::raii::CommandBuffer m_commandBufferCompute{nullptr};
  vk::raii::CommandBuffer m_commandBuffer{nullptr};
  vk::raii::Semaphore m_drawCallGenerationFinished{nullptr};
  vk::raii::Semaphore m_renderingFinished{nullptr};
};
}  // namespace dnm

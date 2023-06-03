#pragma once

#include "BlockWorld.hpp"
#include "Config.hpp"
#include "Handle.hpp"
#include "Renderer.hpp"

namespace dnm {

class ShaderManager;
class StringInterner;

class BlockRenderingModule {
 public:
  explicit BlockRenderingModule(Config* config, Renderer* renderer,
                                ShaderManager* manager, BlockWorld* blockWorld,
                                StringInterner* interner);

  void drawFrame(const vk::raii::Framebuffer& frameBuffer, TimeSpan dt,
                 bool cameraMoved, glm::vec3 cameraPosition);

  vk::raii::Semaphore& getRenderingFinishedSemaphore();

 private:
  void recreatePipeline();
  void recompileShadersIfNecessary(bool force = false);

  void recreateBlockDependentBuffers();

 private:
  Config* m_config;
  Renderer* m_renderer;
  ShaderManager* m_shaderManager;
  BlockWorld* m_blockWorld;
  StringInterner* m_interner;

  ShaderHandle m_vertexHandle;
  ShaderHandle m_fragmentHandle;
  ShaderHandle m_computeHandle;

  vk::raii::ShaderModule m_vertexShaderModule{nullptr};
  vk::raii::ShaderModule m_fragmentShaderModule{nullptr};
  vk::raii::ShaderModule m_drawCallGenerationComputeModule{nullptr};

  dnm::BufferData m_vertexBuffer{nullptr};
  dnm::BufferData m_transformBuffer{nullptr};
  dnm::BufferData m_drawCommandBuffer{nullptr};
  dnm::BufferData m_worldDataBuffer{nullptr};
  dnm::BufferData m_blockTypeBuffer{nullptr};
  dnm::BufferData m_chunkConstantsBuffer{nullptr};
  dnm::BufferData m_chunkRemapIndex{nullptr};
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

  u32 loadCountChunksLastFrame = 0u;
  glm::ivec2 m_cameraChunkLastFrame{-10000, -10000};
  bool m_allChunksUploadedLastFrame = false;
};
}  // namespace dnm

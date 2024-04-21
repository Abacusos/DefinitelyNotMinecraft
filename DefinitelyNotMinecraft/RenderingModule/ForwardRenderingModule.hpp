#pragma once

#include <Logic/BlockWorld.hpp>
#include <Core/Config.hpp>
#include <Core/GPUProfiller.hpp>
#include <Core/Handle.hpp>
#include <Rendering/Renderer.hpp>

namespace dnm {

class Camera;
class ShaderManager;
class StringInterner;

class ForwardRenderingModule {
 public:
  explicit ForwardRenderingModule(Config* config, Renderer* renderer,
                                ShaderManager* manager, BlockWorld* blockWorld,
                                StringInterner* interner);

  void drawFrame(const vk::raii::Framebuffer& frameBuffer,
                 const vk::raii::Semaphore& previousRenderStepFinished);

  vk::raii::Semaphore& getRenderingFinishedSemaphore();

 private:
  void recreatePipeline();
  void recompileShadersIfNecessary(bool force = false);

 private:
  Config* m_config;
  Renderer* m_renderer;
  ShaderManager* m_shaderManager;
  BlockWorld* m_blockWorld;
  StringInterner* m_interner;

  ShaderHandle m_vertexHandle;
  ShaderHandle m_fragmentHandle;

  vk::raii::ShaderModule m_vertexShaderModule{nullptr};
  vk::raii::ShaderModule m_fragmentShaderModule{nullptr};

  dnm::BufferData m_lightBuffer{nullptr};
  dnm::TextureData m_textureData{nullptr};

  vk::raii::DescriptorSetLayout m_descriptorSetLayout{nullptr};
  vk::raii::PipelineLayout m_pipelineLayout{nullptr};

  vk::raii::DescriptorSet m_descriptorSet{nullptr};

  vk::raii::Pipeline m_graphicsPipeline{nullptr};

  vk::raii::CommandBuffer m_commandBuffer{nullptr};
  vk::raii::Semaphore m_renderingFinished{nullptr};

  GPUProfilerContext m_renderingProfilerContext;
};
}  // namespace dnm

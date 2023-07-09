#pragma once

#include "BlockWorld.hpp"
#include "Config.hpp"
#include "Handle.hpp"
#include "Renderer.hpp"

namespace dnm {

class ShaderManager;
class StringInterner;

class GizmoRenderingModule {
 public:
  explicit GizmoRenderingModule(Config* config, Renderer* renderer,
                                ShaderManager* manager, BlockWorld* blockWorld,
                                StringInterner* interner);
  
  struct VertexGizmo {
    glm::vec3 x, y, z;  // Position data
    glm::vec3 r, g, b;  // color
  };
  void drawLines(std::span<VertexGizmo> lineElements);

  void drawFrame(const vk::raii::Framebuffer& frameBuffer, TimeSpan dt,
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
  ShaderHandle m_computeHandle;

  vk::raii::ShaderModule m_vertexShaderModule{nullptr};
  vk::raii::ShaderModule m_fragmentShaderModule{nullptr};

  dnm::BufferData m_vertexBuffer{nullptr};

  vk::raii::RenderPass m_renderPass{nullptr};
  vk::raii::DescriptorSetLayout m_descriptorSetLayout{nullptr};
  vk::raii::PipelineLayout m_pipelineLayout{nullptr};

  vk::raii::DescriptorPool m_descriptorPool{nullptr};
  vk::raii::DescriptorSet m_descriptorSet{nullptr};

  vk::raii::Pipeline m_graphicsPipeline{nullptr};

  vk::raii::CommandBuffer m_commandBuffer{nullptr};
  vk::raii::Semaphore m_renderingFinished{nullptr};

  constexpr static u32 reservedGizmoSpace = 4000;
  std::array<VertexGizmo, reservedGizmoSpace> m_verticesGizmo;
  u32 m_occupiedVertexPlaces = 0u;
};
}  // namespace dnm

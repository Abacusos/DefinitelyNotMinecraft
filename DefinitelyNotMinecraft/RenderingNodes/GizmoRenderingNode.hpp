#pragma once

#include <Core/Config.hpp>
#include <Core/Handle.hpp>
#include <Logic/BlockWorld.hpp>
#include <Logic/GizmoData.hpp>
#include <Rendering/Renderer.hpp>
#include <RenderingNodes/IRenderingNode.hpp>

namespace dnm {
class ShaderManager;
class StringInterner;

class GizmoRenderingNode : public IRenderingNode {
 public:
  explicit GizmoRenderingNode(Config* config, Renderer* renderer,
                              ShaderManager* manager, BlockWorld* blockWorld,
                              StringInterner* interner, GizmoData* gizmoData);

  std::string_view getName() const override;
  bool shouldExecute() const override;
  ExecutionResult execute(const ExecutionData& executionData,
                          vk::raii::CommandBuffer& commandBuffer) override;

 private:
  void recreatePipeline();
  void recompileShadersIfNecessary(bool force = false);

 public:
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

  vk::raii::DescriptorSet m_descriptorSet{nullptr};

  vk::raii::Pipeline m_graphicsPipeline{nullptr};

  GizmoData* m_gizmoData{nullptr};
};
}  // namespace dnm

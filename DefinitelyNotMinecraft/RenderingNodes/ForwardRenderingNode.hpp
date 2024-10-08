#pragma once

#include <Core/Config.hpp>
#include <Core/GPUProfiller.hpp>
#include <Core/Handle.hpp>

#include <Logic/BlockWorld.hpp>

#include <Rendering/Renderer.hpp>
#include <RenderingNodes/IRenderingNode.hpp>

namespace dnm
{
class Camera;
class ShaderManager;
class StringInterner;

class ForwardRenderingNode : public IRenderingNode {
    public:
    explicit ForwardRenderingNode(Config* config, Renderer* renderer, ShaderManager* manager, BlockWorld* blockWorld, StringInterner* interner);

    std::string_view getName() const override;
    bool             shouldExecute() const override;
    ExecutionResult  execute(const ExecutionData& executionData, vk::raii::CommandBuffer& commandBuffer) override;

    private:
    void recreatePipeline();
    void recompileShadersIfNecessary(bool force = false);

    private:
    Config*         m_config;
    Renderer*       m_renderer;
    ShaderManager*  m_shaderManager;
    BlockWorld*     m_blockWorld;
    StringInterner* m_interner;

    ShaderHandle m_vertexHandle;
    ShaderHandle m_fragmentHandle;

    vk::raii::ShaderModule m_vertexShaderModule {nullptr};
    vk::raii::ShaderModule m_fragmentShaderModule {nullptr};

    dnm::BufferData  m_lightConstants {nullptr};
    dnm::BufferData  m_perLightBuffer {nullptr};
    dnm::TextureData m_textureData {nullptr};

    vk::raii::DescriptorSetLayout m_descriptorSetLayout {nullptr};
    vk::raii::PipelineLayout      m_pipelineLayout {nullptr};

    vk::raii::DescriptorSet m_descriptorSet {nullptr};

    vk::raii::Pipeline m_graphicsPipeline {nullptr};

    GPUProfilerContext m_renderingProfilerContext;

    struct PerLightBuffer
    {
        v3    lightColor;
        float padding = 0.0f;
        v3    lightPos;
        float padding2 = 0.0f;
    };
    constexpr static u32 lightLength = 10;
    std::array<PerLightBuffer, lightLength * lightLength> lightTest;
};
}   // namespace dnm

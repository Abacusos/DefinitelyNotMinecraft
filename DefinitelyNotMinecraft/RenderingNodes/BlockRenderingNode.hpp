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

class BlockDrawCallNode : public IRenderingNode {
    public:
    explicit BlockDrawCallNode(Config* config, Renderer* renderer, ShaderManager* manager, BlockWorld* blockWorld, StringInterner* interner);

    std::string_view getName() const override;
    bool             shouldExecute() const override;
    ExecutionResult  execute(const ExecutionData& executionData, vk::raii::CommandBuffer& commandBuffer) override;

    private:
    void recreatePipeline();
    void recompileShadersIfNecessary(bool force = false);

    void recreateBlockDependentBuffers();
    bool updateBlockWorldData(v3 cameraPosition);
    void updateCullingData(const Camera* camera) const;

    Config*         m_config;
    Renderer*       m_renderer;
    ShaderManager*  m_shaderManager;
    BlockWorld*     m_blockWorld;
    StringInterner* m_interner;

    ShaderHandle m_computeHandle;

    vk::raii::ShaderModule m_drawCallGenerationComputeModule {nullptr};

    dnm::BufferData m_transformBuffer {nullptr};
    dnm::BufferData m_drawCommandBuffer {nullptr};
    dnm::BufferData m_worldDataBuffer {nullptr};
    dnm::BufferData m_blockTypeBuffer {nullptr};
    dnm::BufferData m_chunkConstantsBuffer {nullptr};
    dnm::BufferData m_chunkRemapIndex {nullptr};
    dnm::BufferData m_cullingData {nullptr};

    std::unique_ptr<BufferRegistration> m_transformRegistration {nullptr};
    std::unique_ptr<BufferRegistration> m_drawCommandRegistration {nullptr};
    std::unique_ptr<BufferRegistration> m_blockTypeRegistration {nullptr};

    vk::raii::DescriptorSetLayout m_descriptorSetLayout {nullptr};
    vk::raii::PipelineLayout      m_pipelineLayout {nullptr};

    vk::raii::DescriptorSet m_descriptorSet {nullptr};

    vk::raii::Pipeline m_computePipeline {nullptr};

    GPUProfilerContext m_computeProfilerContext;

    u32        loadCountChunksLastFrame = 0u;
    glm::ivec2 m_cameraChunkLastFrame {-10000, -10000};
    bool       m_allChunksUploadedLastFrame = false;
};
}   // namespace dnm

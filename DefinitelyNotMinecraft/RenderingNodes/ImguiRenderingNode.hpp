#pragma once

#include <Rendering/Renderer.hpp>
#include <Core/Config.hpp>
#include <RenderingNodes/IRenderingNode.hpp>

namespace dnm
{
    class ImguiRenderingNode : public IRenderingNode
    {
    public:
        explicit ImguiRenderingNode(Config* config, Renderer* renderer);
        ~ImguiRenderingNode();

        void drawFrame(const vk::raii::Framebuffer& frameBuffer, TimeSpan dt,
                       const vk::raii::Semaphore& previousRenderStepFinished) const;

        vk::raii::Semaphore& getRenderingFinishedSemaphore();

        std::string_view getName() const override;
        bool shouldExecute() const override;
        ExecutionResult execute(
            const ExecutionData& executionData,
            vk::raii::CommandBuffer& commandBuffer) override;
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
    };
} // namespace dnm

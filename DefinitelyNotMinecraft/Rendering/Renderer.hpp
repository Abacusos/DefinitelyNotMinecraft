#pragma once

#include <Core/Chrono.hpp>
#include <Core/Config.hpp>
#include <Core/GLMInclude.hpp>
#include <Rendering/GlobalBuffers.hpp>
#include <Rendering/RAIIUtils.hpp>
#include <functional>
#include <vulkan/vulkan_raii.hpp>

namespace dnm
{
    class ShaderWatcher;

    class Renderer
    {
    public:
        explicit Renderer(Config* config);
        ~Renderer();

        u32 prepareDrawFrame();
        void finishDrawFrame(vk::raii::Semaphore& finishedRendering, u32 imageIndex);

        GLFWwindow* getGLFWwindow() const;
        vk::Extent2D getExtent() const;

        const vk::raii::Instance& getInstance() const;
        const vk::raii::PhysicalDevice& getPhysicalDevice() const;
        const vk::raii::Device& getDevice() const;
        const vk::raii::PipelineCache& getPipelineCache() const;
        const vk::raii::RenderPass& getRenderPass() const;
        const vk::raii::Queue& getComputeQueue() const;
        const vk::raii::Queue& getGraphicsQueue() const;
        const vk::raii::Framebuffer& getFrameBuffer(u32 imageIndex) const;
        const vk::raii::DescriptorPool& getDescriptorPool() const;

        vk::Format getColorFormat() const;

        struct FamilyIndices
        {
            u32 graphicsQueueFamilyIndex;
            u32 presentQueueFamilyIndex;
        };

        FamilyIndices getIndices() const;

        vk::raii::CommandBuffer getCommandBuffer() const;
        BufferData createBuffer(vk::DeviceSize size, vk::BufferUsageFlags flags,
                                std::string_view debugName,
                                vk::MemoryPropertyFlags propertyFlags =
                                    vk::MemoryPropertyFlagBits::eHostVisible |
                                    vk::MemoryPropertyFlagBits::eHostCoherent) const;

        std::unique_ptr<BufferRegistration> registerRAIIBuffer(
            GlobalBuffers bufferIdentifier, const BufferData& buffer);
        void registerBuffer(GlobalBuffers bufferIdentifier, const BufferData& buffer);
        void removeBufferRegistration(GlobalBuffers bufferIdentifier);
        const vk::raii::Buffer* getGlobalBuffer(GlobalBuffers bufferIdentifier) const;

        void oneTimeSubmit(
            std::function<void(const vk::raii::CommandBuffer& commandBuffer)>
            function) const;

        void waitIdle() const;

        m4 getProjectionMatrix() const;

    private:
        void recreateSwapChainFromWindow();
        void recreateSwapChain();

    private:
        Config* m_config;

        vk::raii::Context m_context;
        vk::raii::Instance m_instance{nullptr};
        vk::raii::PhysicalDevice m_physicalDevice{nullptr};
        FamilyIndices m_familyIndices;
        SurfaceData m_surfaceData{nullptr};
        vk::raii::Device m_device{nullptr};
        vk::raii::CommandPool m_commandPool{nullptr};
        vk::raii::DescriptorPool m_descriptorPool{nullptr};
        vk::raii::Queue m_computeQueue{nullptr};
        vk::raii::Queue m_graphicsQueue{nullptr};
        vk::raii::Queue m_presentQueue{nullptr};
        SwapChainData m_swapChainData{nullptr};
        DepthBufferData m_depthBufferData{nullptr};
        vk::raii::RenderPass m_renderPass{nullptr};
        std::vector<vk::raii::Framebuffer> m_framebuffers;
        vk::raii::PipelineCache m_pipelineCache{nullptr};
        vk::raii::Fence m_drawFence{nullptr};
        vk::raii::Semaphore m_imageAcquiredSemaphore{nullptr};

        vk::Format m_colorFormat;

        // In theory this buffer needs to be a weak ptr to make sure that we notice if
        // something was removed already For my use case, I don't really care add and
        // remove things at runtime
        std::array<const vk::raii::Buffer*,
                   static_cast<u32>(GlobalBuffers::Undefined)>
        m_globalBuffers;

        BufferData m_projection{nullptr};
        std::unique_ptr<BufferRegistration> m_projectionClipRegistration{nullptr};
    };
} // namespace dnm

#include <Rendering/Renderer.hpp>

#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif

#include <Core/Math.hpp>
#include <Core/Profiler.hpp>
#include <Core/ShortTypes.hpp>
#include <Rendering/RAIIUtils.hpp>

namespace dnm
{
    Renderer::Renderer(Config* config) : m_config{config}
    {
        m_instance = makeInstance(m_context, "Definitely not Minecraft", "EngineName",
                                  {}, getInstanceExtensions(), VK_API_VERSION_1_2);
#if !defined(NDEBUG)
        vk::raii::DebugUtilsMessengerEXT debugUtilsMessenger(
            m_instance, makeDebugUtilsMessengerCreateInfoEXT());
#endif
        m_physicalDevice = vk::raii::PhysicalDevices(m_instance).front();

        std::vector<vk::ExtensionProperties> extensionProperties =
            m_physicalDevice.enumerateDeviceExtensionProperties();

        auto supportedFeatures =
            m_physicalDevice.getFeatures2<vk::PhysicalDeviceFeatures2,
                                          vk::PhysicalDeviceVulkan11Features,
                                          vk::PhysicalDeviceVulkan12Features>();

        m_surfaceData = SurfaceData(m_instance, "Definitely not Minecraft",
                                    vk::Extent2D(1920, 1017));

        std::pair<u32, u32> graphicsAndPresentQueueFamilyIndex =
            findGraphicsAndPresentQueueFamilyIndex(m_physicalDevice,
                                                   m_surfaceData.surface);
        m_device =
            makeDevice(m_physicalDevice, graphicsAndPresentQueueFamilyIndex.first,
                       getDeviceExtensions(),
                       &supportedFeatures.get<vk::PhysicalDeviceFeatures2>().features,
                       &supportedFeatures.get<vk::PhysicalDeviceVulkan11Features>());
        registerDebugMarker(m_device, "DNM Device");

        m_commandPool = vk::raii::CommandPool(
            m_device, {
                vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                graphicsAndPresentQueueFamilyIndex.first
            });

        std::array sizes{
            vk::DescriptorPoolSize{vk::DescriptorType::eSampler, 1000u},
            vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, 1000u},
            vk::DescriptorPoolSize{vk::DescriptorType::eSampledImage, 1000u},
            vk::DescriptorPoolSize{vk::DescriptorType::eStorageImage, 1000u},
            vk::DescriptorPoolSize{vk::DescriptorType::eUniformTexelBuffer, 1000u},
            vk::DescriptorPoolSize{vk::DescriptorType::eStorageTexelBuffer, 1000u},
            vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer, 1000u},
            vk::DescriptorPoolSize{vk::DescriptorType::eStorageBuffer, 1000u},
            vk::DescriptorPoolSize{vk::DescriptorType::eUniformBufferDynamic, 1000u},
            vk::DescriptorPoolSize{vk::DescriptorType::eStorageBufferDynamic, 1000u},
            vk::DescriptorPoolSize{vk::DescriptorType::eInputAttachment, 1000u},
        };

        m_descriptorPool = makeDescriptorPool(m_device, sizes);

        m_graphicsQueue =
            vk::raii::Queue(m_device, graphicsAndPresentQueueFamilyIndex.first, 0);
        m_computeQueue =
            vk::raii::Queue(m_device, graphicsAndPresentQueueFamilyIndex.first, 0);
        m_presentQueue =
            vk::raii::Queue(m_device, graphicsAndPresentQueueFamilyIndex.second, 0);

        auto device = static_cast<VkDevice>(*m_device);
        auto physicalDevice = static_cast<VkPhysicalDevice>(*m_physicalDevice);
        auto queue = static_cast<VkQueue>(*m_graphicsQueue);

        m_pipelineCache =
            vk::raii::PipelineCache(m_device, vk::PipelineCacheCreateInfo());

        m_projection = createBuffer(
            sizeof(m4), vk::BufferUsageFlagBits::eUniformBuffer, "Projection Matrix");
        m_projectionClipRegistration =
            registerRAIIBuffer(GlobalBuffers::ProjectionClip, m_projection);

        recreateSwapChain();

        m_drawFence = vk::raii::Fence(m_device, {vk::FenceCreateFlagBits::eSignaled});
        m_imageAcquiredSemaphore =
            vk::raii::Semaphore(m_device, vk::SemaphoreCreateInfo());
    }

    Renderer::~Renderer() { m_device.waitIdle(); }

    u32 Renderer::prepareDrawFrame()
    {
        ZoneScoped;
        {
            ZoneScopedN("Wait on fence");
            while (vk::Result::eTimeout ==
                m_device.waitForFences({*m_drawFence}, VK_TRUE, FenceTimeout));
        }

        // Get the index of the next available swapchain image:
        vk::Result result;
        u32 imageIndex;
        {
            ZoneScopedN("Acquire next image");
            std::tie(result, imageIndex) = m_swapChainData.swapChain.acquireNextImage(
                FenceTimeout, *m_imageAcquiredSemaphore);
        }

        if (result == vk::Result::eErrorOutOfDateKHR)
        {
            recreateSwapChainFromWindow();
            return std::numeric_limits<u32>::max();
        }
        else if (result != vk::Result::eSuccess &&
            result != vk::Result::eSuboptimalKHR)
        {
            throw std::runtime_error("failed to acquire swap chain image!");
        }
        assert(imageIndex < m_swapChainData.images.size());
        m_drawFence = vk::raii::Fence(m_device, vk::FenceCreateInfo());

        return imageIndex;
    }

    void Renderer::finishDrawFrame(vk::raii::Semaphore& finishedRendering,
                                   u32 imageIndex)
    {
        ZoneScoped;
        vk::PipelineStageFlags waitDestinationStageMask(
            vk::PipelineStageFlagBits::eColorAttachmentOutput);
        const std::array submitInfo = {
            vk::SubmitInfo{*m_imageAcquiredSemaphore, waitDestinationStageMask},
            vk::SubmitInfo{*finishedRendering, waitDestinationStageMask}
        };
        m_graphicsQueue.submit(submitInfo, *m_drawFence);

        const vk::PresentInfoKHR presentInfoKHR(nullptr, *m_swapChainData.swapChain,
                                                imageIndex);

        const vk::Result result = m_presentQueue.presentKHR(presentInfoKHR);
        switch (result)
        {
        case vk::Result::eSuccess:
            break;
        case vk::Result::eErrorOutOfDateKHR:
            [[fallthrough]];
        case vk::Result::eSuboptimalKHR:
            recreateSwapChainFromWindow();
            break;
        default:
            assert(false); // an unexpected result is returned !
        }
    }

    GLFWwindow* Renderer::getGLFWwindow() const
    {
        return m_surfaceData.window.handle;
    }

    vk::Extent2D Renderer::getExtent() const { return m_surfaceData.extent; }

    const vk::raii::Instance& Renderer::getInstance() const { return m_instance; }

    const vk::raii::PhysicalDevice& Renderer::getPhysicalDevice() const
    {
        return m_physicalDevice;
    }

    const vk::raii::Device& Renderer::getDevice() const { return m_device; }

    const vk::raii::PipelineCache& Renderer::getPipelineCache() const
    {
        return m_pipelineCache;
    }

    const vk::raii::RenderPass& Renderer::getRenderPass() const
    {
        return m_renderPass;
    }

    const vk::raii::Queue& Renderer::getComputeQueue() const
    {
        return m_computeQueue;
    }

    const vk::raii::Queue& Renderer::getGraphicsQueue() const
    {
        return m_graphicsQueue;
    }

    const vk::raii::Framebuffer& Renderer::getFrameBuffer(u32 imageIndex) const
    {
        return m_framebuffers[imageIndex];
    }

    const vk::raii::DescriptorPool& Renderer::getDescriptorPool() const
    {
        return m_descriptorPool;
    }

    vk::Format Renderer::getColorFormat() const { return m_colorFormat; }

    Renderer::FamilyIndices Renderer::getIndices() const { return m_familyIndices; }

    vk::raii::CommandBuffer Renderer::getCommandBuffer() const
    {
        return makeCommandBuffer(m_device, m_commandPool);
    }

    BufferData Renderer::createBuffer(vk::DeviceSize size,
                                      vk::BufferUsageFlags flags,
                                      std::string_view debugName,
                                      vk::MemoryPropertyFlags propertyFlags) const
    {
        auto result =
            BufferData(m_physicalDevice, m_device, size, flags, propertyFlags);
        registerDebugMarker(m_device, result.buffer, debugName);
        return result;
    }

    std::unique_ptr<BufferRegistration> Renderer::registerRAIIBuffer(

        GlobalBuffers bufferIdentifier, const BufferData& buffer)
    {
        registerBuffer(bufferIdentifier, buffer);
        return std::make_unique<BufferRegistration>(bufferIdentifier, this);
    }

    void Renderer::registerBuffer(GlobalBuffers bufferIdentifier,
                                  const BufferData& buffer)
    {
        m_globalBuffers[static_cast<u32>(bufferIdentifier)] = &(buffer.buffer);
    }

    void Renderer::removeBufferRegistration(GlobalBuffers bufferIdentifier)
    {
        m_globalBuffers[static_cast<u32>(bufferIdentifier)] = nullptr;
    }

    const vk::raii::Buffer* Renderer::getGlobalBuffer(
        GlobalBuffers bufferIdentifier) const
    {
        return m_globalBuffers[static_cast<u32>(bufferIdentifier)];
    }

    void Renderer::oneTimeSubmit(
        std::function<void(const vk::raii::CommandBuffer& commandBuffer)> function)
    const
    {
        dnm::oneTimeSubmit(m_device, m_commandPool, m_graphicsQueue,
                           std::move(function));
    }

    void Renderer::waitIdle() const { m_device.waitIdle(); }

    m4 Renderer::getProjectionMatrix() const
    {
        return createProjectionMatrix(
            glm::vec2(m_surfaceData.extent.width, m_surfaceData.extent.height),
            m_config->nearPlane, m_config->farPlane);
    }

    void Renderer::recreateSwapChainFromWindow()
    {
        i32 width = 0, height = 0;
        glfwGetFramebufferSize(m_surfaceData.window.handle, &width, &height);
        while (width == 0 || height == 0)
        {
            glfwGetFramebufferSize(m_surfaceData.window.handle, &width, &height);
            glfwWaitEvents();
        }
        m_surfaceData.extent =
            vk::Extent2D{static_cast<u32>(width), static_cast<u32>(height)};
        recreateSwapChain();
    }

    void Renderer::recreateSwapChain()
    {
        m_device.waitIdle();

        const std::pair<u32, u32> graphicsAndPresentQueueFamilyIndex =
            findGraphicsAndPresentQueueFamilyIndex(m_physicalDevice,
                                                   m_surfaceData.surface);

        m_swapChainData = SwapChainData(
            m_physicalDevice, m_device, m_surfaceData.surface, m_surfaceData.extent,
            vk::ImageUsageFlagBits::eColorAttachment |
            vk::ImageUsageFlagBits::eTransferSrc,
            &m_swapChainData.swapChain, graphicsAndPresentQueueFamilyIndex.first,
            graphicsAndPresentQueueFamilyIndex.second);

        m_depthBufferData = DepthBufferData(
            m_physicalDevice, m_device, vk::Format::eD32Sfloat, m_surfaceData.extent);

        m_colorFormat = pickSurfaceFormat(m_physicalDevice.getSurfaceFormatsKHR(
                *m_surfaceData.surface))
            .format;
        m_renderPass =
            makeRenderPass(m_device, m_colorFormat, m_depthBufferData.format);

        m_framebuffers =
            makeFramebuffers(m_device, m_renderPass, m_swapChainData.imageViews,
                             &m_depthBufferData.imageView, m_surfaceData.extent);

        std::array matrices{getProjectionMatrix()};
        copyToDevice(m_projection.deviceMemory, std::span<const m4>(matrices));
    }
} // namespace dnm

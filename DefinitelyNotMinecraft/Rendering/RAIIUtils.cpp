#include "Rendering/RAIIUtils.hpp"

#include <Rendering/Renderer.hpp>

#include <Shader/ShaderManager.hpp>

namespace dnm
{
vk::raii::DeviceMemory dnm::allocateDeviceMemory(
  const vk::raii::Device&                   device,
  const vk::PhysicalDeviceMemoryProperties& memoryProperties,
  const vk::MemoryRequirements&             memoryRequirements,
  vk::MemoryPropertyFlags                   memoryPropertyFlags) {
    const u32                    memoryTypeIndex = findMemoryType(memoryProperties, memoryRequirements.memoryTypeBits, memoryPropertyFlags);
    const vk::MemoryAllocateInfo memoryAllocateInfo(memoryRequirements.size, memoryTypeIndex);
    return vk::raii::DeviceMemory(device, memoryAllocateInfo);
}

WindowData dnm::createWindow(const std::string& windowName, const vk::Extent2D& extent) {
    struct glfwContext
    {
        glfwContext() {
            glfwInit();
            glfwSetErrorCallback(
              [](int error, const char* msg)
              {
                  std::cerr << "glfw: "
                            << "(" << error << ") " << msg << std::endl;
              });
        }

        ~glfwContext() { glfwTerminate(); }
    };

    static auto glfwCtx = glfwContext();
    (void)glfwCtx;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(extent.width, extent.height, windowName.c_str(), nullptr, nullptr);
    return WindowData(window, windowName, extent);
}

u32 dnm::findGraphicsQueueFamilyIndex(const std::vector<vk::QueueFamilyProperties>& queueFamilyProperties) {
    // get the first index into queueFamiliyProperties which supports graphics
    const std::vector<vk::QueueFamilyProperties>::const_iterator graphicsQueueFamilyProperty = std::find_if(
      queueFamilyProperties.begin(),
      queueFamilyProperties.end(),
      [](const vk::QueueFamilyProperties& qfp) { return qfp.queueFlags & vk::QueueFlagBits::eGraphics && qfp.queueFlags & vk::QueueFlagBits::eCompute; });
    assert(graphicsQueueFamilyProperty != queueFamilyProperties.end());
    return static_cast<u32>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));
}

std::pair<u32, u32> dnm::findGraphicsAndPresentQueueFamilyIndex(const vk::raii::PhysicalDevice& physicalDevice, const vk::raii::SurfaceKHR& surface) {
    const std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
    assert(queueFamilyProperties.size() < std::numeric_limits<u32>::max());

    u32 graphicsQueueFamilyIndex = findGraphicsQueueFamilyIndex(queueFamilyProperties);
    if (physicalDevice.getSurfaceSupportKHR(graphicsQueueFamilyIndex, *surface)) {
        return std::make_pair(
          graphicsQueueFamilyIndex,
          graphicsQueueFamilyIndex);   // the first graphicsQueueFamilyIndex does
                                       // also support presents
    }

    // the graphicsQueueFamilyIndex doesn't support present -> look for another
    // family index that supports both graphics and present
    for (size_t i = 0; i < queueFamilyProperties.size(); i++) {
        if ((queueFamilyProperties [i].queueFlags & vk::QueueFlagBits::eGraphics) && physicalDevice.getSurfaceSupportKHR(static_cast<u32>(i), *surface)) {
            return std::make_pair(static_cast<u32>(i), static_cast<u32>(i));
        }
    }

    // there's nothing like a single family index that supports both graphics and
    // present -> look for another family index that supports present
    for (size_t i = 0; i < queueFamilyProperties.size(); i++) {
        if (physicalDevice.getSurfaceSupportKHR(static_cast<u32>(i), *surface)) {
            return std::make_pair(graphicsQueueFamilyIndex, static_cast<u32>(i));
        }
    }

    throw std::runtime_error("Could not find queues for both graphics or present -> terminating");
}

u32 findComputeQueueFamilyIndex(const vk::raii::PhysicalDevice& physicalDevice) {
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
    assert(queueFamilyProperties.size() < std::numeric_limits<u32>::max());

    const std::vector<vk::QueueFamilyProperties>::iterator computeQueueFamilyProperty = std::find_if(
      queueFamilyProperties.begin(),
      queueFamilyProperties.end(),
      [](const vk::QueueFamilyProperties& qfp) { return qfp.queueFlags & vk::QueueFlagBits::eCompute; });

    if (computeQueueFamilyProperty != queueFamilyProperties.end()) {
        return static_cast<u32>(std::distance(queueFamilyProperties.begin(), computeQueueFamilyProperty));
    }

    throw std::runtime_error("Could not find a queue for compute -> terminating");
}

u32 findMemoryType(const vk::PhysicalDeviceMemoryProperties& memoryProperties, u32 typeBits, vk::MemoryPropertyFlags requirementsMask) {
    u32 typeIndex = u32(~0);
    for (u32 i = 0; i < memoryProperties.memoryTypeCount; i++) {
        if ((typeBits & 1) && ((memoryProperties.memoryTypes [i].propertyFlags & requirementsMask) == requirementsMask)) {
            typeIndex = i;
            break;
        }
        typeBits >>= 1;
    }
    assert(typeIndex != u32(~0));
    return typeIndex;
}

vk::raii::Pipeline makeComputePipeline(
  const vk::raii::Device&         device,
  const vk::raii::PipelineCache&  pipelineCache,
  const vk::raii::ShaderModule&   computeShaderModule,
  const vk::SpecializationInfo*   vertexShaderSpecializationInfo,
  const vk::raii::PipelineLayout& pipelineLayout) {
    const vk::PipelineShaderStageCreateInfo stageCreateInfo(
      {}, vk::ShaderStageFlagBits::eCompute, *computeShaderModule, "main", vertexShaderSpecializationInfo);

    const vk::ComputePipelineCreateInfo computeCreateInfo({}, stageCreateInfo, *pipelineLayout);

    return vk::raii::Pipeline(device, pipelineCache, computeCreateInfo);
}

std::vector<const char*> dnm::gatherExtensions(
  const std::vector<std::string>& extensions
#if !defined(NDEBUG)
  ,
  const std::vector<vk::ExtensionProperties>& extensionProperties
#endif
) {
    std::vector<const char*> enabledExtensions;
    enabledExtensions.reserve(extensions.size());
    for (const auto& ext : extensions) {
        assert(
          std::find_if(extensionProperties.begin(), extensionProperties.end(), [ext](const vk::ExtensionProperties& ep) { return ext == ep.extensionName; })
          != extensionProperties.end());
        enabledExtensions.push_back(ext.data());
    }
#if !defined(NDEBUG)
    if (
      std::find(extensions.begin(), extensions.end(), VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == extensions.end()
      && std::find_if(
           extensionProperties.begin(),
           extensionProperties.end(),
           [](const vk::ExtensionProperties& ep) { return (strcmp(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, ep.extensionName) == 0); })
           != extensionProperties.end()) {
        enabledExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
#endif
    return enabledExtensions;
}

std::vector<const char*> dnm::gatherLayers(
  const std::vector<std::string>& layers
#if !defined(NDEBUG)
  ,
  const std::vector<vk::LayerProperties>& layerProperties
#endif
) {
    std::vector<const char*> enabledLayers;
    enabledLayers.reserve(layers.size());
    for (const auto& layer : layers) {
        assert(
          std::find_if(layerProperties.begin(), layerProperties.end(), [layer](const vk::LayerProperties& lp) { return layer == lp.layerName; })
          != layerProperties.end());
        enabledLayers.push_back(layer.data());
    }
#if !defined(NDEBUG)
    // Enable standard validation layer to find as much errors as possible!
    if (
      std::find(layers.begin(), layers.end(), "VK_LAYER_KHRONOS_validation") == layers.end()
      && std::find_if(
           layerProperties.begin(),
           layerProperties.end(),
           [](const vk::LayerProperties& lp) { return (strcmp("VK_LAYER_KHRONOS_validation", lp.layerName) == 0); })
           != layerProperties.end()) {
        enabledLayers.push_back("VK_LAYER_KHRONOS_validation");
    }
#endif
    return enabledLayers;
}

std::vector<std::string> dnm::getDeviceExtensions() {
    return {VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME, VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME};
}

std::vector<std::string> dnm::getInstanceExtensions() {
    std::vector<std::string> extensions;
    extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#if !defined(NDEBUG)
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif
    return extensions;
}

vk::raii::CommandBuffer dnm::makeCommandBuffer(const vk::raii::Device& device, const vk::raii::CommandPool& commandPool) {
    const vk::CommandBufferAllocateInfo commandBufferAllocateInfo(*commandPool, vk::CommandBufferLevel::ePrimary, 1);
    return std::move(vk::raii::CommandBuffers(device, commandBufferAllocateInfo).front());
}

vk::raii::DescriptorPool dnm::makeDescriptorPool(const vk::raii::Device& device, std::span<const vk::DescriptorPoolSize> poolSizes) {
    assert(!poolSizes.empty());
    const u32 maxSets =
      std::accumulate(poolSizes.begin(), poolSizes.end(), 0, [](u32 sum, const vk::DescriptorPoolSize& dps) { return sum + dps.descriptorCount; });
    assert(0 < maxSets);

    const vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, maxSets, poolSizes);
    return vk::raii::DescriptorPool(device, descriptorPoolCreateInfo);
}

vk::raii::DescriptorSetLayout makeDescriptorSetLayout(
  const vk::raii::Device&                                              device,
  std::span<std::tuple<vk::DescriptorType, u32, vk::ShaderStageFlags>> bindingData,
  vk::DescriptorSetLayoutCreateFlags                                   flags) {
    std::vector<vk::DescriptorSetLayoutBinding> bindings(bindingData.size());
    for (size_t i = 0; i < bindingData.size(); i++) {
        bindings [i] =
          vk::DescriptorSetLayoutBinding(dnm::checked_cast<u32>(i), std::get<0>(bindingData [i]), std::get<1>(bindingData [i]), std::get<2>(bindingData [i]));
    }
    const vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo(flags, bindings);
    return vk::raii::DescriptorSetLayout(device, descriptorSetLayoutCreateInfo);
}

vk::raii::DescriptorSetLayout makeDescriptorSetLayout(
  const vk::raii::Device&            device,
  std::span<BindingSlot>             bindingData,
  vk::ShaderStageFlags               stageFlags,
  vk::DescriptorSetLayoutCreateFlags flags) {
    std::vector<vk::DescriptorSetLayoutBinding> bindings(bindingData.size());
    for (size_t i = 0; i < bindingData.size(); i++) {
        bindings [i] = vk::DescriptorSetLayoutBinding {bindingData [i].bindingSlot, bindingData [i].type, 1u, stageFlags};
    }
    const vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo(flags, bindings);
    return {device, descriptorSetLayoutCreateInfo};
}

vk::raii::Device dnm::makeDevice(
  const vk::raii::PhysicalDevice&   physicalDevice,
  u32                               queueFamilyIndex,
  const std::vector<std::string>&   extensions,
  const vk::PhysicalDeviceFeatures* physicalDeviceFeatures,
  const void*                       pNext) {
    std::vector<const char*> enabledExtensions;
    enabledExtensions.reserve(extensions.size());
    for (const auto& ext : extensions) {
        enabledExtensions.push_back(ext.data());
    }

    const float                queuePriority = 0.0f;
    vk::DeviceQueueCreateInfo  deviceQueueCreateInfo(vk::DeviceQueueCreateFlags(), queueFamilyIndex, 1, &queuePriority);
    const vk::DeviceCreateInfo deviceCreateInfo(vk::DeviceCreateFlags(), deviceQueueCreateInfo, {}, enabledExtensions, physicalDeviceFeatures, pNext);
    return vk::raii::Device(physicalDevice, deviceCreateInfo);
}

std::vector<vk::raii::Framebuffer> dnm::makeFramebuffers(
  const vk::raii::Device&                 device,
  vk::raii::RenderPass&                   renderPass,
  const std::vector<vk::raii::ImageView>& imageViews,
  const vk::raii::ImageView*              pDepthImageView,
  const vk::Extent2D&                     extent) {
    vk::ImageView attachments [2];
    attachments [1] = pDepthImageView ? **pDepthImageView : vk::ImageView();

    const vk::FramebufferCreateInfo framebufferCreateInfo(
      vk::FramebufferCreateFlags(), *renderPass, pDepthImageView ? 2 : 1, attachments, extent.width, extent.height, 1);
    std::vector<vk::raii::Framebuffer> framebuffers;
    framebuffers.reserve(imageViews.size());
    for (const auto& imageView : imageViews) {
        attachments [0] = *imageView;
        framebuffers.push_back(vk::raii::Framebuffer(device, framebufferCreateInfo));
    }

    return framebuffers;
}

vk::raii::Pipeline dnm::makeGraphicsPipeline(
  const Config*                                  config,
  const vk::raii::Device&                        device,
  const vk::raii::PipelineCache&                 pipelineCache,
  const vk::raii::ShaderModule&                  vertexShaderModule,
  const vk::SpecializationInfo*                  vertexShaderSpecializationInfo,
  const vk::raii::ShaderModule&                  fragmentShaderModule,
  const vk::SpecializationInfo*                  fragmentShaderSpecializationInfo,
  u32                                            vertexStride,
  const std::vector<std::pair<vk::Format, u32>>& vertexInputAttributeFormatOffset,
  vk::FrontFace                                  frontFace,
  bool                                           depthBuffered,
  const vk::raii::PipelineLayout&                pipelineLayout,
  const vk::raii::RenderPass&                    renderPass,
  vk::PrimitiveTopology                          topology) {
    std::array<vk::PipelineShaderStageCreateInfo, 2> pipelineShaderStageCreateInfos = {
      vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, *vertexShaderModule, "main", vertexShaderSpecializationInfo),
      vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, *fragmentShaderModule, "main", fragmentShaderSpecializationInfo)};

    std::vector<vk::VertexInputAttributeDescription> vertexInputAttributeDescriptions;
    vk::PipelineVertexInputStateCreateInfo           pipelineVertexInputStateCreateInfo;
    vk::VertexInputBindingDescription                vertexInputBindingDescription(0, vertexStride);

    if (0 < vertexStride) {
        vertexInputAttributeDescriptions.reserve(vertexInputAttributeFormatOffset.size());
        for (u32 i = 0; i < vertexInputAttributeFormatOffset.size(); i++) {
            vertexInputAttributeDescriptions.emplace_back(i, 0, vertexInputAttributeFormatOffset [i].first, vertexInputAttributeFormatOffset [i].second);
        }
        pipelineVertexInputStateCreateInfo.setVertexBindingDescriptions(vertexInputBindingDescription);
        pipelineVertexInputStateCreateInfo.setVertexAttributeDescriptions(vertexInputAttributeDescriptions);
    }

    vk::PipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo(vk::PipelineInputAssemblyStateCreateFlags(), topology);

    vk::PipelineViewportStateCreateInfo pipelineViewportStateCreateInfo(vk::PipelineViewportStateCreateFlags(), 1, nullptr, 1, nullptr);

    vk::PipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo(
      vk::PipelineRasterizationStateCreateFlags(), false, false, vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack, frontFace, false, 0.0f, 0.0f, 0.0f, 1.0f);

    vk::PipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo({}, vk::SampleCountFlagBits::e1);

    vk::StencilOpState                      stencilOpState(vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::CompareOp::eNever);
    vk::StencilOpState                      stencilOpState2(vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::CompareOp::eAlways);
    vk::PipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo(
      vk::PipelineDepthStencilStateCreateFlags(), depthBuffered, depthBuffered, vk::CompareOp::eGreaterOrEqual, false, false, stencilOpState, stencilOpState2);

    vk::ColorComponentFlags colorComponentFlags(
      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    vk::PipelineColorBlendAttachmentState pipelineColorBlendAttachmentState(
      false,
      vk::BlendFactor::eZero,
      vk::BlendFactor::eZero,
      vk::BlendOp::eAdd,
      vk::BlendFactor::eZero,
      vk::BlendFactor::eZero,
      vk::BlendOp::eAdd,
      colorComponentFlags);
    vk::PipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo(
      vk::PipelineColorBlendStateCreateFlags(),
      false,
      vk::LogicOp::eClear,
      pipelineColorBlendAttachmentState,
      {
        {0.0f, 0.0f, 0.0f, 0.0f}
    });

    std::array<vk::DynamicState, 2>    dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo(vk::PipelineDynamicStateCreateFlags(), dynamicStates);

    vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo(
      vk::PipelineCreateFlags(),
      pipelineShaderStageCreateInfos,
      &pipelineVertexInputStateCreateInfo,
      &pipelineInputAssemblyStateCreateInfo,
      nullptr,
      &pipelineViewportStateCreateInfo,
      &pipelineRasterizationStateCreateInfo,
      &pipelineMultisampleStateCreateInfo,
      &pipelineDepthStencilStateCreateInfo,
      &pipelineColorBlendStateCreateInfo,
      &pipelineDynamicStateCreateInfo,
      *pipelineLayout,
      *renderPass);

    return vk::raii::Pipeline(device, pipelineCache, graphicsPipelineCreateInfo);
}

vk::raii::Image dnm::makeImage(const vk::raii::Device& device) {
    const vk::ImageCreateInfo imageCreateInfo(
      {},
      vk::ImageType::e2D,
      vk::Format::eB8G8R8A8Unorm,
      vk::Extent3D(640, 640, 1),
      1,
      1,
      vk::SampleCountFlagBits::e1,
      vk::ImageTiling::eLinear,
      vk::ImageUsageFlagBits::eTransferSrc);
    return vk::raii::Image(device, imageCreateInfo);
}

vk::raii::Instance dnm::makeInstance(
  const vk::raii::Context&        context,
  const std::string&              appName,
  const std::string&              engineName,
  const std::vector<std::string>& layers,
  const std::vector<std::string>& extensions,
  u32                             apiVersion) {
    vk::ApplicationInfo      applicationInfo(appName.c_str(), 1, engineName.c_str(), 1, apiVersion);
    std::vector<const char*> enabledLayers = gatherLayers(
      layers
#if !defined(NDEBUG)
      ,
      context.enumerateInstanceLayerProperties()
#endif
    );
    std::vector<const char*> enabledExtensions = gatherExtensions(
      extensions
#if !defined(NDEBUG)
      ,
      context.enumerateInstanceExtensionProperties()
#endif
    );
#if defined(NDEBUG)
    // in non-debug mode just use the InstanceCreateInfo for instance creation
    vk::StructureChain<vk::InstanceCreateInfo> instanceCreateInfo({{}, &applicationInfo, enabledLayers, enabledExtensions});
#else
    // in debug mode, addionally use the debugUtilsMessengerCallback in instance
    // creation!
    vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    vk::DebugUtilsMessageTypeFlagsEXT     messageTypeFlags(
      vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

    vk::StructureChain<vk::InstanceCreateInfo, vk::DebugUtilsMessengerCreateInfoEXT> instanceCreateInfo(
      {{}, &applicationInfo, enabledLayers, enabledExtensions}, {{}, severityFlags, messageTypeFlags, &debugUtilsMessengerCallback});
#endif

    return vk::raii::Instance(context, instanceCreateInfo.get<vk::InstanceCreateInfo>());
}

vk::raii::RenderPass dnm::makeRenderPass(
  const vk::raii::Device& device,
  vk::Format              colorFormat,
  vk::Format              depthFormat,
  vk::AttachmentLoadOp    loadOp,
  vk::ImageLayout         colorFinalLayout) {
    std::vector<vk::AttachmentDescription> attachmentDescriptions;
    assert(colorFormat != vk::Format::eUndefined);
    attachmentDescriptions.emplace_back(
      vk::AttachmentDescriptionFlags(),
      colorFormat,
      vk::SampleCountFlagBits::e1,
      loadOp,
      vk::AttachmentStoreOp::eStore,
      vk::AttachmentLoadOp::eDontCare,
      vk::AttachmentStoreOp::eDontCare,
      loadOp != vk::AttachmentLoadOp::eLoad ? vk::ImageLayout::eUndefined : vk::ImageLayout::ePresentSrcKHR,
      colorFinalLayout);
    if (depthFormat != vk::Format::eUndefined) {
        attachmentDescriptions.emplace_back(
          vk::AttachmentDescriptionFlags(),
          depthFormat,
          vk::SampleCountFlagBits::e1,
          loadOp,
          vk::AttachmentStoreOp::eStore,
          vk::AttachmentLoadOp::eDontCare,
          vk::AttachmentStoreOp::eDontCare,
          loadOp != vk::AttachmentLoadOp::eLoad ? vk::ImageLayout::eUndefined : vk::ImageLayout::eDepthStencilAttachmentOptimal,
          vk::ImageLayout::eDepthStencilAttachmentOptimal);
    }
    vk::AttachmentReference       colorAttachment(0, vk::ImageLayout::eColorAttachmentOptimal);
    const vk::AttachmentReference depthAttachment(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);
    vk::SubpassDescription        subpassDescription(
      vk::SubpassDescriptionFlags(),
      vk::PipelineBindPoint::eGraphics,
      {},
      colorAttachment,
      {},
      (depthFormat != vk::Format::eUndefined) ? &depthAttachment : nullptr);

    vk::SubpassDependency dependency {
      VK_SUBPASS_EXTERNAL,
      0,
      vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
      vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
      {},
      vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite};

    const vk::RenderPassCreateInfo renderPassCreateInfo(vk::RenderPassCreateFlags(), attachmentDescriptions, subpassDescription, dependency);
    return vk::raii::RenderPass(device, renderPassCreateInfo);
}

vk::Format dnm::pickDepthFormat(const vk::raii::PhysicalDevice& physicalDevice) {
    const std::vector<vk::Format> candidates = {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint};
    for (const vk::Format format : candidates) {
        vk::FormatProperties props = physicalDevice.getFormatProperties(format);

        if (props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
            return format;
        }
    }
    throw std::runtime_error("failed to find supported format!");
}

vk::PresentModeKHR dnm::pickPresentMode(const std::vector<vk::PresentModeKHR>& presentModes) {
    vk::PresentModeKHR pickedMode = vk::PresentModeKHR::eFifo;
    for (const auto& presentMode : presentModes) {
        if (presentMode == vk::PresentModeKHR::eMailbox) {
            pickedMode = presentMode;
            break;
        }

        if (presentMode == vk::PresentModeKHR::eImmediate) {
            pickedMode = presentMode;
        }
    }
    return pickedMode;
}

vk::SurfaceFormatKHR dnm::pickSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats) {
    assert(!formats.empty());
    vk::SurfaceFormatKHR pickedFormat = formats [0];
    if (formats.size() == 1) {
        if (formats [0].format == vk::Format::eUndefined) {
            pickedFormat.format     = vk::Format::eB8G8R8A8Unorm;
            pickedFormat.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
        }
    }
    else {
        // request several formats, the first found will be used
        const vk::Format  requestedFormats [] = {vk::Format::eB8G8R8A8Unorm, vk::Format::eR8G8B8A8Unorm, vk::Format::eB8G8R8Unorm, vk::Format::eR8G8B8Unorm};
        vk::ColorSpaceKHR requestedColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
        for (size_t i = 0; i < sizeof(requestedFormats) / sizeof(requestedFormats [0]); i++) {
            vk::Format requestedFormat = requestedFormats [i];
            auto       it              = std::find_if(
              formats.begin(),
              formats.end(),
              [requestedFormat, requestedColorSpace](const vk::SurfaceFormatKHR& f)
              { return (f.format == requestedFormat) && (f.colorSpace == requestedColorSpace); });
            if (it != formats.end()) {
                pickedFormat = *it;
                break;
            }
        }
    }
    assert(pickedFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear);
    return pickedFormat;
}

void dnm::setImageLayout(
  const vk::raii::CommandBuffer& commandBuffer,
  vk::Image                      image,
  vk::Format                     format,
  vk::ImageLayout                oldImageLayout,
  vk::ImageLayout                newImageLayout,
  u32                            baseMip) {
    vk::AccessFlags sourceAccessMask;
    switch (oldImageLayout) {
        case vk::ImageLayout::eTransferDstOptimal:
            sourceAccessMask = vk::AccessFlagBits::eTransferWrite;
            break;
        case vk::ImageLayout::ePreinitialized:
            sourceAccessMask = vk::AccessFlagBits::eHostWrite;
            break;
        case vk::ImageLayout::eGeneral:   // sourceAccessMask is empty
        case vk::ImageLayout::eUndefined:
            break;
        default:
            assert(false);
            break;
    }

    vk::PipelineStageFlags sourceStage;
    switch (oldImageLayout) {
        case vk::ImageLayout::eGeneral:
        case vk::ImageLayout::ePreinitialized:
            sourceStage = vk::PipelineStageFlagBits::eHost;
            break;
        case vk::ImageLayout::eTransferDstOptimal:
            sourceStage = vk::PipelineStageFlagBits::eTransfer;
            break;
        case vk::ImageLayout::eUndefined:
            sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
            break;
        default:
            assert(false);
            break;
    }

    vk::AccessFlags destinationAccessMask;
    switch (newImageLayout) {
        case vk::ImageLayout::eColorAttachmentOptimal:
            destinationAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
            break;
        case vk::ImageLayout::eDepthStencilAttachmentOptimal:
            destinationAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
            break;
        case vk::ImageLayout::eGeneral:   // empty destinationAccessMask
        case vk::ImageLayout::ePresentSrcKHR:
            break;
        case vk::ImageLayout::eShaderReadOnlyOptimal:
            destinationAccessMask = vk::AccessFlagBits::eShaderRead;
            break;
        case vk::ImageLayout::eTransferSrcOptimal:
            destinationAccessMask = vk::AccessFlagBits::eTransferRead;
            break;
        case vk::ImageLayout::eTransferDstOptimal:
            destinationAccessMask = vk::AccessFlagBits::eTransferWrite;
            break;
        default:
            assert(false);
            break;
    }

    vk::PipelineStageFlags destinationStage;
    switch (newImageLayout) {
        case vk::ImageLayout::eColorAttachmentOptimal:
            destinationStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            break;
        case vk::ImageLayout::eDepthStencilAttachmentOptimal:
            destinationStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
            break;
        case vk::ImageLayout::eGeneral:
            destinationStage = vk::PipelineStageFlagBits::eHost;
            break;
        case vk::ImageLayout::ePresentSrcKHR:
            destinationStage = vk::PipelineStageFlagBits::eBottomOfPipe;
            break;
        case vk::ImageLayout::eShaderReadOnlyOptimal:
            destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
            break;
        case vk::ImageLayout::eTransferDstOptimal:
        case vk::ImageLayout::eTransferSrcOptimal:
            destinationStage = vk::PipelineStageFlagBits::eTransfer;
            break;
        default:
            assert(false);
            break;
    }

    vk::ImageAspectFlags aspectMask;
    if (newImageLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
        aspectMask = vk::ImageAspectFlagBits::eDepth;
        if (format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint) {
            aspectMask |= vk::ImageAspectFlagBits::eStencil;
        }
    }
    else {
        aspectMask = vk::ImageAspectFlagBits::eColor;
    }

    const vk::ImageSubresourceRange imageSubresourceRange(aspectMask, baseMip, 1, 0, 1);
    const vk::ImageMemoryBarrier    imageMemoryBarrier(
      sourceAccessMask, destinationAccessMask, oldImageLayout, newImageLayout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, imageSubresourceRange);
    return commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, nullptr, nullptr, imageMemoryBarrier);
}

void dnm::submitAndWait(const vk::raii::Device& device, const vk::raii::Queue& queue, const vk::raii::CommandBuffer& commandBuffer) {
    const vk::raii::Fence fence(device, vk::FenceCreateInfo());
    queue.submit(vk::SubmitInfo(nullptr, nullptr, *commandBuffer), *fence);
    while (vk::Result::eTimeout == device.waitForFences({*fence}, VK_TRUE, FenceTimeout))
        ;
}

void dnm::updateDescriptorSets(
  const vk::raii::Device&                                                                                         device,
  const vk::raii::DescriptorSet&                                                                                  descriptorSet,
  std::span<std::tuple<vk::DescriptorType, const vk::raii::Buffer&, vk::DeviceSize, const vk::raii::BufferView*>> bufferData,
  const TextureData&                                                                                              textureData,
  u32                                                                                                             bindingOffset) {
    std::vector<vk::DescriptorBufferInfo> bufferInfos;
    bufferInfos.reserve(bufferData.size());

    std::vector<vk::WriteDescriptorSet> writeDescriptorSets;
    writeDescriptorSets.reserve(bufferData.size() + 1);
    u32 dstBinding = bindingOffset;
    for (const auto& bd : bufferData) {
        bufferInfos.emplace_back(*std::get<1>(bd), 0, std::get<2>(bd));
        vk::BufferView bufferView;
        if (std::get<3>(bd)) {
            bufferView = **std::get<3>(bd);
        }
        writeDescriptorSets.emplace_back(
          *descriptorSet, dstBinding++, 0, 1, std::get<0>(bd), nullptr, &bufferInfos.back(), std::get<3>(bd) ? &bufferView : nullptr);
    }

    vk::DescriptorImageInfo imageInfo(*textureData.sampler, *textureData.imageData.imageView, vk::ImageLayout::eShaderReadOnlyOptimal);
    writeDescriptorSets.emplace_back(*descriptorSet, dstBinding, 0, vk::DescriptorType::eCombinedImageSampler, imageInfo, nullptr, nullptr);

    device.updateDescriptorSets(writeDescriptorSets, nullptr);
}

void dnm::updateDescriptorSets(
  const vk::raii::Device&                                                                                         device,
  const vk::raii::DescriptorSet&                                                                                  descriptorSet,
  std::span<std::tuple<vk::DescriptorType, const vk::raii::Buffer&, vk::DeviceSize, const vk::raii::BufferView*>> bufferData,
  std::span<TextureData>                                                                                          textureData,
  u32                                                                                                             bindingOffset) {
    std::vector<vk::DescriptorBufferInfo> bufferInfos;
    bufferInfos.reserve(bufferData.size());

    std::vector<vk::WriteDescriptorSet> writeDescriptorSets;
    writeDescriptorSets.reserve(bufferData.size() + (textureData.empty() ? 0 : 1));
    u32 dstBinding = bindingOffset;
    for (const auto& bd : bufferData) {
        bufferInfos.emplace_back(*std::get<1>(bd), 0, std::get<2>(bd));
        vk::BufferView bufferView;
        if (std::get<3>(bd)) {
            bufferView = **std::get<3>(bd);
        }
        writeDescriptorSets.emplace_back(
          *descriptorSet, dstBinding++, 0, 1, std::get<0>(bd), nullptr, &bufferInfos.back(), std::get<3>(bd) ? &bufferView : nullptr);
    }

    std::vector<vk::DescriptorImageInfo> imageInfos;
    if (!textureData.empty()) {
        imageInfos.reserve(textureData.size());
        for (const auto& thd : textureData) {
            imageInfos.emplace_back(*thd.sampler, *thd.imageData.imageView, vk::ImageLayout::eShaderReadOnlyOptimal);
        }
        writeDescriptorSets.emplace_back(
          *descriptorSet,
          dstBinding,
          0,
          dnm::checked_cast<u32>(imageInfos.size()),
          vk::DescriptorType::eCombinedImageSampler,
          imageInfos.data(),
          nullptr,
          nullptr);
    }

    device.updateDescriptorSets(writeDescriptorSets, nullptr);
}

void updateDescriptorSets(
  const vk::raii::Device&                                                                                         device,
  const vk::raii::DescriptorSet&                                                                                  descriptorSet,
  std::span<std::tuple<vk::DescriptorType, const vk::raii::Buffer&, vk::DeviceSize, const vk::raii::BufferView*>> bufferData) {
    std::vector<vk::DescriptorBufferInfo> bufferInfos;
    bufferInfos.reserve(bufferData.size());

    std::vector<vk::WriteDescriptorSet> writeDescriptorSets;
    writeDescriptorSets.reserve(bufferData.size() + 1);
    u32 dstBinding = 0;
    for (const auto& bd : bufferData) {
        bufferInfos.emplace_back(*std::get<1>(bd), 0, std::get<2>(bd));
        vk::BufferView bufferView;
        if (std::get<3>(bd)) {
            bufferView = **std::get<3>(bd);
        }
        writeDescriptorSets.emplace_back(
          *descriptorSet, dstBinding++, 0, 1, std::get<0>(bd), nullptr, &bufferInfos.back(), std::get<3>(bd) ? &bufferView : nullptr);
    }
    device.updateDescriptorSets(writeDescriptorSets, nullptr);
}

void updateDescriptorSets(
  const vk::raii::Device&         device,
  const vk::raii::DescriptorSet&  descriptorSet,
  std::span<DescriptorSlotUpdate> slotUpdates,
  std::span<BindingSlot>          slots,
  std::span<TextureSlotUpdate>    textureData) {
    std::vector<vk::DescriptorBufferInfo> bufferInfos;
    bufferInfos.reserve(slotUpdates.size());

    std::vector<vk::WriteDescriptorSet> writeDescriptorSets;
    writeDescriptorSets.reserve(slotUpdates.size() + 1);

    for (auto& slotUpdate : slotUpdates) {
        bool foundSlot = false;
        for (auto& slot : slots) {
            if (slot.name == slotUpdate.name) {
                foundSlot = true;
                bufferInfos.emplace_back(*slotUpdate.buffer, 0, slotUpdate.size);
                vk::BufferView bufferView;
                if (slotUpdate.bufferView) {
                    bufferView = **slotUpdate.bufferView;
                }

                writeDescriptorSets.emplace_back(
                  *descriptorSet, slot.bindingSlot, 0, 1, slot.type, nullptr, &bufferInfos.back(), slotUpdate.bufferView ? &bufferView : nullptr);
                break;
            }
        }
        assert(foundSlot);
    }
    std::vector<vk::DescriptorImageInfo> imageInfos;
    for (auto& texture : textureData) {

        bool foundSlot = false;
        for (auto& slot : slots) {
            if (slot.name == texture.name) {
                foundSlot = true;
                imageInfos.reserve(textureData.size());
                imageInfos.emplace_back(*texture.textureData.sampler, *texture.textureData.imageData.imageView, vk::ImageLayout::eShaderReadOnlyOptimal);
                writeDescriptorSets.emplace_back(
                  *descriptorSet, slot.bindingSlot, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imageInfos.back(), nullptr, nullptr);
                break;
            }
        }
        assert(foundSlot);
    }

    device.updateDescriptorSets(writeDescriptorSets, nullptr);
}

VKAPI_ATTR VkBool32 VKAPI_CALL dnm::debugUtilsMessengerCallback(
  VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
  VkDebugUtilsMessageTypeFlagsEXT             messageTypes,
  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
  void* /*pUserData*/) {
#if !defined(NDEBUG)
    if (pCallbackData->messageIdNumber == 648835635) {
        // UNASSIGNED-khronos-Validation-debug-build-warning-message
        return VK_FALSE;
    }
    if (pCallbackData->messageIdNumber == 767975156) {
        // UNASSIGNED-BestPractices-vkCreateInstance-specialuse-extension
        return VK_FALSE;
    }
#endif

    std::cerr << to_string(static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(messageSeverity)) << ": "
              << to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(messageTypes)) << ":\n";
    std::cerr << std::string("\t") << "messageIDName   = <" << pCallbackData->pMessageIdName << ">\n";
    std::cerr << std::string("\t") << "messageIdNumber = " << pCallbackData->messageIdNumber << "\n";
    std::cerr << std::string("\t") << "message         = <" << pCallbackData->pMessage << ">\n";
    if (0 < pCallbackData->queueLabelCount) {
        std::cerr << std::string("\t") << "Queue Labels:\n";
        for (u32 i = 0; i < pCallbackData->queueLabelCount; i++) {
            std::cerr << std::string("\t\t") << "labelName = <" << pCallbackData->pQueueLabels [i].pLabelName << ">\n";
        }
    }
    if (0 < pCallbackData->cmdBufLabelCount) {
        std::cerr << std::string("\t") << "CommandBuffer Labels:\n";
        for (u32 i = 0; i < pCallbackData->cmdBufLabelCount; i++) {
            std::cerr << std::string("\t\t") << "labelName = <" << pCallbackData->pCmdBufLabels [i].pLabelName << ">\n";
        }
    }
    if (0 < pCallbackData->objectCount) {
        std::cerr << std::string("\t") << "Objects:\n";
        for (u32 i = 0; i < pCallbackData->objectCount; i++) {
            std::cerr << std::string("\t\t") << "Object " << i << "\n";
            std::cerr << std::string("\t\t\t") << "objectType   = " << to_string(static_cast<vk::ObjectType>(pCallbackData->pObjects [i].objectType)) << "\n";
            std::cerr << std::string("\t\t\t") << "objectHandle = " << pCallbackData->pObjects [i].objectHandle << "\n";
            if (pCallbackData->pObjects [i].pObjectName) {
                std::cerr << std::string("\t\t\t") << "objectName   = <" << pCallbackData->pObjects [i].pObjectName << ">\n";
            }
        }
    }
    return VK_TRUE;
}

vk::DebugUtilsMessengerCreateInfoEXT dnm::makeDebugUtilsMessengerCreateInfoEXT() {
    return {
      {},
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
      vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
      &debugUtilsMessengerCallback};
}

void registerDebugMarker(const vk::raii::Device& device, const vk::raii::Image& image, std::string_view name) {
#if !defined(NDEBUG)
    device.setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT {vk::ObjectType::eImage, (u64)((VkImage)(*image)), name.data()});
#endif
}

void registerDebugMarker(const vk::raii::Device& device, const vk::raii::Buffer& buffer, std::string_view name) {
#if !defined(NDEBUG)
    device.setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT {vk::ObjectType::eBuffer, (u64)((VkBuffer)(*buffer)), name.data()});
#endif
}

void registerDebugMarker(const vk::raii::Device& device, const vk::raii::Semaphore& semaphore, std::string_view name) {
#if !defined(NDEBUG)
    device.setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT {vk::ObjectType::eSemaphore, (u64)((VkSemaphore)(*semaphore)), name.data()});
#endif
}

void registerDebugMarker(const vk::raii::Device& device, const vk::raii::Pipeline& pipeline, std::string_view name) {
#if !defined(NDEBUG)
    device.setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT {vk::ObjectType::ePipeline, (u64)((VkPipeline)(*pipeline)), name.data()});
#endif
}

void registerDebugMarker(const vk::raii::Device& device, const vk::raii::CommandBuffer& commandBuffer, std::string_view name) {
#if !defined(NDEBUG)
    device.setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT {vk::ObjectType::eCommandBuffer, (u64)((VkCommandBuffer)(*commandBuffer)), name.data()});
#endif
}

void registerDebugMarker(const vk::raii::Device& device, std::string_view name) {
#if !defined(NDEBUG)
    device.setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT {vk::ObjectType::eDevice, (u64)((VkDevice)(*device)), name.data()});
#endif
}

BufferRegistration::~BufferRegistration() {
    if (renderer) {
        renderer->removeBufferRegistration(identifier);
    }
}
}   // namespace dnm

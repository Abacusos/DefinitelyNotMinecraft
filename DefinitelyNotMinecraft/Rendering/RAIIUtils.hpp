#pragma once

// Copyright(c) 2019, NVIDIA CORPORATION. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_NONE
#include <iostream>
#include <numeric>
#include <span>

#include <Core/Config.hpp>
#include <Core/Profiler.hpp>
#include <Core/ShortTypes.hpp>

#include <Rendering/GlobalBuffers.hpp>

#include <Shader/ShaderManager.hpp>

#include <GLFW/glfw3.h>

namespace dnm
{
const uint64_t FenceTimeout = 100000000;

template<typename TargetType, typename SourceType>
VULKAN_HPP_INLINE TargetType checked_cast(SourceType value) {
    static_assert(sizeof(TargetType) <= sizeof(SourceType), "No need to cast from smaller to larger type!");
    static_assert(std::numeric_limits<SourceType>::is_integer, "Only integer types supported!");
    static_assert(!std::numeric_limits<SourceType>::is_signed, "Only unsigned types supported!");
    static_assert(std::numeric_limits<TargetType>::is_integer, "Only integer types supported!");
    static_assert(!std::numeric_limits<TargetType>::is_signed, "Only unsigned types supported!");
    assert(value <= std::numeric_limits<TargetType>::max());
    return static_cast<TargetType>(value);
}

u32 findMemoryType(const vk::PhysicalDeviceMemoryProperties& memoryProperties, u32 typeBits, vk::MemoryPropertyFlags requirementsMask);

vk::raii::DeviceMemory allocateDeviceMemory(
  const vk::raii::Device&                   device,
  const vk::PhysicalDeviceMemoryProperties& memoryProperties,
  const vk::MemoryRequirements&             memoryRequirements,
  vk::MemoryPropertyFlags                   memoryPropertyFlags);

template<typename T>
void copyToDevice(const vk::raii::DeviceMemory& deviceMemory, std::span<const T> data, vk::DeviceSize stride = sizeof(T)) {
    ZoneScoped;
    auto count = data.size();
    assert(count > 0);
    assert(sizeof(T) <= stride);
    uint8_t* deviceData = static_cast<uint8_t*>(deviceMemory.mapMemory(0, count * stride));
    if (stride == sizeof(T)) {
        memcpy(deviceData, data.data(), count * sizeof(T));
    }
    else {
        for (size_t i = 0; i < count; i++) {
            memcpy(deviceData, data.data() + i, sizeof(T));
            deviceData += stride;
        }
    }
    deviceMemory.unmapMemory();
}

template<typename Func>
void oneTimeSubmit(const vk::raii::CommandBuffer& commandBuffer, const vk::raii::Queue& queue, const Func& func) {
    commandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    func(commandBuffer);
    commandBuffer.end();
    vk::SubmitInfo submitInfo(nullptr, nullptr, *commandBuffer);
    queue.submit(submitInfo, nullptr);
    queue.waitIdle();
}

template<typename Func>
void oneTimeSubmit(const vk::raii::Device& device, const vk::raii::CommandPool& commandPool, const vk::raii::Queue& queue, const Func& func) {
    vk::raii::CommandBuffers commandBuffers(device, {*commandPool, vk::CommandBufferLevel::ePrimary, 1});
    oneTimeSubmit(commandBuffers.front(), queue, func);
}

void setImageLayout(
  const vk::raii::CommandBuffer& commandBuffer,
  vk::Image                      image,
  vk::Format                     format,
  vk::ImageLayout                oldImageLayout,
  vk::ImageLayout                newImageLayout,
  u32                            baseMip = 0);

struct BufferData
{
    BufferData(
      const vk::raii::PhysicalDevice& physicalDevice,
      const vk::raii::Device&         device,
      vk::DeviceSize                  size,
      vk::BufferUsageFlags            usage,
      vk::MemoryPropertyFlags         propertyFlags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent) :
        buffer(device, vk::BufferCreateInfo({}, size, usage)),
        deviceMemory(allocateDeviceMemory(device, physicalDevice.getMemoryProperties(), buffer.getMemoryRequirements(), propertyFlags))
#if !defined(NDEBUG)
        ,
        m_size(size),
        m_usage(usage),
        m_propertyFlags(propertyFlags)
#endif
    {
        buffer.bindMemory(*deviceMemory, 0);
    }

    BufferData(std::nullptr_t) {}

    template<typename DataType>
    void upload(const DataType& data) const {
        assert((m_propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) && (m_propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible));
        assert(sizeof(DataType) <= m_size);

        void* dataPtr = deviceMemory.mapMemory(0, sizeof(DataType));
        memcpy(dataPtr, &data, sizeof(DataType));
        deviceMemory.unmapMemory();
    }

    template<typename DataType>
    void upload(const std::vector<DataType>& data, size_t stride = 0) const {
        assert(m_propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible);

        size_t elementSize = stride ? stride : sizeof(DataType);
        assert(sizeof(DataType) <= elementSize);

        copyToDevice(deviceMemory, data.data(), data.size(), elementSize);
    }

    template<typename DataType>
    void upload(
      const vk::raii::PhysicalDevice& physicalDevice,
      const vk::raii::Device&         device,
      const vk::raii::CommandPool&    commandPool,
      const vk::raii::Queue&          queue,
      const std::vector<DataType>&    data,
      size_t                          stride) const {
        assert(m_usage & vk::BufferUsageFlagBits::eTransferDst);
        assert(m_propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal);

        size_t elementSize = stride ? stride : sizeof(DataType);
        assert(sizeof(DataType) <= elementSize);

        size_t dataSize = data.size() * elementSize;
        assert(dataSize <= m_size);

        BufferData stagingBuffer(physicalDevice, device, dataSize, vk::BufferUsageFlagBits::eTransferSrc);
        copyToDevice(stagingBuffer.deviceMemory, data.data(), data.size(), elementSize);

        dnm::oneTimeSubmit(
          device,
          commandPool,
          queue,
          [&](const vk::raii::CommandBuffer& commandBuffer)
          { commandBuffer.copyBuffer(*stagingBuffer.buffer, *this->buffer, vk::BufferCopy(0, 0, dataSize)); });
    }

    // the order of buffer and deviceMemory here is important to get the
    // constructor running !
    vk::raii::Buffer       buffer       = nullptr;
    vk::raii::DeviceMemory deviceMemory = nullptr;
#if !defined(NDEBUG)

    private:
    vk::DeviceSize          m_size;
    vk::BufferUsageFlags    m_usage;
    vk::MemoryPropertyFlags m_propertyFlags;
#endif
};

struct ImageData
{
    ImageData(
      const vk::raii::PhysicalDevice& physicalDevice,
      const vk::raii::Device&         device,
      vk::Format                      format_,
      const vk::Extent2D&             extent,
      vk::ImageTiling                 tiling,
      vk::ImageUsageFlags             usage,
      vk::ImageLayout                 initialLayout,
      vk::MemoryPropertyFlags         memoryProperties,
      vk::ImageAspectFlags            aspectMask,
      u32                             mipCount = 1) :
        format(format_),
        image(
          device,
          {vk::ImageCreateFlags(),
           vk::ImageType::e2D,
           format,
           vk::Extent3D(extent, 1),
           mipCount,
           1,
           vk::SampleCountFlagBits::e1,
           tiling,
           usage | vk::ImageUsageFlagBits::eSampled,
           vk::SharingMode::eExclusive,
           {},
           initialLayout}),
        deviceMemory(allocateDeviceMemory(device, physicalDevice.getMemoryProperties(), image.getMemoryRequirements(), memoryProperties)) {
        image.bindMemory(*deviceMemory, 0);
        imageView = vk::raii::ImageView(device, vk::ImageViewCreateInfo({}, *image, vk::ImageViewType::e2D, format, {}, {aspectMask, 0, mipCount, 0, 1}));
    }

    ImageData(std::nullptr_t) {}

    vk::Format             format;
    vk::raii::Image        image        = nullptr;
    vk::raii::DeviceMemory deviceMemory = nullptr;
    vk::raii::ImageView    imageView    = nullptr;
};

struct DepthBufferData : public ImageData
{
    DepthBufferData(std::nullptr_t) : ImageData(nullptr) {}

    DepthBufferData(const vk::raii::PhysicalDevice& physicalDevice, const vk::raii::Device& device, vk::Format format, const vk::Extent2D& extent) :
        ImageData(
          physicalDevice,
          device,
          format,
          extent,
          vk::ImageTiling::eOptimal,
          vk::ImageUsageFlagBits::eDepthStencilAttachment,
          vk::ImageLayout::eUndefined,
          vk::MemoryPropertyFlagBits::eDeviceLocal,
          vk::ImageAspectFlagBits::eDepth) {}
};

struct WindowData
{
    WindowData(std::nullptr_t) : handle {nullptr} {};

    WindowData(GLFWwindow* wnd, const std::string& name, const vk::Extent2D& extent) : handle {wnd}, name {name}, extent {extent} {};
    WindowData(const WindowData&) = delete;

    WindowData(WindowData&& other) noexcept : handle {}, name {}, extent {} {
        std::swap(handle, other.handle);
        std::swap(name, other.name);
        std::swap(extent, other.extent);
    }

    WindowData& operator=(WindowData&& other) noexcept {
        handle = {};
        name   = {};
        extent = vk::Extent2D();
        std::swap(handle, other.handle);
        std::swap(name, other.name);
        std::swap(extent, other.extent);
        return *this;
    }

    ~WindowData() noexcept { glfwDestroyWindow(handle); }

    GLFWwindow*  handle;
    std::string  name;
    vk::Extent2D extent;
};

WindowData createWindow(const std::string& windowName, const vk::Extent2D& extent);

struct SurfaceData
{
    SurfaceData(std::nullptr_t) {};

    SurfaceData(const vk::raii::Instance& instance, const std::string& windowName, const vk::Extent2D& extent_) :
        extent(extent_), window(createWindow(windowName, extent)) {
        VkSurfaceKHR _surface;
        VkResult     err = glfwCreateWindowSurface(static_cast<VkInstance>(*instance), window.handle, nullptr, &_surface);
        if (err != VK_SUCCESS) {
            throw std::runtime_error("Failed to create window!");
        }
        surface = vk::raii::SurfaceKHR(instance, _surface);
    }

    vk::Extent2D         extent;
    WindowData           window  = nullptr;
    vk::raii::SurfaceKHR surface = nullptr;
};

vk::SurfaceFormatKHR pickSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats);

vk::PresentModeKHR pickPresentMode(const std::vector<vk::PresentModeKHR>& presentModes);

struct SwapChainData
{
    SwapChainData(std::nullptr_t) {};

    SwapChainData(
      const vk::raii::PhysicalDevice& physicalDevice,
      const vk::raii::Device&         device,
      const vk::raii::SurfaceKHR&     surface,
      const vk::Extent2D&             extent,
      vk::ImageUsageFlags             usage,
      const vk::raii::SwapchainKHR*   pOldSwapchain,
      u32                             graphicsQueueFamilyIndex,
      u32                             presentQueueFamilyIndex) {
        vk::SurfaceFormatKHR surfaceFormat = pickSurfaceFormat(physicalDevice.getSurfaceFormatsKHR(*surface));
        colorFormat                        = surfaceFormat.format;

        vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
        vk::Extent2D               swapchainExtent;
        if (surfaceCapabilities.currentExtent.width == std::numeric_limits<u32>::max()) {
            // If the surface size is undefined, the size is set to the size of the
            // images requested.
            swapchainExtent.width  = std::clamp(extent.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
            swapchainExtent.height = std::clamp(extent.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
        }
        else {
            // If the surface size is defined, the swap chain size must match
            swapchainExtent = surfaceCapabilities.currentExtent;
        }
        vk::SurfaceTransformFlagBitsKHR preTransform = (surfaceCapabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity)
                                                       ? vk::SurfaceTransformFlagBitsKHR::eIdentity
                                                       : surfaceCapabilities.currentTransform;
        vk::CompositeAlphaFlagBitsKHR   compositeAlpha =
          (surfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePreMultiplied)    ? vk::CompositeAlphaFlagBitsKHR::ePreMultiplied
            : (surfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePostMultiplied) ? vk::CompositeAlphaFlagBitsKHR::ePostMultiplied
            : (surfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eInherit)        ? vk::CompositeAlphaFlagBitsKHR::eInherit
                                                                                                             : vk::CompositeAlphaFlagBitsKHR::eOpaque;
        vk::PresentModeKHR         presentMode = pickPresentMode(physicalDevice.getSurfacePresentModesKHR(*surface));
        vk::SwapchainCreateInfoKHR swapChainCreateInfo(
          {},
          *surface,
          surfaceCapabilities.minImageCount + 1,
          colorFormat,
          surfaceFormat.colorSpace,
          swapchainExtent,
          1,
          usage,
          vk::SharingMode::eExclusive,
          {},
          preTransform,
          compositeAlpha,
          presentMode,
          true,
          pOldSwapchain ? **pOldSwapchain : nullptr);
        if (graphicsQueueFamilyIndex != presentQueueFamilyIndex) {
            u32 queueFamilyIndices [2]                = {graphicsQueueFamilyIndex, presentQueueFamilyIndex};
            // If the graphics and present queues are from different queue families,
            // we either have to explicitly transfer ownership of images between the
            // queues, or we have to create the swapchain with imageSharingMode as
            // vk::SharingMode::eConcurrent
            swapChainCreateInfo.imageSharingMode      = vk::SharingMode::eConcurrent;
            swapChainCreateInfo.queueFamilyIndexCount = 2;
            swapChainCreateInfo.pQueueFamilyIndices   = queueFamilyIndices;
        }
        swapChain = vk::raii::SwapchainKHR(device, swapChainCreateInfo);

        images = swapChain.getImages();

        imageViews.reserve(images.size());
        vk::ImageViewCreateInfo imageViewCreateInfo({}, {}, vk::ImageViewType::e2D, colorFormat, {}, {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
        for (auto image : images) {
            imageViewCreateInfo.image = image;
            imageViews.emplace_back(device, imageViewCreateInfo);
        }
    }

    vk::Format                       colorFormat;
    vk::raii::SwapchainKHR           swapChain = nullptr;
    std::vector<vk::Image>           images;
    std::vector<vk::raii::ImageView> imageViews;
};

struct TextureData
{
    TextureData(std::nullptr_t) : sampler {nullptr} {};

    TextureData(
      const vk::raii::PhysicalDevice& physicalDevice,
      const vk::raii::Device&         device,
      const vk::Extent2D&             extent_            = {256, 256},
      vk::Format                      format_            = vk::Format::eR8G8B8A8Unorm,
      vk::SamplerAddressMode          addressMode        = vk::SamplerAddressMode::eRepeat,
      u32                             mipCount           = 1,
      vk::ImageUsageFlags             usageFlags         = {},
      vk::FormatFeatureFlags          formatFeatureFlags = {},
      bool                            anisotropyEnable   = false,
      bool                            forceStaging       = false) :
        format(format_),
        extent(extent_),
        sampler(
          device,
          {{},
           vk::Filter::eNearest,
           vk::Filter::eNearest,
           vk::SamplerMipmapMode::eNearest,
           addressMode,
           addressMode,
           addressMode,
           0.0f,
           anisotropyEnable,
           16.0f,
           false,
           vk::CompareOp::eNever,
           0.0f,
           static_cast<float>(mipCount),
           vk::BorderColor::eFloatOpaqueBlack}) {
        vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(format);

        formatFeatureFlags |= vk::FormatFeatureFlagBits::eSampledImage;
        needsStaging = forceStaging || ((formatProperties.linearTilingFeatures & formatFeatureFlags) != formatFeatureFlags);
        vk::ImageTiling         imageTiling;
        vk::ImageLayout         initialLayout;
        vk::MemoryPropertyFlags requirements;
        if (needsStaging) {
            assert((formatProperties.optimalTilingFeatures & formatFeatureFlags) == formatFeatureFlags);
            stagingBufferData = BufferData(physicalDevice, device, extent.width * extent.height * 4, vk::BufferUsageFlagBits::eTransferSrc);
            imageTiling       = vk::ImageTiling::eOptimal;
            usageFlags |= vk::ImageUsageFlagBits::eTransferDst;
            initialLayout = vk::ImageLayout::eUndefined;
        }
        else {
            imageTiling   = vk::ImageTiling::eLinear;
            initialLayout = vk::ImageLayout::ePreinitialized;
            requirements  = vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible;
        }
        imageData = ImageData(
          physicalDevice,
          device,
          format,
          extent,
          imageTiling,
          usageFlags | vk::ImageUsageFlagBits::eSampled,
          initialLayout,
          requirements,
          vk::ImageAspectFlagBits::eColor,
          mipCount);
    }

    void setImage(const vk::raii::CommandBuffer& commandBuffer, void* textureData, bool setShaderReadOnlyOptimal = true, u32 mipLevels = 1) const {
        u64   neededSize = needsStaging ? stagingBufferData.buffer.getMemoryRequirements().size : imageData.image.getMemoryRequirements().size;
        void* data       = needsStaging ? stagingBufferData.deviceMemory.mapMemory(0, neededSize) : imageData.deviceMemory.mapMemory(0, neededSize);
        memcpy(data, textureData, neededSize);
        needsStaging ? stagingBufferData.deviceMemory.unmapMemory() : imageData.deviceMemory.unmapMemory();

        if (needsStaging) {
            // Since we're going to blit to the texture image, set its layout to
            // eTransferDstOptimal
            setImageLayout(commandBuffer, *imageData.image, imageData.format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

            for (u32 mipLevel = 1; mipLevel < mipLevels; ++mipLevel) {
                setImageLayout(commandBuffer, *imageData.image, imageData.format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, mipLevel);
            }

            vk::BufferImageCopy copyRegion(
              0,
              extent.width,
              extent.height,
              vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
              vk::Offset3D(0, 0, 0),
              vk::Extent3D(extent, 1));
            commandBuffer.copyBufferToImage(*stagingBufferData.buffer, *imageData.image, vk::ImageLayout::eTransferDstOptimal, copyRegion);
            // Set the layout for the texture image from eTransferDstOptimal to
            // eShaderReadOnlyOptimal
            if (setShaderReadOnlyOptimal) {
                setImageLayout(
                  commandBuffer, *imageData.image, imageData.format, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
            }
        }
        else {
            // If we can use the linear tiled image as a texture, just do it
            setImageLayout(commandBuffer, *imageData.image, imageData.format, vk::ImageLayout::ePreinitialized, vk::ImageLayout::eShaderReadOnlyOptimal);
        }
    }

    vk::Format        format;
    vk::Extent2D      extent;
    bool              needsStaging;
    BufferData        stagingBufferData = nullptr;
    ImageData         imageData         = nullptr;
    vk::raii::Sampler sampler;
};

u32 findGraphicsQueueFamilyIndex(const std::vector<vk::QueueFamilyProperties>& queueFamilyProperties);

std::pair<u32, u32> findGraphicsAndPresentQueueFamilyIndex(const vk::raii::PhysicalDevice& physicalDevice, const vk::raii::SurfaceKHR& surface);

vk::raii::CommandBuffer makeCommandBuffer(const vk::raii::Device& device, const vk::raii::CommandPool& commandPool);

vk::raii::DescriptorPool makeDescriptorPool(const vk::raii::Device& device, std::span<const vk::DescriptorPoolSize> poolSizes);

vk::raii::DescriptorSetLayout makeDescriptorSetLayout(
  const vk::raii::Device&                                              device,
  std::span<std::tuple<vk::DescriptorType, u32, vk::ShaderStageFlags>> bindingData,
  vk::DescriptorSetLayoutCreateFlags                                   flags = {});

vk::raii::DescriptorSetLayout makeDescriptorSetLayout(
  const vk::raii::Device&            device,
  std::span<BindingSlot>             bindingData,
  vk::ShaderStageFlags               stageFlags,
  vk::DescriptorSetLayoutCreateFlags flags = {});

vk::raii::Device makeDevice(
  const vk::raii::PhysicalDevice&   physicalDevice,
  u32                               queueFamilyIndex,
  const std::vector<std::string>&   extensions             = {},
  const vk::PhysicalDeviceFeatures* physicalDeviceFeatures = nullptr,
  const void*                       pNext                  = nullptr);

std::vector<vk::raii::Framebuffer> makeFramebuffers(
  const vk::raii::Device&                 device,
  vk::raii::RenderPass&                   renderPass,
  const std::vector<vk::raii::ImageView>& imageViews,
  const vk::raii::ImageView*              pDepthImageView,
  const vk::Extent2D&                     extent);

vk::raii::Pipeline makeGraphicsPipeline(
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
  vk::PrimitiveTopology                          topology = vk::PrimitiveTopology::eTriangleList);

vk::raii::Pipeline makeComputePipeline(
  const vk::raii::Device&         device,
  const vk::raii::PipelineCache&  pipelineCache,
  const vk::raii::ShaderModule&   computeShaderModule,
  const vk::SpecializationInfo*   vertexShaderSpecializationInfo,
  const vk::raii::PipelineLayout& pipelineLayout);

std::vector<const char*> gatherExtensions(
  const std::vector<std::string>& extensions
#if !defined(NDEBUG)
  ,
  const std::vector<vk::ExtensionProperties>& extensionProperties
#endif
);

std::vector<const char*> gatherLayers(
  const std::vector<std::string>& layers
#if !defined(NDEBUG)
  ,
  const std::vector<vk::LayerProperties>& layerProperties
#endif
);

vk::raii::Image makeImage(const vk::raii::Device& device);

vk::raii::Instance makeInstance(
  const vk::raii::Context&        context,
  const std::string&              appName,
  const std::string&              engineName,
  const std::vector<std::string>& layers     = {},
  const std::vector<std::string>& extensions = {},
  u32                             apiVersion = VK_API_VERSION_1_2);

vk::raii::RenderPass makeRenderPass(
  const vk::raii::Device& device,
  vk::Format              colorFormat,
  vk::Format              depthFormat,
  vk::AttachmentLoadOp    loadOp           = vk::AttachmentLoadOp::eClear,
  vk::ImageLayout         colorFinalLayout = vk::ImageLayout::ePresentSrcKHR);

vk::Format pickDepthFormat(const vk::raii::PhysicalDevice& physicalDevice);

void submitAndWait(const vk::raii::Device& device, const vk::raii::Queue& queue, const vk::raii::CommandBuffer& commandBuffer);

void updateDescriptorSets(
  const vk::raii::Device&                                                                                         device,
  const vk::raii::DescriptorSet&                                                                                  descriptorSet,
  std::span<std::tuple<vk::DescriptorType, const vk::raii::Buffer&, vk::DeviceSize, const vk::raii::BufferView*>> bufferData,
  const TextureData&                                                                                              textureData,
  u32                                                                                                             bindingOffset = 0);

void updateDescriptorSets(
  const vk::raii::Device&                                                                                         device,
  const vk::raii::DescriptorSet&                                                                                  descriptorSet,
  std::span<std::tuple<vk::DescriptorType, const vk::raii::Buffer&, vk::DeviceSize, const vk::raii::BufferView*>> bufferData,
  std::span<TextureData>                                                                                          textureData,
  u32                                                                                                             bindingOffset = 0);

void updateDescriptorSets(
  const vk::raii::Device&                                                                                         device,
  const vk::raii::DescriptorSet&                                                                                  descriptorSet,
  std::span<std::tuple<vk::DescriptorType, const vk::raii::Buffer&, vk::DeviceSize, const vk::raii::BufferView*>> bufferData);

struct DescriptorSlotUpdate
{
    std::string_view            name;
    const vk::raii::Buffer&     buffer;
    const vk::DeviceSize        size       = VK_WHOLE_SIZE;
    const vk::raii::BufferView* bufferView = nullptr;
};

struct TextureSlotUpdate
{
    std::string_view   name;
    const TextureData& textureData;
};

void updateDescriptorSets(
  const vk::raii::Device&         device,
  const vk::raii::DescriptorSet&  descriptorSet,
  std::span<DescriptorSlotUpdate> slotUpdates,
  std::span<BindingSlot>          slots,
  std::span<TextureSlotUpdate>          textures = {});

std::vector<std::string> getDeviceExtensions();

std::vector<std::string> getInstanceExtensions();

VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsMessengerCallback(
  VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
  VkDebugUtilsMessageTypeFlagsEXT             messageTypes,
  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
  void* /*pUserData*/);

vk::DebugUtilsMessengerCreateInfoEXT makeDebugUtilsMessengerCreateInfoEXT();

class Renderer;

struct BufferRegistration
{
    GlobalBuffers identifier = GlobalBuffers::Undefined;
    Renderer*     renderer   = nullptr;

    BufferRegistration(GlobalBuffers identifier, Renderer* renderer) : identifier {identifier}, renderer {renderer} {};
    BufferRegistration(const BufferRegistration&)            = delete;
    BufferRegistration& operator=(const BufferRegistration&) = delete;
    ~BufferRegistration();
};

void registerDebugMarker(const vk::raii::Device& device, const vk::raii::Image& image, std::string_view name);
void registerDebugMarker(const vk::raii::Device& device, const vk::raii::Buffer& buffer, std::string_view name);
void registerDebugMarker(const vk::raii::Device& device, const vk::raii::Semaphore& semaphore, std::string_view name);
void registerDebugMarker(const vk::raii::Device& device, const vk::raii::Pipeline& pipeline, std::string_view name);
void registerDebugMarker(const vk::raii::Device& device, const vk::raii::CommandBuffer& commandBuffer, std::string_view name);
void registerDebugMarker(const vk::raii::Device& device, std::string_view name);
}   // namespace dnm

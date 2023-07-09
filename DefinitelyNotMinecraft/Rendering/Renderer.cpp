#include "Renderer.hpp"

#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif

#include "RAIIUtils.hpp"
#include "ShortTypes.hpp"
#include "glslang/SPIRV/GlslangToSpv.h"
#include "optick.h"

namespace dnm {

Renderer::Renderer(Config* config) : m_config{config} {
  glslang::InitializeProcess();

  m_instance = makeInstance(m_context, "AppName", "EngineName", {},
                            getInstanceExtensions());
#if !defined(NDEBUG)
  vk::raii::DebugUtilsMessengerEXT debugUtilsMessenger(
      m_instance, makeDebugUtilsMessengerCreateInfoEXT());
#endif
  m_physicalDevice = vk::raii::PhysicalDevices(m_instance).front();

  std::vector<vk::ExtensionProperties> extensionProperties =
      m_physicalDevice.enumerateDeviceExtensionProperties();

  auto feature = m_physicalDevice.getFeatures();

  auto features2 =
      m_physicalDevice.getFeatures2<vk::PhysicalDeviceFeatures2,
                                    vk::PhysicalDevice8BitStorageFeaturesKHR>();

  m_surfaceData = SurfaceData(m_instance, "AppName", vk::Extent2D(1280, 720));

  std::pair<u32, u32> graphicsAndPresentQueueFamilyIndex =
      findGraphicsAndPresentQueueFamilyIndex(m_physicalDevice,
                                             m_surfaceData.surface);
  m_device =
      makeDevice(m_physicalDevice, graphicsAndPresentQueueFamilyIndex.first,
                 getDeviceExtensions(), &feature, &features2);
  registerDebugMarker(m_device, "DNM Device");

  m_commandPool = vk::raii::CommandPool(
      m_device, {vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                 graphicsAndPresentQueueFamilyIndex.first});

  m_graphicsQueue =
      vk::raii::Queue(m_device, graphicsAndPresentQueueFamilyIndex.first, 0);
  m_computeQueue =
      vk::raii::Queue(m_device, graphicsAndPresentQueueFamilyIndex.first, 0);
  m_presentQueue =
      vk::raii::Queue(m_device, graphicsAndPresentQueueFamilyIndex.second, 0);

  auto device = static_cast<VkDevice>(*m_device);
  auto physicalDevice = static_cast<VkPhysicalDevice>(*m_physicalDevice);
  auto queue = static_cast<VkQueue>(*m_graphicsQueue);
  OPTICK_GPU_INIT_VULKAN(&device, &physicalDevice, &queue,
                         &graphicsAndPresentQueueFamilyIndex.first, 1u,
                         nullptr);
  m_pipelineCache =
      vk::raii::PipelineCache(m_device, vk::PipelineCacheCreateInfo());

  m_projection =
      createBuffer(sizeof(glm::mat4x4), vk::BufferUsageFlagBits::eUniformBuffer,
                   "Projection Matrix");
  m_projectionClipRegistration =
      registerRAIIBuffer(GlobalBuffers::ProjectionClip, m_projection);

  recreateSwapChain();

  m_drawFence = vk::raii::Fence(m_device, {vk::FenceCreateFlagBits::eSignaled});
  m_imageAcquiredSemaphore =
      vk::raii::Semaphore(m_device, vk::SemaphoreCreateInfo());
}

Renderer::~Renderer() {
  m_device.waitIdle();
  glslang::FinalizeProcess();
}

u32 Renderer::prepareDrawFrame() {
  OPTICK_EVENT();
  {
    OPTICK_EVENT("Wait on fence");
    while (vk::Result::eTimeout ==
           m_device.waitForFences({*m_drawFence}, VK_TRUE, FenceTimeout))
      ;
  }

  // Get the index of the next available swapchain image:
  vk::Result result;
  u32 imageIndex;
  {
    OPTICK_EVENT("Acquire next image");
    std::tie(result, imageIndex) = m_swapChainData.swapChain.acquireNextImage(
        FenceTimeout, *m_imageAcquiredSemaphore);
  }

  if (result == vk::Result::eErrorOutOfDateKHR) {
    recreateSwapChainFromWindow();
    return std::numeric_limits<u32>::max();
  } else if (result != vk::Result::eSuccess &&
             result != vk::Result::eSuboptimalKHR) {
    throw std::runtime_error("failed to acquire swap chain image!");
  }
  assert(imageIndex < m_swapChainData.images.size());
  m_drawFence = vk::raii::Fence(m_device, vk::FenceCreateInfo());

  return imageIndex;
}

void Renderer::finishDrawFrame(vk::raii::Semaphore& finishedRendering,
                               u32 imageIndex) {
  OPTICK_EVENT();
  vk::PipelineStageFlags waitDestinationStageMask(
      vk::PipelineStageFlagBits::eColorAttachmentOutput);
  std::array<vk::SubmitInfo, 2u> submitInfo = {
      vk::SubmitInfo{*m_imageAcquiredSemaphore, waitDestinationStageMask},
      vk::SubmitInfo{*finishedRendering, waitDestinationStageMask}};
  m_graphicsQueue.submit(submitInfo, *m_drawFence);

  vk::PresentInfoKHR presentInfoKHR(nullptr, *m_swapChainData.swapChain,
                                    imageIndex);

  // Optick does not use the pointer, so just use void
  // OPTICK_GPU_FLIP(nullptr);
  vk::Result result = m_presentQueue.presentKHR(presentInfoKHR);
  switch (result) {
    case vk::Result::eSuccess:
      break;
    case vk::Result::eErrorOutOfDateKHR:
      [[fallthrough]];
    case vk::Result::eSuboptimalKHR:
      recreateSwapChainFromWindow();
      break;
    default:
      assert(false);  // an unexpected result is returned !
  }
}

GLFWwindow* Renderer::getGLFWwindow() const {
  return m_surfaceData.window.handle;
}

vk::Extent2D Renderer::getExtent() const { return m_surfaceData.extent; }

const vk::raii::Instance& Renderer::getInstance() const { return m_instance; }

const vk::raii::PhysicalDevice& Renderer::getPhysicalDevice() const {
  return m_physicalDevice;
}

const vk::raii::Device& Renderer::getDevice() const { return m_device; }

const vk::raii::PipelineCache& Renderer::getPipelineCache() const {
  return m_pipelineCache;
}

const vk::raii::RenderPass& Renderer::getRenderPass() const {
  return m_renderPass;
}

const vk::raii::Queue& Renderer::getComputeQueue() const {
  return m_computeQueue;
}

const vk::raii::Queue& Renderer::getGraphicsQueue() const {
  return m_graphicsQueue;
}

const vk::raii::Framebuffer& Renderer::getFrameBuffer(u32 imageIndex) const {
  return m_framebuffers[imageIndex];
}

vk::Format Renderer::getColorFormat() const { return m_colorFormat; }

Renderer::FamilyIndices Renderer::getIndices() const { return m_familyIndices; }

vk::raii::CommandBuffer Renderer::getCommandBuffer() {
  return makeCommandBuffer(m_device, m_commandPool);
}

BufferData Renderer::createBuffer(vk::DeviceSize size,
                                  vk::BufferUsageFlags flags,
                                  std::string_view debugName,
                                  vk::MemoryPropertyFlags propertyFlags) {
  auto result =
      BufferData(m_physicalDevice, m_device, size, flags, propertyFlags);
  registerDebugMarker(m_device, result.buffer, debugName);
  return result;
}

std::unique_ptr<dnm::BufferRegistration> Renderer::registerRAIIBuffer(

    GlobalBuffers bufferIdentifier, const BufferData& buffer) {
  registerBuffer(bufferIdentifier, buffer);
  return std::make_unique<dnm::BufferRegistration>(bufferIdentifier, this);
}

void Renderer::registerBuffer(GlobalBuffers bufferIdentifier,
                              const BufferData& buffer) {
  m_globalBuffers[static_cast<u32>(bufferIdentifier)] = &(buffer.buffer);
}

void Renderer::removeBufferRegistration(GlobalBuffers bufferIdentifier) {
  m_globalBuffers[static_cast<u32>(bufferIdentifier)] = nullptr;
}

const vk::raii::Buffer* Renderer::getGlobalBuffer(
    GlobalBuffers bufferIdentifier) {
  return m_globalBuffers[static_cast<u32>(bufferIdentifier)];
}

void Renderer::oneTimeSubmit(
    std::function<void(const vk::raii::CommandBuffer& commandBuffer)>
        function) {
  dnm::oneTimeSubmit(m_device, m_commandPool, m_graphicsQueue,
                     std::move(function));
}

void Renderer::waitIdle() { m_device.waitIdle(); }

void Renderer::recreateSwapChainFromWindow() {
  i32 width = 0, height = 0;
  glfwGetFramebufferSize(m_surfaceData.window.handle, &width, &height);
  while (width == 0 || height == 0) {
    glfwGetFramebufferSize(m_surfaceData.window.handle, &width, &height);
    glfwWaitEvents();
  }
  m_surfaceData.extent =
      vk::Extent2D{static_cast<u32>(width), static_cast<u32>(height)};
  recreateSwapChain();
}

void Renderer::recreateSwapChain() {
  m_device.waitIdle();

  std::pair<u32, u32> graphicsAndPresentQueueFamilyIndex =
      findGraphicsAndPresentQueueFamilyIndex(m_physicalDevice,
                                             m_surfaceData.surface);

  m_swapChainData = SwapChainData(m_physicalDevice, m_device,
                                  m_surfaceData.surface, m_surfaceData.extent,
                                  vk::ImageUsageFlagBits::eColorAttachment |
                                      vk::ImageUsageFlagBits::eTransferSrc,
                                  {}, graphicsAndPresentQueueFamilyIndex.first,
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

  // TODO The near and far are swapped here, not sure if this is correct
  std::array<glm::mat4x4, 1u> matrices{
      createProjectionMatrix(m_surfaceData.extent, m_config->farPlane, 0.01f)};
  copyToDevice(m_projection.deviceMemory,
               std::span<const glm::mat4x4>(matrices));
}

glm::mat4x4 Renderer::createProjectionMatrix(const vk::Extent2D& extent,
                                             float n, float f) {
  constexpr float fov = glm::radians(60.0f);
  float aspect = extent.width / (float)extent.height;

  constexpr float fov_rad = 45.0f * 2.0f * glm::pi<float>() / 360.0f;
  float focal_length = 1.0f / std::tan(fov_rad / 2.0f);

  float x = focal_length / aspect;
  float y = -focal_length;
  const auto a = n / (f - n);
  const auto b = f * a;

  glm::mat4x4 projection{
      x,    0.0f, 0.0f, 0.0f,  0.0f, y,    0.0f, 0.0f,
      0.0f, 0.0f, a,    -1.0f, 0.0f, 0.0f, b,    0.0f,
  };

  return projection;
}
}  // namespace dnm
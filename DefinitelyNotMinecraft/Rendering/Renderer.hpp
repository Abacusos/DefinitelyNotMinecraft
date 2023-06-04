#pragma once

#include <functional>
#include <vulkan/vulkan_raii.hpp>

#include "Chrono.hpp"
#include "GlobalBuffers.hpp"
#include "RAIIUtils.hpp"
#include "GLMInclude.hpp"
#include "Config.hpp"

namespace dnm {

class ShaderWatcher;

class Renderer {
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

  vk::Format getColorFormat() const;

  struct FamilyIndices {
    u32 graphicsQueueFamilyIndex;
    u32 presentQueueFamilyIndex;
  };
  FamilyIndices getIndices() const;

  vk::raii::CommandBuffer getCommandBuffer();
  dnm::BufferData createBuffer(vk::DeviceSize size, vk::BufferUsageFlags flags,
                               std::string_view debugName,
                               vk::MemoryPropertyFlags propertyFlags =
                                   vk::MemoryPropertyFlagBits::eHostVisible |
                                   vk::MemoryPropertyFlagBits::eHostCoherent);

  std::unique_ptr<dnm::BufferRegistration> registerRAIIBuffer(
      GlobalBuffers bufferIdentifier, const BufferData& buffer);
  void registerBuffer(GlobalBuffers bufferIdentifier, const BufferData& buffer);
  void removeBufferRegistration(GlobalBuffers bufferIdentifier);
  const vk::raii::Buffer* getGlobalBuffer(GlobalBuffers bufferIdentifier);

  void oneTimeSubmit(
      std::function<void(const vk::raii::CommandBuffer& commandBuffer)>
          function);

  void waitIdle();

 private:
  void recreateSwapChainFromWindow();
  void recreateSwapChain();

  glm::mat4x4 createProjectionMatrix(const vk::Extent2D& extent, float n,
                                     float f);

 private:
  Config* m_config;

  vk::raii::Context m_context;
  vk::raii::Instance m_instance{nullptr};
  vk::raii::PhysicalDevice m_physicalDevice{nullptr};
  FamilyIndices m_familyIndices;
  dnm::SurfaceData m_surfaceData{nullptr};
  vk::raii::Device m_device{nullptr};
  vk::raii::CommandPool m_commandPool{nullptr};
  vk::raii::Queue m_computeQueue{nullptr};
  vk::raii::Queue m_graphicsQueue{nullptr};
  vk::raii::Queue m_presentQueue{nullptr};
  dnm::SwapChainData m_swapChainData{nullptr};
  dnm::DepthBufferData m_depthBufferData{nullptr};
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
}  // namespace dnm
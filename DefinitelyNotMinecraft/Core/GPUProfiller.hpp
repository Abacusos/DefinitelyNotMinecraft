#pragma once

#include <vulkan/vulkan.hpp>
#include <tracy/TracyVulkan.hpp>

#include <Rendering/Renderer.hpp>

namespace dnm {
class GPUProfilerContext {
 public:
  GPUProfilerContext() = default;
  explicit GPUProfilerContext(const Renderer* renderer, const VkQueue& queue,
                              const vk::raii::CommandBuffer& commandBuffer) {
    context =
        TracyVkContext(*renderer->getPhysicalDevice(), *renderer->getDevice(), queue, *commandBuffer);
  }
  ~GPUProfilerContext() {
    if (context) {
      TracyVkDestroy(context);
    }
  }

  GPUProfilerContext(const GPUProfilerContext&) = delete;
  GPUProfilerContext& operator=(const GPUProfilerContext&) = delete;
  GPUProfilerContext(GPUProfilerContext&& other) noexcept
      : context{other.context} {
    other.context = nullptr;
  }
  GPUProfilerContext& operator=(GPUProfilerContext&& other) noexcept {
    context = other.context;
    other.context = nullptr;
    return *this;
  }

  tracy::VkCtx* context;
};
}  // namespace dnm
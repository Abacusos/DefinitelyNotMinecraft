#pragma once

#include <vulkan/vulkan.hpp>

#include <tracy/TracyVulkan.hpp>

#include <Rendering/Renderer.hpp>

namespace dnm
{
class GPUProfilerContext {
    public:
    GPUProfilerContext() = default;

    explicit GPUProfilerContext(const Renderer* renderer) {
        context = TracyVkContextHostCalibrated(
          *renderer->getPhysicalDevice(),
          *renderer->getDevice(),
          renderer->getDevice().getDispatcher()->vkResetQueryPool,
          renderer->getInstance().getDispatcher()->vkGetPhysicalDeviceCalibrateableTimeDomainsEXT,
          renderer->getDevice().getDispatcher()->vkGetCalibratedTimestampsEXT);
    }

    ~GPUProfilerContext() {
        if (context) {
            TracyVkDestroy(context);
        }
    }

    GPUProfilerContext(const GPUProfilerContext&)            = delete;
    GPUProfilerContext& operator=(const GPUProfilerContext&) = delete;

    GPUProfilerContext(GPUProfilerContext&& other) noexcept : context {other.context} { other.context = nullptr; }

    GPUProfilerContext& operator=(GPUProfilerContext&& other) noexcept {
        context       = other.context;
        other.context = nullptr;
        return *this;
    }

    tracy::VkCtx* context;
};
}   // namespace dnm

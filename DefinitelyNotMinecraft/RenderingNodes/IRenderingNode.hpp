#pragma once

#include <Core/ShortTypes.hpp>
#include <string_view>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace dnm {
class Camera;

class IRenderingNode {
 public:
  virtual ~IRenderingNode() = default;

  virtual std::string_view getName() const = 0;
  virtual bool shouldExecute() const = 0;

  struct ExecutionData {
    u32 frameBufferIndex;
    Camera* camera;
  };
  struct ExecutionResult {
    vk::PipelineStageFlags flags;
    const vk::raii::Queue& queue;
  };
  virtual ExecutionResult execute(const ExecutionData& executionData,
                                  vk::raii::CommandBuffer& commandBuffer) = 0;
};
}  // namespace dnm
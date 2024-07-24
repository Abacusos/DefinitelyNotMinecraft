#pragma once

#include <Core/ShortTypes.hpp>
#include <memory>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

namespace dnm {
class Camera;
class Renderer;
class IRenderingNode;

class RenderGraph {
 public:
  explicit RenderGraph(Renderer* renderer, Camera* camera);

  void registerNode(std::unique_ptr<IRenderingNode> node);

  bool prepareFrame();
  void build();
  void execute();

 private:
  Renderer* m_renderer{nullptr};
  Camera* m_camera{nullptr};

  std::vector<std::unique_ptr<IRenderingNode>> m_nodes;

  struct SortedNode {
    vk::raii::Semaphore semaphore;
    vk::raii::CommandBuffer commandBuffer;
    IRenderingNode* node;
  };
  std::vector<SortedNode> m_sortedNodes;
  u32 m_frameBufferIndex;
};
}  // namespace dnm
#include "Rendering/RenderGraph.hpp"

#include <Rendering/Renderer.hpp>
#include <RenderingNodes/IRenderingNode.hpp>

namespace dnm
{
RenderGraph::RenderGraph(Renderer* renderer, Camera* camera) : m_renderer(renderer), m_camera {camera} {}

void RenderGraph::registerNode(std::unique_ptr<IRenderingNode> node) {
    m_nodes.emplace_back(std::move(node));
}

bool RenderGraph::prepareFrame() {
    m_frameBufferIndex = m_renderer->prepareDrawFrame();
    return m_frameBufferIndex != std::numeric_limits<u32>::max();
}

void RenderGraph::build() {
    const auto& device = m_renderer->getDevice();
    m_sortedNodes.clear();
    m_sortedNodes.reserve(m_sortedNodes.size());
    for (const auto& node : m_nodes) {
        if (node->shouldExecute()) {
            auto name          = node->getName();
            auto commandBuffer = m_renderer->getCommandBuffer();
            registerDebugMarker(device, commandBuffer, name);

            auto semaphore = vk::raii::Semaphore(device, vk::SemaphoreCreateInfo());
            registerDebugMarker(device, semaphore, "Gizmo rendering finished");

            m_sortedNodes.emplace_back(std::move(semaphore), std::move(commandBuffer), node.get());
        }
    }
}

void RenderGraph::execute() {
    for (auto i = 0u; i < m_sortedNodes.size(); ++i) {
        auto& node = m_sortedNodes [i];

        IRenderingNode::ExecutionData executionData {m_frameBufferIndex, m_camera};
        auto                          executionResult = node.node->execute(executionData, node.commandBuffer);

        vk::ArrayProxyNoTemporaries<const vk::Semaphore> array;
        if (i != 0) {
            array = *m_sortedNodes [i - 1].semaphore;
        }

        const vk::SubmitInfo computeInfo {
          array,
          executionResult.flags == vk::PipelineStageFlagBits::eNone ? vk::ArrayProxyNoTemporaries<const vk::PipelineStageFlags> {} : executionResult.flags,
          *node.commandBuffer,
          *node.semaphore};
        executionResult.queue.submit(computeInfo);
    }

    if (!m_sortedNodes.empty()) {
        m_renderer->finishDrawFrame(m_sortedNodes [m_sortedNodes.size() - 1].semaphore, m_frameBufferIndex);
    }
}
}   // namespace dnm

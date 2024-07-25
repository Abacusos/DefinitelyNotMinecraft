#include "RenderingNodes/GizmoRenderingNode.hpp"

#include <Core/GLMInclude.hpp>
#include <Core/Profiler.hpp>
#include <Core/StringInterner.hpp>

#include <Shader/ShaderManager.hpp>

namespace dnm
{
namespace
{
    constexpr std::string_view vertexShader   = "Shaders/Gizmo.vert";
    constexpr std::string_view fragmentShader = "Shaders/Gizmo.frag";
}   // namespace

GizmoRenderingNode::GizmoRenderingNode(
  Config*         config,
  Renderer*       renderer,
  ShaderManager*  shaderManager,
  BlockWorld*     blockWorld,
  StringInterner* interner,
  GizmoData*      gizmoData) :
    m_config {config}, m_renderer {renderer}, m_shaderManager {shaderManager}, m_blockWorld {blockWorld}, m_interner {interner}, m_gizmoData {gizmoData} {
    const auto& physicalDevice = m_renderer->getPhysicalDevice();
    const auto& device         = m_renderer->getDevice();

    m_vertexHandle   = m_shaderManager->registerShaderFile(m_interner->addOrGetString(vertexShader), vk::ShaderStageFlagBits::eVertex);
    m_fragmentHandle = m_shaderManager->registerShaderFile(m_interner->addOrGetString(fragmentShader), vk::ShaderStageFlagBits::eFragment);

    m_vertexBuffer = BufferData(physicalDevice, device, GizmoData::reservedGizmoSpace * sizeof(VertexGizmo), vk::BufferUsageFlagBits::eVertexBuffer);
    registerDebugMarker(device, m_vertexBuffer.buffer, "VertexBuffer Gizmos");

    m_renderPass = makeRenderPass(device, renderer->getColorFormat(), vk::Format::eD32Sfloat, vk::AttachmentLoadOp::eLoad);

    recompileShadersIfNecessary(true);
}

std::string_view GizmoRenderingNode::getName() const {
    return "GizmoRenderingNode";
}

bool GizmoRenderingNode::shouldExecute() const {
    return m_gizmoData->m_occupiedVertexPlaces > 0;
}

IRenderingNode::ExecutionResult GizmoRenderingNode::execute(const ExecutionData& executionData, vk::raii::CommandBuffer& commandBuffer) {
    ZoneScoped;

    const auto extent = m_renderer->getExtent();

    auto& frameBuffer = m_renderer->getFrameBuffer(executionData.frameBufferIndex);

    recompileShadersIfNecessary();

    const vk::RenderPassBeginInfo renderPassBeginInfo(*m_renderPass, *frameBuffer, vk::Rect2D(vk::Offset2D(0, 0), extent));

    commandBuffer.reset();
    commandBuffer.begin(vk::CommandBufferBeginInfo());

    {
        copyToDevice(m_vertexBuffer.deviceMemory, std::span<const VertexGizmo>(m_gizmoData->m_verticesGizmo.data(), m_gizmoData->m_occupiedVertexPlaces));
        m_gizmoData->reset();

        commandBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_graphicsPipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *m_pipelineLayout, 0, {*m_descriptorSet}, nullptr);

        commandBuffer.bindVertexBuffers(0, {*m_vertexBuffer.buffer}, {0});

        commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 1.0f, 0.0f));
        commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), extent));
        commandBuffer.draw(m_gizmoData->m_verticesGizmo.size(), 1u, 0u, 0u);

        commandBuffer.endRenderPass();
    }
    commandBuffer.end();

    return ExecutionResult {{vk::PipelineStageFlagBits::eColorAttachmentOutput}, m_renderer->getGraphicsQueue()};
}

void GizmoRenderingNode::recreatePipeline() {
    m_renderer->waitIdle();

    std::array layout {
      // projection
      std::tuple {vk::DescriptorType::eUniformBuffer, 1u, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eCompute},
      // view
      std::tuple {vk::DescriptorType::eUniformBuffer, 1u, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eCompute}
    };

    std::array sizes {
      vk::DescriptorPoolSize {vk::DescriptorType::eUniformBuffer, 2u}
    };

    const auto& device = m_renderer->getDevice();

    m_descriptorSet.clear();

    m_descriptorSetLayout = makeDescriptorSetLayout(device, layout);
    m_pipelineLayout      = vk::raii::PipelineLayout(device, {{}, *m_descriptorSetLayout});

    auto sets       = vk::raii::DescriptorSets(device, {*m_renderer->getDescriptorPool(), *m_descriptorSetLayout});
    m_descriptorSet = std::move(sets.front());

    m_graphicsPipeline = makeGraphicsPipeline(
      m_config,
      device,
      m_renderer->getPipelineCache(),
      m_vertexShaderModule,
      nullptr,
      m_fragmentShaderModule,
      nullptr,
      sizeof(VertexGizmo),
      {
        {vk::Format::eR32G32B32Sfloat,  0},
        {vk::Format::eR32G32B32Sfloat, 12}
    },
      vk::FrontFace::eCounterClockwise,
      true,
      m_pipelineLayout,
      m_renderer->getRenderPass(),
      vk::PrimitiveTopology::eLineList);
    registerDebugMarker(device, m_graphicsPipeline, "Gizmo Graphics Pipeline");

    const auto* projectionClipBuffer = m_renderer->getGlobalBuffer(GlobalBuffers::ProjectionClip);
    assert(projectionClipBuffer);
    const auto* viewBuffer = m_renderer->getGlobalBuffer(GlobalBuffers::CameraView);
    assert(viewBuffer);

    std::array update {
      std::tuple<vk::DescriptorType, const vk::raii::Buffer&, vk::DeviceSize, const vk::raii::BufferView*> {
                                                                                                            vk::DescriptorType::eUniformBuffer, *projectionClipBuffer, VK_WHOLE_SIZE, nullptr},
      std::tuple<vk::DescriptorType, const vk::raii::Buffer&, vk::DeviceSize, const vk::raii::BufferView*> {
                                                                                                            vk::DescriptorType::eUniformBuffer,           *viewBuffer, VK_WHOLE_SIZE, nullptr}
    };

    updateDescriptorSets(device, m_descriptorSet, update);
}

void GizmoRenderingNode::recompileShadersIfNecessary(bool force) {
    const auto& device = m_renderer->getDevice();

    bool anyUpdated = false;

    auto processShader = [this, &device, &anyUpdated, &force](ShaderHandle handle, vk::raii::ShaderModule& shaderModule)
    {
        if (m_shaderManager->wasContentUpdated(handle) || force) {
            auto recompiledVertexShader = m_shaderManager->getCompiledVersion(device, handle, {});
            if (recompiledVertexShader) {
                anyUpdated   = true;
                shaderModule = std::move(recompiledVertexShader.value());
            }
        }
    };

    processShader(m_vertexHandle, m_vertexShaderModule);
    processShader(m_fragmentHandle, m_fragmentShaderModule);

    if (anyUpdated) {
        recreatePipeline();
        std::cout << "Successfully recompiled shaders and recreated the pipeline "
                     "for the gizmo rendering module.\n";
    }
}
}   // namespace dnm

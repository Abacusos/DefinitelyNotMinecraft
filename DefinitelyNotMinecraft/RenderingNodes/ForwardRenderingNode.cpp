#include "RenderingNodes/ForwardRenderingNode.hpp"

#include <Core/GLMInclude.hpp>
#include <Core/Profiler.hpp>
#include <Core/StringInterner.hpp>

#include <Logic/Camera.hpp>

#include <Shader/ShaderManager.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace dnm
{
namespace
{
    struct LightBuffer
    {
        v3    lightColor;
        float ambientStrength;
        v3    lightPos;
        float specularStrength;
        v3    lightDirection;
    };

    constexpr std::string_view vertexShader   = "Shaders/World.vert";
    constexpr std::string_view fragmentShader = "Shaders/World.frag";
        
    constexpr std::string_view lightBindingPoint        = "lightBuffer";

}   // namespace

ForwardRenderingNode::ForwardRenderingNode(Config* config, Renderer* renderer, ShaderManager* shaderManager, BlockWorld* blockWorld, StringInterner* interner) :
    m_config {config}, m_renderer {renderer}, m_shaderManager {shaderManager}, m_blockWorld {blockWorld}, m_interner {interner} {
    const auto& physicalDevice = m_renderer->getPhysicalDevice();
    const auto& device         = m_renderer->getDevice();

    m_vertexHandle   = m_shaderManager->registerShaderFile(m_interner->addOrGetString(vertexShader), vk::ShaderStageFlagBits::eVertex);
    m_fragmentHandle = m_shaderManager->registerShaderFile(m_interner->addOrGetString(fragmentShader), vk::ShaderStageFlagBits::eFragment);

    int      texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load("Textures/TextureSheet.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    // Preventing the last few mipmaps due to artifacts in the distance where the
    // colors are mixed to gray then
    const u32 mipLevels = static_cast<u32>(std::floor(std::log2(std::max(texWidth, texHeight)))) - 3;

    m_textureData = TextureData(
      physicalDevice,
      device,
      vk::Extent2D(texWidth, texHeight),
      vk::Format::eR8G8B8A8Unorm,
      vk::SamplerAddressMode::eClampToEdge,
      mipLevels,
      vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst,
      {},
      false,
      true);
    m_renderer->oneTimeSubmit(
      [&](const vk::raii::CommandBuffer& commandBuffer)
      {
          m_textureData.setImage(commandBuffer, pixels, false, mipLevels);

          vk::ImageMemoryBarrier barrier {};
          barrier.image                           = *m_textureData.imageData.image;
          barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
          barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
          barrier.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
          barrier.subresourceRange.baseArrayLayer = 0;
          barrier.subresourceRange.layerCount     = 1;
          barrier.subresourceRange.levelCount     = 1;

          i32 mipWidth  = texWidth;
          i32 mipHeight = texHeight;

          for (u32 i = 1; i < mipLevels; i++) {
              barrier.subresourceRange.baseMipLevel = i - 1;
              barrier.oldLayout                     = vk::ImageLayout::eTransferDstOptimal;
              barrier.newLayout                     = vk::ImageLayout::eTransferSrcOptimal;
              barrier.srcAccessMask                 = vk::AccessFlagBits::eTransferWrite;
              barrier.dstAccessMask                 = vk::AccessFlagBits::eTransferRead;

              commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);

              std::array srcOffsets {
                vk::Offset3D {       0,         0, 0},
                 vk::Offset3D {mipWidth, mipHeight, 1}
              };
              std::array dstOffsets {
                vk::Offset3D {                              0,                                 0, 0},
                 vk::Offset3D {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1}
              };

              vk::ImageBlit blit {
                {vk::ImageAspectFlagBits::eColor, i - 1, 0, 1},
                srcOffsets, {vk::ImageAspectFlagBits::eColor,     i, 0, 1},
                dstOffsets
              };

              commandBuffer.blitImage(
                *m_textureData.imageData.image,
                vk::ImageLayout::eTransferSrcOptimal,
                *m_textureData.imageData.image,
                vk::ImageLayout::eTransferDstOptimal,
                blit,
                vk::Filter::eLinear);

              barrier.oldLayout     = vk::ImageLayout::eTransferSrcOptimal;
              barrier.newLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
              barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
              barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

              commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);

              if (mipWidth > 1) {
                  mipWidth /= 2;
              }
              if (mipHeight > 1) {
                  mipHeight /= 2;
              }
          }

          barrier.subresourceRange.baseMipLevel = mipLevels - 1;
          barrier.oldLayout                     = vk::ImageLayout::eTransferDstOptimal;
          barrier.newLayout                     = vk::ImageLayout::eShaderReadOnlyOptimal;
          barrier.srcAccessMask                 = vk::AccessFlagBits::eTransferWrite;
          barrier.dstAccessMask                 = vk::AccessFlagBits::eShaderRead;

          commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);

          stbi_image_free(pixels);
      });
    registerDebugMarker(device, m_textureData.imageData.image, "TextureSheet");

    m_renderingProfilerContext = GPUProfilerContext(m_renderer);

    m_lightBuffer = m_renderer->createBuffer(
      sizeof(LightBuffer),
      vk::BufferUsageFlagBits::eUniformBuffer,
      "Light Buffer",
      vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible);

    recompileShadersIfNecessary(true);
}

std::string_view ForwardRenderingNode::getName() const {
    return "ForwardRenderingNode";
}

bool ForwardRenderingNode::shouldExecute() const {
    return true;
}

IRenderingNode::ExecutionResult ForwardRenderingNode::execute(const ExecutionData& executionData, vk::raii::CommandBuffer& commandBuffer) {
    ZoneScoped;

    auto& frameBuffer = m_renderer->getFrameBuffer(executionData.frameBufferIndex);

    recompileShadersIfNecessary();
    const auto extent = m_renderer->getExtent();

    if (m_config->updateLight) {
        LightBuffer buffer {m_config->lightColor, m_config->ambientStrength, m_config->lightPosition, m_config->specularStrength};
        copyToDevice(m_lightBuffer.deviceMemory, std::span<const LightBuffer> {&buffer, 1u});
        m_config->updateLight = false;
    }

    {
        std::array<vk::ClearValue, 2> clearValues;
        clearValues [0].color        = vk::ClearColorValue(0.2f, 0.2f, 0.2f, 0.2f);
        clearValues [1].depthStencil = vk::ClearDepthStencilValue(0.0f, 0);

        const auto&                   pass = m_renderer->getRenderPass();
        const vk::RenderPassBeginInfo renderPassBeginInfo(*pass, *frameBuffer, vk::Rect2D(vk::Offset2D(0, 0), extent), clearValues);

        commandBuffer.reset();
        {
            commandBuffer.begin(vk::CommandBufferBeginInfo());
            TracyVkZone(m_renderingProfilerContext.context, *commandBuffer, "Draw Blocks");
            TracyVkCollect(m_renderingProfilerContext.context, *commandBuffer);

            commandBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
            commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_graphicsPipeline);
            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *m_pipelineLayout, 0, {*m_descriptorSet}, nullptr);

            commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 1.0f, 0.0f));
            commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), extent));
            auto* buffer = m_renderer->getGlobalBuffer(GlobalBuffers::DrawCommand);
            commandBuffer.drawIndirect(**buffer, 0u, 1u, sizeof(u32) * 4);

            commandBuffer.endRenderPass();
        }
        commandBuffer.end();
    }

    return ExecutionResult {{vk::PipelineStageFlagBits::eColorAttachmentOutput}, m_renderer->getGraphicsQueue()};
}

void ForwardRenderingNode::recreatePipeline() {
    m_renderer->waitIdle();

    std::vector<BindingSlot> slots;
    vk::ShaderStageFlags     stageFlags;
    std::array               internedString {m_interner->addOrGetString(vertexShader), m_interner->addOrGetString(fragmentShader)};
    m_shaderManager->getBindingSlots(internedString, slots, stageFlags);
    const auto& device = m_renderer->getDevice();

    m_descriptorSet.clear();
    
    m_descriptorSetLayout = makeDescriptorSetLayout(device, slots, stageFlags);
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
      0,
      {},
      vk::FrontFace::eCounterClockwise,
      true,
      m_pipelineLayout,
      m_renderer->getRenderPass());
    registerDebugMarker(device, m_graphicsPipeline, "Block Rendering Graphics Pipeline");

    const auto* projectionClipBuffer = m_renderer->getGlobalBuffer(GlobalBuffers::ProjectionClip);
    assert(projectionClipBuffer);
    const auto* viewBuffer = m_renderer->getGlobalBuffer(GlobalBuffers::CameraView);
    assert(viewBuffer);
    const auto* blockType = m_renderer->getGlobalBuffer(GlobalBuffers::BlockType);
    assert(blockType);
    const auto* transform = m_renderer->getGlobalBuffer(GlobalBuffers::Transform);
    assert(transform);

    std::array update {
      DescriptorSlotUpdate {projectionBufferBindingPoint, *projectionClipBuffer, VK_WHOLE_SIZE, nullptr},
      DescriptorSlotUpdate {viewBufferBindingPoint,           *viewBuffer, VK_WHOLE_SIZE, nullptr},
      DescriptorSlotUpdate {transformBindingPoint,            *transform, VK_WHOLE_SIZE, nullptr},
      DescriptorSlotUpdate {blockTypeBindingPoint,            *blockType, VK_WHOLE_SIZE, nullptr},
      DescriptorSlotUpdate {lightBindingPoint,  m_lightBuffer.buffer, VK_WHOLE_SIZE, nullptr},
    };

    std::array textureUpdate {
        TextureSlotUpdate{"tex;", m_textureData}
    };

    updateDescriptorSets(device, m_descriptorSet, update, slots, textureUpdate);
}

void ForwardRenderingNode::recompileShadersIfNecessary(bool force) {
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
                     "for the block rendering module.\n";
    }
}
}   // namespace dnm

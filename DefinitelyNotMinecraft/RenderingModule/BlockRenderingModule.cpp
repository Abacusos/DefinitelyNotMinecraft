#include "BlockRenderingModule.hpp"

#include <random>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "GLMInclude.hpp"
#include "ShaderManager.hpp"
#include "StringInterner.hpp"
#include "optick.h"

namespace dnm {
namespace {
struct VertexPT {
  float x, y, z;  // Position data
  float u, v;     // texture u,v
};

constexpr float oneSixth = 1.0f / 12.0f;
constexpr float twoSixth = 2.0f / 12.0f;
constexpr float threeSixth = 3.0f / 12.0f;
constexpr float fourSixth = 4.0f / 12.0f;
constexpr float fiveSixth = 5.0f / 12.0f;
constexpr float sixSixth = 6.0f / 12.0f;

constexpr float oneTwelveth = 1.0f / 12.0f;

static const VertexPT texturedCubeData[] = {
    // left face
    {-0.5f, -0.5f, -0.5f, twoSixth, oneTwelveth},
    {-0.5f, 0.5f, 0.5f, oneSixth, 0.0f},
    {-0.5f, -0.5f, 0.5f, oneSixth, oneTwelveth},
    {-0.5f, 0.5f, 0.5f, oneSixth, 0.0f},
    {-0.5f, -0.5f, -0.5f, twoSixth, oneTwelveth},
    {-0.5f, 0.5f, -0.5f, twoSixth, 0.0f},
    // front face
    {0.5f, 0.5f, 0.5f, threeSixth, 0.0f},
    {-0.5f, 0.5f, 0.5f, twoSixth, 0.0f},
    {-0.5f, -0.5f, 0.5f, twoSixth, oneTwelveth},
    {-0.5f, -0.5f, 0.5f, twoSixth, oneTwelveth},
    {0.5f, -0.5f, 0.5f, threeSixth, oneTwelveth},
    {0.5f, 0.5f, 0.5f, threeSixth, 0.0f},
    // top face
    {-0.5f, 0.5f, -0.5f, fiveSixth, 0.0f},
    {0.5f, 0.5f, -0.5f, sixSixth, 0.0f},
    {0.5f, 0.5f, 0.5f, sixSixth, oneTwelveth},
    {-0.5f, 0.5f, -0.5f, fiveSixth, 0.0f},
    {0.5f, 0.5f, 0.5f, sixSixth, oneTwelveth},
    {-0.5f, 0.5f, 0.5f, fiveSixth, oneTwelveth},
    // bottom face
    {-0.5f, -0.5f, -0.5f, 0.0f, oneTwelveth},
    {0.5f, -0.5f, 0.5f, oneSixth, 0.0f},
    {-0.5f, -0.5f, 0.5f, 0.0f, 0.0f},
    {-0.5f, -0.5f, -0.5f, 0.0f, oneTwelveth},
    {0.5f, -0.5f, -0.5f, oneSixth, oneTwelveth},
    {0.5f, -0.5f, 0.5f, oneSixth, 0.0f},
    // right face
    {0.5f, 0.5f, -0.5f, threeSixth, 0.0f},
    {0.5f, 0.5f, 0.5f, fourSixth, 0.0f},
    {0.5f, -0.5f, 0.5f, fourSixth, oneTwelveth},
    {0.5f, -0.5f, 0.5f, fourSixth, oneTwelveth},
    {0.5f, -0.5f, -0.5f, threeSixth, oneTwelveth},
    {0.5f, 0.5f, -0.5f, threeSixth, 0.0f},
    // back face
    {0.5f, 0.5f, -0.5f, fourSixth, 0.0f},
    {-0.5f, 0.5f, -0.5f, fiveSixth, 0.0f},
    {-0.5f, -0.5f, -0.5f, fiveSixth, oneTwelveth},
    {-0.5f, -0.5f, -0.5f, fiveSixth, oneTwelveth},
    {0.5f, -0.5f, -0.5f, fourSixth, oneTwelveth},
    {0.5f, 0.5f, -0.5f, fourSixth, 0.0f},
};

constexpr std::string_view vertexShader = "Shaders/World.vert";
constexpr std::string_view fragmentShader = "Shaders/World.frag";
constexpr std::string_view computeShader =
    "Shaders/DrawCallGenerationWorld.comp";

constexpr std::string_view localWGSizeX = "LOCAL_SIZE_X";

}  // namespace

BlockRenderingModule::BlockRenderingModule(Config* config, Renderer* renderer,
                                           ShaderManager* shaderManager,
                                           BlockWorld* blockWorld,
                                           StringInterner* interner)
    : m_config{config},
      m_renderer{renderer},
      m_shaderManager{shaderManager},
      m_blockWorld{blockWorld},
      m_interner{interner} {
  const auto& physicalDevice = m_renderer->getPhysicalDevice();
  const auto& device = m_renderer->getDevice();

  m_vertexHandle = m_shaderManager->registerShaderFile(
      m_interner->addOrGetString(vertexShader),
      vk::ShaderStageFlagBits::eVertex);
  m_fragmentHandle = m_shaderManager->registerShaderFile(
      m_interner->addOrGetString(fragmentShader),
      vk::ShaderStageFlagBits::eFragment);
  m_computeHandle = m_shaderManager->registerShaderFile(
      m_interner->addOrGetString(computeShader),
      vk::ShaderStageFlagBits::eCompute);

  int texWidth, texHeight, texChannels;
  stbi_uc* pixels = stbi_load("Textures/TextureSheet.png", &texWidth,
                              &texHeight, &texChannels, STBI_rgb_alpha);

  u32 mipLevels =
      static_cast<u32>(std::floor(std::log2(std::max(texWidth, texHeight)))) +
      1;

  m_textureData =
      TextureData(physicalDevice, device, vk::Extent2D(texWidth, texHeight),
                  vk::Format::eR8G8B8A8Unorm,
                  vk::SamplerAddressMode::eClampToEdge, mipLevels,
                  vk::ImageUsageFlagBits::eTransferSrc |
                      vk::ImageUsageFlagBits::eTransferDst,
                  {}, false, true);
  m_renderer->oneTimeSubmit([&](const vk::raii::CommandBuffer& commandBuffer) {
    m_textureData.setImage(commandBuffer, pixels, false, mipLevels);

    vk::ImageMemoryBarrier barrier{};
    barrier.image = *m_textureData.imageData.image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    i32 mipWidth = texWidth;
    i32 mipHeight = texHeight;

    for (u32 i = 1; i < mipLevels; i++) {
      barrier.subresourceRange.baseMipLevel = i - 1;
      barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
      barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
      barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
      barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

      commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                    vk::PipelineStageFlagBits::eTransfer, {},
                                    {}, {}, barrier);

      std::array<vk::Offset3D, 2u> srcOffsets{
          vk::Offset3D{0, 0, 0}, vk::Offset3D{mipWidth, mipHeight, 1}};
      std::array<vk::Offset3D, 2u> dstOffsets{
          vk::Offset3D{0, 0, 0},
          vk::Offset3D{mipWidth > 1 ? mipWidth / 2 : 1,
                       mipHeight > 1 ? mipHeight / 2 : 1, 1}};

      vk::ImageBlit blit{{vk::ImageAspectFlagBits::eColor, i - 1, 0, 1},
                         srcOffsets,
                         {vk::ImageAspectFlagBits::eColor, i, 0, 1},
                         dstOffsets};

      commandBuffer.blitImage(
          *m_textureData.imageData.image, vk::ImageLayout::eTransferSrcOptimal,
          *m_textureData.imageData.image, vk::ImageLayout::eTransferDstOptimal,
          blit, vk::Filter::eLinear);

      barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
      barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
      barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
      barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

      commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                    vk::PipelineStageFlagBits::eFragmentShader,
                                    {}, {}, {}, barrier);

      if (mipWidth > 1) mipWidth /= 2;
      if (mipHeight > 1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                  vk::PipelineStageFlagBits::eFragmentShader,
                                  {}, {}, {}, barrier);

    stbi_image_free(pixels);
  });
  registerDebugMarker(device, m_textureData.imageData.image, "TextureSheet");

  m_vertexBuffer = BufferData(physicalDevice, device, sizeof(texturedCubeData),
                              vk::BufferUsageFlagBits::eVertexBuffer);
  copyToDevice(m_vertexBuffer.deviceMemory,
               std::span<const VertexPT>(texturedCubeData));

  registerDebugMarker(device, m_vertexBuffer.buffer, "VertexBuffer Blocks");

  m_drawCommandBuffer =
      BufferData(physicalDevice, device, 4 * sizeof(u32),
                 vk::BufferUsageFlagBits::eStorageBuffer |
                     vk::BufferUsageFlagBits::eIndirectBuffer);
  registerDebugMarker(device, m_drawCommandBuffer.buffer, "Drawcommand Buffer");

  m_transformBuffer = m_renderer->createBuffer(
      BlockWorld::totalBlockCount * sizeof(glm::mat4x4),
      vk::BufferUsageFlagBits::eStorageBuffer, "Instance Transform Buffer");

  m_worldDataBuffer = m_renderer->createBuffer(
      BlockWorld::totalBlockCount * sizeof(u8),
      vk::BufferUsageFlagBits::eStorageBuffer, "World Data Blocks");

  m_blockTypeBuffer = m_renderer->createBuffer(
      BlockWorld::totalBlockCount * sizeof(u8),
      vk::BufferUsageFlagBits::eStorageBuffer, "Block Data For Draw Command");

  copyToDevice(m_worldDataBuffer.deviceMemory,
               std::span<const u8>(m_blockWorld->getBlockTypes()));

  m_chunkConstantsBuffer = m_renderer->createBuffer(
      3 * sizeof(u32), vk::BufferUsageFlagBits::eUniformBuffer,
      "Chunk Constants");

  std::array<u32, 3u> constants{BlockWorld::chunkLoadCount,
                                BlockWorld::chunkLocalSize,
                                BlockWorld::chunkHeight};
  copyToDevice(m_chunkConstantsBuffer.deviceMemory,
               std::span<const u32>(constants));

  m_commandBufferCompute = m_renderer->getCommandBuffer();
  m_commandBuffer = m_renderer->getCommandBuffer();

  m_drawCallGenerationFinished =
      vk::raii::Semaphore(device, vk::SemaphoreCreateInfo());
  registerDebugMarker(device, m_drawCallGenerationFinished,
                      "Drawcall generation compute finished");

  m_renderingFinished = vk::raii::Semaphore(device, vk::SemaphoreCreateInfo());
  registerDebugMarker(device, m_renderingFinished, "Block rendering finished");

  recompileShadersIfNecessary(true);
}

void BlockRenderingModule::drawFrame(const vk::raii::Framebuffer& frameBuffer,
                                     TimeSpan dt, bool cameraMoved) {
  OPTICK_EVENT();
  const auto& device = m_renderer->getDevice();

  recompileShadersIfNecessary();

  auto extent = m_renderer->getExtent();
  bool submitCompute = m_blockWorld->wasModified() || cameraMoved || true;
  if (submitCompute) {
    std::array<const u32, 4> empty{36u, 0u, 0u, 0u};
    copyToDevice<u32>(m_drawCommandBuffer.deviceMemory, std::span(empty));

    m_commandBufferCompute.reset();
    m_commandBufferCompute.begin(vk::CommandBufferBeginInfo());
    OPTICK_GPU_CONTEXT(static_cast<VkCommandBuffer>(*m_commandBufferCompute));
    OPTICK_GPU_EVENT("Compute Draw Calls");
    m_commandBufferCompute.bindPipeline(vk::PipelineBindPoint::eCompute,
                                        *m_computePipeline);
    m_commandBufferCompute.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                              *m_pipelineLayout, 0u,
                                              {*m_descriptorSet}, nullptr);
    m_commandBufferCompute.dispatch(BlockWorld::chunkHeight *
                                        BlockWorld::chunkLoadCount *
                                        BlockWorld::chunkLoadCount,
                                    1, 1);
    m_commandBufferCompute.end();

    auto computeQueue = m_renderer->getComputeQueue();
    vk::SubmitInfo computeInfo{
        {}, {}, *m_commandBufferCompute, *m_drawCallGenerationFinished};
    computeQueue.submit(computeInfo);
  }

  {
    std::array<vk::ClearValue, 2> clearValues;
    clearValues[0].color = vk::ClearColorValue(0.2f, 0.2f, 0.2f, 0.2f);
    clearValues[1].depthStencil = m_config->useInverseDepth
                                      ? vk::ClearDepthStencilValue(0.0f, 0)
                                      : vk::ClearDepthStencilValue(1.0f, 0);

    const auto& pass = m_renderer->getRenderPass();
    vk::RenderPassBeginInfo renderPassBeginInfo(
        *pass, *frameBuffer, vk::Rect2D(vk::Offset2D(0, 0), extent),
        clearValues);

    m_commandBuffer.reset();
    m_commandBuffer.begin(vk::CommandBufferBeginInfo());
    OPTICK_GPU_CONTEXT(static_cast<VkCommandBuffer>(*m_commandBuffer));
    OPTICK_GPU_EVENT("Render Blocks");
    m_commandBuffer.beginRenderPass(renderPassBeginInfo,
                                    vk::SubpassContents::eInline);
    m_commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                 *m_graphicsPipeline);
    m_commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                       *m_pipelineLayout, 0, {*m_descriptorSet},
                                       nullptr);

    m_commandBuffer.bindVertexBuffers(0, {*m_vertexBuffer.buffer}, {0});

    m_commandBuffer.setViewport(
        0, vk::Viewport(0.0f, 0.0f, static_cast<float>(extent.width),
                        static_cast<float>(extent.height), 1.0f, 0.0f));
    m_commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), extent));
    m_commandBuffer.drawIndirect(*m_drawCommandBuffer.buffer, 0u, 1u,
                                 sizeof(u32) * 4);

    m_commandBuffer.endRenderPass();
    m_commandBuffer.end();

    auto graphicsQueue = m_renderer->getGraphicsQueue();

    vk::SubmitInfo graphicsInfo{{}, {}, *m_commandBuffer, *m_renderingFinished};
    if (submitCompute) {
      graphicsInfo.setWaitSemaphores(*m_drawCallGenerationFinished);
      vk::PipelineStageFlags waitDestinationStageMask(
          vk::PipelineStageFlagBits::eComputeShader);
      graphicsInfo.setWaitDstStageMask(waitDestinationStageMask);
    }

    graphicsQueue.submit(graphicsInfo);
  }
}

vk::raii::Semaphore& BlockRenderingModule::getRenderingFinishedSemaphore() {
  return m_renderingFinished;
}

void BlockRenderingModule::recreatePipeline() {
  m_renderer->waitIdle();

  std::array<std::tuple<vk::DescriptorType, u32, vk::ShaderStageFlags>, 8>
      layout{

          // projection
          std::tuple<vk::DescriptorType, u32, vk::ShaderStageFlags>{
              vk::DescriptorType::eUniformBuffer, 1u,
              vk::ShaderStageFlagBits::eVertex |
                  vk::ShaderStageFlagBits::eCompute},
          // view
          std::tuple<vk::DescriptorType, u32, vk::ShaderStageFlags>{
              vk::DescriptorType::eUniformBuffer, 1u,
              vk::ShaderStageFlagBits::eVertex |
                  vk::ShaderStageFlagBits::eCompute},
          // transform
          std::tuple<vk::DescriptorType, u32, vk::ShaderStageFlags>{
              vk::DescriptorType::eStorageBuffer, 1u,
              vk::ShaderStageFlagBits::eVertex |
                  vk::ShaderStageFlagBits::eCompute},
          // block type
          std::tuple<vk::DescriptorType, u32, vk::ShaderStageFlags>{
              vk::DescriptorType::eStorageBuffer, 1u,
              vk::ShaderStageFlagBits::eVertex |
                  vk::ShaderStageFlagBits::eCompute},
          // world data block type
          std::tuple<vk::DescriptorType, u32, vk::ShaderStageFlags>{
              vk::DescriptorType::eStorageBuffer, 1u,
              vk::ShaderStageFlagBits::eCompute},
          // draw call buffer
          std::tuple<vk::DescriptorType, u32, vk::ShaderStageFlags>{
              vk::DescriptorType::eStorageBuffer, 1u,
              vk::ShaderStageFlagBits::eCompute},
          // constexpr chunk constants
          std::tuple<vk::DescriptorType, u32, vk::ShaderStageFlags>{
              vk::DescriptorType::eUniformBuffer, 1u,
              vk::ShaderStageFlagBits::eCompute},
          std::tuple<vk::DescriptorType, u32, vk::ShaderStageFlags>{
              vk::DescriptorType::eCombinedImageSampler, 1,
              vk::ShaderStageFlagBits::eFragment}};

  std::array<vk::DescriptorPoolSize, 3u> sizes{
      vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer, 3u},
      vk::DescriptorPoolSize{vk::DescriptorType::eStorageBuffer, 4u},
      vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, 1u}};

  const auto& device = m_renderer->getDevice();

  m_descriptorSet.clear();

  m_descriptorSetLayout = makeDescriptorSetLayout(device, layout);
  m_pipelineLayout =
      vk::raii::PipelineLayout(device, {{}, *m_descriptorSetLayout});

  m_descriptorPool = makeDescriptorPool(device, sizes);
  auto sets = vk::raii::DescriptorSets(
      device, {*m_descriptorPool, *m_descriptorSetLayout});
  m_descriptorSet = std::move(sets.front());

  m_graphicsPipeline = makeGraphicsPipeline(
      m_config, device, m_renderer->getPipelineCache(), m_vertexShaderModule,
      nullptr, m_fragmentShaderModule, nullptr, sizeof(texturedCubeData[0]),
      {{vk::Format::eR32G32B32Sfloat, 0}, {vk::Format::eR32G32Sfloat, 12}},
      vk::FrontFace::eCounterClockwise, true, m_pipelineLayout,
      m_renderer->getRenderPass());

  m_computePipeline = makeComputePipeline(
      device, m_renderer->getPipelineCache(), m_drawCallGenerationComputeModule,
      nullptr, m_pipelineLayout);

  const auto* projectionClipBuffer =
      m_renderer->getGlobalBuffer(GlobalBuffers::ProjectionClip);
  assert(projectionClipBuffer);
  const auto* viewBuffer =
      m_renderer->getGlobalBuffer(GlobalBuffers::CameraView);
  assert(viewBuffer);

  std::array<std::tuple<vk::DescriptorType, const vk::raii::Buffer&,
                        vk::DeviceSize, const vk::raii::BufferView*>,
             7u>
      update{std::tuple<vk::DescriptorType, const vk::raii::Buffer&,
                        vk::DeviceSize, const vk::raii::BufferView*>{
                 vk::DescriptorType::eUniformBuffer, *projectionClipBuffer,
                 VK_WHOLE_SIZE, nullptr},
             std::tuple<vk::DescriptorType, const vk::raii::Buffer&,
                        vk::DeviceSize, const vk::raii::BufferView*>{
                 vk::DescriptorType::eUniformBuffer, *viewBuffer, VK_WHOLE_SIZE,
                 nullptr},
             std::tuple<vk::DescriptorType, const vk::raii::Buffer&,
                        vk::DeviceSize, const vk::raii::BufferView*>{
                 vk::DescriptorType::eStorageBuffer, m_transformBuffer.buffer,
                 VK_WHOLE_SIZE, nullptr},
             std::tuple<vk::DescriptorType, const vk::raii::Buffer&,
                        vk::DeviceSize, const vk::raii::BufferView*>{
                 vk::DescriptorType::eStorageBuffer, m_blockTypeBuffer.buffer,
                 VK_WHOLE_SIZE, nullptr},
             std::tuple<vk::DescriptorType, const vk::raii::Buffer&,
                        vk::DeviceSize, const vk::raii::BufferView*>{
                 vk::DescriptorType::eStorageBuffer, m_worldDataBuffer.buffer,
                 VK_WHOLE_SIZE, nullptr},
             std::tuple<vk::DescriptorType, const vk::raii::Buffer&,
                        vk::DeviceSize, const vk::raii::BufferView*>{
                 vk::DescriptorType::eStorageBuffer, m_drawCommandBuffer.buffer,
                 VK_WHOLE_SIZE, nullptr},
             std::tuple<vk::DescriptorType, const vk::raii::Buffer&,
                        vk::DeviceSize, const vk::raii::BufferView*>{
                 vk::DescriptorType::eUniformBuffer,
                 m_chunkConstantsBuffer.buffer, VK_WHOLE_SIZE, nullptr}};

  updateDescriptorSets(device, m_descriptorSet, update, {m_textureData});
}

void BlockRenderingModule::recompileShadersIfNecessary(bool force) {
  const auto& device = m_renderer->getDevice();

  bool anyUpdated = false;

  auto processShader = [this, &device, &anyUpdated, &force](
                           ShaderHandle handle,
                           vk::raii::ShaderModule& shaderModule, 
      std::span<ShaderManager::Define> defines = {}) {
    if (m_shaderManager->wasContentUpdated(handle) || force) {
      auto recompiledVertexShader =
          m_shaderManager->getCompiledVersion(device, handle, defines);
      if (recompiledVertexShader) {
        anyUpdated = true;
        shaderModule = std::move(recompiledVertexShader.value());
      }
    }
  };

    std::array defines = {
      ShaderManager::Define{m_interner->addOrGetString(localWGSizeX),
                            std::to_string(m_blockWorld->chunkLocalSize *
                                           m_blockWorld->chunkLocalSize)}};

  processShader(m_vertexHandle, m_vertexShaderModule);
  processShader(m_fragmentHandle, m_fragmentShaderModule);
  processShader(m_computeHandle, m_drawCallGenerationComputeModule, defines);

  if (anyUpdated) {
    recreatePipeline();
    std::cout << "Succesfully recompiled shaders and recreated the pipeline "
                 "for the block rendering module.\n";
  }
}
}  // namespace dnm
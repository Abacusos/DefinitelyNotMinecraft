#include <RenderingModule/BlockRenderingModule.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <Core/GLMInclude.hpp>
#include <Core/Profiler.hpp>
#include <Core/StringInterner.hpp>
#include <Logic/Camera.hpp>
#include <Shader/ShaderManager.hpp>

namespace dnm {
namespace {

constexpr std::string_view vertexShader = "Shaders/World.vert";
constexpr std::string_view fragmentShader = "Shaders/World.frag";
constexpr std::string_view computeShader =
    "Shaders/DrawCallGenerationWorld.comp";

constexpr std::string_view localWGSizeX = "LOCAL_SIZE_X";
constexpr std::string_view localWGSizeY = "LOCAL_SIZE_Y";
constexpr std::string_view localWGSizeZ = "LOCAL_SIZE_Z";

struct alignas(16) Plane {
  v3 normal;
  float distance;
};

struct alignas(16) CullingData {
  Plane frustum[6];
  u32 cullingEnabled;
};
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

  // Preventing the last few mipmaps due to artifacts in the distance where the
  // colors are mixed to gray then
  const u32 mipLevels =
      static_cast<u32>(std::floor(std::log2(std::max(texWidth, texHeight)))) -
      3;

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

      std::array srcOffsets{vk::Offset3D{0, 0, 0},
                            vk::Offset3D{mipWidth, mipHeight, 1}};
      std::array dstOffsets{vk::Offset3D{0, 0, 0},
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

  m_drawCommandBuffer =
      BufferData(physicalDevice, device, 6 * sizeof(u32),
                 vk::BufferUsageFlagBits::eStorageBuffer |
                     vk::BufferUsageFlagBits::eIndirectBuffer);
  registerDebugMarker(device, m_drawCommandBuffer.buffer, "Drawcommand Buffer");

  recreateBlockDependentBuffers();

  m_commandBufferCompute = m_renderer->getCommandBuffer();
  registerDebugMarker(device, m_commandBufferCompute,
                      "Drawcall Generation Command Buffer");
  m_computeProfilerContext = GPUProfilerContext(
      m_renderer, *m_renderer->getComputeQueue(), m_commandBufferCompute);

  m_commandBuffer = m_renderer->getCommandBuffer();
  registerDebugMarker(device, m_commandBuffer,
                      "Block Rendering Command Buffer");
  m_renderingProfilerContext = GPUProfilerContext(
      m_renderer, *m_renderer->getGraphicsQueue(), m_commandBuffer);

  m_drawCallGenerationFinished =
      vk::raii::Semaphore(device, vk::SemaphoreCreateInfo());
  registerDebugMarker(device, m_drawCallGenerationFinished,
                      "Drawcall generation compute finished");

  m_renderingFinished = vk::raii::Semaphore(device, vk::SemaphoreCreateInfo());
  registerDebugMarker(device, m_renderingFinished, "Block rendering finished");

  m_cullingData = m_renderer->createBuffer(
      sizeof(CullingData), vk::BufferUsageFlagBits::eUniformBuffer,
      "Culling Data");

  recompileShadersIfNecessary(true);
}

void BlockRenderingModule::drawFrame(const vk::raii::Framebuffer& frameBuffer,
                                     TimeSpan dt, bool cameraMoved,
                                     Camera* camera) {
  ZoneScoped;
  const auto& device = m_renderer->getDevice();

  recompileShadersIfNecessary();

  if (loadCountChunksLastFrame != m_config->loadCountChunks) {
    recreateBlockDependentBuffers();
    recreatePipeline();
  }

  const u32 oneDimensionChunkCount = 1 + m_config->loadCountChunks * 2u;
  const u32 workGroupCount = oneDimensionChunkCount * oneDimensionChunkCount;

  const bool requiresChunkDataUpdate =
      updateBlockWorldData(camera->getPosition());

  const auto extent = m_renderer->getExtent();
  const bool submitCompute = m_config->everyFrameGenerateDrawCalls ||
                             cameraMoved || requiresChunkDataUpdate;
  if (submitCompute) {
    updateCullingData(camera);

    std::array<const u32, 6> empty{0u, 1u, 0u, 0u, 0u, 0u};
    copyToDevice<u32>(m_drawCommandBuffer.deviceMemory, std::span(empty));

    m_commandBufferCompute.reset();
    {
      m_commandBufferCompute.begin(vk::CommandBufferBeginInfo());
      TracyVkZone(m_computeProfilerContext.context, *m_commandBufferCompute,
                  "Generate Draw Calls");
      TracyVkCollect(m_computeProfilerContext.context, *m_commandBufferCompute);

      m_commandBufferCompute.bindPipeline(vk::PipelineBindPoint::eCompute,
                                          *m_computePipeline);
      m_commandBufferCompute.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                                *m_pipelineLayout, 0u,
                                                {*m_descriptorSet}, nullptr);
      m_commandBufferCompute.dispatch(workGroupCount, BlockWorld::chunkHeight,
                                      1);
    }
    m_commandBufferCompute.end();

    const auto computeQueue = m_renderer->getComputeQueue();
    const vk::SubmitInfo computeInfo{
        {}, {}, *m_commandBufferCompute, *m_drawCallGenerationFinished};
    computeQueue.submit(computeInfo);
  }

  {
    std::array<vk::ClearValue, 2> clearValues;
    clearValues[0].color = vk::ClearColorValue(0.2f, 0.2f, 0.2f, 0.2f);
    clearValues[1].depthStencil = vk::ClearDepthStencilValue(0.0f, 0);

    const auto& pass = m_renderer->getRenderPass();
    const vk::RenderPassBeginInfo renderPassBeginInfo(
        *pass, *frameBuffer, vk::Rect2D(vk::Offset2D(0, 0), extent),
        clearValues);

    m_commandBuffer.reset();
    {
      m_commandBuffer.begin(vk::CommandBufferBeginInfo());
      TracyVkZone(m_renderingProfilerContext.context, *m_commandBuffer,
                  "Draw Blocks");
      TracyVkCollect(m_renderingProfilerContext.context, *m_commandBuffer);

      m_commandBuffer.beginRenderPass(renderPassBeginInfo,
                                      vk::SubpassContents::eInline);
      m_commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                   *m_graphicsPipeline);
      m_commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                         *m_pipelineLayout, 0,
                                         {*m_descriptorSet}, nullptr);

      m_commandBuffer.setViewport(
          0, vk::Viewport(0.0f, 0.0f, static_cast<float>(extent.width),
                          static_cast<float>(extent.height), 1.0f, 0.0f));
      m_commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), extent));
      m_commandBuffer.drawIndirect(*m_drawCommandBuffer.buffer, 0u, 1u,
                                   sizeof(u32) * 4);

      m_commandBuffer.endRenderPass();
    }
    m_commandBuffer.end();

    const auto graphicsQueue = m_renderer->getGraphicsQueue();

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

  std::array layout{

      // projection
      std::tuple{
          vk::DescriptorType::eUniformBuffer, 1u,
          vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eCompute},
      // view
      std::tuple{
          vk::DescriptorType::eUniformBuffer, 1u,
          vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eCompute},
      // transform
      std::tuple{
          vk::DescriptorType::eStorageBuffer, 1u,
          vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eCompute},
      // block type
      std::tuple{
          vk::DescriptorType::eStorageBuffer, 1u,
          vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eCompute},
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
      // remap index
      std::tuple<vk::DescriptorType, u32, vk::ShaderStageFlags>{
          vk::DescriptorType::eStorageBuffer, 1u,
          vk::ShaderStageFlagBits::eCompute},
      // culling data
      std::tuple<vk::DescriptorType, u32, vk::ShaderStageFlags>{
          vk::DescriptorType::eUniformBuffer, 1u,
          vk::ShaderStageFlagBits::eCompute},
      // texture sampler
      std::tuple<vk::DescriptorType, u32, vk::ShaderStageFlags>{
          vk::DescriptorType::eCombinedImageSampler, 1,
          vk::ShaderStageFlagBits::eFragment}};

  std::array sizes{
      vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer, 4u},
      vk::DescriptorPoolSize{vk::DescriptorType::eStorageBuffer, 5u},
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
      nullptr, m_fragmentShaderModule, nullptr, 0, {},
      vk::FrontFace::eCounterClockwise, true, m_pipelineLayout,
      m_renderer->getRenderPass());
  registerDebugMarker(device, m_graphicsPipeline,
                      "Block Rendering Graphics Pipeline");

  m_computePipeline = makeComputePipeline(
      device, m_renderer->getPipelineCache(), m_drawCallGenerationComputeModule,
      nullptr, m_pipelineLayout);
  registerDebugMarker(device, m_computePipeline,
                      "Draw Call Generation Compute Pipeline");

  const auto* projectionClipBuffer =
      m_renderer->getGlobalBuffer(GlobalBuffers::ProjectionClip);
  assert(projectionClipBuffer);
  const auto* viewBuffer =
      m_renderer->getGlobalBuffer(GlobalBuffers::CameraView);
  assert(viewBuffer);

  std::array update{std::tuple<vk::DescriptorType, const vk::raii::Buffer&,
                               vk::DeviceSize, const vk::raii::BufferView*>{
                        vk::DescriptorType::eUniformBuffer,
                        *projectionClipBuffer, VK_WHOLE_SIZE, nullptr},
                    std::tuple<vk::DescriptorType, const vk::raii::Buffer&,
                               vk::DeviceSize, const vk::raii::BufferView*>{
                        vk::DescriptorType::eUniformBuffer, *viewBuffer,
                        VK_WHOLE_SIZE, nullptr},
                    std::tuple<vk::DescriptorType, const vk::raii::Buffer&,
                               vk::DeviceSize, const vk::raii::BufferView*>{
                        vk::DescriptorType::eStorageBuffer,
                        m_transformBuffer.buffer, VK_WHOLE_SIZE, nullptr},
                    std::tuple<vk::DescriptorType, const vk::raii::Buffer&,
                               vk::DeviceSize, const vk::raii::BufferView*>{
                        vk::DescriptorType::eStorageBuffer,
                        m_blockTypeBuffer.buffer, VK_WHOLE_SIZE, nullptr},
                    std::tuple<vk::DescriptorType, const vk::raii::Buffer&,
                               vk::DeviceSize, const vk::raii::BufferView*>{
                        vk::DescriptorType::eStorageBuffer,
                        m_worldDataBuffer.buffer, VK_WHOLE_SIZE, nullptr},
                    std::tuple<vk::DescriptorType, const vk::raii::Buffer&,
                               vk::DeviceSize, const vk::raii::BufferView*>{
                        vk::DescriptorType::eStorageBuffer,
                        m_drawCommandBuffer.buffer, VK_WHOLE_SIZE, nullptr},
                    std::tuple<vk::DescriptorType, const vk::raii::Buffer&,
                               vk::DeviceSize, const vk::raii::BufferView*>{
                        vk::DescriptorType::eUniformBuffer,
                        m_chunkConstantsBuffer.buffer, VK_WHOLE_SIZE, nullptr},
                    std::tuple<vk::DescriptorType, const vk::raii::Buffer&,
                               vk::DeviceSize, const vk::raii::BufferView*>{
                        vk::DescriptorType::eStorageBuffer,
                        m_chunkRemapIndex.buffer, VK_WHOLE_SIZE, nullptr},
                    std::tuple<vk::DescriptorType, const vk::raii::Buffer&,
                               vk::DeviceSize, const vk::raii::BufferView*>{
                        vk::DescriptorType::eUniformBuffer,
                        m_cullingData.buffer, VK_WHOLE_SIZE, nullptr}};

  updateDescriptorSets(device, m_descriptorSet, update, {m_textureData});
}

void BlockRenderingModule::recompileShadersIfNecessary(bool force) {
  const auto& device = m_renderer->getDevice();
  auto& physicalDevice = m_renderer->getPhysicalDevice();
  auto prop = physicalDevice.getProperties();

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
                            std::to_string(m_blockWorld->chunkLocalSize)},
      ShaderManager::Define{m_interner->addOrGetString(localWGSizeY),
                            std::to_string(1)},
      ShaderManager::Define{m_interner->addOrGetString(localWGSizeZ),
                            std::to_string(m_blockWorld->chunkLocalSize)}};

  processShader(m_vertexHandle, m_vertexShaderModule);
  processShader(m_fragmentHandle, m_fragmentShaderModule);
  processShader(m_computeHandle, m_drawCallGenerationComputeModule, defines);

  if (anyUpdated) {
    recreatePipeline();
    std::cout << "Successfully recompiled shaders and recreated the pipeline "
                 "for the block rendering module.\n";
  }
}
void BlockRenderingModule::recreateBlockDependentBuffers() {
  const u32 oneDimensionChunkCount = 1 + m_config->loadCountChunks * 2u;
  const u32 blockCountAllLoadedChunks = BlockWorld::perChunkBlockCount *
                                        oneDimensionChunkCount *
                                        oneDimensionChunkCount;

  m_transformBuffer = m_renderer->createBuffer(
      blockCountAllLoadedChunks * sizeof(v4),
      vk::BufferUsageFlagBits::eStorageBuffer, "Instance Transform Buffer",
      vk::MemoryPropertyFlagBits::eDeviceLocal);

  m_worldDataBuffer = m_renderer->createBuffer(
      blockCountAllLoadedChunks * sizeof(u8),
      vk::BufferUsageFlagBits::eStorageBuffer, "World Data Blocks",
      vk::MemoryPropertyFlagBits::eDeviceLocal |
          vk::MemoryPropertyFlagBits::eHostVisible |
          vk::MemoryPropertyFlagBits::eHostCoherent);

  m_blockTypeBuffer = m_renderer->createBuffer(
      static_cast<u64>(blockCountAllLoadedChunks) * (sizeof(u32) * 2u) * 6u,
      vk::BufferUsageFlagBits::eStorageBuffer, "Block Data For Draw Command",
      vk::MemoryPropertyFlagBits::eDeviceLocal);

  std::array<u32, 4u> constants{
      m_config->loadCountChunks, oneDimensionChunkCount,
      BlockWorld::chunkLocalSize, BlockWorld::chunkHeight};

  m_chunkConstantsBuffer = m_renderer->createBuffer(
      sizeof(u32) * 4, vk::BufferUsageFlagBits::eUniformBuffer,
      "Chunk Constants",
      vk::MemoryPropertyFlagBits::eDeviceLocal |
          vk::MemoryPropertyFlagBits::eHostVisible);

  copyToDevice(m_chunkConstantsBuffer.deviceMemory,
               std::span<const u32>(constants));

  m_chunkRemapIndex = m_renderer->createBuffer(
      oneDimensionChunkCount * oneDimensionChunkCount * sizeof(u32),
      vk::BufferUsageFlagBits::eStorageBuffer, "Chunk Remap Index");

  loadCountChunksLastFrame = m_config->loadCountChunks;
}

bool BlockRenderingModule::updateBlockWorldData(v3 cameraPosition) {
  ZoneScoped;
  const u32 oneDimensionChunkCount = 1 + m_config->loadCountChunks * 2u;
  u32 workGroupCount = oneDimensionChunkCount * oneDimensionChunkCount;

  const glm::ivec2 cameraChunk{cameraPosition.x / BlockWorld::chunkLocalSize,
                               cameraPosition.z / BlockWorld::chunkLocalSize};

  const glm::ivec2 min{cameraChunk.x - m_config->loadCountChunks,
                       cameraChunk.y - m_config->loadCountChunks};
  const glm::ivec2 max{cameraChunk.x + m_config->loadCountChunks,
                       cameraChunk.y + m_config->loadCountChunks};

  bool chunkDirty = false;
  for (i32 z = min.y; z <= max.y; ++z) {
    for (i32 x = min.x; x <= max.x; ++x) {
      const glm::ivec2 chunk{x, z};
      if (m_blockWorld->isRenderingDirty(chunk)) {
        chunkDirty = true;
        m_blockWorld->clearRenderingDirty(chunk);
      }
    }
  }

  const bool requiresChunkDataUpdate = cameraChunk != m_cameraChunkLastFrame ||
                                       !m_allChunksUploadedLastFrame ||
                                       chunkDirty;

  if (requiresChunkDataUpdate) {
    m_allChunksUploadedLastFrame = true;
    m_cameraChunkLastFrame = cameraChunk;

    std::vector<u32> remapIndex;
    remapIndex.reserve(workGroupCount);
    std::vector blockData(BlockWorld::perChunkBlockCount * workGroupCount,
                          BlockWorld::air);

    u32 counter = 0u;
    for (i32 z = min.y; z <= max.y; ++z) {
      for (i32 x = min.x; x <= max.x; ++x) {
        const glm::ivec2 chunk{x, z};
        const auto state = m_blockWorld->requestChunk(chunk);
        if (state != BlockWorld::ChunkState::FinishedGeneration) {
          m_allChunksUploadedLastFrame = false;
          ++counter;
          continue;
        }
        remapIndex.emplace_back(counter);
        auto data = m_blockWorld->getChunkData(chunk);
        std::copy(data.begin(), data.end(),
                  blockData.begin() + counter * BlockWorld::perChunkBlockCount);
        ++counter;
      }
    }

    copyToDevice(m_worldDataBuffer.deviceMemory,
                 std::span<const BlockType>(blockData));
    workGroupCount = remapIndex.size();
    if (workGroupCount > 0) {
      copyToDevice(m_chunkRemapIndex.deviceMemory,
                   std::span<const u32>(remapIndex));
    }
  }

  return requiresChunkDataUpdate;
}

namespace {
Plane convertToPlane(v3 p1, v3 normal) {
  Plane p;
  p.normal = glm::normalize(normal);
  p.distance = glm::dot(p.normal, p1);
  return p;
}
}  // namespace

void BlockRenderingModule::updateCullingData(const Camera* camera) const {
  ZoneScoped;

  const float zNear = m_config->nearPlane;
  const float zFar = m_config->farPlane;

  const v3 forward = camera->getForward();
  const v3 up = camera->getUp();
  const v3 right = camera->getRight();

  constexpr float fov = glm::radians(60.0f);
  constexpr float aspect = 1920 / 1017;

  const float halfVSide = zFar * tanf(fov);
  const float halfHSide = halfVSide * aspect;
  const v3 frontMultFar = zFar * forward;

  CullingData data;
  data.cullingEnabled = m_config->cullingEnabled;

  const v3 cameraPosition = camera->getPosition();
  data.frustum[0] = convertToPlane(cameraPosition + zNear * forward, forward);
  data.frustum[1] = convertToPlane(cameraPosition + frontMultFar, -forward);
  data.frustum[2] = convertToPlane(
      cameraPosition, glm::cross(frontMultFar - right * halfHSide, up));
  data.frustum[3] = convertToPlane(
      cameraPosition, glm::cross(up, frontMultFar + right * halfHSide));
  data.frustum[4] = convertToPlane(
      cameraPosition, glm::cross(right, frontMultFar - up * halfVSide));
  data.frustum[5] = convertToPlane(
      cameraPosition, glm::cross(frontMultFar + up * halfVSide, right));

  copyToDevice(m_cullingData.deviceMemory,
               std::span<const CullingData>(&data, 1u));
}
}  // namespace dnm
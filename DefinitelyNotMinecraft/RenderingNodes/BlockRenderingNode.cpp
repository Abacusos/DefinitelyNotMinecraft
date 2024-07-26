#include "RenderingNodes/BlockRenderingNode.hpp"

#include <Core/GLMInclude.hpp>
#include <Core/Profiler.hpp>
#include <Core/StringInterner.hpp>

#include <Logic/Camera.hpp>

#include <Shader/ShaderManager.hpp>

namespace dnm
{
namespace
{
    constexpr std::string_view computeShader = "Shaders/DrawCallGenerationWorld.comp";

    constexpr std::string_view localWGSizeX = "LOCAL_SIZE_X";
    constexpr std::string_view localWGSizeY = "LOCAL_SIZE_Y";
    constexpr std::string_view localWGSizeZ = "LOCAL_SIZE_Z";

    constexpr std::string_view worldDataBindingPoint      = "worldDataBuffer";
    constexpr std::string_view chunkConstantsBindingPoint = "chunkConstants";
    constexpr std::string_view chunkRemapBindingPoint     = "chunkIndexRemap";
    constexpr std::string_view cullingBindingPoint        = "cullingData";

    struct alignas(16) Plane
    {
        v3    normal;
        float distance;
    };

    struct alignas(16) CullingData
    {
        Plane frustum [6];
        u32   cullingEnabled;
    };
}   // namespace

BlockDrawCallNode::BlockDrawCallNode(Config* config, Renderer* renderer, ShaderManager* shaderManager, BlockWorld* blockWorld, StringInterner* interner) :
    m_config {config}, m_renderer {renderer}, m_shaderManager {shaderManager}, m_blockWorld {blockWorld}, m_interner {interner} {
    const auto& physicalDevice = m_renderer->getPhysicalDevice();
    const auto& device         = m_renderer->getDevice();

    m_computeHandle = m_shaderManager->registerShaderFile(m_interner->addOrGetString(computeShader), vk::ShaderStageFlagBits::eCompute);

    m_drawCommandBuffer =
      BufferData(physicalDevice, device, 6 * sizeof(u32), vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer);
    registerDebugMarker(device, m_drawCommandBuffer.buffer, "Drawcommand Buffer");
    m_drawCommandRegistration = renderer->registerRAIIBuffer(GlobalBuffers::DrawCommand, m_drawCommandBuffer);

    recreateBlockDependentBuffers();

    m_computeProfilerContext = GPUProfilerContext(m_renderer);

    m_cullingData = m_renderer->createBuffer(sizeof(CullingData), vk::BufferUsageFlagBits::eUniformBuffer, "Culling Data");

    recompileShadersIfNecessary(true);
}

std::string_view BlockDrawCallNode::getName() const {
    return "BlockDrawCallNode";
}

void BlockDrawCallNode::recreatePipeline() {
    m_renderer->waitIdle();

    const auto& device = m_renderer->getDevice();

    m_descriptorSet.clear();

    std::vector<BindingSlot> slots;
    vk::ShaderStageFlags stageFlags;
    auto internedString = m_interner->addOrGetString(computeShader);
    m_shaderManager->getBindingSlots(std::span{&internedString, 1u}, slots, stageFlags);
    m_descriptorSetLayout = makeDescriptorSetLayout(device, slots, stageFlags);
    m_pipelineLayout      = vk::raii::PipelineLayout(device, {{}, *m_descriptorSetLayout});

    auto sets       = vk::raii::DescriptorSets(device, {*m_renderer->getDescriptorPool(), *m_descriptorSetLayout});
    m_descriptorSet = std::move(sets.front());

    m_computePipeline = makeComputePipeline(device, m_renderer->getPipelineCache(), m_drawCallGenerationComputeModule, nullptr, m_pipelineLayout);
    registerDebugMarker(device, m_computePipeline, "Draw Call Generation Compute Pipeline");

    const auto* projectionClipBuffer = m_renderer->getGlobalBuffer(GlobalBuffers::ProjectionClip);
    assert(projectionClipBuffer);
    const auto* viewBuffer = m_renderer->getGlobalBuffer(GlobalBuffers::CameraView);
    assert(viewBuffer);

    std::array update {
      DescriptorSlotUpdate {projectionBufferBindingPoint,         *projectionClipBuffer, VK_WHOLE_SIZE, nullptr},
      DescriptorSlotUpdate {      viewBufferBindingPoint,                   *viewBuffer, VK_WHOLE_SIZE, nullptr},
      DescriptorSlotUpdate {       transformBindingPoint,      m_transformBuffer.buffer, VK_WHOLE_SIZE, nullptr},
      DescriptorSlotUpdate {       blockTypeBindingPoint,      m_blockTypeBuffer.buffer, VK_WHOLE_SIZE, nullptr},
      DescriptorSlotUpdate {       worldDataBindingPoint,      m_worldDataBuffer.buffer, VK_WHOLE_SIZE, nullptr},
      DescriptorSlotUpdate {     drawCommandBindingPoint,    m_drawCommandBuffer.buffer, VK_WHOLE_SIZE, nullptr},
      DescriptorSlotUpdate {  chunkConstantsBindingPoint, m_chunkConstantsBuffer.buffer, VK_WHOLE_SIZE, nullptr},
      DescriptorSlotUpdate {      chunkRemapBindingPoint,      m_chunkRemapIndex.buffer, VK_WHOLE_SIZE, nullptr},
      DescriptorSlotUpdate {         cullingBindingPoint,          m_cullingData.buffer, VK_WHOLE_SIZE, nullptr}
    };

    updateDescriptorSets(device, m_descriptorSet, update, slots);
}

void BlockDrawCallNode::recompileShadersIfNecessary(bool force) {
    const auto& device = m_renderer->getDevice();

    std::array defines = {
      ShaderManager::Define {m_interner->addOrGetString(localWGSizeX), std::to_string(m_blockWorld->chunkLocalSize)},
      ShaderManager::Define {m_interner->addOrGetString(localWGSizeY),                            std::to_string(1)},
      ShaderManager::Define {m_interner->addOrGetString(localWGSizeZ), std::to_string(m_blockWorld->chunkLocalSize)}
    };

    if (m_shaderManager->wasContentUpdated(m_computeHandle) || force) {
        auto recompiledVertexShader = m_shaderManager->getCompiledVersion(device, m_computeHandle, defines);
        if (recompiledVertexShader) {
            m_drawCallGenerationComputeModule = std::move(recompiledVertexShader.value());

            recreatePipeline();
            std::cout << "Successfully recompiled shaders and recreated the pipeline "
                         "for the block rendering module.\n";
        }
    }
}

void BlockDrawCallNode::recreateBlockDependentBuffers() {
    const u32 oneDimensionChunkCount    = 1 + m_config->loadCountChunks * 2u;
    const u32 blockCountAllLoadedChunks = BlockWorld::perChunkBlockCount * oneDimensionChunkCount * oneDimensionChunkCount;

    m_transformBuffer = m_renderer->createBuffer(
      blockCountAllLoadedChunks * sizeof(v4), vk::BufferUsageFlagBits::eStorageBuffer, "Instance Transform Buffer", vk::MemoryPropertyFlagBits::eDeviceLocal);

    m_transformRegistration = m_renderer->registerRAIIBuffer(GlobalBuffers::Transform, m_transformBuffer);

    m_worldDataBuffer = m_renderer->createBuffer(
      blockCountAllLoadedChunks * sizeof(BlockType),
      vk::BufferUsageFlagBits::eStorageBuffer,
      "World Data Blocks",
      vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible);

    m_blockTypeBuffer = m_renderer->createBuffer(
      static_cast<u64>(blockCountAllLoadedChunks) * (sizeof(u32) * 2u) * 6u,
      vk::BufferUsageFlagBits::eStorageBuffer,
      "Block Data For Draw Command",
      vk::MemoryPropertyFlagBits::eDeviceLocal);

    m_blockTypeRegistration = m_renderer->registerRAIIBuffer(GlobalBuffers::BlockType, m_blockTypeBuffer);

    std::array<u32, 4u> constants {m_config->loadCountChunks, oneDimensionChunkCount, BlockWorld::chunkLocalSize, BlockWorld::chunkHeight};

    m_chunkConstantsBuffer = m_renderer->createBuffer(
      sizeof(u32) * 4,
      vk::BufferUsageFlagBits::eUniformBuffer,
      "Chunk Constants",
      vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible);

    copyToDevice(m_chunkConstantsBuffer.deviceMemory, std::span<const u32>(constants));

    m_chunkRemapIndex =
      m_renderer->createBuffer(oneDimensionChunkCount * oneDimensionChunkCount * sizeof(u32), vk::BufferUsageFlagBits::eStorageBuffer, "Chunk Remap Index");

    loadCountChunksLastFrame = m_config->loadCountChunks;
}

bool BlockDrawCallNode::updateBlockWorldData(v3 cameraPosition) {
    ZoneScoped;
    const u32 oneDimensionChunkCount = 1 + m_config->loadCountChunks * 2u;
    u32       workGroupCount         = oneDimensionChunkCount * oneDimensionChunkCount;

    const glm::ivec2 cameraChunk {cameraPosition.x / BlockWorld::chunkLocalSize, cameraPosition.z / BlockWorld::chunkLocalSize};

    const glm::ivec2 min {cameraChunk.x - m_config->loadCountChunks, cameraChunk.y - m_config->loadCountChunks};
    const glm::ivec2 max {cameraChunk.x + m_config->loadCountChunks, cameraChunk.y + m_config->loadCountChunks};

    bool chunkDirty = false;
    for (i32 z = min.y; z <= max.y; ++z) {
        for (i32 x = min.x; x <= max.x; ++x) {
            const glm::ivec2 chunk {x, z};
            if (m_blockWorld->isRenderingDirty(chunk)) {
                chunkDirty = true;
            }
        }
    }

    const bool requiresChunkDataUpdate =
      cameraChunk != m_cameraChunkLastFrame || !m_allChunksUploadedLastFrame || chunkDirty || m_config->everyFrameGenerateDrawCalls;

    if (requiresChunkDataUpdate) {
        m_allChunksUploadedLastFrame = true;
        m_cameraChunkLastFrame       = cameraChunk;

        std::vector<u32> remapIndex;
        remapIndex.reserve(workGroupCount);
        std::vector blockData(BlockWorld::perChunkBlockCount * workGroupCount, BlockWorld::air);

        u32 counter = 0u;
        for (i32 z = min.y; z <= max.y; ++z) {
            for (i32 x = min.x; x <= max.x; ++x) {
                const glm::ivec2 chunk {x, z};
                const auto       state = m_blockWorld->requestChunk(chunk);
                if (state != BlockWorld::ChunkState::FinishedGeneration) {
                    m_allChunksUploadedLastFrame = false;
                    ++counter;
                    continue;
                }
                remapIndex.emplace_back(counter);
                auto data = m_blockWorld->getChunkData(chunk);
                std::copy(data.begin(), data.end(), blockData.begin() + counter * BlockWorld::perChunkBlockCount);
                ++counter;
            }
        }

        copyToDevice(m_worldDataBuffer.deviceMemory, std::span<const BlockType>(blockData));
        workGroupCount = remapIndex.size();
        if (workGroupCount > 0) {
            copyToDevice(m_chunkRemapIndex.deviceMemory, std::span<const u32>(remapIndex));
        }
    }

    return requiresChunkDataUpdate;
}

namespace
{
    Plane convertToPlane(v3 p1, v3 normal) {
        Plane p;
        p.normal   = glm::normalize(normal);
        p.distance = glm::dot(p.normal, p1);
        return p;
    }
}   // namespace

void BlockDrawCallNode::updateCullingData(const Camera* camera) const {
    ZoneScoped;

    const float zNear = m_config->nearPlane;
    const float zFar  = m_config->farPlane;

    const v3 forward = camera->getForward();
    const v3 up      = camera->getUp();
    const v3 right   = camera->getRight();

    constexpr float fov    = glm::radians(60.0f);
    constexpr float aspect = 1920 / 1017;

    const float halfVSide    = zFar * tanf(fov);
    const float halfHSide    = halfVSide * aspect;
    const v3    frontMultFar = zFar * forward;

    CullingData data;
    data.cullingEnabled = m_config->cullingEnabled;

    const v3 cameraPosition = camera->getPosition();
    data.frustum [0]        = convertToPlane(cameraPosition + zNear * forward, forward);
    data.frustum [1]        = convertToPlane(cameraPosition + frontMultFar, -forward);
    data.frustum [2]        = convertToPlane(cameraPosition, glm::cross(frontMultFar - right * halfHSide, up));
    data.frustum [3]        = convertToPlane(cameraPosition, glm::cross(up, frontMultFar + right * halfHSide));
    data.frustum [4]        = convertToPlane(cameraPosition, glm::cross(right, frontMultFar - up * halfVSide));
    data.frustum [5]        = convertToPlane(cameraPosition, glm::cross(frontMultFar + up * halfVSide, right));

    copyToDevice(m_cullingData.deviceMemory, std::span<const CullingData>(&data, 1u));
}

bool BlockDrawCallNode::shouldExecute() const {
    return true;
}

IRenderingNode::ExecutionResult BlockDrawCallNode::execute(const ExecutionData& executionData, vk::raii::CommandBuffer& commandBuffer) {
    ZoneScoped;

    recompileShadersIfNecessary();

    if (loadCountChunksLastFrame != m_config->loadCountChunks) {
        recreateBlockDependentBuffers();
        recreatePipeline();
    }

    const u32 oneDimensionChunkCount = 1 + m_config->loadCountChunks * 2u;
    const u32 workGroupCount         = oneDimensionChunkCount * oneDimensionChunkCount;

    updateBlockWorldData(executionData.camera->getPosition());
    updateCullingData(executionData.camera);

    std::array<const u32, 6> empty {0u, 1u, 0u, 0u, 0u, 0u};
    copyToDevice<u32>(m_drawCommandBuffer.deviceMemory, std::span(empty));

    {
        commandBuffer.begin(vk::CommandBufferBeginInfo());
        TracyVkZone(m_computeProfilerContext.context, *commandBuffer, "Generate Draw Calls");
        TracyVkCollect(m_computeProfilerContext.context, *commandBuffer);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, *m_computePipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *m_pipelineLayout, 0u, {*m_descriptorSet}, nullptr);
        commandBuffer.dispatch(workGroupCount, BlockWorld::chunkHeight, 1);
    }
    commandBuffer.end();

    return ExecutionResult {{}, m_renderer->getComputeQueue()};
}
}   // namespace dnm

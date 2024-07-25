#pragma once

#include <array>
#include <future>
#include <optional>
#include <span>

#include <unordered_map>

#define GLM_ENABLE_EXPERIMENTAL
#include <Core/GLMInclude.hpp>
#include <Core/ShortTypes.hpp>

#include "PerlinNoise.hpp"
#include "glm/gtx/hash.hpp"

namespace dnm
{
struct Config;
using BlockType = u16;
class Camera;

class BlockWorld {
    public:
    explicit BlockWorld(Config* config);

    enum class ChunkState
    {
        Created,
        InProgress,
        FinishedGeneration,
        RequiresOuterVisibilityUpdate,
        RequiresFullVisibilityUpdate,
    };

    ChunkState                 requestChunk(glm::ivec2 chunkPosition);
    bool                       isRenderingDirty(glm::ivec2 chunkPosition) const;
    std::span<const BlockType> getChunkData(glm::ivec2 chunkPosition) const;

    enum class BlockAction
    {
        Add,
        Destroy
    };

    struct BlockPosition
    {
        glm::ivec2 chunkIndex;
        glm::ivec3 positionWithinChunk;
        bool       blockExists = false;
    };

    void                         modifyFirstTracedBlock(const std::optional<BlockPosition>& potentialTarget);
    std::optional<BlockPosition> getFirstTracedBlock(Camera* camera);
    void                         updateBlock(const BlockPosition& position, BlockType type);

    constexpr static u64       chunkLocalSize     = 32u;
    constexpr static u64       chunkHeight        = 128u;
    constexpr static u64       perChunkBlockCount = (chunkLocalSize * chunkLocalSize * chunkHeight);
    constexpr static BlockType air                = BlockType(65535u);

    private:
    Config* m_config;

    std::vector<BlockType> m_blockTypesWorld;

    std::jthread m_generationThread;

    std::mutex m_workInsertionLock;

    struct GenerationData
    {
        glm::ivec2                                                          position;
        std::span<BlockType, chunkLocalSize * chunkLocalSize * chunkHeight> data;
        std::atomic<bool>*                                                  finished;
    };

    std::vector<GenerationData> m_chunkPositionsQueue;

    struct Chunk
    {
        std::array<BlockType, chunkLocalSize * chunkLocalSize * chunkHeight> data;
        std::atomic<bool>                                                    finished;
        ChunkState                                                           state = ChunkState::Created;
    };

    // The usage of this one may look a bit strange in the cpp file but there is
    // only a problem with inserting and calling find at the same time. The actual
    // access to data only happens from one thread at a time.
    // A better solution would be to have read and write locks, so we can specify
    // this beforehand because all read access can happen concurrently.
    mutable std::mutex                    m_chunkDataMutex;
    std::unordered_map<glm::ivec2, Chunk> m_chunkData;

    std::optional<BlockPosition> getBlockPosition(v3 position);
    // The method will potentially modify the chunk index if necessary
    // and thus allow cross chunk selection.
    BlockPosition                getPositionWithOffset(const BlockPosition& position, i32 x, i32 y, i32 z);
    void                         updateVisibilityBit(const BlockPosition& position, std::span<BlockType> positionChunkData);
    void                         triggerVisibilityUpdateOnNeighbors(const BlockPosition& position);

    // This should be presumably threadsafe as long as the noise is not reseeded
    siv::BasicPerlinNoise<float> m_noiseGrass {42};
    siv::BasicPerlinNoise<float> m_noiseCobble {84};
    siv::BasicPerlinNoise<float> m_noiseStone {126};
    siv::BasicPerlinNoise<float> m_noiseSand {168};
};
}   // namespace dnm

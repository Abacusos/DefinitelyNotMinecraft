#pragma once

#include <array>
#include <future>
#include <optional>
#include <span>
#include <unordered_map>

#define GLM_ENABLE_EXPERIMENTAL
#include "GLMInclude.hpp"
#include "PerlinNoise.hpp"
#include "ShortTypes.hpp"
#include "glm/gtx/hash.hpp"

namespace dnm {
struct Config;
using BlockType = u8;

class BlockWorld {
 public:
  explicit BlockWorld(Config* config);

  enum class ChunkState {
    Created,
    InProgress,
    FinishedGeneration,
    DirtyRendering
  };

  ChunkState requestChunk(glm::ivec2 chunkPosition);
  bool isRenderingDirty(glm::ivec2 chunkPosition) const;
  void clearRenderingDirty(glm::ivec2 chunkPosition);
  std::span<const BlockType> getChunkData(glm::ivec2 chunkPosition) const;

  enum class BlockAction { Add, Destroy };
  struct BlockPosition {
    glm::ivec2 chunkIndex;
    glm::ivec3 positionWithinChunk;
    bool blockExists = false;
  };

  void modifyFirstTracedBlock(
      const std::optional<BlockWorld::BlockPosition>& potentialTarget);
  std::optional<BlockPosition> getFirstTracedBlock(glm::vec3 position,
                                                   glm::vec3 rotation);
  void updateBlock(const BlockPosition& position, BlockType type);

  constexpr static u64 chunkLocalSize = 32u;
  constexpr static u64 chunkHeight = 128u;
  constexpr static u64 perChunkBlockCount =
      (chunkLocalSize * chunkLocalSize * chunkHeight);
  constexpr static BlockType air = BlockType(255);

 private:
  Config* m_config;

  std::vector<BlockType> m_blockTypesWorld;
  struct Chunk {
    std::array<BlockType, chunkLocalSize * chunkLocalSize * chunkHeight> data;
    std::future<void> loadTask;
    ChunkState state = ChunkState::Created;
  };
  // The usage of this one may looks a bit strange in the cpp file but there is
  // only a problem with inserting and calling find at the same time. The actual
  // access to data only happens from one thread at a time.
  // A better solution would be to have read and write locks so we can specify
  // this beforehand because all read access can happen concurrently.
  mutable std::mutex m_chunkDataMutex;
  std::unordered_map<glm::ivec2, Chunk> m_chunkData;

  std::optional<BlockPosition> getBlockPosition(glm::vec3 position);
  // The method will potentially modify the chunk index if necessary
  // and thus allow cross chunk selection.
  BlockPosition getPositionWithOffset(const BlockPosition& position, i32 x,
                                      i32 y, i32 z);
  void updateVisibilityBit(const BlockPosition& position,
                           std::span<BlockType> positionChunkData);

  // This should be presumably threadsafe as long as the noise is not reseeded
  siv::BasicPerlinNoise<float> m_noiseGrass{42};
  siv::BasicPerlinNoise<float> m_noiseCobble{84};
  siv::BasicPerlinNoise<float> m_noiseStone{126};
  siv::BasicPerlinNoise<float> m_noiseSand{168};
};
}  // namespace dnm
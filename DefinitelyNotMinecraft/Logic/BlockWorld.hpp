#pragma once

#include <array>
#include <future>
#include <span>
#include <unordered_map>

#define GLM_ENABLE_EXPERIMENTAL
#include "GLMInclude.hpp"
#include "PerlinNoise.hpp"
#include "ShortTypes.hpp"
#include "glm/gtx/hash.hpp"

namespace dnm {

using BlockType = u8;

class BlockWorld {
 public:
  enum class ChunkState { Created, InProgress, Finished };
  ChunkState requestChunk(glm::ivec2 chunkPosition);
  std::span<const BlockType> getChunkData(glm::ivec2 chunkPosition) const;

  constexpr static u64 chunkLocalSize = 32u;
  constexpr static u64 chunkHeight = 128u;
  constexpr static u64 perChunkBlockCount =
      (chunkLocalSize * chunkLocalSize * chunkHeight);

 private:
  std::vector<BlockType> m_blockTypesWorld;
  struct Chunk {
    std::array<BlockType, chunkLocalSize * chunkLocalSize * chunkHeight> data;
    std::future<void> loadTask;
    ChunkState state = ChunkState::Created;
  };
  std::unordered_map<glm::ivec2, Chunk> m_chunkData;

  // This should be presumably threadsafe as long as the noise is not reseeded
  siv::BasicPerlinNoise<float> m_noiseGrass{42};
  siv::BasicPerlinNoise<float> m_noiseCobble{84};
  siv::BasicPerlinNoise<float> m_noiseStone{126};
  siv::BasicPerlinNoise<float> m_noiseSand{168};
};
}  // namespace dnm
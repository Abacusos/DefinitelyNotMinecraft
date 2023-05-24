#include "BlockWorld.hpp"

#include "PerlinNoise.hpp"

namespace dnm {

BlockWorld::BlockWorld() {

  siv::BasicPerlinNoise<float> noiseGrass(42);
  siv::BasicPerlinNoise<float> noiseCobble(84);
  siv::BasicPerlinNoise<float> noiseStone(126);
  siv::BasicPerlinNoise<float> noiseSand(168);

  auto getBlockType = [&](float x, float y, float z, bool blockAboveExists) {
    x /= 100.0f;
    y /= 100.0f;
    z /= 100.0f;
    auto grass = noiseGrass.octave3D_01(x, y, z, 6);
    auto cobbleStone = noiseCobble.octave3D_01(x, y, z, 6);
    auto stone = noiseStone.octave3D_01(x, y, z, 6);
    auto sand = noiseSand.octave3D_01(x, y, z, 6);

    BlockType result = 255;
    float currentHighestNoise = 0.6f;

    if (grass > currentHighestNoise) {
      currentHighestNoise = grass;
      result = blockAboveExists ? 4 : 0;
    }
    if (cobbleStone > currentHighestNoise) {
      currentHighestNoise = cobbleStone;
      result = 1;
    }
    if (stone > currentHighestNoise) {
      currentHighestNoise = stone;
      result = 2;
    }
    if (sand > currentHighestNoise) {
      currentHighestNoise = sand;
      result = 3;
    }

    return result;
  };

  m_blockTypesWorld.resize(totalBlockCount);
  for (auto chunkZ = 0u; chunkZ < chunkLoadCount; ++chunkZ) {
    for (auto chunkX = 0u; chunkX < chunkLoadCount; ++chunkX) {
      for (i64 localChunkY = chunkHeight-1; localChunkY >= 0;  --localChunkY) {
        for (auto localChunkZ = 0u; localChunkZ < chunkLocalSize;
             ++localChunkZ) {
          for (auto localChunkX = 0u; localChunkX < chunkLocalSize;
               ++localChunkX) {
            auto globalX = chunkX * chunkLocalSize + localChunkX;
            auto globalZ = chunkZ * chunkLocalSize + localChunkZ;

            auto sizeOfChunk = chunkLocalSize * chunkLocalSize * chunkHeight;

            auto offsetPreviousChunk =
                chunkX * sizeOfChunk + chunkZ * chunkLoadCount * sizeOfChunk;

            auto heightOffset = chunkLocalSize * chunkLocalSize * localChunkY;
            auto inLayerOffset = localChunkZ * chunkLocalSize + localChunkX;

            bool blockAboveExists = false;
            if (localChunkY != chunkHeight-1) {
              auto heightOffsetAbove =
                  chunkLocalSize * chunkLocalSize * (localChunkY + 1);
              blockAboveExists =
                  m_blockTypesWorld[offsetPreviousChunk + heightOffsetAbove +
                                    inLayerOffset] != 255;
            }

            m_blockTypesWorld[offsetPreviousChunk + heightOffset +
                              inLayerOffset] =
                getBlockType(globalX, localChunkY, globalZ, blockAboveExists);
          }
        }
      }
    }
  }
}

std::span<const BlockType> BlockWorld::getBlockTypes() const {
  return m_blockTypesWorld;
}

bool BlockWorld::wasModified() {
  bool dirty = m_dirty;
  m_dirty = false;
  return dirty;
}
}  // namespace dnm
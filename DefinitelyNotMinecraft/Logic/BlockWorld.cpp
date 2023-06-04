#include "BlockWorld.hpp"

#include <chrono>

namespace dnm {

namespace {
constexpr u8 highestBit = u8(1) << 7;
}

BlockWorld::ChunkState BlockWorld::requestChunk(glm::ivec2 chunkPosition) {
  auto& chunk = m_chunkData[chunkPosition];

  switch (chunk.state) {
    case ChunkState::Finished: {
      break;
    }
    case ChunkState::InProgress: {
      if (auto status = chunk.loadTask.wait_for(std::chrono::seconds(0));
          status == std::future_status::ready) {
        // Although this is kind of pointless
        chunk.loadTask.get();
        chunk.state = ChunkState::Finished;
      }
      break;
    }
    case ChunkState::Created: {
      // Due to pointer stability of unordered maps, this should be fine even if
      // more things are inserted
      chunk.loadTask = std::async(std::launch::async, [this, chunkPosition,
                                                       blockData =
                                                           &chunk.data]() {
        auto getBlockType = [this](float x, float y, float z,
                                   bool blockAboveExists) {
          x /= 100.0f;
          y /= 100.0f;
          z /= 100.0f;
          auto grass = m_noiseGrass.octave3D_01(x, y, z, 6);
          auto cobbleStone = m_noiseCobble.octave3D_01(x, y, z, 6);
          auto stone = m_noiseStone.octave3D_01(x, y, z, 6);
          auto sand = m_noiseSand.octave3D_01(x, y, z, 6);

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

        for (i64 localChunkY = chunkHeight - 1; localChunkY >= 0;
             --localChunkY) {
          for (auto localChunkZ = 0u; localChunkZ < chunkLocalSize;
               ++localChunkZ) {
            for (auto localChunkX = 0u; localChunkX < chunkLocalSize;
                 ++localChunkX) {
              auto globalX = chunkPosition.x * chunkLocalSize + localChunkX;
              auto globalZ = chunkPosition.y * chunkLocalSize + localChunkZ;

              auto heightOffset = chunkLocalSize * chunkLocalSize * localChunkY;
              auto inLayerOffset = localChunkZ * chunkLocalSize + localChunkX;

              bool blockAboveExists = false;
              if (localChunkY != chunkHeight - 1) {
                auto heightOffsetAbove =
                    chunkLocalSize * chunkLocalSize * (localChunkY + 1);
                blockAboveExists =
                    (*blockData)[heightOffsetAbove + inLayerOffset] != 255;
              }

              (*blockData)[heightOffset + inLayerOffset] =
                  getBlockType(globalX, localChunkY, globalZ, blockAboveExists);
            }
          }
        }

        for (i64 localChunkY = 1; localChunkY < chunkHeight - 1;
             ++localChunkY) {
          for (auto localChunkZ = 1u; localChunkZ < chunkLocalSize - 1;
               ++localChunkZ) {
            for (auto localChunkX = 1u; localChunkX < chunkLocalSize - 1;
                 ++localChunkX) {
              auto heightOffset = chunkLocalSize * chunkLocalSize * localChunkY;
              auto inLayerOffset = localChunkZ * chunkLocalSize + localChunkX;

              if ((*blockData)[heightOffset + inLayerOffset] == 255u) {
                continue;
              }

              bool allNeighborBlocksExist = true;
              for (auto y = -1; y < 2; ++y) {
                for (auto z = -1; z < 2; ++z) {
                  for (auto x = -1; x < 2; ++x) {
                    if (y == 0 && z == 0 && x == 0) {
                      continue;
                    }

                    auto heightOffsetBlock =
                        chunkLocalSize * chunkLocalSize * (localChunkY + y);
                    auto inLayerOffsetBlock =
                        (localChunkZ + z) * chunkLocalSize + (localChunkX + x);
                    if ((*blockData)[heightOffsetBlock + inLayerOffsetBlock] ==
                        255u) {
                      allNeighborBlocksExist = false;
                      break;
                    }
                  }
                }
              }

              if (!allNeighborBlocksExist) {
                continue;
              }

              (*blockData)[heightOffset + inLayerOffset] |= highestBit;
            }
          }
        }
      });
      chunk.state = ChunkState::InProgress;
      break;
    }
    default:
      assert(false);
  }

  return chunk.state;
}

std::span<const BlockType> BlockWorld::getChunkData(
    glm::ivec2 chunkPosition) const {
  auto it = m_chunkData.find(chunkPosition);
  if (it == m_chunkData.end()) {
    assert(false);
    return {};
  }

  return it->second.data;
}
}  // namespace dnm
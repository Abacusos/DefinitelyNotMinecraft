#include <Logic/BlockWorld.hpp>

#include <Logic/Camera.hpp>
#include <Core/Config.hpp>
#include <Core/Profiler.hpp>

namespace dnm {

namespace {
constexpr u8 highestBit = u8(1) << 7;

constexpr bool testWorldSetup = false;
}  // namespace

BlockWorld::BlockWorld(Config* config) : m_config{config} {}

BlockWorld::ChunkState BlockWorld::requestChunk(glm::ivec2 chunkPosition) {
  std::lock_guard g{m_chunkDataMutex};
  auto& chunk = m_chunkData[chunkPosition];

  switch (chunk.state) {
    case ChunkState::FinishedGeneration: {
      break;
    }
    case ChunkState::InProgress: {
      if (const auto status = chunk.loadTask.wait_for(std::chrono::seconds(0));
          status == std::future_status::ready) {
        // Although this is kind of pointless
        chunk.loadTask.get();
        chunk.state = ChunkState::FinishedGeneration;
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
          BlockType result = BlockWorld::air;

          if constexpr (testWorldSetup) {
            if (x > 495 && x < 505 && z == 495) {
              result = 4;
            } else if (x > 495 && x < 505 && z == 505) {
              result = 1;
            } else if (z > 495 && z < 505 && x == 505) {
              result = 2;
            } else if (z > 495 && z < 505 && x == 495) {
              result = 3;
            }
            return result;
          }

          x /= 100.0f;
          y /= 100.0f;
          z /= 100.0f;
          const float grass = m_noiseGrass.octave3D_01(x, y, z, 6);
          const float cobbleStone = m_noiseCobble.octave3D_01(x, y, z, 6);
          const float stone = m_noiseStone.octave3D_01(x, y, z, 6);
          const float sand = m_noiseSand.octave3D_01(x, y, z, 6);

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
              const i64 globalX = chunkPosition.x * chunkLocalSize + localChunkX;
              const i64 globalZ = chunkPosition.y * chunkLocalSize + localChunkZ;

              const auto heightOffset = chunkLocalSize * chunkLocalSize * localChunkY;
              const auto inLayerOffset = localChunkZ * chunkLocalSize + localChunkX;

              bool blockAboveExists = false;
              if (localChunkY != chunkHeight - 1) {
                  const auto heightOffsetAbove =
                    chunkLocalSize * chunkLocalSize * (localChunkY + 1);
                blockAboveExists =
                    (*blockData)[heightOffsetAbove + inLayerOffset] !=
                    BlockWorld::air;
              }

              (*blockData)[heightOffset + inLayerOffset] =
                  getBlockType(globalX, localChunkY, globalZ, blockAboveExists);
            }
          }
        }

        BlockPosition position{};
        position.chunkIndex = chunkPosition;

        for (i64 localChunkY = 0; localChunkY < chunkHeight; ++localChunkY) {
          for (auto localChunkZ = 0; localChunkZ < chunkLocalSize;
               ++localChunkZ) {
            for (auto localChunkX = 0; localChunkX < chunkLocalSize;
                 ++localChunkX) {
              position.positionWithinChunk = {localChunkX, localChunkY,
                                              localChunkZ};
              std::span data = *blockData;
              updateVisibilityBit(position, data);
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

bool BlockWorld::isRenderingDirty(glm::ivec2 chunkPosition) const {
  std::lock_guard g{m_chunkDataMutex};
  const auto it = m_chunkData.find(chunkPosition);
  if (it == m_chunkData.end()) {
    return false;
  }

  return it->second.state == ChunkState::DirtyRendering;
}

void BlockWorld::clearRenderingDirty(glm::ivec2 chunkPosition) {
  std::lock_guard g{m_chunkDataMutex};
  const auto it = m_chunkData.find(chunkPosition);
  if (it == m_chunkData.end()) {
    assert(false);
    return;
  }

  assert(it->second.state == ChunkState::DirtyRendering);
  it->second.state = ChunkState::FinishedGeneration;
}

std::span<const BlockType> BlockWorld::getChunkData(
    glm::ivec2 chunkPosition) const {
  std::lock_guard g{m_chunkDataMutex};
  const auto it = m_chunkData.find(chunkPosition);
  if (it == m_chunkData.end()) {
    assert(false);
    return {};
  }

  return it->second.data;
}

void BlockWorld::modifyFirstTracedBlock(
    const std::optional<BlockWorld::BlockPosition>& potentialTarget) {
    const auto action = static_cast<BlockWorld::BlockAction>(m_config->insertionMode);
  switch (action) {
    case BlockAction::Add: {
      if (potentialTarget) {
        updateBlock(potentialTarget.value(), 1);
      }
    } break;

    case BlockAction::Destroy: {
      if (potentialTarget && potentialTarget->blockExists) {
        updateBlock(potentialTarget.value(), BlockWorld::air);
        break;
      }

    } break;
    default:
      assert(false);
  }
}

std::optional<BlockWorld::BlockPosition> BlockWorld::getFirstTracedBlock(
    Camera* camera) {
  v3 position = camera->getPosition();
  v3 cameraFront = camera->getForward();

  std::optional<BlockPosition> potentialTarget;
  auto action = static_cast<BlockWorld::BlockAction>(m_config->insertionMode);
  switch (action) {
    case BlockAction::Add: {
      for (auto i = 0u; i < m_config->farPlane; ++i) {
        auto marchedPosition = position + (cameraFront * float(i));
        if (auto result = getBlockPosition(marchedPosition); result) {
          if (result->blockExists) {
            // We need to break in both cases because either a previous block
            // along the marched line was already selected (and thus stored in
            // potential target) or none exists at all
            break;
          }
          potentialTarget = result;
        }
      }
    } break;

    case BlockAction::Destroy: {
      for (auto i = 0u; i < m_config->farPlane; ++i) {
        if (auto result = getBlockPosition(position + (cameraFront * float(i)));
            result) {
          if (result->blockExists) {
            potentialTarget = result;
            break;
          }
        }
      }
    } break;
    default:
      assert(false);
  }
  return potentialTarget;
}

void BlockWorld::updateBlock(const BlockWorld::BlockPosition& position,
                             BlockType type) {
  ZoneScoped;
  // In theory, we need to lock here. But the write access only happens on the
  // main thread as well as this access here, so we can skip it for now.

  const auto it = m_chunkData.find(position.chunkIndex);
  assert(it != m_chunkData.end());

  const auto heightOffset =
      chunkLocalSize * chunkLocalSize * position.positionWithinChunk.y;
  const auto inLayerOffset = position.positionWithinChunk.z * chunkLocalSize +
                       position.positionWithinChunk.x;

  it->second.data[heightOffset + inLayerOffset] = type;
  it->second.state = ChunkState::DirtyRendering;

  for (auto y = -1; y < 2; ++y) {
    for (auto z = -1; z < 2; ++z) {
      for (auto x = -1; x < 2; ++x) {
        if (y == 0 && z == 0 && x == 0) {
          continue;
        }

        BlockPosition positionWithOffset =
            getPositionWithOffset(position, x, y, z);
        updateVisibilityBit(positionWithOffset, it->second.data);
      }
    }
  }
}

std::optional<BlockWorld::BlockPosition> BlockWorld::getBlockPosition(
    v3 position) {
  std::lock_guard g{m_chunkDataMutex};
  if (position.y < 0.0f || position.y >= chunkHeight) {
    return {};
  }

  BlockPosition result;
  result.chunkIndex =
      glm::ivec2((static_cast<i32>(position.x) / chunkLocalSize),
                 (static_cast<i32>(position.z) / chunkLocalSize));
  result.positionWithinChunk =
      glm::ivec3((static_cast<i32>(position.x) % chunkLocalSize),
                 static_cast<i32>(position.y),
                 (static_cast<i32>(position.z) % chunkLocalSize));

  const auto it = m_chunkData.find(result.chunkIndex);
  if (it == m_chunkData.end() ||
      it->second.state != ChunkState::FinishedGeneration) {
    result.blockExists = false;
    return result;
  }

  const auto heightOffset =
      chunkLocalSize * chunkLocalSize * result.positionWithinChunk.y;
  const auto inLayerOffset = result.positionWithinChunk.z * chunkLocalSize +
                       result.positionWithinChunk.x;

  const auto blockData = *(it->second.data.begin() + heightOffset + inLayerOffset);
  result.blockExists = blockData != BlockWorld::air;

  return result;
}

BlockWorld::BlockPosition BlockWorld::getPositionWithOffset(
    const BlockPosition& position, i32 x, i32 y, i32 z) {
  BlockPosition result = position;
  result.positionWithinChunk.y =
      std::max(std::min(position.positionWithinChunk.y + y,
                        static_cast<i32>(chunkHeight - 1)),
               0);

  if (position.positionWithinChunk.z + z >= static_cast<i32>(chunkLocalSize)) {
    ++result.chunkIndex.y;
    result.positionWithinChunk.z = 0;
  } else if (position.positionWithinChunk.z + z < 0) {
    --result.chunkIndex.y;
    result.positionWithinChunk.z = chunkLocalSize - 1;
  } else {
    result.positionWithinChunk.z = position.positionWithinChunk.z + z;
  }

  if (position.positionWithinChunk.x + x >= static_cast<i32>(chunkLocalSize)) {
    ++result.chunkIndex.x;
    result.positionWithinChunk.x = 0;
  } else if (position.positionWithinChunk.x + x < 0) {
    --result.chunkIndex.x;
    result.positionWithinChunk.x = chunkLocalSize - 1;
  } else {
    result.positionWithinChunk.x = position.positionWithinChunk.x + x;
  }

  return result;
}

void BlockWorld::updateVisibilityBit(const BlockPosition& position,
                                     std::span<BlockType> positionChunkData) {
  assert(positionChunkData.size() > 0);

  const auto heightOffsetBlockCenter =
      chunkLocalSize * chunkLocalSize * (position.positionWithinChunk.y);
  const auto inLayerOffsetBlockCenter =
      (position.positionWithinChunk.z) * chunkLocalSize +
      (position.positionWithinChunk.x);

  if (positionChunkData[heightOffsetBlockCenter + inLayerOffsetBlockCenter] ==
      BlockWorld::air) {
    return;
  }

  bool allNeighborBlocksExist = true;
  for (auto y = -1; y < 2; ++y) {
    for (auto z = -1; z < 2; ++z) {
      for (auto x = -1; x < 2; ++x) {
        if (position.positionWithinChunk.y + y >= chunkHeight ||
            position.positionWithinChunk.y + y < 0) {
          allNeighborBlocksExist = false;
          break;
        }

        BlockPosition positionWithOffset =
            getPositionWithOffset(position, x, y, z);

        // The block itself should not influence the visibility bit.
        // This happens once due to the loop and potentially if you
        // check the highest block layer in a chunk.
        if (positionWithOffset.positionWithinChunk ==
            position.positionWithinChunk) {
          continue;
        }

        std::span<BlockType> data;

        if (positionWithOffset.chunkIndex != position.chunkIndex) {
          std::lock_guard g{m_chunkDataMutex};
          auto itChunk = m_chunkData.find(position.chunkIndex);
          // This can e.g. happen if the chunk is not done yet
          if (itChunk != m_chunkData.end() ||
              itChunk->second.state != ChunkState::FinishedGeneration) {
            allNeighborBlocksExist = false;
            break;
          }
          data = itChunk->second.data;
          itChunk->second.state = ChunkState::DirtyRendering;
        } else {
          data = positionChunkData;
        }

        const auto heightOffsetBlock = chunkLocalSize * chunkLocalSize *
                                 (positionWithOffset.positionWithinChunk.y);
        const auto inLayerOffsetBlock =
            (positionWithOffset.positionWithinChunk.z) * chunkLocalSize +
            (positionWithOffset.positionWithinChunk.x);
        if (data[heightOffsetBlock + inLayerOffsetBlock] == BlockWorld::air) {
          allNeighborBlocksExist = false;
          break;
        }
      }
      if (!allNeighborBlocksExist) {
        break;
      }
    }
    if (!allNeighborBlocksExist) {
      break;
    }
  }

  if (allNeighborBlocksExist) {
    positionChunkData[heightOffsetBlockCenter + inLayerOffsetBlockCenter] |=
        highestBit;
  } else {
    positionChunkData[heightOffsetBlockCenter + inLayerOffsetBlockCenter] &=
        ~highestBit;
  }
}

}  // namespace dnm
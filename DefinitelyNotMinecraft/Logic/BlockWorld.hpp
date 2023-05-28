#pragma once

#include <span>
#include <vector>

#include "ShortTypes.hpp"

namespace dnm {

using BlockType = u8;

class BlockWorld {
 public:
  BlockWorld();

  std::span<const BlockType> getBlockTypes() const;
  bool wasModified();

  constexpr static u64 chunkLoadCount = 2;
  constexpr static u64 chunkLocalSize = 4;
  constexpr static u64 chunkHeight = 4;
  constexpr static u64 totalBlockCount =
      (chunkLocalSize * chunkLocalSize * chunkHeight) * chunkLoadCount *
      chunkLoadCount;

 private:
  std::vector<BlockType> m_blockTypesWorld;
  bool m_dirty = false;
};
}  // namespace dnm
#pragma once

#include "ShortTypes.hpp"

namespace dnm {

struct Config {
  bool everyFrameGenerateDrawCalls = false;
  u32 loadCountChunks = 4u;
};
}  // namespace dnm

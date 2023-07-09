#pragma once

#include "ShortTypes.hpp"

namespace dnm {

struct Config {
  bool everyFrameGenerateDrawCalls = false;
  u32 loadCountChunks = 4u;
  float farPlane = 250.0f;
  u32 insertionMode = 0;
};
}  // namespace dnm

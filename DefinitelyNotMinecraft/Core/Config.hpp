#pragma once

namespace dnm {

struct Config {
  // TODO This is broken right now, needs fixing
  bool useInverseDepth = false;
  bool flipY = true;
  bool recreateSwapChain = false;
};
}  // namespace dnm

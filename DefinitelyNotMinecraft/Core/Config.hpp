#pragma once

#include <Core/ShortTypes.hpp>

namespace dnm {

struct Config {
  bool everyFrameGenerateDrawCalls = false;
  bool disableImgui = false;
  bool limitFrames = true;
  bool cullingEnabled = true;

  u32 loadCountChunks = 4u;
  f32 nearPlane = 0.01f;
  f32 farPlane = 250.0f;
  u32 insertionMode = 0;
};
}  // namespace dnm

#pragma once

#include <Core/ShortTypes.hpp>

namespace dnm
{
enum class GlobalBuffers : u8
{
    ProjectionClip,
    CameraView,
    Transform,
    BlockType,
    DrawCommand,
    Undefined
};
}   // namespace dnm

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

constexpr std::string_view projectionBufferBindingPoint = "projectionBuffer";
constexpr std::string_view viewBufferBindingPoint       = "viewBuffer";
constexpr std::string_view transformBindingPoint        = "transformBuffer";
constexpr std::string_view blockTypeBindingPoint        = "blockTypeBuffer";
constexpr std::string_view drawCommandBindingPoint      = "drawCallBuffer";
}   // namespace dnm

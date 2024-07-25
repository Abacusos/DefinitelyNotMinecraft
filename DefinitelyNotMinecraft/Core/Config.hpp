#pragma once

#include <Core/GLMInclude.hpp>
#include <Core/ShortTypes.hpp>

namespace dnm
{
struct Config
{
    bool everyFrameGenerateDrawCalls = false;
    bool disableImgui                = false;
    bool limitFrames                 = true;
    bool cullingEnabled              = true;
    bool updateLight                 = true;

    u32 loadCountChunks = 4u;
    f32 nearPlane       = 0.01f;
    f32 farPlane        = 250.0f;
    u32 insertionMode   = 0;

    v3 lookingAt;

    v3    lightColor {1.0f, 1.0f, 1.0f};
    float ambientStrength = 0.1f;
    v3    lightPosition {500.0f, 170.0f, 500.0f};
    float specularStrength = 0.5f;
};
}   // namespace dnm

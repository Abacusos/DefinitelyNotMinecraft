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

    int   lightCount       = 3;
    int   specularPow      = 16;
    float smoothstepMax    = 0.001f;
    float ambientStrength  = 0.0000f;
    float specularStrength = 0.0001f;

    v3 lightColor {1.0f, 0.0f, 0.0f};
    v3 lightPosition {400.0f, 170.0f, 500.0f};
    v3 lightColor2 {0.0f, 1.0f, 0.0f};
    v3 lightPosition2 {600.0f, 170.0f, 500.0f};
    v3 lightColor3 {0.0f, 0.0f, 1.0f};
    v3 lightPosition3 {500.0f, 170.0f, 600.0f};
};
}   // namespace dnm

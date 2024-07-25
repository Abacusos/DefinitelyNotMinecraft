#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace dnm
{
using v2 = glm::vec2;
using v3 = glm::vec3;
using v4 = glm::vec4;

using m4 = glm::mat4;
}   // namespace dnm

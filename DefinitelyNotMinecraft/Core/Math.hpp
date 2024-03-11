#pragma once

#include <Core/ShortTypes.hpp>
#include <Core/GLMInclude.hpp>

namespace dnm {

  m4 createProjectionMatrix(const v2& extent, f32 n,
                                   f32 f);
}  // namespace dnm

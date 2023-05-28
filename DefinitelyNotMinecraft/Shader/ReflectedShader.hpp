#pragma once

#include <vector>

#include "Handle.hpp"

namespace dnm {
struct ReflectedShader {
  std::vector<InternedString> defines;
};
}  // namespace dnm

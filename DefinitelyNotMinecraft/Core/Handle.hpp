#pragma once

#include <limits>

#include "ShortTypes.hpp"

namespace dnm {

template <typename T, typename IndexType = u32>
class Handle {
 public:
  constexpr static IndexType invalidValue =
      std::numeric_limits<IndexType>::max();
  IndexType index = IndexType(0);

  operator bool() const { index != invalidValue; };
};

class String;
using InternedString = Handle<String>;

class Shader;
using ShaderHandle = Handle<Shader>;

}  // namespace dnm

#pragma once

#include <limits>

#include <Core/ShortTypes.hpp>

namespace dnm {

template <typename T, typename IndexType = u32>
class Handle {
 public:
  constexpr static IndexType invalidValue =
      std::numeric_limits<IndexType>::max();
  IndexType index = IndexType(0);

  operator bool() const { return index != invalidValue; };

  friend bool operator==(const Handle& lhs, const Handle& rhs)
  {
      return lhs.index == rhs.index;
  }

  friend bool operator!=(const Handle& lhs, const Handle& rhs)
  {
      return !(lhs == rhs);
  }
};

class String;
using InternedString = Handle<String>;

class Shader;
using ShaderHandle = Handle<Shader>;

}  // namespace dnm

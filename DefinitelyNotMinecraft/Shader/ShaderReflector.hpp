#pragma once

#include <string_view>
#include <vector>

#include "ReflectedShader.hpp"

namespace dnm {
class StringInterner;

class ShaderReflector {
 public:
  ShaderReflector(StringInterner* interner);

  ReflectedShader reflectShader(std::string_view view);

 private:
  StringInterner* m_interner;
};
}  // namespace dnm

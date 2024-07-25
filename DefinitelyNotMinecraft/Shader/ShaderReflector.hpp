#pragma once

#include <Shader/ReflectedShader.hpp>

#include <string_view>

namespace dnm
{
class StringInterner;

class ShaderReflector {
    public:
    ShaderReflector(StringInterner* interner);

    ReflectedShader reflectShader(std::string_view view) const;

    private:
    StringInterner* m_interner;
};
}   // namespace dnm

#pragma once

#include <string_view>

#include <Shader/ReflectedShader.hpp>

namespace dnm
{
    class StringInterner;

    class ShaderReflector
    {
    public:
        ShaderReflector(StringInterner* interner);

        ReflectedShader reflectShader(std::string_view view) const;

    private:
        StringInterner* m_interner;
    };
} // namespace dnm

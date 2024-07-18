#include <Shader/ShaderReflector.hpp>

#include <Core/StringInterner.hpp>
#include <array>
#include <cassert>

namespace dnm
{
    namespace
    {
        constexpr std::array<std::string_view, 3u> searchTokens = {
            "local_size_x", "local_size_y", "local_size_z"
        };
    }

    ShaderReflector::ShaderReflector(StringInterner* interner)
        : m_interner{interner}
    {
    }

    ReflectedShader ShaderReflector::reflectShader(std::string_view view) const
    {
        // Tokenize stream
        std::vector<std::string_view> tokenized;
        // Hard to guess the number of tokens here
        tokenized.reserve(view.size() / 4u);

        u32 index = 0u;
        while (index < view.size())
        {
            auto indexSpace = view.find_first_of(' ', index);
            tokenized.emplace_back(view.substr(index, indexSpace - index));
            if (indexSpace == view.npos)
            {
                break;
            }
            index = indexSpace + 1u;
        }

        ReflectedShader shader;
        for (u32 i = 0u, size = tokenized.size(); i < size; ++i)
        {
            const auto& token = tokenized[i];
            for (auto searchToken : searchTokens)
            {
                if (token == searchToken && i + 2 < size)
                {
                    assert(tokenized[i + 1] == "=");
                    if (tokenized[i + 2].find_first_not_of("0123456789") ==
                        std::string_view::npos)
                    {
                        InternedString define = m_interner->addOrGetString(tokenized[i + 2]);
                        shader.defines.emplace_back(define);
                    }
                }
            }
        }
        return shader;
    }
} // namespace dnm

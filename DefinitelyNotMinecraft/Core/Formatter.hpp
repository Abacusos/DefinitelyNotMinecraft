#pragma once

#include <format>

#include <Core/GLMInclude.hpp>

namespace std
{
    template <class CharT>
    struct formatter<dnm::m4, CharT>
    {
        template <typename FormatParseContext>
        constexpr auto parse(FormatParseContext& pc)
        {
            return pc.end();
        }

        template <typename FormatContext>
        constexpr auto format(const dnm::m4& m, FormatContext& fc) const
        {
            return std::format_to(fc.out(),
                                  "[{:.3f}, {:.3f}, {:.3f}, {:.3f}]\n"
                                  "[{:.3f}, {:.3f}, {:.3f}, {:.3f}]\n"
                                  "[{:.3f}, {:.3f}, {:.3f}, {:.3f}]\n"
                                  "[{:.3f}, {:.3f}, {:.3f}, {:.3f}]\n",
                                  m[0][0], m[0][1], m[0][2], m[0][3], m[1][0], m[1][1],
                                  m[1][2], m[1][3], m[2][0], m[2][1], m[2][2], m[2][3],
                                  m[3][0], m[3][1], m[3][2], m[3][3]);
        }
    };
} // namespace std

#pragma once

#include <vector>

#include <Core/Handle.hpp>

namespace dnm
{
struct ReflectedShader
{
    std::vector<InternedString> defines;
};
}   // namespace dnm

#pragma once

#include <cassert>
#include <string>
#include <vector>

#include "Handle.hpp"

namespace dnm {

class StringInterner {
 public:
  InternedString addOrGetString(std::string_view toBeAddedString) {
    // This is somewhat slow, could add a map later on if necessary
    for (u32 i = 0u, size = m_strings.size(); i < size; ++i) {
      const auto& string = m_strings[i];
      if (string == toBeAddedString) {
        return InternedString{.index = i};
      };
    }
    m_strings.emplace_back(std::string(toBeAddedString));
    return InternedString{.index = static_cast<u32>(m_strings.size() - 1)};
  };

  std::string_view getStringView(InternedString string) {
    assert(string.index < m_strings.size());
    return m_strings[string.index];
  };

 private:
  std::vector<std::string> m_strings;
};
}  // namespace dnm

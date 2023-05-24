// Copyright(c) 2019, NVIDIA CORPORATION. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#pragma once

#include <string>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

namespace dnm {

class Shader {
 public:
  Shader(std::nullptr_t) : m_module{nullptr} {};
  Shader(const vk::raii::Device& device, vk::ShaderStageFlagBits shaderStage,
         std::string_view fileName);

  Shader& operator=(Shader&& shader) noexcept {
    if (this != &shader) {
      m_module = std::move(shader.m_module);
      m_dirty = shader.m_dirty.load();
      m_fileName = std::move(shader.m_fileName);
      m_shaderStage = shader.m_shaderStage;
    }

    return *this;
  }

  bool recompile(const vk::raii::Device& device);

  vk::raii::ShaderModule m_module;
  std::atomic<bool> m_dirty;
  std::string m_fileName;
  vk::ShaderStageFlagBits m_shaderStage = vk::ShaderStageFlagBits::eAll;

 private:
  static std::string readFile(const std::string& filename);
  bool GLSLtoSPV(const std::string& glslShader,
                 std::vector<unsigned int>& spvShader);
};
}  // namespace dnm
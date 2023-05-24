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

#include "Shaders.hpp"

#include <glslang/Public/ResourceLimits.h>
#include <glslang/SPIRV/GlslangToSpv.h>

#include <fstream>
#include <iostream>
#include <vulkan/vulkan.hpp>

namespace dnm {

namespace {
EShLanguage translateShaderStage(vk::ShaderStageFlagBits stage) {
  switch (stage) {
    case vk::ShaderStageFlagBits::eVertex:
      return EShLangVertex;
    case vk::ShaderStageFlagBits::eTessellationControl:
      return EShLangTessControl;
    case vk::ShaderStageFlagBits::eTessellationEvaluation:
      return EShLangTessEvaluation;
    case vk::ShaderStageFlagBits::eGeometry:
      return EShLangGeometry;
    case vk::ShaderStageFlagBits::eFragment:
      return EShLangFragment;
    case vk::ShaderStageFlagBits::eCompute:
      return EShLangCompute;
    case vk::ShaderStageFlagBits::eRaygenNV:
      return EShLangRayGenNV;
    case vk::ShaderStageFlagBits::eAnyHitNV:
      return EShLangAnyHitNV;
    case vk::ShaderStageFlagBits::eClosestHitNV:
      return EShLangClosestHitNV;
    case vk::ShaderStageFlagBits::eMissNV:
      return EShLangMissNV;
    case vk::ShaderStageFlagBits::eIntersectionNV:
      return EShLangIntersectNV;
    case vk::ShaderStageFlagBits::eCallableNV:
      return EShLangCallableNV;
    case vk::ShaderStageFlagBits::eTaskNV:
      return EShLangTaskNV;
    case vk::ShaderStageFlagBits::eMeshNV:
      return EShLangMeshNV;
    default:
      assert(false && "Unknown shader stage");
      return EShLangVertex;
  }
}
}  // namespace

Shader::Shader(const vk::raii::Device& device,
               vk::ShaderStageFlagBits shaderStage, std::string_view fileName)
    : m_module{nullptr},
      m_dirty{true},
      m_fileName{std::string(fileName)},
      m_shaderStage{shaderStage} {
}

bool Shader::recompile(const vk::raii::Device& device) {
  if (m_fileName.empty()) {
    return false;
  }

  auto fileContent = readFile(m_fileName);
  std::vector<unsigned int> shaderSPV;
  if (!GLSLtoSPV(fileContent, shaderSPV)) {
    return false;
  }

  m_module = vk::raii::ShaderModule(
      device,
      vk::ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(), shaderSPV));

  return true;
}

std::string Shader::readFile(const std::string& filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("failed to open file!");
  }

  size_t fileSize = (size_t)file.tellg();
  std::string buffer;
  buffer.resize(fileSize);

  file.seekg(0);
  file.read(buffer.data(), fileSize);

  file.close();

  return buffer;
}

bool Shader::GLSLtoSPV(const std::string& glslShader,
                       std::vector<unsigned int>& spvShader) {
  EShLanguage stage = translateShaderStage(m_shaderStage);

  const char* shaderStrings[1];
  shaderStrings[0] = glslShader.data();

  glslang::TShader shader(stage);
  shader.setStrings(shaderStrings, 1);

  // Enable SPIR-V and Vulkan rules when parsing GLSL
  EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);

  if (!shader.parse(GetDefaultResources(), 100, false, messages)) {
    puts(shader.getInfoLog());
    puts(shader.getInfoDebugLog());
    return false;  // something didn't work
  }

  glslang::TProgram program;
  program.addShader(&shader);

  //
  // Program-level processing...
  //

  if (!program.link(messages)) {
    puts(shader.getInfoLog());
    puts(shader.getInfoDebugLog());
    fflush(stdout);
    return false;
  }

  glslang::GlslangToSpv(*program.getIntermediate(stage), spvShader);
  return true;
}
}  // namespace dnm

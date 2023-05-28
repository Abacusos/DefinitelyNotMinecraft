#include "ShaderManager.hpp"

#include <glslang/Public/ResourceLimits.h>
#include <glslang/SPIRV/GlslangToSpv.h>

#include <filesystem>
#include <fstream>
#include <iostream>

#include "StringInterner.hpp"

namespace dnm {

namespace {
std::string readFile(std::string_view filename) {
  std::ifstream file(filename.data(), std::ios::ate | std::ios::binary);

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
bool GLSLtoSPV(std::string_view glslShader, vk::ShaderStageFlagBits stageFlag,
               std::vector<unsigned int>& spvShader) {
  EShLanguage stage = translateShaderStage(stageFlag);

  const char* shaderStrings[1];
  shaderStrings[0] = glslShader.data();

  glslang::TShader shader(stage);
  shader.setStrings(shaderStrings, 1);

  // Enable SPIR-V and Vulkan rules when parsing GLSL
  EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);

  if (!shader.parse(GetDefaultResources(), 100, false, messages)) {
    std::cerr << shader.getInfoLog() << std::endl;

    if (auto* debugLog = shader.getInfoDebugLog(); debugLog) {
      std::cerr << debugLog << std::endl;
    }
    return false;  // something didn't work
  }

  glslang::TProgram program;
  program.addShader(&shader);

  //
  // Program-level processing...
  //

  if (!program.link(messages)) {
    auto* log = shader.getInfoLog();
    auto* debugLog = shader.getInfoDebugLog();
    puts(log);
    puts(debugLog);
    fflush(stdout);
    return false;
  }

  glslang::GlslangToSpv(*program.getIntermediate(stage), spvShader);
  return true;
}

constexpr std::string_view versionPreamble = "#version 460\n";
}  // namespace

ShaderManager::ShaderManager(StringInterner* interner)
    : m_interner{interner}, m_reflector{interner} {
  m_running.store(true);
  m_watcher = std::thread([this]() {
    while (true) {
      if (!m_running) {
        return;
      }

      constexpr auto sleeptime = std::chrono::seconds(1);
      std::this_thread::sleep_for(sleeptime);

      std::lock_guard guard(m_mutex);
      for (auto& shader : m_shaders) {
        auto lastModification = std::filesystem::last_write_time(
            m_interner->getStringView(shader.filePath));
        if (lastModification != shader.modificationTimeStamp) {
          shader.dirty = true;
          shader.modificationTimeStamp = lastModification;
          m_dirty.store(true);
        }
      };
    }
  });
}
ShaderManager::~ShaderManager() {
  m_running.store(false);
  m_watcher.join();
}
ShaderHandle ShaderManager::registerShaderFile(
    InternedString filePath, vk::ShaderStageFlagBits shaderStage) {
  std::lock_guard guard(m_mutex);

  u32 index = m_shaders.size();
  auto& shader = m_shaders.emplace_back();

  shader.filePath = filePath;
  auto filePathView = m_interner->getStringView(shader.filePath);
  shader.shaderContent = readFile(filePathView);
  shader.shaderStage = shaderStage;
  shader.reflection = m_reflector.reflectShader(shader.shaderContent);
  shader.modificationTimeStamp = std::filesystem::last_write_time(filePathView);

  return ShaderHandle{.index = index};
}

std::optional<vk::raii::ShaderModule> ShaderManager::getCompiledVersion(
    const vk::raii::Device& device, ShaderHandle handle,
    std::span<Define> defines) {
  assert(handle.index < m_shaders.size());

  std::string completeShader;
  u32 size = versionPreamble.size();

  for (auto& define : defines) {
    size += m_interner->getStringView(define.defineName).size();
    size += define.defineValue.size();
  }

  size += m_shaders[handle.index].shaderContent.size();

  completeShader.reserve(size);

  completeShader.insert(0u, versionPreamble);
  for (auto& define : defines) {
    completeShader.insert(
        completeShader.size(),
        std::format("#define {} {}\n",
                    m_interner->getStringView(define.defineName),
                    define.defineValue));
  }

  completeShader.insert(completeShader.size(),
                        m_shaders[handle.index].shaderContent);

  std::vector<unsigned int> shaderSPV;
  if (!GLSLtoSPV(completeShader, m_shaders[handle.index].shaderStage,
                 shaderSPV)) {
    return {};
  }

  return vk::raii::ShaderModule(
      device,
      vk::ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(), shaderSPV));
}
bool ShaderManager::wasContentUpdated(ShaderHandle handle) {
  assert(handle.index < m_shaders.size());
  return m_shaders[handle.index].isContentUpdated;
}
void ShaderManager::update() {
  if (!m_dirty.load()) {
    return;
  }

  std::lock_guard guard(m_mutex);
  for (auto& shader : m_shaders) {
    shader.isContentUpdated = false;
    if (shader.dirty) {
      shader.shaderContent =
          readFile(m_interner->getStringView(shader.filePath));
      shader.isContentUpdated = true;
      shader.dirty = false;
    }
  }
}
}  // namespace dnm
#include "Shader/ShaderManager.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

#include <Core/StringInterner.hpp>

#include <shaderc/shaderc.hpp>

namespace dnm
{
namespace
{
    std::string readFile(std::string_view filename) {
        std::ifstream file(filename.data(), std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }

        const size_t fileSize = (size_t)file.tellg();
        std::string  buffer;
        buffer.resize(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();

        return buffer;
    }

    shaderc_shader_kind translateShaderStage(vk::ShaderStageFlagBits stage) {
        switch (stage) {
            case vk::ShaderStageFlagBits::eVertex:
                return shaderc_vertex_shader;
            case vk::ShaderStageFlagBits::eTessellationControl:
                return shaderc_tess_control_shader;
            case vk::ShaderStageFlagBits::eTessellationEvaluation:
                return shaderc_tess_evaluation_shader;
            case vk::ShaderStageFlagBits::eGeometry:
                return shaderc_geometry_shader;
            case vk::ShaderStageFlagBits::eFragment:
                return shaderc_fragment_shader;
            case vk::ShaderStageFlagBits::eCompute:
                return shaderc_compute_shader;
            case vk::ShaderStageFlagBits::eRaygenNV:
                return shaderc_raygen_shader;
            case vk::ShaderStageFlagBits::eAnyHitNV:
                return shaderc_anyhit_shader;
            case vk::ShaderStageFlagBits::eClosestHitNV:
                return shaderc_closesthit_shader;
            case vk::ShaderStageFlagBits::eMissNV:
                return shaderc_miss_shader;
            case vk::ShaderStageFlagBits::eIntersectionNV:
                return shaderc_intersection_shader;
            case vk::ShaderStageFlagBits::eCallableNV:
                return shaderc_callable_shader;
            case vk::ShaderStageFlagBits::eTaskNV:
                return shaderc_task_shader;
            case vk::ShaderStageFlagBits::eMeshNV:
                return shaderc_mesh_shader;
            default:
                assert(false && "Unknown shader stage");
                return shaderc_glsl_infer_from_source;
        }
    }

    constexpr std::string_view includeSearchView = "#include \"";
    constexpr std::string_view bindingSearchView = "binding = ";
    constexpr std::string_view lineEndViewEnd    = "\"\r\n";
    constexpr std::string_view lineEnding        = "\r\n";

    void printShaderContent(std::string_view shader) {
        size_t lineCount = 1u;
        size_t start     = 0ull;
        size_t line      = shader.find('\n');
        while (line != std::string::npos) {
            std::cout << lineCount++ << ": " << std::string_view(shader.data() + start, shader.data() + line) << "\n";
            start = line + 1;
            line  = shader.find("\n", start);
        }

        std::cout << lineCount++ << ": " << std::string_view(shader.data() + start, shader.size() - start) << "\n";
    }
}   // namespace

ShaderManager::ShaderManager(StringInterner* interner) : m_interner {interner}, m_reflector {interner} {
    m_running.store(true);
    m_watcher = std::thread(
      [this]()
      {
          while (true) {
              if (!m_running) {
                  return;
              }

              constexpr auto sleeptime = std::chrono::seconds(1);
              std::this_thread::sleep_for(sleeptime);

              std::lock_guard guard(m_mutex);
              for (auto& shader : m_shaders) {
                  auto lastModification = std::filesystem::last_write_time(m_interner->getStringView(shader.filePath));
                  if (lastModification != shader.modificationTimeStamp) {
                      shader.dirty                 = true;
                      shader.modificationTimeStamp = lastModification;
                      m_dirty.store(true);
                  }
              }
          }
      });
}

ShaderManager::~ShaderManager() {
    m_running.store(false);
    m_watcher.join();
}

ShaderHandle ShaderManager::registerShaderFile(InternedString filePath, vk::ShaderStageFlagBits shaderStage) {
    std::lock_guard guard(m_mutex);
    return registerShaderFile(filePath, shaderStage, {});
}

void ShaderManager::getBindingSlots(std::span<InternedString> filePaths, std::vector<BindingSlot>& slots, vk::ShaderStageFlags& stageFlags) {
    stageFlags = {};

    for (auto filePath : filePaths) {
        bool found = false;
        for (auto& shader : m_shaders) {
            if (shader.filePath == filePath) {
                slots.reserve(m_shaders.size() * 2u);
                slots.insert(slots.end(), shader.slots.begin(), shader.slots.end());
                for (auto& includer : shader.includes) {
                    getBindingSlots(m_shaders [includer.index].filePath, slots);
                }
                stageFlags |= shader.shaderStage;
                found = true;
                break;
            }
        }
        assert(found);
    }
}

ShaderHandle ShaderManager::registerShaderFile(InternedString filePath, vk::ShaderStageFlagBits shaderStage, std::optional<ShaderHandle> includer) {
    for (auto i = 0u, size = static_cast<u32>(m_shaders.size()); i < size; ++i) {
        auto& shader = m_shaders [i];
        if (shader.filePath == filePath) {
            if (includer) {
                shader.includedBy.emplace_back(includer.value());
            }
            return ShaderHandle {.index = i};
        }
    }

    const u32 index  = static_cast<u32>(m_shaders.size());
    auto&     shader = m_shaders.emplace_back();

    shader.filePath         = filePath;
    const auto filePathView = m_interner->getStringView(shader.filePath);
    shader.shaderContent    = readFile(filePathView);

    std::vector<InternedString> includes;

    size_t includeStart = shader.shaderContent.find(includeSearchView);
    while (includeStart != std::string::npos) {
        const size_t           includeEnd   = shader.shaderContent.find(lineEndViewEnd, includeStart);
        const char*            startInclude = shader.shaderContent.data() + includeStart + includeSearchView.size();
        const std::string_view includeView {startInclude, includeEnd - includeStart - includeSearchView.size()};
        const InternedString   internedInclude = m_interner->addOrGetString(includeView);
        includes.emplace_back(internedInclude);
        includeStart = shader.shaderContent.find(includeSearchView, includeEnd);
    }

    shader.shaderStage           = shaderStage;
    shader.reflection            = m_reflector.reflectShader(shader.shaderContent);
    shader.modificationTimeStamp = std::filesystem::last_write_time(filePathView);

    size_t bindingStart = shader.shaderContent.find(bindingSearchView);
    while (bindingStart != std::string::npos) {
        auto uniqueBinding = ++m_nextUniqueBinding;
        shader.shaderContent.insert(bindingStart + bindingSearchView.size(), std::to_string(uniqueBinding));
        size_t bracketEnd = shader.shaderContent.find(')', bindingStart);
        bracketEnd += 2;
        auto firstKeywordEnd = shader.shaderContent.find(lineEnding, bracketEnd);
        auto keyWordsAndName = std::string_view {&shader.shaderContent [bracketEnd], &shader.shaderContent [firstKeywordEnd]};

        auto beginName = keyWordsAndName.find_last_of(' ') + 1;
        auto name      = std::string_view {&keyWordsAndName [beginName], keyWordsAndName.size() - beginName};
        auto keyWords  = std::string_view {keyWordsAndName.data(), beginName};

        auto& slot = shader.slots.emplace_back();

        if (keyWords.find("readonly") != keyWords.npos) {
            slot.readonly = true;
        }

        if (keyWords.find("buffer") != keyWords.npos) {
            slot.type = vk::DescriptorType::eStorageBuffer;
        }
        else if (keyWords.find("uniform sampler2D") != keyWords.npos) {
            slot.type = vk::DescriptorType::eCombinedImageSampler;
        }
        else if (keyWords.find("uniform") != keyWords.npos) {
            slot.type = vk::DescriptorType::eUniformBuffer;
        }
        else {
            assert(false);
        }

        slot.bindingSlot = uniqueBinding;
        slot.name        = name;

        bindingStart = shader.shaderContent.find(bindingSearchView, bracketEnd);
    }

    for (const auto& include : includes) {
        auto shaderHandle = registerShaderFile(include, vk::ShaderStageFlagBits::eAll, ShaderHandle {.index = index});
        m_shaders [index].includes.emplace_back(shaderHandle);
    }

    return ShaderHandle {.index = index};
}

void ShaderManager::getBindingSlots(InternedString filePath, std::vector<BindingSlot>& slots) {
    for (auto& shader : m_shaders) {
        if (shader.filePath == filePath) {
            for (auto& slot : shader.slots) {
                bool existsAlready = false;
                for (auto& bindingSlot : slots) {
                    if (bindingSlot.bindingSlot == slot.bindingSlot) {
                        existsAlready = true;
                        break;
                    }
                }
                if (!existsAlready) {
                    slots.insert(slots.end(), slot);
                }
            }

            for (auto& includer : shader.includes) {
                getBindingSlots(m_shaders [includer.index].filePath, slots);
            }
        }
    }
}

std::optional<vk::raii::ShaderModule> ShaderManager::getCompiledVersion(const vk::raii::Device& device, ShaderHandle handle, std::span<Define> defines) const {
    assert(handle.index < m_shaders.size());

    auto completeShader = m_shaders [handle.index].shaderContent;

    for (auto includeHandle : m_shaders [handle.index].includes) {
        std::string_view fileName = m_interner->getStringView(m_shaders [includeHandle.index].filePath);
        std::string      include;
        auto             includeLength = includeSearchView.size() + fileName.size() + lineEndViewEnd.size();
        include.reserve(includeLength);
        include = includeSearchView;
        include.append(fileName);
        include.append(lineEndViewEnd);

        const auto includeStart = completeShader.find(include);
        completeShader.replace(includeStart, includeLength, m_shaders [includeHandle.index].shaderContent);
    }

    const shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetForcedVersionProfile(460, shaderc_profile_core);
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
    options.SetOptimizationLevel(shaderc_optimization_level_zero);
    options.SetTargetSpirv(shaderc_spirv_version_1_3);

    for (auto& define : defines) {
        auto name = m_interner->getStringView(define.defineName);
        options.AddMacroDefinition(name.data(), name.size(), define.defineValue.data(), define.defineValue.size());
    }

    const auto shaderNameString = m_interner->getStringView(m_shaders [handle.index].filePath);

    const auto shaderKind = translateShaderStage(m_shaders [handle.index].shaderStage);

    const auto preprocessed = compiler.PreprocessGlsl(completeShader.c_str(), completeShader.size(), shaderKind, shaderNameString.data(), options);

    if (preprocessed.GetCompilationStatus() != shaderc_compilation_status_success) {
        printShaderContent(completeShader);
        std::cerr << preprocessed.GetErrorMessage();
        return {};
    }

    const std::string sourcePreprocessed(preprocessed.cbegin(), preprocessed.cend());
    const auto        compiling = compiler.CompileGlslToSpv(sourcePreprocessed, shaderKind, shaderNameString.data(), options);

    if (compiling.GetCompilationStatus() != shaderc_compilation_status_success) {
        printShaderContent(sourcePreprocessed);
        std::cerr << compiling.GetErrorMessage();
        return {};
    }

    std::vector spirv(compiling.begin(), compiling.end());

    return vk::raii::ShaderModule(device, vk::ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(), spirv));
}

bool ShaderManager::wasContentUpdated(ShaderHandle handle) {
    assert(handle.index < m_shaders.size());
    return m_shaders [handle.index].isContentUpdated;
}

void ShaderManager::update() {
    if (!m_dirty.load()) {
        return;
    }

    std::lock_guard guard(m_mutex);
    for (auto& shader : m_shaders) {
        shader.isContentUpdated = false;
        if (shader.dirty) {
            shader.shaderContent    = readFile(m_interner->getStringView(shader.filePath));
            shader.isContentUpdated = true;
            shader.dirty            = false;
        }
    }
}
}   // namespace dnm

#pragma once

#include <atomic>
#include <filesystem>
#include <mutex>
#include <span>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

#include <Shader/ReflectedShader.hpp>
#include <Shader/ShaderReflector.hpp>

namespace dnm
{
class StringInterner;

struct BindingSlot
{
    std::string_view   name;
    u32                bindingSlot;
    vk::DescriptorType type;
    bool               readonly = false;
};

class ShaderManager {
    public:
    explicit ShaderManager(StringInterner* interner);
    ~ShaderManager();

    ShaderHandle registerShaderFile(InternedString filePath, vk::ShaderStageFlagBits shaderStage);

    void getBindingSlots(std::span<InternedString> filePaths, std::vector<BindingSlot>& slots, vk::ShaderStageFlags& stageFlags);

    struct Define
    {
        InternedString defineName;
        std::string    defineValue;
    };

    std::optional<vk::raii::ShaderModule> getCompiledVersion(const vk::raii::Device& device, ShaderHandle handle, std::span<Define> defines) const;

    bool wasContentUpdated(ShaderHandle handle);

    void update();

    private:
    ShaderHandle registerShaderFile(InternedString filePath, vk::ShaderStageFlagBits shaderStage, std::optional<ShaderHandle> includer);
    void getBindingSlots(InternedString filePath, std::vector<BindingSlot>& slots);

    StringInterner* m_interner;
    ShaderReflector m_reflector;

    struct Shader
    {
        InternedString                                   filePath;
        std::string                                      shaderContent;
        vk::ShaderStageFlagBits                          shaderStage;
        ReflectedShader                                  reflection;
        std::chrono::time_point<std::chrono::file_clock> modificationTimeStamp;
        bool                                             dirty            = false;
        bool                                             isContentUpdated = false;
        std::vector<ShaderHandle>                        includedBy;
        std::vector<ShaderHandle>                        includes;
        std::vector<BindingSlot>                         slots;
    };

    std::vector<Shader> m_shaders;

    // Shader watcher related
    std::mutex        m_mutex;
    std::thread       m_watcher;
    std::atomic<bool> m_running;
    std::atomic<bool> m_dirty;
    u32               m_nextUniqueBinding = 0;
};
}   // namespace dnm

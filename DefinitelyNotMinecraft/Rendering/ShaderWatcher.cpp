#include "ShaderWatcher.hpp"

#include <filesystem>

namespace dnm {

ShaderWatcher::ShaderWatcher() {
  m_running.store(true);
  m_watcher = std::thread([this]() {
    while (true) {
      if (!m_running) {
        return;
      }

      constexpr auto sleeptime = std::chrono::seconds(1);
      std::this_thread::sleep_for(sleeptime);

      std::lock_guard guard(m_mutex);
      for (auto& [shader, filename, modificationTimeStamp] : m_shaders) {
        auto lastModification = std::filesystem::last_write_time(filename);
        if (lastModification != modificationTimeStamp) {
          shader->m_dirty.store(true);
          modificationTimeStamp = lastModification;
        }
      };
    }
  });
}

ShaderWatcher::~ShaderWatcher() {
  m_running.store(false);
  m_watcher.join();
}

void ShaderWatcher::registerShader(Shader* shader, std::string_view filename) {
  auto time = std::filesystem::last_write_time(filename);
  std::lock_guard guard(m_mutex);
  m_shaders.emplace_back(shader, std::string(filename), time);
}

}  // namespace dnm
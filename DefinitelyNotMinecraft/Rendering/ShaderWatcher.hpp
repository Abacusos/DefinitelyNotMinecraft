#pragma once

#include <thread>
#include <vector>

#include "Chrono.hpp"
#include "Shaders.hpp"

namespace dnm {

class ShaderWatcher {
 public:
  explicit ShaderWatcher();
  ~ShaderWatcher();

  void registerShader(Shader* shader, std::string_view filename);

 private:
  std::thread m_watcher;
  std::atomic<bool> m_running;

  struct ShaderEntryData {
    Shader* shader;
    std::string filename;
    std::chrono::time_point<std::chrono::file_clock> modificationTimeStamp; 
  };

  std::mutex m_mutex;
  std::vector<ShaderEntryData> m_shaders;
};
}  // namespace dnm
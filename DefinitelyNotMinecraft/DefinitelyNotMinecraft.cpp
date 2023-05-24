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
// VulkanHpp Samples : 15_DrawCube
//                     Draw a cube

#include "DefinitelyNotMinecraft.hpp"

#include "Camera.hpp"
#include "Chrono.hpp"
#include "Input.hpp"
#include "Renderer.hpp"
#include "ShaderWatcher.hpp"
#include "BlockRenderingModule.hpp"
#include "ImguiRenderingModule.hpp"
#include "Imgui.hpp"
#include "Config.hpp"
#include "optick.h"

int main(int /*argc*/, char** /*argv*/) {
  using namespace dnm;
  try {
    Config config;
    ShaderWatcher watcher{};
    Renderer renderer{&config};
    auto* window = renderer.getGLFWwindow();
    Camera camera(&config, &renderer);
    Input input(&camera, window);
    Imgui imgui(&config, window);
    BlockWorld world;

    BlockRenderingModule blockRenderingModule{&config, &renderer, &watcher, &world};
    ImguiRenderingModule imguiModule{&config, &renderer};

    auto lastFrame = std::chrono::system_clock::now();

    while (!glfwWindowShouldClose(window)) {
      OPTICK_FRAME("MainThread");
      auto now = std::chrono::system_clock::now();
      auto deltaTime = std::chrono::duration_cast<TimeSpan>(now - lastFrame);
      lastFrame = now;

      input.processInput(window, deltaTime);
      bool cameraMoved = camera.update(deltaTime);
      glfwPollEvents();

      imgui.logicFrame(deltaTime, &camera);

      auto imageIndex = renderer.prepareDrawFrame();
      
      if (imageIndex == std::numeric_limits<u32>::max()) {
        continue;
      }

      blockRenderingModule.drawFrame(renderer.getFrameBuffer(imageIndex), deltaTime,
                       cameraMoved);
      imguiModule.drawFrame(
          renderer.getFrameBuffer(imageIndex), deltaTime,
          blockRenderingModule.getRenderingFinishedSemaphore());

      renderer.finishDrawFrame(imguiModule.getRenderingFinishedSemaphore(),
                               imageIndex);
    }

    renderer.waitIdle();
    OPTICK_SHUTDOWN()
  } catch (vk::SystemError& err) {
    std::cout << "vk::SystemError: " << err.what() << std::endl;
    exit(-1);
  } catch (std::exception& err) {
    std::cout << "std::exception: " << err.what() << std::endl;
    exit(-1);
  } catch (...) {
    std::cout << "unknown error\n";
    exit(-1);
  }
  return 0;
}

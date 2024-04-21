#include "DefinitelyNotMinecraft.hpp"

#include <RenderingModule/BlockRenderingModule.hpp>
#include <Logic/Camera.hpp>
#include <Core/Chrono.hpp>
#include <Core/Config.hpp>
#include <RenderingModule/GizmoRenderingModule.hpp>
#include <Logic/Imgui.hpp>
#include <RenderingModule/ImguiRenderingModule.hpp>
#include <RenderingModule/ForwardRenderingModule.hpp>
#include <Logic/Input.hpp>
#include <Core/Profiler.hpp>
#include <Rendering/Renderer.hpp>
#include <Shader/ShaderManager.hpp>
#include <Core/StringInterner.hpp>

int main(int /*argc*/, char** /*argv*/) {
  using namespace dnm;
  try {
    Config config;
    StringInterner interner;
    ShaderManager shaderManager{&interner};
    Renderer renderer{&config};
    auto* window = renderer.getGLFWwindow();
    Camera camera(&config, &renderer);
    BlockWorld world{&config};
    Imgui imgui(&config, window);

    BlockRenderingModule blockRenderingModule{
        &config, &renderer, &shaderManager, &world, &interner};
    ForwardRenderingModule forwardRenderingModule{
        &config, &renderer, &shaderManager, &world, &interner};
    ImguiRenderingModule imguiModule{&config, &renderer};
    GizmoRenderingModule gizmoRenderingModule{
        &config, &renderer, &shaderManager, &world, &interner};

    Input input(&camera, window, &world, &config, &gizmoRenderingModule);

    auto lastFrame = std::chrono::system_clock::now();

    while (!glfwWindowShouldClose(window)) {
      FrameMark;
      auto now = std::chrono::system_clock::now();
      auto deltaTime = std::chrono::duration_cast<TimeSpan>(now - lastFrame);
      lastFrame = now;

      input.processInput(window, deltaTime);
      bool cameraMoved = camera.update(deltaTime);
      glfwPollEvents();

      imgui.logicFrame(deltaTime, &camera);

      shaderManager.update();

      auto imageIndex = renderer.prepareDrawFrame();

      if (imageIndex == std::numeric_limits<u32>::max()) {
        continue;
      }

      blockRenderingModule.drawFrame(renderer.getFrameBuffer(imageIndex),
                                     deltaTime, cameraMoved, &camera);
      forwardRenderingModule.drawFrame(renderer.getFrameBuffer(imageIndex),
                                       blockRenderingModule.getRenderingFinishedSemaphore());
      imguiModule.drawFrame(
          renderer.getFrameBuffer(imageIndex), deltaTime,
          forwardRenderingModule.getRenderingFinishedSemaphore());
      gizmoRenderingModule.drawFrame(
          renderer.getFrameBuffer(imageIndex), deltaTime,
          imguiModule.getRenderingFinishedSemaphore());

      renderer.finishDrawFrame(
          gizmoRenderingModule.getRenderingFinishedSemaphore(), imageIndex);

      if (config.limitFrames) {
        using namespace std::chrono_literals;
        constexpr auto targetFrameRate = std::chrono::milliseconds(16ms);
        auto deltaTime = (std::chrono::system_clock::now() - now);
        if (deltaTime < targetFrameRate) {
          std::this_thread::sleep_for(targetFrameRate - deltaTime);
        }
      }
    }

    renderer.waitIdle();
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

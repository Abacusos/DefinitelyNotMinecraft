#include "DefinitelyNotMinecraft.hpp"

#include "BlockRenderingModule.hpp"
#include "Camera.hpp"
#include "Chrono.hpp"
#include "Config.hpp"
#include "GizmoRenderingModule.hpp"
#include "Imgui.hpp"
#include "ImguiRenderingModule.hpp"
#include "Input.hpp"
#include "Renderer.hpp"
#include "ShaderManager.hpp"
#include "StringInterner.hpp"
#include "optick.h"

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
    ImguiRenderingModule imguiModule{&config, &renderer};
    GizmoRenderingModule gizmoRenderingModule{
        &config, &renderer, &shaderManager, &world, &interner};
    
    Input input(&camera, window, &world, &config, &gizmoRenderingModule);

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

      shaderManager.update();

      auto imageIndex = renderer.prepareDrawFrame();

      if (imageIndex == std::numeric_limits<u32>::max()) {
        continue;
      }

      blockRenderingModule.drawFrame(renderer.getFrameBuffer(imageIndex),
                                     deltaTime, cameraMoved,
                                     camera.getPosition());
      imguiModule.drawFrame(
          renderer.getFrameBuffer(imageIndex), deltaTime,
          blockRenderingModule.getRenderingFinishedSemaphore());
      gizmoRenderingModule.drawFrame(
          renderer.getFrameBuffer(imageIndex), deltaTime,
          imguiModule.getRenderingFinishedSemaphore());

      renderer.finishDrawFrame(
          gizmoRenderingModule.getRenderingFinishedSemaphore(), imageIndex);
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

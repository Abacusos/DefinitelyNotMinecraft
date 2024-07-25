#include "DefinitelyNotMinecraft.hpp"

#include <Core/Chrono.hpp>
#include <Core/Config.hpp>
#include <Core/Profiler.hpp>
#include <Core/StringInterner.hpp>

#include <Logic/Camera.hpp>
#include <Logic/Imgui.hpp>
#include <Logic/Input.hpp>

#include <Rendering/RenderGraph.hpp>
#include <Rendering/Renderer.hpp>
#include <RenderingNodes/BlockRenderingNode.hpp>
#include <RenderingNodes/ForwardRenderingNode.hpp>
#include <RenderingNodes/GizmoRenderingNode.hpp>
#include <RenderingNodes/ImguiRenderingNode.hpp>

#include <Shader/ShaderManager.hpp>

int main(int /*argc*/, char** /*argv*/) {
    using namespace dnm;
    try {
        Config         config;
        StringInterner interner;
        ShaderManager  shaderManager {&interner};
        Renderer       renderer {&config};
        auto*          window = renderer.getGLFWwindow();
        Camera         camera(&config, &renderer);
        BlockWorld     world {&config};
        Imgui          imgui(&config, window);
        RenderGraph    graph {&renderer, &camera};
        GizmoData      gizmoData;
        Input          input(&camera, window, &world, &config, &gizmoData);

        graph.registerNode(std::make_unique<BlockDrawCallNode>(&config, &renderer, &shaderManager, &world, &interner));
        graph.registerNode(std::make_unique<ForwardRenderingNode>(&config, &renderer, &shaderManager, &world, &interner));
        graph.registerNode(std::make_unique<ImguiRenderingNode>(&config, &renderer));
        graph.registerNode(std::make_unique<GizmoRenderingNode>(&config, &renderer, &shaderManager, &world, &interner, &gizmoData));

        auto lastFrame = std::chrono::system_clock::now();

        while (!glfwWindowShouldClose(window)) {
            FrameMark;
            ZoneScopedN("Main");
            auto now       = std::chrono::system_clock::now();
            auto deltaTime = std::chrono::duration_cast<TimeSpan>(now - lastFrame);
            lastFrame      = now;

            input.processInput(window, deltaTime);
            camera.update(deltaTime);
            {
                ZoneScopedN("GLFW Event Polling");
                glfwPollEvents();
            }

            imgui.logicFrame(deltaTime, &camera);

            shaderManager.update();

            auto preparedFrame = graph.prepareFrame();

            if (!preparedFrame) {
                continue;
            }

            graph.build();
            graph.execute();

            if (config.limitFrames) {
                using namespace std::chrono_literals;
                constexpr auto targetFrameRate = std::chrono::milliseconds(16ms);
                auto           deltaTime       = (std::chrono::system_clock::now() - now);
                if (deltaTime < targetFrameRate) {
                    std::this_thread::sleep_for(targetFrameRate - deltaTime);
                }
            }
        }

        renderer.waitIdle();
    }
    catch (vk::SystemError& err) {
        std::cout << "vk::SystemError: " << err.what() << std::endl;
        exit(-1);
    }
    catch (std::exception& err) {
        std::cout << "std::exception: " << err.what() << std::endl;
        exit(-1);
    }
    catch (...) {
        std::cout << "unknown error\n";
        exit(-1);
    }
    return 0;
}

# Add source to this project's executable.
add_executable (DefinitelyNotMinecraft "DefinitelyNotMinecraft.cpp" "DefinitelyNotMinecraft.hpp" "Core/Math.cpp" "Rendering/RAIIUtils.cpp" "Rendering/Renderer.cpp" "Logic/Input.cpp" "Logic/Camera.cpp" "RenderingModule/BlockRenderingModule.cpp" "RenderingModule/ForwardRenderingModule.cpp"   "Logic/BlockWorld.cpp" "RenderingModule/ImguiRenderingModule.cpp" "RenderingModule/GizmoRenderingModule.cpp" "Logic/Imgui.cpp"  "Shader/ShaderReflector.cpp" "Shader/ShaderManager.cpp")

target_include_directories(DefinitelyNotMinecraft PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

set_property(TARGET DefinitelyNotMinecraft PROPERTY CXX_STANDARD 20)

find_package(Vulkan REQUIRED shaderc_combined)

target_link_libraries(DefinitelyNotMinecraft PRIVATE Vulkan::Vulkan)
target_link_libraries(DefinitelyNotMinecraft PRIVATE Vulkan::shaderc_combined)
target_link_libraries(DefinitelyNotMinecraft PRIVATE glfw)
target_link_libraries(DefinitelyNotMinecraft PRIVATE stb)
target_link_libraries(DefinitelyNotMinecraft PRIVATE perlinnoise)
target_link_libraries(DefinitelyNotMinecraft PRIVATE imgui)
target_link_libraries(DefinitelyNotMinecraft PRIVATE TracyClient)
 
add_definitions(-DNOMINMAX -DVK_USE_PLATFORM_WIN32_KHR)

foreach(Library IN LISTS VULKAN_LIB_LIST)
    target_link_libraries(DefinitelyNotMinecraft PRIVATE debug ${VULKAN_LIB_PATH}/${Library}d.lib optimized ${VULKAN_LIB_PATH}/${Library}.lib )
endforeach()

set(TARGET_SHADER_DIRECTORY ${CMAKE_BINARY_DIR}/DefinitelyNotMinecraft/Shaders)
file(MAKE_DIRECTORY ${TARGET_SHADER_DIRECTORY})
set(SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Shaders)
set(SHADER_LIST "World.vert" "World.frag" "Gizmo.vert" "Gizmo.frag" "DrawCallGenerationWorld.comp" "BlockWorldBuffer.glsl" "BlockWorldUtil.glsl" "CameraBuffer.glsl")

foreach(Shader IN LISTS SHADER_LIST)
    configure_file(${SHADER_SOURCE_DIR}/${Shader} ${TARGET_SHADER_DIRECTORY}/${Shader} COPYONLY)
endforeach() 

set(TARGET_TEXTURE_DIRECTORY ${CMAKE_BINARY_DIR}/DefinitelyNotMinecraft/Textures)
file(MAKE_DIRECTORY ${TARGET_TEXTURE_DIRECTORY})
set(TEXTURES_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Textures)
set(TEXTURES_LIST "TextureSheet.png")

foreach(Texture IN LISTS TEXTURES_LIST)
    configure_file(${TEXTURES_SOURCE_DIR}/${Texture} ${TARGET_TEXTURE_DIRECTORY}/${Texture} COPYONLY)
endforeach()
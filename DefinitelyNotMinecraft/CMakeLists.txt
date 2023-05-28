# Add source to this project's executable.
add_executable (DefinitelyNotMinecraft "DefinitelyNotMinecraft.cpp" "DefinitelyNotMinecraft.hpp"   "Rendering/RAIIUtils.cpp" "Rendering/Renderer.cpp" "Logic/Input.cpp" "Logic/Camera.cpp" "RenderingModule/BlockRenderingModule.cpp"   "Logic/BlockWorld.cpp" "RenderingModule/ImguiRenderingModule.cpp" "Logic/Imgui.cpp"  "Shader/ShaderReflector.cpp" "Shader/ShaderManager.cpp")

set(FOLDERS "Core" "Logic" "Rendering" "RenderingModule" "Shader")
foreach(Folder IN LISTS FOLDERS)
    target_include_directories(DefinitelyNotMinecraft PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/${Folder})
endforeach()

set_property(TARGET DefinitelyNotMinecraft PROPERTY CXX_STANDARD 20)

find_package(Vulkan REQUIRED glslang glslc SPIRV-Tools)

target_link_libraries(DefinitelyNotMinecraft PRIVATE Vulkan::Vulkan)
target_link_libraries(DefinitelyNotMinecraft PRIVATE Vulkan::glslang)
target_link_libraries(DefinitelyNotMinecraft PRIVATE Vulkan::SPIRV-Tools)
target_link_libraries(DefinitelyNotMinecraft PRIVATE glfw)
target_link_libraries(DefinitelyNotMinecraft PRIVATE stb)
target_link_libraries(DefinitelyNotMinecraft PRIVATE perlinnoise)
target_link_libraries(DefinitelyNotMinecraft PRIVATE imgui)
target_link_libraries(DefinitelyNotMinecraft PRIVATE OptickCore)
 
add_definitions(-DNOMINMAX -DVK_USE_PLATFORM_WIN32_KHR)

# For some reason these libs are not linked against and I get compile errors if I don't link manually
set(VULKAN_LIB_LIST "SPIRV-Tools-opt;glslang-default-resource-limits")
set(VULKAN_LIB_PATH  ${Vulkan_INCLUDE_DIRS})
STRING(REGEX REPLACE "/Include" "/Lib" VULKAN_LIB_PATH  ${VULKAN_LIB_PATH})

foreach(Library IN LISTS VULKAN_LIB_LIST)
    target_link_libraries(DefinitelyNotMinecraft PRIVATE debug ${VULKAN_LIB_PATH}/${Library}d.lib optimized ${VULKAN_LIB_PATH}/${Library}.lib )
endforeach()

set(TARGET_SHADER_DIRECTORY ${CMAKE_BINARY_DIR}/DefinitelyNotMinecraft/Shaders)
file(MAKE_DIRECTORY ${TARGET_SHADER_DIRECTORY})
set(SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Shaders)
set(SHADER_LIST "World.vert" "World.frag" "DrawCallGenerationWorld.comp")

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

FetchContent_GetProperties(
  optick
  SOURCE_DIR OptickSourceDir
)

if (WIN32)
# copy the .dll file to the same folder as the executable
add_custom_command(
    TARGET DefinitelyNotMinecraft POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    $<TARGET_FILE_DIR:OptickCore>
    $<TARGET_FILE_DIR:DefinitelyNotMinecraft>)
endif()
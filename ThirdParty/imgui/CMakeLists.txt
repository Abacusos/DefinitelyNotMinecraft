include(FetchContent)
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui
  GIT_TAG        9308cfdcfb83c74137f2996c7fc8c2d043436ba5 
  CONFIGURE_COMMAND ""
  BUILD_COMMAND "" 
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC)
target_sources(imgui 
PRIVATE 
${imgui_SOURCE_DIR}/imgui_demo.cpp
${imgui_SOURCE_DIR}/imgui_draw.cpp
${imgui_SOURCE_DIR}/imgui_tables.cpp
${imgui_SOURCE_DIR}/imgui_widgets.cpp
${imgui_SOURCE_DIR}/imgui.cpp
${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui PUBLIC 
${imgui_SOURCE_DIR}
${imgui_SOURCE_DIR}/backends)

find_package(Vulkan REQUIRED)

target_link_libraries(imgui PRIVATE glfw)
target_link_libraries(imgui PRIVATE Vulkan::Vulkan)
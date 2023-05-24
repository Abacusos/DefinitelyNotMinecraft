include(FetchContent)
FetchContent_Declare(
  optick
  GIT_REPOSITORY https://github.com/bombomby/optick
  GIT_TAG        8abd28dee1a4034c973a3d32cd1777118e72df7e 
)
set(OPTICK_ENABLED ON CACHE BOOL "" FORCE)
set(OPTICK_USE_VULKAN ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(optick) 
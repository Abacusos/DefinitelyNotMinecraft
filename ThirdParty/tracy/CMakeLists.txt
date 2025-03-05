include(FetchContent)
FetchContent_Declare(
  tracy
  GIT_REPOSITORY https://github.com/wolfpld/tracy
  GIT_TAG        5d542dc09f3d9378d005092a4ad446bd405f819a 
  GIT_SHALLOW    TRUE
  GIT_PROGRESS   TRUE
)
set(TRACY_ENABLE ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(tracy) 
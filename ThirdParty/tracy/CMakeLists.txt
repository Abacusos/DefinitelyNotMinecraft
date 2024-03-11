include(FetchContent)
FetchContent_Declare(
  tracy
  GIT_REPOSITORY https://github.com/wolfpld/tracy
  GIT_TAG        37aff70dfa50cf6307b3fee6074d627dc2929143 
  GIT_SHALLOW    TRUE
  GIT_PROGRESS   TRUE
)
set(TRACY_ENABLE ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(tracy) 
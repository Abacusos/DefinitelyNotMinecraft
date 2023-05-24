include(FetchContent)
FetchContent_Declare(
  perlinnoise
  GIT_REPOSITORY https://github.com/Reputeless/PerlinNoise
  GIT_TAG        bdf39fe92b2a585cdef485bcec2bca8ab5614095 
  CONFIGURE_COMMAND ""
  BUILD_COMMAND "" 
)
FetchContent_MakeAvailable(perlinnoise)

add_library(perlinnoise INTERFACE)
target_include_directories(perlinnoise INTERFACE ${perlinnoise_SOURCE_DIR})
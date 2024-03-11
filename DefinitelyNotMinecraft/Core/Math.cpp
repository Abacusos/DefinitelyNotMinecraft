#include <Core/Math.hpp>

namespace dnm {
m4 createProjectionMatrix(const v2& extent, float n, float f) {
  // TODO Figure out why this is necessary
  std::swap(n, f);
  constexpr float fov = glm::radians(60.0f);
  float aspect = extent.x / (float)extent.y;

  constexpr float fov_rad = 45.0f * 2.0f * glm::pi<float>() / 360.0f;
  float focal_length = 1.0f / std::tan(fov_rad / 2.0f);

  float x = focal_length / aspect;
  float y = -focal_length;
  const auto a = n / (f - n);
  const auto b = f * a;

  m4 projection{
      x,    0.0f, 0.0f, 0.0f,  
      0.0f, y,    0.0f, 0.0f,
      0.0f, 0.0f, a,    -1.0f, 
      0.0f, 0.0f, b,    0.0f,
  };

  return projection;
}

}  // namespace dnm

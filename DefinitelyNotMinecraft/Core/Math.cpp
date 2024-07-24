#include <Core/Math.hpp>

namespace dnm {
m4 createProjectionMatrix(const v2 &extent, float n, float f) {
  std::swap(n, f);
  constexpr float fov = glm::radians(60.0f);
  const float aspect = extent.x / (float)extent.y;

  constexpr float fov_rad = 45.0f * 2.0f * glm::pi<float>() / 360.0f;
  const float focalLength = 1.0f / std::tan(fov_rad / 2.0f);

  const float x = focalLength / aspect;
  const float y = -focalLength;
  const auto a = n / (f - n);
  const auto b = f * a;

  const m4 projection{
      x,    0.0f, 0.0f, 0.0f,  0.0f, y,    0.0f, 0.0f,
      0.0f, 0.0f, a,    -1.0f, 0.0f, 0.0f, b,    0.0f,
  };

  return projection;
}
} // namespace dnm

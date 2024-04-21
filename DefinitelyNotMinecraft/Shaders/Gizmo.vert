#include "Shaders/CameraBuffer.glsl"

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 color;

layout (location = 0) out vec3 outColor;

void main()
{
  outColor = color;
  gl_Position = (projection * view) * vec4(pos, 1.0f);
}

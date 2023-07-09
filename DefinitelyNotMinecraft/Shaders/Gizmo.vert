
#extension GL_EXT_shader_8bit_storage: require

layout (std140, binding = 0) uniform projectionBuffer
{
  mat4 projection;
};

layout (std140, binding = 1) uniform viewBuffer
{
  mat4 view;
  vec4 cameraPos;
};

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 color;

layout (location = 0) out vec3 outColor;

void main()
{
  outColor = color;
  gl_Position = (projection * view) * vec4(pos, 1.0f);
}

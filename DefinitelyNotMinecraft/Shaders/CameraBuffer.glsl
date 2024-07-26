layout (std140, binding = ) uniform projectionBuffer
{
  mat4 projection;
};

layout (std140, binding = ) uniform viewBuffer
{
  mat4 view;
  vec4 cameraPos;
};

#extension GL_EXT_shader_8bit_storage: require

layout (std140, binding = 0) uniform projectionBuffer
{
  mat4 projection;
};

layout (std140, binding = 1) uniform viewBuffer
{
  mat4 view;
};

layout (std140, binding = 2) readonly buffer transformBuffer
{
  mat4 transforms[];
};

layout (binding = 3) readonly buffer blockTypeBuffer
{
  uint8_t blockType[];
};

layout (location = 0) in vec3 pos;
layout (location = 1) in vec2 inTexCoord;

layout (location = 0) out vec2 outTexCoord;

void main()
{
  outTexCoord = inTexCoord;
  outTexCoord.y += uint(blockType[gl_InstanceIndex]) * 0.083333f;
  gl_Position = (projection * view * transforms[gl_InstanceIndex]) * vec4(pos, 1.0f);
}

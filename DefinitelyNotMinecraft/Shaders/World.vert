
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

layout (std140, binding = 2) readonly buffer transformBuffer
{
  vec4 transforms[];
};

layout (std430, binding = 3) readonly buffer blockTypeBuffer
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
  mat4 modelMatrix = mat4(1);
  modelMatrix[3].xyzw = transforms[gl_InstanceIndex]; 
  gl_Position = (projection * view * modelMatrix) * vec4(pos, 1.0f);
}

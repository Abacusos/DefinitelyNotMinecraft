
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (binding = 7) uniform sampler2D tex;

layout (location = 0) in vec2 inTexCoord;

layout (location = 0) out vec4 outColor;

void main()
{
  outColor = texture(tex, inTexCoord);
  //outColor = vec4(inTexCoord.yy, 0.0f, 1.0f);
}
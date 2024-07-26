
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "Shaders/CameraBuffer.glsl"

layout (std140, binding = ) uniform lightBuffer
{
  vec3 lightColor;  
  float ambientStrength;
  vec3 lightPosition;
  float specularStrength;

};

layout (binding = ) uniform sampler2D tex;

layout (location = 0) in vec2 inTexCoord;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inPosition;
layout (location = 3) in vec3 inWSPos;

layout (location = 0) out vec4 outColor;

void main()
{
    vec3 lightDir = lightPosition - inWSPos;
    vec3 lightDirNormalized = normalize(lightDir);  
      
    vec3 ambient = ambientStrength * lightColor;

    float diff = max(dot(inNormal, lightDirNormalized), 0.0);
    diff = diff * (1.0f - smoothstep(0.0f, 200.0f, length(lightDir)));
    vec3 diffuse = diff * lightColor;

    vec3 viewDirection = normalize(cameraPos.xyz - inWSPos);
    vec3 reflectDirection = reflect(-lightDirNormalized, inNormal);
    float spec = pow(max(dot(viewDirection, reflectDirection), 0.0f), 16);
    vec3 specular = specularStrength * spec * lightColor;  

    outColor = vec4((ambient + diffuse + specular) * (texture(tex, inTexCoord)).xyz, 1.0f);
}
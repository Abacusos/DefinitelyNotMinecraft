
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "Shaders/CameraBuffer.glsl"

layout (std140, binding = ) uniform lightConstants
{
  int lightCount;
  int specularPow;
  float smoothstepMax;
  float ambientStrength;
  float specularStrength;
};

struct PerLightData
{
  vec3 lightColor;  
  vec3 lightPosition;
};

layout (std140, binding = ) buffer perLightBuffer
{
  PerLightData perLightData[];
};

layout (binding = ) uniform sampler2D tex;

layout (location = 0) in vec2 inTexCoord;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inPosition;
layout (location = 3) in vec3 inWSPos;

layout (location = 0) out vec4 outColor;

void main()
{

    vec3 finalResult = vec3(0);

    for(int i = 0; i < lightCount; ++i)
    {
        vec3 lightDir = perLightData[i].lightPosition - inWSPos;
        vec3 lightDirNormalized = normalize(lightDir);  
        
        float diff = max(dot(inNormal, lightDirNormalized), 0.0);
        diff = diff * (1.0f - smoothstep(0.0f, 200.0f, length(lightDir)));
      
        vec3 viewDirection = normalize(cameraPos.xyz - inWSPos);
        vec3 reflectDirection = reflect(-lightDirNormalized, inNormal);
        float spec = pow(max(dot(viewDirection, reflectDirection), 0.0f), specularPow);

        vec3 ambient = ambientStrength * perLightData[i].lightColor;
        vec3 diffuse = diff * perLightData[i].lightColor;
        vec3 specular = specularStrength * spec * perLightData[i].lightColor;  

        finalResult += ambient + diffuse + specular;
    }

    outColor = vec4(finalResult * (texture(tex, inTexCoord)).xyz, 1.0f);
}
#extension GL_EXT_shader_8bit_storage: require

#include "Shaders/CameraBuffer.glsl"

layout (std140, binding = 2) readonly buffer transformBuffer
{
  vec4 transforms[];
};

struct BlockData{
    uint index;
    uint8_t blockType;
    uint8_t visibleFace;
    uint8_t padding;
    uint8_t padding2;
};
layout (std430, binding = 3) readonly buffer blockTypeBuffer
{
  BlockData blockData[];
};

const vec3 vertices[36] = vec3[](
    vec3(-0.5f, -0.5f, -0.5f),
    vec3(-0.5f, -0.5f, 0.5f),
    vec3(-0.5f, 0.5f, 0.5f),
    vec3(-0.5f, 0.5f, 0.5f),
    vec3(-0.5f, 0.5f, -0.5f),
    vec3(-0.5f, -0.5f, -0.5),

    vec3(0.5f, 0.5f, 0.5f),
    vec3(-0.5f, 0.5f, 0.5f),
    vec3(-0.5f, -0.5f, 0.5f),
    vec3(-0.5f, -0.5f, 0.5f),
    vec3(0.5f, -0.5f, 0.5f),
    vec3(0.5f, 0.5f, 0.5f),

    vec3(-0.5f, 0.5f, -0.5f),
    vec3(0.5f, 0.5f, 0.5f),
    vec3(0.5f, 0.5f, -0.5f),
    vec3(-0.5f, 0.5f, -0.5f),
    vec3(-0.5f, 0.5f, 0.5f),
    vec3(0.5f, 0.5f, 0.5f),

    vec3(-0.5f, -0.5f, -0.5f),
    vec3(0.5f, -0.5f, 0.5f),
    vec3(-0.5f, -0.5f, 0.5f),
    vec3(-0.5f, -0.5f, -0.5f),
    vec3(0.5f, -0.5f, -0.5f),
    vec3(0.5f, -0.5f, 0.5f),

    vec3(0.5f, 0.5f, -0.5f),
    vec3(0.5f, 0.5f, 0.5f),
    vec3(0.5f, -0.5f, 0.5f),
    vec3(0.5f, -0.5f, 0.5f),
    vec3(0.5f, -0.5f, -0.5f),
    vec3(0.5f, 0.5f, -0.5f),

    vec3(0.5f, 0.5f, -0.5f),
    vec3(-0.5f, -0.5f, -0.5f),
    vec3(-0.5f, 0.5f, -0.5f),
    vec3(-0.5f, -0.5f, -0.5f),
    vec3(0.5f, 0.5f, -0.5f),
    vec3(0.5f, -0.5f, -0.5f)
);

const float oneSixth = 1.0f / 12.0f;
const float twoSixth = 2.0f / 12.0f;
const float threeSixth = 3.0f / 12.0f;
const float fourSixth = 4.0f / 12.0f;
const float fiveSixth = 5.0f / 12.0f;
const float sixSixth = 6.0f / 12.0f;

const float oneTwelveth = 1.0f / 12.0f;

const vec2 uvs[36] =  vec2[]
(
    vec2(twoSixth, oneTwelveth),
    vec2(oneSixth, oneTwelveth),
    vec2(oneSixth, 0.0f),
    vec2(oneSixth, 0.0f),
    vec2(twoSixth, 0.0f),
    vec2(twoSixth, oneTwelveth),

    vec2(threeSixth, 0.0f),
    vec2(twoSixth, 0.0f),
    vec2(twoSixth, oneTwelveth),
    vec2(twoSixth, oneTwelveth),
    vec2(threeSixth, oneTwelveth),
    vec2(threeSixth, 0.0f),

    vec2(fiveSixth, 0.0f),
    vec2(sixSixth, oneTwelveth),
    vec2(sixSixth, 0.0f),
    vec2(fiveSixth, 0.0f),
    vec2(fiveSixth, oneTwelveth),
    vec2(sixSixth, oneTwelveth),

    vec2(0.0f, oneTwelveth),
    vec2(oneSixth, 0.0f),
    vec2(0.0f, 0.0f),
    vec2(0.0f, oneTwelveth),
    vec2(oneSixth, oneTwelveth),
    vec2(oneSixth, 0.0f),

    vec2(threeSixth, 0.0f),
    vec2(fourSixth, 0.0f),
    vec2(fourSixth, oneTwelveth),
    vec2(fourSixth, oneTwelveth),
    vec2(threeSixth, oneTwelveth),
    vec2(threeSixth, 0.0f),

    vec2(fourSixth, 0.0f),
    vec2(fiveSixth, oneTwelveth),
    vec2(fiveSixth, 0.0f),
    vec2(fiveSixth, oneTwelveth),
    vec2(fourSixth, 0.0f),
    vec2(fourSixth, oneTwelveth)
);

const vec3 normals[6] =  vec3[]
(
    vec3(-1.0f, 0.0f, 0.0f),
    vec3(0.0f, 0.0f, 1.0f),
    vec3(0.0f, 1.0f, 0.0f),    
    vec3(0.0f, -1.0f, 0.0f),
    vec3(1.0f, 0.0f, 0.0f),
    vec3(0.0f, 0.0f, -1.0f)
);

layout (location = 0) out vec2 outTexCoord;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec3 outPosition;
layout (location = 3) out vec3 outWSPos;

void main()
{
  uint faceIndex = uint(gl_VertexIndex / int(6));
  uint vertex = uint(gl_VertexIndex % int(6));
  uint faceDirection = uint(blockData[faceIndex].visibleFace);
  uint blockIndex = blockData[faceIndex].index;
  uint blockType = uint(blockData[faceIndex].blockType);
  uint index = faceDirection * 6 + vertex;

  outTexCoord = uvs[index];
  outTexCoord.y += 0.083333f * float(blockType);
  mat4 modelMatrix = mat4(1);
  modelMatrix[3].xyz = transforms[blockIndex].xyz; 
  gl_Position = (projection * view * modelMatrix) * vec4(vertices[index], 1.0f);
  outNormal = normals[faceDirection];
  outPosition = gl_Position.xyz;
  outWSPos = (modelMatrix * vec4(vertices[index], 1.0f)).xyz;
}

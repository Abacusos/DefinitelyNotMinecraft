
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

vec3 getVertex(int face, int vertex)
{
    return vertices[face * 6 + vertex];
}

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

vec2 getUV(int face, int vertex)
{
    return uvs[face * 6 + vertex];
}

layout (location = 0) out vec2 outTexCoord;

void main()
{
  int faceIndex = int(gl_VertexIndex / int(6));
  int vertex = int(gl_VertexIndex % int(6));
  int faceDirection = int(blockData[faceIndex].visibleFace);

  outTexCoord = getUV(faceDirection, vertex);
  outTexCoord.y += uint(blockData[faceIndex].blockType) * 0.083333f;
  mat4 modelMatrix = mat4(1);
  modelMatrix[3].xyz = transforms[blockData[faceIndex].index].xyz; 
  gl_Position = (projection * view * modelMatrix) * vec4(getVertex(faceDirection, vertex), 1.0f);
}

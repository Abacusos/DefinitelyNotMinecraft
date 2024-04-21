struct DrawCall
{
    uint vertexCount;
    uint instanceCount;
    uint firstVertex;
    uint firstInstance;
    uint globalIndexTransform;
    uint globalIndexFace;
};

layout (std140, binding = 2) buffer transformBuffer
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

layout (std430, binding = 3) buffer blockTypeBuffer
{
    BlockData blockData[];
};

layout (std430, binding = 4) readonly buffer worldDataBuffer
{
    uint8_t blockTypeWorld[];
};

layout (binding = 5) buffer drawCallBuffer
{
    DrawCall drawcall;
};

layout (binding = 6) readonly uniform chunkConstants
{
    int chunkLoadCount;
    int chunksOneDimension;
    int chunkLocalSize;
    int chunkHeight;
};

layout (binding = 7) readonly buffer chunkIndexRemap
{
    uint remapIndex[];
};

struct Plane
{
    vec3 normal;
    float dist;
};

layout (binding = 8) readonly uniform cullingData
{
	Plane planes[6];
	uint cullingEnabled;
};
struct DrawCall
{
    uint vertexCount;
    uint instanceCount;
    uint firstVertex;
    uint firstInstance;
    uint globalIndexTransform;
    uint globalIndexFace;
};

layout (std140, binding = ) buffer transformBuffer
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

layout (std430, binding = ) buffer blockTypeBuffer
{
    BlockData blockData[];
};

layout (std430, binding = ) readonly buffer worldDataBuffer
{
    uint16_t blockTypeWorld[];
};

layout (binding = ) buffer drawCallBuffer
{
    DrawCall drawcall;
};

layout (binding = ) readonly uniform chunkConstants
{
    int chunkLoadCount;
    int chunksOneDimension;
    int chunkLocalSize;
    int chunkHeight;
};

layout (binding = ) readonly buffer chunkIndexRemap
{
    uint remapIndex[];
};

struct Plane
{
    vec3 normal;
    float dist;
};

layout (binding = ) readonly uniform cullingData
{
	Plane planes[6];
	uint cullingEnabled;
};
const uint air = uint(65535);

vec3 indexToPos()
{
  int remap = int(remapIndex[gl_WorkGroupID.x]);
  int remapOffsetX = (remap % chunksOneDimension) - chunkLoadCount;
  int remapOffsetZ = (remap / chunksOneDimension) - chunkLoadCount;
  int chunkStartX = int(chunkLocalSize * (remapOffsetX + int(cameraPos.x / chunkLocalSize)));
  int chunkStartZ = int(chunkLocalSize * (remapOffsetZ + int(cameraPos.z / chunkLocalSize)));
  int x = int(gl_LocalInvocationID.x) + chunkStartX;
  int z = int(gl_LocalInvocationID.z) + chunkStartZ;

  return vec3(x, gl_WorkGroupID.y, z);
}

bool isInsideChunk(ivec3 position)
{
    return position.y >= 0 && position.y < chunkHeight && position.x >= 0 && position.x < chunkLocalSize && position.z >= 0 && position.z < chunkLocalSize;
}

uint toFlatIndex(ivec3 position)
{
  int remap = int(remapIndex[gl_WorkGroupID.x]);
  int chunkStartX = (remap % chunksOneDimension);
  int chunkStartZ = (remap / chunksOneDimension);
  
  int heightOffset = chunkLocalSize * chunkLocalSize * position.y;
  
  int sizeOfChunk = chunkLocalSize * chunkLocalSize * chunkHeight;
  int offsetPreviousChunk = chunkStartX * sizeOfChunk + chunkStartZ * chunksOneDimension * sizeOfChunk;
   
   // Convert any world space positions into chunk local positions
  int inLayerOffset = (position.z % chunkLocalSize) * chunkLocalSize + (position.x % chunkLocalSize);

  return offsetPreviousChunk + heightOffset + inLayerOffset;
}

float getSignedDistanceToPlane(const vec3 point, const Plane plane)
{
    return dot(plane.normal, point) - plane.dist;
}

bool isVisible(vec3 center, float extent)
{
	bool visible = true;

    for(int i = 0; i < 6; ++i)
    {	
        const float r = dot(abs(planes[i].normal), vec3(extent));
        visible = visible && -r <= getSignedDistanceToPlane(center, planes[i]);
    }

	visible = visible || cullingEnabled == 0;
    return visible;
}

const ivec3 direction[6] = ivec3[](
    ivec3(-1, 0, 0),
    ivec3(0, 0, 1),
    ivec3(0, 1, 0),
    ivec3(0, -1, 0),
    ivec3(1, 0, 0),
    ivec3(0, 0, -1)
);

// Reduce the number of visible faces to avoid z fighting
uint[6] getVisibleFaces(in ivec3 center, out uint faceCount)
{
    faceCount = 0;

    uint[6] result;
    uint blockType = uint(blockTypeWorld[toFlatIndex(center)]);

    for(int i = 0; i < 6; ++i)
    {
        if((blockType & uint(1) << (16 - 6 + i)) != 0u)
        {
            result[faceCount] = i;
            ++faceCount;
        }
    }

    return result;
}
#pragma once
#include "cl.hpp"
#include "Math/Vector.hpp"
#include "Common.hpp"

struct Texture
{
	int width, height, offset, padd;
};

AX_ALIGNED(16) struct BVHNode
{
	struct { float3 aabbMin; uint leftFirst; };
	struct { float3 aabbMax; uint triCount; };
	bool isLeaf() const { return triCount > 0; }
};

struct Mesh {
	int triangleStart, numTriangles;
};

struct Tri { 
	float3 vertex0; float padd; 
	float3 vertex1; float padd1; 
	float3 vertex2; float padd3; 
};

typedef int TextureHandle;
typedef int MeshHandle;

namespace ResourceManager
{
	TextureHandle ImportTexture(const char* path);
	MeshHandle ImportMesh(const char* path);

	inline TextureHandle GetWhiteTexture() { return 0; }
	inline TextureHandle GetBlackTexture() { return 1; }
	
	cl_mem* GetTextureHandleMem();
	cl_mem* GetTextureDataMem();
	cl_mem* GetMeshTriangleMem();
	cl_mem* GetBVHMem();
	cl_mem* GetMeshesMem();

	void PrepareMeshes();
	void PushMeshesToGPU(cl_context context);
	ushort GetNumMeshes();

	void Initialize();
	void Destroy();

	void PushTexturesToGPU(cl_context context);
}


#pragma once
#include "cl.hpp"
#include "Math/Vector.hpp"
#include "Common.hpp"

struct Texture
{
	int width, height, offset, padd;
};

struct BVHNode
{
	struct { float3 aabbMin; uint leftFirst; };
	struct { float3 aabbMax; uint triCount; };
	bool isLeaf() const { return triCount > 0; }
};

struct Mesh {
	uint bvhIndex, numTriangles, triangleStart, padd;
	float3 position; float padd1;
};

struct Material {
	uint color, textureIndex;
	float roughness, padd1;
};

#pragma pack(1)
typedef struct Sphere_t
{
	float position[3];
	float radius;
	float roughness;
	unsigned color;
	float rotationx, rotationy;
} Sphere;

struct Tri { 
	float3 vertex0; float centeroidx; 
	float3 vertex1; float centeroidy; 
	float3 vertex2; float centeroidz; 
};

typedef uint TextureHandle;
typedef uint MeshHandle;

namespace ResourceManager
{
	TextureHandle ImportTexture(const char* path);
	MeshHandle ImportMesh(const char* path);

	inline constexpr TextureHandle GetWhiteTexture() { return 0; }
	inline constexpr TextureHandle GetBlackTexture() { return 1; }
	inline constexpr Material DefaultMaterial() {
		Material material;
		material.color = ~0;// white
		material.roughness = 0.5f;
		material.textureIndex = 0;// white texture
		return material;
	}
	
	void SetMeshPosition(MeshHandle handle, float3 point);

	void PrepareTextures();
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
	void* GetAreaPointer(); // unsafe
}


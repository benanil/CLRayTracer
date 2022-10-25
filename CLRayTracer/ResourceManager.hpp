#pragma once
#include "cl.hpp"
#include "Math/Vector.hpp"
#include "Common.hpp"

struct Texture {
	int width, height, offset, padd;
};

AX_ALIGNED(16) struct BVHNode
{
	struct { float3 aabbMin; uint leftFirst; };
	struct { float3 aabbMax; uint triCount; };
	bool isLeaf() const { return triCount > 0; }
};

struct MeshInfo {
	uint numTriangles; 
	uint triangleStart; 
	ushort materialStart; 
	ushort numMaterials; 
};

// this material will store default mesh/submesh
// values and we will be able to change properties for different mesh instances
struct Material {
	uint color; 
	uint albedoTextureIndex;
	uint specularTextureIndex;  
	float shininess;
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

AX_ALIGNED(16) struct Tri { 
	float3 vertex0; float centeroidx; 
	float3 vertex1; float centeroidy; 
	float3 vertex2; float centeroidz; 

	half uv0x, uv0y;
	half uv1x, uv1y;
	half uv2x, uv2y;
	int padd;
};

static_assert(sizeof(Tri) == 64);

typedef ushort TextureHandle;
typedef ushort MeshHandle;
typedef ushort MaterialHandle;

// this is same for each resource type:
//     the idea is:
//         create big chunk of data pool on GPU
//         create resources
//         push to gpu

namespace ResourceManager
{
	TextureHandle ImportTexture(const char* path);
	MeshHandle ImportMesh(const char* path);

	constexpr TextureHandle  WhiteTexture = 0;
	constexpr TextureHandle  BlackTexture = 1;
	constexpr MaterialHandle NoneMaterial = 0;
	constexpr MaterialHandle DefaultMaterial = 0xFFFF;

	void PrepareTextures();

	void GetGPUMemories(
		cl_mem* textureHandleMem,
		cl_mem* textureDataMem  ,
		cl_mem* meshTriangleMem ,
		cl_mem* bvhMem          ,
		cl_mem* meshesMem       ,
		cl_mem* materialMem     
	);

	// manipulate returning material's properties and use handle for other things, REF handle
	// for using with multiple submeshes you need to set count to number of submeshes
	Material* CreateMaterial(MaterialHandle* handle, int count = 1);

	void PrepareMeshes();
	void PushMeshesToGPU(cl_command_queue queue);
	void PushMaterialsToGPU(cl_command_queue queue);
	ushort GetNumMeshes();

	MeshInfo GetMeshInfo(MeshHandle handle);

	void Initialize(cl_context context);
	void Destroy();
	void Finalize();

	void PushTexturesToGPU(cl_command_queue context);
	void* GetAreaPointer(); // unsafe
}


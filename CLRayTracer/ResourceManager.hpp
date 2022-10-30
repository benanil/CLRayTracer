#pragma once
#include "cl.hpp"
#include "Math/Vector.hpp"
#include "Common.hpp"
#include <immintrin.h>

struct Texture {
	int width, height, offset, padd;
};

AX_ALIGNED(16) struct BVHNode
{
	union { struct { float3 aabbMin; uint leftFirst; }; __m128 minv; };
	union { struct { float3 aabbMax; uint triCount; };  __m128 maxv; };
};

struct MeshInfo {
	uint numTriangles; 
	uint triangleStart; 
	ushort materialStart; 
};

// this material will store default mesh/submesh
// values and we will be able to change properties for different mesh instances
#pragma pack(1)
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
	union { struct {float3 vertex0; float centeroidx;};  __m128 v0; };
	union { struct {float3 vertex1; float centeroidy;};  __m128 v1; };
	union { struct {float3 vertex2; float centeroidz;};  __m128 v2; };

	half uv0x, uv0y;
	half uv1x, uv1y;
	half uv2x, uv2y;
	int materialIndex;
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
//         flush cpu memory and fill it again

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


#pragma once
#include "cl.hpp"
#include "Math/Vector.hpp"
#include "Common.hpp"
#include <immintrin.h>

AX_ALIGNED(16) struct BVHNode
{
	union { struct { float3 aabbMin; uint leftFirst; }; __m128 minv; };
	union { struct { float3 aabbMax; uint triCount; };  __m128 maxv; };
};

#pragma pack(push)
struct RGB8 {
	unsigned char r, g, b;
};
#pragma pack(pop)

struct MeshInfo {
	uint numTriangles; 
	uint triangleStart; 
	ushort materialStart; 
	ushort numMaterials;
	const char* path;
};

struct Texture {
	int width, height, offset, padd;
};

struct TextureInfo {
	char* path;
	char* name;
	uint glTextureIcon;
};

struct MaterialInfo {
	char* name;
};

// this material will store default mesh/submesh
// values and we will be able to change properties for different mesh instances
#pragma pack(push)
struct Material {
	uint color;
	uint specularColor;
	ushort albedoTextureIndex;
	ushort specularTextureIndex;  
	half shininess, roughness;
};
#pragma pack(pop)

#pragma pack(push)
AX_ALIGNED(16) struct Tri { 
	union { struct {float3 vertex0; float centeroidx;};  __m128 v0; };
	union { struct {float3 vertex1; float centeroidy;};  __m128 v1; };
	union { struct {float3 vertex2; float centeroidz;};  __m128 v2; };

	half uv0x, uv0y;
	half uv1x, uv1y;
	half uv2x, uv2y;
	short materialIndex;
	half normal0x, normal0y, normal0z;
	half normal1x, normal1y, normal1z;
	half normal2x, normal2y, normal2z;
};
#pragma pack(pop)

static_assert(sizeof(Tri) == (64 + sizeof(__m128)));

typedef ushort TextureHandle;
typedef ushort MeshHandle;
typedef ushort MaterialHandle;

// this is same for each resource type:
//     the idea is:
//         create big chunk of data pool on GPU
//         create resources on cpu
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

	// manipulate returning material's properties and use handle for other things, REF handle
	// for using with multiple submeshes you need to set count to number of submeshes
	Material* CreateMaterial(MaterialHandle* handle, int count = 1);
	Material& EditMaterial(MaterialHandle handle);

	void PrepareMeshes();
	void PushMeshesToGPU();
	void PushMaterialsToGPU();
	ushort GetNumMeshes();

	MeshInfo GetMeshInfo(MeshHandle handle);
	TextureInfo GetTextureInfo(TextureHandle handle);

	void Initialize(cl_context context, cl_command_queue commandQueue);
	void Destroy();
	void Finalize();

	void PushTexturesToGPU();
}


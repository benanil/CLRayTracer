#pragma once
#include "cl.hpp"
#include "Common.hpp"

// max texture and image etc: 16 texture, 16 mesh
#ifndef CL_MAX_RESOURCE
#	define CL_MAX_RESOURCE 16
#endif
// 10mb for both textures and images
constexpr size_t CL_MAX_RESOURCE_MEMORY = 1.049e+7 * 10; //*10 because jpeg compression

struct Texture
{
	int width, height, offset, padd;
};

struct Mesh
{
	int indexCount;
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
	cl_mem* GetMeshVertexMem();
	cl_mem* GetMeshIndexMem();
	cl_mem* GetMeshesMem();

	void PrepareMeshes();
	void PushMeshesToGPU(cl_context context);
	ushort GetNumMeshes();

	void Initialize();
	void Destroy();

	void PushTexturesToGPU(cl_context context);
}


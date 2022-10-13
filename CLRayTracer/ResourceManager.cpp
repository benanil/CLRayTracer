#include "ResourceManager.hpp"
#define __SSE__
#define __SSE2__
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#include <stb_image.h>
#define FAST_OBJ_IMPLEMENTATION
#include <fast_obj.h>
#include <malloc.h>
#include "Logger.hpp"

// we will import textures and mesh to same memory and push it to the gpu
// this way we are avoiding more memory allocations
// 3 = num color channels

namespace ResourceManager
{
	constexpr size_t indexStart = CL_MAX_RESOURCE_MEMORY / 2;

	cl_int clerr;
	cl_mem textureHandleMem, textureDataMem;
	cl_mem meshVertexMem, meshIndexMem, meshMem;

	Texture textures[CL_MAX_RESOURCE];
	Mesh    meshes  [CL_MAX_RESOURCE];
	unsigned char* arena;

	size_t arenaOffset = 6;
	size_t indexOffset = 0;

	int numTextures = 2;
	int numMeshes = 0;

	cl_mem* GetTextureHandleMem() { return &textureHandleMem; }
	cl_mem* GetTextureDataMem()   { return &textureDataMem; }
			
	cl_mem* GetMeshVertexMem() { return &meshVertexMem; }
	cl_mem* GetMeshIndexMem()  { return &meshIndexMem; }
	cl_mem* GetMeshesMem()       { return &meshMem; }

	ushort GetNumMeshes() { return numMeshes; };
}

void ResourceManager::Initialize()
{
	arena = new unsigned char[CL_MAX_RESOURCE_MEMORY];
	// create default textures. white, black
	textures[0].width = 1;  textures[1].width = 1;
	textures[0].height = 1;  textures[1].height = 1;
	textures[0].offset = 0;  textures[1].offset = 3;
	arena[0] = 0xFF; arena[1] = 0xFF; arena[2] = 0xFF;
	arena[3] = 0;    arena[4] = 0;    arena[5] = 0;

	arenaOffset = 6; numTextures = 2;
}

TextureHandle ResourceManager::ImportTexture(const char* path)
{
	Texture& texture = textures[numTextures]; int channels;
    unsigned char* ptr = stbi_load(path, &texture.width, &texture.height, &channels, 3);
	unsigned long numBytes = texture.height * texture.width * 3;
	if (arenaOffset + numBytes >= CL_MAX_RESOURCE_MEMORY)
	{
		AXERROR("mesh importing failed! CL_MAX_RESOURCE_MEMORY is not enough!"); assert(0);
		exit(0); return 0;
	}
	memcpy(arena + arenaOffset, ptr, numBytes);
	texture.offset = arenaOffset / 3;
	arenaOffset += numBytes;
	free(ptr);
	return numTextures++;
}

void ResourceManager::PushTexturesToGPU(cl_context context)
{
	textureDataMem   = clCreateBuffer(context, CL_MEM_COPY_HOST_PTR, arenaOffset, arena, &clerr); assert(clerr == 0);
	textureHandleMem = clCreateBuffer(context, CL_MEM_COPY_HOST_PTR, sizeof(Texture) * numTextures, textures, &clerr); assert(clerr == 0);
}

void ResourceManager::PrepareMeshes()
{
	arenaOffset = 0;
}

MeshHandle ResourceManager::ImportMesh(const char* path)
{
	// todo add texcoords, maybe normals
	Mesh& mesh = meshes[numMeshes];

	fastObjMesh* meshObj = fast_obj_read(path);

	mesh.indexCount = meshObj->index_count;

	if (arenaOffset + meshObj->position_count > indexStart
		|| indexOffset * sizeof(unsigned) + (mesh.indexCount * sizeof(unsigned)) > CL_MAX_RESOURCE_MEMORY)
	{
		AXERROR("mesh importing failed! CL_MAX_RESOURCE_MEMORY is not enough!"); assert(0);
		exit(0); return 0;
	}

	memcpy(arena + arenaOffset, meshObj->positions, sizeof(float) * 3 * meshObj->position_count);
	arenaOffset += meshObj->position_count * sizeof(float) * 3;

	unsigned* idx = (unsigned*)(arena + indexStart + indexOffset);
	fastObjIndex* meshIdx = meshObj->indices;

	for (int i = 0; i < mesh.indexCount; ++i)
	{
		*idx++ = meshIdx->p;
		meshIdx++;
	}

	indexOffset += mesh.indexCount * sizeof(unsigned);

	fast_obj_destroy(meshObj);
	return numMeshes++;
}

void ResourceManager::PushMeshesToGPU(cl_context context)
{
	meshVertexMem = clCreateBuffer(context, CL_MEM_COPY_HOST_PTR, arenaOffset, arena, &clerr); assert(clerr == 0);
	meshIndexMem = clCreateBuffer(context, CL_MEM_COPY_HOST_PTR, indexOffset, arena + indexStart, &clerr); assert(clerr == 0);
	meshMem = clCreateBuffer(context, CL_MEM_COPY_HOST_PTR, sizeof(Mesh) * numMeshes, meshes, &clerr); assert(clerr == 0);
	delete[] arena;
}

void ResourceManager::Destroy()
{
	clReleaseMemObject(textureHandleMem);
	clReleaseMemObject(textureDataMem);
	clReleaseMemObject(meshVertexMem);
	clReleaseMemObject(meshIndexMem);
	clReleaseMemObject(meshMem);
}

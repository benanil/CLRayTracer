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

// max texture and image etc: 16 texture, 16 mesh
#ifndef CL_MAX_RESOURCE
#	define CL_MAX_RESOURCE 16
#endif
// 10mb for both textures and meshes
constexpr size_t CL_MAX_RESOURCE_MEMORY = 1.049e+7 * 10; //*10 because jpeg compression

// we will import textures and mesh to same memory and push it to the gpu
// this way we are avoiding more memory allocations
// 3 = num color channels

namespace ResourceManager
{
	cl_int clerr;
	cl_mem textureHandleMem, textureDataMem;
	cl_mem meshTriangleMem, bvhMem, meshMem;

	Texture textures[CL_MAX_RESOURCE];
	Mesh    meshes  [CL_MAX_RESOURCE];
	unsigned char* arena;

	size_t arenaOffset = 6;

	int numTextures = 2;
	int numMeshes = 0;

	cl_mem* GetTextureHandleMem() { return &textureHandleMem; }
	cl_mem* GetTextureDataMem()   { return &textureDataMem; }
	cl_mem* GetBVHMem()           { return &bvhMem; }
			
	cl_mem* GetMeshTriangleMem() { return &meshTriangleMem; }
	cl_mem* GetMeshesMem()       { return &meshMem;       }

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
	textureDataMem   = clCreateBuffer(context, 32, arenaOffset, arena, &clerr); assert(clerr == 0);
	textureHandleMem = clCreateBuffer(context, 32, sizeof(Texture) * numTextures, textures, &clerr); assert(clerr == 0);
}

void ResourceManager::PrepareMeshes() {
	arenaOffset = 0;
}

MeshHandle ResourceManager::ImportMesh(const char* path)
{
	// todo add texcoords, maybe normals
	Mesh& mesh = meshes[numMeshes];

	fastObjMesh* meshObj = fast_obj_read(path);

	mesh.numTriangles = meshObj->index_count / 3; 
	mesh.triangleStart = arenaOffset / sizeof(Tri);
	
	int indexCount = meshObj->index_count;

	if (arenaOffset + meshObj->position_count > CL_MAX_RESOURCE_MEMORY)
	{
		AXERROR("mesh importing failed! CL_MAX_RESOURCE_MEMORY is not enough!"); assert(0);
		exit(0); return 0;
	}

	Tri* tris = (Tri*)(arena + arenaOffset);

	fastObjIndex* meshIdx = meshObj->indices;

	for (int i = 0; i < mesh.numTriangles; ++i)
	{
		float* posPtr = meshObj->positions + (meshIdx->p * 3);
		tris->vertex0 = float3(posPtr[0], posPtr[1], posPtr[2]);
		meshIdx++;

		posPtr = meshObj->positions + (meshIdx->p * 3);
		tris->vertex1 = float3(posPtr[0], posPtr[1], posPtr[2]);
		meshIdx++;	

		posPtr = meshObj->positions + (meshIdx->p * 3);
		tris->vertex2 = float3(posPtr[0], posPtr[1], posPtr[2]);
		meshIdx++; tris++;
	}

	arenaOffset += mesh.numTriangles * sizeof(Tri);

	fast_obj_destroy(meshObj);
	return numMeshes++;
}

extern BVHNode* BuildBVH(Tri* tris, int numTriangles, BVHNode* nodes, int* nodesUsed);

void ResourceManager::PushMeshesToGPU(cl_context context)
{
	int numTriangles = arenaOffset / sizeof(Tri);
	Tri* tris = (Tri*)arena;
	int nodesUsed;
	BVHNode* nodes = BuildBVH(tris, numTriangles, (BVHNode*)(arena + arenaOffset), &nodesUsed);

	meshTriangleMem = clCreateBuffer(context, 32, arenaOffset, arena, &clerr); assert(clerr == 0);
	bvhMem = clCreateBuffer(context, 32, sizeof(BVHNode) * nodesUsed, nodes, &clerr); assert(clerr == 0);
	meshMem = clCreateBuffer(context, 32, sizeof(Mesh) * numMeshes, meshes, &clerr); assert(clerr == 0);
	delete[] arena;
}

void ResourceManager::Destroy()
{
	clReleaseMemObject(textureHandleMem);
	clReleaseMemObject(textureDataMem);
	clReleaseMemObject(bvhMem);
	clReleaseMemObject(meshTriangleMem);
	clReleaseMemObject(meshMem);
}

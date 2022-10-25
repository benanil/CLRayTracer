#define CL_TARGET_OPENCL_VERSION 300
#include "Renderer.hpp"
#define __SSE__
#define __SSE2__
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#include <stb_image.h>
#define FAST_OBJ_IMPLEMENTATION
#include <fast_obj.h>
#include <malloc.h>
#include "Logger.hpp"
#include "Timer.hpp"
#include <emmintrin.h>
#include <filesystem>

// we are pre allocating gpu memory at the beginning and we are pushing data from cpu 
// whenever we want, after that we can deform that data 
// we are using arena allocators for the cpu resources we are constantly filling it with data
// and pushing to the gpu and fill it again

// you can push maximum 5 texture at once
// you can push maximum 1mn triangle at once, and totally 2mn triangle

// 50mb for textures
constexpr size_t MAX_TEXTURE_MEMORY = 1.049e+7 * 10;
// max num of tris for each gpu push
constexpr size_t MAX_TRIANGLES = 1'000'000;
constexpr size_t MAX_BVHNODES = MAX_TRIANGLES / 8;
constexpr size_t MAX_BVHMEMORY = MAX_BVHNODES * sizeof(BVHNode);
constexpr size_t MAX_MESH_MEMORY = MAX_TRIANGLES * sizeof(Tri) + MAX_BVHMEMORY ;
constexpr size_t MaxTextures = 32;
constexpr size_t MaxMaterials = 256;
constexpr size_t MaxMeshes = 128;

// todo add save load, we can use same arena allocators for each scene
// todo map unmap texture at real time we can create effects with it
// todo add ability to deform meshes with kernels at runtime
// todo add Physics namespace and ray cast on gpu, send ray array and return hit info array execute kernel
// you can even write your own physics

namespace ResourceManager
{
	cl_int clerr;
	cl_mem textureHandleMem, textureDataMem;
	cl_mem meshTriangleMem, bvhMem, bvhIndicesMem, materialsMem;

	Texture textures[MaxTextures];
	fastObjMesh* meshObjs[MaxMeshes];
	// swordMesh, boxMesh
	uint bvhIndices[MaxMeshes];
	MeshInfo meshInfos[MaxMeshes];
	// [sword materials one per each submesh], [another sword materials for same mesh], [box materials], [armor materials], [armor materials 2]
	// we will index these material arrays for each mesh instance
	Material materials[MaxMaterials];

	unsigned char* textureArena; // for each scene we will use same memory
	unsigned char* meshArena;    // for each scene we will use same memory

	size_t textureArenaOffset = 6; // CPU offset 
	size_t meshArenaOffset    = 0; // CPU offset 

	size_t lastTextureOffset = 0; // GPU offset
	size_t lastMeshOffset    = 0; // GPU offset
	size_t lastBVHOffset     = 0; // GPU offset

	int numTextures  = 0;
	int numMeshes    = 0;
	int numMaterials = 0;
	int lastMeshIndex = 0;

	MeshInfo GetMeshInfo(MeshHandle handle) { return meshInfos[handle]; }

	void GetGPUMemories(cl_mem* _textureHandleMem, cl_mem* _textureDataMem, cl_mem* _meshTriangleMem, cl_mem* _bvhMem, cl_mem* _bvhIndicesMem, cl_mem* _materialMem)
	{
		*_textureHandleMem = textureHandleMem;
		*_textureDataMem   = textureDataMem  ;
		*_meshTriangleMem  = meshTriangleMem ;
		*_bvhMem           = bvhMem          ;
		*_bvhIndicesMem    = bvhIndicesMem   ;
		*_materialMem      = materialsMem    ;
	}

	ushort GetNumMeshes() { return numMeshes; };

	void* GetAreaPointer() { return (void*)meshArena; } // unsafe
}

// manipulate returning material's properties and use handle for other things, REF handle
Material* ResourceManager::CreateMaterial(MaterialHandle* handle, int count)
{
	int start = numMaterials; numMaterials += count;
	return materials + start;
}

void ResourceManager::PushMaterialsToGPU(cl_command_queue queue)
{
	clerr = clEnqueueWriteBuffer(queue, materialsMem, false, 0, sizeof(Material) * numMaterials, materials, 0, 0, 0); assert(clerr == 0);
}

void ResourceManager::Initialize(cl_context context)
{
	textureArena = (unsigned char*)malloc(MAX_TEXTURE_MEMORY);
	meshArena    = (unsigned char*)_aligned_malloc(MAX_MESH_MEMORY, 16);

	// total 2mn triangle support for now we can increase it easily because we have a lot more memory in our gpu's 2m triangle has maximum 338 mb memory on gpu
	meshTriangleMem = clCreateBuffer(context, CL_MEM_READ_WRITE, MAX_TRIANGLES * 2 * sizeof(Tri), nullptr, &clerr); assert(clerr == 0);
	bvhMem        = clCreateBuffer(context, CL_MEM_WRITE_ONLY, MAX_BVHMEMORY * 2, nullptr, &clerr); assert(clerr == 0);
	bvhIndicesMem = clCreateBuffer(context, CL_MEM_WRITE_ONLY, MaxMeshes * sizeof(uint), nullptr, &clerr); assert(clerr == 0);
	materialsMem  = clCreateBuffer(context, CL_MEM_WRITE_ONLY, MaxMaterials * sizeof(Material), nullptr, &clerr); assert(clerr == 0);
	
	textureDataMem   = clCreateBuffer(context, CL_MEM_READ_WRITE, MAX_TEXTURE_MEMORY * 2, 0, &clerr); assert(clerr == 0);
	textureHandleMem = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(Texture) * MaxTextures, nullptr, &clerr); assert(clerr == 0);
}

void ResourceManager::PrepareTextures()
{
	// create default textures. white, black
	textures[0].width = 1;  textures[1].width = 1;
	textures[0].height = 1;  textures[1].height = 1;
	textures[0].offset = 0;  textures[1].offset = 3;
	textureArena[0] = 0xFF; textureArena[1] = 0xFF; textureArena[2] = 0xFF;
	textureArena[3] = 0;    textureArena[4] = 0;    textureArena[5] = 0;

	textureArenaOffset = 6; numTextures = 2;
}

TextureHandle ResourceManager::ImportTexture(const char* path)
{
	Texture& texture = textures[numTextures]; int channels;
    unsigned char* ptr = stbi_load(path, &texture.width, &texture.height, &channels, 3);
	unsigned long numBytes = texture.height * texture.width * 3; // 3 = rgb8
	if (!std::filesystem::exists(path))
		AXERROR("texture importing failed! file is not exist"), assert(0);

	if (textureArenaOffset + numBytes >= MAX_TEXTURE_MEMORY)
	{
		AXERROR("texture importing failed! MAX_TEXTURE_MEMORY is not enough!"); assert(0);
		exit(0); return 0;
	}
	memcpy(textureArena + textureArenaOffset, ptr, numBytes);
	texture.offset = (lastTextureOffset + textureArenaOffset) / 3; // 3 = rgb
	textureArenaOffset += numBytes;
	free(ptr);
	return numTextures++;
}

void ResourceManager::PrepareMeshes() {
	meshArenaOffset = 0;
	Material* firstMaterial = materials + 0;
	firstMaterial->color = 0x00FF0000u | (80 << 16) | (55);
	firstMaterial->shininess = 0.5f;
	firstMaterial->albedoTextureIndex = 0u; firstMaterial->specularTextureIndex = 1u;
}

void ResourceManager::PushTexturesToGPU(cl_command_queue queue)
{
	// upload imported
 	clerr = clEnqueueWriteBuffer(queue, textureDataMem, false
	                     , lastTextureOffset 
	                     , textureArenaOffset // size
	                     , textureArena, 0, 0, 0); assert(clerr == 0);
	// reload all of it 
	clerr = clEnqueueWriteBuffer(queue, textureHandleMem, false
	                     , 0 // offset
	                     , MaxTextures * sizeof(Texture) // size
	                     , textures, 0, 0, 0); assert(clerr == 0);
	
	lastTextureOffset += textureArenaOffset;
	textureArenaOffset = 0;
}

MeshHandle ResourceManager::ImportMesh(const char* path)
{
	// maybe normals
	MeshInfo& meshInfo = meshInfos[numMeshes];
	fastObjMesh* meshObj = fast_obj_read(path);

	if (!meshObj) {
		AXERROR("mesh importing failed! obj is not exist!"); exit(0);
	}

	meshObjs[numMeshes] = meshObj;
	meshInfo.numTriangles = meshObj->index_count / 3; 
	meshInfo.triangleStart = meshArenaOffset / sizeof(Tri);
	meshInfo.numMaterials = meshObj->material_count;
	meshInfo.materialStart = numMaterials;
	meshInfo.numMaterials = 0; // we will add after 30 line of code

	if (meshArenaOffset + (meshInfo.numTriangles * sizeof(Tri)) > MAX_TRIANGLES * sizeof(Tri)) {
		AXERROR("mesh importing failed! CL_MAX_RESOURCE_MEMORY is not enough!"); exit(0);
	}

	Tri* tris = (Tri*)(meshArena + meshArenaOffset);

	fastObjIndex* meshIdx = meshObj->indices;

	for (int i = 0; i < meshInfo.numTriangles; ++i)
	{
		float* posPtr = meshObj->positions + (meshIdx->p * 3);
		float* texcoorPtr = meshObj->texcoords + (meshIdx->t * 2);
		
		tris->vertex0 = float3(posPtr[0], posPtr[1], posPtr[2]);
		tris->uv0x = ConvertFloatToHalf(texcoorPtr[0]);
		tris->uv0y = ConvertFloatToHalf(1.0f - texcoorPtr[1]);
		meshIdx++;

		posPtr = meshObj->positions + (meshIdx->p * 3);
		texcoorPtr = meshObj->texcoords + (meshIdx->t * 2);
		tris->vertex1 = float3(posPtr[0], posPtr[1], posPtr[2]);
		tris->uv1x = ConvertFloatToHalf(texcoorPtr[0]);
		tris->uv1y = ConvertFloatToHalf(1.0f - texcoorPtr[1]);
		meshIdx++;	

		posPtr = meshObj->positions + (meshIdx->p * 3);
		texcoorPtr = meshObj->texcoords + (meshIdx->t * 2);
		tris->vertex2 = float3(posPtr[0], posPtr[1], posPtr[2]);
		tris->uv2x = ConvertFloatToHalf(texcoorPtr[0]);
		tris->uv2y = ConvertFloatToHalf(1.0f - texcoorPtr[1]);
		meshIdx++; tris++;
	}

	if (meshObj->material_count == 0)
	{
		Material& material = materials[numMaterials++];
		material.color = ~0u; material.specularTextureIndex = 0; material.albedoTextureIndex = 0;
		meshInfo.numMaterials = 1;
	}
	else for (int i = 0; i < meshObj->material_count; ++i)
	{
		fastObjMaterial& material = meshObj->materials[i];
		// todo store name etc. in somewhere else for showing it on gui	
		materials[numMaterials].color = PackColorRGBAU32(material.Kd);
		materials[numMaterials].shininess = material.Ns;
		char* albedoTexture = material.map_Kd.path;
		materials[numMaterials].albedoTextureIndex = albedoTexture ? ImportTexture(albedoTexture) : 0;
	
		char* specularTexture = material.map_Ks.path;
		materials[numMaterials++].specularTextureIndex = specularTexture ? ImportTexture(specularTexture) : 0;
		meshInfo.numMaterials++;
	}

	// todo add material indexes to vertices

	meshArenaOffset += meshInfo.numTriangles * sizeof(Tri);

	printf("num triangles: %d\n", meshInfo.numTriangles);

	return numMeshes++;
}

extern BVHNode* BuildBVH(Tri* tris, MeshInfo* meshes, int numMeshes, BVHNode* nodes, uint* bvhIndices, uint* _nodesUsed);

void ResourceManager::PushMeshesToGPU(cl_command_queue queue)
{	
	Tri* tris = (Tri*)meshArena;
	uint numNodesUsed;
	BVHNode* nodes = BuildBVH(tris, meshInfos, numMeshes,
			(BVHNode*)(meshArena + meshArenaOffset), bvhIndices, &numNodesUsed);

	// after bvh we dont need these centeroid data, so we evaluate it
	for (int i = lastMeshIndex; i < numMeshes; ++i)
	{
		MeshInfo& mesh = meshInfos[i];
		fastObjMesh* fmesh = meshObjs[i];
		
		if (fmesh->material_count > 0)
			for (int f = 0; f < fmesh->face_count; ++f) {
				tris[mesh.triangleStart + f].centeroidx = *(float*)(fmesh->face_materials + f);
			}
		else for (int f = 0; f < fmesh->face_count; ++f)
			tris[mesh.triangleStart + f].centeroidx = 0;

		fast_obj_destroy(fmesh);
	}

	// add new triangles to gpu buffer
	clerr = clEnqueueWriteBuffer(queue
	                             , meshTriangleMem
	                             , false, lastMeshOffset
	                             , meshArenaOffset
	                             , meshArena, 0, 0, 0); assert(clerr == 0);
	// reload bvh indices (meshes)
	clerr = clEnqueueWriteBuffer(queue, bvhIndicesMem, false, 0, sizeof(uint) * MaxMeshes, bvhIndices, 0, 0, 0); assert(clerr == 0);

	clerr = clEnqueueWriteBuffer(queue, bvhMem, false, lastBVHOffset, sizeof(BVHNode) * numNodesUsed, nodes, 0, 0, 0); assert(clerr == 0);
	
	clerr = clEnqueueWriteBuffer(queue, materialsMem, false, 0, sizeof(Material) * numMaterials, materials, 0, 0, 0); assert(clerr == 0);
	
	lastBVHOffset += sizeof(BVHNode) * numNodesUsed;
	lastMeshOffset += meshArenaOffset;
	meshArenaOffset = 0;
	lastMeshIndex = numMeshes;
}

// destroys the scene
void ResourceManager::Destroy()
{
	clReleaseMemObject(textureHandleMem);
	clReleaseMemObject(textureDataMem);
	clReleaseMemObject(bvhMem);
	clReleaseMemObject(meshTriangleMem);
	clReleaseMemObject(bvhIndicesMem);
}

// finalizes the resources
void ResourceManager::Finalize()
{
	Destroy();
	_aligned_free(meshArena); free(textureArena);
}
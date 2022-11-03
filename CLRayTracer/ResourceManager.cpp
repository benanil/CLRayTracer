#define CL_TARGET_OPENCL_VERSION 300
#include "Renderer.hpp"
#include "ResourceManager.hpp"
#include "AssetManager.hpp"
#define __SSE__
#define __SSE2__
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_MALLOC(size) malloc(size)
#define STBI_REALLOC(ptr, size) realloc(ptr, size)
#define STBI_FREE(ptr)
#include <stb_image.h>
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize.h>

#include <malloc.h>
#include "Logger.hpp"
#include "Timer.hpp"
#include <emmintrin.h>
#include <filesystem>
#include "CLHelper.hpp"
#include "Editor/Editor.hpp"

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
constexpr size_t MAX_BVHNODES = MAX_TRIANGLES / 2;
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
	cl_command_queue commandQueue;

	Texture textures[MaxTextures];
	uint bvhIndices[MaxMeshes];

	// [sword materials one per each submesh], [another sword materials for same mesh], [box materials], [armor materials], [armor materials 2]
	// we will index these material arrays for each mesh instance
	Material materials[MaxMaterials];
	ObjMesh* meshObjs[MaxMeshes];

	MeshInfo meshInfos[MaxMeshes];
	TextureInfo textureInfos[MaxTextures];
	MaterialInfo materialInfos[MaxMaterials];

	unsigned char* meshArena;    // for each scene we will use same memory
	unsigned char* iconStaging;

	size_t meshArenaOffset    = 0; // CPU offset 

	size_t lastTextureOffset = 0; // GPU offset
	size_t lastMeshOffset    = 0; // GPU offset
	size_t lastBVHOffset     = 0; // GPU offset

	int numTextures  = 0;
	int numMeshes    = 0;
	int numMaterials = 0;
	int lastMeshIndex = 0;

	MeshInfo GetMeshInfo(MeshHandle handle)          { return meshInfos[handle];    }
	TextureInfo GetTextureInfo(TextureHandle handle) { return textureInfos[handle]; }
	MaterialInfo GetMaterialInfo(MaterialHandle handle) { return materialInfos[handle]; }

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

#ifndef IMUI_DISABLE
	void DrawMaterialsWindow()
	{
		bool edited = false;
		ImGui::Begin("Materials");
		for (int i = 0; i < numMaterials; ++i)
		{
			Material& material = materials[i];
			ImVec4 vecColor = ImGui::ColorConvertU32ToFloat4(material.color);
			ImGui::PushID(i);
			if (materialInfos[i].name != nullptr)
			ImGui::LabelText("Name:", materialInfos[i].name);
			edited |= ImGui::ColorEdit3("color", &vecColor.x);
			material.color = ImGui::ColorConvertFloat4ToU32(vecColor);
			edited |= ImGui::ImageButton((void*)textureInfos[material.albedoTextureIndex].glTextureIcon, { 64, 64 }); ImGui::SameLine();
			edited |= ImGui::ImageButton((void*)textureInfos[material.specularTextureIndex].glTextureIcon, { 64, 64 });
			edited |= ImGui::DragScalar("roughness", ImGuiDataType_U16, &material.roughness, 100.0f);
			edited |= ImGui::DragScalar("shininess", ImGuiDataType_U16, &material.shininess, 100.0f);
			ImGui::PopID();
		}
		if (edited) PushMaterialsToGPU();
		ImGui::End();
	}
#endif
}

// manipulate returning material's properties and use handle for other things, REF handle
// Todo fix
Material* ResourceManager::CreateMaterial(MaterialHandle* materialPtr, int count)
{
	Material* ptr = materials + numMaterials;
	numMaterials++;
	return ptr;
}

void ResourceManager::PushMaterialsToGPU()
{
	clerr = clEnqueueWriteBuffer(commandQueue, materialsMem, false, 0, sizeof(Material) * numMaterials, materials, 0, 0, 0); assert(clerr == 0);
}

void ResourceManager::Initialize(cl_context context, cl_command_queue command_queue)
{
	commandQueue = command_queue;
	meshArena    = (unsigned char*)_aligned_malloc(MAX_MESH_MEMORY, 16);
	iconStaging = (unsigned char*)malloc(64 * 64 * 3 + 1);

	Editor::AddOnEditor(DrawMaterialsWindow);
	// total 2mn triangle support for now we can increase it easily because we have a lot more memory in our gpu's 2m triangle has maximum 338 mb memory on gpu
	meshTriangleMem = clCreateBuffer(context, CL_MEM_READ_WRITE, MAX_TRIANGLES * 2 * sizeof(Tri), nullptr, &clerr); assert(clerr == 0);
	bvhMem        = clCreateBuffer(context, CL_MEM_WRITE_ONLY, MAX_BVHMEMORY * 2, nullptr, &clerr); assert(clerr == 0);
	bvhIndicesMem = clCreateBuffer(context, CL_MEM_WRITE_ONLY, MaxMeshes * sizeof(uint), nullptr, &clerr); assert(clerr == 0);
	materialsMem  = clCreateBuffer(context, CL_MEM_WRITE_ONLY, MaxMaterials * sizeof(Material), nullptr, &clerr); assert(clerr == 0);
	
	textureDataMem   = clCreateBuffer(context, CL_MEM_READ_WRITE, MAX_TEXTURE_MEMORY * 2, 0, &clerr); assert(clerr == 0);
	textureHandleMem = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(Texture) * MaxTextures, nullptr, &clerr); assert(clerr == 0);
	AssetManager_Initialize();

	// create default textures. white, black
	textures[0].width = 1;  textures[1].width = 1;
	textures[0].height = 1;  textures[1].height = 1;
	textures[0].offset = 0;  textures[1].offset = 3;
	unsigned char defaultTextureData[6];
	defaultTextureData[0] = 0xFF; defaultTextureData[1] = 0xFF; defaultTextureData[2] = 0xFF;
	defaultTextureData[3] = 0;    defaultTextureData[4] = 0;    defaultTextureData[5] = 0;

	lastTextureOffset = 6; numTextures = 2;

	clerr = clEnqueueWriteBuffer(commandQueue, textureDataMem, false, 0, 6, defaultTextureData, 0, 0, 0); assert(clerr == 0);
}

TextureHandle ResourceManager::ImportTexture(const char* path)
{
	Texture& texture = textures[numTextures]; 
	TextureInfo& textureInfo = textureInfos[numTextures];
	textureInfo.path = _strdup(path);
	textureInfo.name = Helper::GetPathName(textureInfo.path);
	
	unsigned long numBytes;

	if (!std::filesystem::exists(path))
		AXERROR("texture importing failed! file is not exist: %s", path), exit(0);

	int channels;
	unsigned char* ptr = stbi_load(path, &texture.width, &texture.height, &channels, 3);
	numBytes = texture.height * texture.width * 3; // 3 = rgb8

	if (!ptr) { AXERROR("texture importing failed! keep in mind texture must be .jpeg"); exit(0); }

	if (lastTextureOffset + numBytes >= MAX_TEXTURE_MEMORY) {
		AXERROR("texture importing failed! MAX_TEXTURE_MEMORY is not enough!"); exit(0);
	}
	
	clerr = clEnqueueWriteBuffer(commandQueue, textureDataMem, false, lastTextureOffset, numBytes, ptr, 0, 0, 0);  assert(clerr == 0);
#ifndef GAME_BUILD
	if (texture.width <= 64 || texture.height <= 64)
		Renderer::CreateGLTexture(textureInfo.glTextureIcon, texture.width, texture.height, ptr);
	else if (numTextures > 3) // Jump skybox texture
	{
		stbir_resize_uint8(ptr, texture.width, texture.height, 0, iconStaging, 64, 64, 0, 3);

		// converts to 64 * 64 icon
		// int scaledW = texture.width / 64;
		// for (int j = 0; j < 64; ++j) 
		// {
		// 	for (int i = 0; i < 64; ++i) 
		// 	{
		// 		int destination = (j * 3 * 64) + (i * 3);
		// 		unsigned char* source = (ptr + (j * 3 * texture.width) + (i * 3 * scaledW));
		// 		ptr[destination + 0] = source[0];
		// 		ptr[destination + 1] = source[1];
		// 		ptr[destination + 2] = source[2];
		// 	}
		// }
		Renderer::CreateGLTexture(textureInfo.glTextureIcon, 64, 64, iconStaging);
	}
#endif
	texture.offset = lastTextureOffset / 3; // 3 = rgb
	lastTextureOffset += numBytes;
	STBI_FREE(ptr);
	AssetManagerResetTempMemory();
	// no need to free ptr memory its arena allocated
	return numTextures++;
}

void ResourceManager::PrepareMeshes() {
	meshArenaOffset = 0;
	Material* firstMaterial = materials + 0;
	firstMaterial->color = 0x00FF0000u | (80 << 16) | (55);
	firstMaterial->specularColor = 250 | (228 << 8) | (210 << 16);
	// shininess = 50.0f, roughness = 0.6f but converted to short instead of half
	firstMaterial->shininess = 30000, firstMaterial->roughness = 40000; 
	firstMaterial->albedoTextureIndex = 0u; firstMaterial->specularTextureIndex = 1u; numMaterials++;
}

void ResourceManager::PushTexturesToGPU()
{
	// reload all of it 
	clerr = clEnqueueWriteBuffer(commandQueue, textureHandleMem, false
	                     , 0 // offset
	                     , MaxTextures * sizeof(Texture) // size
	                     , textures, 0, 0, 0); assert(clerr == 0);
}

MeshHandle ResourceManager::ImportMesh(const char* path)
{
	MeshInfo& meshInfo = meshInfos[numMeshes];
	ObjMesh* mesh = nullptr;
	mesh = AssetManager_ImportMesh(path, (Tri*)meshArena + meshArenaOffset);
	meshInfo.materialStart = mesh->numMaterials  ? numMaterials : 0;
	meshInfo.triangleStart = meshArenaOffset / sizeof(Tri);
	meshInfo.numTriangles = mesh->numTris;
	meshInfo.numMaterials = mesh->numMaterials;
	meshObjs[numMeshes] = mesh;

	for (int i = 0; i < mesh->numMaterials; ++i)
	{
		ObjMaterial& material = mesh->materials[i];
		MaterialInfo& materialInfo = materialInfos[numMaterials];
		materialInfo.name = mesh->mtlText + material.name;
		// todo store name etc. in somewhere else for showing it on gui	
		materials[numMaterials].color = material.diffuseColor;
		materials[numMaterials].specularColor = material.specularColor;
		materials[numMaterials].shininess = material.shininess;
		materials[numMaterials].roughness = material.roughness;
		
		char* albedoTexture = material.diffusePath ? mesh->mtlText + material.diffusePath : nullptr;
		materials[numMaterials].albedoTextureIndex = albedoTexture ? ImportTexture(albedoTexture) : 0;
	
		char* specularTexture = material.specularPath ? mesh->mtlText + material.specularPath : nullptr;
		materials[numMaterials++].specularTextureIndex = specularTexture ? ImportTexture(specularTexture) : 0;
	}

	meshArenaOffset += meshInfo.numTriangles * sizeof(Tri);

	AXLOG("num triangles: %d\n", meshInfo.numTriangles);

	return numMeshes++;
}

extern BVHNode* BuildBVH(Tri* tris, MeshInfo* meshes, int numMeshes, BVHNode* nodes, uint* bvhIndices, uint* _nodesUsed);

void ResourceManager::PushMeshesToGPU()
{	
	Tri* tris = (Tri*)meshArena;
	uint numNodesUsed;
	BVHNode* nodes = BuildBVH(tris, meshInfos, numMeshes,
			(BVHNode*)(meshArena + (lastMeshOffset + meshArenaOffset)), bvhIndices, &numNodesUsed);

	// add new triangles to gpu buffer
	clerr = clEnqueueWriteBuffer(commandQueue
	                             , meshTriangleMem
	                             , false, lastMeshOffset
	                             , meshArenaOffset
	                             , meshArena, 0, 0, 0); assert(clerr == 0);
	// reload bvh indices (meshes)
	clerr = clEnqueueWriteBuffer(commandQueue, bvhIndicesMem, false, 0, sizeof(uint) * MaxMeshes, bvhIndices, 0, 0, 0); assert(clerr == 0);
	
	clerr = clEnqueueWriteBuffer(commandQueue, bvhMem, false, lastBVHOffset, sizeof(BVHNode) * numNodesUsed, nodes, 0, 0, 0); assert(clerr == 0);

	clerr = clEnqueueWriteBuffer(commandQueue, materialsMem, false, 0, sizeof(Material) * numMaterials, materials, 0, 0, 0); assert(clerr == 0);
	
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
	while (numMeshes--) AssetManager_DestroyMesh(meshObjs[numMeshes]);
	while (numTextures--) free(textureInfos[numTextures].path); // we cant delete texture icon for now, operating system will clean it anyway and it is small data either
	free(iconStaging);
	_aligned_free(meshArena); 
	AssetManager_Destroy();
}

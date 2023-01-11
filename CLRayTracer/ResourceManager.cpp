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
constexpr size_t MAX_TRIANGLES = 1'200'000;
constexpr size_t MAX_BVHNODES = MAX_TRIANGLES;
constexpr size_t MAX_BVHMEMORY = MAX_BVHNODES * sizeof(BVHNode);
constexpr size_t MAX_MESH_MEMORY = MAX_TRIANGLES * sizeof(Tri);
constexpr size_t MaxTextures = 32;
constexpr size_t MaxMaterials = 256;
constexpr size_t MaxMeshes = 128;

// Todo:
//      map unmap texture at real time we can create effects with it
//      add ability to deform meshes with kernels at runtime
//      add save load, we can use same arena allocators for each scene
//      add Physics namespace and ray cast on gpu, send ray array and return hit info array execute kernel

// globals
RGB8* g_TexturePixels = nullptr;
BVHNode* g_BVHNodes = nullptr;
Tri* g_Triangles = nullptr;    // for each scene we will use same memory

Material* g_Materials = nullptr;
Texture* g_Textures = nullptr;
uint* g_BVHIndices = nullptr;

namespace ResourceManager
{
	cl_int clerr;
	cl_mem textureHandleMem, textureDataMem;
	cl_mem meshTriangleMem, bvhMem, bvhIndicesMem, materialsMem;
	cl_command_queue commandQueue;
	
	Material m_Materials[MaxMaterials];
	Texture m_Textures[MaxTextures];
	uint m_BVHIndices[MaxMeshes];

	uint numberOfBVH = 0;
	
	// [sword materials one per each submesh], [another sword materials for same mesh], [box materials], [armor materials], [armor materials 2]
	// we will index these material arrays for each mesh instance
	ObjMesh* meshObjs[MaxMeshes];

	MeshInfo meshInfos[MaxMeshes];
	TextureInfo textureInfos[MaxTextures];
	MaterialInfo materialInfos[MaxMaterials];
	
	unsigned char* iconStaging;

	size_t numTriangles = 0; 

	size_t lastTextureOffset = 0; // GPU offset
	size_t lastTriangleCount = 0; // GPU offset
	size_t lastBVHIndex      = 0; // GPU offset

	int numTextures   = 0;
	int numMeshes     = 0;
	int numMaterials  = 0;
	int lastMeshIndex = 0;

	MeshInfo GetMeshInfo(MeshHandle handle)          { return meshInfos[handle];    }
	TextureInfo GetTextureInfo(TextureHandle handle) { return textureInfos[handle]; }
	MaterialInfo GetMaterialInfo(MaterialHandle handle) { return materialInfos[handle]; }
	
	Material& EditMaterial(MaterialHandle handle) { return g_Materials[handle]; }

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

#ifndef IMUI_DISABLE
	void DrawMaterialsWindow()
	{
		bool edited = false;
		ImGui::Begin("Resources");

		if (ImGui::CollapsingHeader("materials"))
		{
			for (int i = 0; i < numMaterials; ++i)
			{
				Material& material = g_Materials[i];
				ImVec4 vecColor = ImGui::ColorConvertU32ToFloat4(material.color);
				ImGui::PushID(i);
				if (materialInfos[i].name != nullptr)
					ImGui::LabelText("Name:", materialInfos[i].name);
				edited |= ImGui::ColorEdit3("color", &vecColor.x);
				material.color = ImGui::ColorConvertFloat4ToU32(vecColor);
				if (material.albedoTextureIndex > 1) edited |= ImGui::ImageButton((void*)textureInfos[material.albedoTextureIndex].glTextureIcon, { 64, 64 }), ImGui::SameLine();
				if (material.specularTextureIndex > 1) edited |= ImGui::ImageButton((void*)textureInfos[material.specularTextureIndex].glTextureIcon, { 64, 64 });
				edited |= ImGui::DragScalar("roughness", ImGuiDataType_U16, &material.roughness, 100.0f);
				edited |= ImGui::DragScalar("shininess", ImGuiDataType_U16, &material.shininess, 100.0f);
				ImGui::PopID();
			}
			if (edited) PushMaterialsToGPU();
			ImGui::Separator();
		}
	
		ImGui::End();
	}
#endif
}

// manipulate returning material's properties and use handle for other things, REF handle
// Todo fix
Material* ResourceManager::CreateMaterial(MaterialHandle* materialPtr, int count)
{
	Material* ptr = g_Materials + numMaterials;
	numMaterials++;
	return ptr;
}

void ResourceManager::PushMaterialsToGPU()
{
	clerr = clEnqueueWriteBuffer(commandQueue, materialsMem, false, 0, sizeof(Material) * numMaterials, g_Materials, 0, 0, 0); assert(clerr == 0);
}

void ResourceManager::Initialize(cl_context context, cl_command_queue command_queue)
{
	commandQueue = command_queue;
	// allocate memorys
	g_Triangles   = (Tri*)_aligned_malloc(MAX_MESH_MEMORY, 16);
	iconStaging = (unsigned char*)malloc(64 * 64 * 3 + 1);
	g_BVHNodes    = (BVHNode*)_aligned_malloc(MAX_BVHMEMORY, 16);
	g_TexturePixels = (RGB8*)malloc(MAX_TEXTURE_MEMORY * 2);

	g_Materials = m_Materials; g_Textures = m_Textures; g_BVHIndices = m_BVHIndices; // initialize global pointers

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
	g_Textures[0].width = 1;  g_Textures[1].width = 1;
	g_Textures[0].height = 1;  g_Textures[1].height = 1;
	g_Textures[0].offset = 0;  g_Textures[1].offset = 3;
	unsigned char defaultTextureData[6];
	defaultTextureData[0] = 0xFF; defaultTextureData[1] = 0xFF; defaultTextureData[2] = 0xFF;
	defaultTextureData[3] = 0;    defaultTextureData[4] = 0;    defaultTextureData[5] = 0;

	lastTextureOffset = 6; numTextures = 2;

	clerr = clEnqueueWriteBuffer(commandQueue, textureDataMem, false, 0, 6, defaultTextureData, 0, 0, 0); assert(clerr == 0);
}

TextureHandle ResourceManager::ImportTexture(const char* path)
{
	Texture& texture = g_Textures[numTextures]; 
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
	
	// copy texture to gpu
	clerr = clEnqueueWriteBuffer(commandQueue, textureDataMem, false, lastTextureOffset, numBytes, ptr, 0, 0, 0);  assert(clerr == 0);
	// copy texture to cpu
	memcpy(g_TexturePixels + lastTextureOffset, ptr, numBytes);

#ifndef GAME_BUILD
	if (texture.width <= 64 || texture.height <= 64)
		Renderer::CreateGLTexture(textureInfo.glTextureIcon, texture.width, texture.height, ptr);
	else if (numTextures > 3) // Jump skybox texture
	{
		stbir_resize_uint8(ptr, texture.width, texture.height, 0, iconStaging, 64, 64, 0, 3);
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
	Material* firstMaterial = g_Materials + 0;
	firstMaterial->color = 0x00FF0000u | (80 << 16) | (55);
	firstMaterial->specularColor = 250 | (228 << 8) | (210 << 16);
	// shininess = 50.0f, roughness = 0.6f but converted to short instead of half
	firstMaterial->shininess = 30000, firstMaterial->roughness = 40000; 
	firstMaterial->albedoTextureIndex = 0u; firstMaterial->specularTextureIndex = 1u; numMaterials++;
}

void ResourceManager::PushTexturesToGPU()
{
	// reload all of it 
	clerr = clEnqueueWriteBuffer(commandQueue, textureHandleMem, false, 0 // offset
	                     , MaxTextures * sizeof(Texture) // size
	                     , g_Textures, 0, 0, 0); assert(clerr == 0);
}

MeshHandle ResourceManager::ImportMesh(const char* path)
{
	MeshInfo& meshInfo = meshInfos[numMeshes];
	ObjMesh* mesh = nullptr;
	mesh = AssetManager_ImportMesh(path, g_Triangles + numTriangles);
	meshInfo.materialStart = mesh->numMaterials  ? numMaterials : 0;
	meshInfo.triangleStart = numTriangles;
	meshInfo.numTriangles = mesh->numTris;
	meshInfo.numMaterials = mesh->numMaterials;
	meshObjs[numMeshes] = mesh;

	for (int i = 0; i < mesh->numMaterials; ++i)
	{
		ObjMaterial& objMaterial = mesh->materials[i];
		MaterialInfo& materialInfo = materialInfos[numMaterials];
		Material& material = g_Materials[numMaterials++];
		materialInfo.name = mesh->mtlText + objMaterial.name;
		// todo store name etc. in somewhere else for showing it on gui	
		material.color = objMaterial.diffuseColor;
		material.specularColor = objMaterial.specularColor;
		material.shininess = objMaterial.shininess;
		material.roughness = objMaterial.roughness;
		
		char* albedoTexture = objMaterial.diffusePath ? mesh->mtlText + objMaterial.diffusePath : nullptr;
		material .albedoTextureIndex = albedoTexture ? ImportTexture(albedoTexture) : 0;
	
		char* specularTexture = objMaterial.specularPath ? mesh->mtlText + objMaterial.specularPath : nullptr;
		material .specularTextureIndex = specularTexture ? ImportTexture(specularTexture) : 0;
	}

	numTriangles += meshInfo.numTriangles;

	AXLOG("num triangles: %d\n", meshInfo.numTriangles);

	return numMeshes++;
}

extern uint BuildBVH(Tri* tris, MeshInfo* meshes, int numMeshes, BVHNode* nodes, uint* bvhIndices);

void ResourceManager::PushMeshesToGPU()
{	
	uint numNodesUsed = BuildBVH(g_Triangles, meshInfos, numMeshes, g_BVHNodes + lastBVHIndex, g_BVHIndices + numberOfBVH);
	size_t addedTriangleSize = size_t(numTriangles - lastTriangleCount) * sizeof(Tri);

	// add new triangles to gpu buffer
	clerr = clEnqueueWriteBuffer(commandQueue, meshTriangleMem, false, lastTriangleCount * sizeof(Tri), addedTriangleSize, g_Triangles, 0, 0, 0); assert(clerr == 0);
		
	size_t bvhIndexStart = numberOfBVH * sizeof(BVHNode);
	size_t bvhIndexSize = size_t(numMeshes - numberOfBVH) * sizeof(uint);

	clerr = clEnqueueWriteBuffer(commandQueue, bvhIndicesMem, false, bvhIndexStart, bvhIndexSize, g_BVHIndices + numberOfBVH, 0, 0, 0); assert(clerr == 0);
	
	clerr = clEnqueueWriteBuffer(commandQueue, bvhMem, false, lastBVHIndex * sizeof(BVHNode), sizeof(BVHNode) * numNodesUsed, g_BVHNodes + lastBVHIndex, 0, 0, 0); assert(clerr == 0);

	clerr = clEnqueueWriteBuffer(commandQueue, materialsMem, false, 0, sizeof(Material) * numMaterials, g_Materials, 0, 0, 0); assert(clerr == 0);
	
	numberOfBVH = numMeshes;
	lastBVHIndex += numNodesUsed;
	lastTriangleCount += numTriangles;
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
	free(g_TexturePixels);
	_aligned_free(g_Triangles);
	_aligned_free(g_BVHNodes);
	AssetManager_Destroy();
}

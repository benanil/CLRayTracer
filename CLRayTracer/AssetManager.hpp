#pragma once
#include "ResourceManager.hpp"

struct ObjMaterial {
	int name;
	unsigned diffuseColor, specularColor;
	ushort shininess, roughness;
	int diffusePath, specularPath;
};

struct ObjMesh
{
	char* name;

	Tri* tris;
	int numTris;
	
	ObjMaterial materials[32]; // sponza has 25 material
	int numMaterials;
	// we will use this for storing .mtl text and texture paths
	char* mtlText; 
};

ObjMesh* AssetManager_ImportMesh(const char* path, Tri* triArena);
void AssetManager_DestroyMesh(ObjMesh* mesh);

void AssetManager_Initialize();
void AssetManager_Destroy();

void* AssetManagerTempAllocate(size_t size);

inline void AssetManagerTempReserve(char* ptr, size_t* size, size_t newSize);

void AssetManagerTempFree(void* ptr);

void* AssetManagerTempRealloc(void* ptr);

void AssetManagerResetTempMemory();




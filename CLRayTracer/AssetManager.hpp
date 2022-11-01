#pragma once
#include "ResourceManager.hpp"

struct ObjMaterial {
	char* name;
	unsigned diffuseColor, specularColor;
	float shininess, roughness;
	char* diffusePath, * specularPath;
};

struct ObjMesh
{
	Tri* tris;
	int numTris;

	ObjMaterial materials[32]; // sponza has 25 material
	int numMaterials;
	int materialRanges[128][2];
	// we will use this for storing .obj and .mtl text and texture paths
	char* textMem;
};

namespace AssetManager
{
	ObjMesh* ImportMesh(const char* path, Tri* triArena);
	void DestroyObj(ObjMesh* mesh);
}
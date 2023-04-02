#include <filesystem>
#include <fstream>
#include "AssetManager.hpp"
#include "CLHelper.hpp"
#include "Logger.hpp"
#include "Random.hpp"
#include "quicklz.h"
#include <cassert>

FINLINE bool IsNumber(const char c) { return c <= '9' && c >= '0'; }
FINLINE bool IsWhitespace(char c)   { return (c == ' ' || c == '\t' || c == '\r'); }

inline char* ParseFloat(float* f, char* __restrict ptr)
{
	while (IsWhitespace(*ptr)) ptr++;
	
	double sign = 1.0;
	if(*ptr == '-') sign = -1.0, ptr++; 

	double num = 0.0;

	while (IsNumber(*ptr)) 
		num = 10.0 * num + (double)(*ptr++ - '0');

	if (*ptr == '.') ptr++;

	double fra = 0.0, div = 1.0;

	while (IsNumber(*ptr))
		fra = 10.0f * fra + (double)(*ptr++ - '0'), div *= 10.0f;
	
	num += fra / div;
	*f = (float)(sign * num);
	return ptr;
}

inline int PointerDistance(const char* __restrict less, const char* __restrict higher)
{
	int result = 0;
	while (less++ < higher) result++;
	return result;
}

static char* tempAlloc0, * tempAlloc1, * tempAlloc2;

static size_t sizeTemp0 = 1024 * sizeof(float3);
static size_t sizeTemp1 = 1024 * sizeof(float2);
static size_t sizeTemp2 = 1024 * sizeof(float3);

static size_t currTemp0 = 0, currTemp1 = 0, currTemp2 = 0;

// usefull for stbi image

inline void AssetManagerTempReserve(char** ptr, size_t* size, size_t newSize)
{
	if (newSize > *size) {
		*size = Max(*size + (newSize / 2ul), newSize);
		*ptr = (char*)realloc(*ptr, *size);
	}
}

void* AssetManagerTempAllocate(size_t size)
{
	AssetManagerTempReserve(&tempAlloc0, &sizeTemp0, currTemp0 + size);
	size_t oldPoint = currTemp0;
	currTemp0 += size;
	return tempAlloc0 + oldPoint;
}

void AssetManagerTempFree(void* ptr) { }

void* AssetManagerTempRealloc(void* ptr, size_t size) { assert(0); return 0; };

void AssetManagerResetTempMemory() { currTemp0 = 0; }

void AssetManager_Initialize()
{
	tempAlloc0 = (char*)malloc(30000 * sizeof(float3));
	tempAlloc1 = (char*)malloc(1024 * sizeof(float2));
	tempAlloc2 = (char*)malloc(1024 * sizeof(float3));
}

void AssetManager_Destroy()
{
	FREE_ALL(tempAlloc0, tempAlloc1, tempAlloc2);
}

void AssetManager_SaveMeshToDisk(char* path, ObjMesh* mesh, uint msz);

static ObjMesh* AssetManager_ImportObj(const char* path, Tri* triArena, char* pathDup, size_t pathLen)
{
	if (!std::filesystem::exists(path)) {
		AXERROR("mesh file is not exist!\n %c", path);
		return nullptr;
	}

	std::ifstream meshFile(path, std::ios::in | std::ios::binary | std::ios::failbit);
	SkipBOM(meshFile); 
	
	ObjMesh* mesh = new ObjMesh; 
	mesh->numMaterials = 0, mesh->numTris = 0;
	mesh->tris = triArena;
	mesh->mtlText = nullptr;

	const uintmax_t sz = std::filesystem::file_size(path);
	char* objText = Helper::ReadAllText(path);
	Helper::ChangeExtension(pathDup, "mtl", pathLen);
	char* mtlPath = pathDup; // for readibility
	uint msz = std::filesystem::exists(mtlPath) ? (uint)std::filesystem::file_size(mtlPath) : 0u;

	// read if material path exist 
	if (msz) mesh->mtlText = Helper::ReadAllText(mtlPath);

	// todo string pool
	int numVertices = 0, numTexCoords = 0, numNormals = 0;

	float* currVertices = (float*)tempAlloc0, *currTexCoords = (float*)tempAlloc1, *currNormals = (float*)tempAlloc2;

	// hashMap for material indices, materialMap[materialNameHash & 255] = material Index 
	unsigned char materialMap[512] = {0};
	
	// import materials
	char* curr = mesh->mtlText, *currEnd = curr + msz;
	ObjMaterial* currMaterial;

	while (curr && *curr && curr < currEnd)
	{
		if (*curr == '#')  while (*curr++ != '\n'); // skip line
		while (*curr == '\n' || IsWhitespace(*curr)) curr++;
		
		if (curr[0] == 'n' && curr[1] == 'e' && curr[2] == 'w') // no need to m, t, l 
		{
			curr += 7; // skip newmtl + space
			currMaterial = mesh->materials + mesh->numMaterials;
			// set default properties
			currMaterial->specularColor = ~0u, currMaterial->diffuseColor = ~0u;
			currMaterial->shininess = ConvertFloatToHalf(2.2f), currMaterial->roughness = ConvertFloatToHalf(0.6f);
			currMaterial->diffusePath = 0, currMaterial->specularPath = 0;
			currMaterial->name = PointerDistance(mesh->mtlText, curr);
			unsigned hash = Random::WangHash(uint(curr[0]) | uint(curr[1] << 8) | uint(curr[2] << 16));
			while(*curr != '\n' && !IsWhitespace(*curr))
				hash = *curr++ + (hash << 6) + (hash << 16) - hash; 
			*curr++ = '\0'; // null terminator
			if (materialMap[hash & 511])
				AXERROR("mesh importing failed material hash collision detected!"), exit(0);

			materialMap[hash & 511] = mesh->numMaterials++;
		}
		else if (curr[0] == 'N' && curr[1] == 's') { // shininess
			float f;
			curr = ParseFloat(&f, curr + 2);
			f = Clamp(f, 0.0f, 100.0f) / 50.0f;
			currMaterial->shininess = ConvertFloatToHalf(f); // float range to short range
		}
		else if (curr[0] == 'd') { // roughness
			float f;
			curr = ParseFloat(&f, curr + 2);
			currMaterial->roughness = ConvertFloatToHalf(Clamp(f, 0.0f, 1.0f)); // float range to short range
		}
		else if (curr[0] == 'K' && curr[1] == 'd') { // diffuse color
			float colorf[3];
			curr = ParseFloat(colorf + 0, curr + 2);
			curr = ParseFloat(colorf + 1, curr);
			curr = ParseFloat(colorf + 2, curr);
			currMaterial->diffuseColor = PackColorRGBU32(colorf);
		}
		else if (curr[0] == 'K' && curr[1] == 's') { // specular color
			float colorf[3];
			curr = ParseFloat(colorf + 0, curr + 2);
			curr = ParseFloat(colorf + 1, curr);
			curr = ParseFloat(colorf + 2, curr);
			currMaterial->specularColor = PackColorRGBU32(colorf);
		}
		else if (curr[0] == 'm') // map_bla
		{
			if (curr[4] == 'K' && curr[5] == 'd') {
				// set this pointer as texture path data
				currMaterial->diffusePath = PointerDistance(mesh->mtlText, (curr += 7));
				// find end point of path
				while (*curr != '\n' && *curr != '\r') curr++;
				*curr++ = '\0'; // addd null terminator
			}
			else if (curr[4] == 'K' && curr[5] == 's') {
				currMaterial->specularPath = PointerDistance(mesh->mtlText, (curr += 7)); // set this pointer as texture path data
				while (*curr != '\n' && *curr != '\r') curr++; // find end point of path
				*curr++ = '\0'; // addd null terminator
			}
			else while (*curr != '\n') curr++;
		} // skip line. header is unknown | unused
		else while (*curr != '\n' && *curr) curr++;
	}

	curr = objText, currEnd = curr + sz;
	unsigned currentMaterial = 0;

	while (curr && *curr && curr < currEnd)
	{
		if (*curr == '#')  while (*curr++ != '\n');
		while (*curr == '\n' || IsWhitespace(*curr)) curr++;

		if (*curr == 'v')
		{
			while (curr[1] == ' ') { // vertex=position 
				curr += 2; 
				AssetManagerTempReserve(&tempAlloc0, &sizeTemp0, numVertices * sizeof(float3) + sizeof(float3));
				currVertices = (float*)(tempAlloc0 + (numVertices * sizeof(float3)));
				curr = ParseFloat(currVertices++, curr); 
				curr = ParseFloat(currVertices++, curr); 
				curr = ParseFloat(currVertices++, curr); numVertices++;
				// skip line&whiteSpace
				while (*curr == '\n' || IsWhitespace(*curr)) curr++;
			}

			while (curr[1] == 't') {
				curr += 2;
				AssetManagerTempReserve(&tempAlloc1, &sizeTemp1, numTexCoords * sizeof(float2) + sizeof(float2));
				currTexCoords = (float*)(tempAlloc1 + (numTexCoords * sizeof(float2)));
				curr = ParseFloat(currTexCoords++, curr); 
				curr = ParseFloat(currTexCoords++, curr); numTexCoords++;
				// skip line&whiteSpace
				while (*curr == '\n' || IsWhitespace(*curr)) curr++;
			}

			while (curr[1] == 'n') {
				curr += 2; 
				AssetManagerTempReserve(&tempAlloc2, &sizeTemp2, numNormals * sizeof(float3) + sizeof(float3));
				currNormals = (float*)(tempAlloc2 + (numNormals * sizeof(float3)));
				curr = ParseFloat(currNormals++, curr);
				curr = ParseFloat(currNormals++, curr); 
				curr = ParseFloat(currNormals++, curr); numNormals++;
				// skip line&whiteSpace
				while (*curr == '\n' || IsWhitespace(*curr)) curr++;
			}
		} 
		
		if (curr[0] == 'u' && curr[1] == 's' && curr[2] == 'e') // && curr[3] == 'm' && curr[4] == 't' no need
		{
			curr += 7; // skip usemtl + space
			unsigned hash = Random::WangHash(uint(curr[0]) | uint(curr[1] << 8) | uint(curr[2] << 16));
			while(*curr != '\n' && !IsWhitespace(*curr)) // create texture path hash 
				hash = *curr++ + (hash << 6) + (hash << 16) - hash;
			while (*curr == '\n' || IsWhitespace(*curr)) curr++;
			currentMaterial = materialMap[hash & 511]; // use material index for this group of triangles
		}

		while (curr[0] == 'f')
		{
			curr += 2;
			int vtn = 0; // number of indices = vertex, texture and normal v, vt, vtn?
			Tri* tri = mesh->tris + mesh->numTris; mesh->numTris++;
			// for now we cant import more than 1mn triangle for each gpu push
			if (mesh->numTris >= 1'000'000) // ResourceManager::Max_Triangles
				AXERROR("too many triangles for mesh!"), exit(0);

			float* vertPtr = (float*)tri; 
			half* texCoordPtr = &tri->uv0x;
			half* normalPtr = &tri->normal0x;
			float* texcoords = (float*)tempAlloc1, *normals = (float*)tempAlloc2;
      
			for(int i = 0; i < 3; ++i, texCoordPtr += 2, vertPtr += 4, normalPtr += 3) // compiler please unroll :D
			{
				int positionIdx = 0, textureIdx = 0, normalIdx = 0;
				// since we know indices are not negative values we are parsing like this
				while (IsNumber(*curr)) positionIdx = 10 * positionIdx + (*curr++ - '0'); curr++; // last ptr++ for jump '/'
				while (IsNumber(*curr)) textureIdx = 10 * textureIdx + (*curr++ - '0'); curr++;
				while (IsNumber(*curr)) normalIdx = 10 * normalIdx + (*curr++ - '0'); curr++;
			
				positionIdx--, textureIdx--, normalIdx--;// obj index always starts from 1
				memcpy(vertPtr, tempAlloc0 + (positionIdx * 3 * 4), sizeof(float3));
				texCoordPtr[0] = ConvertFloatToHalf(texcoords[textureIdx * 2 + 0]);
				texCoordPtr[1] = ConvertFloatToHalf(1.0f - texcoords[textureIdx * 2 + 1]);
				normalPtr[0] = ConvertFloatToHalf(normals[normalIdx * 3 + 0]);
				normalPtr[1] = ConvertFloatToHalf(normals[normalIdx * 3 + 1]);
				normalPtr[2] = ConvertFloatToHalf(normals[normalIdx * 3 + 2]);
			}

			tri->materialIndex = currentMaterial;
			
			// skip line&whiteSpace
			while (IsWhitespace(*curr) || *curr == '\n') curr++;
		}

		if (*curr == 'o' || *curr == 'm' || *curr == 's')  while (*curr++ != '\n'); // skip line, header is unknown|unused
	}
	delete[] objText;
	Helper::ChangeExtension(pathDup, "clm", (uint)pathLen);
	AssetManager_SaveMeshToDisk(pathDup, mesh, msz);
	return mesh;
}

constexpr uint CMeshVersion = 0;
constexpr uint CTextureVersion = 0;

static void AssetManager_SaveMeshToDisk(char* path, ObjMesh* mesh, uint msz)
{
	std::ofstream stream = std::ofstream(path, std::ios::out | std::ios::binary);
	uint temp = CMeshVersion;
	stream.write((char*)&temp, sizeof(uint));
	stream.write((char*)&mesh->numTris, sizeof(int));
	stream.write((char*)&mesh->numMaterials, sizeof(int));
	stream.write((char*)mesh->materials, sizeof(ObjMaterial) * mesh->numMaterials);
	
	stream.write((char*)&msz, sizeof(uint));
	stream.write(mesh->mtlText, msz);

	if (mesh->numTris < 1000) {
		stream.write((char*)mesh->tris, mesh->numTris * sizeof(Tri));
	}
	else {
		qlz_state_compress* state_compress = new qlz_state_compress();

		AssetManagerTempReserve(&tempAlloc0, &sizeTemp0, mesh->numTris * sizeof(Tri) + 64);
		char* buff = tempAlloc0;
		size_t triCompSize = qlz_compress(mesh->tris, buff, sizeof(Tri) * mesh->numTris, state_compress);
		
		stream.write((char*)&triCompSize, sizeof(size_t));
		stream.write((char*)buff, triCompSize); // write compressed data to file	
		delete state_compress; // vertex compressing end
	}
	stream.close();
}

static ObjMesh* AssetManager_LoadMeshFromDisk(const char* path, Tri* triArena)
{
	std::ifstream triFile(path, std::ios::in | std::ios::binary | std::ios::failbit);
	SkipBOM(triFile);
	ObjMesh* result = new ObjMesh;
	result->tris = triArena;

	uint meshVersion;
	triFile.read((char*)&meshVersion, sizeof(uint));
	triFile.read((char*)&result->numTris, sizeof(int));
	triFile.read((char*)&result->numMaterials, sizeof(int));
	
	triFile.read((char*)&result->materials, sizeof(ObjMaterial) * result->numMaterials);

	uint msz;
	triFile.read((char*)&msz, sizeof(uint));
	result->mtlText = (char*)malloc(msz);
	triFile.read(result->mtlText, msz);
	
	if (meshVersion != CMeshVersion)   AXERROR("mesh version is not same!"), exit(0);

	if (result->numTris < 1000)
		triFile.read((char*)triArena, result->numTris * sizeof(Tri));
	else
	{
		qlz_state_decompress* state_decompress = new qlz_state_decompress();
		size_t triCompSize = 0; // get compressed size of vertices
		triFile.read((char*)&triCompSize, sizeof(size_t));
		
		AssetManagerTempReserve(&tempAlloc0, &sizeTemp0, triCompSize);
		char* buff = (char*)tempAlloc0;
		triFile.read(buff, triCompSize); // read compressed vertex data
		// decompress vertex data to vertices
		qlz_decompress(buff, triArena, state_decompress);
		delete state_decompress; // reading vertex data end
	}
	triFile.close();
	return result;
}

ObjMesh* AssetManager_ImportMesh(const char* path, Tri* triArena)
{
	char* dupPath = _strdup(path);
	size_t pathLen = strlen(path);

	Helper::ChangeExtension(dupPath, "clm", pathLen); // change to .clm format and search if it exist below
	ObjMesh* result = nullptr;

	// load .clm mesh(our custom)
	if (std::filesystem::exists(dupPath)) {
		result = AssetManager_LoadMeshFromDisk(dupPath, triArena);
	}
	else {
		result = AssetManager_ImportObj(path, triArena, dupPath, pathLen);
	}
	free(dupPath);
	return result;
}

void AssetManager_DestroyMesh(ObjMesh* mesh) 
{
	free(mesh->mtlText); 
	delete mesh;
	// _aligned_free(mesh->tris);
}

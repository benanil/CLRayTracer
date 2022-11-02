#include <filesystem>
#include <fstream>
#include "AssetManager.hpp"
#include "CLHelper.hpp"
#include "Logger.hpp"
#include "Math/Random.hpp"
#include "quicklz.h"

constexpr uint CMeshVersion = 0;

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

inline int PointerDistance(const char* less, const char* higher)
{
	int result = 0;
	while (less++ < higher) result++;
	return result;
}

inline void ChangeExtension(char* path, const char* newExt, int len)
{
	path[len - 1] = newExt[2]; path[len - 2] = newExt[1]; path[len - 3] = newExt[0];
}

void AssetManager_SaveMeshToDisk(char* path, ObjMesh* mesh, uint msz);

static ObjMesh* AssetManager_ImportObj(const char* path, Tri* triArena, char* pathDup, size_t pathLen)
{
	ChangeExtension(pathDup, "mtl", pathLen);
	char* mtlPath = pathDup; // for readibility

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
	uint msz = std::filesystem::exists(mtlPath) ? (uint)std::filesystem::file_size(mtlPath) : 0u;

	char* objText = Helper::ReadAllText(path);
	// read if material path exist 
	if (msz) mesh->mtlText = Helper::ReadAllText(mtlPath);

	// todo string pool
	// todo use arena allocator or some part of the .obj text data
	float* positions  = (float*)malloc(1024 * sizeof(float3));
	float* texCoords = (float*)malloc(1024 * sizeof(float2));
	float* normals   = (float*)malloc(1024 * sizeof(float3)); // todo

	int numVertices = 0, numTexCoords = 0, numNormals = 0;

	int sizeVertices = 1024, sizeTexCoords = 1024, sizeNormals = 1024; // size of the arrays 
	float* currVertices = positions, *currTexCoords = texCoords, *currNormals = normals;
	
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
			currMaterial->shininess = 3.0f, currMaterial->roughness = 0.65f;
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
			f = Clamp(f, 0.0f, 100.0f) / 100.0f;
			currMaterial->shininess = (ushort)(f * 65000.0f);
		}
		else if (curr[0] == 'd') { // roughness
			float f;
			curr = ParseFloat(&f, curr + 2);
			currMaterial->roughness = (ushort)(Clamp(f, 0.0f, 1.0f) * 65000.0f);
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
				if (numVertices + 1 >= sizeVertices) {
					sizeVertices += sizeVertices / 2;
					positions = (float*)realloc(positions, sizeVertices * sizeof(float3));
					currVertices = positions + (numVertices * 3);
				}
			
				curr = ParseFloat(currVertices++, curr); 
				curr = ParseFloat(currVertices++, curr); 
				curr = ParseFloat(currVertices++, curr); numVertices++;
				// skip line&whiteSpace
				while (*curr == '\n' || IsWhitespace(*curr)) curr++;
			}

			while (curr[1] == 't') {
				curr += 2;
				if (numTexCoords + 1 >= sizeTexCoords) {
					sizeTexCoords += sizeTexCoords / 2;
					texCoords = (float*)realloc(texCoords, sizeTexCoords * sizeof(float2));
					currTexCoords = texCoords + (numTexCoords * 2);
				}
			
				curr = ParseFloat(currTexCoords++, curr); 
				curr = ParseFloat(currTexCoords++, curr); numTexCoords++;
				// skip line&whiteSpace
				while (*curr == '\n' || IsWhitespace(*curr)) curr++;
			}

			while (curr[1] == 'n') {
				curr += 2; 
				if (numNormals + 1 >= sizeNormals) {
					sizeNormals += sizeNormals / 2;
					normals = (float*)realloc(normals, sizeNormals * sizeof(float3));
					currNormals = normals + (numNormals * 3);
				}
			
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
			short* texCoordPtr = &tri->uv0x;
			short* normalPtr = &tri->normal0x;

			for(int i = 0; i < 3; ++i, texCoordPtr += 2, vertPtr += 4, normalPtr += 3) // compiler please unroll :D
			{
				int positionIdx = 0, textureIdx = 0, normalIdx = 0;
				// since we know indices are not negative values we are parsing like this
				while (IsNumber(*curr)) positionIdx = 10 * positionIdx + (*curr++ - '0'); curr++; // last ptr++ for jump '/'
				while (IsNumber(*curr)) textureIdx = 10 * textureIdx + (*curr++ - '0'); curr++;
				while (IsNumber(*curr)) normalIdx = 10 * normalIdx + (*curr++ - '0'); curr++;
			
				positionIdx--, textureIdx--, normalIdx--;// obj index always starts from 1
				std::memcpy(vertPtr, positions + (positionIdx * 3), sizeof(float3));
				texCoordPtr[0] = short(texCoords[textureIdx * 2 + 0] * 32766.0f);
				texCoordPtr[1] = short((1.0f - texCoords[textureIdx * 2 + 1]) * 32766.0f);
				normalPtr[0] = short(normals[normalIdx * 3 + 0] * 32766.0f);
				normalPtr[1] = short(normals[normalIdx * 3 + 1] * 32766.0f);
				normalPtr[2] = short(normals[normalIdx * 3 + 2] * 32766.0f);
			}

			tri->materialIndex = currentMaterial;
			
			// skip line&whiteSpace
			while (IsWhitespace(*curr) || *curr == '\n') curr++;
		}

		if (*curr == 'o' || *curr == 'm' || *curr == 's')  while (*curr++ != '\n'); // skip line, header is unknown|unused
	}
	free(positions), free(texCoords), free(normals);
	delete[] objText;
	ChangeExtension(pathDup, "clm", (uint)pathLen);
	AssetManager_SaveMeshToDisk(pathDup, mesh, msz);
	return mesh;
}

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
		// todo use arena allocator
		char* buff = (char*)malloc(mesh->numTris * sizeof(Tri) + 64);
		size_t triCompSize = qlz_compress(mesh->tris, buff, sizeof(Tri) * mesh->numTris, state_compress);
		
		stream.write((char*)&triCompSize, sizeof(size_t));
		stream.write((char*)buff, triCompSize); // write compressed data to file	
		delete state_compress; // vertex compressing end

		free(buff);
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
	result->mtlText = new char[msz];
	triFile.read(result->mtlText, msz);

	if (meshVersion != CMeshVersion)  
		AXERROR("mesh version is not same!"), exit(0); 

	if (result->numTris < 1000)
		triFile.read((char*)triArena, result->numTris * sizeof(Tri));
	else
	{
		qlz_state_decompress* state_decompress = new qlz_state_decompress();
		size_t triCompSize = 0; // get compressed size of vertices
		triFile.read((char*)&triCompSize, sizeof(size_t));
		// allocate buffer for reading compressed data from file
		char* buff = (char*)malloc(triCompSize);
		triFile.read(buff, triCompSize); // read compressed vertex data
		// decompress vertex data to vertices
		qlz_decompress(buff, triArena, state_decompress);
		delete state_decompress; // reading vertex data end
		free(buff);
	}
	triFile.close();
	return result;
}

ObjMesh* AssetManager_ImportMesh(const char* path, Tri* triArena)
{
	char* dupPath = _strdup(path);
	size_t pathLen = strlen(path);

	ChangeExtension(dupPath, "clm", pathLen); // change to .clm format and search if it exist below
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
	delete[] mesh->mtlText; 
	delete mesh;
	// _aligned_free(mesh->tris);
}

#include "CLHelper.hpp"
#include <fstream>
#include <filesystem>
#include "Logger.hpp"
#include "ResourceManager.hpp"

static void SkipBOM(std::ifstream& in)
{
	char test[3] = { 0 };
	in.read(test, 3);
	if ((unsigned char)test[0] == 0xEF &&
		(unsigned char)test[1] == 0xBB &&
		(unsigned char)test[2] == 0xBF) {
		return;
	}
	in.seekg(0);
}

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
	*f = sign * num;
	return ptr;
}

ObjMesh* Helper::ImportObj(const char* path, Tri* triArena)
{
	char* mtlPath = _strdup(path);
	size_t pathLen = strlen(path);
	mtlPath[pathLen-1] = 'l'; mtlPath[pathLen-2] = 't'; mtlPath[pathLen-3] = 'm';
	bool isSponza = path[pathLen - 5] == 'a' && path[pathLen - 6] == 'z' && path[pathLen - 7] == 'n';

	if (!std::filesystem::exists(path)) {
		AXERROR("mesh file is not exist!\n %c", path);
		return nullptr;
	}

	std::ifstream meshFile(path, std::ios::in | std::ios::binary | std::ios::failbit);
	SkipBOM(meshFile); 
	
	ObjMesh* mesh = new ObjMesh; 
	mesh->numMaterials = 0, mesh->numTris = 0;
	mesh->tris = triArena;

	const uintmax_t sz = std::filesystem::file_size(path);
	uintmax_t msz = std::filesystem::exists(mtlPath) ? std::filesystem::file_size(mtlPath) : 0;

	mesh->textMem = new char[sz + 1 + msz + 1]; // we delete this after we destroy the obj
	mesh->textMem[sz] = '\0';

	if (msz) { // material path exist 
		std::ifstream mtlFile(mtlPath, std::ios::in | std::ios::binary | std::ios::failbit);
		SkipBOM(mtlFile);
		mesh->textMem[sz + msz] = '\0';
		mtlFile.read(mesh->textMem + sz, msz);
		mtlFile.close();
	}

	meshFile.read(mesh->textMem, sz);
	meshFile.close();
	free(mtlPath); // path text
	// todo string pool
	// todo use arena allocator or some part of the .obj text data
	float* positions  = (float*)malloc(1024 * sizeof(float3));
	float* texCoords = (float*)malloc(1024 * sizeof(float2));
	// float* normals   = (float*)malloc(1024 * sizeof(float) * 3); // todo

	int numVertices = 0, numTexCoords = 0;//numNormals = 0;

	int sizeVertices = 1024, sizeTexCoords = 1024;//, sizeNormals = 1024; // size of the arrays 
	float* currVertices = positions, *currTexCoords = texCoords;//, *currNormals = normals;
	
	// hashMap for material indices, materialMap[materialNameHash & 64] = material Index 
	unsigned char materialMap[128] = {0};
	
	// import materials
	char* curr = mesh->textMem + sz + 1, *currEnd = curr + msz;
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
			currMaterial->shininess = 0.5f;
			currMaterial->diffusePath = nullptr, currMaterial->specularPath = nullptr;
			currMaterial->name = curr;
			unsigned hash = 5381;
			while(*curr != '\n' && !IsWhitespace(*curr))
				hash = ((hash << 5) + hash) + *curr++;
			*curr++ = '\0'; // null terminator
			//if (materialMap[hash & 127])
			//	AXERROR("mesh importing failed material hash collision detected!"), exit(0);

			materialMap[hash & 127] = mesh->numMaterials++;
		}
		else if (curr[0] == 'N' && curr[1] == 's') { // shininess
			curr = ParseFloat(&currMaterial->shininess, curr + 2);
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
				currMaterial->diffusePath = (curr += 7);
				// find end point of path
				while (*curr != '\n' && *curr != '\r') curr++;
				*curr++ = '\0'; // addd null terminator
			}
			else if (curr[4] == 'K' && curr[5] == 's') {
				currMaterial->specularPath = (curr += 7); // set this pointer as texture path data
				while (*curr != '\n' && *curr != '\r') curr++; // find end point of path
				*curr++ = '\0'; // addd null terminator
			}
			else while (*curr != '\n') curr++;
		} // skip line. header is unknown | unused
		else while (*curr != '\n' && *curr) curr++;
	}

	curr = mesh->textMem, currEnd = curr + sz;
	unsigned currentMaterial = 0;
	unsigned groupOffset = 1, groupOffsetTexture = 1;

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
				// skip normals for now
				while (*curr++ != '\n'); // skip line
				while (IsWhitespace(*curr)) curr++;
			}
		} 
		
		if (curr[0] == 'u' && curr[1] == 's' && curr[2] == 'e') // && curr[3] == 'm' && curr[4] == 't' no need
		{
			curr += 7; // skip usemtl + space
			unsigned hash = 5381;
			while(*curr != '\n' && !IsWhitespace(*curr)) // create texture path hash 
				hash = ((hash << 5) + hash) + *curr++;
			while (*curr == '\n' || IsWhitespace(*curr)) curr++;
			currentMaterial = materialMap[hash & 127]; // use material index for this group of triangles
		}
		bool groupProcessed = 0;

		while (curr[0] == 'f')
		{
			groupProcessed = true;
			curr += 2;
			int vtn = 0; // number of indices = vertex, texture and normal v, vt, vtn?
			Tri* tri = mesh->tris + mesh->numTris; mesh->numTris++;
			// for now we cant import more than 1mn triangle for each gpu push
			if (mesh->numTris >= 1'000'000) // ResourceManager::Max_Triangles
				AXERROR("too many triangles for mesh!"), exit(0);

			float* vertPtr = (float*)tri; 
			half* texCoordPtr = &tri->uv0x;

			for(int i = 0; i < 3; ++i, texCoordPtr += 2, vertPtr += 4) // compiler please unroll :D
			{
				int positionIdx = 0, textureIdx = 0, normalIdx = 0;
				// since we know indices are not negative values we are parsing like this
				while (IsNumber(*curr)) positionIdx = 10 * positionIdx + (*curr++ - '0'); curr++; // last ptr++ for jump '/'
				while (IsNumber(*curr)) textureIdx = 10 * textureIdx + (*curr++ - '0'); curr++;
				while (IsNumber(*curr)) curr++; curr++;// normalIdx = 10 * curr + (*ptr++ - '0'); ptr++; // no normal yet but its here for now
			
				positionIdx -= groupOffset, textureIdx -= groupOffsetTexture; // obj index always starts from 1
				std::memcpy(vertPtr, positions + (positionIdx * 3), sizeof(float3));
				texCoordPtr[0] = ConvertFloatToHalf(texCoords[textureIdx * 2 + 0]);
				texCoordPtr[1] = ConvertFloatToHalf(1.0f - texCoords[textureIdx * 2 + 1]);
			}

			tri->materialIndex = currentMaterial;
			
			// skip line&whiteSpace
			while (IsWhitespace(*curr) || *curr == '\n') curr++;
		}

		//if (groupProcessed && !isSponza) { // we don't want to push all group triangles to  
		//	groupOffset += numVertices;
		//	groupOffsetTexture += numTexCoords;
		//	numVertices = numTexCoords = 0;
		//	currTexCoords = texCoords;
		//	currVertices = positions;
		//}

		if (*curr == 'o' || *curr == 'm' || *curr == 's')  while (*curr++ != '\n'); // skip line, header is unknown|unused
	}
	free(positions), free(texCoords);
	printf("done");
	return mesh;
}

void Helper::DestroyObj(ObjMesh* mesh) 
{
	delete[] mesh->textMem; 
	delete mesh;
	// _aligned_free(mesh->tris);
}

char* Helper::ReadAllText(const char* path)
{
	if (!std::filesystem::exists(path)) {
		AXERROR("file is not exist!\n %c", path);
		return nullptr;
	}

	std::ifstream f(path, std::ios::in | std::ios::binary | std::ios::failbit);
	SkipBOM(f);

	const uintmax_t sz = std::filesystem::file_size(path);
	char* code = new char[sz+1];
	f.read(code, sz);
	code[sz] = 0;

	printf(code); printf("\n");

	f.close();
	return code;
}

char* Helper::ReadCombineKernels()
{
	if (!std::filesystem::exists("kernels/kernel_main.cl") || !std::filesystem::exists("kernels/MathAndSTL.cl") ) {
		AXERROR("one or more kernel file is not exist!\n %c");
		return nullptr;
	}

	std::ifstream f ("kernels/kernel_main.cl", std::ios::in | std::ios::binary | std::ios::failbit);
	std::ifstream fm("kernels/MathAndSTL.cl" , std::ios::in | std::ios::binary | std::ios::failbit);
	
	SkipBOM(f); SkipBOM(fm);
	const uintmax_t msz = std::filesystem::file_size("kernels/MathAndSTL.cl");
	const uintmax_t sz  = std::filesystem::file_size("kernels/kernel_main.cl");

	char* code = (char*)ResourceManager::GetAreaPointer();
	fm.read(code, msz);
	f.read(code + msz, sz);

	code[sz + msz] = '\0';

	printf(code); printf("\n");

	f.close(); fm.close();
	return code;
}
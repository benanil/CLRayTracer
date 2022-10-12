#include "ResourceManager.hpp"
#define __SSE__
#define __SSE2__
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <malloc.h>
#include "Logger.hpp"

// 3 = num color channels

namespace ResourceManager
{
	cl_mem textureHandleMem, textureDataMem;
	Texture textures[CL_MAX_TEXTURES * 4];
	unsigned char* arena;

	size_t arenaOffset = 6;
	int    numTextures = 2;
}

cl_mem* ResourceManager::GetTextureHandleMem() { return &textureHandleMem; }
cl_mem* ResourceManager::GetTextureDataMem()   { return &textureDataMem; }

TextureHandle ResourceManager::ImportTexture(const char* path)
{
	Texture& texture = textures[numTextures]; int channels;
    unsigned char* ptr = stbi_load(path, &texture.width, &texture.height, &channels, 3);
	memcpy(arena + arenaOffset, ptr, texture.height * texture.width * 3);
	texture.offset = arenaOffset / 3;
	arenaOffset += texture.width * texture.height * 3;
	free(ptr);
	return numTextures++;
}

void ResourceManager::PushTexturesToGPU(cl_context context)
{
	cl_int clerr;
	textureDataMem   = clCreateBuffer(context, CL_MEM_COPY_HOST_PTR, arenaOffset, arena, &clerr); assert(clerr == 0);
	textureHandleMem = clCreateBuffer(context, CL_MEM_COPY_HOST_PTR, sizeof(Texture) * numTextures, textures, &clerr); assert(clerr == 0);
}

void ResourceManager::Initialize()
{
	arena = new unsigned char[2048 * 2048 * CL_MAX_TEXTURES * 3];  
	// create default textures. white, black
	textures[0].width  = 1;  textures[1].width = 1;
	textures[0].height = 1;  textures[1].height = 1;
	textures[0].offset = 0;  textures[1].offset = 3;
	arena[0] = 0xFF; arena[1] = 0xFF; arena[2] = 0xFF; 
	arena[3] = 0;    arena[4] = 0;    arena[5] = 0;

	arenaOffset = 6; numTextures = 2;
}

void ResourceManager::Destroy()
{
	clReleaseMemObject(textureHandleMem);
	clReleaseMemObject(textureDataMem);
	delete[] arena;
}

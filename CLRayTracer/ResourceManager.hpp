#pragma once
#include "cl.hpp"
#include "Common.hpp"

// max number of textures note: assumed all textures are 2k, 
// max texture size is 4k for above you need to increment this value
#ifndef CL_MAX_TEXTURES
#	define CL_MAX_TEXTURES 40
#endif

struct Texture
{
	int width, height, offset, padd;
};

typedef int TextureHandle;

namespace ResourceManager
{
	TextureHandle ImportTexture(const char* path);
	
	inline TextureHandle GetWhiteTexture() { return 0; }
	inline TextureHandle GetBlackTexture() { return 1; }
	
	cl_mem* GetTextureHandleMem();
	cl_mem* GetTextureDataMem();
	void Initialize();
	void Destroy();

	void PushTexturesToGPU(cl_context context);
}


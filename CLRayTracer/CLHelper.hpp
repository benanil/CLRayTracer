#pragma once
#include "ResourceManager.hpp"

#define SkipBOM(in)                           \
{                                             \
	char test[3] = { 0 };                     \
	in.read(test, 3);                         \
	if (!((unsigned char)test[0] == 0xEF &&   \
		(unsigned char)test[1] == 0xBB &&     \
		(unsigned char)test[2] == 0xBF)) {    \
		in.seekg(0);                          \
	}                                         \
}

namespace Helper
{
	char* ReadAllText(const char* path);
	void WriteAllText(const char* path, const char* text);
	char* ReadCombineKernels();
}

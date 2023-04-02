#pragma once

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
	inline void ChangeExtension(char* path, const char* newExt, size_t len)
	{
		path[len - 1] = newExt[2]; path[len - 2] = newExt[1]; path[len - 3] = newExt[0];
	}

	inline char* GetPathName(char* path)
	{
		int lastSeperator = 0, i = 0;
		char* ptr = path;
		while (*path) {
			if (*path == '//' || *path == '/') lastSeperator = i + 1;
			path++, i++;
		}
		return ptr + lastSeperator;
	}
		
	char* ReadAllText(const char* path);
	void WriteAllText(const char* path, const char* text);
	char* ReadCombineKernels();
}

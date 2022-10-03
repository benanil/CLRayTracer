#include "CLHelper.hpp"
#include <fstream>
#include <filesystem>
#include "Logger.hpp"

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

char* Helper::ReadAllText(const char* path)
{
	if (!std::filesystem::exists(path)) {
		AXERROR("file is not exist!\n %c", path);
		return nullptr;
	}

	std::ifstream f(path, std::ios::in | std::ios::binary | std::ios::failbit);
	SkipBOM(f);

	cl_int clerr;
	const uintmax_t sz = std::filesystem::file_size(path);
	char* code = new char[sz+1];
	f.read(code, sz);
	code[sz] = 0;

	printf(code); printf("\n");

	f.close();
	return code;
}

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
	std::ifstream fm("kernels/MathAndSTL.cl"       , std::ios::in | std::ios::binary | std::ios::failbit);
	
	SkipBOM(f); SkipBOM(fm);
	const uintmax_t msz = std::filesystem::file_size("kernels/MathAndSTL.cl");
	const uintmax_t sz  = std::filesystem::file_size("kernels/kernel_main.cl");

	char* code = new char[sz + msz +1];
	fm.read(code, msz);
	f.read(code + msz, sz);

	code[sz + msz] = '\0';

	printf(code); printf("\n");

	f.close(); fm.close();
	return code;
}
#include "CLHelper.hpp"
#include <fstream>
#include <filesystem>
#include "Logger.hpp"

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
	f.close();
	return code;
}

void Helper::WriteAllText(const char* path, const char* text)
{
	std::ofstream ofs(path, std::ios::out | std::ios::binary | std::ios::failbit);
	const uintmax_t sz = std::filesystem::file_size(path);
	ofs.write(text, sz);
	ofs.close();
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
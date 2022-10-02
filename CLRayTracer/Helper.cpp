#include "CLHelper.hpp"
#include <fstream>
#include <filesystem>

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

cl::Program::Program(cl_context context, const char* path)
{
	if (!std::filesystem::exists(path)) {
		printf("file is not exist! %c ", path);
	}

	std::ifstream f(path, std::ios::in | std::ios::binary | std::ios::failbit);
	SkipBOM(f);

	cl_int clerr;
	const uintmax_t sz = std::filesystem::file_size(path);
	char* code = new char[sz+1];
	f.read(code, sz);
	code[sz] = 0;

	printf(code); printf("\n");

	program = clCreateProgramWithSource(context, 1, (const char**)&code, NULL, &clerr);

	// compile the program
	if (clBuildProgram(program, 0, NULL, NULL, NULL, NULL) != CL_SUCCESS)
	{
		printf("Error building program\n");
		assert(0);
	}
	f.close();
	delete[] code;
}

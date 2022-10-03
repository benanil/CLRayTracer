#pragma once
#include "cl.hpp"
#include <stdio.h>
#include <cassert>

enum eMemFlags
{
	eMemRead = CL_MEM_READ_ONLY,
	eMemWrite = CL_MEM_WRITE_ONLY,
	eMemReadWrite = CL_MEM_READ_WRITE,
	eMemCopyHostPtr = CL_MEM_COPY_HOST_PTR
};

namespace cl
{
	struct Kernel
	{
		cl_kernel kernel;

		operator cl_kernel() const { return kernel; }
		Kernel() {}
		Kernel(cl_program program, const char* funcName)
		{
			cl_int clerr;
			kernel = clCreateKernel(program, funcName, &clerr);
			assert(clerr >= 0);
		}

		template<typename T>
		cl_int SetArg(cl_uint index, T* data) {
			return clSetKernelArg(kernel, index, sizeof(T), data);
		}

		~Kernel() {
			clReleaseKernel(kernel);
		}
	};
}

namespace Helper
{
	char* ReadAllText(const char* path);
}
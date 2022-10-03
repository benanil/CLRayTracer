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

namespace Helper
{
	char* ReadAllText(const char* path);
}
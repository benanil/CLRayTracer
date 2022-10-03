#include <stdio.h>
#include <Windows.h>
#include <wingdi.h>
#include <glad/glad.h>
#include <cassert>
#include "cl.hpp"
#include "Renderer.hpp"
#include "CLHelper.hpp"
#include "Window.hpp"
#include <algorithm>
#include <thread>
#include "Logger.hpp"

// todo: parse kernel from file
//	     abstract buffer
//       create ray generator

namespace Renderer
{
	const GLchar* vertexShaderSource = "#version 330 core\n\
noperspective out vec2 texCoord;\
void main()\
{\
	float x = -1.0 + float((gl_VertexID & 1) << 2);\
	float y = -1.0 + float((gl_VertexID & 2) << 1);\
	texCoord.x = (x + 1.0) * 0.5;\
	texCoord.y = (y + 1.0) * 0.5;\
	gl_Position = vec4(x, y, 0, 1);\
}";

	const GLchar* fragmentShaderSource = "#version 330 core\n\
out vec4 color;\
noperspective in vec2 texCoord;\
uniform sampler2D texture0;\
void main()\
{\
	color = texture(texture0, texCoord);\
}";
	
	enum KernelIndex
	{
		eKernelGenRays, eKernelTrace
	};

	cl_context context;
	cl::Kernel kernel;
	cl_command_queue command_queue;
	cl_program program;
	cl_int clerr;
	cl_mem input;

	GLuint VAO;
	GLuint shaderProgram;
	GLuint screenTexture;
	GLuint textureLoc;
}

typedef void (*GLFWglproc)(void);
extern "C" {  GLFWglproc glfwGetProcAddress(const char* procname); }

int Renderer::Initialize()
{
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		printf("Failed to initialize GLAD"); return 0;
	}
	
	std::this_thread::yield();
	std::this_thread::yield();
	std::this_thread::yield();
	std::this_thread::yield();

	// initialize openCL
	{
		constexpr int DATA_SIZE = 10;
		int inputData[DATA_SIZE] = { 2, 4, 6, 8, 16, 32, 64, 128, 256, 512 };

		cl_uint num_of_platforms = 0;
		cl_platform_id platform_id;
		// retreives a list of platforms available
		if (clGetPlatformIDs(1, &platform_id, &num_of_platforms) != CL_SUCCESS)
		{
			printf("Unable to get platform_id\n"); return 0;
		}

		cl_uint num_of_devices = 0;
		cl_device_id device_id;
		// try to get a supported GPU device
		if (clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_GPU, 1, &device_id, &num_of_devices) != CL_SUCCESS)
		{
			printf("Unable to get device_id\n"); return 0;
		}

		// context properties list - must be terminated with 0
		cl_context_properties properties[] =
		{
			CL_GL_CONTEXT_KHR, (cl_context_properties)wglGetCurrentContext(),
			CL_WGL_HDC_KHR, (cl_context_properties)wglGetCurrentDC(),
			CL_CONTEXT_PLATFORM, (cl_context_properties)platform_id,
			0
		};

		// create a context with the GPU device
		context = clCreateContext(properties, 1, &device_id, NULL, NULL, &clerr);

		// create command queue using the context and device
		command_queue = clCreateCommandQueueWithProperties(context, device_id, 0, &clerr);

		char* kernelCode = Helper::ReadAllText("../kernels/kernel_main.cl");
		if (!kernelCode) { return 0; }

		program = clCreateProgramWithSource(context, 1, (const char**)&kernelCode, NULL, &clerr); assert(clerr == 0);

		// compile the program
		if (clBuildProgram(program, 0, NULL, NULL, NULL, NULL) != CL_SUCCESS)
		{
			delete[] kernelCode;
			AXERROR("Error building program\n");
			size_t param_value_size;
			clerr = clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &param_value_size);
			assert(clerr == 0);
			char * buildLog = new char[param_value_size];
			clerr = clGetProgramBuildInfo( program, device_id, CL_PROGRAM_BUILD_LOG, param_value_size, buildLog, NULL);
			AXWARNING(buildLog);
			assert(clerr == 0);
			delete[] buildLog;
			return 0;
		}
		
		// specify which kernel from the program to execute
		kernel = cl::Kernel(program, "hello");
		input = clCreateBuffer(context, eMemReadWrite | eMemCopyHostPtr, 4ull * DATA_SIZE, inputData, &clerr); assert(clerr == 0);

		// load data into the input buffer
		clEnqueueWriteBuffer(command_queue, input, CL_TRUE, 0,
			sizeof(int) * DATA_SIZE, inputData, 0, NULL, NULL);
		
		int startVal = 50;

		// set the argument list for the kernel command
		clerr |= kernel.SetArg(0, &input);
		clerr |= kernel.SetArg(1, &startVal);
		size_t global = DATA_SIZE;

		// enqueue the kernel command for execution
		clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL, &global,
			NULL, 0, NULL, NULL);
	
		clFinish(command_queue);

		// // copy the results from out of the output buffer
		clEnqueueReadBuffer(command_queue, input, CL_TRUE, 0,
			sizeof(int) * DATA_SIZE, inputData, 0, NULL, NULL);

		// print the results
		printf("output: ");
		for (int i = 0; i < DATA_SIZE; i++)
		{
			printf("%d ", inputData[i]);
		}
	}

	// Build and compile our shader program
	{
		// Vertex shader
		GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
		glCompileShader(vertexShader);
		// Check for compile time errors
		GLint success;
		GLchar infoLog[512];
		glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
		if (!success)
		{
			glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
			printf("ERROR::SHADER::PROGRAM::Vertex_FAILED\n %c \n", infoLog); return 0;
		}
		// Fragment shader
		GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
		glCompileShader(fragmentShader);
		// Check for compile time errors
		glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
		if (!success)
		{
			glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
			printf("ERROR::SHADER::PROGRAM::FRAGMENT_FAILED\n"); 
			printf(infoLog);
			return 0;
		}
		// Link shaders
		shaderProgram = glCreateProgram();
		glAttachShader(shaderProgram, vertexShader);
		glAttachShader(shaderProgram, fragmentShader);
		glLinkProgram(shaderProgram);
		// Check for linking errors
		glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
		if (!success) {
			glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
			printf("ERROR::SHADER::PROGRAM::LINKING_FAILED\n"); 
			printf(infoLog);
			return 0;
		}
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);
		glUseProgram(shaderProgram);
	}

	textureLoc = glGetUniformLocation(shaderProgram, "texture0");
	
	glGenTextures(1, &screenTexture);
	glBindTexture(GL_TEXTURE_2D, screenTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Window::GetWidth(), Window::GetHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	
	cl::Kernel textureKernel = cl::Kernel(program, "texture");

	cl_mem clglTexture = clCreateFromGLTexture(context, GL_WRITE_ONLY, GL_TEXTURE_2D, 0, screenTexture, &clerr); assert(clerr == 0);

	clerr = clEnqueueAcquireGLObjects(command_queue, 1, &clglTexture, 0, 0, 0); assert(clerr == 0);

	clerr = textureKernel.SetArg(0, &clglTexture);								assert(clerr == 0);

	size_t globalWorkSize[2] = { Window::GetWidth(), Window::GetHeight() };
	size_t globalOffset[2] = { 1, 1 };

	clerr = clEnqueueNDRangeKernel(command_queue, textureKernel, 2, globalOffset, globalWorkSize, 0, 0, 0, 0); assert(clerr == 0);
	clerr = clEnqueueReleaseGLObjects(command_queue, 1, &clglTexture, 0, 0, 0); assert(clerr == 0);

	{ // create empty vao unfortunately this step is necessary for ogl 3.2
		glGenVertexArrays(1, &VAO);
		glBindVertexArray(VAO);
	}
	return 1;
}

void Renderer::Render()
{
	glClear(GL_COLOR_BUFFER_BIT);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, screenTexture);

	glDrawArrays(GL_TRIANGLES, 0, 3);
}

void Renderer::Terminate()
{
	glDeleteVertexArrays(1, &VAO);
	glDeleteProgram(shaderProgram);

	// cleanup - release OpenCL resources
	clReleaseMemObject(input);
	clReleaseProgram(program);
	clReleaseKernel(kernel);
	clReleaseCommandQueue(command_queue);
	clReleaseContext(context);
}
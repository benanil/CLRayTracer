#define CL_TARGET_OPENCL_VERSION 300
#include <stdio.h>
#include <Windows.h>
#include <wingdi.h>
#include <glad/glad.h>
#include <cassert>
#include "cl.hpp"
#include "Renderer.hpp"
#include "CLHelper.hpp"
#include "Window.hpp"
#include "Logger.hpp"
#include "Math/Camera.hpp"

// todo: 
//		 calculate noise at runtime with time value
//		 vector3 vector2... math
//		 basic ray tracer
//       create ray generator
//		 input callbacks
//       window resize callback

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

	enum KernelIndex {
		eKernelGenRays, eKernelTrace
	};

	cl_context context;
	cl_kernel textureKernel, rayGenKernel;
	cl_command_queue command_queue;
	cl_program program;

	cl_mem clglTexture, rayMem;

	GLuint VAO;
	GLuint shaderProgram;
	GLuint screenTexture;
	Camera camera;
}

typedef void (*GLFWglproc)(void);
extern "C" {  GLFWglproc glfwGetProcAddress(const char* procname); }

int Renderer::Initialize()
{
	camera = Camera(Window::GetWindowScale());

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		printf("Failed to initialize GLAD"); return 0;
	}
	
	// Build and compile our shader program
	{
		// Vertex shader
		GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
		glCompileShader(vertexShader);
		// Check for compile time errors
		GLint success;
		glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success); assert(success);
		// Fragment shader
		GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
		glCompileShader(fragmentShader);
		// Check for compile time errors
		glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success); assert(success);
		// Link shaders
		shaderProgram = glCreateProgram();
		glAttachShader(shaderProgram, vertexShader);
		glAttachShader(shaderProgram, fragmentShader);
		glLinkProgram(shaderProgram);
		// Check for linking errors
		glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success); assert(success);
	
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);
		glUseProgram(shaderProgram);

		glGenTextures(1, &screenTexture);
		glBindTexture(GL_TEXTURE_2D, screenTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); 
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Window::GetWidth(), Window::GetHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glActiveTexture(GL_TEXTURE0);

		// create empty vao unfortunately this step is necessary for ogl 3.2
		glGenVertexArrays(1, &VAO);
		glBindVertexArray(VAO);
	}
	
	cl_int clerr;
	
	// initialize openCL
	
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
	
	// create a context with the GPU devices
	context = clCreateContext(properties, 1, &device_id, NULL, NULL, &clerr);
	// create command queue using the context and device
	command_queue = clCreateCommandQueueWithProperties(context, device_id, nullptr, &clerr);
	
	char* kernelCode = Helper::ReadAllText("kernels/kernel_main.cl");
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
		char* buildLog = new char[param_value_size];
		clerr = clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, param_value_size, buildLog, NULL);
		AXWARNING(buildLog);
		assert(clerr == 0);
		delete[] buildLog;
		return 0;
	}
	
	// specify which kernel from the program to execute
	int rayCount = Window::GetWidth() * Window::GetHeight();
	rayMem = clCreateBuffer(context, eMemReadWrite, sizeof(Vector3f) * rayCount, nullptr, &clerr); assert(clerr == 0);

	textureKernel = clCreateKernel(program, "texture", &clerr); assert(clerr == 0);
	rayGenKernel     = clCreateKernel(program, "RayGen" , &clerr);  assert(clerr == 0);

	clglTexture = clCreateFromGLTexture(context, GL_WRITE_ONLY, GL_TEXTURE_2D, 0, screenTexture, &clerr); assert(clerr == 0);
	
	return 1;
}

void Renderer::Render()
{
	glClear(GL_COLOR_BUFFER_BIT);

	Vector2i windowSize = Window::GetWindowScale();
	size_t globalWorkSize[2] = { windowSize.x, windowSize.y };
	float time = Window::GetTime();

	int rayCount = windowSize.x * windowSize.y;
	cl_int clerr;
	cl_event event;

	clerr = clSetKernelArg(rayGenKernel, 0, sizeof(int), &windowSize.x);                 assert(clerr == 0);
	clerr = clSetKernelArg(rayGenKernel, 1, sizeof(int), &windowSize.y);                 assert(clerr == 0);
	clerr = clSetKernelArg(rayGenKernel, 2, sizeof(cl_mem), &rayMem);                    assert(clerr == 0);
	clerr = clSetKernelArg(rayGenKernel, 3, sizeof(Matrix4), &camera.inverseView);       assert(clerr == 0);
	clerr = clSetKernelArg(rayGenKernel, 4, sizeof(Matrix4), &camera.inverseProjection); assert(clerr == 0);

	clerr = clEnqueueNDRangeKernel(command_queue, rayGenKernel, 2, nullptr, globalWorkSize, 0, 0, 0, &event); assert(clerr == 0);

	clerr = clEnqueueAcquireGLObjects(command_queue, 1, &clglTexture, 0, 0, 0); assert(clerr == 0);
	clerr = clSetKernelArg(textureKernel, 0, sizeof(cl_mem), &clglTexture);     assert(clerr == 0);
	clerr = clSetKernelArg(textureKernel, 1, sizeof(cl_mem), &rayMem);   	 	assert(clerr == 0);
	clerr = clSetKernelArg(textureKernel, 2, sizeof(float), &time);   	

	clerr = clEnqueueNDRangeKernel(command_queue, textureKernel, 2, nullptr, globalWorkSize, 0, 1, &event, 0); assert(clerr == 0);
	clerr = clEnqueueReleaseGLObjects(command_queue, 1, &clglTexture, 0, 0, 0); assert(clerr == 0);

	clFinish(command_queue);

	glDrawArrays(GL_TRIANGLES, 0, 3);
	Window::ChangeName((Window::GetTime() - time) * 1000.0);
}

void Renderer::Terminate()
{
	glDeleteVertexArrays(1, &VAO);
	glDeleteProgram(shaderProgram);

	// cleanup - release OpenCL resources
	clReleaseMemObject(clglTexture);
	clReleaseProgram(program);
	clReleaseCommandQueue(command_queue);
	clReleaseKernel(textureKernel);
	clReleaseContext(context);
}
#define CL_TARGET_OPENCL_VERSION 300
#define __SSE__
#define __SSE2__
#include <Windows.h>
#include <glad/glad.h>
#include <cassert>
#include "cl.hpp"
#include "Renderer.hpp"
#include "CLHelper.hpp"
#include "Window.hpp"
#include "Logger.hpp"
#include "Math/Camera.hpp"
#include "Math/Random.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#include <stb_image.h>

// todo: 
//      specular, reflections
//      plane, shadow, optimize
//      dont render when window is not focused

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
	cl_kernel traceKernel, rayGenKernel;
	cl_command_queue command_queue;
	cl_program program;

	cl_mem clglTexture, rayMem, clglSkybox, sphereMem;
	constexpr int numSpheres = 25;
	Sphere spheres[numSpheres];

	GLuint VAO;
	GLuint shaderProgram;
	GLuint screenTexture, skyboxTexture;
	Camera camera;
}

typedef void (*GLFWglproc)(void);
extern "C" {  GLFWglproc glfwGetProcAddress(const char* procname); }

void CreateGLTexture(GLuint& texture, int width, int height, void* data = nullptr)
{
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); 
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
}

int Renderer::Initialize()
{
	camera = Camera(Window::GetWindowScale());

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		AXERROR("Failed to initialize GLAD"); return 0;
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

		int width, height, channels;
		unsigned char* skyboxData = stbi_load("../Assets/cape_hill_4k.jpg", &width, &height, &channels, 3);
		if (!skyboxData) { AXERROR("cannot load skybox image"); return 0; }
		
		CreateGLTexture(skyboxTexture, width, height, skyboxData);
		CreateGLTexture(screenTexture, Window::GetWidth(), Window::GetHeight());
		glActiveTexture(GL_TEXTURE0);
		
		// create empty vao unfortunately this step is necessary for ogl 3.2
		glGenVertexArrays(1, &VAO);
		glBindVertexArray(VAO);
		free(skyboxData);
	}
	
	cl_int clerr;
	
	// initialize openCL
	
	cl_uint num_of_platforms = 0;
	cl_platform_id platform_id;
	// retreives a list of platforms available
	if (clGetPlatformIDs(1, &platform_id, &num_of_platforms) != CL_SUCCESS) {
		AXERROR("Unable to get platform_id"); return 0;
	}
	
	cl_uint num_of_devices = 0;
	cl_device_id device_id;
	// try to get a supported GPU device
	if (clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_GPU, 1, &device_id, &num_of_devices) != CL_SUCCESS) {
		AXERROR("Unable to get device_id"); return 0;
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
	
	char* kernelCode = Helper::ReadCombineKernels();
	if (!kernelCode) { return 0; }
	
	program = clCreateProgramWithSource(context, 1, (const char**)&kernelCode, NULL, &clerr); assert(clerr == 0);
	
	delete[] kernelCode;
	// compile the program
	if (clBuildProgram(program, 0, NULL, NULL, NULL, NULL) != CL_SUCCESS)
	{
		AXERROR("Error building program");
		size_t param_value_size;
		clerr = clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &param_value_size);
		char* buildLog = new char[param_value_size + 2]; buildLog[0] = '\n';
		clerr = clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, param_value_size, buildLog + 1, NULL);
		AXWARNING(buildLog); 
		delete[] buildLog;
		return 0;
	}
	
	// specify which kernel from the program to execute
	rayMem = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(Vector3f) * Window::GetWidth() * Window::GetHeight(), nullptr, &clerr); assert(clerr == 0);

	traceKernel  = clCreateKernel(program, "Trace", &clerr); assert(clerr == 0);
	rayGenKernel = clCreateKernel(program, "RayGen", &clerr); assert(clerr == 0);

	clglTexture = clCreateFromGLTexture(context, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, screenTexture, &clerr); assert(clerr == 0);
	clglSkybox  = clCreateFromGLTexture(context, CL_MEM_READ_ONLY , GL_TEXTURE_2D, 0, skyboxTexture, &clerr); 
	
	if (clerr != CL_SUCCESS ) {
		AXERROR("texture creation failed with error code: %d", clerr);
		return 0;
	}

	Random::PCG pcg{};

	for (int i = 0; i < 5; i++)
	{
		for (int j = 0; j < 5; j++)
		{
			spheres[i + j * 5].position[0] = i * 2.8f;
			spheres[i + j * 5].position[1] = 1.0f;
			spheres[i + j * 5].position[2] = j * 2.8f;
			spheres[i + j * 5].radius      = 1.0f;
			spheres[i + j * 5].roughness   = i * 0.1f;	
			spheres[i + j * 5].color = pcg.NextBound(255u) | (pcg.NextBound(255u) << 8) | (pcg.NextBound(255u) << 16);	
		}
	}

	sphereMem = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(Sphere) * numSpheres, spheres, &clerr); assert(clerr == 0);

	return 1;
}

void Renderer::OnKeyPressed(int keyCode, int action) {}

void Renderer::OnWindowResize(int width, int height)
{
	clFinish(command_queue); cl_int clerr;
	clReleaseMemObject(clglTexture);
	clReleaseMemObject(rayMem);
	glDeleteTextures(1, &screenTexture);
	rayMem = clglTexture = nullptr; screenTexture = 0u;
	CreateGLTexture(screenTexture, width, height);
	clglTexture = clCreateFromGLTexture(context, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, screenTexture, &clerr);  assert(clerr == 0);
	rayMem = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(Vector3f) * width * height, nullptr, &clerr); assert(clerr == 0);
	glViewport(0, 0, width, height);
	camera.RecalculateProjection(width, height);
}

void Renderer::Render()
{
	glClear(GL_COLOR_BUFFER_BIT);

	Vector2i windowSize = Window::GetWindowScale();
	size_t globalWorkSize[2] = { windowSize.x, windowSize.y };
	float time = Window::GetTime();

	camera.Update();

	struct TraceArgs {
		Vector3f cameraPosition;
		int numSpheres;
	} trace_args;

	trace_args.cameraPosition = camera.position;
	trace_args.numSpheres = numSpheres;

	cl_int clerr; cl_event event;

	// prepare ray generation kernel
	clerr = clSetKernelArg(rayGenKernel, 0, sizeof(int), &windowSize.x);                 assert(clerr == 0);
	clerr = clSetKernelArg(rayGenKernel, 1, sizeof(int), &windowSize.y);                 assert(clerr == 0);
	clerr = clSetKernelArg(rayGenKernel, 2, sizeof(cl_mem), &rayMem);                    assert(clerr == 0);
	clerr = clSetKernelArg(rayGenKernel, 3, sizeof(Matrix4), &camera.inverseView);       assert(clerr == 0);
	clerr = clSetKernelArg(rayGenKernel, 4, sizeof(Matrix4), &camera.inverseProjection); assert(clerr == 0);
	// execute ray generation
	clerr = clEnqueueNDRangeKernel(command_queue, rayGenKernel, 2, nullptr, globalWorkSize, 0, 0, 0, &event); assert(clerr == 0);
	// prepare Rendering
	cl_mem textures[2] = { clglTexture, clglSkybox };
	clerr = clEnqueueAcquireGLObjects(command_queue, 1, &clglTexture, 0, 0, 0); assert(clerr == 0);
	
	clerr = clSetKernelArg(traceKernel, 0, sizeof(cl_mem), &clglTexture);    assert(clerr == 0);
	clerr = clSetKernelArg(traceKernel, 1, sizeof(cl_mem), &clglSkybox);  assert(clerr == 0);
	clerr = clSetKernelArg(traceKernel, 2, sizeof(cl_mem), &rayMem);   	 	assert(clerr == 0);
	clerr = clSetKernelArg(traceKernel, 3, sizeof(cl_mem), &sphereMem);   	assert(clerr == 0);
	clerr = clSetKernelArg(traceKernel, 4, sizeof(TraceArgs), &trace_args);   	assert(clerr == 0);
	clerr = clSetKernelArg(traceKernel, 5, sizeof(float), &time);   	assert(clerr == 0);
	// execute rendering
	clerr = clEnqueueNDRangeKernel(command_queue, traceKernel, 2, nullptr, globalWorkSize, 0, 1, &event, 0); assert(clerr == 0);
	clerr = clEnqueueReleaseGLObjects(command_queue, 2, textures, 0, 0, 0); assert(clerr == 0);

	clFinish(command_queue);

	glDrawArrays(GL_TRIANGLES, 0, 3);
	Window::ChangeName((Window::GetTime() - time) * 1000.0);
}

void Renderer::Terminate()
{
	glDeleteVertexArrays(1, &VAO);
	glDeleteProgram(shaderProgram);
	glDeleteTextures(1, &skyboxTexture); glDeleteTextures(1, &screenTexture);

	// cleanup - release OpenCL resources
	clReleaseMemObject(clglTexture);
	clReleaseMemObject(rayMem);
	clReleaseMemObject(sphereMem);
	clReleaseProgram(program);
	clReleaseCommandQueue(command_queue);
	clReleaseKernel(traceKernel);
	clReleaseContext(context);
}
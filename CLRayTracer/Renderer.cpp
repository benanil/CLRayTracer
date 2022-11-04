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
#include "Random.hpp"
#include "Engine.hpp"

// todo: 
//      dynamicly add meshes, spheres, textures, materials, skybox
//      update gpu buffers
//      run renderer on seperate thread
//      run gameplay, physics etc. code in main thread
//      specular, reflections
//      shadow, optimize, brdf

namespace 
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

	cl_context context;
	cl_kernel traceKernel, rayGenKernel;
	cl_command_queue command_queue;
	cl_program program;

	cl_mem clglScreen, rayMem, sphereMem, instanceMem;
	cl_int clerr;
		
	uint NumSpheres = 0;
	Sphere spheres[Renderer::MaxNumSpheres];
	MeshInstance meshInstances[Renderer::MaxNumInstances];

	uint numMeshInstances = 0u;

	GLuint VAO;
	GLuint shaderProgram;
	GLuint screenTexture;
	Camera camera;
}

typedef void (*GLFWglproc)(void);
extern "C" GLFWglproc glfwGetProcAddress(const char* procname); 

void Renderer::CreateGLTexture(GLuint& texture, int width, int height, void* data)
{
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); 
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
}

SphereHandle Renderer::PushSphere(const Sphere& sphere)
{
	spheres[NumSpheres] = sphere;
	return NumSpheres++;
}

void Renderer::RemoveSphere(SphereHandle sphere)
{
	spheres[sphere] = spheres[NumSpheres--];
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

		CreateGLTexture(screenTexture, Window::GetWidth(), Window::GetHeight());
		glActiveTexture(GL_TEXTURE0);
		
		// create empty vao unfortunately this step is necessary for ogl 3.2
		glGenVertexArrays(1, &VAO);
		glBindVertexArray(VAO);
	}
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

	ResourceManager::Initialize(context, command_queue);
	
	// no need to delete this kernel code memory allocated from arena allocator
	char* kernelCode = Helper::ReadCombineKernels(); 
	if (!kernelCode) { return 0; }
	
	program = clCreateProgramWithSource(context, 1, (const char**)&kernelCode, NULL, &clerr); assert(clerr == 0);
	
	// compile the program
	if (clBuildProgram(program, 0, NULL, "-Werror -cl-single-precision-constant -cl-mad-enable", NULL, NULL) != CL_SUCCESS)
	{
		AXERROR("Error building program");
		size_t param_value_size;
		clerr = clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &param_value_size);
		char* buildLog = new char[param_value_size + 2]; buildLog[0] = '\n';
		clerr = clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, param_value_size, buildLog + 1, NULL);
		AXERROR(buildLog); 
		delete[] buildLog;
		return 0;
	}

	instanceMem = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(MeshInstance) * MaxNumInstances, nullptr, &clerr); assert(clerr == 0);
	
	rayMem = clCreateBuffer(context, 0, sizeof(Vector3f) * Window::GetWidth() * Window::GetHeight(), nullptr, &clerr); assert(clerr == 0);

	traceKernel  = clCreateKernel(program, "Trace", &clerr); assert(clerr == 0);
	rayGenKernel = clCreateKernel(program, "RayGen", &clerr); assert(clerr == 0);

	clglScreen = clCreateFromGLTexture(context, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, screenTexture, &clerr); assert(clerr == 0);
	sphereMem  = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(Sphere) * MaxNumSpheres, nullptr, &clerr); assert(clerr == 0);

	return 1;
}

void Renderer::UpdateSpheres()
{
	clerr = clEnqueueWriteBuffer(command_queue, sphereMem, false, 0, NumSpheres * sizeof(Sphere), spheres, 0, 0, 0); assert(clerr == 0);
}

void Renderer::OnKeyPressed(int keyCode, int action) {}

void Renderer::OnWindowResize(int width, int height)
{
	if (width < 16 || height < 16) return;
	clFinish(command_queue); cl_int clerr;
	clReleaseMemObject(clglScreen);
	clReleaseMemObject(rayMem);
	glDeleteTextures(1, &screenTexture);
	rayMem = clglScreen = nullptr; screenTexture = 0u;
	CreateGLTexture(screenTexture, width, height);
	clglScreen = clCreateFromGLTexture(context, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, screenTexture, &clerr);  assert(clerr == 0);
	rayMem = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(Vector3f) * width * height, nullptr, &clerr); assert(clerr == 0);
	glViewport(0, 0, width, height);
	camera.RecalculateProjection(width, height);
}

static uint numRegisteredInstances = 0, lastRegisterInstanceIndex = 0;

void Renderer::BeginInstanceRegister() {
	numRegisteredInstances = 0;
}

MeshInstanceHandle Renderer::RegisterMeshInstance(
	 MeshHandle handle, MaterialHandle materialHandle, float3 position)
{
	if (numMeshInstances >= MaxNumInstances) { AXERROR("too many mesh instances please reduce number of instances or increase number of instances!"); exit(0); }
	
	if (materialHandle == ResourceManager::DefaultMaterial) {
		materialHandle = ResourceManager::GetMeshInfo(handle).materialStart;
	}
	MeshInstance& instance = meshInstances[numMeshInstances++];
	instance.meshIndex = handle;
	instance.materialStart = materialHandle;
	instance.position = position;
	numRegisteredInstances++;
	return numMeshInstances-1;
}

void Renderer::EndInstanceRegister() {
	clerr = clEnqueueWriteBuffer(command_queue, instanceMem, true, lastRegisterInstanceIndex * sizeof(MeshInstance), numRegisteredInstances * sizeof(MeshInstance), meshInstances + lastRegisterInstanceIndex, 0, 0, 0);	
	lastRegisterInstanceIndex += numRegisteredInstances;
	numRegisteredInstances = 0;
}

static ushort removedInstances[50]; // we can remove max 50 objects each frame
static ushort numRemovedInstances = 0;
static ushort MinUpdatedInstanceIndex = 0xFFFFu, MaxUpdatedInstanceIndex = 0u;
static bool shouldUpdateInstances = 0;
static bool hasRemovedInstances = 0;
void Renderer::RemoveMeshInstance(MeshInstanceHandle handle) {
	if (numRegisteredInstances == 0) { AXERROR("you cant remove mesh instances while registering instances!"); exit(0); }
	hasRemovedInstances = true;
	removedInstances[numRemovedInstances++] = handle;
}

void Renderer::SetMeshInstanceMaterial(MeshInstanceHandle instanceHandle, MaterialHandle materialHandle)
{
	meshInstances[instanceHandle].materialStart = materialHandle; shouldUpdateInstances = true;
	MinUpdatedInstanceIndex = Min(instanceHandle, (uint)MinUpdatedInstanceIndex);
	MaxUpdatedInstanceIndex = Max(instanceHandle, (uint)MaxUpdatedInstanceIndex);
}

void Renderer::SetMeshPosition(MeshInstanceHandle instanceHandle, float3 position) {
	meshInstances[instanceHandle].position = position; shouldUpdateInstances = true;
	MinUpdatedInstanceIndex = Min(instanceHandle, (uint)MinUpdatedInstanceIndex);
	MaxUpdatedInstanceIndex = Max(instanceHandle, (uint)MaxUpdatedInstanceIndex);
}

void Renderer::ClearAllInstances() { numMeshInstances = 0; }

unsigned Renderer::Render(float sunAngle)
{
	camera.Update();
	float time = (float)Window::GetTime();

	if (Window::IsFocused()) // && camera.wasPressing 
	{
		if (shouldUpdateInstances)
		{
			clEnqueueWriteBuffer(command_queue, instanceMem
			  , true, MinUpdatedInstanceIndex * sizeof(MeshInstance)
			  , MaxUpdatedInstanceIndex - MinUpdatedInstanceIndex * sizeof(MeshInstance)
			  , meshInstances + MinUpdatedInstanceIndex, 0, 0, 0);
			MinUpdatedInstanceIndex = 0xFFFFu; MaxUpdatedInstanceIndex = 0x0u;
			shouldUpdateInstances = false;
		}

		if (hasRemovedInstances) { /*todo*/ }

		size_t globalWorkSize[2] = { (size_t)camera.projWidth, (size_t)camera.projHeight};

		struct TraceArgs {
			Vector3f cameraPosition;
			float time;
			uint numSpheres, numMeshes;
			float sunAngle;
		} trace_args;

		trace_args.cameraPosition = camera.position;
		trace_args.numSpheres = NumSpheres;
		trace_args.numMeshes = numMeshInstances;
		trace_args.sunAngle = sunAngle;
		trace_args.time = time;

		cl_int clerr; cl_event event;

		// prepare ray generation kernel
		clerr = clSetKernelArg(rayGenKernel, 0, sizeof(int), &camera.projWidth);                 assert(clerr == 0);
		clerr = clSetKernelArg(rayGenKernel, 1, sizeof(int), &camera.projHeight);                 assert(clerr == 0);
		clerr = clSetKernelArg(rayGenKernel, 2, sizeof(cl_mem), &rayMem);                    assert(clerr == 0);
		clerr = clSetKernelArg(rayGenKernel, 3, sizeof(Matrix4), &camera.inverseView);       assert(clerr == 0);
		clerr = clSetKernelArg(rayGenKernel, 4, sizeof(Matrix4), &camera.inverseProjection); assert(clerr == 0);
		// execute ray generation
		clerr = clEnqueueNDRangeKernel(command_queue, rayGenKernel, 2, nullptr, globalWorkSize, 0, 0, 0, &event); assert(clerr == 0);
		// prepare Rendering
		clerr = clEnqueueAcquireGLObjects(command_queue, 1, &clglScreen, 0, 0, 0); assert(clerr == 0);
		
		cl_mem textureHandleMem, textureDataMem, meshTriangleMem, bvhMem, bvhIndicesMem, materialsMem;
		ResourceManager::GetGPUMemories(&textureHandleMem, &textureDataMem, &meshTriangleMem, &bvhMem, &bvhIndicesMem, &materialsMem);

		clerr = clSetKernelArg(traceKernel, 0, sizeof(cl_mem), &clglScreen      ); assert(clerr == 0);
		clerr = clSetKernelArg(traceKernel, 1, sizeof(cl_mem), &textureHandleMem); assert(clerr == 0);
		clerr = clSetKernelArg(traceKernel, 2, sizeof(cl_mem), &textureDataMem  ); assert(clerr == 0);
		clerr = clSetKernelArg(traceKernel, 3, sizeof(cl_mem), &bvhIndicesMem   ); assert(clerr == 0);
		clerr = clSetKernelArg(traceKernel, 4, sizeof(cl_mem), &meshTriangleMem ); assert(clerr == 0);
		clerr = clSetKernelArg(traceKernel, 5, sizeof(cl_mem), &rayMem          ); assert(clerr == 0);
		clerr = clSetKernelArg(traceKernel, 6, sizeof(cl_mem), &sphereMem       ); assert(clerr == 0);
		clerr = clSetKernelArg(traceKernel, 7, sizeof(TraceArgs), &trace_args   );  assert(clerr == 0);
		clerr = clSetKernelArg(traceKernel, 8, sizeof(cl_mem), &bvhMem); assert(clerr == 0);
		clerr = clSetKernelArg(traceKernel, 9, sizeof(cl_mem), &materialsMem); assert(clerr == 0);
		clerr = clSetKernelArg(traceKernel, 10, sizeof(cl_mem), &instanceMem); assert(clerr == 0);

		// execute rendering
		clerr = clEnqueueNDRangeKernel(command_queue, traceKernel, 2, nullptr, globalWorkSize, 0, 1, &event, 0);  assert(clerr == 0);
		clerr = clEnqueueReleaseGLObjects(command_queue, 1, &clglScreen, 0, 0, 0); assert(clerr == 0);

		clFinish(command_queue);
	}
	// glDrawArrays(GL_TRIANGLES, 0, 3);
	double ms = (Window::GetTime() - time) * 1000.0;
	if (time > 5.0f && ms > 80) { AXERROR("GPU Botleneck! %f ms", ms); exit(0); }
	Engine_UpdateProfilerStats(ProfilerStats_Render, (float)ms);

	return screenTexture;
}

void Renderer::Terminate()
{
	glDeleteVertexArrays(1, &VAO);
	glDeleteProgram(shaderProgram);
	glDeleteTextures(1, &screenTexture);
	ResourceManager::Finalize();

	// cleanup - release OpenCL resources
	clReleaseMemObject(clglScreen);
	clReleaseMemObject(rayMem);
	clReleaseMemObject(sphereMem);
	clReleaseMemObject(instanceMem);
	clReleaseProgram(program);
	clReleaseCommandQueue(command_queue);
	clReleaseKernel(traceKernel);
	clReleaseContext(context);
}
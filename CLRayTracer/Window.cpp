#include "Window.hpp"
#include <GLFW/glfw3.h>
#include <stdio.h>

// Function prototypes
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);

// Window dimensions
namespace Window
{
	GLFWwindow* window;

	const GLuint WIDTH = 800, HEIGHT = 600;

	bool ShouldClose() { bool s = glfwWindowShouldClose(window); glfwPollEvents(); return s; }
	void EndFrame() { glfwSwapBuffers(window); }

	unsigned GetWidth()  { return WIDTH; }
	unsigned GetHeight() { return HEIGHT; }
}

int Window::Create()
{
	// Init GLFW
	glfwInit();
	// Set all the required options for GLFW
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

	// Create a GLFWwindow object that we can use for GLFW's functions
	window = glfwCreateWindow(WIDTH, HEIGHT, "CLRayTracer", nullptr, nullptr);
	if (!window) return 0;
	glfwMakeContextCurrent(window);
	// Set the required callback functions
	glfwSetKeyCallback(window, key_callback);

	// todo add callbacks

	// Define the viewport dimensions
	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	glViewport(0, 0, width, height);
	return 1;
}

void Window::Destroy()
{
	glfwDestroyWindow(window);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);
}
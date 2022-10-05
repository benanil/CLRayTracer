#include "Window.hpp"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <Windows.h>


// Window dimensions
namespace Window
{
	GLFWwindow* window;

	unsigned Width = 800, Height = 600;
	Vector2i monitorScale;

	double dt;
	
	void EndFrame() { glfwSwapBuffers(window); }

	Vector2i GetMonitorScale() { return monitorScale; }
	Vector2i GetWindowScale()  { return Vector2i(Width, Height); }

	unsigned GetWidth()  { return Width;  }
	unsigned GetHeight() { return Height; }

	double GetTime()     { return glfwGetTime(); }
	double DeltaTime()   { return dt;     }

	bool ShouldClose() 
	{
		bool s = glfwWindowShouldClose(window); 
		glfwPollEvents(); 
		static double LastTime = 0;
		double currTime = glfwGetTime();
		dt = currTime - LastTime;
		LastTime = currTime;
		return s;
	}

	void InfiniteMouse()
	{
		POINT point;
		GetCursorPos(&point);

		if (point.x > monitorScale.x - 2) { SetMouseMonitorPos({ 2, point.y }); }
		if (point.y > monitorScale.y - 2) { SetMouseMonitorPos({ point.x, 2 }); }
		if (point.x < 2) { SetMouseMonitorPos({ monitorScale.x - 3, point.y }); }
		if (point.y < 2) { SetMouseMonitorPos({ point.x, monitorScale.y - 3 }); }
	}

	Vector2i GetMouseMonitorPos() {
		POINT point;
		GetCursorPos(&point);
		return Vector2i(point.x, point.y);
	}

	Vector2f GetMouseWindowPos() {
		double x, y;
		glfwGetCursorPos(window, &x, &y);
		return Vector2f(x, y);
	}
	
	void SetMouseMonitorPos(Vector2i pos) {
		SetCursorPos(pos.x, pos.y);
	}

	void SetMouseWindowPos (Vector2f pos) {
		double x = pos.x, y = pos.y;
		glfwSetCursorPos(window, x, y);
	}

	void ChangeName(float ms) 
	{
		char str[28];
		sprintf(str, "CLRenderer ms: %.3f", ms);
		glfwSetWindowTitle(window, str);
	}
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);
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
	window = glfwCreateWindow(Width, Height, "CLRayTracer", nullptr, nullptr);
	if (!window) return 0;
	glfwMakeContextCurrent(window);
	// Set the required callback functions
	glfwSetKeyCallback(window, key_callback);

	const GLFWvidmode* vidMode = glfwGetVideoMode(glfwGetPrimaryMonitor());
	monitorScale.x = vidMode->width;
	monitorScale.y = vidMode->height;
	// todo add callbacks

	// Define the viewport dimensions
	glViewport(0, 0, Width, Height);
	return 1;
}

void Window::Destroy()
{
	glfwDestroyWindow(window);
}
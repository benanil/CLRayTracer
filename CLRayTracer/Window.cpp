#include "Window.hpp"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <Windows.h>
#include "Renderer.hpp"
#include "Editor.hpp"

// Window dimensions
namespace Window
{
	GLFWwindow* window;

	int Width = 900, Height = 700;
	bool Focused = true;
	Vector2i monitorScale;

	double dt;
	
	void EndFrame(unsigned screenImagegl) {
		Editor::Render(screenImagegl);
		glfwSwapInterval(1); 
		glfwSwapBuffers(window); 
	}

	bool IsFocused() { return Focused; }

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
		Editor::Begin();
		return s;
	}

	Vector2i GetMouseScreenPos() {
		POINT point;
		GetCursorPos(&point);
		return Vector2i(point.x, point.y);
	}

	Vector2f GetMouseWindowPos() {
		double x, y;
		glfwGetCursorPos(window, &x, &y);
		return Vector2f(x, y);
	}
	
	void SetMouseScreenPos(Vector2i pos) {
		SetCursorPos(pos.x, pos.y);
	}

	void SetMouseWindowPos (Vector2f pos) {
		double x = pos.x, y = pos.y;
		glfwSetCursorPos(window, x, y);
	}

	bool GetKey(int keyCode)     { return glfwGetKey(window, keyCode) == GLFW_PRESS;   }
	bool GetKeyUp(int keyCode)   { return glfwGetKey(window, keyCode) == GLFW_RELEASE; }
	bool GetKeyDown(int keyCode) { return  !GetKey(keyCode) && !GetKeyUp(keyCode); }

	bool GetMouseButton(int keyCode)     { return glfwGetMouseButton(window, keyCode) == GLFW_PRESS; }
	bool GetMouseButtonUp(int keyCode)   { return glfwGetMouseButton(window, keyCode) == GLFW_RELEASE; }
	bool GetMouseButtonDown(int keyCode) { return !GetMouseButton(keyCode) && !GetMouseButtonUp(keyCode); }

	void ChangeName(float ms) 
	{
		char str[28];
		if (ms < 0.1f)
			glfwSetWindowTitle(window, "Super Fast > 1000fps");
		else {
			sprintf(str, "CLRenderer ms: %.3f", ms);
			glfwSetWindowTitle(window, str);
		}
	}

	void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mode)
	{
		Renderer::OnKeyPressed(key, action);
	}

	void WindowCallback(GLFWwindow* window, int width, int height)
	{
		Width = width; Height = height;
		// todo only use this on gameplay not on editor
		// Renderer::OnWindowResize(width, height);
	}

	void FocusCallback(GLFWwindow* window, int focused)
	{
		Focused = focused;
	}
}

typedef BOOL(WINAPI* PFNWGLSWAPINTERVALEXTPROC)(int interval);

int Window::Create()
{
	// Init GLFW
	glfwInit();
	// Set all the required options for GLFW
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	//glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
	// Create a GLFWwindow object that we can use for GLFW's functions
	window = glfwCreateWindow(Width, Height, "CLRayTracer", nullptr, nullptr);
	if (!window) return 0;

	glfwMakeContextCurrent(window);
	// Set the required callback functions
	glfwSetKeyCallback(window, Window::KeyCallback);
	glfwSetWindowSizeCallback(window, Window::WindowCallback);
	glfwSetWindowFocusCallback(window, Window::FocusCallback);

#ifdef _WIN32
	// Turn on vertical screen sync under Windows.
	// (I.e. it uses the WGL_EXT_swap_control extension)
	PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = NULL;
	wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
	if (wglSwapIntervalEXT) wglSwapIntervalEXT(1);
#endif

	const GLFWvidmode* vidMode = glfwGetVideoMode(glfwGetPrimaryMonitor());
	monitorScale.x = vidMode->width;
	monitorScale.y = vidMode->height;
	Editor::Create(window);
	// Define the viewport dimensions
	glViewport(0, 0, Width, Height);
	return 1;
}

void Window::Destroy()
{
	glfwDestroyWindow(window);
}
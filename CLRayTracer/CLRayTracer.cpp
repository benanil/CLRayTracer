#define CL_TARGET_OPENCL_VERSION 300
#define WIN32_LEAN_AND_MEAN
#define __SSE__
#define __SSE2__
#include <stdio.h>
#include <cassert>
#include "Renderer.hpp"
#include "Window.hpp"

// The MAIN function, from here we start the application and run the game loop
int main()
{
    if(!Window::Create() || !Renderer::Initialize()) return 0;

    while (!Window::ShouldClose())
    {
        Renderer::Render();
        Window::EndFrame();
    }
    Window::Destroy();
    Renderer::Terminate();
    return 1;
}
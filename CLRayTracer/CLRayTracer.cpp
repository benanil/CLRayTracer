#define WIN32_LEAN_AND_MEAN
#include "Renderer.hpp"
#include "Window.hpp"
#include "Math/Matrix.hpp"
#include <stdio.h>

int main()
{
    if (!Window::Create()) return 0;
    if (!Renderer::Initialize()) return 0;

    while (!Window::ShouldClose())
    {
        Renderer::Render();
        Window::EndFrame();
    }
    Window::Destroy();
    Renderer::Terminate();
    return 1;
}
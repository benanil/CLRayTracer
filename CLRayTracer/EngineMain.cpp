#define WIN32_LEAN_AND_MEAN
#include "Renderer.hpp"
#include "Engine.hpp"
#include "Window.hpp"
#include "Math/Matrix.hpp"
#include <stdio.h>

namespace Engine
{
    static void(*EndFrameEvents[20])(void);
    static int NumEndFrameEvents = 0;

    void AddEndOfFrameEvent(void(*action)())
    {
        EndFrameEvents[NumEndFrameEvents++] = action;
    }
}

int main()
{
    if (!Window::Create()) return 0;
    if (!Renderer::Initialize()) return 0;

    while (!Window::ShouldClose())
    {
        while (Engine::NumEndFrameEvents)
            Engine::EndFrameEvents[--Engine::NumEndFrameEvents]();

        unsigned screenImagegl = Renderer::Render();
        Window::EndFrame(screenImagegl);
    }
    Window::Destroy();
    Renderer::Terminate();
    return 1;
}
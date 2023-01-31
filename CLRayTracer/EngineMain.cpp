#include "Renderer.hpp"
#include "Engine.hpp"
#include "Window.hpp"

int main()
{
	if (!Window::Create()) return 0;
    if (!Renderer::Initialize()) return 0;
	Engine_Start();

    while (!Window::ShouldClose())
    {
        float sunAngle = Engine_Tick();
        unsigned screenImagegl = Renderer::Render(sunAngle);
        Window::EndFrame(screenImagegl);
        Engine_EndFrame();
    }

    Engine_Exit();
    Window::Destroy();
    Renderer::Terminate();
    return 1;
}

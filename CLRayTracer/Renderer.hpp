#pragma once

namespace Renderer
{
	int Initialize();
	void Terminate();
	void Render();
	void OnKeyPressed(int keyCode, int action);
	void OnWindowResize(int width, int height);
}
#pragma once

namespace Renderer
{
	int Initialize();
	void Terminate();
	void Render();
	void OnKeyPressed(int keyCode, int action);
}
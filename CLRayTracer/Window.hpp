#pragma once

namespace Window
{
	int Create();
	void Destroy();
	bool ShouldClose();
	void EndFrame();
	unsigned GetWidth();
	unsigned GetHeight();
}

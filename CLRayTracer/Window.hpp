#pragma once
#include "Math/Vector.hpp"

namespace Window
{
	int Create();
	void Destroy();
	bool ShouldClose();
	void EndFrame();
	unsigned GetWidth();
	unsigned GetHeight();
	void ChangeName(float ms);
	
	// TIME
	double GetTime();
	double DeltaTime();

	// INPUT
	void InfiniteMouse();
	Vector2i GetWindowScale();
	Vector2i GetMonitorScale();
	Vector2i GetMouseMonitorPos();
	Vector2f GetMouseWindowPos();

	void SetMouseMonitorPos(Vector2i pos);
	void SetMouseWindowPos (Vector2f pos);
}

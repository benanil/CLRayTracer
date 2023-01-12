#pragma once
#include "Math/Vector.hpp"
#include "Renderer.hpp"

struct HitRecord
{
	Vector3f normal;
	Vector2f uv;
	float distance;
	uint color;
	uint index;
};

constexpr float RayacastMissDistance = 1e30f;

void CPU_RayTraceInitialize();

HitRecord CPU_RayCast(RaySSE ray);

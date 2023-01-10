#pragma once
#include "Math/Vector.hpp"
#include "Renderer.hpp"

struct HitRecord
{
	Vector3f normal;
	Vector3f color;
	Vector2f uv;
	float distance;
	uint index;
};

constexpr float RayacastMissDistance = 1e30f;

void CPU_RayTraceInitialize(MeshInstance* meshInstances);

HitRecord CPU_RayCast(Ray ray);

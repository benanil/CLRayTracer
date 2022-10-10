#pragma once

namespace Helper
{
	char* ReadAllText(const char* path);
}

#pragma pack(1);
typedef struct Sphere_t
{
	float position[3];
	float radius;
	float roughness;
	unsigned color;
} Sphere;
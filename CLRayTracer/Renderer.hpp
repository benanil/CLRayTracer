#pragma once
#include "ResourceManager.hpp"

// internal struct do not use
struct MeshInstance { 
	float3 position; // todo add rotation and scale aka matrix
	ushort meshIndex; 
	ushort materialStart; // each submesh can have material
};

typedef uint MeshInstanceHandle;

namespace Renderer
{
	constexpr uint NumSpheres = 1;
	constexpr uint MaxNumInstances = 40;

	// called from main.cpp
	int Initialize();
	void Terminate();
	void Render();
	
	// called from window.cpp
	void OnKeyPressed(int keyCode, int action);
	void OnWindowResize(int width, int height);

	// called from everywhere
	// spawns a mesh to the world
	// spawning static objects first and dynamic objects afterwords will upgrade the speed of the rendering
	// recomended to seperate the registering to two stages/areas left is static right is dynamic
	void BeginInstanceRegister();
	MeshInstanceHandle RegisterMeshInstance(MeshHandle handle, MaterialHandle material, float3 position);
	void EndInstanceRegister();
	void RemoveMeshInstance(MeshInstanceHandle  handle);
	void ClearAllInstances();

	void SetMeshInstanceMaterial(MeshInstanceHandle meshHandle, MaterialHandle materialHandle);
	
	void SetMeshPosition(MeshInstanceHandle  handle, float3 position);
	// todo rotation scale 
}
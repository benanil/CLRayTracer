#pragma once
#include "ResourceManager.hpp"
#include "Math/Camera.hpp"

// internal struct do not use
struct MeshInstance { 
	Matrix4 inverseTransform;
	ushort meshIndex;  
	ushort materialStart; // each submesh can have material
};

typedef uint MeshInstanceHandle;

namespace Renderer
{
	constexpr uint MaxNumInstances = 401;

	// called from main.cpp
	int Initialize();
	void Terminate();
	unsigned Render(float sunAngle);
	
	// called from window.cpp
	void OnKeyPressed(int keyCode, int action);
	void OnWindowResize(int width, int height);

	// called from everywhere
	// spawns a mesh to the world
	// spawning static objects first and dynamic objects afterwords will upgrade the speed of the rendering
	// recomended to seperate the registering to two stages/areas left is static right is dynamic	
	void BeginInstanceRegister();
	MeshInstanceHandle RegisterMeshInstance(MeshHandle handle, MaterialHandle material, const Matrix4& mat);
	MeshInstanceHandle RegisterMeshInstance(MeshHandle handle, MaterialHandle material, float3 position, const Quaternion& rotation, const float3& scale);
	void EndInstanceRegister();
	void RemoveMeshInstance(MeshInstanceHandle  handle);
	void ClearAllInstances();

	// rgb8 only
	void CreateGLTexture(uint& texture, int width, int height, void* data = nullptr);

	void SetMeshInstanceMaterial(MeshInstanceHandle meshHandle, MaterialHandle materialHandle);
	
	void SetMeshPosition(MeshInstanceHandle handle, float3 position);
	void SetMeshMatrix(MeshInstanceHandle handle, const Matrix4& matrix);
	const Camera& GetCamera();
}

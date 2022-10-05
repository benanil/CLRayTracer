#pragma once
#include "Transform.hpp"
#include "Vector4.hpp"

struct Ray
{
	Vector3f origin;
	Vector3f direction;
};

struct Camera
{
	Matrix4 projection;
	Matrix4 view;
	
	Matrix4 inverseProjection;
	Matrix4 inverseView;

	Transform transform;

	float verticalFOV = 45.0f;
	float nearClip = 0.1f;
	float farClip = 100.0f;
	Vector2i viewportSize;

	Vector3f* m_RayDirections = nullptr;

	Camera() {}

	Camera(Vector2i xviewPortSize) : viewportSize(xviewPortSize)
	{
		m_RayDirections = new Vector3f[xviewPortSize.x * xviewPortSize.y];
		transform.SetPosition(Vector3f(0,0,3));
		RecalculateProjection();
		RecalculateView();
	}

	void RecalculateProjection()
	{
		// The Projection matrix goes from Camera Space to NDC.
		// So inverse(ProjectionMatrix) goes from NDC to Camera Space.
		float aspectRatio = (float)viewportSize.x / (float)viewportSize.y;
		projection = Matrix4::PerspectiveFovLH(verticalFOV * DegToRad, aspectRatio, nearClip, farClip);
		inverseProjection = Matrix4::Inverse(projection);	
	}

	void RecalculateView()
	{
		// The View Matrix goes from World Space to Camera Space.
		// So inverse(ViewMatrix) goes from Camera Space to World Space.
		view = Matrix4::LookAtLH(transform.position, Vector3f(0, 0, -1), Vector3f::Up());
		inverseView = Matrix4::Inverse(view);
	}

	static bool IsEqual(const float  x, const float y) { return fabsf(x - y) <= 0.001f; }

	static bool IsEqual(float* a, float* b)
	{
		return IsEqual(a[0], b[0]) && IsEqual(a[1], b[1]) && IsEqual(a[2], b[2]);
	}

	void CalculateRays()
	{
		#pragma omp parallel for
		for (int y = 0; y < viewportSize.y; ++y)
		{
			for (int x = 0; x < viewportSize.x; ++x)
			{
				Vector2f coord(float(x) / (float)viewportSize.x, float(y) / (float)viewportSize.y);
				coord = coord * 2.0f - 1.0f;
				Vector4 target = inverseProjection * Vector4(coord.x, coord.y, 1.0f, 1.0f);
				target /= target.w;
				target = inverseView * target.Normalized();
				target.y = 0.0f - target.y;
				m_RayDirections[x + y * viewportSize.x] = Vector3f(target.x, target.y, target.z);
			}
		}
	}
};

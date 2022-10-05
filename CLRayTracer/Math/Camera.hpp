#pragma once
#include "Transform.hpp"
#include "Vector4.hpp"

struct Ray
{
	Vector3f origin;
	Vector3f direction;
	Ray(){}
	Ray(Vector3f o, Vector3f d) : origin(o), direction(d) {}
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
		transform.SetPosition(Vector3f(0,0,1));
		RecalculateProjection();
		RecalculateView();
	}

	void RecalculateProjection()
	{
		float aspectRatio = (float)viewportSize.x / (float)viewportSize.y;
		projection = Matrix4::PerspectiveFovLH(verticalFOV * DegToRad, aspectRatio, nearClip, farClip);
		inverseProjection = Matrix4::Inverse(projection);	
	}

	void RecalculateView()
	{
		view = Matrix4::LookAtLH(transform.position, Vector3f(0, 0, -1), Vector3f::Up());
		inverseView = Matrix4::Inverse(view);
	}

	Ray ScreenPointToRay(Vector2i pos)
	{
		Vector2f coord(float(pos.x) / (float)viewportSize.x, float(pos.y) / (float)viewportSize.y);
		coord = coord * 2.0f - 1.0f;
		Vector4 target = inverseProjection * Vector4(coord.x, coord.y, 1.0f, 1.0f);
		target /= target.w;
		target = inverseView * target.Normalized();
		target.y = 0.0f - target.y;
		return Ray(transform.position, Vector3f(target.x, target.y, target.z));
	}
};

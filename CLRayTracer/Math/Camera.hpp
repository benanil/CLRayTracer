#pragma once
#include "Transform.hpp"
#include "Vector4.hpp"
#include "Window.hpp"
#include "../Logger.hpp"

struct Ray
{
	Vector3f origin;
	Vector3f direction;
	Ray(Vector3f o, Vector3f d) : origin(o), direction(d) {}
};

struct Camera
{
	Matrix4 projection;
	Matrix4 view;
	
	Matrix4 inverseProjection;
	Matrix4 inverseView;

	float verticalFOV = 65.0f;
	float nearClip = 0.01f;
	float farClip = 500.0f;
	
	Vector2i viewportSize;
	
	Vector3f position, targetPosition;
	float angle = 0.0f;

	Camera() {}

	Camera(Vector2i xviewPortSize) : viewportSize(xviewPortSize), position(0.0f,0.0f,5.0f)
	{
		RecalculateProjection();
		RecalculateView();
	}
	
	void Update()
	{
		if (!Window::GetMouseButton(MouseButton_Right)) return;
		
		float dt = (float)Window::DeltaTime();
		float speed = dt * (1.0f + Window::GetKey(KeyCode_LEFT_SHIFT) * 2.0f);

		if (Window::GetKey(KeyCode_D)) angle += speed;
		if (Window::GetKey(KeyCode_A)) angle -= speed;
		
		position = Vector3f::Lerp(position, Vector3f(sin(angle)*5.0f, 0.0f, cos(angle)*5.0f), 8.0f * dt);
		
		RecalculateView();
	}

	void RecalculateProjection()
	{
		projection = Matrix4::PerspectiveFovRH(verticalFOV * DegToRad, viewportSize.x, viewportSize.y, nearClip, farClip);
		inverseProjection = Matrix4::Inverse(projection);	
	}

	void RecalculateView()
	{
		view = Matrix4::LookAtRH(position, Vector3f(-sin(angle), 0.0f, -cos(angle)), Vector3f::Up());
		inverseView = Matrix4::Inverse(view);
	}

	Ray ScreenPointToRay(Vector2i pos)
	{
		Vector2f coord(float(pos.x) / (float)viewportSize.x, float(pos.y) / (float)viewportSize.y);
		coord = coord * 2.0f - 1.0f;
		Vector4 target = inverseProjection * Vector4(coord.x, coord.y, 1.0f, 1.0f);
		target /= target.w;
		target = inverseView * target.Normalized();
		target.y = target.y;
		return Ray(position, Vector3f(target.x, target.y, target.z));
	}
};

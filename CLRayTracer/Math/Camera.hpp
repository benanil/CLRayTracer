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
	
	Vector2i viewportSize, monitorSize;
	
	Vector3f position, targetPosition;
	Vector2f mouseOld;
	
	Vector3f Front, Right, Up;
	
	float pitch = 0.0f, yaw = 0.0f, senstivity = 20.0f;
	float angle;

	Camera() {}

	Camera(Vector2i xviewPortSize)
	: viewportSize(xviewPortSize), position(0.0f,0.0f, 5.0f), targetPosition(0.0f,0.0f,5.0f), Front(0.0f,0.0f,1.0f)
	{
		monitorSize = Window::GetMonitorScale();
		RecalculateProjection(xviewPortSize.x, xviewPortSize.y);
		RecalculateView();
	}
	
	void InfiniteMouse(const Vector2f& point)
	{
		#define SET_CURSOR_POS(_x, _y) { Window::SetMouseScreenPos(Vector2i(_x, _y)); mouseOld.x = _x; mouseOld.y = _y; }
		if (point.x > monitorSize.x - 2) SET_CURSOR_POS(3, point.y);
		if (point.y > monitorSize.y - 2) SET_CURSOR_POS(point.x, 3);
		
		if (point.x < 2) SET_CURSOR_POS(monitorSize.x - 3, point.y);
		if (point.y < 2) SET_CURSOR_POS(point.x, monitorSize.y - 3);
        #undef SET_CURSOR_POS
	}

	void Update()
	{
		static bool wasPressing = false;
		bool pressing = Window::GetMouseButton(MouseButton_Right);
		if (!pressing) { wasPressing = false; return; }

		float dt = (float)Window::DeltaTime();
		float speed = dt * (1.0f + Window::GetKey(KeyCode_LEFT_SHIFT) * 2.0f);

		const Vector2f mousePos = ToVector2f(Window::GetMouseScreenPos());
		Vector2f diff = mousePos - mouseOld;
		
		if (wasPressing && diff.x + diff.y < 130.0f)
		{
			pitch -= diff.y * dt * senstivity;
			yaw   += diff.x * dt * senstivity;
			pitch = Clamp(pitch, -89.0f, 89.0f);
		}
		
		Front.x = cosf(yaw * DegToRad) * cosf(pitch * DegToRad);
		Front.y = sinf(pitch * DegToRad);
		Front.z = sinf(yaw * DegToRad) * cosf(pitch * DegToRad);
		Front.Normalized();
		// also re-calculate the Right and Up vector
		Right = Vector3f::Normalize(Vector3f::Cross(Front, Vector3f::Up()));  // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
		Up = Vector3f::Normalize(Vector3f::Cross(Right, Front));

		if (Window::GetKey(KeyCode_D)) position += Right * speed;
		if (Window::GetKey(KeyCode_A)) position -= Right * speed;
		if (Window::GetKey(KeyCode_W)) position += Front * speed;
		if (Window::GetKey(KeyCode_S)) position -= Front * speed;
		if (Window::GetKey(KeyCode_Q)) position -= Up * speed;
		if (Window::GetKey(KeyCode_E)) position += Up * speed;

		mouseOld = mousePos;
		wasPressing = true;

		InfiniteMouse(mousePos);
		RecalculateView();
	}

	void RecalculateProjection(int width, int height)
	{
		projection = Matrix4::PerspectiveFovRH(verticalFOV * DegToRad, width, height, nearClip, farClip);
		inverseProjection = Matrix4::Inverse(projection);	
	}

	void RecalculateView()
	{
		view = Matrix4::LookAtRH(position, Front, Vector3f::Up());
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


#pragma once
#include "Matrix.hpp"

class Transform
{
private:
	Matrix4 transform{};
public:
	Vector3f position{};
	bool needsUpdate = false;
	Quaternion rotation{};
	Vector3f scale {1, 1, 1};

	Transform() {}

	void SetScale(Vector3f scale) {
		this->scale = scale; needsUpdate = true;
	}
	
	void SetPosition(Vector3f position) { this->position = position; needsUpdate = true; }
	
	void SetPosition(float x, float y, float z) { this->position.x = x; this->position.y = y; this->position.z = z; needsUpdate = true; }

	void SetRotationEuler(Vector3f euler) {
		this->rotation = Quaternion::FromEuler(euler); needsUpdate = true;
	}
	
	void SetRotationEulerDegree(Vector3f euler) {
		this->rotation = Quaternion::FromEuler(euler * DegToRad); needsUpdate = true;
	}

	void SetRotationQuaternion(const Quaternion& rotation) {
		this->rotation = rotation; needsUpdate = true;
	}

	void SetMatrix(const Matrix4& matrix)
	{
		this->rotation = Matrix4::ExtractRotation(matrix);
		this->position = Matrix4::ExtractPosition(matrix);
		this->scale    = Matrix4::ExtractScale(matrix);
	}

	void UpdateMatrix()
	{
		transform = Matrix4::Identity() * Matrix4::FromQuaternion(rotation) * Matrix4::CreateScale(scale) * Matrix4::FromPosition(position);
	}

	void UpdatePosition()
	{
		transform.m[3][0] = position.x;
		transform.m[3][1] = position.y;
		transform.m[3][2] = position.z;
	}

	Matrix4& GetMatrix()
	{
		if (needsUpdate) {
			transform = Matrix4::Identity() * Matrix4::FromQuaternion(rotation) * Matrix4::CreateScale(scale) * Matrix4::FromPosition(position);
		}
		return transform;
	}

	Vector3f GetEuler()       const { return Quaternion::ToEulerAngles(rotation); }
	Vector3f GetEulerDegree() const { return Quaternion::ToEulerAngles(rotation) * RadToDeg; }

	Vector3f GetForward() const { return rotation.GetForward();  }
	Vector3f GetRight()   const { return rotation.GetRight();    }
	Vector3f GetLeft()    const { return rotation.GetLeft();     }
	Vector3f GetUp()      const { return rotation.GetUp();       }
};
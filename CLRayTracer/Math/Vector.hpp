#pragma once
#include "Math.hpp"

template<typename T>
struct Vector2
{
	T x, y;

	Vector2()                   noexcept  : x(0), y(0) { }
	constexpr Vector2(T a, T b) noexcept  : x(a), y(b) { }
	
	float Length()		  const { return sqrtf(LengthSquared()); }
	float LengthSquared() const { return x * x + y * y; }

	static float Distance(const Vector2<T> a, const Vector2<T> b)
	{
		float diffx = a.x - b.x;
		float diffy = a.y - b.y;
		return sqrtf(diffx * diffx + diffy * diffy);
	}

	void Normalized() const { *this /= Length(); }
	
	Vector2 Normalize(Vector2 other) { return other.Normalize(); }
	
	Vector2 operator-() { return Vector2(-x, -y); }
	Vector2 operator+(Vector2 other) const { return Vector2(x + other.x, y + other.y); }
	Vector2 operator*(Vector2 other) const { return Vector2(x * other.x, y * other.y); }
	Vector2 operator/(Vector2 other) const { return Vector2(x / other.x, y / other.y); }
	Vector2 operator-(Vector2 other) const { return Vector2(x - other.x, y - other.y); }

	Vector2 operator+(T other) const { return Vector2(x + other, y + other); }
	Vector2 operator*(T other) const { return Vector2(x * other, y * other); }
	Vector2 operator/(T other) const { return Vector2(x / other, y / other); }
	Vector2 operator-(T other) const { return Vector2(x - other, y - other); }
	
	Vector2 operator += (Vector2 o) { x += o.x; y += o.y; return *this; }
	Vector2 operator *= (Vector2 o) { x *= o.x; y *= o.y; return *this; }
	Vector2 operator /= (Vector2 o) { x /= o.x; y /= o.y; return *this; }
	Vector2 operator -= (Vector2 o) { x -= o.x; y -= o.y; return *this; }

	Vector2 operator += (T o) { x += o; y += o; return *this; }
	Vector2 operator *= (T o) { x *= o; y *= o; return *this; }
	Vector2 operator /= (T o) { x /= o; y /= o; return *this; }
	Vector2 operator -= (T o) { x -= o; y -= o; return *this; }
};

struct Vector3f
{
	union
	{
		struct { float x, y, z; };
		float arr[3];
	};

	Vector3f() : x(0), y(0), z(0) { }
	constexpr Vector3f(float scale) : x(scale), y(scale), z(scale) { }
	constexpr Vector3f(float a, float b, float c) : x(a), y(b), z(c) { }
	
	float& operator[] (int index) { return arr[index]; }
	float operator[] (int index) const { return arr[index]; }

	float Length() const { return sqrtf(LengthSquared()); }
	constexpr float LengthSquared() const { return x * x + y * y + z * z; }

	Vector3f& Normalized() { *this /= Length(); return *this; }

	static float Distance(const Vector3f& a, const Vector3f& b)
	{
		Vector3f diff = a - b;
		diff *= diff;
		return sqrtf(diff.x + diff.y + diff.z);
	}

	static float Length(const Vector3f& vec) { return vec.Length(); }

	static float Dot(const Vector3f& a, const Vector3f& b) 
	{
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}

	static Vector3f Lerp(const Vector3f& a, const Vector3f& b, float t) 
	{
		return Vector3f(
			a.x + (b.x - a.x) * t,
			a.y + (b.y - a.y) * t,
			a.z + (b.z - a.z) * t
		);
	}

	static Vector3f Cross(const Vector3f& a, const Vector3f& b) 
	{
		return Vector3f(a.y * b.z - b.y * a.z,
			            a.z * b.x - b.z * a.x,
			            a.x * b.y - b.x * a.y);
	}

	static Vector3f Reflect(const Vector3f& in, const Vector3f& normal)
	{
		return in - normal * Dot(normal, in) * 2.0f;
	}

	static Vector3f Normalize(const Vector3f& a)  {
		return a * rsqrt(Dot(a, a));
	}

	Vector3f operator-() { return Vector3f(-x, -y, -z); }
	Vector3f operator+(const Vector3f& other) const { return Vector3f(x + other.x, y + other.y, z + other.z); }
	Vector3f operator*(const Vector3f& other) const { return Vector3f(x * other.x, y * other.y, z * other.z); }
	Vector3f operator/(const Vector3f& other) const { return Vector3f(x / other.x, y / other.y, z / other.z); }
	Vector3f operator-(const Vector3f& other) const { return Vector3f(x - other.x, y - other.y, z - other.z); }

	Vector3f operator+(float other) const { return Vector3f(x + other, y + other, z + other); }
	Vector3f operator*(float other) const { return Vector3f(x * other, y * other, z * other); }
	Vector3f operator/(float other) const { return Vector3f(x / other, y / other, z / other); }
	Vector3f operator-(float other) const { return Vector3f(x - other, y - other, z - other); }
	
	Vector3f operator += (const Vector3f& o) { x += o.x; y += o.y; z += o.z; return *this; }
	Vector3f operator *= (const Vector3f& o) { x *= o.x; y *= o.y; z *= o.z; return *this; }
	Vector3f operator /= (const Vector3f& o) { x /= o.x; y /= o.y; z /= o.z; return *this; }
	Vector3f operator -= (const Vector3f& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }

	Vector3f operator += (float o) { x += o; y += o; z += o; return *this; }
	Vector3f operator *= (float o) { x *= o; y *= o; z *= o; return *this; }
	Vector3f operator /= (float o) { x /= o; y /= o; z /= o; return *this; }
	Vector3f operator -= (float o) { x -= o; y -= o; z -= o; return *this; }
	
	FINLINE static constexpr Vector3f Up()       { return Vector3f( 0,  1,  0); } 
	FINLINE static constexpr Vector3f Left()     { return Vector3f(-1,  0,  0); } 
	FINLINE static constexpr Vector3f Down()     { return Vector3f( 0, -1,  0); } 
	FINLINE static constexpr Vector3f Right()    { return Vector3f( 1,  0,  0); } 
	FINLINE static constexpr Vector3f Forward()  { return Vector3f( 0,  0,  1); } 
	FINLINE static constexpr Vector3f Backward() { return Vector3f( 0,  0, -1); } 
};

using Vector2f = Vector2<float>;
using Vector2i = Vector2<int>;

typedef Vector3f float3;
typedef Vector2f float2;

FINLINE float3 fminf(const float3& a, const float3& b) { return float3(fminf(a.x, b.x), fminf(a.y, b.y), fminf(a.z, b.z)); }
FINLINE float3 fmaxf(const float3& a, const float3& b) { return float3(fmaxf(a.x, b.x), fmaxf(a.y, b.y), fmaxf(a.z, b.z)); }

FINLINE float Max3(float3 a) { return fmaxf(fmaxf(a.x, a.y), a.z); }
FINLINE float Min3(float3 a) { return fminf(fminf(a.x, a.y), a.z); }

inline Vector2f ToVector2f(const Vector2i& vec) { return Vector2f((float)vec.x, (float)vec.y); }

inline Vector2i ToVector2f(const Vector2f& vec) { return Vector2i((int)vec.x, (int)vec.y);  }

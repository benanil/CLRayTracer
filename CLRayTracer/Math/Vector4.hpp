#pragma once
#include "SIMDCommon.hpp"
#include "Math.hpp"

AX_ALIGNED(16) struct Vector4
{
	union
	{
		struct { float x, y, z, w; };
		float arr[4];
		__m128 vec;
	};

	Vector4()            : x(0), y(0), z(0), w(0) {}
	Vector4(float scale) : x(scale), y(scale), z(scale), w(scale) {}
	Vector4(__m128 _vec) : vec(_vec) {}
	Vector4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}

	operator __m128() const { return vec; }

	FINLINE static __m128 VECTORCALL Normalize(const __m128 V)
	{
		return _mm_mul_ps(_mm_rsqrt_ps(_mm_dp_ps(V, V, 0xff)), V);
	}

	FINLINE static __m128 VECTORCALL Dot(const __m128 V1, const __m128 V2)
	{
		return _mm_dp_ps(V1, V2, 0xff);
	}

	Vector4& Normalized() { vec = _mm_mul_ps(_mm_rsqrt_ps(_mm_dp_ps(vec, vec, 0xff)), vec); return *this; }

	Vector4 VECTORCALL operator + (const Vector4 b) const { return _mm_add_ps(vec, b.vec); }
	Vector4 VECTORCALL operator - (const Vector4 b) const { return _mm_sub_ps(vec, b.vec); }
	Vector4 VECTORCALL operator * (const Vector4 b) const { return _mm_mul_ps(vec, b.vec); }
	Vector4 VECTORCALL operator / (const Vector4 b) const { return _mm_div_ps(vec, b.vec); }

	Vector4& VECTORCALL operator += (const Vector4 b) { vec = _mm_add_ps(vec, b.vec); return *this; }
	Vector4& VECTORCALL operator -= (const Vector4 b) { vec = _mm_sub_ps(vec, b.vec); return *this; }
	Vector4& VECTORCALL operator *= (const Vector4 b) { vec = _mm_mul_ps(vec, b.vec); return *this; }
	Vector4& VECTORCALL operator /= (const Vector4 b) { vec = _mm_div_ps(vec, b.vec); return *this; }

	Vector4 operator *  (const float b) const { return _mm_mul_ps(vec, _mm_set_ps1(b)); }
	Vector4 operator /  (const float b) const { return _mm_div_ps(vec, _mm_set_ps1(b)); }
	Vector4& operator *= (const float b) { vec = _mm_mul_ps(vec, _mm_set_ps1(b)); return *this; }
	Vector4& operator /= (const float b) { vec = _mm_div_ps(vec, _mm_set_ps1(b)); return *this; }
};
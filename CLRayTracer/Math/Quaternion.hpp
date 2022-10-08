#pragma once
#include "Vector.hpp"
#include "Vector4.hpp"
#include "SIMDCommon.hpp"

AX_ALIGNED(16) struct Quaternion
{
	union
	{
		struct { float x, y, z, w; };
		float arr[4];
		__m128 vec;
	};

	Quaternion() : x(0), y(0), z(0), w(1) {}
	Quaternion(float scale) : x(scale), y(scale), z(scale), w(scale) {}
	Quaternion(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
	VECTORCALL Quaternion(__m128 _vec) : vec(_vec) {}

	operator __m128() const { return vec; }

	const float  operator [] (int index) const { return arr[index]; }
	float& operator [] (int index) { return arr[index]; }

	FINLINE static Quaternion Identity() { return g_XMIdentityR3.vec; }

	inline static __m128 VECTORCALL Mul(const __m128 Q1, const __m128 Q2) noexcept
	{
		static const __m128 ControlWZYX = { 1.0f,-1.0f, 1.0f,-1.0f };
		static const __m128 ControlZWXY = { 1.0f, 1.0f,-1.0f,-1.0f };
		static const __m128 ControlYXWZ = { -1.0f, 1.0f, 1.0f,-1.0f };
		__m128 Q2X = Q2, Q2Y = Q2, Q2Z = Q2, vResult = Q2;
		vResult = _mm_shuffle_ps(vResult, vResult, _MM_SHUFFLE(3, 3, 3, 3));
		Q2X = _mm_shuffle_ps(Q2X, Q2X, _MM_SHUFFLE(0, 0, 0, 0));
		Q2Y = _mm_shuffle_ps(Q2Y, Q2Y, _MM_SHUFFLE(1, 1, 1, 1));
		Q2Z = _mm_shuffle_ps(Q2Z, Q2Z, _MM_SHUFFLE(2, 2, 2, 2));
		vResult = _mm_mul_ps(vResult, Q1);
		__m128 Q1Shuffle = Q1;
		Q1Shuffle = _mm_shuffle_ps(Q1Shuffle, Q1Shuffle, _MM_SHUFFLE(0, 1, 2, 3));
		Q2X = _mm_mul_ps(Q2X, Q1Shuffle);
		Q1Shuffle = _mm_shuffle_ps(Q1Shuffle, Q1Shuffle, _MM_SHUFFLE(2, 3, 0, 1));
		Q2X = _mm_mul_ps(Q2X, ControlWZYX);
		Q2Y = _mm_mul_ps(Q2Y, Q1Shuffle);
		Q1Shuffle = _mm_shuffle_ps(Q1Shuffle, Q1Shuffle, _MM_SHUFFLE(0, 1, 2, 3));
		Q2Y = _mm_mul_ps(Q2Y, ControlZWXY);
		Q2Z = _mm_mul_ps(Q2Z, Q1Shuffle);
		vResult = _mm_add_ps(vResult, Q2X);
		Q2Z = _mm_mul_ps(Q2Z, ControlYXWZ);
		Q2Y = _mm_add_ps(Q2Y, Q2Z);
		vResult = _mm_add_ps(vResult, Q2Y);
		return vResult;
	}

	inline static __m128 VECTORCALL MulVec3(__m128 vec, __m128 quat)
	{
		__m128 temp = SSEVector3Cross(quat, vec);
		__m128 temp1 = _mm_mul_ps(vec, SSESplatZ(quat)) ;
		temp = _mm_add_ps(temp, temp1);
		temp1 = _mm_mul_ps(SSEVector3Cross(quat, temp), _mm_set1_ps(2.0f));
		return _mm_add_ps(vec, temp1);
	}

	FINLINE static __m128 VECTORCALL Dot(const Quaternion V1, const Quaternion V2) noexcept
	{
		__m128 vTemp2 = V2.vec;
		__m128 vTemp = _mm_mul_ps(V1.vec, vTemp2);
		vTemp2 = _mm_shuffle_ps(vTemp2, vTemp, _MM_SHUFFLE(1, 0, 0, 0)); // Copy X to the Z position and Y to the W position
		vTemp2 = _mm_add_ps(vTemp2, vTemp);          // Add Z = X+Z; W = Y+W;
		vTemp = _mm_shuffle_ps(vTemp, vTemp2, _MM_SHUFFLE(0, 3, 0, 0));  // Copy W to the Z position
		vTemp = _mm_add_ps(vTemp, vTemp2);           // Add Z and W together
		return _mm_shuffle_ps(vTemp, vTemp, _MM_SHUFFLE(2, 2, 2, 2));    // Splat Z and return
	}

	inline Quaternion VECTORCALL Slerp(const Quaternion Q0, const Quaternion Q1, float t) noexcept
	{
		const __m128 T = _mm_set_ps1(t);
		// Result = Q0 * sin((1.0 - t) * Omega) / sin(Omega) + Q1 * sin(t * Omega) / sin(Omega)
		static const Vector4 OneMinusEpsilon = { { { 1.0f - 0.00001f, 1.0f - 0.00001f, 1.0f - 0.00001f, 1.0f - 0.00001f } } };
		static const Vector4UI SignMask2 = { 0x80000000, 0x00000000, 0x00000000, 0x00000000 };

		__m128 CosOmega = _mm_dp_ps(Q0.vec, Q1.vec, 0xff);

		const __m128 Zero = _mm_setzero_ps();
		__m128 Control = _mm_cmplt_ps(CosOmega, Zero);
		__m128 Sign = SSESelect(g_XMOne, g_XMNegativeOne, Control);

		CosOmega = _mm_mul_ps(CosOmega, Sign);

		Control = _mm_cmplt_ps(CosOmega, OneMinusEpsilon.vec);

		__m128 SinOmega = _mm_mul_ps(CosOmega, CosOmega);
		SinOmega = _mm_sub_ps(g_XMOne, SinOmega);
		SinOmega = _mm_sqrt_ps(SinOmega);

		__m128 Omega = _mm_atan2_ps(SinOmega, CosOmega);

		__m128 V01 = _mm_permute_ps(T, _MM_SHUFFLE(2, 3, 0, 1));
		V01 = _mm_and_ps(V01, g_XMMaskXY);
		V01 = _mm_xor_ps(V01, SignMask2);
		V01 = _mm_add_ps(g_XMIdentityR0, V01);

		__m128 S0 = _mm_mul_ps(V01, Omega);
		S0 = _mm_sin_ps(S0);
		S0 = _mm_div_ps(S0, SinOmega);

		S0 = SSESelect(V01, S0, Control);

		__m128 S1 = SSESplatY(S0);
		S0 = SSESplatX(S0);

		S1 = _mm_mul_ps(S1, Sign);
		__m128 Result = _mm_mul_ps(Q0.vec, S0);
		S1 = _mm_mul_ps(S1, Q1.vec);
		return _mm_add_ps(Result, S1);
	}

	FINLINE Quaternion static FromEuler(float x, float y, float z) noexcept
	{
		// Abbreviations for the various angular functions
		x *= 0.5f; y *= 0.5f; z *= 0.5f;
		float cy, sy;
		ScalarSinCos(&sy, &cy, x);
		float cp, sp;
		ScalarSinCos(&sp, &cp, y);
		float cr, sr;
		ScalarSinCos(&sr, &cr, z);

		Quaternion q;
		q.w = cr * cp * cy + sr * sp * sy;
		q.x = sr * cp * cy - cr * sp * sy;
		q.y = cr * sp * cy + sr * cp * sy;
		q.z = cr * cp * sy - sr * sp * cy;
		return q;
	}

	FINLINE Quaternion static VECTORCALL FromEuler(Vector3f euler) noexcept
	{
		return FromEuler(euler.x, euler.y, euler.z);
	}

	inline Vector3f static ToEulerAngles(const Quaternion& q) noexcept {
		Vector3f eulerAngles;

		// Threshold for the singularities found at the north/south poles.
		constexpr float SINGULARITY_THRESHOLD = 0.4999995f;

		const float sqw = q.w * q.w;
		const float sqx = q.x * q.x;
		const float sqy = q.y * q.y;
		const float sqz = q.z * q.z;
		const float unit = sqx + sqy + sqz + sqw; // if normalised is one, otherwise is correction factor
		const float singularityTest = (q.x * q.z) + (q.w * q.y);

		if (singularityTest > SINGULARITY_THRESHOLD * unit) {
			eulerAngles.z = 2.0f * atan2(q.x, q.w);
			eulerAngles.y = PI / 2;
			eulerAngles.x = 0;
		}
		else if (singularityTest < -SINGULARITY_THRESHOLD * unit)
		{
			eulerAngles.z = -2.0f * atan2(q.x, q.w);
			eulerAngles.y = -(PI / 2);
			eulerAngles.x = 0;
		}
		else {
			eulerAngles.z = atan2(2 * ((q.w * q.z) - (q.x * q.y)), sqw + sqx - sqy - sqz);
			eulerAngles.y = asin(2 * singularityTest / unit);
			eulerAngles.x = atan2(2 * ((q.w * q.x) - (q.y * q.z)), sqw - sqx - sqy + sqz);
		}
		return eulerAngles;
	}

	FINLINE static __m128 VECTORCALL Conjugate(const __m128 vec)
	{
		static const Vector432F NegativeOne3 = { 1.0f,-1.0f,-1.0f,-1.0f};
		return _mm_mul_ps(vec, NegativeOne3);
	}

	FINLINE Quaternion operator *  (const Quaternion& b) { return Mul(this->vec, b.vec); }
	FINLINE Quaternion operator *= (const Quaternion& b) { this->vec = Mul(this->vec, b.vec); return *this; }
};
